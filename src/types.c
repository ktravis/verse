#include "types.h"
#include "util.h"

static int last_type_id = 0;
static TypeList *type_defs = NULL;

Type *make_type(char *name, int base, int size) {
    Type *t = malloc(sizeof(Type));
    t->named = 1;
    t->held = 0;
    t->base = base;
    t->size = size;
    t->id = last_type_id++;
    if (name == NULL) {
        int l = snprintf(NULL, 0, "%d", t->id);
        name = malloc((l + 1) * sizeof(char));
        snprintf(name, l, "%d", t->id);
        name[l] = 0;
    }
    t->name = name;
    return t;
}

Type *make_fn_type(int nargs, TypeList *args, Type *ret) {
    char *parts[10]; // TODO duh
    size_t size = 6; // fn + ( + ) + : + \0
    TypeList *_args = args;
    for (int i = 0; _args != NULL; i++) {
        parts[i] = _args->item->name;
        size += strlen(parts[i]);
        if (i < nargs - 1) {
            size += 1; // + ','
        }
        _args = _args->next;
    }
    char *retname = ret->name;
    size_t retlen = strlen(retname);
    size += retlen;
    char *buf = malloc(sizeof(char) * size);
    strncpy(buf, "fn(", 3);
    char *m = buf + 3;
    _args = args;
    for (int i = 0; _args != NULL; i++) {
        size_t n = strlen(parts[i]);
        strncpy(m, parts[i], n);
        m += n;
        if (_args->item->base == FN_T) {
            free(parts[i]);
        }
        _args = _args->next;
    }
    strncpy(m, "):", 2);
    m += 2;
    strncpy(m, retname, retlen);
    if (ret->base == FN_T) {
        free(retname);
    }
    buf[size-1] = 0;

    Type *t = make_type(buf, FN_T, 8);
    t->nargs = nargs;
    t->args = args;
    t->ret = ret;
    t->bindings = NULL;
    t->named = 0;

    return t;
}

Type *make_ptr_type(Type *inner) {
    char *name = malloc((strlen(inner->name) + 2) * sizeof(char));
    sprintf(name, "^%s", inner->name);
    Type *type = make_type(name, PTR_T, 8);
    type->inner = inner;
    type->named = 0;
    return type;
}

Type *make_static_array_type(Type *inner, long length) {
    char *name = malloc((strlen(inner->name) + 2 + snprintf(NULL, 0, "%ld", length)) * sizeof(char));
    sprintf(name, "[%ld]%s", length, inner->name);
    Type *type = make_type(name, STATIC_ARRAY_T, length * inner->size);
    type->inner = inner;
    type->named = 0;
    type->length = length;
    return type;
}

Type *make_array_type(Type *inner) {
    char *name = malloc((strlen(inner->name) + 2) * sizeof(char));
    sprintf(name, "[]%s", inner->name);
    Type *type = make_type(name, ARRAY_T, 16);
    type->inner = inner;
    type->named = 0;
    return type;
}

Type *make_struct_type(char *name, int nmembers, char **member_names, Type **member_types) {
    int named = (name != NULL);
    if (!named) {
        int len = 8;
        for (int i = 0; i < nmembers; i++) {
            len += strlen(member_types[i]->name);
            if (i > 0) {
                len += 2; // comma and space
            }
        }
        name = malloc(sizeof(char) * (len + 1));
        sprintf(name, "struct{");
        int c = 7;
        for (int i = 0; i < nmembers; i++) {
            if (i > 0) {
                sprintf(name + (c++), ",");
            }
            sprintf(name + c, "%s", member_types[i]->name);
            c += strlen(member_types[i]->name);
        }
        sprintf(name + c, "}");
        name[len] = 0;
    }
    Type *s = make_type(name, STRUCT_T, 0);
    s->named = named;
    s->nmembers = nmembers;
    s->member_names = member_names;
    s->member_types = member_types;
    for (int i = 0; i < s->nmembers; i++) {
        s->size += s->member_types[i]->size;
    }
    return s;
}

Type *find_type(int id) {
    TypeList *types = type_defs;
    while (types != NULL) {
        if (types->item->id == id) {
            return types->item;
        }
        types = types->next;
    }
    return NULL;
}

Type *find_type_by_name(char *name) {
    TypeList *types = type_defs;
    while (types != NULL) {
        if (!strcmp(types->item->name, name)) {
            return types->item;
        }
        types = types->next;
    }
    return NULL;
}

int is_numeric(Type *t) {
    return t->base == INT_T || t->base == UINT_T || t->base == FLOAT_T;
}

