#ifndef PACKAGE_H
#define PACKAGE_H

#include "common.h"

Package *new_package(char *path, char *name);
Package *package_check_semantics(Package *p);
Package *get_current_package();
Package *get_main_package();
Package *init_main_package(char *path);
void define_global(Var *v);
Package **all_loaded_packages();
void push_current_package(Package *p);
Package *pop_current_package();
Package *load_package(int from_line, char *current_file, Scope *scope, char *path);
char *package_path_from_import_string(char *imp);
char **package_source_files(int from_line, char *from_file, char *package_path);
Package *lookup_imported_package(Scope *scope, char *name);
int file_is_verse_source(char *name, int namelen);

#endif // PACKAGE_H
