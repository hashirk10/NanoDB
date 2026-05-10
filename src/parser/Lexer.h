#pragma once
#include "Token.h"
#include "../utils/CustomArray.h"
#include <cstring>
#include <cstdlib>
#include <cctype>

// tokenizes a SQL-like query string
class Lexer {
    const char* src;
    int         pos;
    int         len;

    char peek()  { return pos < len ? src[pos] : '\0'; }
    char next()  { return src[pos++]; }
    void skip()  {
        while (pos < len && isspace((unsigned char)src[pos])) pos++;
    }

    bool matchKeyword(const char* kw, int klen) {
        if (pos + klen > len) return false;
        for (int i = 0; i < klen; i++)
            if (toupper((unsigned char)src[pos+i]) != kw[i]) return false;
        // make sure it's not part of a longer identifier
        if (pos + klen < len && (isalnum((unsigned char)src[pos+klen]) || src[pos+klen] == '_'))
            return false;
        pos += klen;
        return true;
    }

public:
    Lexer(const char* s) : src(s), pos(0), len(s ? strlen(s) : 0) {}

    CustomArray<Token> tokenize() {
        CustomArray<Token> tokens;
        skip();

        while (pos < len) {
            char c = peek();

            // string literal
            if (c == '"' || c == '\'') {
                char quote = next();
                Token t(TokenType::STRING_LIT);
                int i = 0;
                while (pos < len && src[pos] != quote && i < 255)
                    t.sval[i++] = next();
                t.sval[i] = '\0';
                if (pos < len) next(); // closing quote
                tokens.push_back(t);
                skip(); continue;
            }

            // number
            if (isdigit((unsigned char)c) || (c == '-' && pos+1 < len && isdigit((unsigned char)src[pos+1]))) {
                Token t;
                char buf[64]; int i = 0;
                if (c == '-') buf[i++] = next();
                bool isFloat = false;
                while (pos < len && (isdigit((unsigned char)src[pos]) || src[pos] == '.')) {
                    if (src[pos] == '.') isFloat = true;
                    buf[i++] = next();
                }
                buf[i] = '\0';
                t.fval = atof(buf);
                t.type = isFloat ? TokenType::FLOAT_LIT : TokenType::INT_LIT;
                tokens.push_back(t);
                skip(); continue;
            }

            // two-char operators
            if (c == '=' && pos+1 < len && src[pos+1] == '=') { pos+=2; tokens.push_back(Token(TokenType::EQ));  skip(); continue; }
            if (c == '!' && pos+1 < len && src[pos+1] == '=') { pos+=2; tokens.push_back(Token(TokenType::NEQ)); skip(); continue; }
            if (c == '<' && pos+1 < len && src[pos+1] == '=') { pos+=2; tokens.push_back(Token(TokenType::LTE)); skip(); continue; }
            if (c == '>' && pos+1 < len && src[pos+1] == '=') { pos+=2; tokens.push_back(Token(TokenType::GTE)); skip(); continue; }

            // single-char operators/parens
            if (c == '<')  { next(); tokens.push_back(Token(TokenType::LT));     skip(); continue; }
            if (c == '>')  { next(); tokens.push_back(Token(TokenType::GT));     skip(); continue; }
            if (c == '+')  { next(); tokens.push_back(Token(TokenType::PLUS));   skip(); continue; }
            if (c == '-')  { next(); tokens.push_back(Token(TokenType::MINUS));  skip(); continue; }
            if (c == '*')  { next(); tokens.push_back(Token(TokenType::STAR));   skip(); continue; }
            if (c == '/')  { next(); tokens.push_back(Token(TokenType::SLASH));  skip(); continue; }
            if (c == '%')  { next(); tokens.push_back(Token(TokenType::PERCENT));skip(); continue; }
            if (c == '(')  { next(); tokens.push_back(Token(TokenType::LPAREN)); skip(); continue; }
            if (c == ')')  { next(); tokens.push_back(Token(TokenType::RPAREN)); skip(); continue; }

            // keywords and identifiers
            if (isalpha((unsigned char)c) || c == '_') {
                char buf[256]; int i = 0;
                while (pos < len && (isalnum((unsigned char)src[pos]) || src[pos] == '_'))
                    buf[i++] = next();
                buf[i] = '\0';

                // keyword check (uppercase compare)
                char upper[256];
                for (int j = 0; j <= i; j++) upper[j] = toupper((unsigned char)buf[j]);

                Token t;
                if      (strcmp(upper, "SELECT")          == 0) t.type = TokenType::KW_SELECT;
                else if (strcmp(upper, "INSERT")          == 0) t.type = TokenType::KW_INSERT;
                else if (strcmp(upper, "UPDATE")          == 0) t.type = TokenType::KW_UPDATE;
                else if (strcmp(upper, "DELETE")          == 0) t.type = TokenType::KW_DELETE;
                else if (strcmp(upper, "JOIN")            == 0) t.type = TokenType::KW_JOIN;
                else if (strcmp(upper, "WHERE")           == 0) t.type = TokenType::KW_WHERE;
                else if (strcmp(upper, "SET")             == 0) t.type = TokenType::KW_SET;
                else if (strcmp(upper, "FROM")            == 0) t.type = TokenType::KW_FROM;
                else if (strcmp(upper, "AND")             == 0) t.type = TokenType::AND;
                else if (strcmp(upper, "OR")              == 0) t.type = TokenType::OR;
                else if (strcmp(upper, "NOT")             == 0) t.type = TokenType::NOT;
                else if (strcmp(upper, "SEQUENTIAL_SCAN") == 0) t.type = TokenType::KW_SEQ_SCAN;
                else if (strcmp(upper, "INDEX_SCAN")      == 0) t.type = TokenType::KW_INDEX_SCAN;
                else {
                    t.type = TokenType::IDENTIFIER;
                    strncpy(t.sval, buf, 255);
                    t.sval[255] = '\0';
                }
                tokens.push_back(t);
                skip(); continue;
            }

            // skip anything else
            next();
        }

        tokens.push_back(Token(TokenType::END));
        return tokens;
    }
};