int add_binding(Type *t, Type *b) {
    TypeList *bindings = malloc(sizeof(TypeList));
    bindings->next = t->bindings;
    bindings->item = b;
    b->offset = t->bindings == NULL ? 0 : (t->bindings->item->offset + b->size);
    t->bindings = bindings;
    return b->offset;
}

TypeList *typelist_append(TypeList *list, Type *t) {
    TypeList *tl = malloc(sizeof(TypeList));
    tl->item = t;
    tl->next = list;
    return tl;
}

TypeList *reverse_typelist(TypeList *list) {
    TypeList *tail = list;
    if (tail == NULL) {
        return NULL;
    }
    TypeList *head = tail;
    TypeList *tmp = head->next;
    head->next = NULL;
    while (tmp != NULL) {
        tail = head;
        head = tmp;
        tmp = tmp->next;
        head->next = tail;
    }
    return head;
}

Type *define_type(Type *t) {
    type_defs = typelist_append(type_defs, t);
    return t;
}

long array_size(Type *type) {
    Type *t = type;
    if (type->base == PTR_T) {
        t = type->inner;
    }
    switch (t->base) {
    case STATIC_ARRAY_T:
        return t->length;
    }
    error(-1, "Not an array, man ('%s').", type->name);
    return -1;
}

int can_cast(Type *from, Type *to) {
    switch (from->base) {
    case BASEPTR_T:
        return (to->base == PTR_T || to->base == BASEPTR_T);
    case PTR_T:
        return to->base == BASEPTR_T ||
            (to->base == INT_T && to->size == 8) ||
            (to->base == PTR_T && check_type(from->inner, to->inner));
    case FN_T:
        if (from->nargs == to->nargs && check_type(from->ret, to->ret)) {
            TypeList *from_args = from->args;
            TypeList *to_args = to->args;
            while (from_args != NULL) {
                if (!check_type(from_args->item, to_args->item)) {
                    return 0;
                }
                from_args = from_args->next;
                to_args = to_args->next;
            }
            return 1;
        }
        return 0;
    case STRUCT_T:
        for (int i = 0; i < to->nmembers; i++) {
            if (!check_type(from->member_types[i], to->member_types[i])) {
                return 0;
            }
        }
        return 1;
    case UINT_T:
        if (to->base == INT_T && to->size > from->size) { // TODO should we allow this? i.e. uint8 -> int
            return 1;
        }
        return from->base == to->base;
    case INT_T:
        if (from->size == 8 && (to->base == PTR_T || to->base == BASEPTR_T)) {
            return 1;
        }
        if (to->base == FLOAT_T && to->size >= from->size) {
            return 1;
        } // DO want fallthrough here
    default:
        return from->base == to->base;
    }
    return 0;
}

int precision_loss_uint(Type *t, unsigned long ival) {
    if (t->size >= 8) {
        return 0;
    } else if (t->size >= 4) {
        return ival >= UINT_MAX;
    } else if (t->size >= 2) {
        return ival >= USHRT_MAX;
    } else if (t->size >= 1) {
        return ival >= UCHAR_MAX;
    }
    return 1;
}

int precision_loss_int(Type *t, long ival) {
    if (t->size >= 8) {
        return 0;
    } else if (t->size >= 4) {
        return ival >= INT_MAX;
    } else if (t->size >= 2) {
        return ival >= SHRT_MAX;
    } else if (t->size >= 1) {
        return ival >= CHAR_MAX;
    }
    return 1;
}

int precision_loss_float(Type *t, double fval) {
    if (t->size >= 8) {
        return 0;
    } else {
        return fval >= FLT_MAX;
    }
    return 1;
}

int is_array(Type *t) {
    return t->base == ARRAY_T || t->base == STATIC_ARRAY_T || t->base == DYN_ARRAY_T;
}

int check_type(Type *a, Type *b) {
    if (a->base == BASEPTR_T) {
        return (b->base == PTR_T || b->base == BASEPTR_T);
    } else if (b->base == BASEPTR_T) {
        return (a->base == PTR_T);
    }
    if (a->named || b->named) {
        return a->id == b->id;
    }
    if (a->base == b->base) {
        if (a->base == FN_T) {
            if (a->nargs == b->nargs && check_type(a->ret, b->ret)) {
                TypeList *a_args = a->args;
                TypeList *b_args = b->args;
                while (a_args != NULL) {
                    if (!check_type(a_args->item, b_args->item)) {
                        return 0;
                    }
                    a_args = a_args->next;
                    b_args = b_args->next;
                }
                return 1;
            }
            return 0;
        } else if (a->base == STATIC_ARRAY_T) {
            return check_type(a->inner, b->inner) && (
                    (a->length == b->length && a->length != -1) ||
                    a->length == -1 ||
                    b->length == -1);
        } else if (a->base == DYN_ARRAY_T) {
            return check_type(a->inner, b->inner);
        } else if (a->base == STRUCT_T) {
            if (a->nmembers != b->nmembers) {
                return 0;
            }
            for (int i = 0; i < a->nmembers; i++) {
                if (!check_type(a->member_types[i], b->member_types[i])) {
                    return 0;
                }
            }
            return 1;
        } else if (a->base == PTR_T) {
            return check_type(a->inner, b->inner);
        }
        return 1;
    }
    return 0;
}

