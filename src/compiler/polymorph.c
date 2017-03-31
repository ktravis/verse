#include <stdlib.h>

#include "polymorph.h"
#include "typechecking.h"

Polymorph *create_polymorph(AstFnDecl *decl, TypeList *arg_types) {
    Polymorph *p = malloc(sizeof(Polymorph));
    p->id = decl->polymorphs == NULL ? 0 : decl->polymorphs->id + 1;
    p->args = arg_types;
    p->next = decl->polymorphs;
    p->scope = new_fn_scope(decl->scope);
    p->scope->fn_var = decl->var;
    p->scope->polymorph = p;
    p->body = copy_ast_block(p->scope, decl->body); // p->scope or scope?
    p->ret = NULL;
    decl->polymorphs = p;
    return p;
}

Polymorph *check_for_existing_polymorph(AstFnDecl *decl, TypeList *arg_types) {
    Polymorph *match = NULL;
    for (Polymorph *p = decl->polymorphs; p != NULL; p = p->next) {
        TypeList *types = arg_types;

        for (TypeList *list = p->args; list != NULL; list = list->next) {
            match = p;
            if (!check_type(list->item, types->item)) {
                match = NULL;
                break;
            }
            types = types->next;
        }
        if (match != NULL) {
            return match;
        }
    }
    return NULL;
}

