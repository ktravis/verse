#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../array/array.h"
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
    return var;
}

Var *copy_var(Scope *scope, Var *v) {
    Var *var = malloc(sizeof(Var));
    *var = *v;
    var->name = malloc(strlen(v->name)+1);
    strcpy(var->name, v->name); // just in case
    if (v->type != NULL) {
        var->type = copy_type(scope, v->type);
    }
    // TODO: check for type and members, proxy?
    // copy fn_decl?
    return var;
}

void init_struct_var(Var *var) {
    Type *type = resolve_alias(var->type);
    assert(type->comp == STRUCT);

    var->initialized = 1;
    var->members = NULL;

    for (int i = 0; i < array_len(type->st.member_names); i++) {
        int l = strlen(var->name)+strlen(type->st.member_names[i])+1;
        char *member_name;
        member_name = malloc((l+1)*sizeof(char));
        sprintf(member_name, "%s.%s", var->name, type->st.member_names[i]);
        member_name[l] = 0;

        Var *v = make_var(member_name, type->st.member_types[i]); // TODO
        v->initialized = 1; // maybe wrong?
        array_push(var->members, v);
    }
}
