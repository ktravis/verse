#ifndef TYPES_H
#define TYPES_H

#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

enum {
    INT_T = 1,
    UINT_T,
    FLOAT_T,
    BOOL_T,
    STRING_T,
    VOID_T,
    BASEPTR_T
    //ANY_T
};

typedef struct TypeData {
    int base;
    int size;
    long length;
} TypeData;

// Structs, enums, and other base types are "primitives"
// (Any is a 'special' struct)
// all others (functions, refs, arrays, aliases) are not primitives, they use
// Type and not TypeData directly
// Structs can have parameters, the bare type is primitive, but the parametrized
// type is not -- it is expressed as
// a Type<PARAMS>(Type<BASIC>(TypeData<STRUCT_T>), ...)
// i.e. Box is a primitive type -- Box<u16> is parametrized, and so is not
// primitive
// Maybe a better definition -> If ANY resolution can potentially happen to make
// a type concrete, it is not considered primitive
// TODO: does this mean structs are not primitive though? what about a struct

typedef struct TypeList {
    struct Type *item;
    struct TypeList *next;
    struct TypeList *prev;
} TypeList;

typedef enum TypeComp {
    BASIC,
    ALIAS,
    POLY,
    POLYDEF,
    PARAMS,
    STATIC_ARRAY,
    ARRAY,
    REF,
    FUNC,
    STRUCT,
    ENUM
} TypeComp;

struct Scope;

typedef struct FnType {
    int nargs;
    struct TypeList *args;
    struct Type *ret;
    int variadic;
} FnType;

typedef struct StructType {
    int nmembers;
    char **member_names;
    struct Type **member_types;
} StructType;

typedef struct EnumType {
    int nmembers;
    char **member_names;
    long *member_values;
    struct Type *inner;
} EnumType;

typedef struct Type {
    int id;
    TypeComp comp;
    struct Scope *scope;
    union {
        TypeData *data; // basic
        char *name; // alias
        TypeList *params;
        struct {
            int size;
            struct Type *inner;
        } array;
        struct Type *inner; // slice / ref
        FnType fn;
        StructType st;
        EnumType en;
    };
} Type;

typedef struct TypeDef {
    char *name;
    Type *type;
} TypeDef;

typedef struct TypeDefList {
    TypeDef *item;
    struct TypeDefList *next;
    struct TypeDefList *prev;
} TypeDefList;

void init_types();

TypeData *make_primitive(int base, int size);

Type *make_type(struct Scope *scope, char *name);
Type *make_poly(struct Scope *scope, char *name, int id);
Type *make_ref_type(Type *inner);
Type *make_fn_type(int nargs, TypeList *args, Type *ret, int variadic);
Type *make_static_array_type(Type *inner, long length);
Type *make_array_type(Type *inner);

Type *make_enum_type(Type *inner, int nmembers, char **member_names, long *member_values);
Type *make_struct_type(int nmembers, char **member_names, Type **member_types);

Type *resolve_alias(Type *type);
TypeData *resolve_type_data(Type *t);

int get_type_base(struct Scope *scope, Type *t);

int precision_loss_uint(TypeData *t, unsigned long ival);
int precision_loss_int(TypeData *t, long ival);
int precision_loss_float(TypeData *t, double ival);

TypeData *promote_number_type(TypeData *a, int left_lit, TypeData *b, int right_lit);

TypeList *reverse_typelist(TypeList *list);
TypeList *typelist_append(TypeList *list, Type *t);
TypeDefList *reverse_typedeflist(TypeDefList *list);
TypeDefList *typedeflist_append(TypeDefList *list, TypeDef *t);

#endif
