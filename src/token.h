#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "types.h"

enum {
    TOK_STR,
    TOK_INT,
    TOK_FLOAT,
    TOK_BOOL,
    TOK_COLON,
    TOK_SEMI,
    TOK_CARET,
    TOK_ID,
    TOK_FN,
    TOK_TYPE,
    TOK_OP,
    TOK_UOP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_STARTBIND,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LSQUARE,
    TOK_RSQUARE,
    TOK_IF,
    TOK_ELSE,
    TOK_RETURN,
    TOK_EXTERN,
    TOK_STRUCT,
    TOK_HOLD,
    TOK_RELEASE,
    TOK_WHILE,
    TOK_BREAK,
    TOK_CONTINUE,
};

enum {
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_XOR,
    OP_BINAND,
    OP_BINOR,
    OP_ASSIGN,
    OP_AND,
    OP_OR,
    OP_EQUALS,
    OP_NEQUALS,
    OP_NOT,
    OP_DOT,
    OP_GT,
    OP_GTE,
    OP_LT,
    OP_LTE,
    OP_ADDR,
    OP_AT,
    OP_CAST
};

typedef struct Tok {
    int type;
    union {
        int ival;
        double fval;
        char *sval;
        int op;
    };
} Tok;

void skip_spaces();
int is_id_char(char c);
int type_id(char *buf);

Tok *make_token(int t);
Tok *next_token();
Tok *peek_token();

void unget_token(Tok *tok);

double read_decimal(char c);
Tok *read_number(char c);
Tok *read_string(char quote);
Tok *read_identifier(char c);
Tok *check_reserved(char *buf);

int priority_of(Tok *t);
const char *to_string(Tok *t);
const char *token_type(int type);
const char *op_to_str(int op);
int is_comparison(int op);
int valid_unary_op(int op);

Tok *expect(int type);

int lineno();

#endif
