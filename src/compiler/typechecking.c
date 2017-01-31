#include <assert.h>
#include <string.h>

#include "ast.h"
#include "typechecking.h"
#include "scope.h"

int check_type(Type *a, Type *b) {
    /*a = resolve_polymorph(a);*/
    /*b = resolve_polymorph(b);*/
    if (a->comp == EXTERNAL) {
        a = resolve_external(a);
    }
    if (b->comp == EXTERNAL) {
        b = resolve_external(b);
    }
    if (a->comp != b->comp) {
        if (!((a->comp == ALIAS && b->comp == POLYDEF) ||
              (a->comp == POLYDEF && b->comp == ALIAS))) {
            return 0;
        }
    }
    switch (a->comp) {
    case BASIC:
    case ENUM:
        return a->id == b->id;
    case POLYDEF:
    case ALIAS:
        return find_type_definition(a) == find_type_definition(b);
    case REF:
        return check_type(a->inner, b->inner);
    case ARRAY:
        return check_type(a->inner, b->inner);
    case STATIC_ARRAY:
        return a->array.length == b->array.length && check_type(a->array.inner, b->array.inner);
    case STRUCT:
        if (a->st.nmembers != b->st.nmembers) {
            return 0;
        }
        for (int i = 0; i < a->st.nmembers; i++) {
            if (strcmp(a->st.member_names[i], b->st.member_names[i])) {
                return 0;
            }
            if (!check_type(a->st.member_types[i], b->st.member_types[i])) {
                return 0;
            }
        }
        return 1;
    case FUNC:
        if (a->fn.variadic != b->fn.variadic) {
            return 0;
        }
        TypeList *aargs = a->fn.args;
        TypeList *bargs = b->fn.args;
        for (;;) {
            if (aargs == NULL && bargs == NULL) {
                break;
            } else if (aargs == NULL || bargs == NULL) {
                return 0;
            }
            if (!check_type(aargs->item, bargs->item)) {
                return 0;
            }
            aargs = aargs->next;
            bargs = bargs->next;
        }
        return check_type(a->fn.ret, b->fn.ret);
    default:
        error(-1, "internal", "Cmon mang");
    }
    return 0;
}

int can_cast(Type *from, Type *to) {
    if (resolve_alias(from)->comp == ENUM) {
        from = resolve_alias(from);
        return can_cast(from->en.inner, to);
    } else if (resolve_alias(to)->comp == ENUM) {
        to = resolve_alias(to);
        return can_cast(from, to->en.inner);
    }

    switch (from->comp) {
    case REF:
        to = resolve_alias(to); 
        return to->comp == REF ||
            (to->comp == BASIC &&
                 (to->data->base == BASEPTR_T ||
                 (to->data->base == INT_T && to->data->size == 8)));
    case FUNC:
        to = resolve_alias(to);
        return check_type(from, to);
    case STRUCT:
        to = resolve_alias(to);
        return check_type(from, to);
    case BASIC:
        from = resolve_alias(from); // TODO: need this in other cases?
        to = resolve_alias(to);
        switch (from->data->base) {
        case BASEPTR_T:
            return (to->comp == BASIC && to->data->base == BASEPTR_T) || to->comp == REF;
        case UINT_T:
            if (to->comp != BASIC) {
                return 0;
            }
            if (to->data->base == INT_T && to->data->size > from->data->size) { // TODO should we allow this? i.e. uint8 -> int
                return 1;
            }
            return from->data->base == to->data->base;
        case INT_T:
            if (to->comp == REF && from->data->size == 8) {
                return 1;
            }
            if (to->comp == BASIC) {

                if (to->data->base == BASEPTR_T) {
                    return from->data->size == 8;
                } else if (to->data->base == FLOAT_T) {
                    return to->data->size >= from->data->size;
                /*} else if (to->data->base == UINT_T) {*/
                } else if (to->data->base == INT_T) {
                    return from->data->size <= to->data->size;
                }
            }
        }
        return from->data->base == to->data->base;
    default:
        from = resolve_alias(from);
        to = resolve_alias(to);
        return can_cast(from, to);
    }
    return 0;
}

int match_polymorph(Scope *scope, Type *expected, Type *got) {
    if (expected->comp == POLYDEF) {
        define_polymorph(scope, expected, got);
        return 1;
    }
    Type *res = resolve_alias(got);
    if (res->comp != expected->comp) {
        if (expected->comp == ARRAY && res->comp == STATIC_ARRAY) {
            return match_polymorph(scope, expected->inner, res->array.inner);
        }
        return 0;
    }
    switch (expected->comp) {
    case REF:
        return match_polymorph(scope, expected->inner, res->inner);
    case ARRAY:
        return match_polymorph(scope, expected->inner, res->inner);
    case FUNC:
        if (expected->fn.variadic != res->fn.variadic) {
            return 0;
        }
        TypeList *exp_args = expected->fn.args;
        TypeList *got_args = res->fn.args;
        for (;;) {
            if (exp_args == NULL && got_args == NULL) {
                break;
            } else if (exp_args == NULL || got_args == NULL) {
                return 0;
            }
            // TODO: not sure this was right
            if (is_polydef(exp_args->item)) {
                if (!match_polymorph(scope, exp_args->item, got_args->item)) {
                    return 0;
                }
            } else if (!check_type(exp_args->item, got_args->item)) {
                return 0;
            }
            exp_args = exp_args->next;
            got_args = got_args->next;
        }
        return check_type(expected->fn.ret, res->fn.ret);
    case STRUCT: // naw dog
    case STATIC_ARRAY: // can't use static array as arg can we?
    case PARAMS:
    case POLYDEF:
    /*case POLY:*/
    case BASIC:
    case ENUM:
    case EXTERNAL:
    case ALIAS:
        error(-1, "internal", "Cmon mang");
        return 0;
    }
    return 1;
}

