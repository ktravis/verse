#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SWAP(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
     memcpy(swap_temp,&y,sizeof(x)); \
     memcpy(&y,&x,       sizeof(x)); \
     memcpy(&x,swap_temp,sizeof(x)); \
    } while(0)

struct string_type {
    int len;
    int alloc;
    char *bytes;
};
void print_str(struct string_type *str) {
    printf("%s", str->bytes);
}
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
    return v;
}
struct string_type *copy_string(struct string_type *str) {
    struct string_type *v = malloc(sizeof(struct string_type));
    v->bytes = malloc(str->alloc);
    strncpy(v->bytes, str->bytes, str->len);
    v->len = str->len;
    v->alloc = str->alloc;
    return v;
}
struct string_type *append_string(struct string_type *lhs, char *bytes, int len) {
    int lhs_len = lhs->len;
    lhs->len += len;
    if (lhs->len > lhs->alloc) {
        lhs->alloc = lhs->len * 2;
        lhs->bytes = realloc(lhs->bytes, lhs->alloc);
    }
    strncpy(lhs->bytes + lhs_len, bytes, len);
    return lhs;
}
