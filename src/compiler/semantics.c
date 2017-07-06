#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "array/array.h"
#include "semantics.h"
#include "eval.h"
#include "parse.h"
#include "package.h"
#include "polymorph.h"
#include "types.h"
#include "typechecking.h"

// TODO: do this better
static int in_impl = 0;
static Ast **global_fn_decls = NULL;

Ast **get_global_funcs() {
    return global_fn_decls;
}

void check_for_undefined_with_scope(Ast *ast, Type *t, Scope *scope) {
    if (t->resolved && t->resolved->comp == POLYDEF) {
        return;
    }
    Scope *tmp = t->scope;
    t->scope = scope;
    if (t->name) {
        if (!find_type_or_polymorph(t)) {
            error(ast->line, ast->file, "Unknown type '%s'.", t->name);
        }
        t->scope = tmp;
        return;
    }
    switch (t->resolved->comp) {
    case FUNC:
        for (int i = 0; i < array_len(t->resolved->fn.args); i++) {
            check_for_undefined_with_scope(ast, t->resolved->fn.args[i], scope);
        }
        if (t->resolved->fn.ret) {
            check_for_undefined_with_scope(ast, t->resolved->fn.ret, scope);
        }
        break;
    case STRUCT: {
        // line numbers can be weird on this...
        for (int i = 0; i < array_len(t->resolved->st.member_types); i++) {
            check_for_undefined_with_scope(ast, t->resolved->st.member_types[i], scope);
        }
        break;
    }
    case REF:
        check_for_undefined_with_scope(ast, t->resolved->ref.inner, scope);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        check_for_undefined_with_scope(ast, t->resolved->array.inner, scope);
        break;
    case BASIC:
    case POLYDEF:
    case PARAMS:
    case EXTERNAL:
    case ENUM:
        break;
    }
    t->scope = tmp;
}


void _check_for_undefined_with_ignore(Ast *ast, Type *t, char **ignore) {
    if (t->name) {
        for (int i = 0; i < array_len(ignore); i++) {
            if (!strcmp(t->name, ignore[i])) {
                return;
            }
        }
        if (!t->resolved) {
            if (!find_type_or_polymorph(t)) {
                error(ast->line, ast->file, "Unknown type '%s'.", t->name);
            }
            return;
        }
    }
    switch (t->resolved->comp) {
    case FUNC:
        for (int i = 0; i < array_len(t->resolved->fn.args); i++) {
            _check_for_undefined_with_ignore(ast, t->resolved->fn.args[i], ignore);
        }
        if (t->resolved->fn.ret) {
            _check_for_undefined_with_ignore(ast, t->resolved->fn.ret, ignore);
        }
        break;
    case STRUCT: {
        if (t->resolved->st.generic) {
            char **tmp = NULL;
            for (int i = 0; i < array_len(t->resolved->st.arg_params); i++) {
                array_push(tmp, t->resolved->st.arg_params[i]->name);
            }
            for (int i = 0; i < array_len(t->resolved->st.member_types); i++) {
                _check_for_undefined_with_ignore(ast, t->resolved->st.member_types[i], tmp);
            }
            array_free(tmp);
            break;
        } else {
            // line numbers can be weird on this...
            for (int i = 0; i < array_len(t->resolved->st.member_types); i++) {
                _check_for_undefined_with_ignore(ast, t->resolved->st.member_types[i], ignore);
            }
        }
        break;
    }
    case REF:
        _check_for_undefined_with_ignore(ast, t->resolved->ref.inner, ignore);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        _check_for_undefined_with_ignore(ast, t->resolved->array.inner, ignore);
        break;
    case BASIC:
    case POLYDEF:
    case PARAMS:
    case EXTERNAL:
    case ENUM:
        break;
    }
}

void check_for_undefined(Ast *ast, Type *t) {
    _check_for_undefined_with_ignore(ast, t, NULL);
}

// TODO: make reify_struct deduplicate reifications?
Type *reify_struct(Scope *scope, Ast *ast, Type *t) {
    t = copy_type(t->scope, t);

    ResolvedType *r = t->resolved;
    switch (r->comp) {
    case FUNC:
        for (int i = 0; i < array_len(r->fn.args); i++) {
            if (contains_generic_struct(r->fn.args[i])) {
                r->fn.args[i] = reify_struct(scope, ast, r->fn.args[i]);
            }
        }
        if (r->fn.ret != NULL && contains_generic_struct(r->fn.ret)) {
            r->fn.ret = reify_struct(scope, ast, r->fn.ret);
        }
        return t;
    case STRUCT: {
        for (int i = 0; i < array_len(r->st.member_types); i++) {
            if (contains_generic_struct(r->st.member_types[i])) {
                r->st.member_types[i] = reify_struct(scope, ast, r->st.member_types[i]);
            }
        }
        return t;
    }
    case REF:
        if (contains_generic_struct(r->ref.inner)) {
            r->ref.inner = reify_struct(scope, ast, r->ref.inner);
        }
        return t;
    case ARRAY:
    case STATIC_ARRAY:
        if (contains_generic_struct(r->array.inner)) {
            r->array.inner = reify_struct(scope, ast, r->array.inner);
        }
        return t;
    case BASIC:
    case POLYDEF:
    case EXTERNAL:
    case ENUM:
        error(-1, "<internal>", "what are you even");
        break;
    case PARAMS:
        for (int i = 0; i < array_len(r->params.args); i++) {
            if (contains_generic_struct(r->params.args[i])) {
                r->params.args[i] = reify_struct(scope, ast, r->params.args[i]);
            }
        }
        break;
    }

    assert(r->comp == PARAMS);
    assert(r->params.args != NULL);

    Type *_inner = r->params.inner;
    if (!_inner->resolved) {
        _inner = find_type_or_polymorph(_inner);
    }
    ResolvedType *inner = _inner->resolved;
    if (inner->comp != STRUCT) {
        error(ast->line, ast->file, "Invalid parameterization, type '%s' is not a generic struct.", type_to_string(r->params.inner));
    }

    Type **given = r->params.args;
    Type **expected = inner->st.arg_params;
    
    for (int i = 0; i < array_len(given); i++) {
        Type *tmp = resolve_type(given[i]);
        check_for_undefined(ast, given[i]);
        given[i] = tmp;
    }

    if (array_len(given) != array_len(expected)) {
        error(ast->line, ast->file, "Invalid parameterization, type '%s' expects %d parameters but received %d.",
              type_to_string(r->params.inner), array_len(expected), array_len(given));
    }

    Type **member_types = array_copy(inner->st.member_types);
    for (int i = 0; i < array_len(given); i++) {
        for (int j = 0; j < array_len(inner->st.member_types); j++) {
            assert(expected[i]->name != NULL);
            member_types[j] = replace_type_by_name(copy_type(member_types[j]->scope, member_types[j]), expected[i]->name, given[i]);
        }
    }

    Type *out = make_struct_type(inner->st.member_names, member_types);
    out->scope = t->scope;
    out->resolved->st.generic_base = t;
    resolve_type(out);
    register_type(out);

    return out;
}

static Ast *check_uop_semantics(Scope *scope, Ast *ast) {
    ast->unary->object = check_semantics(scope, ast->unary->object);
    Ast *o = ast->unary->object;
    switch (ast->unary->op) {
    case OP_NOT:
        // TODO: make recognition of these types better
        if (!is_bool(o->var_type)) {
            error(ast->line, ast->file, "Cannot perform logical negation on type '%s'.", type_to_string(o->var_type));
        }
        ast->var_type = base_type(BOOL_T);
        break;
    case OP_DEREF: // TODO precedence is wrong, see @x.data
        if (o->var_type->resolved->comp != REF) {
            error(ast->line, ast->file, "Cannot dereference a non-reference type (must cast baseptr).");
        }
        ast->var_type = o->var_type->resolved->ref.inner;
        register_type(ast->var_type);
        break;
    case OP_REF:
        if (o->type != AST_IDENTIFIER && o->type != AST_DOT) {
            error(ast->line, ast->file, "Cannot take a reference to a non-variable.");
        }
        ast->var_type = make_ref_type(o->var_type);
        register_type(ast->var_type);
        break;
    case OP_MINUS:
    case OP_PLUS: {
        Type *t = o->var_type;
        if (!is_numeric(t)) { // TODO: try implicit cast to base type
            error(ast->line, ast->file, "Cannot perform '%s' operation on non-numeric type '%s'.", op_to_str(ast->unary->op), type_to_string(t));
        }
        ast->var_type = t;
        break;
    }
    default:
        error(ast->line, ast->file, "Unknown unary operator '%s' (%d).", op_to_str(ast->unary->op), ast->unary->op);
    }
    if (o->type == AST_LITERAL) {
        return eval_const_uop(ast);
    }
    return ast;
}

static Ast *check_ident_semantics(Scope *scope, Ast *ast) {
    Var *v = lookup_var(scope, ast->ident->varname);
    if (v == NULL) {
        // couldn't find var, try enum
        Type *t = make_type(scope, ast->ident->varname);
        Type *found = resolve_type(t);
        if (found) {
            Ast *a = ast_alloc(AST_TYPE_IDENT);
            a->line = ast->line;
            a->file = ast->file;
            a->type_ident->type = found;
            a->var_type = typeinfo_ref();
            return a;
        }

        // couldn't find var or enum, try pacakge
        Package *p = lookup_imported_package(scope, ast->ident->varname);
        if (p != NULL) {
            Ast *a = ast_alloc(AST_PACKAGE);
            a->line = ast->line;
            a->file = ast->file;
            a->pkg->pkg_name = p->name;
            a->pkg->package = p;
            a->var_type = base_type(VOID_T); // TODO: maybe make a builtin Package type
            return a;
        } 
        error(ast->line, ast->file, "Undefined identifier '%s' encountered.", ast->ident->varname);
    }

    if (v->proxy) { // USE-proxy
        Type *t = resolve_type(v->type);
        if (t->resolved->comp == ENUM) { // proxy for enum
            for (int i = 0; i < array_len(t->resolved->en.member_names); i++) {
                if (!strcmp(t->resolved->en.member_names[i], v->name)) {
                    Ast *a = ast_alloc(AST_LITERAL);
                    a->line = ast->line;
                    a->file = ast->file;
                    a->lit->lit_type = ENUM_LIT;
                    a->lit->enum_val.enum_index = i;
                    a->lit->enum_val.enum_type = t;
                    a->var_type = v->type;
                    return a;
                }
            }
            error(-1, "<internal>", "How'd this happen");
        } else { // proxy for normal var
            // TODO: this may not be needed again, semantics already
            // checked?
            /*return v->proxy;*/
            return check_semantics(scope, v->proxy);
        }
    }
    ast->ident->var = v;
    ast->var_type = resolve_type(ast->ident->var->type);
    return ast;
}

