#ifndef TYPES_H
#define TYPES_H

#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

struct AstScope; // forward declare this

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
    ENUM_T,
    TYPE_T,
    ANY_T
};

struct TypeList;

typedef struct Type {
    int id;
    char *name;

    int named;
    int base;
    int held;
    int size;
    long length;

    unsigned char unresolved;
    unsigned char builtin;

    union {
        struct {
            // for functions
            int nargs;
            struct TypeList *args;
            struct Type *ret;
            int variadic;
        } fn;

        struct Type *inner;

        struct {
            int nmembers;
            char **member_names;
            struct Type **member_types;
        } st;

        struct {
            int nmembers;
            char **member_names;
            long *member_values;
            struct Type *inner;
        } _enum;
    };

    struct TypeList *bindings;
    int offset;
    int bindings_id;
} Type;

typedef struct TypeList {
    Type *item;
    struct TypeList *next;
    struct TypeList *prev;
} TypeList;


Type *define_builtin_type(Type *type);


Type *base_type(int t);
Type *base_numeric_type(int t, int size);

Type *make_type(char *name, int base, int size);
Type *make_ptr_type(Type *inner);
Type *make_static_array_type(Type *inner, long length);
Type *make_array_type(Type *inner);
Type *make_fn_type(int nargs, TypeList *args, Type *ret, int variadic);
Type *make_enum_type(char *name, Type *inner, int nmembers, char **member_names, long *member_values);
Type *make_struct_type(char *name, int nmembers, char **member_names, Type **member_types);

Type *register_type(Type *t);
TypeList *get_used_types();
int get_num_used_types();

int add_binding(Type *t, Type *b);

int can_cast(Type *from, Type *to);
int is_numeric(Type *t);
int is_dynamic(Type *t);
int is_array(Type *t);
int is_any(Type *t);
int types_are_equal(Type *a, Type *b);
int check_type(Type *a, Type *b);
int type_can_coerce(Type *from, Type *to);
int type_equality_comparable(Type *a, Type *b);
long array_size(Type *t);
Type *typeinfo_ptr();

int precision_loss_uint(Type *t, unsigned long ival);
int precision_loss_int(Type *t, long ival);
int precision_loss_float(Type *t, double ival);

Type *promote_number_type(Type *a, int left_lit, Type *b, int right_lit);

TypeList *reverse_typelist(TypeList *list);
TypeList *typelist_append(TypeList *list, Type *t);
void remove_type(int id);

#endif
