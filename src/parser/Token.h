#pragma once

enum class TokenType {
    // literals
    INT_LIT, FLOAT_LIT, STRING_LIT, IDENTIFIER,
    // comparison ops
    EQ, NEQ, LT, LTE, GT, GTE,
    // arithmetic ops
    PLUS, MINUS, STAR, SLASH, PERCENT,
    // logical
    AND, OR, NOT,
    // parens
    LPAREN, RPAREN,
    // keywords
    KW_SELECT, KW_INSERT, KW_UPDATE, KW_DELETE, KW_JOIN,
    KW_WHERE, KW_SET, KW_FROM, KW_SEQ_SCAN, KW_INDEX_SCAN,
    // misc
    END
};

struct Token {
    TokenType type;
    char      sval[256]; // string or identifier value
    double    fval;      // numeric value

    Token() : type(TokenType::END), fval(0.0) { sval[0] = '\0'; }
    Token(TokenType t) : type(t), fval(0.0) { sval[0] = '\0'; }
};
