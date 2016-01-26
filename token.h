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

typedef struct Tok {
    int type;
    union {
        int ival;
        int tval;
        char *sval;
        char c;
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
char *to_string(Tok *t);
const char *token_type(int type);

#endif
