#include <assert.h>
#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../array/array.h"
#include "ast.h"
#include "types.h"
#include "typechecking.h"
#include "util.h"

static int last_type_id = 0;

typedef struct MethodList {
    Type *type;
    char *name;
    Ast *decl;
    struct MethodList *next;
} MethodList;

MethodList *all_methods = NULL;

Ast *find_method(Type *t, char *name) {
    t = resolve_alias(t);
    Ast *possible = NULL;
    // TODO: NEED TO ALLOW FOR list->type to be an alias (Array) and t be struct
    // (reified Array(string)
    for (MethodList *list = all_methods; list != NULL; list = list->next) {
        if (!strcmp(list->name, name)) {
            if (check_type(resolve_alias(list->type), t)) {
                return list->decl;
            } else if (t->comp == STRUCT && t->st.generic_base != NULL) {
                assert(t->st.generic_base->comp == PARAMS);
                if (check_type(list->type, t->st.generic_base->params.inner)) {
                    // not an exact match, so don't return yet in case there is
                    // a specific one
                    possible = list->decl;
                }
            }
        }
    }
    return possible;
}

Ast *define_method(Type *t, Ast *decl) {
    assert(decl->type == AST_FUNC_DECL);

    Type *resolved = resolve_alias(t);
    for (MethodList *list = all_methods; list != NULL; list = list->next) {
        if (list->type == resolved && !strcmp(list->name, decl->fn_decl->var->name)) {
            return list->decl;
        }
    }
    MethodList *last = all_methods;
    all_methods = calloc(sizeof(MethodList), 1);
    all_methods->type = t;
    all_methods->name = decl->fn_decl->var->name;
    all_methods->decl = decl;
    all_methods->next = last;
    return NULL;
}

Type *make_primitive(int base, int size) {
    TypeData *data = malloc(sizeof(TypeData));
    data->base = base;
    data->size = size;

    Type *type = calloc(sizeof(Type), 1);
    type->comp = BASIC;
    type->data = data;
    type->id = last_type_id++;
    return type;
}

int size_of_type(Type *t) {
    t = resolve_alias(t);
    if (t == NULL) {
        // TODO: internal error
        return -1;
    }
    switch (t->comp) {
    case BASIC:
        return t->data->size; // TODO: is this right?
    case STATIC_ARRAY:
        return t->array.length * size_of_type(t->array.inner);
    case ARRAY:
        return 16;
    case REF:
        return 8;
    case FUNC:
        return 8;
    case STRUCT: {
        int size = 0;
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            size += size_of_type(t->st.member_types[i]);
        }
        return size;
    }
    case ENUM:
        return size_of_type(t->en.inner);
    default:
        break;
    }
    error(-1, "<internal>", "size_of_type went wrong");
    return -1;
}

Type *copy_type(Scope *scope, Type *t) {
    // Should this be separated into 2 functions, one that replaces scope and
    // the other that doesn't?
    // TODO: change id?
    if (t->comp == BASIC) {
        return t;
    }
    Type *type = malloc(sizeof(Type));
    *type = *t;
    type->scope = scope;
    switch (t->comp) {
    case BASIC:
    case ALIAS:
    case POLYDEF:
    case EXTERNAL:
        break;
    case PARAMS:
        type->params.args = array_copy(t->params.args);
        for (int i = 0; i < array_len(type->params.args); i++) {
            type->params.args[i] = copy_type(scope, type->params.args[i]);
        }
        type->params.inner = copy_type(scope, t->params.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        type->array.inner = copy_type(scope, t->array.inner);
        break;
    case REF:
        type->ref.inner = copy_type(scope, t->ref.inner);
        break;
    case FUNC:
        type->fn.args = array_copy(t->fn.args);
        for (int i = 0; i < array_len(type->fn.args); i++) {
            type->fn.args[i] = copy_type(scope, type->fn.args[i]);
        }
        type->fn.ret = copy_type(scope, t->fn.ret);
        break;
    case STRUCT:
        type->st.member_types = array_copy(t->st.member_types);
        type->st.member_names = array_copy(t->st.member_names);
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            type->st.member_types[i] = copy_type(scope, t->st.member_types[i]);
            type->st.member_names[i] = malloc(sizeof(char) * strlen(t->st.member_names[i] + 1));
            strcpy(type->st.member_names[i], t->st.member_names[i]);
        }
        if (type->st.generic) {
            type->st.arg_params = array_copy(t->st.arg_params);
            for (int i = 0; i < array_len(type->st.arg_params); i++) {
                type->st.arg_params[i] = copy_type(scope, type->st.arg_params[i]);
            }
            type->st.generic_base = copy_type(scope, t->st.generic_base);
        }
        break;
    case ENUM:
        type->en.inner = copy_type(scope, t->en.inner);
        type->en.member_names = array_copy(t->en.member_names);
        type->en.member_values = array_copy(t->en.member_values);
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            type->en.member_values[i] = t->en.member_values[i];
            type->en.member_names[i] = malloc(sizeof(char) * strlen(t->en.member_names[i] + 1));
            strcpy(type->st.member_names[i], t->st.member_names[i]);
        }
        break;
    }
    return type;
}

