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
    TOK_BOOL,
    TOK_COLON,
    TOK_SEMI,
    TOK_ID,
    TOK_TYPE,
    TOK_OP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_IF,
    TOK_ELSE
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
    OP_EQUALS
};

typedef struct Tok {
    int type;
    union {
        int ival;
        int tval;
        char *sval;
        int op;
    };
} Tok;

static Tok *last = NULL;

void skip_spaces();
int is_id_char(char c);
int type_id(char *buf);

Tok *make_token(int t);
Tok *next_token();
Tok *peek_token();

void unget_token(Tok *tok);

Tok *read_integer(char c);
Tok *read_string(char quote);
Tok *read_identifier(char c);
Tok *check_reserved(char *buf);

int priority_of(Tok *t);
const char *to_string(Tok *t);
const char *token_type(int type);
const char *op_to_str(int op);

#endif
