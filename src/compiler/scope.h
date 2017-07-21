#ifndef SCOPE_H
#define SCOPE_H

#include "common.h"
#include "types.h"
#include "util.h"
#include "var.h"

#include "hashmap/hashmap.h"

Scope *new_scope(Scope *parent);
Scope *new_fn_scope(Scope *parent);
Scope *new_loop_scope(Scope *parent);
Scope *closest_loop_scope(Scope *scope);
Scope *closest_fn_scope(Scope *scope);
Type **fn_scope_return_type(Scope *scope);

Type *lookup_polymorph(Scope *s, char *name);
Type *lookup_type(Scope *s, char *name);
Type *lookup_local_type(Scope *s, char *name);
int define_polydef_alias(Scope *scope, Type *t, Ast *ast);
Type *define_polymorph(Scope *s, Type *poly, Type *type, Ast *ast);
Type *define_type(Scope *s, char *name, Type *type, Ast *ast);
Type *add_proxy_type(Scope *s, Package *src, TypeDef *orig);
TypeDef *find_type_definition(Type *t);
int local_type_name_conflict(Scope *scope, char *name);
void register_type(Type *t);

void attach_var(Scope *scope, Var *var);
Var *lookup_local_var(Scope *scope, char *name);
Var *lookup_var(Scope *scope, char *name);

TempVar *allocate_ast_temp_var(Scope *scope, Ast *ast);
TempVar *make_temp_var(Scope *scope, Type *t, int id);
Var *find_temp_var(Scope *scope, Ast *ast);
void remove_temp_var_by_id(Scope *scope, int id);

void define_builtin(Var *v);
Var *find_builtin_var(char *name);

void init_builtin_types();
Type **builtin_types();
Type **all_used_types();

#endif
