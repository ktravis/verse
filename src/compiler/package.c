#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "array/array.h"
#include "package.h"
#include "scope.h"
#include "parse.h"
#include "token.h"
#include "types.h"

static Package *current_package;
static Package **pkg_stack;

Package *new_package(char *name, char *path) {
    Package *p = calloc(sizeof(Package), 1);
    p->path = path;
    p->name = name;
    p->scope = new_scope(NULL);
    return p;
}

Ast *check_semantics(Scope *scope, Ast *ast);
Ast *first_pass(Scope *scope, Ast *ast);

Package *package_check_semantics(Package *p) {
    if (p->semantics_checked) {
        return p;
    }
    p->semantics_checked = 1;
    push_current_package(p);
    for (int i = 0; i < array_len(p->files); i++) {
        p->files[i]->root = check_semantics(p->scope, p->files[i]->root);
    }
    pop_current_package();
    return p;
}

static PkgFile *package_add_file(Package *p, char *path, Ast *file_ast) {
    PkgFile *f = malloc(sizeof(PkgFile));
    f->name = path;
    f->root = file_ast;
    array_push(p->files, f);
    return f;
}

Package *get_current_package() {
    return current_package;
}

static Package *main_package = NULL;

Package *get_main_package() {
    assert(main_package);
    return main_package;
}

Package *init_main_package(char *path) {
    assert(main_package == NULL);
    main_package = new_package("<main>", path);
    push_current_package(main_package);
    return main_package;
}

void define_global(Var *v) {
    array_push(current_package->globals, v);
}

static Package **all_packages = NULL;

static Package *package_previously_loaded(char *path) {
    for (int i = 0; i < array_len(all_packages); i++) {
        if (!strcmp(all_packages[i]->path, path)) {
            return all_packages[i];
        }
    }
    return NULL;
}

Package **all_loaded_packages() {
    return all_packages;
}

void push_current_package(Package *p) {
    array_push(pkg_stack, p);
    current_package = p;
}

Package *pop_current_package() {
    assert(array_len(pkg_stack));
    array_raw_len(pkg_stack) -= 1;
    current_package = pkg_stack[array_len(pkg_stack)-1];
    return current_package;
}

static char *verse_root = NULL;

char *package_path_from_import_string(char *imp) {
    assert(imp != NULL);
    if (imp[0] != '/') {
        // TODO: hardcoded, this should also read env var if available
        if (verse_root == NULL) {
            // if env var is set, use that
            // else:
            verse_root = root_from_binary();
        }
        int dirlen = strlen(verse_root) + 4; // + "src/"
        int pathlen = strlen(imp);
        char *tmp = malloc(sizeof(char) * (dirlen + pathlen + 1));
        snprintf(tmp, dirlen + pathlen + 1, "%ssrc/%s", verse_root, imp);
        return tmp;
    }
    return imp;
}

Package *load_package(int from_line, char *current_file, Scope *scope, char *path) {
    path = package_path_from_import_string(path);
    Package *p = package_previously_loaded(path);
    if (p) {
        for (int i = 0; i < array_len(scope->imported_packages); i++) {
            if (!strcmp(scope->imported_packages[i]->path, path)) {
                return p;
            }
        }
        array_push(scope->imported_packages, p);
        return p;
    }
    p = new_package(package_name(path), path);

    char **filenames = package_source_files(lineno(), current_file, path);
    if (!filenames) {
        error(lineno(), current_file, "No verse source files found in package '%s' ('%s').", p->name, p->path);
    }

    push_current_package(p);
    for (int i = 0; i < array_len(filenames); i++) {
        Ast *file_ast = parse_source_file(from_line, current_file, filenames[i]);
        file_ast = first_pass(p->scope, file_ast);
        package_add_file(p, filenames[i], file_ast);
    }
    pop_current_package();

    array_push(all_packages, p);
    array_push(scope->imported_packages, p);
    return p;
}

char **package_source_files(int from_line, char *from_file, char *package_path) {
    DIR *d = opendir(package_path);
    if (!d) {
        error(from_line, from_file, "Could not load package with path '%s': %s", package_path, strerror(errno));
    }
    char **out = NULL;
    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type != DT_REG) {
            continue;
        }
        int namelen = strlen(ent->d_name);
        if (!file_is_verse_source(ent->d_name, namelen)) {
            continue;
        }
        int len = strlen(package_path) + namelen;
        char *filepath = malloc(sizeof(char) * (len + 2));
        snprintf(filepath, len + 2, "%s/%s", package_path, ent->d_name);
        array_push(out, filepath);
    }
    closedir(d);
    return out;
}

Package *lookup_imported_package(Scope *scope, char *name) {
    // go to root
    while (scope->parent != NULL) {
        scope = scope->parent;
    }
    for (int i = 0; i < array_len(scope->imported_packages); i++) {
        if (!strcmp(scope->imported_packages[i]->name, name)) {
            return scope->imported_packages[i];
        }
    }
    return NULL;
}

int file_is_verse_source(char *name, int namelen) {
    if (namelen < 4) {
        return 0;
    }
    if (strcmp(name + namelen - 3, ".vs")) {
        return 0;
    }
    if (namelen > 9 && !strcmp(name + namelen - 8, "_test.vs")) {
        return 0;
    }
    return 1;
}
