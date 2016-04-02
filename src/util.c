#include "util.h"

void print_quoted_string(char *val) {
    for (char *c = val; *c; c++) {
        if (*c == '\"') {// || *c == '\\') {
            printf("\\");
        }
        printf("%c", *c);
    }
}

void error(int line, char *fmt, ...) {
    fprintf(stderr, "[line %d] ", line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1); 
}

int escaped_strlen(const char *str) {
    int n = 0, i = 0;
    char c;
    while ((c = str[i++]) != 0) {
        if (c != '\\') {
            n++;
        }
    }
    return n;
}
