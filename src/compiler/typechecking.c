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
        return check_type(a->ref.inner, b->ref.inner);
    case ARRAY:
        return check_type(a->array.inner, b->array.inner);
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
        error(-1, "internal", "typechecking unhandled case");
    }
    return 0;
}

int can_cast(Type *from, Type *to) {
    if (is_any(to)) {
        return 1;
    }

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
            if (to->comp == BASIC) {
                switch (to->data->base) {
                case BASEPTR_T:
                    return 1;
                case UINT_T:
                case INT_T:
                    return to->data->size == 8;
                default:
                    break;
                }
            } else if (to->comp == REF) {
                return 1;
            }
            return 0;
        case UINT_T:
            if (to->comp != BASIC) {
                return 0;
            }
            if (to->data->base == INT_T && to->data->size > from->data->size) {
                return 1;
            } else if (to->data->base == BASEPTR_T) {
                return from->data->size == 8;
            }
            return from->data->base == to->data->base;
        case INT_T:
            if (to->comp == BASIC) {
                if (to->data->base == BASEPTR_T) {
                    return from->data->size == 8;
                } else if (to->data->base == FLOAT_T) {
                    return to->data->size >= from->data->size;
                } else if (to->data->base == INT_T) {
                    return from->data->size <= to->data->size;
                }
            }
        }
        if (to->comp == BASIC) {
            return to->data->base == from->data->base;
        }
        return to->comp == from->comp;
    case STATIC_ARRAY: {
        to = resolve_alias(to);
        if (to->comp == ARRAY) {
            return check_type(from->array.inner, to->array.inner);
        }
        return 0;
    }
    case ARRAY:
        return 0;
    default:
        from = resolve_alias(from);
        to = resolve_alias(to);
        return can_cast(from, to);
    }
    return 0;
}

