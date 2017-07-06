#include <assert.h>
#include <string.h>

#include "array/array.h"
#include "ast.h"
#include "typechecking.h"
#include "scope.h"

int check_type(Type *a, Type *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }
    if (a->aka != NULL) {
        return check_type(a->aka, b);
    }
    if (b->aka != NULL) {
        return check_type(a, b->aka);
    }
    if (a->name && b->name) {
        if (strcmp(a->name, b->name)) {
            return 0;
        }
    }
    if (a->id == b->id) {
        return 1;
    }
    if (a->name && !a->resolved) {
        TypeDef *adef = find_type_definition(a);
        if (!adef) {
            return 0;
        }
        a->id = adef->type->id;
        a->resolved = adef->type->resolved;
    }
    if (b->name && !b->resolved) {
        TypeDef *bdef = find_type_definition(b);
        if (!bdef) {
            return 0;
        }
        b->id = bdef->type->id;
        b->resolved = bdef->type->resolved;
    }
    ResolvedType *ar = a->resolved;
    ResolvedType *br = b->resolved;
    TypeComp ac = ar->comp;
    TypeComp bc = br->comp;
    if (ac == EXTERNAL) {
        a = find_type_or_polymorph(a);
        ar = a->resolved;
        ac = ar->comp;
    }
    if (bc == EXTERNAL) {
        b = find_type_or_polymorph(b);
        br = b->resolved;
        bc = br->comp;
    }
    if (ac != bc) {
        return 0;
    }
    if (ar == br) {
        if (a->name == NULL && b->name == NULL) {
            return 1;
        } else if (a->name && b->name && !strcmp(a->name, b->name)) {
            return 1;
        }
        return 0;
    } else if (a->name && b->name && !strcmp(a->name, b->name) && a->scope == b->scope) {
        return 1;
    }
    if ((a->name == NULL && b->name != NULL) || 
        (a->name != NULL && b->name == NULL)) {
        return 0;
    }
    switch (ac) {
    case BASIC:
    case ENUM:
        return a->id == b->id;
    case POLYDEF:
        return find_type_definition(a) == find_type_definition(b);
    case REF:
        return check_type(ar->ref.inner, br->ref.inner);
    case ARRAY:
        return check_type(ar->array.inner, br->array.inner);
    case STATIC_ARRAY:
        return ar->array.length == br->array.length &&
            check_type(ar->array.inner, br->array.inner);
    case STRUCT:
        if (array_len(ar->st.member_names) != array_len(br->st.member_names)) {
            return 0;
        }
        for (int i = 0; i < array_len(ar->st.member_names); i++) {
            if (strcmp(ar->st.member_names[i], br->st.member_names[i])) {
                return 0;
            }
            if (!check_type(ar->st.member_types[i], br->st.member_types[i])) {
                return 0;
            }
        }
        return 1;
    case FUNC:
        if (ar->fn.variadic != br->fn.variadic) {
            return 0;
        }
        if (array_len(ar->fn.args) != array_len(br->fn.args)) {
            return 0;
        }
        for (int i = 0; i < array_len(ar->fn.args); i++) {
            if (!check_type(ar->fn.args[i], br->fn.args[i])) {
                return 0;
            }
        }
        return check_type(ar->fn.ret, br->fn.ret);
    default:
        error(-1, "internal", "typechecking unhandled case");
    }
    return 0;
}

