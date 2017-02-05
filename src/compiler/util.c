#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

void print_quoted_string(char *val) {
    for (char *c = val; *c; c++) {
        switch (*c) {
        case 0x27:
            printf("\\\'");
            break;
        case 0x22:
            printf("\\\"");
            break;
        case 0x3f:
            printf("\\?");
            break;
        case 0x5c:
            printf("\\\\");
            break;
        case 0x07:
            printf("\\a");
            break;
        case 0x08:
            printf("\\b");
            break;
        case 0x0c:
            printf("\\f");
            break;
        case 0x0a:
            printf("\\n");
            break;
        case 0x0d:
            printf("\\r");
            break;
        case 0x09:
            printf("\\t");
            break;
        case 0x0b:
            printf("\\v");
            break;
        default:
            printf("%c", *c);
        }
    }
}

void error(int line, char *file, char *fmt, ...) {
    fprintf(stderr, "%s:%d: ", file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1); 
}

void errlog(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
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

// TODO: handle this better, this is sloppy
char *package_name(char *path) {
    int last = 0;
    int i = 0;
    char c;
    int n = 0;
    while ((c = path[i]) != '\0') {
        if (c == '/') {
            if (path[i+1] == '\0') {
                break;
            }
            n = 0;
            last = i + 1;
        } else {
            n++;
        }
        i++;
    }
    char *out = malloc(sizeof(char) * (n + 1));
    for (int i = 0; i < n; i++) {
        out[i] = path[last + i];
    }
    out[n] = '\0';
    return out;
}

// TODO: make this better
// Return including slash?
char *dir_name(char *fname) {
    int last = 0;
    int i = 0;
    char c;
    while ((c = fname[i]) != '\0') {
        if (c == '/') {
            if (fname[i+1] == '\0') {
                return fname;
            }
            last = i + 1;
        }
        i++;
    }
    char *out = malloc(sizeof(char) * (last + 1));
    for (int i = 0; i < last; i++) {
        out[i] = fname[i];
    }
    out[last] = '\0';
    return out;
}

char *executable_path() {
    char buf[1024];
    ssize_t n = readlink("/proc/self/exe", buf, 1024);
    char *out = malloc(sizeof(char) * (n + 1));

    for (int i = 0; i < n; i++) {
        out[i] = buf[i];
    }
    out[n] = '\0';
    return out;
}

char *root_from_binary() {
    char *bin = executable_path();
    char *bin_dir = dir_name(bin);
    free(bin);
    // remove trailing '/'
    if (bin_dir[strlen(bin_dir) - 1] == '/') {
        bin_dir[strlen(bin_dir) - 1] = '\0';
    }
    char *root_dir = dir_name(bin_dir);
    free(bin_dir);
    return root_dir;
}
