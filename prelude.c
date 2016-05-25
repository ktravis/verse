#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <alloca.h>
#include <math.h>

#define SWAP(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
     memcpy(swap_temp,&y,sizeof(x)); \
     memcpy(&y,&x,       sizeof(x)); \
     memcpy(&x,swap_temp,sizeof(x)); \
    } while(0)

typedef void * fn_type;
typedef void * ptr_type;
struct string_type {
    int length;
    char *bytes;
};
struct array_type {
    void *data;
    long length;
};
struct Type {
    int id;
    struct string_type name;
};
struct NumType {
    int id;
    struct string_type name;
    int size;
    unsigned char is_signed;
};
struct PtrType {
    int id;
    struct string_type name;
    struct Type *inner;
};
struct StructType {
    int id;
    struct string_type name;
    struct array_type members;
    /*int nmembers;*/
    /*struct StructMember *members;*/
};
struct StructMember {
    char *name;
    struct Type *type;
}
struct FnType {
    int id;
    struct string_type name;
    struct array_type args;
    struct Type *return_type;
};
// TODO double-check nulls are in the right spot
struct string_type init_string(const char *str, int l) {
    struct string_type v;
    v.length = l;
    v.bytes = malloc(l+1);
    strncpy(v.bytes, str, l);
    v.bytes[l] = 0;
    return v;
}
struct string_type copy_string(struct string_type str) {
    struct string_type v;
    int l = str.length;
    v.length = l;
    v.bytes = malloc(l+1);
    strncpy(v.bytes, str.bytes, l);
    v.bytes[l] = 0;
    return v;
}
struct string_type append_string(struct string_type lhs, struct string_type rhs) {
    struct string_type v;
    int l = lhs.length + rhs.length;
    v.length = l;
    v.bytes = malloc(l+1);
    strncpy(v.bytes, lhs.bytes, lhs.length);
    strncpy(v.bytes + lhs.length, rhs.bytes, rhs.length);
    v.bytes[l] = 0;
    return v;
}
struct string_type append_string_lit(struct string_type lhs, char *bytes, int length) {
    struct string_type v;
    int l = lhs.length + length;
    v.length = l;
    v.bytes = malloc(l+1);
    strncpy(v.bytes, lhs.bytes, lhs.length);
    strncpy(v.bytes + lhs.length, bytes, length);
    v.bytes[l] = 0;
    return v;
}
int streq_lit(struct string_type left, char *right, int n) {
    if (left.length != n) {
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (left.bytes[i] != right[i]) {
            return 0;
        }
    }
    return 1;
}
int streq(struct string_type left, struct string_type right) {
    if (left.length != right.length) {
        return 0;
    }
    for (int i = 0; i < left.length; i++) {
        if (left.bytes[i] != right.bytes[i]) {
            return 0;
        }
    }
    return 1;
}

// builtins
void _vs_assert(unsigned char a) {
    assert(a);
}
void _vs_println(struct string_type str) {
    printf("%s\n", str.bytes);
    free(str.bytes);
    /*free(str);*/
}
unsigned char _vs_validptr(ptr_type p) {
    return (p != NULL);
}
void _vs_print_str(struct string_type str) {
    printf("%s", str.bytes);
    free(str.bytes);
    /*free(str);*/
}
struct string_type _vs_itoa(int x) {
    struct string_type v;
    v.bytes = malloc(8);
    snprintf(v.bytes, 7, "%d", x);
    v.length = strlen(v.bytes);
    return v;
}
uint8_t _vs_getc() {
    return (uint8_t)getc(stdin);
}
