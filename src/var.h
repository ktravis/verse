#ifndef VAR_H
#define VAR_H

#include <stdio.h>
#include "types.h"

typedef struct Var {
    char *name;
    Type *type;
    int id;
    int length;
    int temp;
    int consumed;
    int initialized;
    unsigned char constant;
    //int held;
    int ext;
    struct Var *proxy;
    struct Var **members;
} Var;

typedef struct VarList {
    Var *item;
    struct VarList *next;
} VarList;

int new_var_id();
Var *make_var(char *name, Type *type);

VarList *varlist_append(VarList *list, Var *v);
VarList *varlist_remove(VarList *list, char *name);
Var *varlist_find(VarList *list, char *name);
VarList *reverse_varlist(VarList *list);

#endif
