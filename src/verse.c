#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "array/array.h"
#include "compiler/ast.h"
#include "compiler/codegen.h"
#include "compiler/parse.h"
#include "compiler/package.h"
#include "compiler/semantics.h"
#include "compiler/types.h"
#include "compiler/util.h"
#include "compiler/var.h"

#include "prelude.h"

int main(int argc, char **argv) {
    if (argc == 2) {
        // TODO some sort of util function for opening a file and gracefully
        // handling errors (needs to be different for here vs. from #import
        push_file_source(argv[1], fopen(argv[1], "r"));
    } else if (argc == 1) {
        push_file_source("<stdin>", stdin);
    } else {
        printf("Usage:\n\t%s [source.vs]\n", argv[0]);
        exit(1);
    }
    
    Package *main_package = init_main_package(current_file_name());
    Scope *root_scope = main_package->scope;
    init_builtin_types();
    init_builtins();

    Ast *root = parse_block(0);
    root = check_semantics(root_scope, root);

    Var *main_var = NULL;

    printf("%.*s\n", prelude_length, prelude);

    /*TypeList *used_types = reverse_typelist(root_scope->used_types);*/
    Type **used_types = all_used_types();
    Type **builtins = builtin_types();

    // declare structs
    for (int i = 0; i < array_len(builtins); i++) {
        Type *t = resolve_alias(builtins[i]);
        if (t->comp == STRUCT) {
            emit_struct_decl(root_scope, t);
        }
    }
    for (int i = 0; i < array_len(used_types); i++) {
        Type *t = resolve_alias(used_types[i]);
        if (t->comp == STRUCT) {
            emit_struct_decl(root_scope, t);
        }
    }
    // declare other types
    for (int i = 0; i < array_len(builtins); i++) {
        emit_typeinfo_decl(root_scope, builtins[i]);
    }
    for (int i = 0; i < array_len(used_types); i++) {
        emit_typeinfo_decl(root_scope, used_types[i]);
    }

    // declare globals
    for (int i = 0; i < array_len(main_package->globals); i++) {
        emit_var_decl(root_scope, main_package->globals[i]);
    }
    Package **packages = all_loaded_packages();
    for (int i = 0; i < array_len(packages); i++) {
        Package *p = packages[i];
        for (int j = 0; j < array_len(p->globals); j++) {
            emit_var_decl(p->scope, p->globals[j]);
        }    
    }

    // init types
    printf("void _verse_init_typeinfo() {\n");
    change_indent(1);
    for (int i = 0; i < array_len(builtins); i++) {
        emit_typeinfo_init(root_scope, builtins[i]);
    }
    for (int i = 0; i < array_len(used_types); i++) {
        emit_typeinfo_init(root_scope, used_types[i]);
    }
    change_indent(-1);
    printf("}\n");

    Ast **fns = get_global_funcs();
    for (int i = 0; i < array_len(fns); i++) {
        Var *v = fns[i]->fn_decl->var;
        if (!strcmp(v->name, "main")) {
            main_var = v;
        }
        emit_forward_decl(root_scope, fns[i]->fn_decl);
    }
    for (int i = 0; i < array_len(fns); i++) {
        emit_func_decl(root_scope, fns[i]);
    }

    printf("void _verse_init() {\n");
    change_indent(1);

    for (int i = 0; i < array_len(packages); i++) {
        Package *p = packages[i];
        emit_scope_start(p->scope);
        for (int j = 0; j < array_len(p->files); j++) {
            compile(p->scope, p->files[j]->root);
        }
        emit_init_scope_end(p->scope);
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
