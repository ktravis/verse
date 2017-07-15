#include <assert.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "array/array.h"
#include "ast.h"
#include "types.h"
#include "typechecking.h"
#include "util.h"

static int last_type_id = 0;

typedef struct MethodList {
    Type *type;
    char *name;
    Ast *decl;
    Scope *scope;
    struct MethodList *next;
} MethodList;

MethodList *all_methods = NULL;

// TODO: move on-demand reification somehwere else so that we don't have this
// weird dependency
Type *reify_struct(Scope *scope, Ast *ast, Type *type);

Ast *find_method(Type *t, char *name) {
    Ast *possible = NULL;
    // TODO: NEED TO ALLOW FOR list->type to be an alias (Array) and t be struct
    // (reified Array(string)
    for (MethodList *list = all_methods; list != NULL; list = list->next) {
        if (!strcmp(list->name, name)) {
            resolve_type(list->type);
            if (contains_generic_struct(list->type)) {
                list->type = reify_struct(list->scope, list->decl, list->type);
            }
            if (check_type(/*resolve_type*/(list->type), t)) {
                return list->decl;
            } else if (t->resolved->comp == STRUCT && t->resolved->st.generic_base != NULL) {
                assert(t->resolved->st.generic_base->resolved->comp == PARAMS);
                if (check_type(list->type, t->resolved->st.generic_base->resolved->params.inner)) {
                    // not an exact match, so don't return yet in case there is
                    // a specific one
                    possible = list->decl;
                }
            }
        }
    }
    return possible;
}

Ast *define_method(Scope *impl_scope, Type *t, Ast *decl) {
    assert(decl->type == AST_FUNC_DECL);
    for (MethodList *list = all_methods; list != NULL; list = list->next) {
        if (t->resolved && !strcmp(list->name, decl->fn_decl->var->name)) {
            if (t->id == list->type->id || t->resolved == list->type->resolved) {
                return list->decl;
            }
        }
    }
    MethodList *last = all_methods;
    all_methods = calloc(sizeof(MethodList), 1);
    all_methods->type = t;
    all_methods->name = decl->fn_decl->var->name;
    all_methods->scope = impl_scope;
    all_methods->decl = decl;
    all_methods->next = last;
    return NULL;
}

static ResolvedType *make_resolved(TypeComp comp) {
    ResolvedType *r = calloc(sizeof(ResolvedType), 1);
    r->comp = comp;
    return r;
}

Type *make_primitive(int base, int size) {
    TypeData *data = malloc(sizeof(TypeData));
    data->base = base;
    data->size = size;

    Type *type = calloc(sizeof(Type), 1);
    type->resolved = make_resolved(BASIC);
    type->resolved->data = data;
    type->id = last_type_id++;
    return type;
}

int size_of_type(Type *t) {
    t = resolve_type(t);
    if (t == NULL) {
        error(-1, "<internal>", "Could not resolve type to determine size: '%s'", t->name);
    }
    ResolvedType *r = t->resolved;
    switch (r->comp) {
    case BASIC:
        return r->data->size; // TODO: is this right?
    case STATIC_ARRAY:
        return r->array.length * size_of_type(r->array.inner);
    case ARRAY:
        return 16;
    case REF:
        return 8;
    case FUNC:
        return 8;
    case STRUCT: {
        int size = 0;
        for (int i = 0; i < array_len(r->st.member_types); i++) {
            size += size_of_type(r->st.member_types[i]);
        }
        return size;
    }
    case ENUM:
        return size_of_type(r->en.inner);
    default:
        break;
    }
    error(-1, "<internal>", "size_of_type went wrong");
    return -1;
}

