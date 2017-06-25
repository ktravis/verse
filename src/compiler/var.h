#ifndef VAR_H
#define VAR_H

#include "common.h"
#include "types.h"

Var *make_var(char *name, Type *type);
Var *copy_var(struct Scope *scope, Var *v);
void init_struct_var(Var *var);

#endif
