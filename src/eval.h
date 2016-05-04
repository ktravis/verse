#ifndef EVAL_H
#define EVAL_H

#include "ast.h"
#include "types.h"

Ast *eval_const_uop(Ast *ast);
Ast *eval_const_binop(Ast *ast);

#endif