static Ast *check_dot_op_semantics(Scope *scope, Ast *ast) {
    if (ast->dot->object->type == AST_IDENTIFIER) {
        ast->dot->object = check_ident_semantics(scope, ast->dot->object);
        // handle the below types specifically, without running check_semantics
        // again (TODO: find a way to make this easier/smoother)
    } else if (ast->dot->object->type != AST_TYPE_IDENT && ast->dot->object->type != AST_PACKAGE) {
        ast->dot->object = check_semantics(scope, ast->dot->object);
    }

    if (ast->dot->object->type == AST_TYPE_IDENT) {
        // TODO: need to make sure that an enum or package in outer scope cannot
        // shadow a locally (or closer to locally) declared variable just
        // because this is happening before the other resolution
        AstTypeIdent *tp = ast->dot->object->type_ident;
        if (tp->type->resolved->comp == ENUM) {
            for (int i = 0; i < array_len(tp->type->resolved->en.member_names); i++) {
                if (!strcmp(tp->type->resolved->en.member_names[i], ast->dot->member_name)) {
                    Ast *a = ast_alloc(AST_LITERAL);
                    a->lit->lit_type = ENUM_LIT;
                    a->lit->enum_val.enum_index = i;
                    a->lit->enum_val.enum_type = tp->type;
                    a->var_type = tp->type;
                    return a;
                }
            }
            // TODO allow BaseType.members
            /*if (!strcmp("members", ast->dot->member_name)) {*/
                
            /*}*/
            error(ast->line, ast->file, "No value '%s' in enum type '%s'.",
                    ast->dot->member_name, type_to_string(tp->type));
        } else {
            error(ast->line, ast->file, "Can't get member '%s' from non-enum type '%s'.",
                    ast->dot->member_name, type_to_string(tp->type));
        }
    } else if (ast->dot->object->type == AST_PACKAGE) {
        Package *p = ast->dot->object->pkg->package;
        Var *v = lookup_var(p->scope, ast->dot->member_name);
        if (!v) {
            // try finding a type
            Type *t = make_type(p->scope, ast->dot->member_name);
            Type *found = resolve_type(t);
            if (found) {
                Ast *a = ast_alloc(AST_TYPE_IDENT);
                a->line = ast->line;
                a->file = ast->file;
                a->type_ident->type = found;
                a->var_type = typeinfo_ref();
                return a;
            }
            
            error(ast->line, ast->file, "No declared identifier '%s' in package '%s'.", ast->dot->member_name, p->name);
            // TODO better error for an enum here
        }
        if (v->proxy) { // USE-proxy
            error(ast->line, ast->file, "No declared identifier '%s' in package '%s' (*use* doesn't count).", ast->dot->member_name, p->name);
        }
        ast = ast_alloc(AST_IDENTIFIER);
        ast->ident->var = v;
        ast->var_type = v->type;
        return ast;
    }

    Type *orig = ast->dot->object->var_type;
    Type *t = resolve_type(orig);

    if (t->resolved->comp == REF) {
        t = t->resolved->ref.inner;
    }

    Ast *decl = find_method(t, ast->dot->member_name);
    // TODO: this could be nicer
    if (!decl && orig->resolved->comp == STRUCT && orig->resolved->st.generic_base != NULL) {
        decl = find_method(orig->resolved->st.generic_base->resolved->params.inner, ast->dot->member_name);
    }
    if (decl) {
        // just make "method" a field in AstCall?
        Ast *m = ast_alloc(AST_METHOD);
        m->line = ast->dot->object->line;
        m->file = ast->dot->object->file;
        m->method->recv = ast->dot->object;
        m->method->name = ast->dot->member_name;
        m->method->decl = decl->fn_decl;
        m->var_type = m->method->decl->var->type;

        if (!is_lvalue(ast->dot->object)) {
            allocate_ast_temp_var(scope, ast->dot->object);
        }
        return m;
    }

    if (is_array(t)) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            ast->var_type = make_ref_type(t->resolved->array.inner);
        } else {
            error(ast->line, ast->file,
                    "Cannot dot access member '%s' on array (only length or data).", ast->dot->member_name);
        }
    } else if (is_string(t)) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "bytes")) {
            ast->var_type = make_ref_type(base_numeric_type(UINT_T, 8));
        } else {
            error(ast->line, ast->file,
                    "Cannot dot access member '%s' on string (only length or bytes).", ast->dot->member_name);
        }
    } else if (t->resolved->comp == STRUCT) {
        for (int i = 0; i < array_len(t->resolved->st.member_names); i++) {
            if (!strcmp(ast->dot->member_name, t->resolved->st.member_names[i])) {
                ast->var_type = t->resolved->st.member_types[i];
                return ast;
            }
        }
        error(ast->line, ast->file, "No member named '%s' in struct '%s'.",
                ast->dot->member_name, type_to_string(orig));
    } else {
        error(ast->line, ast->file,
                "Cannot use dot operator on non-struct type '%s'.", type_to_string(orig));
    }
    return ast;
}

static Ast *check_assignment_semantics(Scope *scope, Ast *ast) {
    ast->binary->left = check_semantics(scope, ast->binary->left);
    ast->binary->right = check_semantics(scope, ast->binary->right);

    if (!is_lvalue(ast->binary->left)) {
        error(ast->line, ast->file, "LHS of assignment is not an lvalue.");
    } else if (ast->binary->left->type == AST_INDEX) {
        Type *lt = ast->binary->left->index->object->var_type;
        if (is_string(lt)) {
            error(ast->line, ast->file, "Strings are immutable and cannot be subscripted for assignment.");
        }
    }

    // TODO refactor to "is_constant"
    if (ast->binary->left->type == AST_IDENTIFIER && ast->binary->left->ident->var->constant) {
        error(ast->line, ast->file, "Cannot reassign constant '%s'.", ast->binary->left->ident->var->name);
    }

    Type *lt = ast->binary->left->var_type;
    Type *rt = ast->binary->right->var_type;

    if (needs_temp_var(ast->binary->right) || is_dynamic(lt)) {
    /*if(needs_temp_var(ast->binary->right)) {*/
        allocate_ast_temp_var(scope, ast->binary->right);
    }

    // owned references
    if (lt->resolved->comp == REF && lt->resolved->ref.owned) {
        if (!(ast->binary->right->type == AST_NEW/* || ast->binary->right == AST_MOVE*/)) {
            error(ast->line, ast->file, "Owned reference can only be assigned from new or move expression.");
        }
    } else if (rt->resolved->comp == REF && rt->resolved->ref.owned) {
        allocate_ast_temp_var(scope, ast->binary->right);
    }

    if (lt->resolved->comp == ARRAY && lt->resolved->array.owned) {
        if (!(ast->binary->right->type == AST_NEW/* || ast->binary->right == AST_MOVE*/)) {
            error(ast->line, ast->file, "Owned array slice can only be assigned from new or move expression.");
        }
    } else if (rt->resolved->comp == ARRAY && rt->resolved->array.owned) {
        allocate_ast_temp_var(scope, ast->binary->right);
    }

    if (!check_type(lt, rt)) {
        ast->binary->right = coerce_type(scope, lt, ast->binary->right, 1);
        if (!ast->binary->right) {
            error(ast->binary->left->line, ast->binary->left->file,
                "LHS of assignment has type '%s', while RHS has type '%s'.",
                type_to_string(lt), type_to_string(rt));
        }
    }

    ast->var_type = base_type(VOID_T);
    return ast;
}

