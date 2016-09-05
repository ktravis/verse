#ifndef COMPILER_H
#define COMPILER_H

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

void emit_type(Type *t);
void emit_free(Var *var);
void emit_free_locals(Scope *scope);
void emit_scope_start(Scope *scope);
void emit_scope_end(Scope *scope);
void compile_scope(Scope *scope);
void compile_block(AstBlock *block);
void compile(Scope *scope, Ast *ast);
void compile_unspecified_array(Scope *scope, Ast *ast);
void compile_static_array(Scope *scope, Ast *ast);
void emit_structmember(char *name, Type *st);
void emit_binop(Ast *ast);
void emit_free_bindings(int id, TypeList *bindings);
void emit_static_array_copy(Type *t, char *dest, char *src);

#endif
