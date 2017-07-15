#ifndef PARSE_H
#define PARSE_H

#include "ast.h"
#include "token.h"
#include "types.h"

Type *parse_type(Tok *t, int poly_ok);
Ast *parse_directive(Tok *t);
Ast *parse_expression(Tok *t, int priority);
Ast *parse_arg_list(Ast *left);
Ast *parse_primary(Tok *t);
Ast *parse_binop(char op, Ast *left, Ast *right);
Ast *parse_declaration(Tok *t);

Ast *parse_statement(Tok *t, int eat_semi);

Ast *parse_source_file(int line, char *source_file, char *filename);
Ast **parse_statement_list();

AstBlock *parse_astblock(int bracketed);
Ast *parse_block(int bracketed);

Ast *parse_conditional();

Type *parse_struct_type(int poly_ok);

void init_builtins();

#endif