Type *promote_number_type(Type *a, int left_lit, Type *b, int right_lit) {
    Type *aa = resolve_alias(a);
    assert(aa->comp == BASIC);
    int abase = aa->data->base;
    int asize = aa->data->size;

    Type *bb = resolve_alias(b);
    assert(bb->comp == BASIC);
    int bbase = bb->data->base;
    int bsize = aa->data->size;

    if (abase == FLOAT_T) {
        if (bbase != FLOAT_T) {
            return a; // TODO address precision loss if a->size < b->size
        }
    } else if (bbase == FLOAT_T) {
        if (abase != FLOAT_T) {
            return b; // TODO address precision loss if b->size < a->size
        }
    }
    if (left_lit) {
        return asize > bsize ? a : b;
    } else if (right_lit) {
        return bsize > asize ? b : a;
    }
    return asize > bsize ? a : b;
}

int type_equality_comparable(Type *a, Type *b) {
    /*a = resolve_polymorph(a);*/
    /*b = resolve_polymorph(b);*/

    if (is_numeric(a)) {
        return is_numeric(b);
    }
    if (a->comp == REF || (a->comp == BASIC && a->data->base == BASEPTR_T)) {
        return b->comp == REF || (b->comp == BASIC && b->data->base == BASEPTR_T);
    }

    return check_type(a, b); // TODO: something different here
}

int can_coerce_type_no_error(Scope *scope, Type *to, Ast *from) {
    if (is_any(to)) {
        if (!is_any(from->var_type) && !is_lvalue(from)) {
            allocate_ast_temp_var(scope, from);
        }
        return 1;
    }
    
    // (TODO: why was this comment here?)
    // resolve polymorphs
    Type *t = resolve_alias(to);
    if (t->comp == ARRAY) {
        if (from->var_type->comp == ARRAY) {
            return check_type(t->inner, from->var_type->inner);
        } else if (from->var_type->comp == STATIC_ARRAY) {
            t = t->inner;
            Type *from_type = from->var_type->array.inner;
            while (from_type->comp == STATIC_ARRAY && t->comp == ARRAY) {
                t = t->inner;
                from_type = from_type->array.inner;
            }
            return check_type(t, from_type);
        }
    }
    if (from->type == AST_LITERAL) {
        if (is_numeric(t) && is_numeric(from->var_type)) {
            int loss = 0;
            if (from->lit->lit_type == INTEGER) {
                if (t->data->base == UINT_T) {
                    if (from->lit->int_val < 0) {
                        return 0;
                    }
                    loss = precision_loss_uint(t, from->lit->int_val);
                } else {
                    loss = precision_loss_int(t, from->lit->int_val);
                }
            } else if (from->lit->lit_type == FLOAT) {
                if (t->data->base != FLOAT_T) {
                    return 0;
                }
                loss = precision_loss_float(t, from->lit->float_val);
            }
            if (!loss) {
                return 1;
            }
        } else {
            if (can_cast(from->var_type, t)) {
                return 1;
            }
        }
    }
    return 0;
}
int can_coerce_type(Scope *scope, Type *to, Ast *from) {
    if (is_any(to)) {
        if (!is_any(from->var_type) && !is_lvalue(from)) {
            allocate_ast_temp_var(scope, from);
        }
        return 1;
    }
    
    Type *t = resolve_alias(to);
    if (t->comp == ARRAY) {
        if (from->var_type->comp == ARRAY) {
            return check_type(t->inner, from->var_type->inner);
        } else if (from->var_type->comp == STATIC_ARRAY) {
            t = t->inner;
            Type *from_type = from->var_type->array.inner;
            while (from_type->comp == STATIC_ARRAY && t->comp == ARRAY) {
                t = t->inner;
                from_type = from_type->array.inner;
            }
            return check_type(t, from_type);
        }
    }
    if (from->type == AST_LITERAL) {
        if (is_numeric(t) && is_numeric(from->var_type)) {
            int loss = 0;
            if (from->lit->lit_type == INTEGER) {
                if (t->data->base == UINT_T) {
                    if (from->lit->int_val < 0) {
                        error(from->line, from->file, 
                            "Cannot coerce negative literal value into integer type '%s'.",
                            type_to_string(to));
                    }
                    loss = precision_loss_uint(t, from->lit->int_val);
                } else {
                    loss = precision_loss_int(t, from->lit->int_val);
                }
            } else if (from->lit->lit_type == FLOAT) {
                if (t->data->base != FLOAT_T) {
                    error(from->line, from->file,
                        "Cannot coerce floating point literal into integer type '%s'.",
                        type_to_string(to));
                }
                loss = precision_loss_float(t, from->lit->float_val);
            }
            if (loss) {
                error(from->line, from->file,
                    "Cannot coerce literal value of type '%s' into type '%s' due to precision loss.",
                    type_to_string(from->var_type), type_to_string(to));
            }
            return 1;
        } else {
            if (can_cast(from->var_type, t)) {
                return 1;
            }

            error(from->line, from->file,
                "Cannot coerce literal value of type '%s' into type '%s'.",
                type_to_string(from->var_type), type_to_string(to));
        }
    }
    return 0;
}