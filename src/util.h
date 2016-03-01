#ifndef UTIL_H
#define UTIL_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "token.h"

void print_quoted_string(char *val);
void error(char *fmt, ...);
int escaped_strlen(const char *str);

#endif
