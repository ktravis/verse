#ifndef TYPES_H
#define TYPES_H

#include <assert.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

void init_types();

Type *typeinfo_ref();

char *type_to_string(Type *t);
int is_any(Type *t);
int is_dynamic(Type *t);
int is_numeric(Type *t);
int is_string(Type *t);
int is_bool(Type *t);
int is_polydef(Type *t);

Type *copy_type(Scope *scope, Type *t);
Type *make_primitive(int base, int size);
Type *make_type(Scope *scope, char *name);
Type *make_polydef(Scope *scope, char *name);
Type *make_poly(Scope *scope, char *name, int id);
Type *make_ref_type(Type *inner);
Type *make_fn_type(int nargs, TypeList *args, Type *ret, int variadic);
Type *make_static_array_type(Type *inner, long length);
Type *make_array_type(Type *inner);
Type *make_enum_type(Type *inner, int nmembers, char **member_names, long *member_values);
Type *make_struct_type(int nmembers, char **member_names, Type **member_types);

Type *resolve_polymorph(Type *type);
Type *resolve_alias(Type *type);
TypeData *resolve_type_data(Type *t);
int match_polymorph(Scope *scope, Type *expected, Type *got);

int get_any_type_id();
Type *get_any_type();
int get_typeinfo_type_id();
Type *base_type(int t);
Type *base_numeric_type(int t, int size);

int check_type(Type *a, Type *b);
int can_cast(Type *from, Type *to);

int precision_loss_uint(Type *t, unsigned long ival);
int precision_loss_int(Type *t, long ival);
int precision_loss_float(Type *t, double ival);

Type *promote_number_type(Type *a, int left_lit, Type *b, int right_lit);

TypeList *reverse_typelist(TypeList *list);
TypeList *typelist_append(TypeList *list, Type *t);

void emit_typeinfo_decl(Scope *scope, Type *t);
void emit_typeinfo_init(Scope *scope, Type *t);

#endif
