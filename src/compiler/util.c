#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "util.h"

void print_quoted_string(FILE *f, char *val) {
    for (char *c = val; *c; c++) {
        switch (*c) {
        case 0x27:
            fprintf(f, "\\\'");
            break;
        case 0x22:
            fprintf(f, "\\\"");
            break;
        case 0x3f:
            fprintf(f, "\\?");
            break;
        case 0x5c:
            fprintf(f, "\\\\");
            break;
        case 0x07:
            fprintf(f, "\\a");
            break;
        case 0x08:
            fprintf(f, "\\b");
            break;
        case 0x0c:
            fprintf(f, "\\f");
            break;
        case 0x0a:
            fprintf(f, "\\n");
            break;
        case 0x0d:
            fprintf(f, "\\r");
            break;
        case 0x09:
            fprintf(f, "\\t");
            break;
        case 0x0b:
            fprintf(f, "\\v");
            break;
        default:
            fprintf(f, "%c", *c);
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

char *strip_vs_ext(char *filename) {
    char *ext_start = strstr(filename, ".vs");
    if (!ext_start) {
        return NULL;
    }
    int n = ext_start - filename;
    char *out = malloc(sizeof(char) * (n + 1));
    strncpy(out, filename, n);
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

FILE *open_file_or_quit(const char *filename, const char *mode) {
    char *err;
    FILE *f = open_file_or_error(filename, mode, &err);
    if (!f) {
        fprintf(stderr, "Unable to open file '%s': %s\n", filename, err);
        exit(1);
    }
    return f;
}

FILE *open_file_or_error(const char *filename, const char *mode, char **err) {
    assert(err != NULL);
    FILE *f = fopen(filename, mode);
    if (!f) {
        *err = strerror(errno);
        return NULL;
    }
    return f;
}
