#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

void print_quoted_string(FILE *f, char *val);
void error(int line, char *file, char *fmt, ...);
int escaped_strlen(const char *str);
void errlog(char *fmt, ...);
char *package_name(char *path);
char *dir_name(char *fname);
char *strip_vs_ext(char *filename);
char *executable_path();
char *root_from_binary();
FILE *open_file_or_quit(const char *filename, const char *mode);
FILE *open_file_or_error(const char *filename, const char *mode, char **err);

#endif
