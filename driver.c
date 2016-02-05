#include <stdio.h>

extern void * asm_main(void);

int add(int a, int b) {
    return a + b;
}

struct str {
    int len;
    int alloc;
    char bytes[1];
};

char *cstr(struct str *s) {
    return (char *)s->bytes;
}

int main(int argc, char **argv) {
    printf("%d\n", asm_main());
    return 0;
}
