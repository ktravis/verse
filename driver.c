#include <stdio.h>

extern int asm_main(void);

int add(int a, int b) {
    return a + b;
}

int main(int argc, char **argv) {
    printf("%d\n", asm_main());
    return 0;
}
