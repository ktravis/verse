#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>

enum {
    INT_T = 1,
    BOOL_T,
    STRING_T,
    VOID_T,
    FN_T
};

typedef struct Type {
    int base; // string, int, func, void
    // for functions
    int nargs;
    struct Type **args; // not sure if this should be an array of pointers, or just values.
    struct Type *ret;
    // for structs
    int struct_id;
} Type;

char *type_as_str(int type);
Type *make_type(int base);
Type *make_fn_type(int nargs, Type **args, Type *ret);

#endif