Type *make_type(Scope *scope, char *name) {
    Type *type = calloc(sizeof(Type), 1);
    type->comp = ALIAS;
    type->name = name;
    type->scope = scope;
    type->id = last_type_id++;
    return type;
}

Type *make_polydef(Scope *scope, char *name) {
    Type *type = calloc(sizeof(Type), 1);
    type->comp = POLYDEF;
    type->name = name;
    type->scope = scope;
    return type;
}

Type *make_ref_type(Type *inner) {
    Type *type = calloc(sizeof(Type), 1);
    type->comp = REF;
    type->ref.inner = inner;
    type->id = last_type_id++;
    return type;
}

Type *make_fn_type(Type **args, Type *ret, int variadic) {
    Type *type = calloc(sizeof(Type), 1);
    type->comp = FUNC;
    type->fn.args = args;
    type->fn.ret = ret;
    type->fn.variadic = variadic;
    type->id = last_type_id++;
    return type;
}

Type *make_static_array_type(Type *inner, long length) {
    Type *type = calloc(sizeof(Type), 1);
    type->comp = STATIC_ARRAY;
    type->array.inner = inner;
    type->array.length = length;
    type->id = last_type_id++;
    return type;
}

Type *make_array_type(Type *inner) {
    Type *type = calloc(sizeof(Type), 1);
    type->comp = ARRAY;
    type->array.inner = inner;
    type->array.length = -1; // eh?
    type->id = last_type_id++;
    return type;
}

Type *make_enum_type(Type *inner, char **member_names, long *member_values) {
    Type *t = calloc(sizeof(Type), 1);
    t->comp = ENUM;
    t->en.inner = inner;
    t->en.member_names = member_names;
    t->en.member_values = member_values;
    t->id = last_type_id++;
    return t;
}

Type *make_struct_type(char **member_names, Type **member_types) {
    Type *s = calloc(sizeof(Type), 1);
    s->comp = STRUCT;
    s->st.generic_base = NULL;
    s->st.member_names = member_names;
    s->st.member_types = member_types;
    s->st.generic = 0;
    s->id = last_type_id++;
    return s;
}

Type *make_generic_struct_type(char **member_names, Type **member_types, Type **params) {
    Type *s = make_struct_type(member_names, member_types);
    s->st.arg_params = params;
    s->st.generic = 1;
    return s;
}

Type *make_params_type(Type *inner, Type **params) {
    Type *t = calloc(sizeof(Type), 1);
    t->comp = PARAMS;
    t->params.inner = inner;
    t->params.args = params;
    t->id = last_type_id++;
    return t;
}

Type *make_external_type(char *pkg, char *name) {
    Type *t = calloc(sizeof(Type), 1);
    t->comp = EXTERNAL;
    t->ext.pkg_name = pkg;
    t->ext.type_name = name;
    return t;
}

