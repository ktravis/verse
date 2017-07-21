#ifndef TYPES_H
#define TYPES_H

#include "common.h"

typedef enum PrimitiveType {
    INT_T = 1,
    UINT_T,
    FLOAT_T,
    BOOL_T,
    STRING_T,
    VOID_T,
    BASEPTR_T
} PrimitiveType;

typedef struct TypeData {
    int base;
    int size;
    //long length;
} TypeData;

void init_types();

Type *typeinfo_ref();

char *type_to_string(Type *t);
int is_any(Type *t);
int is_dynamic(Type *t);
int is_array(Type *t);
int is_numeric(Type *t);
int is_void(Type *t);
int is_string(Type *t);
int is_bool(Type *t);
int is_polydef(Type *t);
int is_concrete(Type *t);
int is_owned(Type *t);
int contains_generic_struct(Type *t);

Ast *find_method(Type *t, char *name);
Ast *define_method(Scope *impl_scope, Type *t, Ast *decl);

Type *copy_type(Scope *scope, Type *t);
Type *make_primitive(int base, int size);
Type *make_type(Scope *scope, char *name);
Type *make_polydef(Scope *scope, char *name);
Type *make_poly(Scope *scope, char *name, int id);
Type *make_ref_type(Type *inner);
Type *make_fn_type(Type **args, Type **ret, int variadic);
Type *make_static_array_type(Type *inner, long length);
Type *make_array_type(Type *inner);
Type *make_enum_type(Type *inner, char **member_names, long *member_values);
Type *make_params_type(Type *inner, Type **params);
Type *make_struct_type(char **member_names, Type **member_types);
Type *make_generic_struct_type(char **member_names, Type **member_types, Type **params);
Type *make_external_type(char *pkg, char *name);

Type *find_type_or_polymorph(Type *type);

long enum_type_val(Type *type, int index);

Type *resolve_polymorph_recursively(Type *type);
Type *resolve_external(Type *type);
Type *resolve_type(Type *type);
//Type *replace_type(Type *base, Type *from, Type *to);
Type *replace_type_by_name(Type *base, char *from_name, Type *to);

int get_any_type_id();
Type *get_any_type();
int get_typeinfo_type_id();
int get_basetype_id(TypeComp c);
int get_numtype_type_id();
int get_structmember_type_id();
Type *base_type(PrimitiveType t);
Type *base_numeric_type(int t, int size);

int size_of_type(Type *t);

int precision_loss_uint(Type *t, unsigned long ival);
int precision_loss_int(Type *t, long ival);
int precision_loss_float(Type *t, double ival);

#endif