int can_cast(Type *from, Type *to) {
    if (is_any(to)) {
        return 1;
    }

    ResolvedType *fr = from->resolved;
    ResolvedType *tr = to->resolved;

    if (fr->comp == ENUM) {
        return can_cast(fr->en.inner, to);
    } else if (tr->comp == ENUM) {
        return can_cast(from, tr->en.inner);
    }

    switch (fr->comp) {
    case REF:
        return tr->comp == REF ||
            (tr->comp == BASIC &&
                 (tr->data->base == BASEPTR_T ||
                 (tr->data->base == INT_T && tr->data->size == 8)));
    case FUNC:
        if (array_len(fr->fn.args) != array_len(tr->fn.args)) {
            return 0;
        }
        for (int i = 0; i < array_len(fr->fn.args); i++) {
            if (!check_type(fr->fn.args[i], tr->fn.args[i])) {
                return 0;
            }
        }
        if (fr->fn.ret == tr->fn.ret) {
            return 1;
        }
        return check_type(fr->fn.ret, tr->fn.ret);
    case STRUCT:
        if (array_len(fr->st.member_types) != array_len(tr->st.member_types)) {
            return 0;
        }
        for (int i = 0; i < array_len(fr->st.member_types); i++) {
            if (!check_type(fr->st.member_types[i], tr->st.member_types[i])) {
                return 0;
            }
            if (strcmp(fr->st.member_names[i], tr->st.member_names[i])) {
                return 0;
            }
        }
        return 1;
    case BASIC:
        switch (fr->data->base) {
        case BASEPTR_T:
            if (tr->comp == BASIC) {
                switch (tr->data->base) {
                case BASEPTR_T:
                    return 1;
                case UINT_T:
                case INT_T:
                    return tr->data->size == 8;
                default:
                    break;
                }
            } else if (tr->comp == REF) {
                return 1;
            }
            return 0;
        case UINT_T:
            if (tr->comp != BASIC) {
                return 0;
            }
            if (tr->data->base == INT_T && tr->data->size > fr->data->size) {
                return 1;
            } else if (tr->data->base == BASEPTR_T) {
                return fr->data->size == 8;
            }
            return fr->data->base == tr->data->base;
        case INT_T:
            if (tr->comp == BASIC) {
                if (tr->data->base == BASEPTR_T) {
                    return fr->data->size == 8;
                } else if (tr->data->base == FLOAT_T) {
                    return tr->data->size >= fr->data->size;
                } else if (tr->data->base == INT_T) {
                    return fr->data->size <= tr->data->size;
                } else if (tr->data->base == UINT_T) {
                    return fr->data->size < tr->data->size;
                }
            }
        }
        if (tr->comp == BASIC) {
            return tr->data->base == fr->data->base;
        }
        return tr->comp == fr->comp;
    case STATIC_ARRAY: {
        if (tr->comp == ARRAY) {
            return check_type(fr->array.inner, tr->array.inner);
        }
        return 0;
    }
    case ARRAY:
        return 0;
    default:
        break;
    }
    return 0;
}

