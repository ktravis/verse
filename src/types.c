#include "types.h"
#include "util.h"

static StructType *struct_defs = NULL;

char* type_as_str(Type *t) {
    switch (t->base) {
    case BOOL_T: return "bool";
    case INT_T: return "int";
    case STRING_T: return "string";
    case BASEPTR_T: return "ptr";
    case PTR_T: {
        char *inner = type_as_str(t->inner);
        char *buf = malloc(sizeof(char) * (strlen(inner) + 2));
        buf[0] = '^';
        strncpy(buf+1, inner, strlen(inner));
        buf[strlen(inner)+1] = 0;
        return buf;
    }
    case FN_T: {
         char *parts[10]; // TODO duh
         size_t size = 6; // fn + ( + ) + : + \0
         TypeList *args = t->args;
         for (int i = 0; args != NULL; i++) {
             parts[i] = type_as_str(args->item);
             size += strlen(parts[i]);
             if (i < t->nargs - 1) {
                 size += 1; // + ','
             }
             args = args->next;
         }
         char *ret = type_as_str(t->ret);
         size_t retlen = strlen(ret);
         size += retlen;
         char *buf = malloc(sizeof(char) * size);
         strncpy(buf, "fn(", 3);
         char *m = buf + 3;
         // TODO not sure args is correct
         args = t->args;
         for (int i = 0; args != NULL; i++) {
             size_t n = strlen(parts[i]);
             strncpy(m, parts[i], n);
             m += n;
             if (args->item->base == FN_T) {
                 free(parts[i]);
             }
             args = args->next;
         }
         strncpy(m, "):", 2);
         m += 2;
         strncpy(m, ret, retlen);
         if (t->ret->base == FN_T) {
             free(ret);
         }
         buf[size-1] = 0;
         return buf;
    }
    case VOID_T: return "void";
    case AUTO_T: return "auto";
    case STRUCT_T: {
        StructType *s = get_struct_type(t->struct_id);
        if (s == NULL) {
            return "invalid struct";
        }
        size_t size = strlen(s->name) + 8;
        char *n = malloc(sizeof(char) * size);
        strncpy(n, "struct ", 7);
        strncpy(n+7, s->name, size - 8);
        n[size-1] = 0;
        return n;
    }
    }
    return "null";
}

Type *make_type(int base) {
    Type *t = malloc(sizeof(Type));
    t->held = 0;
    t->base = base;
    return t;
}

Type *make_fn_type(int nargs, TypeList *args, Type *ret) {
    Type *t = malloc(sizeof(Type));
    t->held = 0;
    t->base = FN_T;
    t->nargs = nargs;
    t->args = args;
    t->ret = ret;
    t->bindings = NULL;
    return t;
}

StructType *make_struct_type(char *name, int nmembers, char **member_names, Type **member_types) {
    StructType *s = malloc(sizeof(StructType));    
    s->name = name;
    s->nmembers = nmembers;
    s->member_names = member_names;
    s->member_types = member_types;
    s->id = struct_defs == NULL ? 0 : struct_defs->id + 1;
    s->next = struct_defs;
    struct_defs = s;
    return s;
}

Type *find_struct_type(char *name) {
    StructType *s = struct_defs;
    while (s != NULL) {
        if (!strcmp(s->name, name)) {
            Type *t = malloc(sizeof(Type));
            t->base = STRUCT_T;
            t->struct_id = s->id;
            return t;
        }
        s = s->next;
    }
    return NULL;
}

StructType *get_struct_type(int id) {
    StructType *s = struct_defs;
    while (s != NULL) {
        if (id == s->id) {
            break;
        }
        s = s->next;
    }
    return s;
}

int var_size(Type *t) {
    switch (t->base) {
    case BOOL_T: return sizeof(unsigned char);
    case INT_T: return sizeof(int);
    case FN_T:
    case STRING_T:
    case PTR_T:
    case BASEPTR_T:
        return sizeof(Type*);
    case VOID_T: return 0;
    case AUTO_T: error(-1, "var_size called on auto_t?");
    case STRUCT_T: {
        StructType *st = get_struct_type(t->struct_id);
        if (st == NULL) {
            error(-1, "invalid struct");
        }
        int size = 0;
        for (int i = 0; i < st->nmembers; i++) {
            size += var_size(st->member_types[i]);
        }
        return size;
    }
    }
    error(-1, "var_size fallthrough %d", t->base);
    return -1;
}

int add_binding(Type *t, Type *b) {
    TypeList *bindings = malloc(sizeof(TypeList));
    bindings->next = t->bindings;
    bindings->item = b;
    b->offset = t->bindings == NULL ? 0 : (t->bindings->item->offset + var_size(b));
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
