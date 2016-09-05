#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "parse.h"
#include "scope.h"
#include "types.h"
#include "var.h"

void define_builtin(Var *v);

Type *define_builtin_type(Type *type);
Type *find_builtin_type(char *name);

Ast *parse_uop_semantics(Ast *ast, Scope *scope);
Ast *parse_dot_op_semantics(Ast *ast, Scope *scope);
Ast *parse_assignment_semantics(Ast *ast, Scope *scope);
Ast *parse_binop_semantics(Ast *ast, Scope *scope);
Ast *parse_enum_decl_semantics(Ast *ast, Scope *scope);

AstBlock *parse_block_semantics(AstBlock *block, Scope *scope, int fn_body);

Ast *parse_directive_semantics(Ast *ast, Scope *scope);
Ast *parse_struct_literal_semantics(Ast *ast, Scope *scope);
Ast *parse_use_semantics(Ast *ast, Scope *scope);
Ast *parse_slice_semantics(Ast *ast, Scope *scope);
Ast *parse_declaration_semantics(Ast *ast, Scope *scope);
Ast *parse_call_semantics(Ast *ast, Scope *scope);

Ast *parse_semantics(Ast *ast, Scope *scope);

TypeList *get_global_hold_funcs();
VarList *get_global_vars();

#endif
