#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <string.h>

enum {
    INT_T = 1,
    UINT_T,
    BOOL_T,
    STRING_T,
    VOID_T,
    FN_T,
    AUTO_T,
    STRUCT_T,
    BASEPTR_T,
    PTR_T,
    ARRAY_T,
    DYNARRAY_T,
    DERIVED_T
};

typedef struct TypeList TypeList;

typedef struct Type {
    char *name;

    int named;
    int base; // string, int, func, void
    int held;
    int size;
    // for structs / derived
    int id;
    union {
        struct {
            // for functions
            int nargs;
            TypeList *args;
            struct Type *ret;
        };
        struct Type *inner;
        struct {
            int nmembers;
            char **member_names;
            struct Type **member_types;
        };
    };
    TypeList *bindings;
    int offset;
    int bindings_id;
} Type;

typedef struct TypeList {
    Type *item;
    struct TypeList *next;
} TypeList;

//char *type_as_str(Type *t);
Type *make_type(char *name, int base, int size);
Type *make_ptr_type(Type *inner);
Type *make_fn_type(int nargs, TypeList *args, Type *ret);
Type *make_struct_type(char *name, int nmembers, char **member_names, Type **member_types);
Type *find_type(int id);
Type *find_type_by_name(char *name);
//Type *find_struct_type(char *name);
//Type *get_struct_type(int id);
//int var_size(Type *t);
int add_binding(Type *t, Type *b);
TypeList *reverse_typelist(TypeList *list);
TypeList *typelist_append(TypeList *list, Type *t);
Type *define_type(Type *t);
void remove_type(int id);

#endif
