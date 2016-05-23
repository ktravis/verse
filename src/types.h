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
    FN_T,
    AUTO_T,
    STRUCT_T,
    BASEPTR_T,
    PTR_T,
    ARRAY_T,
    STATIC_ARRAY_T,
    DYN_ARRAY_T,
    ENUM_T
};

struct TypeList;

typedef struct Type {
    char *name;

    int named;
    int base; // string, int, func, void
    int held;
    int size;
    long length;
    // for structs / derived
    int id;
    union {
        struct {
            // for functions
            int nargs;
            struct TypeList *args;
            struct Type *ret;
        };
        struct Type *inner;
        struct {
            int nmembers;
            char **member_names;
            struct Type **member_types;
        };
    };
    struct TypeList *bindings;
    int offset;
    int bindings_id;
} Type;

typedef struct TypeList {
    Type *item;
    struct TypeList *next;
} TypeList;


Type *base_type(int t);
Type *base_numeric_type(int t, int size);

Type *make_type(char *name, int base, int size);
Type *make_ptr_type(Type *inner);
Type *make_static_array_type(Type *inner, long length);
Type *make_array_type(Type *inner);
Type *make_fn_type(int nargs, TypeList *args, Type *ret);
Type *make_struct_type(char *name, int nmembers, char **member_names, Type **member_types);
Type *find_type(int id);
Type *find_type_by_name(char *name);

int add_binding(Type *t, Type *b);

int can_cast(Type *from, Type *to);
int is_numeric(Type *t);
int is_dynamic(Type *t);
int is_array(Type *t);
int check_type(Type *a, Type *b);
int type_can_coerce(Type *from, Type *to);
int type_equality_comparable(Type *a, Type *b);
long array_size(Type *t);

int precision_loss_uint(Type *t, unsigned long ival);
int precision_loss_int(Type *t, long ival);
int precision_loss_float(Type *t, double ival);

Type *promote_number_type(Type *a, Type *b);

TypeList *reverse_typelist(TypeList *list);
TypeList *typelist_append(TypeList *list, Type *t);
Type *define_type(Type *t);
void remove_type(int id);

#endif
