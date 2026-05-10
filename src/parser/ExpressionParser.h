#pragma once
#include "../utils/CustomStack.h"
#include "../utils/CustomArray.h"
#include "../utils/Logger.h"
#include "../schema/Value.h"
#include "../schema/Row.h"
#include "../schema/Table.h"
#include "Token.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// operator precedence table
static int opPrec(TokenType t) {
    switch(t) {
        case TokenType::OR:  return 1;
        case TokenType::AND: return 2;
        case TokenType::EQ: case TokenType::NEQ:
        case TokenType::LT: case TokenType::LTE:
        case TokenType::GT: case TokenType::GTE: return 3;
        case TokenType::PLUS: case TokenType::MINUS: return 4;
        case TokenType::STAR: case TokenType::SLASH:
        case TokenType::PERCENT: return 5;
        default: return 0;
    }
}

static bool isOperator(TokenType t) { return opPrec(t) > 0; }
static bool isLeftAssoc(TokenType) { return true; } // all left-associative here

class ExpressionParser {
    // Shunting Yard: converts infix token list to postfix
    static CustomArray<Token> toPostfix(const CustomArray<Token>& infix) {
        CustomArray<Token> output;
        CustomStack<Token> ops;

        // build a debug string for the log
        char infixStr[512] = "";
        for (int i = 0; i < infix.size(); i++) {
            char tmp[64] = "";
            Token t = infix[i];
            if (t.type == TokenType::IDENTIFIER || t.type == TokenType::STRING_LIT)
                snprintf(tmp, 64, "%s ", t.sval);
            else if (t.type == TokenType::INT_LIT || t.type == TokenType::FLOAT_LIT)
                snprintf(tmp, 64, "%.6g ", t.fval);
            else {
                const char* sym = "?";
                switch(t.type) {
                    case TokenType::AND:  sym = "AND"; break;
                    case TokenType::OR:   sym = "OR";  break;
                    case TokenType::EQ:   sym = "==";  break;
                    case TokenType::NEQ:  sym = "!=";  break;
                    case TokenType::LT:   sym = "<";   break;
                    case TokenType::LTE:  sym = "<=";  break;
                    case TokenType::GT:   sym = ">";   break;
                    case TokenType::GTE:  sym = ">=";  break;
                    case TokenType::PLUS: sym = "+";   break;
                    case TokenType::MINUS:sym = "-";   break;
                    case TokenType::STAR: sym = "*";   break;
                    case TokenType::SLASH:sym = "/";   break;
                    case TokenType::PERCENT: sym = "%";break;
                    default: break;
                }
                snprintf(tmp, 64, "%s ", sym);
            }
            strncat(infixStr, tmp, sizeof(infixStr)-strlen(infixStr)-1);
        }

        for (int i = 0; i < infix.size(); i++) {
            Token t = infix[i];

            if (t.type == TokenType::IDENTIFIER || t.type == TokenType::INT_LIT ||
                t.type == TokenType::FLOAT_LIT  || t.type == TokenType::STRING_LIT) {
                output.push_back(t);
            } else if (isOperator(t.type)) {
                while (!ops.empty() && isOperator(ops.top().type) &&
                       ((isLeftAssoc(t.type) && opPrec(t.type) <= opPrec(ops.top().type)) ||
                        (!isLeftAssoc(t.type) && opPrec(t.type) < opPrec(ops.top().type)))) {
                    output.push_back(ops.pop());
                }
                ops.push(t);
            } else if (t.type == TokenType::LPAREN) {
                ops.push(t);
            } else if (t.type == TokenType::RPAREN) {
                while (!ops.empty() && ops.top().type != TokenType::LPAREN)
                    output.push_back(ops.pop());
                if (!ops.empty()) ops.pop(); // discard LPAREN
            }
        }
        while (!ops.empty()) output.push_back(ops.pop());

        // log infix -> postfix conversion
        char postfixStr[512] = "";
        for (int i = 0; i < output.size(); i++) {
            char tmp[64] = "";
            Token t = output[i];
            if (t.type == TokenType::IDENTIFIER || t.type == TokenType::STRING_LIT)
                snprintf(tmp, 64, "%s ", t.sval);
            else if (t.type == TokenType::INT_LIT || t.type == TokenType::FLOAT_LIT)
                snprintf(tmp, 64, "%.6g ", t.fval);
            else {
                const char* sym = "?";
                switch(t.type) {
                    case TokenType::AND:  sym = "AND"; break;
                    case TokenType::OR:   sym = "OR";  break;
                    case TokenType::EQ:   sym = "==";  break;
                    case TokenType::NEQ:  sym = "!=";  break;
                    case TokenType::LT:   sym = "<";   break;
                    case TokenType::LTE:  sym = "<=";  break;
                    case TokenType::GT:   sym = ">";   break;
                    case TokenType::GTE:  sym = ">=";  break;
                    case TokenType::PLUS: sym = "+";   break;
                    case TokenType::MINUS:sym = "-";   break;
                    case TokenType::STAR: sym = "*";   break;
                    case TokenType::SLASH:sym = "/";   break;
                    case TokenType::PERCENT: sym = "%";break;
                    default: break;
                }
                snprintf(tmp, 64, "%s ", sym);
            }
            strncat(postfixStr, tmp, sizeof(postfixStr)-strlen(postfixStr)-1);
        }
        Logger::log("Infix \"%s\" converted to Postfix \"%s\"", infixStr, postfixStr);

        return output;
    }

