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

Type *parse_type(Tok *t, Scope *scope);
Ast *parse_directive(Tok *t, Scope *scope);
Ast *parse_expression(Tok *t, int priority, Scope *scope);
Ast *parse_arg_list(Ast *left, Scope *scope);
Ast *parse_primary(Tok *t, Scope *scope);
Ast *parse_binop(char op, Ast *left, Ast *right, Scope *scope);
Ast *parse_declaration(Tok *t, Scope *scope);

Ast *parse_statement(Tok *t, Scope *scope);

Ast *parse_source_file(char *filename, Scope *scope);
Ast *parse_statement_list(Scope *scope);

Ast *parse_block_ast(Scope *scope, int bracketed);
Ast *parse_block(Scope *scope, int bracketed);

Ast *parse_conditional(Scope *scope);

Type *parse_struct_type(Scope *scope);

Ast *parse_semantics(Ast *ast, Scope *scope);

void init_builtins();

#endif
