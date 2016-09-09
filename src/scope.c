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
Type *define_polymorph(Scope *s, Type *poly, Type *type) {
    assert(poly->comp == POLYDEF);
    if (lookup_local_type(s, poly->name) != NULL) {
        error(-1, "internal", "Type '%s' already declared within this scope.", poly->name);
    }

    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = poly->name;
    td->type = type;
    td->next = s->types;

    s->types = td;

    Type *t = make_poly(s, poly->name, type->id);

    while (s->parent != NULL) {
        s = s->parent;
    }
    s->used_types = typelist_append(s->used_types, type);
    return t;
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

    // TODO: why? so we can deifned used_types only on root?
    while (s->parent != NULL) {
        s = s->parent;
    }
    // maybe just do this somewhere else
    s->used_types = typelist_append(s->used_types, type);
    return type;
}
int local_type_name_conflict(Scope *scope, char *name) {
    for (TypeDef *td = scope->types; td != NULL; td = td->next) {
        if (!strcmp(td->name, name)) {
            return 1;
        }
    }
    return 0;
}
TypeDef *find_type_definition(Type *t) {
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