static Ast *check_binop_semantics(Scope *scope, Ast *ast) {
    ast->binary->left = check_semantics(scope, ast->binary->left);
    ast->binary->right = check_semantics(scope, ast->binary->right);

    Ast *l = ast->binary->left;
    Ast *r = ast->binary->right;

    Type *lt = l->var_type;
    Type *rt = r->var_type;

    switch (ast->binary->op) {
    case OP_PLUS: {
        int numeric = is_numeric(lt) && is_numeric(rt);
        int strings = is_string(lt) && is_string(rt);
        if (!numeric && !strings) {
            error(ast->line, ast->file, "Operator '%s' is not valid between types '%s' and '%s'.",
                    op_to_str(ast->binary->op), type_to_string(lt), type_to_string(rt));
        }
        break;
    }
    case OP_MINUS:
    case OP_MUL:
    case OP_DIV:
    case OP_XOR:
    case OP_BINAND:
    case OP_BINOR:
    case OP_GT:
    case OP_GTE:
    case OP_LT:
    case OP_LTE:
        if (!is_numeric(lt)) {
            error(ast->line, ast->file, "LHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->binary->op), type_to_string(lt));
        } else if (!is_numeric(rt)) {
            error(ast->line, ast->file, "RHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->binary->op), type_to_string(rt));
        }
        break;
    case OP_AND:
    case OP_OR:
        if (lt->id != base_type(BOOL_T)->id) {
            error(ast->line, ast->file, "Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(ast->binary->op), type_to_string(lt));
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        if (!check_type(lt, rt)) {
            ast->binary->right = coerce_type(scope, lt, ast->binary->right, 1);
            if (!ast->binary->right) {
                error(ast->line, ast->file, "Cannot compare equality of non-comparable types '%s' and '%s'.",
                        type_to_string(lt), type_to_string(rt));
            }
        }
        break;
    }

    if (l->type == AST_LITERAL && r->type == AST_LITERAL) {
        // TODO: this doesn't work for string slices
        return eval_const_binop(ast);
    }

    if (is_comparison(ast->binary->op)) {
        ast->var_type = base_type(BOOL_T);
    } else if (ast->binary->op != OP_ASSIGN && is_numeric(lt)) {
        ast->var_type = promote_number_type(lt, l->type == AST_LITERAL, rt, r->type == AST_LITERAL);
    } else {
        ast->var_type = lt;
    }
    return ast;
}

static Ast *check_enum_decl_semantics(Scope *scope, Ast *ast) {
    Type *et = ast->enum_decl->enum_type; 
    ResolvedType *r = et->resolved;
    assert(r->comp == ENUM);
    Ast **exprs = ast->enum_decl->exprs;

    long val = 0;
    for (int i = 0; i < array_len(r->en.member_names); i++) {
        if (exprs[i] != NULL) {
            exprs[i] = check_semantics(scope, exprs[i]);
            Type *t = exprs[i]->var_type;
            // TODO allow const other stuff in here
            if (exprs[i]->type != AST_LITERAL) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-constant expression.", et->name, r->en.member_names[i]);
            } else if (t->resolved->data->base != INT_T) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-integer expression.", et->name, r->en.member_names[i]);
            }
            Type *e = exprs[i]->var_type;

            if (!check_type(r->en.inner, e)) {
                if (!coerce_type(scope, r->en.inner, exprs[i], 0)) {
                    error(exprs[i]->line, exprs[i]->file,
                        "Cannot initialize enum '%s' member '%s' with expression of type '%s' (base is '%s').",
                        et->name, r->en.member_names[i], type_to_string(e), type_to_string(r->en.inner));
                }
            }
            val = exprs[i]->lit->int_val;
        }
        array_push(r->en.member_values, val);
        val += 1;
        for (int j = 0; j < i; j++) {
            if (!strcmp(r->en.member_names[i], r->en.member_names[j])) {
                error(ast->line, ast->file, "Enum '%s' member name '%s' defined twice.",
                        ast->enum_decl->enum_name, r->en.member_names[i]);
            }
            /*if (et->en.member_values[i] == et->en.member_values[j]) {*/
                /*error(ast->line, ast->file, "Enum '%s' contains duplicate values for members '%s' and '%s'.",*/
                    /*ast->enum_decl->enum_name, et->en.member_names[j],*/
                    /*et->en.member_names[i]);*/
            /*}*/
        }
    }
    ast->var_type = base_type(VOID_T);
    return ast;
}

void first_pass_type(Scope *scope, Type *t) {
    t->scope = scope;
    ResolvedType *r = t->resolved;
    if (!r) {
        return;
    }
    switch (r->comp) {
    case POLYDEF:
    case EXTERNAL:
        break;
    case PARAMS:
        for (int i = 0; i < array_len(r->params.args); i++) {
            first_pass_type(scope, r->params.args[i]);
        }
        first_pass_type(scope, r->params.inner);
        break;
    case FUNC:
        for (int i = 0; i < array_len(r->fn.args); i++) {
            first_pass_type(scope, r->fn.args[i]);
        }
        first_pass_type(scope, r->fn.ret);
        break;
    case STRUCT:
        // TODO: owned references need to check for circular type declarations
        for (int i = 0; i < array_len(r->st.member_types); i++) {
            first_pass_type(scope, r->st.member_types[i]);
        }
        for (int i = 0; i < array_len(r->st.arg_params); i++) {
            first_pass_type(scope, r->st.arg_params[i]);
        }
        break;
    case ENUM:
        first_pass_type(scope, r->en.inner);
        break;
    case REF:
        first_pass_type(scope, r->ref.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        first_pass_type(scope, r->array.inner);
        break;
    default:
        break;
    }
}

Ast *first_pass(Scope *scope, Ast *ast) {
    switch (ast->type) {
    case AST_IMPORT:
        // error here or elsewhere?
        if (scope->parent != NULL) {
            error(ast->line, ast->file, "All imports must be done at root scope.");
        }
        ast->import->package = load_package(ast->file, scope, ast->import->path);
        break;
    case AST_LITERAL:
        if (ast->lit->lit_type == STRUCT_LIT || ast->lit->lit_type == COMPOUND_LIT) {
            first_pass_type(scope, ast->lit->compound_val.type);
            for (int i = 0; i < array_len(ast->lit->compound_val.member_exprs); i++) {
                ast->lit->compound_val.member_exprs[i] = first_pass(scope, ast->lit->compound_val.member_exprs[i]);
            }

        } else if (ast->lit->lit_type == ENUM_LIT) {
            first_pass_type(scope, ast->lit->enum_val.enum_type);
        }
        break;
    case AST_DECL:
        if (ast->decl->var->type != NULL) {
            first_pass_type(scope, ast->decl->var->type);
        }
        if (ast->decl->init != NULL) {
            ast->decl->init = first_pass(scope, ast->decl->init);
        }
        break;
    case AST_EXTERN_FUNC_DECL:
        if (lookup_local_var(scope, ast->fn_decl->var->name) != NULL) {
            error(ast->line, ast->file, "Declared extern function name '%s' already exists in this scope.", ast->fn_decl->var->name);
        }

        first_pass_type(scope, ast->fn_decl->var->type);
        break;
    case AST_FUNC_DECL:
    case AST_ANON_FUNC_DECL:
        // TODO: split off if polydef here
        ast->fn_decl->scope = new_fn_scope(scope);
        ast->fn_decl->scope->fn_var = ast->fn_decl->var;

        if (!ast->fn_decl->anon && !in_impl) {
            if (lookup_local_var(scope, ast->fn_decl->var->name) != NULL) {
                error(ast->line, ast->file, "Declared function name '%s' already exists in this scope.", ast->fn_decl->var->name);
            }

            array_push(scope->vars, ast->fn_decl->var);
            array_push(ast->fn_decl->scope->vars, ast->fn_decl->var);
        }

        first_pass_type(ast->fn_decl->scope, ast->fn_decl->var->type);

        // TODO handle case where arg has same name as func
        if (!is_polydef(ast->fn_decl->var->type)) {
            array_push(global_fn_decls, ast);
        }
        break;
    case AST_ENUM_DECL:
        if (local_type_name_conflict(scope, ast->enum_decl->enum_name)) {
            error(ast->line, ast->file, "Type named '%s' already exists in local scope.", ast->enum_decl->enum_name);
        }
        // TODO: need to do the exprs here or not?
        first_pass_type(scope, ast->enum_decl->enum_type);
        ast->enum_decl->enum_type = define_type(scope, ast->enum_decl->enum_name, ast->enum_decl->enum_type, ast);
        break;
    case AST_TYPE_DECL:
        ast->type_decl->target_type->scope = scope;
        first_pass_type(scope, ast->type_decl->target_type);
        ast->type_decl->target_type = define_type(scope, ast->type_decl->type_name, ast->type_decl->target_type, ast);
        break;
    case AST_DOT:
        ast->dot->object = first_pass(scope, ast->dot->object);
        // TODO: what about when LHS is a package and RHS is a type?
        break;
    case AST_ASSIGN:
    case AST_BINOP:
        ast->binary->left = first_pass(scope, ast->binary->left);
        ast->binary->right = first_pass(scope, ast->binary->right);
        break;
    case AST_UOP:
        ast->unary->object = first_pass(scope, ast->unary->object);
        break;
    case AST_COPY:
        ast->copy->expr = first_pass(scope, ast->copy->expr);
        break;
    case AST_CALL:
        ast->call->fn = first_pass(scope, ast->call->fn);
        for (int i = 0; i < array_len(ast->call->args); i++) {
            ast->call->args[i] = first_pass(scope, ast->call->args[i]);
        }
        if (ast->call->variadic_tempvar) {
            first_pass_type(scope, ast->call->variadic_tempvar->var->type);
        }
        break;
    case AST_SLICE:
        ast->slice->object = first_pass(scope, ast->slice->object);
        if (ast->slice->offset != NULL) {
            ast->slice->offset = first_pass(scope, ast->slice->offset);
        }
        if (ast->slice->length != NULL) {
            ast->slice->length = first_pass(scope, ast->slice->length);
        }
        break;
    case AST_INDEX:
        ast->index->object = first_pass(scope, ast->index->object);
        ast->index->index = first_pass(scope, ast->index->index);
        break;
    case AST_CONDITIONAL:
        ast->cond->condition = first_pass(scope, ast->cond->condition);
        ast->cond->scope = new_scope(scope);
        if (ast->cond->else_body != NULL) {
            ast->cond->else_scope = new_scope(scope);
        }
        break;
    case AST_WHILE:
        ast->while_loop->condition = first_pass(scope, ast->while_loop->condition);
        ast->while_loop->scope = new_loop_scope(scope);
        break;
    case AST_FOR:
        ast->for_loop->iterable = first_pass(scope, ast->for_loop->iterable);
        ast->for_loop->scope = new_loop_scope(scope);
        if (ast->for_loop->itervar->type != NULL) {
            first_pass_type(scope, ast->for_loop->itervar->type);
        }
        if (ast->for_loop->index != NULL) {
            first_pass_type(scope, ast->for_loop->index->type);
        }
        break;
    case AST_ANON_SCOPE:
        ast->anon_scope->scope = new_scope(scope);
        break;
    case AST_BLOCK:
        break;
    case AST_RETURN:
        if (ast->ret->expr) {
            ast->ret->expr = first_pass(scope, ast->ret->expr);
        }
        break;
    case AST_CAST:
        ast->cast->object = first_pass(scope, ast->cast->object);
        first_pass_type(scope, ast->cast->cast_type);
        break;
    case AST_DIRECTIVE:
        if (ast->directive->object != NULL) {
            ast->directive->object = first_pass(scope, ast->directive->object);
        }
        if (ast->var_type != NULL) {
            first_pass_type(scope, ast->var_type);
        }
        break;
    case AST_TYPEINFO:
        first_pass_type(scope, ast->typeinfo->typeinfo_target);
        break;
    case AST_USE:
        ast->use->object = first_pass(scope, ast->use->object);
        break;
    case AST_DEFER:
        ast->defer->call = first_pass(scope, ast->defer->call);
        break;
    case AST_TYPE_OBJ:
        first_pass_type(scope, ast->type_obj->t);
        break;
    case AST_SPREAD:
        ast->spread->object = first_pass(scope, ast->spread->object);
        break;
    case AST_NEW:
        if (ast->new->count != NULL) {
            ast->new->count = first_pass(scope, ast->new->count);
        }
        first_pass_type(scope, ast->new->type);
        break;
    case AST_IMPL:
        if (scope->parent != NULL) {
            error(ast->line, ast->file, "Type impl declarations must be at root scope.");
        }

        first_pass_type(scope, ast->impl->type);

        if (!(ast->impl->type->name  || ast->impl->type->resolved->comp == PARAMS)) {
            // others to do?
            error(ast->line, ast->file, "Type impl declarations only valid for named types.");
        }

        in_impl = 1;
        for (int i = 0; i < array_len(ast->impl->methods); i++) {
            Ast *meth = ast->impl->methods[i];
            if (meth->type != AST_FUNC_DECL) {
                error(meth->line, meth->file, "Only method declarations are valid inside an impl block.");
            }

            first_pass(scope, meth);

            if (meth->fn_decl->args == NULL) {
                error(meth->line, meth->file, "Method '%s' of type %s must have a receiver argument.", meth->fn_decl->var->name, type_to_string(ast->impl->type));
            }
            // TODO: check name against struct/enum members (or "builtin members" for string/array)
            Ast *last_decl = define_method(scope, ast->impl->type, meth);
            if (last_decl != NULL) {
                error(meth->line, meth->file, "Method '%s' of type %s already defined at %s:%d.", meth->fn_decl->var->name, type_to_string(ast->impl->type), last_decl->file, last_decl->line);
            }
        }
        in_impl = 0;
        break;
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_IDENTIFIER:
        break;
    case AST_METHOD:
        error(ast->line, ast->file, "<internal> first_pass on method");
        break;
    case AST_TYPE_IDENT:
        error(ast->line, ast->file, "<internal> first_pass on type ident");
        break;
    case AST_PACKAGE:
        error(ast->line, ast->file, "<internal> first_pass on ast package");
        break;
    }
    return ast;
}

AstBlock *check_block_semantics(Scope *scope, AstBlock *block, int fn_body) {
    for (int i = 0; i < array_len(block->statements); i++) {
        block->statements[i] = first_pass(scope, block->statements[i]);
    }

    int mainline_return_reached = 0;
    for (int i = 0; i < array_len(block->statements); i++) {
        Ast *stmt = block->statements[i];
        if (i > 0) {
            switch (block->statements[i-1]->type) {
            case AST_RETURN:
                error(stmt->line, stmt->file, "Unreachable statements following return.");
                break;
            case AST_BREAK:
                error(stmt->line, stmt->file, "Unreachable statements following break.");
                break;
            case AST_CONTINUE:
                error(stmt->line, stmt->file, "Unreachable statements following continue.");
                break;
            default:
                break;
            }
        }
        stmt = check_semantics(scope, stmt);
        if (needs_temp_var(stmt)) {
            allocate_ast_temp_var(scope, stmt);
        }
        if (stmt->type == AST_RETURN) {
            mainline_return_reached = 1;
        }
        block->statements[i] = stmt;
    }

    // if it's a function that needs return and return wasn't reached, break
    if (!mainline_return_reached && fn_body) {
        Type *rt = fn_scope_return_type(scope);
        if (rt && !is_void(rt)) {
            error(block->endline, block->file,
                "Control reaches end of function '%s' without a return statement.",
                closest_fn_scope(scope)->fn_var->name);
            // TODO: anonymous function ok here?
        }
    }
    return block;
}

static Ast *check_directive_semantics(Scope *scope, Ast *ast) {
    // TODO does this checking need to happen in the first pass? may be a
    // reason to do this later!
    char *n = ast->directive->name;
    if (!strcmp(n, "typeof")) {
        ast->directive->object = check_semantics(scope, ast->directive->object);
        // TODO should this only be acceptible on identifiers? That would be
        // more consistent (don't have to explain that #type does NOT evaluate
        // arguments -- but is that too restricted / unnecessary?
        Ast *t = ast_alloc(AST_TYPEINFO);
        t->line = ast->line;
        t->typeinfo->typeinfo_target = ast->directive->object->var_type;
        t->var_type = typeinfo_ref();
        return t;
    } else if (!strcmp(n, "type")) {
        Ast *t = ast_alloc(AST_TYPEINFO);
        t->line = ast->line;
        t->typeinfo->typeinfo_target = resolve_type(ast->var_type); // this is the only place this will be done!
        t->var_type = typeinfo_ref();
        return t;
    }

    error(ast->line, ast->file, "Unrecognized directive '%s'.", n);
    return NULL;
}

static Ast *_check_struct_literal_semantics(Scope *scope, Ast *ast, Type *type) {
    ResolvedType *r = type->resolved;
    assert(r->comp == STRUCT);

    // TODO: resolve_type here or somewhere else?
    ast->var_type = type;

    AstLiteral *lit = ast->lit;

    // if not named, check that there are enough members
    if (!lit->compound_val.named && array_len(lit->compound_val.member_exprs) != 0) {
        if (array_len(lit->compound_val.member_exprs) != array_len(r->st.member_names)) {
            error(ast->line, ast->file, "Wrong number of members for struct '%s', expected %d but received %d.",
                type_to_string(type), array_len(r->st.member_names), array_len(lit->compound_val.member_exprs));
        }
    }

    for (int i = 0; i < array_len(lit->compound_val.member_exprs); i++) {
        Ast *expr = check_semantics(scope, lit->compound_val.member_exprs[i]);
        Type *t = r->st.member_types[i];
        char *name = r->st.member_names[i];

        if (lit->compound_val.named) {
            int found = -1;
            for (int j = 0; j < array_len(r->st.member_names); j++) {
                if (!strcmp(lit->compound_val.member_names[i], r->st.member_names[j])) {
                    found = j;
                    break;
                }
            }
            if (found == -1) {
                error(ast->line, ast->file, "Struct '%s' has no member named '%s'.",
                        type_to_string(type), lit->compound_val.member_names[i]);
            }

            t = r->st.member_types[found];
            name = r->st.member_names[found];
        }

        if (!check_type(t, expr->var_type)) {
            Ast *coerced = coerce_type(scope, t, expr, 1);
            if (!coerced) {
                error(ast->line, ast->file, "Type mismatch in struct literal '%s' field '%s', expected %s but received %s.",
                    type_to_string(type), name, type_to_string(t), type_to_string(expr->var_type));
            }
            expr = coerced;
        }

        lit->compound_val.member_names[i] = name;
        lit->compound_val.member_exprs[i] = expr;
    }
    return ast;
}

static Ast *check_struct_literal_semantics(Scope *scope, Ast *ast) {
    Type *type = ast->lit->compound_val.type;
    if (contains_generic_struct(type)) {
        type = reify_struct(scope, ast, type);
    }
    Type *tmp = resolve_type(type);
    if (!tmp) {
        check_for_undefined(ast, type);
    }
    ast->lit->compound_val.type = tmp;
    return _check_struct_literal_semantics(scope, ast, type);
}

static Ast *check_compound_literal_semantics(Scope *scope, Ast *ast) {
    Type *type = ast->lit->compound_val.type;
    if (contains_generic_struct(type)) {
        type = reify_struct(scope, ast, type);
    }
    Type *tmp = resolve_type(type);
    if (!tmp) {
        check_for_undefined(ast, type);
    }
    ast->lit->compound_val.type = tmp;

    long nmembers = array_len(ast->lit->compound_val.member_exprs);

    ResolvedType *resolved = type->resolved;
    if (resolved->comp == STRUCT) {
        ast->lit->lit_type = STRUCT_LIT;
        return _check_struct_literal_semantics(scope, ast, type);
    } else if (resolved->comp == STATIC_ARRAY) {
        // validate length
        if (resolved->array.length == -1) {
            resolved->array.length = nmembers;
        } else if (resolved->array.length != nmembers) {
            error(ast->line, ast->file, "Wrong number of members for static array literal, expected %d but received %d.",
                resolved->array.length, nmembers);
        }
    } else if (resolved->comp == ARRAY) {
        // TODO: need something here?
    } else {
        error(ast->line, ast->file, "Invalid compound literal: type '%s' is not a struct or array.", type_to_string(type));
    }

    // otherwise we have an array
    ast->lit->lit_type = ARRAY_LIT;
    ast->var_type = type;

    Type *inner = type->resolved->array.inner;

    for (int i = 0; i < nmembers; i++) {
        Ast *expr = check_semantics(scope, ast->lit->compound_val.member_exprs[i]);

        if (!check_type(inner, expr->var_type)) {
            expr = coerce_type(scope, inner, expr, 1);
            if (!expr) {
                error(ast->line, ast->file, "Type mismatch in array literal '%s', expected %s but got %s.",
                    type_to_string(type), type_to_string(inner), type_to_string(expr->var_type));
            }
        }
        ast->lit->compound_val.member_exprs[i] = expr;
    }

    // TODO: should this be resolved, or type?
    Type *tmpvar_type = make_static_array_type(inner, nmembers);
    ast->lit->compound_val.array_tempvar = make_temp_var(scope, tmpvar_type, ast->id);
    return ast;
}

static Ast *check_use_semantics(Scope *scope, Ast *ast) {
    if (scope->type == Root) {
        error(ast->line, ast->file, "'use' is not permitted in global scope.");
    }
    if (ast->dot->object->type == AST_IDENTIFIER) {
        ast->dot->object = check_ident_semantics(scope, ast->dot->object);
        // handle the below types specifically, without running check_semantics
        // again (TODO: find a way to make this easier/smoother)
    } else if (ast->dot->object->type != AST_TYPE_IDENT && ast->dot->object->type != AST_PACKAGE) {
        ast->dot->object = check_semantics(scope, ast->dot->object);
    }

    if (ast->use->object->type == AST_TYPE_IDENT) {
        Type *t = ast->use->object->type_ident->type;
        ResolvedType *resolved = t->resolved;
        if (resolved->comp != ENUM) {
            error(ast->use->object->line, ast->file,
                "'use' is not valid on non-enum type '%s'.", type_to_string(t));
        }

        for (int i = 0; i < array_len(resolved->en.member_names); i++) {
            char *name = resolved->en.member_names[i];

            if (lookup_local_var(scope, name) != NULL) {
                error(ast->use->object->line, ast->file,
                    "'use' statement on enum '%s' conflicts with local variable named '%s'.", type_to_string(t), name);
            }

            if (find_builtin_var(name) != NULL) {
                error(ast->use->object->line, ast->file,
                    "'use' statement on enum '%s' conflicts with builtin variable named '%s'.", type_to_string(t), name);
            }

            Var *v = make_var(name, t);
            v->constant = 1;

            // proxy to literal
            Ast *a = ast_alloc(AST_LITERAL);
            a->line = ast->line;
            a->file = ast->file;
            a->lit->lit_type = ENUM_LIT;
            a->lit->enum_val.enum_index = i;
            a->lit->enum_val.enum_type = t;
            a->var_type = t;
            v->proxy = a;
            array_push(scope->vars, v);
        }
        return ast;
    } else if (ast->use->object->type == AST_PACKAGE) {
        Package *p = ast->use->object->pkg->package;
        for (int i = 0; i < array_len(p->scope->vars); i++) {
            Var *v = p->scope->vars[i];
            if (v->proxy) {
                // skip use aliases
                continue;
            }
            if (lookup_local_var(scope, v->name) != NULL) {
                error(ast->use->object->line, ast->file,
                    "'use' statement on package '%s' conflicts with local variable named '%s'.", p->name, v->name);
            }

            if (find_builtin_var(v->name) != NULL) {
                error(ast->use->object->line, ast->file,
                    "'use' statement on enum '%s' conflicts with builtin variable named '%s'.", p->name, v->name);
            }

            Var *new_v = make_var(v->name, v->type);
            Ast *dot = make_ast_dot_op(ast->use->object, v->name);
            dot->line = ast->line;
            dot->file = ast->file;
            new_v->proxy = dot;
            array_push(scope->vars, new_v);
        }
        return ast;
    }

    Type *orig = ast->use->object->var_type;
    Type *t = orig;

    if (t->resolved->comp == REF) {
        t = t->resolved->ref.inner;
    }

    if (t->resolved->comp != STRUCT) {
        error(ast->use->object->line, ast->file,
            "'use' is not valid on non-struct type '%s'.", type_to_string(orig));
    }

    char *name = get_varname(ast->use->object);
    if (name == NULL) {
        error(ast->use->object->line, ast->use->object->file, "Can't 'use' a non-variable.");
    }

    for (int i = 0; i < array_len(t->resolved->st.member_names); i++) {
        char *member_name = t->resolved->st.member_names[i];
        if (lookup_local_var(scope, member_name) != NULL) {
            error(ast->use->object->line, ast->file,
                "'use' statement on struct type '%s' conflicts with local variable named '%s'.",
                type_to_string(t), member_name);
        }
        if (find_builtin_var(member_name) != NULL) {
            error(ast->use->object->line, ast->file,
                "'use' statement on struct type '%s' conflicts with builtin type named '%s'.",
                type_to_string(t), member_name);
        }

        Var *v = make_var(member_name, t->resolved->st.member_types[i]);
        Ast *dot = make_ast_dot_op(ast->use->object, member_name);
        dot->line = ast->line;
        dot->file = ast->file;
        v->proxy = check_semantics(scope, dot);
        array_push(scope->vars, v);
    }
    return ast;
}

static Ast *check_slice_semantics(Scope *scope, Ast *ast) {
    AstSlice *slice = ast->slice;

    slice->object = check_semantics(scope, slice->object);
    if (needs_temp_var(slice->object)) {
        allocate_ast_temp_var(scope, slice->object);
    }

    Type *a = slice->object->var_type;

    if (a->resolved->comp == ARRAY || a->resolved->comp == STATIC_ARRAY) {
        ast->var_type = make_array_type(a->resolved->array.inner);
    } else if (a->resolved->comp == BASIC && a->resolved->data->base == STRING_T) {
        // TODO: is this right?
        ast->var_type = a;
    } else {
        error(ast->line, ast->file, "Cannot slice non-array type '%s'.", type_to_string(a));
    }

    if (slice->offset != NULL) {
        slice->offset = check_semantics(scope, slice->offset);

        // TODO: checks here for string literal
        if (slice->offset->type == AST_LITERAL && a->resolved->comp == STATIC_ARRAY) {
            // TODO check that it's an int?
            long o = slice->offset->lit->int_val;
            if (o < 0) {
                error(ast->line, ast->file, "Negative slice start is not allowed.");
            } else if (o >= a->resolved->array.length) {
                error(ast->line, ast->file,
                    "Slice offset outside of array bounds (offset %ld to array length %ld).",
                    o, a->resolved->array.length);
            }
        }
    }

    if (slice->length != NULL) {
        slice->length = check_semantics(scope, slice->length);

        if (slice->length->type == AST_LITERAL && a->resolved->comp == STATIC_ARRAY) {
            // TODO check that it's an int?
            long l = slice->length->lit->int_val;
            if (l > a->resolved->array.length) {
                error(ast->line, ast->file,
                        "Slice length outside of array bounds (%ld to array length %ld).",
                        l, a->resolved->array.length);
            }
        }
    }
    return ast;
}

static Ast *check_declaration_semantics(Scope *scope, Ast *ast) {
    AstDecl *decl = ast->decl;
    Ast *init = decl->init;

    if (decl->var->type) {
        Type *t = decl->var->type;
        if (contains_generic_struct(t)) {
            t = reify_struct(scope, ast, t);
        }
        Type *tmp = resolve_type(t);
        if (!tmp) {
            check_for_undefined(ast, t);
        }
        t = tmp;
        if (init == NULL && t->resolved->comp == STRUCT && t->resolved->st.generic) {
            error(ast->line, ast->file, "Cannot declare variable '%s' of parametrized type '%s' without parameters.",
                  decl->var->name, type_to_string(decl->var->type));
        }
        decl->var->type = t;
        register_type(t);
    }

    if (init == NULL) {
        if (decl->var->type->resolved->comp == STATIC_ARRAY && decl->var->type->resolved->array.length == -1) {
            error(ast->line, ast->file, "Cannot use unspecified array type for variable '%s' without initialization.", decl->var->name);
        }
    } else {
        decl->init = check_semantics(scope, init);
        init = decl->init;

        if (decl->init->type == AST_LITERAL &&
                init->var_type->resolved->comp == STATIC_ARRAY) {
            TempVar *tmp = decl->init->lit->compound_val.array_tempvar;
            if (tmp != NULL) {
                remove_temp_var_by_id(scope, tmp->var->id);
            }
        }

        if (decl->var->type == NULL) {
            decl->var->type = init->var_type;
            ResolvedType *r = decl->var->type->resolved;
            switch (r->comp) {
            case STRUCT:
                init_struct_var(decl->var); // need to do this anywhere else?
                break;
            // TODO: is there a better way to do this?
            case REF:
                if (r->ref.owned) {
                    if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                        || init->type == AST_CALL)) {
                        decl->var->type = make_ref_type(r->ref.inner);
                    }
                }
                break;
            case ARRAY:
                if (r->array.owned) {
                    if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                        || init->type == AST_CALL)) {
                        decl->var->type = make_array_type(r->array.inner);
                    }
                }
                break;
            default:
                break;
            }
        } else {
            Type *lt = decl->var->type;
            ResolvedType *resolved = lt->resolved;
            ResolvedType *init_res = init->var_type->resolved;
            
            // owned references
            if (resolved->comp == REF && resolved->ref.owned) {
                if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                    || (init->type == AST_CALL && init_res->comp == REF && init_res->ref.owned))) {
                    error(ast->line, ast->file, "Owned reference can only be assigned from new or move expression.");
                }
            }

            if (resolved->comp == ARRAY && resolved->array.owned) {
                if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                    || (init->type == AST_CALL && init_res->comp == ARRAY && init_res->array.owned))) {
                    error(ast->line, ast->file, "Owned array slice can only be assigned from new or move expression.");
                }
            }

            // TODO: shouldn't this check RHS type first?
            // TODO: do we want to resolve here?
            if (resolved->comp == STATIC_ARRAY && resolved->array.length == -1) {
                resolved->array.length = init_res->array.length;
            }

            // TODO: should do this for assign also?
            if (init->type == AST_LITERAL && is_numeric(lt)) {
                int b = resolved->data->base;
                if (init->lit->lit_type == FLOAT && (b == UINT_T || b == INT_T)) {
                    error(ast->line, ast->file, "Cannot implicitly cast float literal '%f' to integer type '%s'.", init->lit->float_val, type_to_string(lt));
                }
                if (b == UINT_T) {
                    if (init->lit->int_val < 0) {
                        error(ast->line, ast->file, "Cannot assign negative integer literal '%d' to unsigned type '%s'.", init->lit->int_val, type_to_string(lt));
                    }
                    if (precision_loss_uint(lt, init->lit->int_val)) {
                        error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, type_to_string(lt));
                    }
                } else if (b == INT_T) {
                    if (precision_loss_int(lt, init->lit->int_val)) {
                        error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, type_to_string(lt));
                    }
                } else if (b == FLOAT_T) {
                    if (precision_loss_float(lt, init->lit->lit_type == FLOAT ? init->lit->float_val : init->lit->int_val)) {
                        error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->float_val, type_to_string(lt));
                    }
                } else {
                    error(-1, "internal", "wtf");
                }
            }

            if (!check_type(lt, init->var_type)) {
                init = coerce_type(scope, lt, init, 1);
                if (!init) {
                    error(ast->line, ast->file, "Cannot assign value '%s' to type '%s'.",
                            type_to_string(decl->init->var_type), type_to_string(lt));
                }
                decl->init = init;
            }
        }

    }

    if (lookup_local_var(scope, decl->var->name) != NULL) {
        error(ast->line, ast->file, "Declared variable '%s' already exists.", decl->var->name);
    }

    if (local_type_name_conflict(scope, decl->var->name)) {
        error(ast->line, ast->file, "Variable name '%s' already in use by type.", decl->var->name);
    }

    ast->var_type = base_type(VOID_T);

    if (scope->parent == NULL) {
        ast->decl->global = 1;
        define_global(decl->var);
        array_push(scope->vars, ast->decl->var); // should this be after the init parsing?

        if (decl->init != NULL) {
            Ast *id = ast_alloc(AST_IDENTIFIER);
            id->line = ast->line;
            id->file = ast->file;
            id->ident->varname = decl->var->name;
            id->ident->var = decl->var;

            id = check_semantics(scope, id);

            ast = make_ast_assign(id, decl->init);
            ast->line = id->line;
            ast->file = id->file;
            return ast;
        }
    } else {
        array_push(scope->vars, ast->decl->var); // should this be after the init parsing?
    }

    return ast;
}

