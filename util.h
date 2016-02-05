#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void error(char *fmt, ...);
void emit(char *fmt, ...);
void label(char *fmt, ...);

#endif
