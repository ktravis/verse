#ifndef TYPES_H
#define TYPES_H

enum {
    INT_T = 1,
    BOOL_T,
    STRING_T,
    VOID_T
};

int type_offset(int type);
char *type_as_str(int type);

#endif
