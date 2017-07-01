#include <stdlib.h>

#include "array/array.h"
#include "hashmap/hashmap.h"

#include "polymorph.h"
#include "typechecking.h"

Polymorph *create_polymorph(AstFnDecl *decl, Type **arg_types) {
    Polymorph *p = malloc(sizeof(Polymorph));
    p->id = array_len(decl->polymorphs);
    p->args = arg_types;
    p->scope = new_fn_scope(decl->scope);
    p->scope->fn_var = decl->var;
    p->scope->polymorph = p;
    p->body = copy_ast_block(p->scope, decl->body); // p->scope or scope?
    p->ret = NULL;
    p->var = NULL;
    hashmap_init(&p->defs);
    array_push(decl->polymorphs, p);
    return p;
}

Polymorph *check_for_existing_polymorph(AstFnDecl *decl, Type **arg_types) {
    Polymorph *match = NULL;
    for (int i = 0; i < array_len(decl->polymorphs); i++) {
        Polymorph *p = decl->polymorphs[i];
        for (int j = 0; j < array_len(p->args); j++) {
            match = p;
            if (!check_type(p->args[j], arg_types[j])) {
                match = NULL;
                break;
            }
        }
        if (match) {
            return match;
        }
    }
    return NULL;
}

