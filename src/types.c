#include "types.h"

char* type_as_str(int type) {
    switch (type) {
    case BOOL_T: return "bool";
    case INT_T: return "int";
    case STRING_T: return "string";
    case FN_T: return "fn"; // do more here
    case VOID_T: return "void";
    }
    return "null";
}

Type *make_type(int base) {
    Type *t = malloc(sizeof(Type));
    t->base = base;
    return t;
}

Type *make_fn_type(int nargs, Type **args, Type *ret) {
    Type *t = malloc(sizeof(Type));
    t->base = FN_T;
    t->nargs = nargs;
    t->args = args;
    t->ret = ret;
    return t;
}
