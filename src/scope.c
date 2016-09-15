#include "scope.h"

// --- Scope ---

Scope *new_scope(Scope *parent) {
    Scope *s = calloc(sizeof(Scope), 1);
    s->parent = parent;
    s->type = parent != NULL ? Simple : Root;
    return s;
}
Scope *new_fn_scope(Scope *parent) {
    Scope *s = new_scope(parent);
    s->type = Function;
    return s;
}
Scope *new_loop_scope(Scope *parent) {
    Scope *s = new_scope(parent);
    s->type = Loop;
    return s;
}
Scope *closest_loop_scope(Scope *scope) {
    while (scope->parent != NULL && scope->type != Loop) {
        scope = scope->parent;
    }
    return scope;
}
Scope *closest_fn_scope(Scope *scope) {
    while (scope->parent != NULL && scope->type != Function) {
        scope = scope->parent;
    }
    return scope;
}
Type *fn_scope_return_type(Scope *scope) {
    scope = closest_fn_scope(scope);
    if (scope == NULL) {
        return NULL;
    }
    // TODO: don't resolve this?
    return resolve_alias(scope->fn_var->type->fn.ret);
}

Type *lookup_local_type(Scope *s, char *name) {
    if (s->polymorph != NULL) {
        for (TypeDef *td = s->polymorph->defs; td != NULL; td = td->next) {
            if (!strcmp(td->name, name)) {
                return td->type;
            }
        }
    }
    for (TypeDef *td = s->types; td != NULL; td = td->next) {
        if (!strcmp(td->name, name)) {
            return td->type;
        }
    }
    return NULL;
}
Type *lookup_type(Scope *s, char *name) {
    for (; s != NULL; s = s->parent) {
        Type *t = lookup_local_type(s, name);
        if (t != NULL) {
            return t;
        }
    }
    return NULL;
}
void _register_type(Scope *s, Type *t) {
    t = resolve_alias(t); // eh?
    if (t == NULL) {
        return;
    }
    for (TypeList *list = s->used_types; list != NULL; list = list->next) {
        if (list->item->id == t->id) {
            return;
        }
    }
    s->used_types = typelist_append(s->used_types, t);
    switch (t->comp) {
    case STRUCT:
        for (int i = 0; i < t->st.nmembers; i++) {
            _register_type(s, t->st.member_types[i]);
        }
        break;
    case ENUM:
        _register_type(s, t->en.inner);
        break;
    case STATIC_ARRAY:
        _register_type(s, t->array.inner);
        break;
    case ARRAY:
    case REF:
        _register_type(s, t->inner);
        break;
    case FUNC:
        for (TypeList *list = t->fn.args; list != NULL; list = list->next) {
            _register_type(s, list->item);
        }
        _register_type(s, t->fn.ret);
        break;
    default:
        break;
    }
}
void register_type(Scope *s, Type *t) {
    while (s->parent != NULL) {
        s = s->parent;
    }
    _register_type(s, t);
}
Type *define_polymorph(Scope *s, Type *poly, Type *type) {
    assert(poly->comp == POLYDEF);
    assert(s->polymorph != NULL);
    if (lookup_local_type(s, poly->name) != NULL) {
        error(-1, "internal", "Type '%s' already declared within this scope.", poly->name);
    }

    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = poly->name;
    td->type = type;
    td->next = s->polymorph->defs;

    s->polymorph->defs = td;

    return make_poly(s, poly->name, type->id);
}
Type *define_type(Scope *s, char *name, Type *type) {
    if (lookup_local_type(s, name) != NULL) {
        error(-1, "internal", "Type '%s' already declared within this scope.", name);
    }

    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = name;
    td->type = type;
    td->next = s->types;

    s->types = td;

    type = make_type(s, name);

    register_type(s, type);
    return type;
}
int local_type_name_conflict(Scope *scope, char *name) {
    return lookup_local_type(scope, name) != NULL;
}
TypeDef *find_type_definition(Type *t) {
    // handle t->comp == POLY? or is this covered by resolve_polymorph?
    assert(t->comp == ALIAS);
    Scope *scope = t->scope;
    while (scope != NULL) {
        for (TypeDef *td = scope->types; td != NULL; td = td->next) {
            if (!strcmp(td->name, t->name)) {
                return td;
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

void attach_var(Scope *scope, Var *var) {
    scope->vars = varlist_append(scope->vars, var);
}

void detach_var(Scope *scope, Var *var) {
    scope->vars = varlist_remove(scope->vars, var->name);
}

Var *lookup_var(Scope *scope, char *name) {
    Var *v = lookup_local_var(scope, name);
    int in_function = scope->type == Function;
    while (v == NULL && scope->parent != NULL) {
        v = lookup_var(scope->parent, name);
        if (v != NULL) {
            if (!in_function || v->constant) {
                return v;
            }
            v = NULL;
        }
        scope = scope->parent;
        in_function = in_function || scope->type == Function;
    }
    return v;
}

Var *lookup_local_var(Scope *scope, char *name) {
    return varlist_find(scope->vars, name);
}

Var *allocate_temp_var(Scope *scope, struct Ast *ast) {
    Var *v = make_var("", ast->var_type);
    v->temp = 1;
    TempVarList *list = malloc(sizeof(TempVarList));
    list->id = ast->id;
    list->var = v;
    list->next = scope->temp_vars;
    scope->temp_vars = list;
    attach_var(scope, v);
    return v;
}

Var *find_temp_var(Scope *scope, struct Ast *ast) {
    for (TempVarList *list = scope->temp_vars; list != NULL; list = list->next) {
        if (list->id == ast->id) {
            return list->var;
        }
    }
    return NULL;
}
