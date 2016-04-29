#include "types.h"
#include "util.h"

static int last_type_id = 0;
static TypeList *type_defs = NULL;
/*static StructType *struct_defs = NULL;*/

/*char* type_as_str(Type *t) {*/
    /*return t->name;*/
/*}*/
/*char* type_as_str(Type *t) {*/
    /*switch (t->base) {*/
    /*case BOOL_T: return "bool";*/
    /*case INT_T: return "int";*/
    /*case STRING_T: return "string";*/
    /*case BASEPTR_T: return "ptr";*/
    /*case PTR_T: {*/
        /*char *inner = type_as_str(t->inner);*/
        /*char *buf = malloc(sizeof(char) * (strlen(inner) + 2));*/
        /*buf[0] = '^';*/
        /*strncpy(buf+1, inner, strlen(inner));*/
        /*buf[strlen(inner)+1] = 0;*/
        /*return buf;*/
    /*}*/
    /*case FN_T: {*/
         /*char *parts[10]; // TODO duh*/
         /*size_t size = 6; // fn + ( + ) + : + \0*/
         /*TypeList *args = t->args;*/
         /*for (int i = 0; args != NULL; i++) {*/
             /*parts[i] = type_as_str(args->item);*/
             /*size += strlen(parts[i]);*/
             /*if (i < t->nargs - 1) {*/
                 /*size += 1; // + ','*/
             /*}*/
             /*args = args->next;*/
         /*}*/
         /*char *ret = type_as_str(t->ret);*/
         /*size_t retlen = strlen(ret);*/
         /*size += retlen;*/
         /*char *buf = malloc(sizeof(char) * size);*/
         /*strncpy(buf, "fn(", 3);*/
         /*char *m = buf + 3;*/
         /*// TODO not sure args is correct*/
         /*args = t->args;*/
         /*for (int i = 0; args != NULL; i++) {*/
             /*size_t n = strlen(parts[i]);*/
             /*strncpy(m, parts[i], n);*/
             /*m += n;*/
             /*if (args->item->base == FN_T) {*/
                 /*free(parts[i]);*/
             /*}*/
             /*args = args->next;*/
         /*}*/
         /*strncpy(m, "):", 2);*/
         /*m += 2;*/
         /*strncpy(m, ret, retlen);*/
         /*if (t->ret->base == FN_T) {*/
             /*free(ret);*/
         /*}*/
         /*buf[size-1] = 0;*/
         /*return buf;*/
    /*}*/
    /*case VOID_T: return "void";*/
    /*case AUTO_T: return "auto";*/
    /*case STRUCT_T: {*/
        /*size_t size = strlen(t->name) + 8;*/
        /*char *n = malloc(sizeof(char) * size);*/
        /*strncpy(n, "struct ", 7);*/
        /*strncpy(n+7, t->name, size - 8);*/
        /*n[size-1] = 0;*/
        /*return n;*/
    /*}*/
    /*}*/
    /*return "null";*/
/*}*/

Type *make_type(char *name, int base, int size) {
    Type *t = malloc(sizeof(Type));
    t->named = 0;
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

    return t;
}

/*Type *make_type_def(char *name, Type *inner*/
Type *make_ptr_type(Type *inner) {
    char *name = malloc((strlen(inner->name) + 2) * sizeof(char));
    sprintf(name, "^%s", inner->name);
    Type *type = make_type(name, PTR_T, 8);
    type->inner = inner;
    return type;
}

Type *make_struct_type(char *name, int nmembers, char **member_names, Type **member_types) {
    if (name == NULL) {
        int len = 8;
        for (int i = 0; i < nmembers; i++) {
            len += strlen(member_types[i]->name);
            if (i < nmembers - 1) {
                len += 2; // comma and space
            }
        }
        name = malloc(sizeof(char) * (len + 1));
        sprintf(name, "struct{");
        int c = 7;
        for (int i = 0; i < nmembers; i++) {
            int n = strlen(member_types[i]->name);
            if (i > 0) {
                sprintf(name + c, member_types[i]->name, ", %s");
                c += n + 2;
            } else {
                sprintf(name + c, member_types[i]->name, "%s");
                c += n;
            }
        }
        sprintf(name + c, "}");
        name[len] = 0;
    }
    Type *s = make_type(name, STRUCT_T, 0);
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

/*StructType *get_struct_type(int id) {*/
    /*StructType *s = struct_defs;*/
    /*while (s != NULL) {*/
        /*if (id == s->id) {*/
            /*break;*/
        /*}*/
        /*s = s->next;*/
    /*}*/
    /*return s;*/
/*}*/

/*int var_size(Type *t) {*/
    /*switch (t->base) {*/
    /*case BOOL_T: return sizeof(unsigned char);*/
    /*case INT_T: return sizeof(int);*/
    /*case FN_T:*/
    /*case STRING_T:*/
    /*case PTR_T:*/
    /*case BASEPTR_T:*/
        /*return sizeof(Type*);*/
    /*case VOID_T: return 0;*/
    /*case AUTO_T: error(-1, "var_size called on auto_t?");*/
    /*case STRUCT_T: {*/
        /*StructType *st = get_struct_type(t->struct_id);*/
        /*if (st == NULL) {*/
            /*error(-1, "invalid struct");*/
        /*}*/
        /*int size = 0;*/
        /*for (int i = 0; i < st->nmembers; i++) {*/
            /*size += var_size(st->member_types[i]);*/
        /*}*/
        /*return size;*/
    /*}*/
    /*}*/
    /*error(-1, "var_size fallthrough %d", t->base);*/
    /*return -1;*/
/*}*/

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
