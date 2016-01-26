#include "types.h"

int type_offset(int type) {
    switch (type) {
    case BOOL_T: return 1;
    case INT_T: return 4; // if this is 4, then eax needs to be used in place of rax (etc) in some places
    case STRING_T: return 8;
    case VOID_T: return 0;
    }
    return 0;
}

char* type_as_str(int type) {
    switch (type) {
    case BOOL_T: return "bool";
    case INT_T: return "int"; // if this is 4, then eax needs to be used in place of rax (etc) in some places
    case STRING_T: return "string";
    case VOID_T: return "void";
    }
    return "null";
}