static int verify_arg_count(Scope *scope, Ast *ast, Type *fn_type) {
    int given = array_len(ast->call->args);
    int defined_arg_count = array_len(fn_type->resolved->fn.args);

    if (!fn_type->resolved->fn.variadic) {
        if (given != defined_arg_count) {
            error(ast->line, ast->file,
                "Incorrect argument count to function (expected %d, got %d)",
                defined_arg_count, given);
        }
        return -1;
    }

    if (given < defined_arg_count - 1) {
        error(ast->line, ast->file,
            "Expected at least %d arguments to variadic function, but only got %d.",
            defined_arg_count-1, given);
        return -1;
    }

    for (int i = 0; i < array_len(ast->call->args); i++) {
        Ast *arg = ast->call->args[i];
        if (arg->type == AST_SPREAD) {
            if (i != array_len(ast->call->args)-1) {
                error(ast->line, ast->file, "Only the last argument to a function call may be a spread.");
            }
            if (i != given - 1) {
                error(ast->line, ast->file, "Spread array cannot be combined with other values for variadic function argument.");
            }
            ast->call->has_spread = 1;
        }
    }
    return array_len(ast->call->args);
}

static void verify_arg_types(Scope *scope, Ast *ast, Type **expected_types, Ast **arg_vals, int variadic) {
    AstFnDecl *decl = NULL;
    int ext = 0;
    if (ast->call->fn->type == AST_IDENTIFIER) {
        decl = ast->call->fn->ident->var->fn_decl;
        if (decl != NULL) {
            ext = decl->var->ext;
        }
    }

    int j = 0;
    for (int i = 0; i < array_len(arg_vals); i++) {
        Ast *arg = arg_vals[i];
        Type *expected = resolve_polymorph_recursively(expected_types[j]);

        if (arg->type == AST_SPREAD) {
            // handled by verify_arg_count
            assert(variadic);
            assert(ast->call->has_spread);
            assert(j == array_len(expected_types) - 1);
            assert(!ext);

            arg = arg->spread->object;

            if (!is_array(arg->var_type)) {
                error(ast->line, ast->file, "Cannot spread non-array type '%s'.", type_to_string(arg->var_type));
            }

            if (!check_type(arg->var_type->resolved->array.inner, expected)) {
                error(ast->line, ast->file,
                    "Expected argument (%d) of type '%s', but got type '%s'.",
                    i, type_to_string(expected), type_to_string(arg->var_type));
            }

            // TODO: decide if we want to not hide the AST_SPREAD part of this
            arg_vals[i] = arg;
            break;
        }

        if (ext && (decl->ext_autocast & (1 << i))) {
            // TODO: should this check that the cast is still valid? or is total
            // freedom acceptable in this case?
            Ast *c = ast_alloc(AST_CAST);
            c->line = arg->line;
            c->file = arg->file;
            c->cast->cast_type = expected;
            c->cast->object = arg;
            c->var_type = expected;
            arg_vals[i] = c;
        } else if (!check_type(arg->var_type, expected)) {
            Ast* a = coerce_type(scope, expected, arg, 1);
            if (!a) {
                error(ast->line, ast->file, "Expected argument (%d) of type '%s', but got type '%s'.",
                    i, type_to_string(expected), type_to_string(arg->var_type));
            }
            arg_vals[i] = a;
        }

        if (!(variadic && j == array_len(expected_types) - 1)) {
            j++;
        }
    }
}

