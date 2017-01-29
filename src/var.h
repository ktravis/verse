#ifndef VAR_H
#define VAR_H

#include "common.h"
#include "types.h"

typedef struct VarList {
    Var *item;
    struct VarList *next;
} VarList;

Var *make_var(char *name, Type *type);
Var *copy_var(struct Scope *scope, Var *v);
void init_struct_var(Var *var);

VarList *varlist_append(VarList *list, Var *v);
VarList *varlist_remove(VarList *list, char *name);
Var *varlist_find(VarList *list, char *name);
VarList *reverse_varlist(VarList *list);

#endif
