#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "ast.h"
#include "eval.h"
#include "token.h"
#include "util.h"
#include "types.h"
#include "var.h"

#define MAX_ARGS 6

enum {
    PARSE_MAIN,
    PARSE_FUNC
};

void attach_var(Var *var, Ast *scope);
Var *find_local_var(char *name, Ast *scope);
Var *find_var(char *name, Ast *scope);

AstList *get_global_funcs();
TypeList *get_global_structs();

Type *parse_type(Tok *t, Ast *scope);
Ast *parse_expression(Tok *t, int priority, Ast *scope);
Ast *parse_arg_list(Ast *left, Ast *scope);
Ast *parse_primary(Tok *t, Ast *scope);
Ast *parse_binop(char op, Ast *left, Ast *right, Ast *scope);
Ast *parse_declaration(Tok *t, Ast *scope);
Ast *parse_statement(Tok *t, Ast *scope);
Ast *parse_block(Ast *scope, int bracketed);
Ast *parse_scope(Ast *parent);
Ast *parse_conditional(Ast *scope);
Type *parse_struct_type(Ast *scope);

Ast *parse_semantics(Ast *ast, Ast *scope);

VarList *get_global_vars();
AstList *get_init_list();
VarList *get_global_bindings();
AstList *get_binding_exprs(int id);
void add_binding_expr(int id, Ast *expr);

void init_builtins();
void init_types();

#endif