static AstFnDecl *get_fn_decl(Ast *ast) {
    assert(ast->type == AST_CALL);
    AstFnDecl *decl = NULL;
    if (ast->call->fn->type == AST_IDENTIFIER) {
        decl = ast->call->fn->ident->var->fn_decl;
    } else if (ast->call->fn->type == AST_ANON_FUNC_DECL) {
        decl = ast->call->fn->fn_decl;
    } 
    if (decl == NULL) {
        error(ast->line, ast->file, "<internal> Unable to find function declaration for polymorphic function call.");
    }
    return decl;
}

static Ast *check_poly_call_semantics(Scope *scope, Ast *ast, Type *fn_type) {
    assert(fn_type->resolved->comp == FUNC);
    // collect call arg types
    Type **call_arg_types = NULL;
    for (int i = 0; i < array_len(ast->call->args); i++) {
        Ast *arg = ast->call->args[i];

        if (arg->type == AST_SPREAD) {
            arg->spread->object = check_semantics(scope, arg->spread->object);
            arg->var_type = arg->spread->object->var_type;
            // TODO: there may be an issue here if this is arg->var_type is
            // a STATIC_ARRAY
            array_push(call_arg_types, arg->var_type);
        } else {
            arg = check_semantics(scope, ast->call->args[i]);

            Type *t = arg->var_type;
            if (t->resolved->comp == STATIC_ARRAY) {
                t = make_array_type(t->resolved->array.inner);
            }
            if (!(fn_type->resolved->fn.variadic && i >= array_len(fn_type->resolved->fn.args))) {
                array_push(call_arg_types, t);
            }
        }

        ast->call->args[i] = arg;
    }

    int given_count = verify_arg_count(scope, ast, fn_type);

    fn_type = copy_type(scope, fn_type); // TODO: added this, might not need
    ResolvedType *r = fn_type->resolved;
    // create variadic temp var if necessary (variadic, no spread, more than
    // 0 args to variadic "slot")
    if (r->fn.variadic && !ast->call->has_spread && given_count >= array_len(r->fn.args)) {
        if (array_len(call_arg_types) > 0) {
            long length = array_len(ast->call->args) - (array_len(r->fn.args) - 1);
            Type *a = make_static_array_type(call_arg_types[array_len(call_arg_types)-1], length);
            ast->call->variadic_tempvar = make_temp_var(scope, a, -1);
        }
    }

    AstFnDecl *decl = get_fn_decl(ast);

    Polymorph *match = check_for_existing_polymorph(decl, call_arg_types);
    if (match != NULL) {
        // made a match to existing polymorph
        ast->call->polymorph = match;

        Ast *id = ast_alloc(AST_IDENTIFIER);
        id->line = ast->call->fn->line;
        id->file = ast->call->fn->file;
        id->ident->var = match->var;
        id->ident->varname = "";
        id->var_type = id->ident->var->type;
        ast->call->fn = id;
        ast->var_type = resolve_polymorph_recursively(match->ret);
        return ast;
    } else {
        match = create_polymorph(decl, call_arg_types);
    }

    // Important: copy/clear the type, so that we don't modify another!
    fn_type = copy_type(match->scope, fn_type);
    r = fn_type->resolved;

    decl->scope->polymorph = match;
    ast->call->polymorph = match;
    
    Type **defined_arg_types = NULL;
    Type **fn_arg_types = NULL;
    Var **arg_vars = NULL;

    for (int i = 0; i < array_len(r->fn.args); i++) {
        Type *expected_type = r->fn.args[i];
        if (i >= array_len(ast->call->args)) {
            // we should only get to this point if verify_arg_counts has passed already
            assert(r->fn.variadic);
            assert(i == array_len(r->fn.args)-1); // last argument

            array_push(defined_arg_types, expected_type);

            Var *v = copy_var(match->scope, decl->args[i]);
            v->type = expected_type;
            if (r->fn.variadic && i == array_len(r->fn.args) - 1) {
                v->type = make_array_type(v->type);
            }
            array_push(match->scope->vars, v);
            // TODO: I think this case might be wrong
            array_push(arg_vars, v);
            array_push(fn_arg_types, v->type);
            break;
        }
        Type *arg_type = ast->call->args[i]->var_type;

        if (is_polydef(expected_type)) {
            if (!match_polymorph(match->scope, expected_type, arg_type)) {
                error(ast->line, ast->file, "Expected polymorphic argument type %s, but got an argument of type %s",
                        type_to_string(expected_type), type_to_string(arg_type));
            }

            expected_type = arg_type;

            if (arg_type->resolved->comp == STATIC_ARRAY) {
                expected_type = make_array_type(arg_type->resolved->array.inner);
            }
        }

        array_push(defined_arg_types, expected_type);

        Var *v = copy_var(match->scope, decl->args[i]);
        v->type = expected_type;
        if (r->fn.variadic && i == array_len(r->fn.args) - 1) {
            v->type = make_array_type(v->type);
        }
        array_push(fn_arg_types, v->type);
        /*array_push(match->scope->vars, v);*/
        array_push(arg_vars, v);
    }

    verify_arg_types(scope, ast, defined_arg_types, ast->call->args, r->fn.variadic);

    if (contains_generic_struct(r->fn.ret)) {
        r->fn.ret = reify_struct(match->scope, ast, r->fn.ret);
    }

    Type *ret = copy_type(match->scope, r->fn.ret);
    if (ret != NULL) {
        // TODO: check_for_undefined here?
        ret = resolve_type(ret);
        if (contains_generic_struct(ret)) {
            ret = reify_struct(match->scope, ast, ret);
        }
    } else {
        ret = base_type(VOID_T);
    }

    match->var = make_var("", make_fn_type(fn_arg_types, ret, r->fn.variadic));

    Ast *generated_ast = ast_alloc(AST_FUNC_DECL);
    generated_ast->line = ast->line; // just use call site info for now
    generated_ast->file = ast->file;
    generated_ast->fn_decl->var = match->var;
    generated_ast->fn_decl->anon = decl->anon;
    generated_ast->fn_decl->args = arg_vars;
    generated_ast->fn_decl->scope = match->scope;
    generated_ast->fn_decl->polymorph_of = decl;
    generated_ast->fn_decl->body = match->body;
    /*generated_ast->fn_decl->ext_autocast = decl->ext_autocast;*/
    array_push(global_fn_decls, generated_ast);

    match->ret = ret; // this is used in the body, must be done before check_block_semantics
    generated_ast = check_semantics(match->scope, generated_ast);
    /*match->body = check_block_semantics(match->scope, match->body, 1);*/


    Ast *id = ast_alloc(AST_IDENTIFIER);
    id->line = ast->call->fn->line;
    id->file = ast->call->fn->file;
    id->ident->var = match->var;
    id->ident->varname = "";
    id->var_type = generated_ast->fn_decl->var->type;

    ast->call->fn = id;
    ast->var_type = ret;

    return ast;
}

