#ifndef COMPILER_H
#define COMPILER_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "ast.h"
#include "var.h"
#include "parse.h"
#include "types.h"

void emit_type(Type *t);
void emit_free(Var *var);
void emit_free_locals(Ast *scope);
void emit_scope_start(Ast *ast);
void emit_scope_end(Ast *ast);
void compile(Ast *ast);
void compile_unspecified_array(Ast *ast);
void compile_static_array(Ast *ast);
void emit_binop(Ast *ast);
void emit_free_bindings(int id, TypeList *bindings);
void emit_static_array_copy(Type *t, char *dest, char *src);

#endif
