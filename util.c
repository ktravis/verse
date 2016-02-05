#include "util.h"

void error(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1); 
}

void emit(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("\t");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void label(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}
