#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "src/ast.h"
#include "src/codegen.h"
#include "src/parse.h"
#include "src/semantics.h"
#include "src/types.h"
#include "src/util.h"
#include "src/var.h"

#include "prelude.h"

int main(int argc, char **argv) {
    if (argc == 2) {
        // TODO some sort of util function for opening a file and gracefully
        // handling errors (needs to be different for here vs. from #import
        set_file_source(argv[1], fopen(argv[1], "r"));
    } else if (argc == 1) {
        set_file_source("<stdin>", stdin);
    } else {
        printf("Usage:\n\t%s [source.vs]\n", argv[0]);
        exit(1);
    }
    
    Scope *root_scope = new_scope(NULL);
    init_builtin_types();
    init_builtins();

    Ast *root = parse_block(0);
    // Should this go somewhere else?
    for (PkgList *list = all_loaded_packages(); list != NULL; list = list->next) {
        for (PkgFileList *files = list->item->files; files != NULL; files = files->next) {
            files->item->root = parse_semantics(list->item->scope, files->item->root);
        }
    }
    root = parse_semantics(root_scope, root);

    Var *main_var = NULL;

    printf("%.*s\n", prelude_length, prelude);

    TypeList *used_types = reverse_typelist(root_scope->used_types);
    TypeList *builtins = reverse_typelist(builtin_types());

    // declare structs
    for (TypeList *list = builtins; list != NULL; list = list->next) {
        Type *t = resolve_alias(list->item);
        if (t->comp == STRUCT) {
            emit_struct_decl(root_scope, t);
        }
    }
    for (TypeList *list = used_types; list != NULL; list = list->next) {
        Type *t = resolve_alias(list->item);
        if (t->comp == STRUCT) {
            emit_struct_decl(root_scope, t);
        }
    }
    // declare other types
    for (TypeList *list = builtins; list != NULL; list = list->next) {
        emit_typeinfo_decl(root_scope, list->item);
    }
    for (TypeList *list = used_types; list != NULL; list = list->next) {
        emit_typeinfo_decl(root_scope, list->item);
    }

    // declare globals
    for (VarList *vars = get_global_vars(); vars != NULL; vars = vars->next) {
        emit_var_decl(root_scope, vars->item);
    }

    // init types
    printf("void _verse_init_typeinfo() {\n");
    change_indent(1);
    for (TypeList *list = builtins; list != NULL; list = list->next) {
        emit_typeinfo_init(root_scope, list->item);
    }
    for (TypeList *list = used_types; list != NULL; list = list->next) {
        emit_typeinfo_init(root_scope, list->item);
    }
    change_indent(-1);
    printf("}\n");

    AstList *fnlist = get_global_funcs();
    while (fnlist != NULL) {
        Var *v = fnlist->item->fn_decl->var;
        if (!strcmp(v->name, "main")) {
            main_var = v;
        }
        emit_forward_decl(root_scope, fnlist->item->fn_decl);
        fnlist = fnlist->next;
    }

    fnlist = get_global_funcs();
    while (fnlist != NULL) {
        emit_func_decl(root_scope, fnlist->item);
        fnlist = fnlist->next;
    }

    printf("void _verse_init() {\n");
    change_indent(1);

    for (PkgList *list = all_loaded_packages(); list != NULL; list = list->next) {
        emit_scope_start(list->item->scope);
        for (PkgFileList *files = list->item->files; files != NULL; files = files->next) {
            compile(list->item->scope, files->item->root);
        }
        emit_scope_end(list->item->scope);
    }

    emit_scope_start(root_scope);
    compile(root_scope, root);
    emit_scope_end(root_scope);

    change_indent(-1);
    printf("}\n");

    printf("int main(int argc, char** argv) {\n"
           "    _verse_init_typeinfo();\n"
           "    _verse_init();\n");
    if (main_var != NULL) {
        printf("    return _vs_%d();\n}", main_var->id);
    } else {
        printf("    return 0;\n}");
    }
    return 0;
}
