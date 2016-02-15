#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "token.h"
#include "util.h"

#define MAX_ARGS 6

enum {
    AST_STRING,
    AST_INTEGER,
    AST_BOOL,
    AST_BINOP,
    AST_IDENTIFIER,
    AST_TEMP_VAR,
    AST_DECL,
    AST_CALL,
    AST_CONDITIONAL,
    AST_SCOPE,
    AST_BLOCK
};

typedef struct Var {
    char *name;
    int type;
    int offset;
    int length;
    int temp;
    int consumed;
    int initialized;
    struct Var *next;
} Var;

typedef struct Ast {
    int type;
    union {
        // int / bool
        int ival;
        // string
        struct {
            char *sval;
            int sid;
            struct Ast *snext;
        };
        // var
        Var *var;
        // decl
        struct {
            Var *decl_var;
            struct Ast *init;
        };
        // binop
        struct {
            int op;
            struct Ast *left;
            struct Ast *right;
        };
        // call
        struct {
            char *fn; // should be symbol, but not until decl works
            int nargs;
            struct Ast **args;
        };
        // block
        struct {
            struct Ast **statements; // change this to be a linked list or something
            int num_statements;
        };
        // scope
        struct {
            struct Ast *parent;
            struct Ast *body;
            Var *locals;
        };
        // conditional
        struct {
            struct Ast *condition;
            struct Ast *if_body;
            struct Ast *else_body;
            int cond_id;
        };
        // temp var
        struct {
            Var *tmpvar;
            struct Ast *expr;
        };
    };
} Ast;

Var *make_var(char *name, int type, Ast *scope);
Var *find_local_var(char *name, Ast *scope);
Var *find_var(char *name, Ast *scope);

void print_ast(Ast *ast);
char var_type(Ast *ast);
int is_dynamic(int t);

Ast *find_or_make_string(char *str);
Ast *get_string_list();

Ast *make_ast_string(char *val);
Ast *make_ast_identifier(char *ident, int type);

Ast *parse_expression(Tok *t, int priority, Ast *scope);
Ast *parse_arg_list(Tok *t, Ast *scope);
Ast *parse_primary(Tok *t, Ast *scope);
Ast *parse_binop(char op, Ast *left, Ast *right, Ast *scope);
Ast *parse_declaration(Tok *t, Ast *scope);
Ast *parse_statement(Tok *t, Ast *scope);
Ast *parse_block(Ast *scope, int bracketed);
Ast *parse_scope(Ast *parent);
Ast *parse_conditional(Ast *scope);

#endif
