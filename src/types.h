#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <string.h>

enum {
    INT_T = 1,
    BOOL_T,
    STRING_T,
    VOID_T,
    FN_T,
    AUTO_T,
    STRUCT_T,
    BASEPTR_T,
    PTR_T,
    ARRAY_T,
    DYNARRAY_T
};

typedef struct Type {
    int base; // string, int, func, void
    int held;
    // for functions
    int nargs;
    int binds;
    struct Type **args; // not sure if this should be an array of pointers, or just values.
    struct Type *ret;
    // for structs
    int struct_id;
    //
    struct Type *inner;
    int size;
} Type;

typedef struct StructType {
    char *name;
    int nmembers;
    char **member_names;
    Type **member_types;
    struct StructType *next;
    int id;
} StructType;

char *type_as_str(Type *t);
Type *make_type(int base);
Type *make_fn_type(int nargs, Type **args, Type *ret);
StructType *make_struct_type(char *name, int nmembers, char **member_names, Type **member_types);
Type *find_struct_type(char *name);
StructType *get_struct_type(int id);
int var_size(Type *t);

#endif