static Ast *check_call_semantics(Scope *scope, Ast *ast) {
    ast->call->fn = check_semantics(scope, ast->call->fn);

    Type *called_fn_type = ast->call->fn->var_type;
    if (called_fn_type->resolved->comp != FUNC) {
        error(ast->line, ast->file, "Cannot perform call on non-function type '%s'", type_to_string(called_fn_type));
        return NULL;
    }

    if (ast->call->fn->type == AST_METHOD) {
        Ast *m = ast->call->fn;

        Ast *id = ast_alloc(AST_IDENTIFIER);
        id->line = ast->call->fn->line;
        id->file = ast->call->fn->file;
        id->ident->var = m->method->decl->var;
        id->ident->varname = m->method->name;
        id->var_type = ast->call->fn->var_type;

        ast->call->fn = id;
        // TODO: we may need to modify the args array for a method call
        // separately
        /*ast->call->nargs += 1;*/

        Ast *recv = m->method->recv;
        Type *first_arg_type = resolve_type(m->method->decl->args[0]->type);
        if (is_polydef(first_arg_type)) {
            if (!match_polymorph(NULL, first_arg_type, recv->var_type)) {
                if (!(first_arg_type->resolved->comp == REF && match_polymorph(NULL, first_arg_type->resolved->ref.inner, recv->var_type))) {
                    error(ast->line, ast->file, "Expected method '%s' receiver of type '%s', but got type '%s'.",
                        ast->call->fn->method->name, type_to_string(first_arg_type), type_to_string(recv->var_type));
                }
                /*error(ast->line, ast->file, "Expected method '%s' receiver of polymorphic type '%s', but got type '%s'.",*/
                    /*ast->call->fn->method->name, type_to_string(first_arg_type), type_to_string(recv->var_type));*/
                Ast *uop = ast_alloc(AST_UOP);
                uop->line = recv->line;
                uop->file = recv->file;
                uop->unary->op = OP_REF;
                uop->unary->object = recv;
                uop->var_type = make_ref_type(recv->var_type);
                recv = uop;
            }
        } else if (!check_type(recv->var_type, first_arg_type)) {
            if (!(first_arg_type->resolved->comp == REF && check_type(recv->var_type, first_arg_type->resolved->ref.inner))) {
                error(ast->line, ast->file, "Expected method '%s' receiver of type '%s', but got type '%s'.",
                    ast->call->fn->method->name, type_to_string(first_arg_type), type_to_string(recv->var_type));
            }
            Ast *uop = ast_alloc(AST_UOP);
            uop->line = recv->line;
            uop->file = recv->file;
            uop->unary->op = OP_REF;
            uop->unary->object = recv;
            uop->var_type = make_ref_type(recv->var_type);
            recv = uop;
        }

        // "prepend" the receiver to the array
        array_push(ast->call->args, ast->call->args[0]);
        ast->call->args[0] = recv;
    }

    if (is_polydef(called_fn_type)) {
        return check_poly_call_semantics(scope, ast, called_fn_type);
    }
    
    int given_count = verify_arg_count(scope, ast, called_fn_type);

    ResolvedType *r = called_fn_type->resolved;
    // create variadic temp var if necessary
    if (r->fn.variadic && !ast->call->has_spread && array_len(r->fn.args) <= given_count) {
        for (int i = 0; i < array_len(r->fn.args); i++) {
            if (i == array_len(r->fn.args) - 1) {
                long length = array_len(ast->call->args) - (array_len(r->fn.args) - 1);
                Type *a = make_static_array_type(r->fn.args[i], length);
                ast->call->variadic_tempvar = make_temp_var(scope, a, -1); // using -1 here because the arg may already have a tempvar defined, with the same ID
            }
        }
    }
    
    for (int i = 0; i < array_len(ast->call->args); i++) {
        Ast *arg = ast->call->args[i];
        if (arg->type == AST_SPREAD) {
            arg->spread->object = check_semantics(scope, arg->spread->object);
            arg->var_type = arg->spread->object->var_type;
            ast->call->args[i] = arg;
            continue;
        }
        ast->call->args[i] = check_semantics(scope, arg);
    }

    // check types and allocate tempvars for "Any"
    verify_arg_types(scope, ast, r->fn.args, ast->call->args, r->fn.variadic);

    ast->var_type = resolve_polymorph_recursively(r->fn.ret);

    return ast;
}

