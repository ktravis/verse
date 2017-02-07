#include <stdlib.h>

#include "polymorph.h"
#include "typechecking.h"

Polymorph *create_polymorph(AstFnDecl *decl, TypeList *arg_types) {
    Polymorph *p = malloc(sizeof(Polymorph));
    p->id = decl->polymorphs == NULL ? 0 : decl->polymorphs->id + 1;
    p->args = arg_types;
    p->next = decl->polymorphs;
    p->scope = new_scope(decl->scope);
    p->body = copy_ast_block(p->scope, decl->body); // p->scope or scope?
    decl->polymorphs = p;
    return p;
}

Polymorph *check_for_existing_polymorph(AstFnDecl *decl, TypeList *arg_types) {
    Polymorph *match = NULL;
    for (Polymorph *p = decl->polymorphs; p != NULL; p = p->next) {
        TypeList *types = arg_types;

        for (TypeList *list = p->args; list != NULL; list = list->next) {
            match = p;
            // NOTE: this is handled in the calling function now
            // if variadic and list->next == NULL
            /*if (decl->var->type->fn.variadic && list->next == NULL) {*/
                // for remaining call args
                /*for (TypeList *rem = types; rem->next != NULL; rem = rem->next) {*/
                    // if !check_type(arg, list->item->inner) || ...
                    /*if (!check_type(rem->item, list->item->inner)) {*/
                        /*match = NULL;*/
                        /*break;*/
                    /*}*/
                /*}*/
                /*break;*/
            /*}*/
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

