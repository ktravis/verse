#include "src/compiler.h"
#include "src/util.h"

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
    init_types(root_scope);
    init_builtins();

    Ast *root = parse_block_ast(root_scope, 0);
    root = parse_semantics(root_scope, root);

    Var *main_var = NULL;

    printf("%.*s\n", prelude_length, prelude);
    _indent = 0;

    VarList *varlist = get_global_vars();
    while (varlist != NULL) {
        emit_var_decl(varlist->item);
        varlist = varlist->next;
    }

    TypeList *reg = root_scope->types->used_types;
    TypeList *tmp = reg;
    while (tmp != NULL) {
        if (tmp->item->base == STRUCT_T) {
            emit_struct_decl(tmp->item);
        }
        tmp = tmp->next;
    }
    if (reg != NULL) {
        /*fprintf(stderr, "Types in use:\n");*/
        while (reg != NULL) {
            if (reg->item->unresolved) {
                error(-1, "internal", "Undefined type '%s'.", reg->item->name);
            }
            emit_typeinfo_decl(reg->item);
            reg = reg->next;
        }
    }

    printf("void _verse_init_typeinfo() {\n");
    _indent++;
    reg = get_used_types();
    while (reg != NULL) {
        emit_typeinfo_init(reg->item);
        reg = reg->next;
    }
    _indent--;
    printf("}\n");

    // TODO: factor these into global vars
    AstList *fnlist = get_global_funcs();
    while (fnlist != NULL) {
        Var *v = fnlist->item->fn_decl->var;
        if (!strcmp(v->name, "main")) {
            main_var = v;
        }
        emit_forward_decl(v);
        fnlist = fnlist->next;
    }

    fnlist = get_global_funcs();
    while (fnlist != NULL) {
        emit_func_decl(fnlist->item);
        fnlist = fnlist->next;
    }

    printf("void _verse_init() ");

    compile(root);

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
