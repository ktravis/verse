#include "common.h"

typedef struct LibEntry {
    char *name;
    char **required_by;
} LibEntry;

LibEntry *find_libs(Package **pkgs);