// if you don't want to define, just pass NULL for scope
int match_polymorph(Scope *scope, Type *expected, Type *got) {
    ResolvedType *er = expected->resolved;
    ResolvedType *gr = got->resolved;
    if (er->comp == POLYDEF) {
        if (scope != NULL) {
            define_polymorph(scope, expected, got, NULL); // TODO: probably need to pass a real Ast* here!
        }
        return 1;
    }
    if (gr->comp != er->comp) {
        if (er->comp == ARRAY && gr->comp == STATIC_ARRAY) {
            return match_polymorph(scope, er->array.inner, gr->array.inner);
        }
        if (er->comp == PARAMS && gr->comp == STRUCT) {
            return match_polymorph(scope, expected, gr->st.generic_base);
        }
        return 0;
    }
    switch (er->comp) {
    case REF:
        return match_polymorph(scope, er->ref.inner, gr->ref.inner);
    case ARRAY:
        return match_polymorph(scope, er->array.inner, gr->array.inner);
    case FUNC:
        if (er->fn.variadic != gr->fn.variadic) {
            return 0;
        }
        if (array_len(er->fn.args) != array_len(gr->fn.args)) {
            return 0;
        }
        for (int i = 0; i < array_len(er->fn.args); i++) {
            // TODO: not sure this was right
            if (is_polydef(er->fn.args[i])) {
                if (!match_polymorph(scope, er->fn.args[i], gr->fn.args[i])) {
                    return 0;
                }
            } else if (!check_type(er->fn.args[i], gr->fn.args[i])) {
                return 0;
            }
        }
        return check_type(er->fn.ret, gr->fn.ret);
    case PARAMS: {
        if (!check_type(er->params.inner, gr->params.inner)) {
            return 0;
        }
        if (array_len(er->params.args) != array_len(gr->params.args)) {
            return 0;
        }
        for (int i = 0; i < array_len(er->params.args); i++) {
            // TODO: not sure this was right
            if (is_polydef(er->params.args[i])) {
                if (!match_polymorph(scope, er->params.args[i], gr->params.args[i])) {
                    return 0;
                }
            } else if (!check_type(er->params.args[i], gr->params.args[i])) {
                return 0;
            }
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
        error(-1, "internal", "Cmon mang");
        return 0;
    }
    return 1;
}

Type *promote_number_type(Type *a, int left_lit, Type *b, int right_lit) {
    ResolvedType *aa = a->resolved;
    assert(aa->comp == BASIC);
    int abase = aa->data->base;
    int asize = aa->data->size;

    ResolvedType *bb = b->resolved;
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
    if (is_numeric(a)) {
        return is_numeric(b);
    }
    if (a->resolved->comp == REF || (a->resolved->comp == BASIC && a->resolved->data->base == BASEPTR_T)) {
        return b->resolved->comp == REF || (b->resolved->comp == BASIC && b->resolved->data->base == BASEPTR_T);
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

Ast *coerce_type(Scope *scope, Type *to, Ast *from, int raise_error) {
    if (is_any(to)) {
        if (is_any(from->var_type)) {
            return from;
        }
        return any_cast(scope, from);
    }
    
    // (TODO: why was this comment here?)
    // resolve polymorphs
    ResolvedType *tr = to->resolved;
    if (tr->comp == ARRAY) {
        if (from->var_type->resolved->comp == STATIC_ARRAY) {
            Type *inner = tr->array.inner;
            Type *from_type = from->var_type->resolved->array.inner;
            while (from_type->resolved->comp == STATIC_ARRAY && inner->resolved->comp == ARRAY) {
                inner = inner->resolved->array.inner;
                from_type = from_type->resolved->array.inner;
            }
            if (!check_type(inner, from_type)) {
                return NULL;
            }
            // TODO: double check this is right
            return from;
        }
    }
    if (from->type == AST_LITERAL) {
        if (is_numeric(to) && is_numeric(from->var_type)) {
            int loss = 0;
            if (from->lit->lit_type == INTEGER) {
                if (tr->data->base == UINT_T) {
                    if (from->lit->int_val < 0) {
                        if (raise_error) {
                            error(from->line, from->file, 
                                "Cannot coerce negative literal value into integer type '%s'.",
                                type_to_string(to));
                        } else {
                            return NULL;
                        }
                    }
                    loss = precision_loss_uint(to, from->lit->int_val);
                } else {
                    loss = precision_loss_int(to, from->lit->int_val);
                }
            } else if (from->lit->lit_type == FLOAT) {
                if (tr->data->base != FLOAT_T) {
                    return NULL;
                }
                loss = precision_loss_float(to, from->lit->float_val);
            }
            if (loss) {
                if (raise_error) {
                    error(from->line, from->file,
                        "Cannot coerce literal value of type '%s' into type '%s' due to precision loss.",
                        type_to_string(from->var_type), type_to_string(to));
                } else {
                    return NULL;
                }
            }
            return number_cast(scope, from, to);
        } else {
            if (is_string(from->var_type) &&
              tr->comp == BASIC && tr->data->base == UINT_T && tr->data->size == 1 &&
              strlen(from->lit->string_val) == 1) {
                Ast *c = ast_alloc(AST_LITERAL);
                c->lit->int_val = from->lit->string_val[0];
                c->lit->lit_type = INTEGER;
                c->line = from->line;
                c->file = from->file;
                c->var_type = to;
                return c;
            }

            if (can_cast(from->var_type, to)) {
                if (!is_lvalue(from)) {
                    allocate_ast_temp_var(scope, from);
                }
                Ast *c = ast_alloc(AST_CAST);
                c->cast->cast_type = to;
                c->cast->object = from;
                c->line = from->line;
                c->file = from->file;
                c->var_type = to;
                return c;
            }

            if (raise_error) {
                error(from->line, from->file,
                    "Cannot coerce literal value of type '%s' into type '%s'.",
                    type_to_string(from->var_type), type_to_string(to));
            }
        }
    }
    return NULL;
}