// if you don't want to define, just pass NULL for scope
int match_polymorph(Scope *scope, Type *expected, Type *got) {
    if (expected->comp == POLYDEF) {
        if (scope != NULL) {
            define_polymorph(scope, expected, got);
        }
        return 1;
    }
    Type *res = resolve_alias(got);
    if (res->comp != expected->comp) {
        if (expected->comp == ARRAY && res->comp == STATIC_ARRAY) {
            return match_polymorph(scope, expected->array.inner, res->array.inner);
        }
        if (expected->comp == PARAMS && res->comp == STRUCT) {
            return match_polymorph(scope, expected, res->st.generic_base);
        }
        return 0;
    }
    switch (expected->comp) {
    case REF:
        return match_polymorph(scope, expected->ref.inner, res->ref.inner);
    case ARRAY:
        return match_polymorph(scope, expected->array.inner, res->array.inner);
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
    case PARAMS: {
        if (!check_type(expected->params.inner, res->params.inner)) {
            return 0;
        }
        TypeList *exp_params = expected->params.args;
        TypeList *got_params = res->params.args;
        for (;;) {
            if (exp_params == NULL && got_params == NULL) {
                break;
            } else if (exp_params == NULL || got_params == NULL) {
                return 0;
            }
            // TODO: not sure this was right
            if (is_polydef(exp_params->item)) {
                if (!match_polymorph(scope, exp_params->item, got_params->item)) {
                    return 0;
                }
            } else if (!check_type(exp_params->item, got_params->item)) {
                return 0;
            }
            exp_params = exp_params->next;
            got_params = got_params->next;
        }
        return 1;
    }
    case STRUCT: // naw dog
    case STATIC_ARRAY: // can't use static array as arg can we?
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

Ast *any_cast(Scope *scope, Ast *a) {
    assert(!is_any(a->var_type));

    if (!is_lvalue(a)) {
        allocate_ast_temp_var(scope, a);
    }
    Ast *c = ast_alloc(AST_CAST);
    c->cast->cast_type = get_any_type();
    c->cast->object = a;
    c->line = a->line;
    c->file = a->file;
    c->var_type = c->cast->cast_type;
    return c;
}

static Ast *number_cast(Scope *scope, Ast *a, Type *num_type) {
    Ast *c = ast_alloc(AST_CAST);
    c->cast->cast_type = num_type;
    c->cast->object = a;
    c->var_type = num_type;
    return c;
}

Ast *coerce_type_no_error(Scope *scope, Type *to, Ast *from) {
    if (is_any(to)) {
        if (is_any(from->var_type)) {
            return from;
        }
        return any_cast(scope, from);
    }
    
    // (TODO: why was this comment here?)
    // resolve polymorphs
    Type *t = resolve_alias(to);
    if (t->comp == ARRAY) {
        if (from->var_type->comp == STATIC_ARRAY) {
            t = t->array.inner;
            Type *from_type = from->var_type->array.inner;
            while (from_type->comp == STATIC_ARRAY && t->comp == ARRAY) {
                t = t->array.inner;
                from_type = from_type->array.inner;
            }
            if (!check_type(t, from_type)) {
                return NULL;
            }
            // TODO: double check this is right
            return from;
        }
    }
    if (from->type == AST_LITERAL) {
        // TODO: string literal of length 1 -> u8
        if (is_numeric(t) && is_numeric(from->var_type)) {
            int loss = 0;
            if (from->lit->lit_type == INTEGER) {
                if (t->data->base == UINT_T) {
                    if (from->lit->int_val < 0) {
                        return NULL;
                    }
                    loss = precision_loss_uint(t, from->lit->int_val);
                } else {
                    loss = precision_loss_int(t, from->lit->int_val);
                }
            } else if (from->lit->lit_type == FLOAT) {
                if (t->data->base != FLOAT_T) {
                    return NULL;
                }
                loss = precision_loss_float(t, from->lit->float_val);
            }
            if (!loss) {
                return number_cast(scope, from, to);
            }
        } else {
            if (is_string(from->var_type) &&
              t->data->base == UINT_T && t->data->size == 1 &&
              strlen(from->lit->string_val) == 1) {
                Ast *c = ast_alloc(AST_LITERAL);
                c->lit->int_val = from->lit->string_val[0];
                c->lit->lit_type = INTEGER;
                c->line = from->line;
                c->file = from->file;
                c->var_type = t;
                return c;
            }

            if (can_cast(from->var_type, t)) {
                if (!is_lvalue(from)) {
                    allocate_ast_temp_var(scope, from);
                }
                Ast *c = ast_alloc(AST_CAST);
                c->cast->cast_type = to;
                c->cast->object = from;
                c->line = from->line;
                c->file = from->file;
                c->var_type = t;
                return c;
            }
        }
    }
    return NULL;
}

Ast *coerce_type(Scope *scope, Type *to, Ast *from) {
    if (is_any(to)) {
        if (is_any(from->var_type)) {
            return from;
        }
        return any_cast(scope, from);
    }
    
    // (TODO: why was this comment here?)
    // resolve polymorphs
    Type *t = resolve_alias(to);
    if (t->comp == ARRAY) {
        if (from->var_type->comp == STATIC_ARRAY) {
            t = t->array.inner;
            Type *from_type = from->var_type->array.inner;
            while (from_type->comp == STATIC_ARRAY && t->comp == ARRAY) {
                t = t->array.inner;
                from_type = from_type->array.inner;
            }
            if (!check_type(t, from_type)) {
                return NULL;
            }
            // TODO: double check this is right
            return from;
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
                    return NULL;
                }
                loss = precision_loss_float(t, from->lit->float_val);
            }
            if (loss) {
                error(from->line, from->file,
                    "Cannot coerce literal value of type '%s' into type '%s' due to precision loss.",
                    type_to_string(from->var_type), type_to_string(to));
            }
            return number_cast(scope, from, to);
        } else {
            if (is_string(from->var_type) &&
              t->data->base == UINT_T && t->data->size == 1 &&
              strlen(from->lit->string_val) == 1) {
                Ast *c = ast_alloc(AST_LITERAL);
                c->lit->int_val = from->lit->string_val[0];
                c->lit->lit_type = INTEGER;
                c->line = from->line;
                c->file = from->file;
                c->var_type = t;
                return c;
            }

            if (can_cast(from->var_type, t)) {
                if (!is_lvalue(from)) {
                    allocate_ast_temp_var(scope, from);
                }
                Ast *c = ast_alloc(AST_CAST);
                c->cast->cast_type = to;
                c->cast->object = from;
                c->line = from->line;
                c->file = from->file;
                c->var_type = t;
                return c;
            }

            error(from->line, from->file,
                "Cannot coerce literal value of type '%s' into type '%s'.",
                type_to_string(from->var_type), type_to_string(to));
        }
    }
    return NULL;
}
