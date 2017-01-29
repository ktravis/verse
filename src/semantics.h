#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "scope.h"
#include "var.h"

void define_builtin(Var *v);

Ast *parse_uop_semantics(Scope *scope, Ast *ast);
Ast *parse_dot_op_semantics(Scope *scope, Ast *ast);
Ast *parse_assignment_semantics(Scope *scope, Ast *ast);
Ast *parse_binop_semantics(Scope *scope, Ast *ast);
Ast *parse_enum_decl_semantics(Scope *scope, Ast *ast);
Ast *parse_directive_semantics(Scope *scope, Ast *ast);
Ast *parse_struct_literal_semantics(Scope *scope, Ast *ast);
Ast *parse_use_semantics(Scope *scope, Ast *ast);
Ast *parse_slice_semantics(Scope *scope, Ast *ast);
Ast *parse_declaration_semantics(Scope *scope, Ast *ast);
Ast *parse_call_semantics(Scope *scope, Ast *ast);

Ast *parse_semantics(Scope *scope, Ast *ast);

AstBlock *parse_block_semantics(Scope *scope, AstBlock *block, int fn_body);

#endif