Type *copy_type(Scope *scope, Type *t) {
    // Should this be separated into 2 functions, one that replaces scope and
    // the other that doesn't?
    /*if (t->resolved && t->resolved->comp == BASIC) {*/
        /*return t;*/
    /*}*/
    Type *type = calloc(sizeof(Type), 1);
    *type = *t;
    type->scope = scope;
    if (!type->resolved) {
        return type;
    }
    type->resolved = calloc(sizeof(ResolvedType), 1);
    *type->resolved = *t->resolved;
    ResolvedType *r = type->resolved;
    ResolvedType *cr = t->resolved;
    switch (r->comp) {
    case BASIC:
    case POLYDEF:
    case EXTERNAL:
        break;
    case PARAMS:
        r->params.args = array_copy(cr->params.args);
        for (int i = 0; i < array_len(cr->params.args); i++) {
            r->params.args[i] = copy_type(scope, cr->params.args[i]);
        }
        r->params.inner = copy_type(scope, cr->params.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        r->array.inner = copy_type(scope, cr->array.inner);
        break;
    case REF:
        r->ref.inner = copy_type(scope, cr->ref.inner);
        break;
    case FUNC:
        r->fn.args = array_copy(cr->fn.args);
        for (int i = 0; i < array_len(cr->fn.args); i++) {
            r->fn.args[i] = copy_type(scope, cr->fn.args[i]);
        }
        r->fn.ret = copy_type(scope, cr->fn.ret);
        break;
    case STRUCT:
        r->st.member_types = array_copy(cr->st.member_types);
        r->st.member_names = array_copy(cr->st.member_names);
        for (int i = 0; i < array_len(cr->st.member_types); i++) {
            r->st.member_types[i] = copy_type(scope, cr->st.member_types[i]);
            r->st.member_names[i] = malloc(sizeof(char) * strlen(cr->st.member_names[i] + 1));
            strcpy(r->st.member_names[i], cr->st.member_names[i]);
        }
        if (cr->st.generic) {
            r->st.arg_params = array_copy(cr->st.arg_params);
            for (int i = 0; i < array_len(cr->st.arg_params); i++) {
                r->st.arg_params[i] = copy_type(scope, cr->st.arg_params[i]);
            }
        }
        if (cr->st.generic_base) {
            r->st.generic_base = copy_type(scope, cr->st.generic_base);
        }
        break;
    case ENUM:
        r->en.inner = copy_type(scope, cr->en.inner);
        r->en.member_names = array_copy(cr->en.member_names);
        r->en.member_values = array_copy(cr->en.member_values);
        for (int i = 0; i < array_len(cr->st.member_types); i++) {
            r->en.member_values[i] = cr->en.member_values[i];
            r->en.member_names[i] = malloc(sizeof(char) * strlen(cr->en.member_names[i] + 1));
            strcpy(r->st.member_names[i], cr->st.member_names[i]);
        }
        break;
    }
    return type;
}

Type *make_type(Scope *scope, char *name) {
    Type *type = calloc(sizeof(Type), 1);
    type->name = name;
    type->scope = scope;
    type->id = last_type_id++;
    return type;
}

Type *make_polydef(Scope *scope, char *name) {
    Type *type = calloc(sizeof(Type), 1);
    type->name = name;
    type->scope = scope;
    type->resolved = make_resolved(POLYDEF);
    return type;
}

Type *make_ref_type(Type *inner) {
    Type *type = calloc(sizeof(Type), 1);
    type->id = last_type_id++;
    type->resolved = make_resolved(REF);
    type->resolved->ref.inner = inner;
    return type;
}

Type *make_fn_type(Type **args, Type *ret, int variadic) {
    Type *type = calloc(sizeof(Type), 1);
    type->id = last_type_id++;
    type->resolved = make_resolved(FUNC);
    type->resolved->fn.args = args;
    type->resolved->fn.ret = ret;
    type->resolved->fn.variadic = variadic;
    return type;
}

Type *make_static_array_type(Type *inner, long length) {
    Type *type = calloc(sizeof(Type), 1);
    type->id = last_type_id++;
    type->resolved = make_resolved(STATIC_ARRAY);
    type->resolved->array.inner = inner;
    type->resolved->array.length = length;
    return type;
}

Type *make_array_type(Type *inner) {
    Type *type = calloc(sizeof(Type), 1);
    type->id = last_type_id++;
    type->resolved = make_resolved(ARRAY);
    type->resolved->array.inner = inner;
    type->resolved->array.length = -1; // eh?
    return type;
}

Type *make_enum_type(Type *inner, char **member_names, long *member_values) {
    Type *type = calloc(sizeof(Type), 1);
    type->id = last_type_id++;
    type->resolved = make_resolved(ENUM);
    type->resolved->en.inner = inner;
    type->resolved->en.member_names = member_names;
    type->resolved->en.member_values = member_values;
    return type;
}

long enum_type_val(Type *t, int index) {
    assert(t != NULL);
    assert(t->resolved != NULL);
    assert(t->resolved->comp == ENUM);
    assert(index >= 0);
    assert(index < array_len(t->resolved->en.member_values));
    return t->resolved->en.member_values[index];
}

Type *make_struct_type(char **member_names, Type **member_types) {
    Type *s = calloc(sizeof(Type), 1);
    s->id = last_type_id++;
    s->resolved = make_resolved(STRUCT);
    s->resolved->st.generic_base = NULL;
    s->resolved->st.member_names = member_names;
    s->resolved->st.member_types = member_types;
    s->resolved->st.generic = 0;
    return s;
}

Type *make_generic_struct_type(char **member_names, Type **member_types, Type **params) {
    Type *s = make_struct_type(member_names, member_types);
    s->resolved->st.arg_params = params;
    s->resolved->st.generic = 1;
    return s;
}

Type *make_params_type(Type *inner, Type **params) {
    Type *t = calloc(sizeof(Type), 1);
    t->id = last_type_id++;
    t->resolved = make_resolved(PARAMS);
    t->resolved->params.inner = inner;
    t->resolved->params.args = params;
    return t;
}

Type *make_external_type(char *pkg, char *name) {
    Type *t = calloc(sizeof(Type), 1);
    t->id = last_type_id++;
    t->name = name;
    t->resolved = make_resolved(EXTERNAL);
    t->resolved->ext.pkg_name = pkg;
    t->resolved->ext.type_name = name;
    return t;
}

int precision_loss_uint(Type *t, unsigned long ival) {
    assert(t->resolved->comp == BASIC);
    if (t->resolved->data->size >= 8) {
        return 0;
    } else if (t->resolved->data->size >= 4) {
        return ival > UINT_MAX;
    } else if (t->resolved->data->size >= 2) {
        return ival > USHRT_MAX;
    } else if (t->resolved->data->size >= 1) {
        return ival > UCHAR_MAX;
    }
    return 1;
}

int precision_loss_int(Type *t, long ival) {
    assert(t->resolved->comp == BASIC);
    if (t->resolved->data->size >= 8) {
        return 0;
    } else if (t->resolved->data->size >= 4) {
        return ival >= INT_MAX;
    } else if (t->resolved->data->size >= 2) {
        return ival >= SHRT_MAX;
    } else if (t->resolved->data->size >= 1) {
        return ival >= CHAR_MAX;
    }
    return 1;
}

int precision_loss_float(Type *t, double fval) {
    assert(t->resolved->comp == BASIC);
    if (t->resolved->data->size >= 8) {
        return 0;
    } else {
        return fval >= FLT_MAX;
    }
    return 1;
}

Type *lookup_polymorph(Scope *s, char *name);
Type *lookup_type(Scope *s, char *name);
Type *lookup_local_type(Scope *s, char *name);
Package *lookup_imported_package(Scope *s, char *name);

Type *_resolve_polymorph(Type *type) {
    assert(type->name);
    Type *t = lookup_polymorph(type->scope, type->name);
    if (!t) {
        return type;
    }
    if (t != type) {
        assert(t->resolved);
        type->resolved = t->resolved;
        type->aka = t;
    }
    return type;
}

Type *resolve_polymorph_recursively(Type *type) {
    if (type->name) {
        return _resolve_polymorph(type);
    }
    Type *p = NULL;
    switch (type->resolved->comp) {
    case POLYDEF:
        break;
    case REF:
        p = resolve_polymorph_recursively(type->resolved->ref.inner);
        if (p) {
            type->resolved->ref.inner = p;
        }
        break;
    case ARRAY:
    case STATIC_ARRAY:
        p = resolve_polymorph_recursively(type->resolved->array.inner);
        if (p) {
            type->resolved->array.inner = p;
        }
        break;
    case STRUCT:
        for (int i = 0; i < array_len(type->resolved->st.member_types); i++) {
            p = resolve_polymorph_recursively(type->resolved->st.member_types[i]);
            if (p) {
                type->resolved->st.member_types[i] = p;
            }
        }
        break;
    case FUNC:
        for (int i = 0; i < array_len(type->resolved->fn.args); i++) {
            p = resolve_polymorph_recursively(type->resolved->fn.args[i]);
            if (p) {
                type->resolved->fn.args[i] = p;
            }
        }
        if (type->resolved->fn.ret) {
            p = resolve_polymorph_recursively(type->resolved->fn.ret);;
            if (p) {
                type->resolved->fn.ret = p; 
            }
        }
        break;
    case PARAMS:
        for (int i = 0; i < array_len(type->resolved->params.args); i++) {
            p = resolve_polymorph_recursively(type->resolved->params.args[i]);
            if (p) {
                type->resolved->params.args[i] = p;
            }
        }
        p = resolve_polymorph_recursively(type->resolved->params.inner);
        if (p) {
            type->resolved->params.inner = p;
        }
        break;
    case BASIC:
        break;
    default:
        break;
    }
    return type;
}

Type *resolve_type(Type *type) {
    if (!type) {
        return NULL;
    }
    if (type->name) {
        Type *found = NULL;
        if (type->resolved && type->resolved->comp != EXTERNAL) {
            // In this case, only check for polymorph. No need to check again
            // for resolution.
            found = _resolve_polymorph(type);
        } else {
            found = find_type_or_polymorph(type);
        }
        if (found) {
            if (type != found) {
                type->id = found->id;
                assert(found->resolved);
                type->resolved = found->resolved;
            }
        }
    }
    if (!type->resolved) {
        return NULL;
    }
    if (is_polydef(type)) {
        return type;
    }
    Type **used_types = all_used_types();
    for (int i = 0; i < array_len(used_types); i++) {
        if (check_type(used_types[i], type)) {
            if (used_types[i]->resolved) {
                type->resolved = used_types[i]->resolved;
            } else {
                assert(type->resolved);
                used_types[i]->resolved = type->resolved;
            }
            type->id = used_types[i]->id;
            return type;
        }
    }
    // find_type_or_polymorph should take care of this!
    assert(type->resolved->comp != EXTERNAL);
    switch (type->resolved->comp) {
    case POLYDEF:
        break;
    case PARAMS:
        for (int i = 0; i < array_len(type->resolved->params.args); i++) {
            type->resolved->params.args[i] = resolve_type(type->resolved->params.args[i]);
        }
        type->resolved->params.inner = resolve_type(type->resolved->params.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY: {
        Type *r = resolve_type(type->resolved->array.inner);
        if (r) {
            type->resolved->array.inner = r;
        }
        break;
    }
    case REF: {
        Type *r = resolve_type(type->resolved->ref.inner);
        if (r) {
            type->resolved->ref.inner = resolve_type(type->resolved->ref.inner);
        }
        break;
    }
    case FUNC:
        for (int i = 0; i < array_len(type->resolved->fn.args); i++) {
            Type *r = resolve_type(type->resolved->fn.args[i]);
            if (r) {
                type->resolved->fn.args[i] = r;
            }
        }
        type->resolved->fn.ret = resolve_type(type->resolved->fn.ret);
        break;
    case STRUCT:
        /*type->resolved->st.generic_base = resolve_type(type->resolved->st.generic_base);*/
        for (int i = 0; i < array_len(type->resolved->st.member_types); i++) {
            Type *r = resolve_type(type->resolved->st.member_types[i]);
            if (r) { // This is so that generic placeholders aren't overwritten, maybe find a way to prevent this from mattering earlier?
                type->resolved->st.member_types[i] = r;
            }
        }
        /*for (int i = 0; i < array_len(type->resolved->st.arg_params); i++) {*/
            /*type->resolved->st.arg_params[i] = resolve_type(type->resolved->st.arg_params[i]);*/
        /*}*/
        break;
    case ENUM:
        type->resolved->en.inner = resolve_type(type->resolved->en.inner);
        break;
    default:
        break;
    }
    return type;
}

Type *find_type_or_polymorph(Type *type) {
    if (!type) {
        return NULL;
    }
    type = _resolve_polymorph(type);
    if (type->resolved) {
        if (type->resolved->comp == EXTERNAL) {
            Package *p = lookup_imported_package(type->scope, type->resolved->ext.pkg_name); 
            if (p == NULL) {
                return NULL;
            }
            type = make_type(p->scope, type->resolved->ext.type_name);
            return find_type_or_polymorph(type);
        } else {
            return type;
        }
    }
    if (!type->name) {
        return type;
    }
    Scope *s = type->scope;
    assert(s);
    return lookup_type(s, type->name);
}

int is_numeric(Type *t) {
    if (t->resolved->comp != BASIC) {
        return 0;
    }
    int b = t->resolved->data->base;
    return b == INT_T || b == UINT_T || b == FLOAT_T;
}

// TODO: inline all this jazz?
int is_base_type(Type *t, int base) {
    assert(t->resolved);
    return t->resolved->comp == BASIC && t->resolved->data->base == base;
}

int is_void(Type *t) {
    return is_base_type(t, VOID_T);
}

int is_string(Type *t) {
    return is_base_type(t, STRING_T);
}

int is_bool(Type *t) {
    return is_base_type(t, BOOL_T);
}

int is_array(Type *t) {
    assert(t->resolved);
    return t->resolved->comp == ARRAY || t->resolved->comp == STATIC_ARRAY;
}

int is_dynamic(Type *t) {
    assert(t->resolved);

    switch (t->resolved->comp) {
    case BASIC:
        return t->resolved->data->base == STRING_T;
    case STRUCT:
        for (int i = 0; i < array_len(t->resolved->st.member_types); i++) {
            if (is_dynamic(t->resolved->st.member_types[i])) {
                return 1;
            }
        }
        return 0;
    case REF:
        return t->resolved->ref.owned;
    case STATIC_ARRAY:
        return is_dynamic(t->resolved->array.inner);
    default:
        break;
    }
    return 0;
}

int is_polydef(Type *t) {
    if (!t->resolved) {
        // or should this only be called on resolved?
        return 0;
    }
    if (t->resolved->comp == POLYDEF) {
        return 1;
    } else if (t->name) {
        assert(t->name[0] != '$');
        return 0;
    }
    switch (t->resolved->comp) {
    case POLYDEF:
        return 1;
    case REF:
        return is_polydef(t->resolved->ref.inner);
    case ARRAY:
    case STATIC_ARRAY:
        return is_polydef(t->resolved->array.inner);
    case STRUCT:
        for (int i = 0; i < array_len(t->resolved->st.member_types); i++) {
            if (is_polydef(t->resolved->st.member_types[i])) {
                return 1;
            }
        }
        return 0;
    case FUNC:
        for (int i = 0; i < array_len(t->resolved->fn.args); i++) {
            if (is_polydef(t->resolved->fn.args[i])) {
                return 1;
            }
        }
        return 0;
    case PARAMS:
        for (int i = 0; i < array_len(t->resolved->params.args); i++) {
            if (is_polydef(t->resolved->params.args[i])) {
                return 1;
            }
        }
        if (is_polydef(t->resolved->params.inner)) {
            return 1;
        }
        return 0;
    default:
        break;
    }
    return 0;
}

int is_concrete(Type *t) {
    assert(t->resolved);
    // TODO: this is to handle polymorphic function args. There is probably
    // a better way to deal with this, but not sure what it is at the moment.
    // also this doesn't look for cases other than alias? what is this even
    // doing at all?
    if (t == NULL) {
        return 1;
    }
    if (t->resolved->comp == STRUCT && t->resolved->st.generic) {
        return 0;
    }
    return 1;
}

int is_owned(Type *t) {
    switch (t->resolved->comp) {
    case REF:
        return t->resolved->ref.owned;
    case ARRAY:
        return t->resolved->array.owned;
    default:
        break;
    }
    return 0;
}

int contains_generic_struct(Type *t) {
    if (t->name) {
        return 0;
    }
    switch (t->resolved->comp) {
    case POLYDEF:
        return 0;
    case REF:
        return contains_generic_struct(t->resolved->ref.inner);
    case ARRAY:
    case STATIC_ARRAY:
        return contains_generic_struct(t->resolved->array.inner);
    case STRUCT:
        for (int i = 0; i < array_len(t->resolved->st.member_types); i++) {
            if (contains_generic_struct(t->resolved->st.member_types[i])) {
                return 1;
            }
        }
        return 0;
    case FUNC:
        for (int i = 0; i < array_len(t->resolved->fn.args); i++) {
            if (contains_generic_struct(t->resolved->fn.args[i])) {
                return 1;
            }
        }
        return 0;
    case PARAMS:
        return 1;
    default:
        break;
    }
    return 0;
}

/*Type *replace_type(Type *base, Type *from, Type *to) {*/
    /*if (base == from || base->id == from->id) {*/
        /*return to;*/
    /*}*/

    /*if (base->name && !base->resolved) {*/
        /*if (from->name && from->scope == base->scope && !strcmp(from->name, base->name)) {*/
            /*return to;*/
        /*}*/
        /*return base;*/
    /*}*/

    /*ResolvedType *r = base->resolved;*/
    /*switch (r->comp) {*/
    /*case PARAMS:*/
        /*for (int i = 0; i < array_len(r->params.args); i++) {*/
            /*r->params.args[i] = replace_type(r->params.args[i], from, to);*/
        /*}*/
        /*r->params.inner = replace_type(r->params.inner, from, to);*/
        /*break;*/
    /*case REF:*/
        /*r->ref.inner = replace_type(r->ref.inner, from, to);*/
        /*break;*/
    /*case ARRAY:*/
    /*case STATIC_ARRAY:*/
        /*r->array.inner = replace_type(r->array.inner, from, to);*/
        /*break;*/
    /*case STRUCT:*/
        /*for (int i = 0; i < array_len(r->st.member_types); i++) {*/
            /*r->st.member_types[i] = replace_type(r->st.member_types[i], from, to);*/
        /*}*/
        /*break;*/
    /*case FUNC:*/
        /*for (int i = 0; i < array_len(r->fn.args); i++) {*/
            /*r->fn.args[i] = replace_type(r->fn.args[i], from, to);*/
        /*}*/
        /*r->fn.ret = replace_type(r->fn.ret, from, to);*/
        /*break;*/
    /*default:*/
        /*break;*/
    /*}*/
    /*return base;*/
/*}*/

Type *replace_type_by_name(Type *base, char *from_name, Type *to) {
    if (base->name && !strcmp(from_name, base->name)) {
        return to;
    }
    if (!base->resolved) {
        return base;
    }

    Type *old = NULL;
    int changed = 0;
    ResolvedType *r = base->resolved;
    switch (r->comp) {
    case PARAMS:
        for (int i = 0; i < array_len(r->params.args); i++) {
            old = r->params.args[i];
            r->params.args[i] = replace_type_by_name(r->params.args[i], from_name, to);
            if (r->params.args[i] != old) {
                changed = 1;
            }
        }
        old = r->params.inner;
        r->params.inner = replace_type_by_name(r->params.inner, from_name, to);
        if (r->params.inner != old) {
            changed = 1;
        }
        break;
    case REF:
        old = r->ref.inner;
        r->ref.inner = replace_type_by_name(r->ref.inner, from_name, to);
        if (r->ref.inner != old) {
            changed = 1;
        }
        break;
    case ARRAY:
    case STATIC_ARRAY:
        old = r->array.inner;
        r->array.inner = replace_type_by_name(r->array.inner, from_name, to);
        if (r->array.inner != old) {
            changed = 1;
        }
        break;
    case STRUCT:
        for (int i = 0; i < array_len(r->st.member_types); i++) {
            old = r->st.member_types[i];
            r->st.member_types[i] = replace_type_by_name(r->st.member_types[i], from_name, to);
            if (r->st.member_types[i] != old) {
                changed = 1;
            }
        }
        break;
    case FUNC:
        for (int i = 0; i < array_len(r->fn.args); i++) {
            old = r->fn.args[i];
            r->fn.args[i] = replace_type_by_name(r->fn.args[i], from_name, to);
            if (old != r->fn.args[i]) {
                changed = 1;
            }
        }
        old = r->fn.ret;
        r->fn.ret = replace_type_by_name(r->fn.ret, from_name, to);
        if (old != r->fn.ret) {
            changed = 1;
        }
        break;
    default:
        break;
    }
    if (changed) {
        base->id = last_type_id++;
        register_type(base);
    }
    return base;
}

static int types_initialized = 0;
static Type *void_type = NULL;
static int void_type_id;
static Type *int_type = NULL;
static int int_type_id;
static Type *int8_type = NULL;
static int int8_type_id;
static Type *int16_type = NULL;
static int int16_type_id;
static Type *int32_type = NULL;
static int int32_type_id;
static Type *int64_type = NULL;
static int int64_type_id;
static Type *uint_type = NULL;
static int uint_type_id;
static Type *uint8_type = NULL;
static int uint8_type_id;
static Type *uint16_type = NULL;
static int uint16_type_id;
static Type *uint32_type = NULL;
static int uint32_type_id;
static Type *uint64_type = NULL;
static int uint64_type_id;
static Type *float_type = NULL;
static int float_type_id;
static Type *float32_type = NULL;
static int float32_type_id;
static Type *float64_type = NULL;
static int float64_type_id;
static Type *bool_type = NULL;
static int bool_type_id;
static Type *string_type = NULL;
static int string_type_id;
static Type *baseptr_type = NULL;
static int baseptr_type_id;
static Type *basetype_type = NULL;
static int basetype_type_id;
static Type *typeinfo_type = NULL;
static int typeinfo_type_id;
static Type *typeinfo_ref_type = NULL;
static int typeinfo_ref_type_id;
static Type *numtype_type = NULL;
static int numtype_type_id;
static Type *reftype_type = NULL;
static int reftype_type_id;
static Type *structmember_type = NULL;
static int structmember_type_id;
static Type *structtype_type = NULL;
static int structtype_type_id;
static Type *enumtype_type = NULL;
static int enumtype_type_id;
static Type *arraytype_type = NULL;
static int arraytype_type_id;
static Type *fntype_type = NULL;
static int fntype_type_id;
static Type *any_type = NULL;
static int any_type_id;

int is_any(Type *t) {
    return t->id == any_type_id;
}

Type *get_any_type() {
    return any_type;
}
int get_any_type_id() {
    return any_type_id;
}
int get_typeinfo_type_id() {
    return typeinfo_type_id;
}

char *type_to_string(Type *t) {
    if (t->name) {
        if (t->aka) {
            char *inner = type_to_string(t->aka);
            int n = strlen(t->name) + 7 + strlen(inner) + 1;
            char *name = malloc(sizeof(char) * n);
            snprintf(name, n, "%s (aka %s)", t->name, inner);
            return name;
        }
        char *name = malloc(sizeof(char) * (strlen(t->name) + 1));
        strcpy(name, t->name);
        return name;
    }
    assert(t->resolved);
    ResolvedType *r = t->resolved;

    switch (r->comp) {
    /*case BASIC:*/
        // this should error, right?
    /*case POLY:*/
    case POLYDEF: {
        if (t->aka) {
            char *inner = type_to_string(t->aka);
            int n = strlen(t->name) + 7 + strlen(inner) + 1;
            char *name = malloc(sizeof(char) * n);
            snprintf(name, n, "%s (aka %s)", t->name, inner);
            return name;
        }
        int n = strlen(t->name) + 1;
        char *name = malloc(sizeof(char) * n);
        snprintf(name, n, "%s", t->name);
        return name;
    }
    case STATIC_ARRAY: {
        char *inner = type_to_string(r->array.inner);
        int len = strlen(inner) + 3 + snprintf(NULL, 0, "%ld", r->array.length);
        char *dest = malloc(sizeof(char) * len);
        dest[len] = '\0';
        sprintf(dest, "[%ld]%s", r->array.length, inner);
        free(inner);
        return dest;
    }
    case ARRAY: {
        char *inner = type_to_string(r->array.inner);
        int len = strlen(inner) + 3;
        if (r->array.owned) {
            len += 1;
        }
        char *dest = malloc(sizeof(char) * len);
        snprintf(dest, len, "%s[]%s", r->array.owned ? "'" : "", inner);
        free(inner);
        return dest;
    }
    case REF: {
        char *inner = type_to_string(r->ref.inner);
        char *dest = malloc(sizeof(char) * (strlen(inner) + 2));
        dest[strlen(inner) + 1] = '\0';
        sprintf(dest, "%c%s", r->ref.owned ? '\'' : '&', inner);
        free(inner);
        return dest;
    }
    case FUNC: {
        int len = 5; // fn() + \0
        char **args = NULL;
        for (int i = 0; i < array_len(r->fn.args); i++) {
            char *name = type_to_string(r->fn.args[i]);
            len += strlen(name);
            array_push(args, name);
        }
        if (r->fn.variadic) {
            len += 3;
        }
        if (array_len(args) > 1) {
            len += array_len(args) - 1;
        }
        char *ret = NULL;
        if (r->fn.ret != void_type) {
            ret = type_to_string(r->fn.ret);
            len += 2 + strlen(ret);
        }
        char *dest = malloc(sizeof(char) * len);
        dest[0] = '\0';
        snprintf(dest, 4, "fn(");
        for (int i = 0; i < array_len(args); i++) {
            if (i > 0) {
                strcat(dest, ",");
            }
            strcat(dest, args[i]);
            free(args[i]);
        }
        if (r->fn.variadic) {
            strcat(dest, "...");
        }
        array_free(args);
        strcat(dest, ")");
        if (ret != NULL) {
            strcat(dest, "->");
            strcat(dest, ret);
            free(ret);
        }
        return dest;
    }
    case STRUCT: {
        if (r->st.generic_base != NULL) {
            return type_to_string(r->st.generic_base);
        }
        int len = 9; // struct{} + \0
        int n = array_len(r->st.member_types);
        char **member_types = malloc(sizeof(char*) * n);
        for (int i = 0; i < n; i++) {
            len += strlen(r->st.member_names[i]);
            char *name = type_to_string(r->st.member_types[i]);
            len += 1+strlen(name);
            member_types[i] = name;
        }
        if (n > 1) {
            len += n - 1;
        }
        char *start = malloc(sizeof(char) * len);
        /*dest[0] = '\0';*/
        /*dest[1] = '\0';*/
        char *dest = start;
        len -= snprintf(dest, len, "struct{");
        dest += 7;
        for (int i = 0; i < array_len(r->st.member_names); i++) {
            int n = 0;
            if (i != array_len(r->st.member_names) - 1) {
                n = snprintf(dest, len, "%s:%s,", r->st.member_names[i], member_types[i]);
            } else {
                n = snprintf(dest, len, "%s:%s}", r->st.member_names[i], member_types[i]);
            }
            len -= n;
            dest += n;
            free(member_types[i]);
        }
        free(member_types);
        return start;
    }
    case PARAMS: {
        char *left = type_to_string(r->params.inner);
        int len = strlen(left) + 3; // left<>\0
        // TODO: this is inefficient, let's do this a better way
        for (int i = 0; i < array_len(r->params.args); i++) {
            if (i > 0) {
                len++; // ,
            }
            len += strlen(type_to_string(r->params.args[i]));
        }
        char *name = malloc(sizeof(char) * len);
        name[0] = '\0'; // for strcat
        strcat(name, left);
        strcat(name, "(");
        for (int i = 0; i < array_len(r->params.args); i++) {
            strcat(name, type_to_string(r->params.args[i]));
            if (i > 0) {
                strcat(name, ",");
            }
        }
        strcat(name, ")");
        return name;

    }
    case EXTERNAL: {
        int len = strlen(r->ext.pkg_name) + strlen(r->ext.type_name) + 2;
        char *name = malloc(sizeof(char) * len);
        snprintf(name, len, "%s.%s", r->ext.pkg_name, r->ext.type_name);
        return name;
    }
    case ENUM:
        return "enum";
    default:
        error(-1, "internal", "but why bro");
        break;
    }
    return NULL;
}


// TODO inline?
Type *base_type(PrimitiveType t) {
    if (!types_initialized) {
        error (-1, "internal", "Must first initialize types before calling base_type(...)");
    }
    switch (t) {
    case INT_T:
        return int_type;
    case UINT_T:
        return uint_type;
    case FLOAT_T:
        return float_type;
    case BOOL_T:
        return bool_type;
    case STRING_T:
        return string_type;
    case VOID_T:
        return void_type;
    case BASEPTR_T:
        return baseptr_type;
    /*case TYPE_T:*/
        /*return typeinfo_type;*/
    /*case ANY_T:*/
        /*return any_type;*/
    default:
        error(-1, "internal", "cmon man");
    }
    return NULL;
}

Type *base_numeric_type(int t, int size) {
    if (!types_initialized) {
        error (-1, "internal", "Must first initialize types before calling base_type(...)");
    }
    switch (t) {
    case INT_T:
        switch (size) {
        case 8: return int8_type;
        case 16: return int16_type;
        case 32: return int32_type;
        case 64: return int64_type;
        }
    case UINT_T:
        switch (size) {
        case 8: return uint8_type;
        case 16: return uint16_type;
        case 32: return uint32_type;
        case 64: return uint64_type;
        }
    case FLOAT_T:
        switch (size) {
        case 32: return float32_type;
        case 64: return float64_type;
        }
    default:
        error(-1, "internal", "cmon man");
    }
    return NULL;
}

Type *typeinfo_ref() {
    return typeinfo_ref_type;
}

Type *define_type(Scope *s, char *name, Type *type, Ast *ast);
void register_type(Type *t);

void init_types(Scope *scope) {
    void_type = define_type(scope, "void", make_primitive(VOID_T, 0), NULL);
    void_type_id = void_type->id;

    int_type = define_type(scope, "int", make_primitive(INT_T, 8), NULL);
    int_type_id = int_type->id;
    int8_type = define_type(scope, "s8", make_primitive(INT_T, 1), NULL);
    int8_type_id = int8_type->id;
    int16_type = define_type(scope, "s16", make_primitive(INT_T, 2), NULL);
    int16_type_id = int16_type->id;
    int32_type = define_type(scope, "s32", make_primitive(INT_T, 4), NULL);
    int32_type_id = int32_type->id;
    int64_type = define_type(scope, "s64", make_primitive(INT_T, 8), NULL);
    int64_type_id = int64_type->id;

    uint_type = define_type(scope, "uint", make_primitive(UINT_T, 8), NULL);
    uint_type_id = uint_type->id;
    uint8_type = define_type(scope, "u8", make_primitive(UINT_T, 1), NULL);
    uint8_type_id = uint8_type->id;
    uint16_type = define_type(scope, "u16", make_primitive(UINT_T, 2), NULL);
    uint16_type_id = uint16_type->id;
    uint32_type = define_type(scope, "u32", make_primitive(UINT_T, 4), NULL);
    uint32_type_id = uint32_type->id;
    uint64_type = define_type(scope, "u64", make_primitive(UINT_T, 8), NULL);
    uint64_type_id = uint64_type->id;

    float_type = define_type(scope, "float", make_primitive(FLOAT_T, 4), NULL);
    float_type_id = float_type->id;
    float32_type = define_type(scope, "float32", make_primitive(FLOAT_T, 4), NULL);
    float32_type_id = float32_type->id;
    float64_type = define_type(scope, "float64", make_primitive(FLOAT_T, 8), NULL);
    float64_type_id = float64_type->id;

    bool_type = define_type(scope, "bool", make_primitive(BOOL_T, 1), NULL);
    bool_type_id = bool_type->id;

    string_type = define_type(scope, "string", make_primitive(STRING_T, 16), NULL);
    string_type_id = string_type->id;

    char **member_names = NULL;
    array_push(member_names, "INT");
    array_push(member_names, "BOOL");
    array_push(member_names, "FLOAT");
    array_push(member_names, "VOID");
    array_push(member_names, "ANY");
    array_push(member_names, "STRING");
    array_push(member_names, "ARRAY");
    array_push(member_names, "FN");
    array_push(member_names, "ENUM");
    array_push(member_names, "REF");
    array_push(member_names, "STRUCT");
    array_push(member_names, "PTR");
    long *member_values = NULL;
    array_push(member_values, 1);
    array_push(member_values, 2);
    array_push(member_values, 3);
    array_push(member_values, 4);
    array_push(member_values, 5);
    array_push(member_values, 6);
    array_push(member_values, 7);
    array_push(member_values, 8);
    array_push(member_values, 9);
    array_push(member_values, 10);
    array_push(member_values, 11);
    array_push(member_values, 12);
    basetype_type = define_type(scope, "BaseType", make_enum_type(int32_type, member_names, member_values), NULL);
    basetype_type_id = basetype_type->id;

    baseptr_type = define_type(scope, "ptr", make_primitive(BASEPTR_T, 8), NULL);
    baseptr_type_id = baseptr_type->id;

    // TODO can these be defined in a "basic.vs" ?
    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    Type **member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    typeinfo_type = define_type(scope, "Type", make_struct_type(member_names, member_types), NULL);
    typeinfo_type_id = typeinfo_type->id;

    typeinfo_ref_type = make_ref_type(typeinfo_type);
    typeinfo_ref_type_id = typeinfo_ref_type->id;
    register_type(typeinfo_ref_type);

    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    array_push(member_names, "size_in_bytes");
    array_push(member_names, "is_signed");
    member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    array_push(member_types, int_type);
    array_push(member_types, bool_type);
    numtype_type = define_type(scope, "NumType", make_struct_type(member_names, member_types), NULL);
    numtype_type_id = numtype_type->id;

    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    array_push(member_names, "owned");
    array_push(member_names, "inner");
    member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    array_push(member_types, bool_type);
    array_push(member_types, typeinfo_ref_type);
    reftype_type = define_type(scope, "RefType", make_struct_type(member_names, member_types), NULL);
    reftype_type_id = reftype_type->id;

    member_names = NULL;
    array_push(member_names, "name");
    array_push(member_names, "type");
    array_push(member_names, "offset");
    member_types = NULL;
    array_push(member_types, string_type);
    array_push(member_types, typeinfo_ref_type);
    array_push(member_types, uint_type);
    structmember_type = define_type(scope, "StructMember", make_struct_type(member_names, member_types), NULL);
    structmember_type_id = structmember_type->id;

    Type *structmember_array_type = make_array_type(structmember_type);
    register_type(structmember_array_type);

    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    array_push(member_names, "members");
    member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    array_push(member_types, structmember_array_type);
    structtype_type = define_type(scope, "StructType", make_struct_type(member_names, member_types), NULL);
    structtype_type_id = structtype_type->id;

    Type *string_array_type = make_array_type(string_type);
    register_type(string_array_type);
    Type *int64_array_type = make_array_type(int64_type);
    register_type(int64_array_type);

    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    array_push(member_names, "inner");
    array_push(member_names, "members");
    array_push(member_names, "values");
    member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    array_push(member_types, typeinfo_ref_type);
    array_push(member_types, string_array_type);
    array_push(member_types, int64_array_type);
    enumtype_type = define_type(scope, "EnumType", make_struct_type(member_names, member_types), NULL);
    enumtype_type_id = enumtype_type->id;

    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    array_push(member_names, "inner");
    array_push(member_names, "size");
    array_push(member_names, "is_static");
    array_push(member_names, "owned");
    member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    array_push(member_types, typeinfo_ref_type);
    array_push(member_types, int_type);
    array_push(member_types, bool_type);
    array_push(member_types, bool_type);
    arraytype_type = define_type(scope, "ArrayType", make_struct_type(member_names, member_types), NULL);
    arraytype_type_id = arraytype_type->id;

    Type *typeinfo_array_type = make_array_type(typeinfo_type);
    register_type(typeinfo_array_type);

    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    array_push(member_names, "args");
    array_push(member_names, "return_type");
    array_push(member_names, "anonymous");
    member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    array_push(member_types, typeinfo_array_type);
    array_push(member_types, typeinfo_ref_type);
    array_push(member_types, bool_type);
    fntype_type = define_type(scope, "FnType", make_struct_type(member_names, member_types), NULL);
    fntype_type_id = fntype_type->id;

    member_names = NULL;
    array_push(member_names, "value_pointer");
    array_push(member_names, "type");
    member_types = NULL;
    array_push(member_types, baseptr_type);
    array_push(member_types, typeinfo_ref_type);
    any_type = define_type(scope, "Any", make_struct_type(member_names, member_types), NULL);
    any_type_id = any_type->id;

    types_initialized = 1;
}

void emit_typeinfo_decl(Scope *scope, Type *t) {
    int id = t->id;
    int base_id = typeinfo_type_id;
    assert(t->resolved);

    ResolvedType *r = t->resolved;

    switch (r->comp) {
    case REF:
        base_id = reftype_type_id;
        break;
    case STRUCT:
        printf("struct _type_vs_%d _type_info%d_members[%d];\n", structmember_type_id, id, array_len(r->st.member_types));
        base_id = structtype_type_id;
        break;
    case STATIC_ARRAY:
    case ARRAY:
        base_id = arraytype_type_id;
        break;
    case FUNC:
        printf("struct _type_vs_%d *_type_info%d_args[%d];\n", typeinfo_type_id, id, array_len(r->fn.args));
        base_id = fntype_type_id;
        break;
    case ENUM:
        printf("struct string_type _type_info%d_members[%d] = {\n", id, array_len(r->en.member_values));
        for (int i = 0; i < array_len(r->en.member_names); i++) {
            printf("  {%ld, \"%s\"},\n", strlen(r->en.member_names[i]), r->en.member_names[i]);
        }
        printf("};\n");
        printf("int64_t _type_info%d_values[%d] = {\n", id, array_len(r->en.member_values));
        for (int i = 0; i < array_len(r->en.member_values); i++) {
            printf(" %ld,\n", r->en.member_values[i]);
        }
        printf("};\n");
        base_id = enumtype_type_id;
        break;
    case BASIC:
        switch (r->data->base) {
        case INT_T:
        case UINT_T:
        case FLOAT_T:
            base_id = numtype_type_id;
            break;
        case BASEPTR_T:
        case STRING_T:
        case BOOL_T:
            break;
        }
    default:
        break;
    }
    printf("struct _type_vs_%d _type_info%d;\n", base_id, id);
}

void indent();

void emit_string_struct(char *str) {
    printf("{%ld,\"", strlen(str));
    print_quoted_string(str);
    printf("\"}");
}

void emit_typeinfo_init(Scope *scope, Type *t) {
    int id = t->id;

    char *name = type_to_string(t); // eh?
    ResolvedType *r = t->resolved;

    switch (r->comp) {
    case ENUM:
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 9, ", id, enumtype_type_id, id);
        emit_string_struct(name);
        printf(", (struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, r->en.inner->id);
        printf("{%d, _type_info%d_members}, {%d, _type_info%d_values}};\n",
                array_len(r->en.member_names), id, array_len(r->en.member_names), id);
        break;
    case REF:
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 10, ", id, reftype_type_id, id);
        emit_string_struct(name);
        printf(", %d, (struct _type_vs_%d *)&_type_info%d};\n", r->ref.owned, typeinfo_type_id, r->ref.inner->id);
        break;
    case STRUCT: {
        int offset = 0;
        for (int i = 0; i < array_len(r->st.member_names); i++) {
            indent();
            printf("_type_info%d_members[%d] = (struct _type_vs_%d){", id, i, structmember_type_id);
            emit_string_struct(r->st.member_names[i]);
            printf(", (struct _type_vs_%d *)&_type_info%d, %d};\n",
                   typeinfo_type_id, r->st.member_types[i]->id, offset);
            offset += size_of_type(r->st.member_types[i]);
        }
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 11, ", id, structtype_type_id, id);
        emit_string_struct(name);
        printf(", {%d, _type_info%d_members}};\n", array_len(r->st.member_names), id);
        break;
    }
    case STATIC_ARRAY:
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 7, ", id, arraytype_type_id, id);
        emit_string_struct(name);
        printf(", (struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, r->array.inner->id);
        printf("%ld, %d, %d};\n", r->array.length, 1, 0);
        break;
    case ARRAY: // TODO make this not have a name? switch Type to have enum in name slot for base type
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 7, ", id, arraytype_type_id, id);
        emit_string_struct(name);
        printf(", (struct _type_vs_%d *)&_type_info%d, %ld, %d, %d};\n",
                typeinfo_type_id, r->array.inner->id, (long)0, 0, r->array.owned);
        break;
    case FUNC: {
        for (int i = 0; i < array_len(r->fn.args); i++) {
            indent();
            printf("_type_info%d_args[%d] = (struct _type_vs_%d *)&_type_info%d;\n",
                id, i, typeinfo_type_id, r->fn.args[i]->id);
        }
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 8, ", id, fntype_type_id, id);
        emit_string_struct(name);
        printf(", {%d, (struct _type_vs_%d **)_type_info%d_args}, ", array_len(r->fn.args), typeinfo_type_id, id);

        Type *ret = r->fn.ret;
        if (ret->resolved->comp == BASIC && ret->resolved->data->base == VOID_T) {
            printf("NULL, ");
        } else {
            printf("(struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, r->fn.ret->id);
        }
        printf("0};\n");
        break;
    }
    case BASIC: {
        switch (r->data->base) {
        case INT_T:
        case UINT_T:
            indent();
            printf("_type_info%d = (struct _type_vs_%d){%d, 1, ", id, numtype_type_id, id);
            emit_string_struct(name);
            printf(", %d, %d};\n", r->data->size, r->data->base == INT_T);
            break;
        case FLOAT_T:
            indent();
            printf("_type_info%d = (struct _type_vs_%d){%d, 3, ", id, numtype_type_id, id);
            emit_string_struct(name);
            printf(", %d, 1};\n", r->data->size);
            break;
        case BASEPTR_T:
            indent();
            printf("_type_info%d = (struct _type_vs_%d){%d, 12, ", id, typeinfo_type_id, id);
            emit_string_struct(name);
            printf("};\n");
            break;
        case STRING_T:
            indent();
            printf("_type_info%d = (struct _type_vs_%d){%d, 6, ", id, typeinfo_type_id, id);
            emit_string_struct(name);
            printf("};\n");
            break;
        case BOOL_T:
            indent();
            printf("_type_info%d = (struct _type_vs_%d){%d, 2, ", id, typeinfo_type_id, id);
            emit_string_struct(name);
            printf("};\n");
            break;
        }
    }
    default:
        break;
    }
}