int precision_loss_uint(Type *t, unsigned long ival) {
    assert(t->comp == BASIC);
    if (t->data->size >= 8) {
        return 0;
    } else if (t->data->size >= 4) {
        return ival >= UINT_MAX;
    } else if (t->data->size >= 2) {
        return ival >= USHRT_MAX;
    } else if (t->data->size >= 1) {
        return ival >= UCHAR_MAX;
    }
    return 1;
}

int precision_loss_int(Type *t, long ival) {
    assert(t->comp == BASIC);
    if (t->data->size >= 8) {
        return 0;
    } else if (t->data->size >= 4) {
        return ival >= INT_MAX;
    } else if (t->data->size >= 2) {
        return ival >= SHRT_MAX;
    } else if (t->data->size >= 1) {
        return ival >= CHAR_MAX;
    }
    return 1;
}

int precision_loss_float(Type *t, double fval) {
    assert(t->comp == BASIC);
    if (t->data->size >= 8) {
        return 0;
    } else {
        return fval >= FLT_MAX;
    }
    return 1;
}

Type *lookup_type(Scope *s, char *name);
Type *lookup_local_type(Scope *s, char *name);
Package *lookup_imported_package(Scope *s, char *name);

Type *_resolve_polymorph(Type *type) {
    for (;;) {
        if (type->comp == POLYDEF) {
            Type *tmp = lookup_type(type->scope, type->name);
            if (!tmp) {
                break;
            }
            type = tmp;
        } else if (type->comp == ALIAS && type->scope->polymorph != NULL) {
            TypeDef **tmp = hashmap_get(&type->scope->polymorph->defs, type->name);
            if (!tmp) {
                break;
            }
            type = (*tmp)->type;
        } else {
            break;
        }
    }
    return type;
}

Type *resolve_polymorph(Type *type) {
    switch (type->comp) {
    case POLYDEF:
    case ALIAS:
        type = _resolve_polymorph(type);
        break;
    case REF:
        type->ref.inner = resolve_polymorph(type->ref.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        type->array.inner = resolve_polymorph(type->array.inner);
        break;
    case STRUCT:
        for (int i = 0; i < array_len(type->st.member_types); i++) {
            type->st.member_types[i] = resolve_polymorph(type->st.member_types[i]);
        }
        break;
    case FUNC:
        for (int i = 0; i < array_len(type->fn.args); i++) {
            type->fn.args[i] = resolve_polymorph(type->fn.args[i]);
        }
        if (type->fn.ret) {
            type->fn.ret = resolve_polymorph(type->fn.ret);
        }
        break;
    case PARAMS:
        for (int i = 0; i < array_len(type->params.args); i++) {
            type->params.args[i] = resolve_polymorph(type->params.args[i]);
        }
        type->params.inner = resolve_polymorph(type->params.inner);
        break;
    case BASIC:
    default:
        break;
    }
    return type;
}

Type *resolve_external(Type *type) {
    assert(type->comp == EXTERNAL);
    assert(type->scope != NULL);

    Package *p = lookup_imported_package(type->scope, type->ext.pkg_name); 
    if (p == NULL) {
        // TODO: decide on how this error behavior should work, where is it
        // caught, etc
        return NULL;
    }

    type = make_type(p->scope, type->ext.type_name);
    return type;
}

// TODO: this could be better
Type *resolve_alias(Type *type) {
    if (type == NULL) {
        return NULL;
    }
    type = _resolve_polymorph(type);
    Scope *s = type->scope;
    if (type->comp == EXTERNAL) {
        return resolve_alias(resolve_external(type));
    }
    if (s == NULL || type->comp != ALIAS) {
        return type;
    }

    Type *t = lookup_type(s, type->name);
    if (t != NULL && t->comp == ALIAS) {
        return resolve_alias(t);
    }
    return t;
}

TypeData *resolve_type_data(Type *t) {
    t = resolve_alias(t);
    if (t->comp != BASIC) {
        error(-1, "internal", "Can't resolve typedata on this");
    }
    return t->data;
}

int is_numeric(Type *t) {
    t = resolve_alias(t);
    if (t->comp != BASIC) {
        return 0;
    }
    int b = t->data->base;
    return b == INT_T || b == UINT_T || b == FLOAT_T;
}

// TODO: inline all this jazz?
int is_base_type(Type *t, int base) {
    t = resolve_alias(t);
    return t->comp == BASIC && t->data->base == base;
}

int is_string(Type *t) {
    return is_base_type(t, STRING_T);
}

int is_bool(Type *t) {
    return is_base_type(t, BOOL_T);
}

int is_array(Type *t) {
    t = resolve_alias(t);
    return t->comp == ARRAY || t->comp == STATIC_ARRAY;
}

int is_dynamic(Type *t) {
    t = resolve_alias(t);

    if (t->comp == BASIC) {
        return t->data->base == STRING_T;
    } else if (t->comp == STRUCT) {
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            if (is_dynamic(t->st.member_types[i])) {
                return 1;
            }
        }
        return 0;
    } else if (t->comp == REF) {
        return t->ref.owned;
    } else if (t->comp == STATIC_ARRAY) {
        return is_dynamic(t->array.inner);
    }
    return 0;
}

