#pragma once
#include "Token.h"
#include "Lexer.h"
#include "../utils/CustomArray.h"
#include <cstring>
#include <cstdio>

enum class QueryType { SELECT, INSERT, UPDATE, DELETE_ROWS, JOIN, SEQ_SCAN, INDEX_SCAN, UNKNOWN };

struct QueryObject {
    QueryType type;
    char      table[64];
    char      where[512];   // WHERE clause or value list for INSERT
    char      joins[8][64]; // extra table names for JOIN
    int       numJoins;
    int       priority;

    QueryObject() : type(QueryType::UNKNOWN), numJoins(0), priority(1) {
        table[0] = where[0] = '\0';
        for (int i = 0; i < 8; i++) joins[i][0] = '\0';
    }
};

class QueryParser {
    CustomArray<Token> tokens;
    int pos;

    Token peek()    { return tokens[pos]; }
    Token advance() { return tokens[pos++]; }
    bool  atEnd()   { return tokens[pos].type == TokenType::END; }

    // collect everything after keyword into where buffer
    void collectRest(char* buf, int maxLen) {
        int off = 0;
        // skip WHERE keyword if present
        if (!atEnd() && tokens[pos].type == TokenType::KW_WHERE) advance();
        while (!atEnd() && off < maxLen-1) {
            Token t = advance();
            char tmp[300] = "";
            if      (t.type == TokenType::IDENTIFIER)  snprintf(tmp, 300, "%s ", t.sval);
            else if (t.type == TokenType::STRING_LIT)   snprintf(tmp, 300, "\"%s\" ", t.sval);
            else if (t.type == TokenType::INT_LIT)      snprintf(tmp, 300, "%d ", (int)t.fval);
            else if (t.type == TokenType::FLOAT_LIT)    snprintf(tmp, 300, "%.2f ", t.fval);
            else {
                const char* sym = "";
                switch(t.type) {
                    case TokenType::AND: sym = "AND"; break; case TokenType::OR: sym = "OR"; break;
                    case TokenType::EQ:  sym = "==";  break; case TokenType::NEQ: sym = "!="; break;
                    case TokenType::LT:  sym = "<";   break; case TokenType::LTE: sym = "<="; break;
                    case TokenType::GT:  sym = ">";   break; case TokenType::GTE: sym = ">="; break;
                    case TokenType::PLUS: sym = "+";  break; case TokenType::MINUS: sym = "-"; break;
                    case TokenType::STAR: sym = "*";  break; case TokenType::SLASH: sym = "/"; break;
                    case TokenType::PERCENT: sym = "%"; break;
                    case TokenType::LPAREN: sym = "("; break; case TokenType::RPAREN: sym = ")"; break;
                    case TokenType::KW_WHERE: sym = "WHERE"; break;
                    case TokenType::KW_SET:   sym = "SET";   break;
                    case TokenType::KW_FROM:  sym = "FROM";  break;
                    default: break;
                }
                snprintf(tmp, 300, "%s ", sym);
            }
            int tl = strlen(tmp);
            if (off + tl >= maxLen-1) break;
            memcpy(buf+off, tmp, tl);
            off += tl;
        }
        buf[off] = '\0';
    }

public:
    QueryParser(const char* query) : pos(0) {
        // handle ADMIN: prefix (sets priority 0)
        const char* src = query;
        if (strncmp(src, "ADMIN:", 6) == 0) src += 6;
        Lexer lex(src);
        tokens = lex.tokenize();
    }

    QueryObject* parse() {
        QueryObject* qo = new QueryObject();

        // check for ADMIN: prefix in original (we already stripped it in ctor)
        // priority is passed externally, default 1

        if (atEnd()) return qo;

        Token first = advance();

        if (first.type == TokenType::KW_SELECT) {
            qo->type = QueryType::SELECT;
            if (!atEnd() && peek().type == TokenType::IDENTIFIER) {
                strncpy(qo->table, peek().sval, 63); advance();
            }
            collectRest(qo->where, 512);

        } else if (first.type == TokenType::KW_INSERT) {
            qo->type = QueryType::INSERT;
            if (!atEnd() && peek().type == TokenType::IDENTIFIER) {
                strncpy(qo->table, peek().sval, 63); advance();
            }
            collectRest(qo->where, 512);

        } else if (first.type == TokenType::KW_UPDATE) {
            qo->type = QueryType::UPDATE;
            if (!atEnd() && peek().type == TokenType::IDENTIFIER) {
                strncpy(qo->table, peek().sval, 63); advance();
            }
            // collect SET ... WHERE ... as-is
            char buf[512] = "";
            int off = 0;
            while (!atEnd() && off < 510) {
                Token t = advance();
                char tmp[300] = "";
                if      (t.type == TokenType::IDENTIFIER) snprintf(tmp, 300, "%s ", t.sval);
                else if (t.type == TokenType::STRING_LIT)  snprintf(tmp, 300, "\"%s\" ", t.sval);
                else if (t.type == TokenType::INT_LIT)     snprintf(tmp, 300, "%d ", (int)t.fval);
                else if (t.type == TokenType::FLOAT_LIT)   snprintf(tmp, 300, "%.2f ", t.fval);
                else {
                    const char* sym = "";
                    switch(t.type) {
                        case TokenType::KW_SET:   sym = "SET";   break;
                        case TokenType::KW_WHERE: sym = "WHERE"; break;
                        case TokenType::EQ:       sym = "==";    break;
                        default: break;
                    }
                    snprintf(tmp, 300, "%s ", sym);
                }
                int tl = strlen(tmp);
                if (off + tl >= 510) break;
                memcpy(buf+off, tmp, tl); off += tl;
            }
            buf[off] = '\0';
            strncpy(qo->where, buf, 511);

        } else if (first.type == TokenType::KW_DELETE) {
            qo->type = QueryType::DELETE_ROWS;
            if (!atEnd() && peek().type == TokenType::IDENTIFIER) {
                strncpy(qo->table, peek().sval, 63); advance();
            }
            collectRest(qo->where, 512);

        } else if (first.type == TokenType::KW_JOIN) {
            qo->type = QueryType::JOIN;
            if (!atEnd() && peek().type == TokenType::IDENTIFIER) {
                strncpy(qo->table, peek().sval, 63); advance();
            }
            while (!atEnd() && peek().type == TokenType::IDENTIFIER && qo->numJoins < 7) {
                strncpy(qo->joins[qo->numJoins++], peek().sval, 63);
                advance();
            }

        } else if (first.type == TokenType::KW_SEQ_SCAN) {
            qo->type = QueryType::SEQ_SCAN;
            if (!atEnd() && peek().type == TokenType::IDENTIFIER) {
                strncpy(qo->table, peek().sval, 63); advance();
            }

        } else if (first.type == TokenType::KW_INDEX_SCAN) {
            qo->type = QueryType::INDEX_SCAN;
            if (!atEnd() && peek().type == TokenType::IDENTIFIER) {
                strncpy(qo->table, peek().sval, 63); advance();
            }
            collectRest(qo->where, 512);
        }

        return qo;
    }
};
