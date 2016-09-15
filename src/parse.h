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

Type *parse_type(Scope *scope, Tok *t, int poly_ok);
Ast *parse_directive(Scope *scope, Tok *t);
Ast *parse_expression(Scope *scope, Tok *t, int priority);
Ast *parse_arg_list(Scope *scope, Ast *left);
Ast *parse_primary(Scope *scope, Tok *t);
Ast *parse_binop(Scope *scope, char op, Ast *left, Ast *right);
Ast *parse_declaration(Scope *scope, Tok *t);

Ast *parse_statement(Scope *scope, Tok *t);

Ast *parse_source_file(Scope *scope, char *filename);
AstList *parse_statement_list(Scope *scope);

AstBlock *parse_astblock(Scope *scope, int bracketed);
Ast *parse_block(Scope *scope, int bracketed);

Ast *parse_conditional(Scope *scope);

Type *parse_struct_type(Scope *scope, int poly_ok);

void init_builtins();

#endif