static Ast *check_func_decl_semantics(Scope *scope, Ast *ast) {
    int poly = 0;
    Scope *type_check_scope = scope;

    Type *fn_type = ast->fn_decl->var->type;
    ResolvedType *r = fn_type->resolved;

    if (is_polydef(fn_type)) {
        type_check_scope = new_scope(scope);
        poly = 1;

        // this is prior to the below loop in case the polydef is after another
        // argument using its type, as in fn (T, $T) -> T
        for (int i = 0; i < array_len(ast->fn_decl->args); i++) {
            if (is_polydef(ast->fn_decl->args[i]->type)) {
                if (i == array_len(ast->fn_decl->args) - 1 && r->fn.variadic) {
                    // variadic is polydef
                    error(ast->line, ast->file, "Variadic function argument cannot define a polymorphic type.");
                }
                // get alias
                define_polydef_alias(type_check_scope, ast->fn_decl->args[i]->type, ast);
            }
        }
    }

    {
        Type *fn_type = ast->fn_decl->var->type;
        Type **fn_args = fn_type->resolved->fn.args;
        for (int i = 0; i < array_len(fn_args); i++) {
            Type *a = fn_args[i];
            // TODO: what if the polydef isn't the generic struct? i.e. fn
            // type, is this okay?
            if (!is_polydef(a) && contains_generic_struct(a)) {
                a = reify_struct(ast->fn_decl->scope, ast, a);
                if (fn_type->resolved->fn.variadic && i == array_len(fn_args) - 1) {
                    ast->fn_decl->args[i]->type = make_array_type(a);
                } else {
                    ast->fn_decl->args[i]->type = a;
                }
                fn_args[i] = a;
                /*ast->fn_decl->args[i]->type = a;*/
            }
            if (!poly) {
                array_push(ast->fn_decl->scope->vars, ast->fn_decl->args[i]);
            }
        }

        Type *ret = ast->fn_decl->var->type->resolved->fn.ret;
        if (ret != NULL && !is_polydef(ast->fn_decl->var->type) && contains_generic_struct(ret)) {
            ast->fn_decl->var->type->resolved->fn.ret = reify_struct(ast->fn_decl->scope, ast, ret);
        }
    }

    for (int i = 0; i < array_len(ast->fn_decl->args); i++) {
        Var *arg = ast->fn_decl->args[i];
        check_for_undefined_with_scope(ast, arg->type, type_check_scope);

        if (!poly) {
            resolve_type(arg->type);
            if (!is_concrete(arg->type)) {
                error(ast->line, ast->file, "Argument '%s' has generic type '%s' (not allowed currently).",
                      arg->name, type_to_string(arg->type));
            }

            if (arg->use) {
                // allow polydef here?
                Type *t = resolve_type(arg->type);
                if (!t) {
                    check_for_undefined(ast, arg->type);
                }
                arg->type = t;
                while (t->resolved->comp == REF) {
                    t = t->resolved->ref.inner;
                }
                if (t->resolved->comp != STRUCT) {
                    error(ast->line, ast->file,
                        "'use' is not allowed on args of non-struct type '%s'.",
                        type_to_string(t));
                }
                Ast *lhs = make_ast_id(arg, arg->name);
                lhs->line = ast->line;
                lhs->file = ast->file;

                ResolvedType *r = t->resolved;
                for (int i = 0; i < array_len(r->st.member_names); i++) {
                    char *name = r->st.member_names[i];
                    if (lookup_local_var(ast->fn_decl->scope, name) != NULL) {
                        error(ast->line, ast->file,
                            "'use' statement on struct type '%s' conflicts with existing argument named '%s'.",
                            type_to_string(t), name);
                    } else if (local_type_name_conflict(scope, name)) {
                        error(ast->line, ast->file,
                            "'use' statement on struct type '%s' conflicts with builtin type named '%s'.",
                            type_to_string(t), name);
                    }

                    Var *v = make_var(name, r->st.member_types[i]);
                    Ast *dot = make_ast_dot_op(lhs, name);
                    dot->line = ast->line;
                    dot->file = ast->file;
                    v->proxy = check_semantics(ast->fn_decl->scope, first_pass(ast->fn_decl->scope, dot));
                    array_push(ast->fn_decl->scope->vars, v);
                }
            }
        }
    }

    check_for_undefined_with_scope(ast, r->fn.ret, type_check_scope);

    if (!poly) {
        resolve_type(r->fn.ret);
        ast->fn_decl->body = check_block_semantics(ast->fn_decl->scope, ast->fn_decl->body, 1);
    }

    if (ast->type == AST_ANON_FUNC_DECL) {
        ast->var_type = fn_type;
    }
    return ast;
}

static Ast *check_index_semantics(Scope *scope, Ast *ast) {
    ast->index->object = check_semantics(scope, ast->index->object);
    ast->index->index = check_semantics(scope, ast->index->index);
    if (needs_temp_var(ast->index->object)) {
        allocate_ast_temp_var(scope, ast->index->object);
    }
    if (needs_temp_var(ast->index->index)) {
        allocate_ast_temp_var(scope, ast->index->index);
    }

    Type *obj_type = ast->index->object->var_type;
    Type *ind_type = ast->index->index->var_type;

    int size = -1;

    if (is_string(obj_type)) {
        ast->var_type = base_numeric_type(UINT_T, 8);
        if (ast->index->object->type == AST_LITERAL) {
            size = strlen(ast->index->object->lit->string_val);
        }
    } else if (obj_type->resolved->comp == STATIC_ARRAY) {
        size = obj_type->resolved->array.length;
        ast->var_type = obj_type->resolved->array.inner;
    } else if (obj_type->resolved->comp == ARRAY) {
        ast->var_type = obj_type->resolved->array.inner;
    } else {
        error(ast->index->object->line, ast->index->object->file,
            "Cannot perform index/subscript operation on non-array type (type is '%s').",
            type_to_string(obj_type));
    }

    ResolvedType *resolved = ind_type->resolved;
    if (resolved->comp != BASIC ||
       (resolved->data->base != INT_T &&
        resolved->data->base != UINT_T)) {
        error(ast->line, ast->file, "Cannot index array with non-integer type '%s'.",
                type_to_string(ind_type));
    }

    if (ast->index->index->type == AST_LITERAL) {
        int i = ast->index->index->lit->int_val;
        if (i < 0) {
            error(ast->line, ast->file, "Negative index is not allowed.");
        } else if (size != -1 && i >= size) {
            error(ast->line, ast->file, "Array index is larger than object length (%ld vs length %ld).", i, size);
        }
    }

    return ast;
}

