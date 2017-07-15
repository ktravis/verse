#include <stdio.h>

#include "array/array.h"
#include "ast.h"
#include "common.h"
#include "find_libs.h"
#include "package.h"
#include "parse.h"
#include "semantics.h"
#include "token.h"
#include "util.h"

static char **packages;
static char **includes;

static LibEntry *libs;

static int seen_file(char *name) {
    // TODO: speed
    for (int i = 0; i < array_len(includes); i++) {
        if (!strcmp(name, includes[i])) {
            return 1;
        }
    }
    return 0;
}

static int seen_package(char *name) {
    // TODO: speed
    for (int i = 0; i < array_len(packages); i++) {
        if (!strcmp(name, packages[i])) {
            return 1;
        }
    }
    return 0;
}

static FILE *open_file(int line, char *source_file, char *filename) {
    char *err;
    FILE *f = open_file_or_error(filename, "r", &err);
    if (!f) {
        error(line, source_file, "Could not open source file '%s': %s", filename, err);
        return NULL;
    }
    push_file_source(filename, f);
    return f;
}

void find_libs_in_file_recursively(char *current_package, char *filename) {
    Tok *t;
    for (;;) {
        t = next_token();
        if (!t) {
            break;
        }
        switch (t->type) {
        case TOK_DIRECTIVE:
            if (!strcmp(t->sval, "include")) {
                t = next_token();
                int line = lineno();
                if (t->type != TOK_STR || !expect_line_break_or_semicolon()) {
                    error(line, filename, "Unexpected token '%s' while parsing include directive.",
                        tok_to_string(t));
                }
                char *path = t->sval;
                if (path[0] != '/') {
                    char *dir = dir_name(current_file_name());
                    int dirlen = strlen(dir);
                    int pathlen = strlen(path);
                    char *tmp = malloc(sizeof(char) * (dirlen + pathlen + 1));
                    snprintf(tmp, dirlen + pathlen + 1, "%s%s", dir, path);
                    path = tmp;
                }
                if (!seen_file(path)) {
                    open_file(line, filename, path);
                    find_libs_in_file_recursively(current_package, path);
                    pop_file_source();
                    array_push(includes, path);
                }
            } else if (!strcmp(t->sval, "import")) {
                t = next_token();
                int line = lineno();
                if (t->type != TOK_STR || !expect_line_break_or_semicolon()) {
                    error(line, filename, "Unexpected token '%s' while parsing import directive.",
                        tok_to_string(t));
                }
                char *import_string = t->sval;
                if (seen_package(import_string)) {
                    break;
                }
                char *package_path = package_path_from_import_string(import_string);
                char **package_files = package_source_files(line, filename, package_path);
                if (!package_files) {
                    error(line, filename, "No verse source files found in package '%s' ('%s').", import_string, package_path);
                }

                array_push(packages, import_string);
                for (int i = 0; i < array_len(package_files); i++) {
                    open_file(line, filename, package_files[i]);
                    find_libs_in_file_recursively(import_string, package_files[i]);
                    pop_file_source();
                }
            } else if (!strcmp(t->sval, "lib")) {
                t = next_token();
                int line = lineno();
                if (t->type != TOK_STR || !expect_line_break_or_semicolon()) {
                    error(line, current_file_name(),
                        "Unexpected token '%s' while parsing import directive.",
                        tok_to_string(t));
                }
                int found = 0;
                for (int i = 0; i < array_len(libs); i++) {
                    if (!strcmp(t->sval, libs[i].name)) {
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
                    lib.name = t->sval;
                    lib.required_by = NULL;
                    array_push(lib.required_by, current_package);
                    array_push(libs, lib);
                }
            }
            break;
        default:
            break;
        }
    }
    unget_token(t);
}

LibEntry *find_libs(char *entrypoint) {
    find_libs_in_file_recursively("main", entrypoint);
    return libs;
}
