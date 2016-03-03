#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "token.h"
#include "util.h"
#include "types.h"

#define MAX_ARGS 6

enum {
    AST_STRING,
    AST_INTEGER,
    AST_BOOL,
    AST_BINOP,
    AST_UOP,
    AST_IDENTIFIER,
    AST_TEMP_VAR,
    AST_DECL,
    AST_FUNC_DECL,
    AST_ANON_FUNC_DECL,
    AST_EXTERN_FUNC_DECL,
    AST_CALL,
    AST_CONDITIONAL,
    AST_SCOPE,
    AST_RETURN,
    AST_BLOCK
};

enum {
    PARSE_MAIN,
    PARSE_FUNC
};

typedef struct Var {
    char *name;
    Type *type;
    int id;
    int length;
    int temp;
    int consumed;
    int initialized;
    int ext;
} Var;

typedef struct VarList {
    Var *item;
    struct VarList *next;
} VarList;

typedef struct Ast {
    int type;
    union {
        // int / bool
        int ival;
        // string
        struct {
            char *sval;
            int sid;
        };
        // var
        struct {
            Var *var;
            char *varname;
        };
        // decl
        struct {
            Var *decl_var;
            struct Ast *init;
        };
        // func decl
        struct {
            Var *fn_decl_var;
            int anon;
            Var **fn_decl_args;
            struct Ast *fn_body;
        };
        // uop / binop
        struct {
            int op;
            struct Ast *left;
            struct Ast *right;
        };
        // call
        struct {
            char *fn;
            Var *fn_var;
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
            VarList *locals;
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
        // return
        struct {
            struct Ast *fn_scope;
            struct Ast *ret_expr;
        };
    };
} Ast;

typedef struct AstList {
    Ast *item;
    struct AstList *next;
} AstList;

Var *make_var(char *name, Type *type);
void attach_var(Var *var, Ast *scope);
Var *find_local_var(char *name, Ast *scope);
Var *find_var(char *name, Ast *scope);

void print_ast(Ast *ast);
Type *var_type(Ast *ast);
int is_dynamic(Type *t);

Ast *find_or_make_string(char *str);
AstList *get_global_funcs();
Ast *generate_ast();
Ast *parse_semantics(Ast *ast, Ast *scope);

Ast *make_ast_string(char *val);

Type *parse_type(Tok *t, Ast *scope);
Ast *parse_expression(Tok *t, int priority, Ast *scope);
Ast *parse_arg_list(Tok *t, Ast *scope);
Ast *parse_primary(Tok *t, Ast *scope);
Ast *parse_binop(char op, Ast *left, Ast *right, Ast *scope);
Ast *parse_declaration(Tok *t, Ast *scope);
Ast *parse_statement(Tok *t, Ast *scope);
Ast *parse_block(Ast *scope, int bracketed);
Ast *parse_scope(Ast *parent);
Ast *parse_conditional(Ast *scope);

VarList *get_global_vars();
AstList *get_init_list();

#endif
