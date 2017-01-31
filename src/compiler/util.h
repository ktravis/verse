#ifndef UTIL_H
#define UTIL_H

void print_quoted_string(char *val);
void error(int line, char *file, char *fmt, ...);
int escaped_strlen(const char *str);
void errlog(char *fmt, ...);
char *package_name(char *path);
char *dir_name(char *fname);

#endif