    // evaluate postfix expression against a row
    static bool evalPostfix(const CustomArray<Token>& pf, const Row& row, const CustomArray<Column>& cols) {
        CustomStack<Value*> stack;

        for (int i = 0; i < pf.size(); i++) {
            Token t = pf[i];

            if (t.type == TokenType::IDENTIFIER) {
                // look up column value in this row
                Value* v = row.getByName(t.sval, cols);
                if (v) stack.push(v->clone());
                else   stack.push(new IntValue(0));
            } else if (t.type == TokenType::INT_LIT) {
                stack.push(new IntValue((int)t.fval));
            } else if (t.type == TokenType::FLOAT_LIT) {
                stack.push(new FloatValue((float)t.fval));
            } else if (t.type == TokenType::STRING_LIT) {
                stack.push(new StringValue(t.sval));
            } else if (isOperator(t.type)) {
                if (stack.size() < 2) break;
                Value* b = stack.pop();
                Value* a = stack.pop();

                Value* result = nullptr;
                bool boolResult = false;
                bool isBool = true;

                switch (t.type) {
                    case TokenType::PLUS:    result = a->add(*b); isBool = false; break;
                    case TokenType::MINUS:   result = a->sub(*b); isBool = false; break;
                    case TokenType::STAR:    result = a->mul(*b); isBool = false; break;
                    case TokenType::SLASH:   result = a->div(*b); isBool = false; break;
                    case TokenType::PERCENT: result = a->mod(*b); isBool = false; break;
                    case TokenType::EQ:  boolResult = a->eq(*b);  break;
                    case TokenType::NEQ: boolResult = a->neq(*b); break;
                    case TokenType::LT:  boolResult = a->lt(*b);  break;
                    case TokenType::LTE: boolResult = a->lte(*b); break;
                    case TokenType::GT:  boolResult = a->gt(*b);  break;
                    case TokenType::GTE: boolResult = a->gte(*b); break;
                    case TokenType::AND: boolResult = (a->toDouble() != 0) && (b->toDouble() != 0); break;
                    case TokenType::OR:  boolResult = (a->toDouble() != 0) || (b->toDouble() != 0); break;
                    default: break;
                }

                delete a; delete b;
                if (isBool) stack.push(new IntValue(boolResult ? 1 : 0));
                else { stack.push(result); }
            }
        }

        bool res = false;
        if (!stack.empty()) {
            res = stack.top()->toDouble() != 0;
            while (!stack.empty()) delete stack.pop();
        }
        return res;
    }

public:
    // evaluate WHERE clause string against a row
    static bool evaluate(const char* expr, const Row& row, const CustomArray<Column>& cols,
                         const CustomArray<Token>& tokens) {
        (void)expr;
        CustomArray<Token> pf = toPostfix(tokens);
        return evalPostfix(pf, row, cols);
    }

    static bool evaluatePostfix(const CustomArray<Token>& postfix, const Row& row,
                                const CustomArray<Column>& cols) {
        return evalPostfix(postfix, row, cols);
    }

    static CustomArray<Token> buildPostfix(const CustomArray<Token>& infix) {
        return toPostfix(infix);
    }
};
