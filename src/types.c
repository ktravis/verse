#include "types.h"


char* type_as_str(Type *t) {
    switch (t->base) {
    case BOOL_T: return "bool";
    case INT_T: return "int";
    case STRING_T: return "string";
    case FN_T: {
         char *parts[10]; // TODO duh
         size_t size = 6; // fn + ( + ) + : + \0
         for (int i = 0; i < t->nargs; i++) {
             parts[i] = type_as_str(t->args[i]);
             size += strlen(parts[i]);
             if (i < t->nargs - 1) {
                 size += 1; // + ','
             }
         }
         char *ret = type_as_str(t->ret);
         size_t retlen = strlen(ret);
         size += retlen;
         char *buf = malloc(sizeof(char) * size);
         strncpy(buf, "fn(", 3);
         char *m = buf + 3;
         // TODO not sure args is correct
         for (int i = 0; i < t->nargs; i++) {
             size_t n = strlen(parts[i]);
             strncpy(m, parts[i], n);
             m += n;
             if (t->args[i]->base == FN_T) {
                 free(parts[i]);
             }
         }
         strncpy(m, "):", 2);
         m += 2;
         strncpy(m, ret, retlen);
         if (t->ret->base == FN_T) {
             free(ret);
         }
         buf[size-1] = 0;
         return buf;
    }
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