Ast *check_semantics(Scope *scope, Ast *ast) {
    switch (ast->type) {
    case AST_LITERAL: {
        switch (ast->lit->lit_type) {
        case STRING: 
            ast->var_type = base_type(STRING_T);
            break;
        case CHAR: 
            ast->var_type = base_numeric_type(UINT_T, 8);
            break;
        case INTEGER: 
            ast->var_type = base_type(INT_T);
            break;
        case FLOAT: 
            ast->var_type = base_type(FLOAT_T);
            break;
        case BOOL: 
            ast->var_type = base_type(BOOL_T);
            break;
        case STRUCT_LIT:
            ast = check_struct_literal_semantics(scope, ast);
            register_type(ast->var_type);
            break;
        case ARRAY_LIT:
        case COMPOUND_LIT:
            ast = check_compound_literal_semantics(scope, ast);
            register_type(ast->var_type);
            break;
        case ENUM_LIT: // eh?
            break;
        }
        break;
    }
    case AST_IDENTIFIER: {
        ast = check_ident_semantics(scope, ast);
        if (ast->type == AST_PACKAGE || ast->type == AST_TYPE_IDENT) { // TODO:  make this nicer/more robust
            error(ast->line, ast->file, "Non-variable identifier used outside of valid expression.");
        }
        break;
    }
    case AST_DOT:
        return check_dot_op_semantics(scope, ast);
    case AST_ASSIGN:
        return check_assignment_semantics(scope, ast);
    case AST_BINOP:
        return check_binop_semantics(scope, ast);
    case AST_UOP:
        return check_uop_semantics(scope, ast);
    case AST_USE:
        return check_use_semantics(scope, ast);
    case AST_SLICE:
        return check_slice_semantics(scope, ast);
    case AST_DEFER:
        if (ast->defer->call->type != AST_CALL) {
            error(ast->line, ast->file, "Defer statement must be a function call.");
        }
        if (scope->parent == NULL) {
            error(ast->line, ast->file, "Defer statement cannot be at root scope.");
        }
        array_push(scope->deferred, check_semantics(scope, ast->defer->call));
        break;
    case AST_NEW: {
        ast->var_type = ast->new->type;
        Type *res = resolve_type(ast->var_type);
        if (!res) {
            check_for_undefined(ast, ast->var_type);
            error(ast->line, ast->file, "Unknown type for new, '%s'", type_to_string(ast->var_type));
        }

        if (res->resolved->comp == ARRAY) {
            // could be a named array type?
            if (ast->new->count == NULL) {
                error(ast->line, ast->file, "Cannot call 'new' on array type '%s' without specifying a count.", type_to_string(ast->var_type));
            }

            ast->new->count = check_semantics(scope, ast->new->count);
            Type *count_type = ast->new->count->var_type;
            if (count_type->resolved->comp != BASIC || !(count_type->resolved->data->base == INT_T || count_type->resolved->data->base == UINT_T)) {
                error(ast->line, ast->file, "Count for a new array must be an integer type, not '%s'", type_to_string(ast->new->count->var_type));
            }
            if (ast->new->count->type == AST_LITERAL) {
                assert(ast->new->count->lit->lit_type == INTEGER);
                if (ast->new->count->lit->int_val < 1) {
                    error(ast->line, ast->file, "Count for a new array must be greater than 0.");
                }
            }
        } else {
            // parser should reject this
            assert(ast->new->count == NULL); 
            assert(res->resolved->comp == REF);

            // TODO: blegh.. this kind of expression should be shorter / easier
            if (res->resolved->ref.inner->resolved->comp != STRUCT) {
                error(ast->line, ast->file, "Cannot use 'new' on a type that is not a struct or array.");
            }
        }
        ast->var_type = res;
        return ast;
    }
    case AST_CAST: {
        AstCast *cast = ast->cast;
        cast->object = check_semantics(scope, cast->object);
        Type *res = resolve_type(cast->cast_type);
        if (!res) {
            error(ast->line, ast->file, "Unknown cast result type '%s'", type_to_string(cast->cast_type));
        }
        register_type(res);

        if (check_type(cast->object->var_type, res)) {
            error(ast->line, ast->file, "Unnecessary cast, types are both '%s'.", type_to_string(res));
        } else {
            Ast *coerced = coerce_type(scope, res, cast->object, 1);
            if (coerced) {
                // coerce_type does the cast for us 
                cast = coerced->cast;
                ast = coerced;
            } else if (!can_cast(cast->object->var_type, res)) {
                error(ast->line, ast->file, "Cannot cast type '%s' to type '%s'.", type_to_string(cast->object->var_type), type_to_string(res));
            }
        }

        cast->cast_type = res;
        ast->var_type = res;
        break;
    }
    case AST_DECL:
        return check_declaration_semantics(scope, ast);
    case AST_TYPE_DECL: {
        if (lookup_local_var(scope, ast->type_decl->type_name) != NULL) {
            error(ast->line, ast->file, "Type name '%s' already exists as variable.", ast->type_decl->type_name);
        }
        // TODO consider instead just having an "unresolved types" list
        char **ignore = NULL;
        array_push(ignore, ast->type_decl->type_name);
        _check_for_undefined_with_ignore(ast, ast->type_decl->target_type, ignore);
        array_free(ignore);
        // TODO: error about unspecified length
        if (contains_generic_struct(ast->type_decl->target_type)) {
            ast->type_decl->target_type = reify_struct(scope, ast, ast->type_decl->target_type);
        }
        break;
    }
    case AST_TYPE_OBJ:
        error(ast->line, ast->file, "<internal> unexpected type '%s'.", type_to_string(ast->type_obj->t));
        break;
    case AST_EXTERN_FUNC_DECL:
        // TODO: fix this
        if (scope->parent != NULL) {
            error(ast->line, ast->file, "Cannot declare an extern inside scope ('%s').", ast->fn_decl->var->name);
        }
        for (int i = 0; i < array_len(ast->fn_decl->var->type->resolved->fn.args); i++) {
            Type *t = ast->fn_decl->var->type->resolved->fn.args[i];
            if (!resolve_type(t)) {
                error(ast->line, ast->file, "Unknown type '%s' in declaration of extern function '%s'",
                        type_to_string(t), ast->fn_decl->var->name);
            }
        }
        if (!resolve_type(ast->fn_decl->var->type->resolved->fn.ret)) {
            error(ast->line, ast->file, "Unknown type '%s' in declaration of extern function '%s'",
                    type_to_string(ast->fn_decl->var->type->resolved->fn.ret), ast->fn_decl->var->name);
        }

        array_push(scope->vars, ast->fn_decl->var);
        define_global(ast->fn_decl->var);
        break;
    case AST_ANON_FUNC_DECL:
    case AST_FUNC_DECL:
        return check_func_decl_semantics(scope, ast);
    case AST_CALL:
        return check_call_semantics(scope, ast);
    case AST_INDEX: 
        return check_index_semantics(scope, ast);
    case AST_IMPL:
        if (!resolve_type(ast->impl->type)) {
            check_for_undefined(ast, ast->impl->type);
        }
        if (contains_generic_struct(ast->impl->type)) {
            ast->impl->type = reify_struct(scope, ast, ast->impl->type);
        }
        for (int i = 0; i < array_len(ast->impl->methods); i++) {
            Ast *m = check_semantics(scope, ast->impl->methods[i]);
            Type *recv = m->fn_decl->args[0]->type;
            resolve_type(recv);
            if (recv->resolved->comp == REF) {
                recv = recv->resolved->ref.inner;
            }
            if (!is_concrete(ast->impl->type) && recv->resolved->comp == PARAMS) {
                recv = recv->resolved->params.inner;
            }

            if (!check_type(recv, ast->impl->type)) {
                char *name = type_to_string(ast->impl->type);
                error(m->line, m->file, "Method '%s' of type %s must have type %s or a reference to it as the receiver (first) argument.", m->fn_decl->var->name, name, name);
            }
            ast->impl->methods[i] = m;
        }

        ast->var_type = base_type(VOID_T);
        break;
    case AST_CONDITIONAL: {
        AstConditional *c = ast->cond;
        c->condition = check_semantics(scope, c->condition);
        c->scope->parent_deferred = c->scope->parent != NULL ? array_len(c->scope->parent->deferred)-1 : -1;

        // TODO: I don't think typedefs of bool should be allowed here...
        if (!is_bool(c->condition->var_type)) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for if statement.",
                type_to_string(c->condition->var_type));
        }
        
        c->if_body = check_block_semantics(c->scope, c->if_body, 0);

        if (c->else_body != NULL) {
            c->else_body = check_block_semantics(c->else_scope, c->else_body, 0);
            c->else_scope->parent_deferred = c->else_scope->parent != NULL ? array_len(c->else_scope->parent->deferred)-1 : -1;
        }

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_WHILE: {
        AstWhile *lp = ast->while_loop;
        lp->condition = check_semantics(scope, lp->condition);
        lp->scope->parent_deferred = lp->scope->parent != NULL ? array_len(lp->scope->parent->deferred)-1 : -1;

        if (!is_bool(lp->condition->var_type)) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for while loop.",
                    type_to_string(lp->condition->var_type));
        }

        lp->body = check_block_semantics(lp->scope, lp->body, 0);

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_FOR: {
        AstFor *lp = ast->for_loop;
        lp->iterable = check_semantics(scope, lp->iterable);
        lp->scope->parent_deferred = lp->scope->parent != NULL ? array_len(lp->scope->parent->deferred)-1 : -1;

        if (lp->index != NULL) {
            if (!strcmp(lp->index->name, lp->itervar->name)) {
                error(ast->line, ast->file, "Cannot name iteration and index variables the same.");
            }
            Type *t = resolve_type(lp->index->type);
            if (!t) {
                check_for_undefined(ast, lp->index->type);
            }
            lp->index->type = t;

            if (!(t->resolved->data->base == INT_T || t->resolved->data->base == UINT_T)) {
                error(ast->line, ast->file, "Index variable '%s' has type '%s', which is not an integer type.", lp->index->name, type_to_string(t));
            }
            // TODO: check for overflow of index type on static array
            array_push(lp->scope->vars, lp->index);
        }

        Type *it_type = lp->iterable->var_type;
        Type *r = resolve_type(it_type);
        if (!r) {
            check_for_undefined(lp->iterable, r);
        }
        lp->iterable->var_type = r;
        lp->itervar->type = r;

        if (r->resolved->comp == ARRAY || r->resolved->comp == STATIC_ARRAY) {
            lp->itervar->type = r->resolved->array.inner;
        } else if (r->resolved->comp == BASIC && r->resolved->data->base == STRING_T) {
            lp->itervar->type = base_numeric_type(UINT_T, 8);
        } else {
            error(ast->line, ast->file,
                "Cannot use for loop on non-iterable type '%s'.", type_to_string(r));
        }
        if (ast->for_loop->by_reference) {
            lp->itervar->type = make_ref_type(lp->itervar->type);
        }
        // TODO type check for when type of itervar is explicit
        array_push(lp->scope->vars, lp->itervar);
        lp->body = check_block_semantics(lp->scope, lp->body, 0);

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_ANON_SCOPE:
        ast->anon_scope->body = check_block_semantics(ast->anon_scope->scope, ast->anon_scope->body, 0);
        if (ast->anon_scope->scope->parent != NULL) {
            ast->anon_scope->scope->parent_deferred = array_len(ast->anon_scope->scope->parent->deferred)-1;
        }
        break;
    case AST_RETURN: {
        // TODO don't need to copy string being returned?
        Scope *fn_scope = closest_fn_scope(scope);
        if (fn_scope == NULL) {
            error(ast->line, ast->file, "Return statement outside of function body.");
        }

        fn_scope->has_return = 1;

        Type *fn_ret_t = fn_scope->fn_var->type->resolved->fn.ret;
        Type *ret_t = base_type(VOID_T);

        if (fn_scope->polymorph != NULL) {
            fn_ret_t = fn_scope->polymorph->ret;
        }

        if (ast->ret->expr != NULL) {
            ast->ret->expr = check_semantics(scope, ast->ret->expr);
            ret_t = resolve_polymorph_recursively(ast->ret->expr->var_type);
            resolve_type(ret_t);
        }

        if (ret_t->resolved->comp == STATIC_ARRAY) {
            error(ast->line, ast->file, "Cannot return a static array from a function.");
        }

        if (!check_type(fn_ret_t, ret_t)) {
            ast->ret->expr = coerce_type(scope, fn_ret_t, ast->ret->expr, 1);
            if (!ast->ret->expr) {
                error(ast->line, ast->file,
                    "Return statement type '%s' does not match enclosing function's return type '%s'.",
                    type_to_string(ret_t), type_to_string(fn_ret_t));
            }
        }

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_BREAK:
        if (closest_loop_scope(scope) == NULL) {
            error(ast->line, ast->file, "Break statement outside of loop.");
        }
        ast->var_type = base_type(VOID_T);
        break;
    case AST_CONTINUE:
        if (closest_loop_scope(scope) == NULL) {
            error(ast->line, ast->file, "Continue statement outside of loop.");
        }
        ast->var_type = base_type(VOID_T);
        break;
    case AST_BLOCK:
        ast->block = check_block_semantics(scope, ast->block, 0);
        break;
    case AST_DIRECTIVE:
        return check_directive_semantics(scope, ast);
    case AST_ENUM_DECL:
        return check_enum_decl_semantics(scope, ast);
    case AST_TYPEINFO:
        break;
    case AST_IMPORT:
        if (!ast->import->package->semantics_checked) {
            Package *p = ast->import->package;
            p->semantics_checked = 1;
            push_current_package(p);
            /*p->pkg_name = package_name(p->path);*/
            for (int i = 0; i < array_len(p->files); i++) {
                p->files[i]->root = check_semantics(p->scope, p->files[i]->root);
            }
            pop_current_package();
        }
        break;
    case AST_SPREAD:
        error(ast->line, ast->file, "Spread is not valid outside of function call.");
    default:
        error(-1, "internal", "idk parse semantics %d", ast->type);
    }
    return ast;
}
