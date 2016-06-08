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

void attach_var(Var *var, AstScope *scope);
Var *find_local_var(char *name, AstScope *scope);
Var *find_var(char *name, AstScope *scope);

AstList *get_global_funcs();
TypeList *get_global_structs();

Type *define_type(Type *type, AstScope *scope);
Type *define_builtin_type(Type *type);
Type *parse_type(Tok *t, AstScope *scope);
Ast *parse_directive(Tok *t, AstScope *scope);
Ast *parse_expression(Tok *t, int priority, AstScope *scope);
Ast *parse_arg_list(Ast *left, AstScope *scope);
Ast *parse_primary(Tok *t, AstScope *scope);
Ast *parse_binop(char op, Ast *left, Ast *right, AstScope *scope);
Ast *parse_declaration(Tok *t, AstScope *scope);
Ast *parse_statement(Tok *t, AstScope *scope);
Ast *parse_block(AstScope *scope, int bracketed);
AstScope *new_scope(AstScope *parent);
Ast *parse_scope(AstScope *scope, AstScope *parent);
Ast *parse_conditional(AstScope *scope);
Type *parse_struct_type(AstScope *scope);

Ast *parse_semantics(Ast *ast, AstScope *scope);
Ast *parse_source_file(char *filename, AstScope *scope);

TypeList *get_builtin_types();
VarList *get_global_vars();
AstList *get_init_list();
VarList *get_global_bindings();
AstList *get_binding_exprs(int id);
TypeList *get_global_hold_funcs();
void add_binding_expr(int id, Ast *expr);

Type *find_type(int id);
Type *find_type_by_name(char *name, AstScope *scope, ResolutionList *child);

void init_builtins();
void init_types();

#endif