int type_can_coerce(Type *from, Type *to) {
    return is_array(from) && to->base == ARRAY_T;
}

int type_equality_comparable(Type *a, Type *b) {
    if (is_numeric(a)) {
        return is_numeric(b);
    } else if (a->base == PTR_T || a->base == BASEPTR_T) {
        return b->base == PTR_T || b->base == BASEPTR_T;
    }
    // TODO check for named here?
    return a->base == b->base;
}

int is_dynamic(Type *t) {
    if (t->base == STRUCT_T) {
        for (int i = 0; i < t->nmembers; i++) {
            if (is_dynamic(t->member_types[i])) {
                return 1;
            }
        }
        return 0;
    } else if (t->base == STATIC_ARRAY_T) {
        return is_dynamic(t->inner);
    }
    return t->base == STRING_T || (t->base == FN_T && t->bindings != NULL);
}

Type *promote_number_type(Type *a, Type *b) {
    if (a->base == FLOAT_T) {
        if (b->base != FLOAT_T) {
            return a; // TODO address precision loss if a->size < b->size
        }
    } else if (b->base == FLOAT_T) {
        if (a->base != FLOAT_T) {
            return b; // TODO address precision loss if b->size < a->size
        }
    }
    return a->size > b->size ? a : b;
}

static int types_initialized = 0;
static Type *auto_type = NULL;
static Type *void_type = NULL;
static Type *int_type = NULL;
static Type *int8_type = NULL;
static Type *int16_type = NULL;
static Type *int32_type = NULL;
static Type *int64_type = NULL;
static Type *uint_type = NULL;
static Type *uint8_type = NULL;
static Type *uint16_type = NULL;
static Type *uint32_type = NULL;
static Type *uint64_type = NULL;
static Type *float_type = NULL;
static Type *float32_type = NULL;
static Type *float64_type = NULL;
static Type *bool_type = NULL;
static Type *string_type = NULL;
static Type *baseptr_type = NULL;

// TODO inline?
Type *base_type(int t) {
    if (!types_initialized) {
        error (-1, "Must first initialize types before calling base_type(...)");
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
    case FN_T:
    case AUTO_T:
    case STRUCT_T:
    case PTR_T:
    case ARRAY_T:
    case DYN_ARRAY_T:
    default:
        error(-1, "cmon man");
    }
    return NULL;
}

Type *base_numeric_type(int t, int size) {
    if (!types_initialized) {
        error (-1, "Must first initialize types before calling base_type(...)");
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
        error(-1, "cmon man");
    }
    return NULL;
}

void init_types() {
    auto_type = define_type(make_type("auto", AUTO_T, -1));
    void_type = define_type(make_type("void", VOID_T, 0));

    int_type = define_type(make_type("int", INT_T, 4));
    int8_type = define_type(make_type("int8", INT_T, 1));
    int16_type = define_type(make_type("int16", INT_T, 2));
    int32_type = define_type(make_type("int32", INT_T, 4));
    int64_type = define_type(make_type("int64", INT_T, 8));

    uint_type = define_type(make_type("uint", UINT_T, 4));
    uint8_type = define_type(make_type("uint8", UINT_T, 1));
    uint16_type = define_type(make_type("uint16", UINT_T, 2));
    uint32_type = define_type(make_type("uint32", UINT_T, 4));
    uint64_type = define_type(make_type("uint64", UINT_T, 8));

    float_type = define_type(make_type("float", FLOAT_T, 4));
    float32_type = define_type(make_type("float32", FLOAT_T, 4));
    float64_type = define_type(make_type("float64", FLOAT_T, 8));

    bool_type = define_type(make_type("bool", BOOL_T, 1));

    string_type = define_type(make_type("string", STRING_T, 16)); // should be checked that non-pointer is used in bindings

    baseptr_type = define_type(make_type("ptr", BASEPTR_T, 8)); // should be checked that non-pointer is used in bindings
    types_initialized = 1;
}
