#include "var.h"

static int last_var_id = 0;

int new_var_id() {
    return last_var_id++;
}

Var *make_var(char *name, Type *type) {
    Var *var = calloc(sizeof(Var), 1);

    var->name = malloc(strlen(name)+1);
    strcpy(var->name, name);

    var->id = new_var_id();
    var->type = type;

    if (type->base == STRUCT_T) {
        var->initialized = 1;
        var->members = malloc(sizeof(Var*)*type->st.nmembers);
        for (int i = 0; i < type->st.nmembers; i++) {
            int l = strlen(name)+strlen(type->st.member_names[i])+1;
            char *member_name;
            if (type->held) {
                member_name = malloc((l+2)*sizeof(char));
                sprintf(member_name, "%s->%s", name, type->st.member_names[i]);
                member_name[l+1] = 0;
            } else {
                member_name = malloc((l+1)*sizeof(char));
                sprintf(member_name, "%s.%s", name, type->st.member_names[i]);
                member_name[l] = 0;
            }
            var->members[i] = make_var(member_name, type->st.member_types[i]); // TODO
            var->members[i]->initialized = 1; // maybe wrong?
        }
    }
    return var;
}

VarList *varlist_append(VarList *list, Var *v) {
    VarList *vl = malloc(sizeof(VarList));
    vl->item = v;
    vl->next = list;
    return vl;
}

VarList *varlist_remove(VarList *list, char *name) {
    if (list != NULL) {
        if (!strcmp(list->item->name, name)) {
            return list->next;
        }
        VarList *curr = NULL;
        VarList *last = list;
        while (last->next != NULL) {
            curr = last->next;
            if (!strcmp(curr->item->name, name)) {
                last->next = curr->next;
                break;
            }
            last = curr;
        }
    }
    return list;
}

Var *varlist_find(VarList *list, char *name) {
    Var *v = NULL;
    while (list != NULL) {
        if (!strcmp(list->item->name, name)) {
            v = list->item;
            break;
        }
        list = list->next;
    }
    return v;
}

VarList *reverse_varlist(VarList *list) {
    VarList *tail = list;
    if (tail == NULL) {
        return NULL;
    }
    VarList *head = tail;
    VarList *tmp = head->next;
    head->next = NULL;
    while (tmp != NULL) {
        tail = head;
        head = tmp;
        tmp = tmp->next;
        head->next = tail;
    }
    return head;
}