int is_polydef(Type *t) {
    switch (t->comp) {
    case POLYDEF:
        return 1;
    case REF:
        return is_polydef(t->ref.inner);
    case ARRAY:
    case STATIC_ARRAY:
        return is_polydef(t->array.inner);
    case STRUCT:
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            if (is_polydef(t->st.member_types[i])) {
                return 1;
            }
        }
        return 0;
    case FUNC:
        for (int i = 0; i < array_len(t->fn.args); i++) {
            if (is_polydef(t->fn.args[i])) {
                return 1;
            }
        }
        return 0;
    case PARAMS:
        for (int i = 0; i < array_len(t->params.args); i++) {
            if (is_polydef(t->params.args[i])) {
                return 1;
            }
        }
        if (is_polydef(t->params.inner)) {
            return 1;
        }
        return 0;
    default:
        break;
    }
    return 0;
}

int is_concrete(Type *t) {
    t = resolve_alias(t);
    // TODO: this is to handle polymorphic function args. There is probably
    // a better way to deal with this, but not sure what it is at the moment.
    // also this doesn't look for cases other than alias? what is this even
    // doing at all?
    if (t == NULL) {
        return 1;
    }
    if (t->comp == STRUCT && t->st.generic) {
        return 0;
    }
    return 1;
}

int is_owned(Type *t) {
    t = resolve_alias(t);
    assert(t != NULL);
    switch (t->comp) {
    case REF:
        return t->ref.owned;
    case ARRAY:
        return t->array.owned;
    default:
        break;
    }
    return 0;
}

