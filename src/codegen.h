#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

#include "ast.h"
#include "var.h"
#include "parse.h"
#include "semantics.h"
#include "types.h"

void indent();

void emit_comparison(Scope *scope, Ast *ast);

void emit_string_comparison(Scope *scope, Ast *ast);
void emit_string_binop(Scope *scope, Ast *ast);

void emit_assignment(Scope *scope, Ast *ast);
void emit_dot_op(Scope *scope, Ast *ast);
void emit_uop(Scope *scope, Ast *ast);
void emit_binop(Scope *scope, Ast *ast);
void emit_copy(Scope *scope, Ast *ast);

void emit_type(Type *type);

void compile_unspecified_array(Scope *scope, Ast *ast);
void compile_static_array(Scope *scope, Ast *ast);

void emit_static_array_copy(Scope *scope, Type *t, char *dest, char *src);
void emit_static_array_decl(Scope *scope, Ast *ast);
void emit_decl(Scope *scope, Ast *ast);
void emit_func_decl(Scope *scope, Ast *fn);

void emit_structmember(Scope *scope, Type *st, char *name);
void emit_struct_decl(Scope *scope, Type *st);

void compile_ref(Scope *scope, Ast *ast);

void compile_block(Scope *scope, AstBlock *block);

void compile(Scope *scope, Ast *ast);

void emit_scope_end(Scope *scope);
void emit_scope_start(Scope *scope);

void emit_free(Scope *scope, Var *var);
void emit_free_struct(Scope *scope, char *name, Type *st, int is_ref);
void emit_free_locals(Scope *scope);

void emit_var_decl(Scope *scope, Var *v);
void emit_forward_decl(Scope *scope, Var *v);
void emit_typeinfo_decl(Scope *scope, Type *t);
void emit_typeinfo_init(Scope *scope, Type *t);

#endif
