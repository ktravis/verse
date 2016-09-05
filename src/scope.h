#ifndef SCOPE_H
#define SCOPE_H

#include "types.h"
#include "var.h"

typedef enum {
    Root,
    Simple,
    Function,
    Loop
} ScopeType;

typedef struct TempVarList {
    int id;
    Var *var;
    struct TempVarList *next;
} TempVarList;

typedef struct Scope {
    struct Scope *parent;
    ScopeType type;
    VarList *vars;
    struct TempVarList *temp_vars;
    TypeDefList *types;
    TypeList *used_types;
    unsigned char has_return;
    Var *fn_var;
} Scope;

Scope *new_scope(Scope *parent);
Scope *new_fn_scope(Scope *parent);
Scope *new_loop_scope(Scope *parent);
Scope *closest_loop_scope(Scope *scope);
Scope *closest_fn_scope(Scope *scope);
Type *fn_scope_return_type(Scope *scope);

Type *lookup_type(Scope *s, char *name);
Type *lookup_local_type(Scope *s, char *name);
Type *define_type(Scope *s, char *name, Type *type);
int local_type_name_conflict(Scope *scope, char *name);

Var *make_temp_var(Scope *scope, Type *type);

void attach_var(Scope *scope, Var *var);
void detach_var(Scope *scope, Var *var);
Var *lookup_var(Scope *scope, char *name);
Var *lookup_local_var(Scope *scope, char *name);
Var *find_var(Scope *scope, char *name);

struct Ast;
Var *allocate_temp_var(Scope *scope, struct Ast *ast);
Var *find_temp_var(Scope *scope, struct Ast *ast);

#endif
