#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "compiler/array/array.h"
#include "compiler/ast.h"
#include "compiler/codegen.h"
#include "compiler/parse.h"
#include "compiler/package.h"
#include "compiler/semantics.h"
#include "compiler/types.h"
#include "compiler/util.h"
#include "compiler/var.h"

#include "prelude.h"

struct flag {
    char *short_name;
    char *long_name;
    char *help;
    int set;
    int expects_value;
    char *value;
} flag;

struct flag_set {
    union {
        struct {
            struct flag help_flag;
            struct flag output_flag;
        };
        struct flag set[2];
    };
};

void print_usage(struct flag_set *flags, char *bin_name) {
    fprintf(stderr, "Usage:\n\t%s [-flag]* [source.vs]\n", bin_name);
    if (flags) {
        int num_flags = sizeof(flags->set) / sizeof(struct flag);
        fprintf(stderr, "Flags:\n");
        for (int i = 0; i < num_flags; i++) {
            struct flag f = flags->set[i];
            if (f.short_name) {
                fprintf(stderr, "-%s, ", f.short_name);
            }
            fprintf(stderr, "-%s", f.long_name);
            if (f.expects_value) {
                fprintf(stderr, " <value>\t");
            } else {
                fprintf(stderr, "\t\t");
            }
            fprintf(stderr, "%s\n", f.help);
        }
    }
}

char **parse_flags_get_args(struct flag_set *flags, int argc, char **argv) {
    int num_flags = sizeof(flags->set) / sizeof(struct flag);
    char **args = NULL;
    struct flag *expecting_value = NULL;
    // NOTE: i = 1, skip binary name
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        int len = strlen(arg);
        if (expecting_value) {
            expecting_value->value = arg;
            expecting_value = NULL;
            continue;
        }
        if (len > 1 && arg[0] == '-') {
            int found = 0;
            for (int j = 0; j < num_flags; j++) {
                struct flag *f = &flags->set[j];
                if (!strcmp(arg+1, f->long_name) || !strcmp(arg+1, f->short_name)) {
                    found = 1;
                    f->set = 1;
                    if (f->expects_value) {
                        assert(expecting_value == NULL);
                        expecting_value = f;
                    }
                    break;
                }
            }
            if (!found) {
                errlog("Unknown flag '%s'", arg);
                print_usage(flags, argv[0]);
                exit(1);
            }
        } else {
            array_push(args, arg);
        }
    }
    if (expecting_value) {
        errlog("Expected value for flag '-%s'", expecting_value->long_name);
        print_usage(flags, argv[0]);
        exit(1);
    }
    return args;
}

int main(int argc, char **argv) {
    struct flag_set flags = {
        .help_flag   = {"h", "help", "print usage and exit", 0, 0, ""},
        .output_flag = {"o", "output", "specify output file, defaults to [input-base].c", 0, 1, ""},
    };

    char **args = parse_flags_get_args(&flags, argc, argv);

    if (array_len(args) != 1) {
        print_usage(&flags, argv[0]);
        exit(1);
    }

    char *base_name = "main";
    if (!strcmp(args[0], "-")) {
        push_file_source("<stdin>", stdin);
    } else {
        push_file_source(args[0], open_file_or_quit(args[0], "r"));
        base_name = strip_vs_ext(package_name(args[0]));
    }
    
    Package *main_package = init_main_package(current_file_name());
    Scope *root_scope = main_package->scope;
    init_builtin_types();
    init_builtins();

    Ast *root = parse_block(0);
    root = check_semantics(root_scope, root);

    // determine output file name, and open it
    FILE *output_file = stdout;
    char *output_filename = NULL;
    if (flags.output_flag.set) {
        if (!strcmp(flags.output_flag.value, "-")) {
            // go to stdout
        } else {
            output_filename = flags.output_flag.value;
        }
    } else {
        // use input file base as output file name
        output_filename = malloc(sizeof(char) * (strlen(base_name) + 3));
        sprintf(output_filename, "%s.c", base_name);
    }
    if (output_filename) {
        char *err;
        FILE *f = open_file_or_error(output_filename, "w", &err);
        if (!f) {
            fprintf(stderr, "Could not open file '%s' for output: %s\n", output_filename, err);
            exit(1);
        }
        output_file = f;
    }
    codegen_set_output(output_file);

    Var *main_var = NULL;

    write_bytes("%.*s\n", prelude_length, prelude);

    Type **used_types = all_used_types();
    Type **builtins = builtin_types();

    int *declared_type_ids = NULL;
    // declare structs
    for (int i = 0; i < array_len(builtins); i++) {
        recursively_declare_types(declared_type_ids, root_scope, builtins[i]);
    }
    for (int i = 0; i < array_len(used_types); i++) {
        recursively_declare_types(declared_type_ids, root_scope, used_types[i]);
    }
    // declare other types
    for (int i = 0; i < array_len(builtins); i++) {
        emit_typeinfo_decl(root_scope, builtins[i]);
    }
    array_free(declared_type_ids);
    declared_type_ids = NULL;
    for (int i = 0; i < array_len(used_types); i++) {
        char skip = 0;
        for (int j = 0; j < array_len(declared_type_ids); j++) {
            if (used_types[i]->id == declared_type_ids[j]) {
                skip = 1;
                break;
            }
        }
        if (!skip) {
            array_push(declared_type_ids, used_types[i]->id);
            emit_typeinfo_decl(root_scope, used_types[i]);
        }
    }
    array_free(declared_type_ids);
    declared_type_ids = NULL;

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
    emit_typeinfo_init_routine(root_scope, builtins, used_types);

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

    emit_init_routine(packages, root_scope, root, main_var);
    emit_entrypoint();

    if (!output_file) {
        return 0;
    }

    return 0;
}
