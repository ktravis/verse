#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "scope.h"

Ast *first_pass(Scope *scope, Ast *ast);
Ast *parse_semantics(Scope *scope, Ast *ast);

#endif