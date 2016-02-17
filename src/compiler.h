#ifndef COMPILER_H
#define COMPILER_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "parse.h"
#include "types.h"

#define MAX_ARGS 6

const static char *ARG_REGS[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

void emit(char *fmt, ...);
void label(char *fmt, ...);
void emit_data_section();
void emit_func_start();
void emit_func_end();
void emit_scope_start(Ast *ast);
void emit_scope_end(Ast *ast);
void compile(Ast *ast);
void emit_binop(Ast *ast);

#endif
