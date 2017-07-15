#include "common.h"

typedef struct LibEntry {
    char *name;
    char **required_by;
} LibEntry;

void find_libs_in_file_recursively(char *current_package, char *filename);
LibEntry *find_libs(char *entrypoint);