int contains_generic_struct(Type *t) {
    switch (t->comp) {
    case POLYDEF:
        return 0;
    case REF:
        return contains_generic_struct(t->ref.inner);
    case ARRAY:
    case STATIC_ARRAY:
        return contains_generic_struct(t->array.inner);
    case STRUCT:
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            if (contains_generic_struct(t->st.member_types[i])) {
                return 1;
            }
        }
        return 0;
    case FUNC:
        for (int i = 0; i < array_len(t->fn.args); i++) {
            if (contains_generic_struct(t->fn.args[i])) {
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

Type *replace_type(Type *base, Type *from, Type *to) {
    if (base == from || base->id == from->id) {
        return to;
    }

    switch (base->comp) {
    case ALIAS:
        if (from->comp == ALIAS && from->scope == base->scope && !strcmp(from->name, base->name)) {
            return to;
        }
        break;
    case PARAMS:
        for (int i = 0; i < array_len(base->params.args); i++) {
            base->params.args[i] = replace_type(base->params.args[i], from, to);
        }
        base->params.inner = replace_type(base->params.inner, from, to);
        break;
    case REF:
        base->ref.inner = replace_type(base->ref.inner, from, to);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        base->array.inner = replace_type(base->array.inner, from, to);
        break;
    case STRUCT:
        for (int i = 0; i < array_len(base->st.member_types); i++) {
            base->st.member_types[i] = replace_type(base->st.member_types[i], from, to);
        }
        break;
    case FUNC:
        for (int i = 0; i < array_len(base->fn.args); i++) {
            base->fn.args[i] = replace_type(base->fn.args[i], from, to);
        }
        base->fn.ret = replace_type(base->fn.ret, from, to);
        break;
    default:
        break;
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
    // TODO: make these not call resolve_alias every time
    return resolve_alias(t)->id == any_type_id;
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
    switch (t->comp) {
    /*case BASIC:*/
        // this should error, right?
    case ALIAS: {
        char *name = malloc(sizeof(char) * (strlen(t->name) + 1));
        strcpy(name, t->name);
        return name;
    }
    /*case POLY:*/
    case POLYDEF: {
        Type *r = resolve_polymorph(t);
        if (t == r) {
            int n = strlen(t->name) + 1;
            char *name = malloc(sizeof(char) * n);
            snprintf(name, n, "%s", t->name);
            return name;
        }
        char *inner = type_to_string(r);
        int n = strlen(t->name) + 7 + strlen(inner) + 1;
        char *name = malloc(sizeof(char) * n);
        snprintf(name, n, "%s (aka %s)", t->name, inner);
        return name;
    }
    case STATIC_ARRAY: {
        char *inner = type_to_string(t->array.inner);
        int len = strlen(inner) + 3 + snprintf(NULL, 0, "%ld", t->array.length);
        char *dest = malloc(sizeof(char) * len);
        dest[len] = '\0';
        sprintf(dest, "[%ld]%s", t->array.length, inner);
        free(inner);
        return dest;
    }
    case ARRAY: {
        char *inner = type_to_string(t->array.inner);
        int len = strlen(inner) + 3;
        if (t->array.owned) {
            len += 1;
        }
        char *dest = malloc(sizeof(char) * len);
        snprintf(dest, len, "%s[]%s", t->array.owned ? "'" : "", inner);
        free(inner);
        return dest;
    }
    case REF: {
        char *inner = type_to_string(t->ref.inner);
        char *dest = malloc(sizeof(char) * (strlen(inner) + 2));
        dest[strlen(inner) + 1] = '\0';
        sprintf(dest, "%c%s", t->ref.owned ? '\'' : '&', inner);
        free(inner);
        return dest;
    }
    case FUNC: {
        int len = 5; // fn() + \0
        int i = 0;
        char **args = NULL;
        for (int i = 0; i < array_len(t->fn.args); i++) {
            char *name = type_to_string(t->fn.args[i]);
            len += strlen(name);
            array_push(args, name);
        }
        if (t->fn.variadic) {
            len += 3;
        }
        if (array_len(args) > 1) {
            len += i - 1;
        }
        char *ret = NULL;
        if (t->fn.ret != void_type) {
            ret = type_to_string(t->fn.ret);
            len += 2 + strlen(ret);
        }
        char *dest = malloc(sizeof(char) * len);
        dest[0] = '\0';
        snprintf(dest, 4, "fn(");
        for (int j = 0; j < i; j++) {
            strcat(dest, args[j]);
            if (j != i - 1) {
                strcat(dest, ",");
            }
            free(args[j]);
        }
        if (t->fn.variadic) {
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
        if (t->st.generic_base != NULL) {
            return type_to_string(t->st.generic_base);
        }
        int len = 9; // struct{} + \0
        int n = array_len(t->st.member_types);
        char **member_types = malloc(sizeof(char*) * n);
        for (int i = 0; i < n; i++) {
            len += strlen(t->st.member_names[i]);
            char *name = type_to_string(t->st.member_types[i]);
            len += strlen(name);
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
        for (int i = 0; i < n; i++) {
            int n = 0;
            if (i != n - 1) {
                n = snprintf(dest, len, "%s:%s,", t->st.member_names[i], member_types[i]);
            } else {
                n = snprintf(dest, len, "%s:%s}", t->st.member_names[i], member_types[i]);
            }
            len -= n;
            dest += n;
            free(member_types[i]);
        }
        free(member_types);
        return start;
    }
    case PARAMS: {
        char *left = type_to_string(t->params.inner);
        int len = strlen(left) + 3; // left<>\0
        // TODO: this is inefficient, let's do this a better way
        for (int i = 0; i < array_len(t->params.args); i++) {
            if (i > 0) {
                len++; // ,
            }
            len += strlen(type_to_string(t->params.args[i]));
        }
        char *name = malloc(sizeof(char) * len);
        name[0] = '\0'; // for strcat
        strcat(name, left);
        strcat(name, "(");
        for (int i = 0; i < array_len(t->params.args); i++) {
            strcat(name, type_to_string(t->params.args[i]));
            if (i > 0) {
                strcat(name, ",");
            }
        }
        strcat(name, ")");
        return name;

    }
    case EXTERNAL: {
        int len = strlen(t->ext.pkg_name) + strlen(t->ext.type_name) + 2;
        char *name = malloc(sizeof(char) * len);
        snprintf(name, len, "%s.%s", t->ext.pkg_name, t->ext.type_name);
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

Type *define_type(Scope *s, char *name, Type *type);
void register_type(Type *t);

void init_types(Scope *scope) {
    void_type = define_type(scope, "void", make_primitive(VOID_T, 0));
    void_type_id = resolve_alias(void_type)->id;

    int_type = define_type(scope, "int", make_primitive(INT_T, 8));
    int_type_id = resolve_alias(int_type)->id;
    int8_type = define_type(scope, "s8", make_primitive(INT_T, 1));
    int8_type_id = resolve_alias(int8_type)->id;
    int16_type = define_type(scope, "s16", make_primitive(INT_T, 2));
    int16_type_id = resolve_alias(int16_type)->id;
    int32_type = define_type(scope, "s32", make_primitive(INT_T, 4));
    int32_type_id = resolve_alias(int32_type)->id;
    int64_type = define_type(scope, "s64", make_primitive(INT_T, 8));
    int64_type_id = resolve_alias(int64_type)->id;

    uint_type = define_type(scope, "uint", make_primitive(UINT_T, 8));
    uint_type_id = resolve_alias(uint_type)->id;
    uint8_type = define_type(scope, "u8", make_primitive(UINT_T, 1));
    uint8_type_id = resolve_alias(uint8_type)->id;
    uint16_type = define_type(scope, "u16", make_primitive(UINT_T, 2));
    uint16_type_id = resolve_alias(uint16_type)->id;
    uint32_type = define_type(scope, "u32", make_primitive(UINT_T, 4));
    uint32_type_id = resolve_alias(uint32_type)->id;
    uint64_type = define_type(scope, "u64", make_primitive(UINT_T, 8));
    uint64_type_id = resolve_alias(uint64_type)->id;

    float_type = define_type(scope, "float", make_primitive(FLOAT_T, 4));
    float_type_id = resolve_alias(float_type)->id;
    float32_type = define_type(scope, "float32", make_primitive(FLOAT_T, 4));
    float32_type_id = resolve_alias(float32_type)->id;
    float64_type = define_type(scope, "float64", make_primitive(FLOAT_T, 8));
    float64_type_id = resolve_alias(float64_type)->id;

    bool_type = define_type(scope, "bool", make_primitive(BOOL_T, 1));
    bool_type_id = resolve_alias(bool_type)->id;

    string_type = define_type(scope, "string", make_primitive(STRING_T, 16));
    string_type_id = resolve_alias(string_type)->id;

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
    basetype_type = define_type(scope, "BaseType", make_enum_type(int32_type, member_names, member_values));
    basetype_type_id = resolve_alias(basetype_type)->id;

    baseptr_type = define_type(scope, "ptr", make_primitive(BASEPTR_T, 8));
    baseptr_type_id = resolve_alias(baseptr_type)->id;

    // TODO can these be defined in a "basic.vs" ?
    member_names = NULL;
    array_push(member_names, "id");
    array_push(member_names, "base");
    array_push(member_names, "name");
    Type **member_types = NULL;
    array_push(member_types, int_type);
    array_push(member_types, basetype_type);
    array_push(member_types, string_type);
    typeinfo_type = define_type(scope, "Type", make_struct_type(member_names, member_types));
    typeinfo_type_id = resolve_alias(typeinfo_type)->id;

    typeinfo_ref_type = make_ref_type(typeinfo_type);
    typeinfo_ref_type_id = resolve_alias(typeinfo_ref_type)->id;
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
    numtype_type = define_type(scope, "NumType", make_struct_type(member_names, member_types));
    numtype_type_id = resolve_alias(numtype_type)->id;

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
    reftype_type = define_type(scope, "RefType", make_struct_type(member_names, member_types));
    reftype_type_id = resolve_alias(reftype_type)->id;

    member_names = NULL;
    array_push(member_names, "name");
    array_push(member_names, "type");
    array_push(member_names, "offset");
    member_types = NULL;
    array_push(member_types, string_type);
    array_push(member_types, typeinfo_ref_type);
    array_push(member_types, uint_type);
    structmember_type = define_type(scope, "StructMember", make_struct_type(member_names, member_types));
    structmember_type_id = resolve_alias(structmember_type)->id;

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
    structtype_type = define_type(scope, "StructType", make_struct_type(member_names, member_types));
    structtype_type_id = resolve_alias(structtype_type)->id;

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
    enumtype_type = define_type(scope, "EnumType", make_struct_type(member_names, member_types));
    enumtype_type_id = resolve_alias(enumtype_type)->id;

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
    arraytype_type = define_type(scope, "ArrayType", make_struct_type(member_names, member_types));
    arraytype_type_id = resolve_alias(arraytype_type)->id;

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
    fntype_type = define_type(scope, "FnType", make_struct_type(member_names, member_types));
    fntype_type_id = resolve_alias(fntype_type)->id;

    member_names = NULL;
    array_push(member_names, "value_pointer");
    array_push(member_names, "type");
    member_types = NULL;
    array_push(member_types, baseptr_type);
    array_push(member_types, typeinfo_ref_type);
    any_type = define_type(scope, "Any", make_struct_type(member_names, member_types));
    any_type_id = resolve_alias(any_type)->id;

    types_initialized = 1;
}

void emit_typeinfo_decl(Scope *scope, Type *t) {
    int id = t->id;
    int base_id = typeinfo_type_id;

    Type *resolved = resolve_alias(t);

    switch (resolved->comp) {
    case REF:
        base_id = reftype_type_id;
        break;
    case STRUCT:
        printf("struct _type_vs_%d _type_info%d_members[%d];\n", structmember_type_id, id, array_len(resolved->st.member_types));
        base_id = structtype_type_id;
        break;
    case STATIC_ARRAY:
    case ARRAY:
        base_id = arraytype_type_id;
        break;
    case FUNC:
        printf("struct _type_vs_%d *_type_info%d_args[%d];\n", typeinfo_type_id, id, array_len(t->fn.args));
        base_id = fntype_type_id;
        break;
    case ENUM:
        printf("struct string_type _type_info%d_members[%d] = {\n", id, array_len(resolved->en.member_values));
        for (int i = 0; i < array_len(resolved->en.member_names); i++) {
            printf("  {%ld, \"%s\"},\n",
                   strlen(resolved->en.member_names[i]),
                          resolved->en.member_names[i]);
        }
        printf("};\n");
        printf("int64_t _type_info%d_values[%d] = {\n", id, array_len(resolved->en.member_values));
        for (int i = 0; i < array_len(resolved->en.member_values); i++) {
            printf(" %ld,\n", resolved->en.member_values[i]);
        }
        printf("};\n");
        base_id = enumtype_type_id;
        break;
    case BASIC:
        switch (resolved->data->base) {
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

int get_type_id(Type *t);

void emit_typeinfo_init(Scope *scope, Type *t) {
    int id = get_type_id(t);

    char *name = type_to_string(t); // eh?
    Type *resolved = resolve_alias(t);

    switch (resolved->comp) {
    case ENUM:
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 9, ", id, enumtype_type_id, id);
        emit_string_struct(name);
        printf(", (struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, get_type_id(resolved->en.inner));
        printf("{%d, _type_info%d_members}, {%d, _type_info%d_values}};\n",
                array_len(resolved->en.member_names), id, array_len(resolved->en.member_names), id);
        break;
    case REF:
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 10, ", id, reftype_type_id, id);
        emit_string_struct(name);
        printf(", %d, (struct _type_vs_%d *)&_type_info%d};\n", resolved->ref.owned, typeinfo_type_id, get_type_id(resolved->ref.inner));
        break;
    case STRUCT: {
        int offset = 0;
        for (int i = 0; i < array_len(resolved->st.member_names); i++) {
            indent();
            printf("_type_info%d_members[%d] = (struct _type_vs_%d){", id, i, structmember_type_id);
            emit_string_struct(resolved->st.member_names[i]);
            printf(", (struct _type_vs_%d *)&_type_info%d, %d};\n",
                   typeinfo_type_id, get_type_id(resolved->st.member_types[i]), offset);
            offset += size_of_type(resolved->st.member_types[i]);
        }
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 11, ", id, structtype_type_id, id);
        emit_string_struct(name);
        printf(", {%d, _type_info%d_members}};\n", array_len(resolved->st.member_names), id);
        break;
    }
    case STATIC_ARRAY:
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 7, ", id, arraytype_type_id, id);
        emit_string_struct(name);
        printf(", (struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, get_type_id(resolved->array.inner));
        printf("%ld, %d, %d};\n", resolved->array.length, 1, 0);
        break;
    case ARRAY: // TODO make this not have a name? switch Type to have enum in name slot for base type
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 7, ", id, arraytype_type_id, id);
        emit_string_struct(name);
        printf(", (struct _type_vs_%d *)&_type_info%d, %ld, %d, %d};\n",
                typeinfo_type_id, get_type_id(resolved->array.inner), (long)0, 0, resolved->array.owned);
        break;
    case FUNC: {
        for (int i = 0; i < array_len(resolved->fn.args); i++) {
            indent();
            printf("_type_info%d_args[%d] = (struct _type_vs_%d *)&_type_info%d;\n",
                id, i, typeinfo_type_id, get_type_id(resolved->fn.args[i]));
        }
        indent();
        printf("_type_info%d = (struct _type_vs_%d){%d, 8, ", id, fntype_type_id, id);
        emit_string_struct(name);
        printf(", {%d, (struct _type_vs_%d **)_type_info%d_args}, ", array_len(resolved->fn.args), typeinfo_type_id, id);

        Type *ret = resolve_alias(resolved)->fn.ret;
        if (ret->comp == BASIC && ret->data->base == VOID_T) {
            printf("NULL, ");
        } else {
            printf("(struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, get_type_id(resolved->fn.ret));
        }
        printf("0};\n");
        break;
    }
    case BASIC: {
        switch (resolved->data->base) {
        case INT_T:
        case UINT_T:
            indent();
            printf("_type_info%d = (struct _type_vs_%d){%d, 1, ", id, numtype_type_id, id);
            emit_string_struct(name);
            printf(", %d, %d};\n", resolved->data->size, resolved->data->base == INT_T);
            break;
        case FLOAT_T:
            indent();
            printf("_type_info%d = (struct _type_vs_%d){%d, 3, ", id, numtype_type_id, id);
            emit_string_struct(name);
            printf(", %d, 1};\n", resolved->data->size);
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
