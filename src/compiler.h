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
#include "types.h"

void emit_type(Type *t);
void emit_free(Var *var);
void emit_free_locals(AstScope *scope);
void emit_scope_start(AstScope *ast);
void emit_scope_end(AstScope *ast);
void compile_scope(AstScope *scope);
void compile_block(AstBlock *block);
void compile(Ast *ast);
void compile_unspecified_array(Ast *ast);
void compile_static_array(Ast *ast);
void emit_structmember(char *name, Type *st);
void emit_binop(Ast *ast);
void emit_free_bindings(int id, TypeList *bindings);
void emit_static_array_copy(Type *t, char *dest, char *src);

#endif
