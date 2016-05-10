#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "var.h"
#include "token.h"
#include "util.h"
#include "types.h"

enum {
    AST_STRING,
    AST_INTEGER,
    AST_FLOAT,
    AST_BOOL,
    AST_DOT,
    AST_ASSIGN,
    AST_BINOP,
    AST_UOP,
    AST_IDENTIFIER,
    AST_COPY,
    AST_TEMP_VAR,
    AST_RELEASE,
    AST_DECL,
    AST_TYPE,
    AST_FUNC_DECL,
    AST_ANON_FUNC_DECL,
    AST_EXTERN_FUNC_DECL,
    AST_CALL,
    AST_INDEX,
    AST_SLICE,
    AST_CONDITIONAL,
    AST_SCOPE,
    AST_RETURN,
    AST_TYPE_DECL,
    AST_BLOCK,
    AST_WHILE,
    AST_BREAK,
    AST_CONTINUE,
    AST_HOLD,
    AST_BIND,
    AST_STRUCT,
    AST_CAST,
};

struct AstList;

typedef struct Ast {
    int type;
    int line;
    union {
        // int / bool
        long ival; // TODO this will overflow for uint64
        // float
        double fval;
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
            VarList *fn_decl_args;
            struct Ast *fn_body;
            Var *bindings_var;
        };
        // uop / binop
        struct {
            int op;
            struct Ast *left;
            struct Ast *right;
        };
        // cast
        struct {
            struct Ast *cast_left;
            Type *cast_type;
        };
        // dot
        struct {
            struct Ast *dot_left;
            char *member_name;
        };
        // call
        struct {
            struct Ast *fn;
            //Var *fn_var;
            int nargs;
            struct AstList *args;
        };
        // slice
        struct {
            struct Ast *slice_inner;
            Type *slice_array_type;
            struct Ast *slice_offset;
            struct Ast *slice_length;
        };
        // binding
        struct {
            int bind_offset;
            int bind_id;
            struct Ast *bind_expr;
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
            struct AstList *bindings;
            struct AstList *anon_funcs;
            unsigned char has_return;
            unsigned char is_function;
        };
        // conditional
        struct {
            struct Ast *condition;
            struct Ast *if_body;
            struct Ast *else_body;
        };
        // type decl
        struct {
            char *type_name;
            Type *target_type;
        };
        // temp var
        struct {
            Var *tmpvar;
            struct Ast *expr;
        };
        // copy
        struct Ast *copy_expr;
        // return
        struct {
            struct Ast *fn_scope;
            struct Ast *ret_expr;
        };
        // release
        struct {
            struct Ast *release_target;
        };
        // while
        struct {
            struct Ast *while_condition;
            struct Ast *while_body;
        };
        // struct literal
        struct {
            char *struct_lit_name;
            Type *struct_lit_type;
            int nmembers;
            char **member_names;
            struct Ast **member_exprs;
        };
    };
} Ast;

typedef struct AstList {
    Ast *item;
    struct AstList *next;
} AstList;

typedef struct AstListList {
    int id;
    AstList *item;
    struct AstListList *next;
} AstListList;

Ast *ast_alloc(int type);
Ast *make_ast_copy(Ast *ast);

Ast *make_ast_bool(long ival);
Ast *make_ast_string(char *val);
Ast *make_ast_dot_op(Ast *dot_left, char *member_name);
Ast *make_ast_tmpvar(Ast *ast, Var *tmpvar);
Ast *make_ast_id(Var *var, char *name);
Ast *make_ast_decl(char *name, Type *type);
Ast *make_ast_assign(Ast *left, Ast *right);
Ast *make_ast_binop(int op, Ast *left, Ast *right);
Ast *make_ast_slice(Ast *inner, Ast *offset, Ast *length);

Type *var_type(Ast *ast);

Var *get_ast_var(Ast *ast);

AstList *astlist_append(AstList *list, Ast *ast);
AstList *reverse_astlist();

int is_lvalue(Ast *ast);
int is_literal(Ast *ast);
Ast *cast_literal(Type *t, Ast *ast);
Ast *try_implicit_cast(Type *t, Ast *ast);

void print_ast(Ast *ast);

#endif
