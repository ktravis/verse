#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define SWAP(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
     memcpy(swap_temp,&y,sizeof(x)); \
     memcpy(&y,&x,       sizeof(x)); \
     memcpy(&x,swap_temp,sizeof(x)); \
    } while(0)

typedef void * fn_type;
typedef void * ptr_type;
struct string_type {
    int len;
    int alloc;
    char *bytes;
};
// TODO double-check nulls are in the right spot
struct string_type *init_string(const char *str) {
    struct string_type *v = malloc(sizeof(struct string_type));
    int l = strlen(str);
    v->len = l;
    v->alloc = l * 2;
    if (v->alloc < 8) {
        v->alloc = 8;
    }
    v->bytes = malloc(l*2);
    strncpy(v->bytes, str, l);
    v->bytes[l] = 0;
    return v;
}
struct string_type *copy_string(struct string_type *str) {
    struct string_type *v = malloc(sizeof(struct string_type));
    v->bytes = malloc(str->alloc);
    strncpy(v->bytes, str->bytes, str->len);
    v->bytes[str->len] = 0;
    v->len = str->len;
    v->alloc = str->alloc;
    return v;
}
struct string_type *append_string(struct string_type *lhs, struct string_type *rhs) {
    int lhs_len = lhs->len;
    lhs->len += rhs->len;
    if (lhs->len > lhs->alloc) {
        lhs->alloc = lhs->len * 2;
        lhs->bytes = realloc(lhs->bytes, lhs->alloc);
    }
    strncpy(lhs->bytes + lhs_len, rhs->bytes, rhs->len);
    lhs->bytes[lhs_len + rhs->len] = 0;
    return lhs;
}
struct string_type *append_string_lit(struct string_type *lhs, char *bytes, int len) {
    int lhs_len = lhs->len;
    lhs->len += len;
    if (lhs->len > lhs->alloc) {
        lhs->alloc = lhs->len * 2;
        lhs->bytes = realloc(lhs->bytes, lhs->alloc);
    }
    strncpy(lhs->bytes + lhs_len, bytes, len);
    lhs->bytes[lhs_len + len] = 0;
    return lhs;
}
int streq_lit(struct string_type *left, char *right, int n) {
    if (left->len != n) {
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (left->bytes[i] != right[i]) {
            return 0;
        }
    }
    return 1;
}
int streq(struct string_type *left, struct string_type *right) {
    if (left->len != right->len) {
        return 0;
    }
    for (int i = 0; i < left->len; i++) {
        if (left->bytes[i] != right->bytes[i]) {
            return 0;
        }
    }
    return 1;
}

// builtins
void _vs_assert(unsigned char a) {
    assert(a);
}
void _vs_println(struct string_type *str) {
    printf("%s\n", str->bytes);
    free(str->bytes);
    free(str);
}
unsigned char _vs_validptr(ptr_type p) {
    return (p != NULL);
}
void _vs_print_str(struct string_type *str) {
    printf("%s", str->bytes);
    free(str->bytes);
    free(str);
}
struct string_type *_vs_itoa(int x) {
    struct string_type *v = malloc(sizeof(struct string_type));
    v->alloc = 8;
    v->bytes = malloc(8);
    snprintf(v->bytes, 7, "%d", x);
    v->len = strlen(v->bytes);
    return v;
}
