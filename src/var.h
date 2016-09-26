#ifndef VAR_H
#define VAR_H

#include <stdio.h>

#include "common.h"
#include "types.h"

int new_var_id();
Var *make_var(char *name, Type *type);
Var *copy_var(Scope *scope, Var *v);
void init_struct_var(Var *var);

VarList *varlist_append(VarList *list, Var *v);
VarList *varlist_remove(VarList *list, char *name);
Var *varlist_find(VarList *list, char *name);
VarList *reverse_varlist(VarList *list);

#endif
