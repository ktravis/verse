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
    AST_DOT,
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
    //int held;
    int ext;
    struct Var **members;
} Var;

typedef struct VarList {
    Var *item;
    struct VarList *next;
} VarList;

typedef struct AstList AstList;

typedef struct Ast {
    int type;
    int line;
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
            AstList *args;
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
            AstList *bindings;
            AstList *anon_funcs;
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

Var *make_var(char *name, Type *type);
void attach_var(Var *var, Ast *scope);
Var *find_local_var(char *name, Ast *scope);
Var *find_var(char *name, Ast *scope);

void print_ast(Ast *ast);
Type *var_type(Ast *ast);
int is_dynamic(Type *t);
int check_type(Type *a, Type *b);
int can_cast(Type *from, Type *to);

Ast *find_or_make_string(char *str);
AstList *get_global_funcs();
TypeList *get_global_structs();
Ast *generate_ast();
Ast *parse_semantics(Ast *ast, Ast *scope);
Var *get_ast_var(Ast *ast);

Ast *make_ast_string(char *val);
Ast *make_ast_dot_op(Ast *dot_left, char *member_name);

Type *parse_type(Tok *t, Ast *scope);
Ast *parse_expression(Tok *t, int priority, Ast *scope);
Ast *parse_arg_list(Ast *left, Ast *scope);
Ast *parse_primary(Tok *t, Ast *scope);
Ast *parse_binop(char op, Ast *left, Ast *right, Ast *scope);
//Ast *parse_declaration(Tok *t, Ast *scope, int held);
Ast *parse_declaration(Tok *t, Ast *scope);
Ast *parse_statement(Tok *t, Ast *scope);
Ast *parse_block(Ast *scope, int bracketed);
Ast *parse_scope(Ast *parent);
Ast *parse_conditional(Ast *scope);
Type *parse_struct_type(Ast *scope);

VarList *get_global_vars();
AstList *reverse_astlist();
AstList *get_init_list();
VarList *get_global_bindings();
AstList *get_binding_exprs(int id);
void add_binding_expr(int id, Ast *expr);

void init_builtins();
void init_types();

#endif
