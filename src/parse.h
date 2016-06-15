#ifndef PARSE_H
#define PARSE_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "ast.h"
#include "eval.h"
#include "token.h"
#include "util.h"
#include "types.h"
#include "var.h"
#include "semantics.h"

enum {
    PARSE_MAIN,
    PARSE_FUNC
};

AstList *get_global_funcs();
TypeList *get_global_structs();

Type *parse_type(Tok *t, AstScope *scope);
Ast *parse_directive(Tok *t, AstScope *scope);
Ast *parse_expression(Tok *t, int priority, AstScope *scope);
Ast *parse_arg_list(Ast *left, AstScope *scope);
Ast *parse_primary(Tok *t, AstScope *scope);
Ast *parse_binop(char op, Ast *left, Ast *right, AstScope *scope);
Ast *parse_declaration(Tok *t, AstScope *scope);
Ast *parse_statement(Tok *t, AstScope *scope);
Ast *parse_block(AstScope *scope, int bracketed);
Ast *parse_scope(AstScope *scope, AstScope *parent);
Ast *parse_conditional(AstScope *scope);
Type *parse_struct_type(AstScope *scope);

Ast *parse_semantics(Ast *ast, AstScope *scope);
Ast *parse_source_file(char *filename, AstScope *scope);

TypeList *get_builtin_types();
AstList *get_init_list();

void init_builtins();
void init_types();

#endif
