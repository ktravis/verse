#include <stdio.h>

#include "array/array.h"
#include "common.h"
#include "find_libs.h"

static const unsigned char marker[] = "libs:";

static LibEntry *add_lib(LibEntry *libs, char *current_package, char *name) {
    int found = 0;
    for (int i = 0; i < array_len(libs); i++) {
        if (!strcmp(name, libs[i].name)) {
            int done = 0;
            for (int j = 0; j < array_len(libs[i].required_by); j++) {
                if (!strcmp(libs[i].required_by[j], current_package)) {
                    done = 1;
                    break;
                }
            }
            found = 1;
            if (!done) {
                array_push(libs[i].required_by, current_package);
            }
            break;
        }
    }
    if (!found) {
        LibEntry lib;
        lib.name = name;
        lib.required_by = NULL;
        array_push(lib.required_by, current_package);
        array_push(libs, lib);
    }
    return libs;
}

LibEntry *find_libs(Package **pkgs) {
    LibEntry *libs = NULL;
    for (int i = 0; i < array_len(pkgs); i++) {
        for (int j = 0; j < array_len(pkgs[i]->top_level_comments); j++) {
            if (!memcmp(marker, pkgs[i]->top_level_comments[j], sizeof(marker)-1)) {
                char *comment = pkgs[i]->top_level_comments[j] + sizeof(marker) - 1;
                char *lib = comment;
                int started = 0;
                while (*comment) {
                    if (*comment == ' ') {
                        if (started) {
                            char *name = malloc(sizeof(char) * (started + 1));
                            snprintf(name, started+1, "%s", lib);
                            libs = add_lib(libs, pkgs[i]->name, name);
                            started = 0;
                        }
                    } else {
                        if (!started) {
                            lib = comment;
                        }
                        started++;
                    }
                    comment++;
                }
                if (started) {
                    char *name = malloc(sizeof(char) * (started + 1));
                    snprintf(name, started, "%s", lib);
                    libs = add_lib(libs, pkgs[i]->name, lib);
                }
            }
        }
    }
    return libs;
}
