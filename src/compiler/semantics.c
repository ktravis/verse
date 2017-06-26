#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../array/array.h"
#include "semantics.h"
#include "eval.h"
#include "parse.h"
#include "package.h"
#include "polymorph.h"
#include "types.h"
#include "typechecking.h"

// TODO: do this better
static int in_impl = 0;

void check_for_unresolved_with_scope(Ast *ast, Type *t, Scope *scope) {
    Scope *tmp = t->scope;
    t->scope = scope;
    switch (t->comp) {
    case FUNC:
        for (int i = 0; i < array_len(t->fn.args); i++) {
            check_for_unresolved_with_scope(ast, t->fn.args[i], scope);
        }
        if (t->fn.ret != NULL) {
            check_for_unresolved_with_scope(ast, t->fn.ret, scope);
        }
        break;
    case STRUCT: {
        // line numbers can be weird on this...
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            check_for_unresolved_with_scope(ast, t->st.member_types[i], scope);
        }
        break;
    }
    case ALIAS:
        if (!resolve_alias(t)) {
            error(ast->line, ast->file, "Unknown type '%s'.", t->name, scope);
        }
        break;
    case REF:
        check_for_unresolved_with_scope(ast, t->ref.inner, scope);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        check_for_unresolved_with_scope(ast, t->array.inner, scope);
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

void check_for_unresolved(Ast *ast, Type *t) {
    switch (t->comp) {
    case FUNC:
        for (int i = 0; i < array_len(t->fn.args); i++) {
            check_for_unresolved(ast, t->fn.args[i]);
        }
        if (t->fn.ret != NULL) {
            check_for_unresolved(ast, t->fn.ret);
        }
        break;
    case STRUCT: {
        // line numbers can be weird on this...
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            check_for_unresolved(ast, t->st.member_types[i]);
        }
        break;
    }
    case ALIAS:
        if (!resolve_alias(t)) {
            error(ast->line, ast->file, "Unknown type '%s'.", t->name);
        }
        break;
    case REF:
        check_for_unresolved(ast, t->ref.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        check_for_unresolved(ast, t->array.inner);
        break;
    case BASIC:
    case POLYDEF:
    case PARAMS:
    case EXTERNAL:
    case ENUM:
        break;
    }
}

// TODO: make reify_struct deduplicate reifications?
Type *reify_struct(Scope *scope, Ast *ast, Type *t) {
    t = copy_type(t->scope, t);

    switch (t->comp) {
    case FUNC:
        for (int i = 0; i < array_len(t->fn.args); i++) {
            if (contains_generic_struct(t->fn.args[i])) {
                t->fn.args[i] = reify_struct(scope, ast, t->fn.args[i]);
            }
        }
        if (t->fn.ret != NULL && contains_generic_struct(t->fn.ret)) {
            t->fn.ret = reify_struct(scope, ast, t->fn.ret);
        }
        return t;
    case STRUCT: {
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            if (contains_generic_struct(t->st.member_types[i])) {
                t->st.member_types[i] = reify_struct(scope, ast, t->st.member_types[i]);
            }
        }
        return t;
    }
    case REF:
        if (contains_generic_struct(t->ref.inner)) {
            t->ref.inner = reify_struct(scope, ast, t->ref.inner);
        }
        return t;
    case ARRAY:
    case STATIC_ARRAY:
        if (contains_generic_struct(t->array.inner)) {
            t->array.inner = reify_struct(scope, ast, t->array.inner);
        }
        return t;
    case BASIC:
    case ALIAS:
    case POLYDEF:
    case EXTERNAL:
    case ENUM:
        error(-1, "<internal>", "what are you even");
        break;
    case PARAMS:
        for (int i = 0; i < array_len(t->params.args); i++) {
            if (contains_generic_struct(t->params.args[i])) {
                t->params.args[i] = reify_struct(scope, ast, t->params.args[i]);
            }
        }
        break;
    }

    assert(t->comp == PARAMS);
    assert(t->params.args != NULL);

    Type *inner = resolve_alias(t->params.inner);
    if (inner->comp != STRUCT) {
        error(ast->line, ast->file, "Invalid parameterization, type '%s' is not a generic struct.", type_to_string(t->params.inner));
    }

    Type **given = t->params.args;
    Type **expected = inner->st.arg_params;
    
    for (int i = 0; i < array_len(given); i++) {
        check_for_unresolved(ast, given[i]);
    }

    if (array_len(given) != array_len(expected)) {
        error(ast->line, ast->file, "Invalid parameterization, type '%s' expects %d parameters but received %d.",
              type_to_string(t->params.inner), array_len(expected), array_len(given));
    }

    Type **member_types = array_copy(inner->st.member_types);
    for (int i = 0; i < array_len(given); i++) {
        for (int j = 0; j < array_len(inner->st.member_types); j++) {
            member_types[j] = replace_type(copy_type(member_types[j]->scope, member_types[j]), expected[i], given[i]);
        }
    }

    Type *r = make_struct_type(inner->st.member_names, member_types);
    r->scope = t->scope;
    r->st.generic_base = t;
    register_type(r);

    return r;
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
        if (o->var_type->comp != REF) {
            error(ast->line, ast->file, "Cannot dereference a non-reference type (must cast baseptr).");
        }
        ast->var_type = o->var_type->ref.inner;
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

static Ast *check_dot_op_semantics(Scope *scope, Ast *ast) {
    if (ast->dot->object->type == AST_IDENTIFIER) {
        // TODO: need to make sure that an enum or package in outer scope cannot
        // shadow a locally (or closer to locally) declared variable just
        // because this is happening before the other resolution
        //
        // Enum case
        Type *t = make_type(scope, ast->dot->object->ident->varname);
        Type *resolved = resolve_alias(t);
        if (resolved) {
            if (resolved->comp == ENUM) {
                for (int i = 0; i < array_len(resolved->en.member_names); i++) {
                    if (!strcmp(resolved->en.member_names[i], ast->dot->member_name)) {
                        Ast *a = ast_alloc(AST_LITERAL);
                        a->lit->lit_type = ENUM_LIT;
                        a->lit->enum_val.enum_index = i;
                        a->lit->enum_val.enum_type = resolved;
                        a->var_type = t;
                        return a;
                    }
                }
                // TODO allow BaseType.members
                /*if (!strcmp("members", ast->dot->member_name)) {*/
                    
                /*}*/
                error(ast->line, ast->file, "No value '%s' in enum type '%s'.",
                        ast->dot->member_name, ast->dot->object->ident->varname);
            } else {
                error(ast->line, ast->file, "Can't get member '%s' from non-enum type '%s'.",
                        ast->dot->member_name, ast->dot->object->ident->varname);
            }
        }

        // Package case
        // maybe change this to have check_semantics on the object return an
        // "AST_PACKAGE" type? That way more can be done with it, more
        // versatile?
        Package *p = lookup_imported_package(scope, ast->dot->object->ident->varname);
        Var *v = lookup_var(scope, ast->dot->object->ident->varname);
        if ((v == NULL || v->proxy) && p != NULL) {
            Var *v = lookup_var(p->scope, ast->dot->member_name);
            if (!v) {
                error(ast->line, ast->file, "No declared identifier '%s' in package '%s'.", ast->dot->member_name, p->name);
                // TODO better error for an enum here
            }
            if (v->proxy) { // USE-proxy
                error(ast->line, ast->file, "No declared identifier '%s' in package '%s' (*use* doesn't count).", ast->dot->member_name, p->name);
            }

            // TODO: decide if we want to allow promotion of "exports" via USE
            /*if (v->proxy != NULL) { // USE-proxy*/
                /*Type *resolved = resolve_alias(v->type);*/
                /*if (resolved->comp == ENUM) { // proxy for enum*/
                    /*for (int i = 0; i < resolved->en.nmembers; i++) {*/
                        /*if (!strcmp(resolved->en.member_names[i], v->name)) {*/
                            /*Ast *a = ast_alloc(AST_LITERAL);*/
                            /*a->lit->lit_type = ENUM_LIT;*/
                            /*a->lit->enum_val.enum_index = i;*/
                            /*a->lit->enum_val.enum_type = resolved;*/
                            /*a->var_type = v->type;*/
                            /*return a;*/
                        /*}*/
                    /*}*/
                    /*error(-1, "<internal>", "How'd this happen");*/
                /*} else { // proxy for normal var*/
                    /*int i = 0;*/
                    /*assert(v->proxy != v);*/
                    /*while (v->proxy) {*/
                        /*assert(i++ < 666); // lol*/
                        /*v = v->proxy; // this better not loop!*/
                    /*}*/
                /*}*/
            /*}*/
            ast = ast_alloc(AST_IDENTIFIER);
            ast->ident->var = v;
            ast->var_type = v->type;
            return ast;
        }
    } 
    ast->dot->object = check_semantics(scope, ast->dot->object);

    Type *orig = ast->dot->object->var_type;
    Type *t = orig;

    if (t->comp == REF) {
        t = t->ref.inner;
    }

    // TODO: how much resolving should happen here? does this allow for more
    // indirection than we wanted?
    t = resolve_alias(t);

    Ast *decl = find_method(t, ast->dot->member_name);
    // TODO: this could be nicer
    if (decl == NULL && orig->comp == STRUCT && orig->st.generic_base != NULL) {
        decl = find_method(orig->st.generic_base->params.inner, ast->dot->member_name);
    }
    if (decl != NULL) {
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

    if (t->comp == ARRAY || t->comp == STATIC_ARRAY) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            ast->var_type = make_ref_type(t->array.inner);
        } else {
            error(ast->line, ast->file,
                    "Cannot dot access member '%s' on array (only length or data).", ast->dot->member_name);
        }
    } else if (t->comp == BASIC && t->data->base == STRING_T) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "bytes")) {
            ast->var_type = make_ref_type(base_numeric_type(UINT_T, 8));
        } else {
            error(ast->line, ast->file,
                    "Cannot dot access member '%s' on string (only length or bytes).", ast->dot->member_name);
        }
    } else if (t->comp == STRUCT) {
        for (int i = 0; i < array_len(t->st.member_names); i++) {
            if (!strcmp(ast->dot->member_name, t->st.member_names[i])) {
                ast->var_type = t->st.member_types[i];
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
        Type *lt = resolve_alias(ast->binary->left->index->object->var_type);
        if (lt->comp == BASIC && lt->data->base == STRING_T) {
            error(ast->line, ast->file, "Strings are immutable and cannot be subscripted for assignment.");
        }
    }

    // TODO refactor to "is_constant"
    if (ast->binary->left->type == AST_IDENTIFIER && ast->binary->left->ident->var->constant) {
        error(ast->line, ast->file, "Cannot reassign constant '%s'.", ast->binary->left->ident->var->name);
    }

    Type *lt = resolve_polymorph(ast->binary->left->var_type); // TODO: this is a temporary fix
    Type *rt = resolve_polymorph(ast->binary->right->var_type);

    if (needs_temp_var(ast->binary->right) || is_dynamic(ast->binary->left->var_type)) {
    /*if(needs_temp_var(ast->binary->right)) {*/
        allocate_ast_temp_var(scope, ast->binary->right);
    }

    Type *res_l = resolve_alias(lt);
    /*Type *res_r = resolve_alias(rt);*/

    // owned references
    if (res_l->comp == REF && res_l->ref.owned) {
        if (!(ast->binary->right->type == AST_NEW/* || ast->binary->right == AST_MOVE*/)) {
            error(ast->line, ast->file, "Owned reference can only be assigned from new or move expression.");
        }
    } else if (rt->comp == REF && rt->ref.owned) {
        allocate_ast_temp_var(scope, ast->binary->right);
    }

    if (res_l->comp == ARRAY && res_l->array.owned) {
        if (!(ast->binary->right->type == AST_NEW/* || ast->binary->right == AST_MOVE*/)) {
            error(ast->line, ast->file, "Owned array slice can only be assigned from new or move expression.");
        }
    } else if (rt->comp == ARRAY && rt->array.owned) {
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
            /*errlog("type on left %s", type_to_string(lt));*/
            /*errlog("type on right %s", type_to_string(ast->binary->right->var_type));*/
            /*errlog("right is string lit %d", ast->binary->right->type == AST_LITERAL && ast->binary->right->lit->lit_type == STRING);*/
            /*if (ast->binary->right->type == AST_LITERAL && ast->binary->right->lit->lit_type == STRING) {*/
                /*errlog("right lit value %s", ast->binary->right->lit->string_val);*/
                /*errlog("right lit len %d", strlen(ast->binary->right->lit->string_val));*/
            /*}*/
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
    et = resolve_alias(et);
    assert(et->comp == ENUM);
    Ast **exprs = ast->enum_decl->exprs;

    long val = 0;
    for (int i = 0; i < array_len(et->en.member_names); i++) {
        if (exprs[i] != NULL) {
            exprs[i] = check_semantics(scope, exprs[i]);
            Type *t = resolve_alias(exprs[i]->var_type);
            // TODO allow const other stuff in here
            if (exprs[i]->type != AST_LITERAL) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-constant expression.", et->name, et->en.member_names[i]);
            } else if (t->data->base != INT_T) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-integer expression.", et->name, et->en.member_names[i]);
            }
            Type *e = exprs[i]->var_type;

            if (!check_type(et->en.inner, e)) {
                if (!coerce_type(scope, et->en.inner, exprs[i], 0)) {
                    error(exprs[i]->line, exprs[i]->file,
                        "Cannot initialize enum '%s' member '%s' with expression of type '%s' (base is '%s').",
                        et->name, et->en.member_names[i], type_to_string(e), type_to_string(et->en.inner));
                }
            }
            val = exprs[i]->lit->int_val;
        }
        array_push(et->en.member_values, val);
        val += 1;
        for (int j = 0; j < i; j++) {
            if (!strcmp(et->en.member_names[i], et->en.member_names[j])) {
                error(ast->line, ast->file, "Enum '%s' member name '%s' defined twice.",
                        ast->enum_decl->enum_name, et->en.member_names[i]);
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
    switch (t->comp) {
    case POLYDEF:
    case ALIAS:
    case EXTERNAL:
        break;
    case PARAMS:
        for (int i = 0; i < array_len(t->params.args); i++) {
            first_pass_type(scope, t->params.args[i]);
        }
        first_pass_type(scope, t->params.inner);
        break;
    case FUNC:
        for (int i = 0; i < array_len(t->fn.args); i++) {
            first_pass_type(scope, t->fn.args[i]);
        }
        // register type?
        first_pass_type(scope, t->fn.ret);
        break;
    case STRUCT:
        // TODO: owned references need to check for circular type declarations
        for (int i = 0; i < array_len(t->st.member_types); i++) {
            first_pass_type(scope, t->st.member_types[i]);
        }
        for (int i = 0; i < array_len(t->st.arg_params); i++) {
            first_pass_type(scope, t->st.arg_params[i]);
        }
        if (!t->st.generic) {
            register_type(t);
        }
        break;
    case ENUM:
        first_pass_type(scope, t->en.inner);
        register_type(t);
        break;
    case REF:
        first_pass_type(scope, t->ref.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        first_pass_type(scope, t->array.inner);
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
        {
            Type *fn_type = ast->fn_decl->var->type;
            Type **fn_args = fn_type->fn.args;
            for (int i = 0; i < array_len(fn_args); i++) {
                Type *a = fn_args[i];
                // TODO: what if the polydef isn't the generic struct? i.e. fn
                // type, is this okay?
                if (!is_polydef(a) && contains_generic_struct(a)) {
                    a = reify_struct(ast->fn_decl->scope, ast, a);
                    if (fn_type->fn.variadic && i == array_len(fn_args) - 1) {
                        ast->fn_decl->args[i]->type = make_array_type(a);
                    } else {
                        ast->fn_decl->args[i]->type = a;
                    }
                    fn_args[i] = a;
                }
            }

            Type *ret = ast->fn_decl->var->type->fn.ret;
            if (ret != NULL && !is_polydef(ast->fn_decl->var->type) && contains_generic_struct(ret)) {
                ast->fn_decl->var->type->fn.ret = reify_struct(ast->fn_decl->scope, ast, ret);
            }
        }

        // TODO handle case where arg has same name as func
        if (!is_polydef(ast->fn_decl->var->type)) {
            for (int i = 0; i < array_len(ast->fn_decl->args); i++) {
                array_push(ast->fn_decl->scope->vars, ast->fn_decl->args[i]);
            }
        }
        break;
    case AST_ENUM_DECL:
        if (local_type_name_conflict(scope, ast->enum_decl->enum_name)) {
            error(ast->line, ast->file, "Type named '%s' already exists in local scope.", ast->enum_decl->enum_name);
        }
        // TODO: need to do the exprs here or not?
        first_pass_type(scope, ast->enum_decl->enum_type);
        ast->enum_decl->enum_type = define_type(scope, ast->enum_decl->enum_name, ast->enum_decl->enum_type);
        break;
    case AST_TYPE_DECL:
        ast->type_decl->target_type->scope = scope;
        first_pass_type(scope, ast->type_decl->target_type);
        ast->type_decl->target_type = define_type(scope, ast->type_decl->type_name, ast->type_decl->target_type);
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
        register_type(ast->cast->cast_type);
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

        if (!(ast->impl->type->comp == ALIAS || ast->impl->type->comp == PARAMS)) {
            // others to do?
            error(ast->line, ast->file, "Type impl declarations only valid for named types.");
        }
        if (contains_generic_struct(ast->impl->type)) {
            ast->impl->type = reify_struct(scope, ast, ast->impl->type);
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
            Ast *last_decl = define_method(ast->impl->type, meth);
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
        if (i > 0 && block->statements[i-1]->type == AST_RETURN) {
            error(stmt->line, stmt->file, "Unreachable statements following return.");
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
        if (rt != NULL && rt->data->base != VOID_T) {
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
        t->typeinfo->typeinfo_target = ast->var_type;
        t->var_type = typeinfo_ref();
        return t;
    }

    error(ast->line, ast->file, "Unrecognized directive '%s'.", n);
    return NULL;
}

static Ast *_check_struct_literal_semantics(Scope *scope, Ast *ast, Type *type, Type *resolved) {
    assert(resolved->comp == STRUCT);

    ast->var_type = type;

    AstLiteral *lit = ast->lit;

    // if not named, check that there are enough members
    if (!lit->compound_val.named && array_len(lit->compound_val.member_exprs) != 0) {
        if (array_len(lit->compound_val.member_exprs) != array_len(resolved->st.member_names)) {
            error(ast->line, ast->file, "Wrong number of members for struct '%s', expected %d but received %d.",
                type_to_string(type), array_len(resolved->st.member_names), array_len(lit->compound_val.member_exprs));
        }
    }

    for (int i = 0; i < array_len(lit->compound_val.member_exprs); i++) {
        Ast *expr = check_semantics(scope, lit->compound_val.member_exprs[i]);
        Type *t = resolved->st.member_types[i];
        char *name = resolved->st.member_names[i];

        if (lit->compound_val.named) {
            int found = -1;
            for (int j = 0; j < array_len(resolved->st.member_names); j++) {
                if (!strcmp(lit->compound_val.member_names[i], resolved->st.member_names[j])) {
                    found = j;
                    break;
                }
            }
            if (found == -1) {
                error(ast->line, ast->file, "Struct '%s' has no member named '%s'.",
                        type_to_string(type), lit->compound_val.member_names[i]);
            }

            t = resolved->st.member_types[found];
            name = resolved->st.member_names[found];
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
    // TODO: shouldn't we be doing this?
    /*Type *type = resolve_polymorph(ast->lit->compound_val.type);*/
    Type *type = ast->lit->compound_val.type;
    if (contains_generic_struct(type)) {
        type = reify_struct(scope, ast, type);
    }

    Type *resolved = resolve_alias(type);
    return _check_struct_literal_semantics(scope, ast, type, resolved);
}

static Ast *check_compound_literal_semantics(Scope *scope, Ast *ast) {
    Type *type = ast->lit->compound_val.type;
    if (contains_generic_struct(type)) {
        type = reify_struct(scope, ast, type);
    }

    long nmembers = array_len(ast->lit->compound_val.member_exprs);

    Type *resolved = resolve_alias(type);
    if (resolved->comp == STRUCT) {
        ast->lit->lit_type = STRUCT_LIT;
        return _check_struct_literal_semantics(scope, ast, type, resolved);
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

    Type *inner = type->array.inner;

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
    if (ast->use->object->type == AST_IDENTIFIER) {
        char *used_name = ast->use->object->ident->varname;
        Type *t = make_type(scope, used_name);
        Type *resolved = resolve_alias(t);
        if (resolved != NULL) {
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
                a->lit->lit_type = ENUM_LIT;
                a->lit->enum_val.enum_index = i;
                a->lit->enum_val.enum_type = resolved;
                a->var_type = t;
                v->proxy = a;
                array_push(scope->vars, v);
            }
            return ast;
        }

        Package *p = lookup_imported_package(scope, used_name);
        if (p) {
            for (int i = 0; i < array_len(p->scope->vars); i++) {
                Var *v = p->scope->vars[i];
                if (v->proxy) {
                    // skip use aliases
                    continue;
                }
                if (lookup_local_var(scope, v->name) != NULL) {
                    error(ast->use->object->line, ast->file,
                        "'use' statement on package '%s' conflicts with local variable named '%s'.", used_name, v->name);
                }

                if (find_builtin_var(v->name) != NULL) {
                    error(ast->use->object->line, ast->file,
                        "'use' statement on enum '%s' conflicts with builtin variable named '%s'.", used_name, v->name);
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
    }

    ast->use->object = check_semantics(scope, ast->use->object);

    Type *orig = ast->use->object->var_type;
    Type *t = orig;

    if (t->comp == REF) {
        t = t->ref.inner;
    }

    Type *resolved = resolve_alias(t);
    if (resolved->comp != STRUCT) {
        error(ast->use->object->line, ast->file,
            "'use' is not valid on non-struct type '%s'.", type_to_string(orig));
    }

    char *name = get_varname(ast->use->object);
    if (name == NULL) {
        error(ast->use->object->line, ast->use->object->file, "Can't 'use' a non-variable.");
    }

    for (int i = 0; i < array_len(resolved->st.member_names); i++) {
        char *member_name = resolved->st.member_names[i];
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

        Var *v = make_var(member_name, resolved->st.member_types[i]);
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
    Type *resolved = resolve_alias(a);

    if (resolved->comp == ARRAY || resolved->comp == STATIC_ARRAY) {
        ast->var_type = make_array_type(resolved->array.inner);
    } else if (resolved->comp == BASIC && resolved->data->base == STRING_T) {
        // TODO: is this right?
        ast->var_type = a;
    } else {
        error(ast->line, ast->file, "Cannot slice non-array type '%s'.", type_to_string(a));
    }

    if (slice->offset != NULL) {
        slice->offset = check_semantics(scope, slice->offset);

        // TODO: checks here for string literal
        if (slice->offset->type == AST_LITERAL && resolved->comp == STATIC_ARRAY) {
            // TODO check that it's an int?
            long o = slice->offset->lit->int_val;
            if (o < 0) {
                error(ast->line, ast->file, "Negative slice start is not allowed.");
            } else if (o >= resolved->array.length) {
                error(ast->line, ast->file,
                    "Slice offset outside of array bounds (offset %ld to array length %ld).",
                    o, resolved->array.length);
            }
        }
    }

    if (slice->length != NULL) {
        slice->length = check_semantics(scope, slice->length);

        if (slice->length->type == AST_LITERAL && resolved->comp == STATIC_ARRAY) {
            // TODO check that it's an int?
            long l = slice->length->lit->int_val;
            if (l > resolved->array.length) {
                error(ast->line, ast->file,
                        "Slice length outside of array bounds (%ld to array length %ld).",
                        l, resolved->array.length);
            }
        }
    }
    return ast;
}

static Ast *check_declaration_semantics(Scope *scope, Ast *ast) {
    AstDecl *decl = ast->decl;
    Ast *init = decl->init;

    if (decl->var->type != NULL) {
        check_for_unresolved(ast, decl->var->type);

        Type *t = resolve_alias(decl->var->type);
        if (init == NULL && t->comp == STRUCT && t->st.generic) {
            error(ast->line, ast->file, "Cannot declare variable '%s' of parametrized type '%s' without parameters.",
                  decl->var->name, type_to_string(decl->var->type));
        }

        // TODO: need some sort of "validate_struct" that would catch things
        // like using an invalid parameter in a struct
        if (contains_generic_struct(t)) {
            decl->var->type = reify_struct(scope, ast, t);
        }
    }


    if (init == NULL) {
        if (decl->var->type->comp == STATIC_ARRAY && decl->var->type->array.length == -1) {
            error(ast->line, ast->file, "Cannot use unspecified array type for variable '%s' without initialization.", decl->var->name);
        }
    } else {
        decl->init = check_semantics(scope, init);
        init = decl->init;

        if (decl->init->type == AST_LITERAL &&
                resolve_alias(init->var_type)->comp == STATIC_ARRAY) {
            TempVar *tmp = decl->init->lit->compound_val.array_tempvar;
            if (tmp != NULL) {
                remove_temp_var_by_id(scope, tmp->var->id);
            }
        }

        if (decl->var->type == NULL) {
            decl->var->type = init->var_type;

            switch (resolve_alias(decl->var->type)->comp) {
            case STRUCT:
                init_struct_var(decl->var); // need to do this anywhere else?
                break;
            // TODO: is there a better way to do this?
            case REF:
                if (decl->var->type->ref.owned) {
                    if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                        || init->type == AST_CALL)) {
                        decl->var->type = make_ref_type(decl->var->type->ref.inner);
                    }
                }
                break;
            case ARRAY:
                if (decl->var->type->array.owned) {
                    if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                        || init->type == AST_CALL)) {
                        decl->var->type = make_array_type(decl->var->type->array.inner);
                    }
                }
                break;
            default:
                break;
            }
        } else {
            Type *lt = decl->var->type;
            Type *resolved = resolve_alias(lt);
            
            // owned references
            if (resolved->comp == REF && resolved->ref.owned) {
                if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                    || (init->type == AST_CALL && init->var_type->comp == REF && init->var_type->ref.owned))) {
                    error(ast->line, ast->file, "Owned reference can only be assigned from new or move expression.");
                }
            }

            if (resolved->comp == ARRAY && resolved->array.owned) {
                if (!(init->type == AST_NEW/* || ast->binary->right == AST_MOVE*/
                    || (init->type == AST_CALL && init->var_type->comp == ARRAY && init->var_type->array.owned))) {
                    error(ast->line, ast->file, "Owned array slice can only be assigned from new or move expression.");
                }
            }

            // TODO: shouldn't this check RHS type first?
            // TODO: do we want to resolve here?
            if (resolved->comp == STATIC_ARRAY && resolved->array.length == -1) {
                resolved->array.length = resolve_alias(init->var_type)->array.length;
            }

            // TODO: should do this for assign also?
            if (init->type == AST_LITERAL && is_numeric(resolved)) {
                int b = resolved->data->base;
                if (init->lit->lit_type == FLOAT && (b == UINT_T || b == INT_T)) {
                    error(ast->line, ast->file, "Cannot implicitly cast float literal '%f' to integer type '%s'.", init->lit->float_val, type_to_string(lt));
                }
                if (b == UINT_T) {
                    if (init->lit->int_val < 0) {
                        error(ast->line, ast->file, "Cannot assign negative integer literal '%d' to unsigned type '%s'.", init->lit->int_val, type_to_string(lt));
                    }
                    if (precision_loss_uint(resolved, init->lit->int_val)) {
                        error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, type_to_string(lt));
                    }
                } else if (b == INT_T) {
                    if (precision_loss_int(resolved, init->lit->int_val)) {
                        error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, type_to_string(lt));
                    }
                } else if (b == FLOAT_T) {
                    if (precision_loss_float(resolved, init->lit->lit_type == FLOAT ? init->lit->float_val : init->lit->int_val)) {
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
    // TODO: why? was this supposed to be decl->var->type?
    register_type(ast->var_type);

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
    int defined_arg_count = array_len(fn_type->fn.args);

    if (!fn_type->fn.variadic) {
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
        Type *expected = resolve_polymorph(expected_types[j]);

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

            if (!check_type(arg->var_type->array.inner, expected)) {
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
            c->cast->cast_type = expected;
            c->cast->object = arg;
            c->line = arg->line;
            c->file = arg->file;
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

static Ast *check_poly_call_semantics(Scope *scope, Ast *ast, Type *resolved) {
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
            if (t->comp == STATIC_ARRAY) {
                t = make_array_type(t->array.inner);
            }
            if (!(resolved->fn.variadic && i >= array_len(resolved->fn.args))) {
                array_push(call_arg_types, t);
            }
        }

        ast->call->args[i] = arg;
    }

    int given_count = verify_arg_count(scope, ast, resolved);

    // create variadic temp var if necessary (variadic, no spread, more than
    // 0 args to variadic "slot")
    if (resolved->fn.variadic && !ast->call->has_spread && given_count >= array_len(resolved->fn.args)) {
        if (array_len(call_arg_types) > 0) {
            long length = array_len(ast->call->args) - (array_len(resolved->fn.args) - 1);
            Type *a = make_static_array_type(call_arg_types[array_len(call_arg_types)-1], length);
            ast->call->variadic_tempvar = make_temp_var(scope, a, -1);
        }
    }

    AstFnDecl *decl = get_fn_decl(ast);

    Polymorph *match = check_for_existing_polymorph(decl, call_arg_types);
    if (match != NULL) {
        // made a match to existing polymorph
        ast->call->polymorph = match;
        ast->var_type = resolve_polymorph(match->ret);
        return ast;
    } else {
        match = create_polymorph(decl, call_arg_types);
    }

    decl->scope->polymorph = match;
    ast->call->polymorph = match;
    
    // reset call_args
    Type **defined_arg_types = NULL;

    for (int i = 0; i < array_len(resolved->fn.args); i++) {
        Type *expected_type = resolved->fn.args[i];
        if (i >= array_len(ast->call->args)) {
            // we should only get to this point if verify_arg_counts has passed already
            assert(resolved->fn.variadic);
            assert(i == array_len(resolved->fn.args)-1); // last argument

            array_push(defined_arg_types, expected_type);

            Var *v = copy_var(match->scope, decl->args[i]);
            v->type = expected_type;
            if (resolved->fn.variadic && i == array_len(resolved->fn.args) - 1) {
                v->type = make_array_type(v->type);
            }
            array_push(match->scope->vars, v);
            break;
        }
        Type *arg_type = ast->call->args[i]->var_type;

        if (is_polydef(expected_type)) {
            if (!match_polymorph(match->scope, expected_type, arg_type)) {
                error(ast->line, ast->file, "Expected polymorphic argument type %s, but got an argument of type %s",
                        type_to_string(expected_type), type_to_string(arg_type));
            }

            expected_type = arg_type;

            if (arg_type->comp == STATIC_ARRAY) {
                expected_type = make_array_type(arg_type->array.inner);
            }
        }

        array_push(defined_arg_types, expected_type);

        Var *v = copy_var(match->scope, decl->args[i]);
        v->type = expected_type;
        if (resolved->fn.variadic && i == array_len(resolved->fn.args) - 1) {
            v->type = make_array_type(v->type);
        }
        array_push(match->scope->vars, v);
    }

    verify_arg_types(scope, ast, defined_arg_types, ast->call->args, resolved->fn.variadic);

    if (contains_generic_struct(resolved->fn.ret)) {
        resolved->fn.ret = reify_struct(match->scope, ast, resolved->fn.ret);
    }

    Type *ret = copy_type(match->scope, resolved->fn.ret);
    if (ret != NULL) {
        // TODO: check_for_unresolved here?
        ret = resolve_polymorph(ret);
        if (contains_generic_struct(ret)) {
            ret = reify_struct(match->scope, ast, ret);
        }
    } else {
        ret = base_type(VOID_T);
    }
    // TODO: I think that this probably shouldn't modify resolved, it should be
    // maybe adding something to the scope instead?
    /*resolved->fn.ret = ret;*/

    match->ret = ret; // this is used in the body, must be done before check_block_semantics
    match->body = check_block_semantics(match->scope, match->body, 1);

    ast->var_type = ret;

    return ast;
}

static Ast *check_call_semantics(Scope *scope, Ast *ast) {
    ast->call->fn = check_semantics(scope, ast->call->fn);

    Type *orig = ast->call->fn->var_type;
    Type *resolved = resolve_alias(orig);
    if (resolved->comp != FUNC) {
        error(ast->line, ast->file, "Cannot perform call on non-function type '%s'", type_to_string(orig));
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
        Type *first_arg_type = m->method->decl->args[0]->type;
        if (is_polydef(first_arg_type)) {
            if (!match_polymorph(NULL, first_arg_type, recv->var_type)) {
                if (!(first_arg_type->comp == REF && match_polymorph(NULL, first_arg_type->ref.inner, recv->var_type))) {
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
            if (!(first_arg_type->comp == REF && check_type(recv->var_type, first_arg_type->ref.inner))) {
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

    if (is_polydef(resolved)) {
        return check_poly_call_semantics(scope, ast, resolved);
    }
    
    int given_count = verify_arg_count(scope, ast, resolved);

    // create variadic temp var if necessary
    if (resolved->fn.variadic && !ast->call->has_spread && array_len(resolved->fn.args) <= given_count) {
        for (int i = 0; i < array_len(resolved->fn.args); i++) {
            if (i == array_len(resolved->fn.args) - 1) {
                long length = array_len(ast->call->args) - (array_len(resolved->fn.args) - 1);
                Type *a = make_static_array_type(resolved->fn.args[i], length);
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
    verify_arg_types(scope, ast, resolved->fn.args, ast->call->args, resolved->fn.variadic);

    ast->var_type = resolve_polymorph(resolved->fn.ret);

    return ast;
}

static Ast *check_func_decl_semantics(Scope *scope, Ast *ast) {
    int poly = 0;
    Scope *type_check_scope = scope;

    if (is_polydef(ast->fn_decl->var->type)) {
        type_check_scope = new_scope(scope);
        poly = 1;

        // this is prior to the below loop in case the polydef is after another
        // argument using its type, as in fn (T, $T) -> T
        for (int i = 0; i < array_len(ast->fn_decl->args); i++) {
            if (is_polydef(ast->fn_decl->args[i]->type)) {
                if (i == array_len(ast->fn_decl->args) - 1 && ast->fn_decl->var->type->fn.variadic) {
                    // variadic is polydef
                    error(ast->line, ast->file, "Variadic function argument cannot define a polymorphic type.");
                }
                // get alias
                define_polydef_alias(type_check_scope, ast->fn_decl->args[i]->type);
            }
        }
    }

    for (int i = 0; i < array_len(ast->fn_decl->args); i++) {
        Var *arg = ast->fn_decl->args[i];
        check_for_unresolved_with_scope(ast, arg->type, type_check_scope);

        if (!is_concrete(arg->type)) {
            error(ast->line, ast->file, "Argument '%s' has generic type '%s' (not allowed currently).",
                  arg->name, type_to_string(arg->type));
        }

        if (arg->use) {
            // allow polydef here?
            Type *orig = arg->type;
            Type *t = resolve_alias(orig);
            while (t->comp == REF) {
                t = resolve_alias(t->ref.inner);
            }
            if (t->comp != STRUCT) {
                error(ast->line, ast->file,
                    "'use' is not allowed on args of non-struct type '%s'.",
                    type_to_string(orig));
            }
            Ast *lhs = make_ast_id(arg, arg->name);
            lhs->line = ast->line;
            lhs->file = ast->file;

            for (int i = 0; i < array_len(t->st.member_names); i++) {
                char *name = t->st.member_names[i];
                if (lookup_local_var(ast->fn_decl->scope, name) != NULL) {
                    error(ast->line, ast->file,
                        "'use' statement on struct type '%s' conflicts with existing argument named '%s'.",
                        type_to_string(orig), name);
                } else if (local_type_name_conflict(scope, name)) {
                    error(ast->line, ast->file,
                        "'use' statement on struct type '%s' conflicts with builtin type named '%s'.",
                        type_to_string(orig), name);
                }

                Var *v = make_var(name, t->st.member_types[i]);
                Ast *dot = make_ast_dot_op(lhs, name);
                dot->line = ast->line;
                dot->file = ast->file;
                v->proxy = check_semantics(ast->fn_decl->scope, first_pass(ast->fn_decl->scope, dot));
                array_push(ast->fn_decl->scope->vars, v);
            }
        }
    }

    if (!poly) {
        ast->fn_decl->body = check_block_semantics(ast->fn_decl->scope, ast->fn_decl->body, 1);
    }

    Scope *tmp = ast->fn_decl->var->type->fn.ret->scope;
    ast->fn_decl->var->type->fn.ret->scope = type_check_scope;

    if (resolve_alias(ast->fn_decl->var->type->fn.ret) == NULL) {
        error(ast->line, ast->file, "Unknown type '%s' in declaration of function '%s'", type_to_string(ast->fn_decl->var->type->fn.ret), ast->fn_decl->var->name);
    }

    ast->fn_decl->var->type->fn.ret->scope = tmp;

    if (ast->type == AST_ANON_FUNC_DECL) {
        ast->var_type = ast->fn_decl->var->type;
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
    } else if (obj_type->comp == STATIC_ARRAY) {
        size = obj_type->array.length;
        ast->var_type = obj_type->array.inner;
    } else if (obj_type->comp == ARRAY) {
        ast->var_type = obj_type->array.inner;
    } else {
        error(ast->index->object->line, ast->index->object->file,
            "Cannot perform index/subscript operation on non-array type (type is '%s').",
            type_to_string(obj_type));
    }

    Type *resolved = resolve_alias(ind_type);
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
            check_for_unresolved(ast, ast->lit->compound_val.type);
            ast = check_struct_literal_semantics(scope, ast);
            register_type(ast->var_type);
            break;
        case ARRAY_LIT:
        case COMPOUND_LIT:
            check_for_unresolved(ast, ast->lit->compound_val.type);
            ast = check_compound_literal_semantics(scope, ast);
            register_type(ast->var_type);
            break;
        case ENUM_LIT: // eh?
            break;
        }
        break;
    }
    case AST_IDENTIFIER: {
        Var *v = lookup_var(scope, ast->ident->varname);
        if (v == NULL) {
            error(ast->line, ast->file, "Undefined identifier '%s' encountered.", ast->ident->varname);
            // TODO better error for an enum here
        }

        if (v->proxy) { // USE-proxy
            Type *resolved = resolve_alias(v->type);
            if (resolved->comp == ENUM) { // proxy for enum
                for (int i = 0; i < array_len(resolved->en.member_names); i++) {
                    if (!strcmp(resolved->en.member_names[i], v->name)) {
                        Ast *a = ast_alloc(AST_LITERAL);
                        a->lit->lit_type = ENUM_LIT;
                        a->lit->enum_val.enum_index = i;
                        a->lit->enum_val.enum_type = resolved;
                        a->var_type = v->type;
                        return a;
                    }
                }
                error(-1, "<internal>", "How'd this happen");
            } else { // proxy for normal var
                // TODO: this may not be needed again, semantics already
                // checked?
                return check_semantics(scope, v->proxy);
            }
        }
        ast->ident->var = v;
        ast->var_type = ast->ident->var->type;
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
        // TODO: TODO: TODO: TODO: make sure the order is reversed in codegen!!!
        break;
    case AST_NEW: {
        ast->var_type = ast->new->type;

        Type *res = resolve_alias(ast->var_type);
        if (res == NULL) {
            error(ast->line, ast->file, "Unknown type for new, '%s'", type_to_string(ast->var_type));
        }
        check_for_unresolved(ast, res);

        if (res->comp == ARRAY) {
            // could be a named array type?
            if (ast->new->count == NULL) {
                error(ast->line, ast->file, "Cannot call 'new' on array type '%s' without specifying a count.", type_to_string(ast->var_type));
            }

            ast->new->count = check_semantics(scope, ast->new->count);
            Type *r = resolve_alias(ast->new->count->var_type);
            if (r->comp != BASIC || !(r->data->base == INT_T || r->data->base == UINT_T)) {
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
            assert(res->comp == REF);

            Type *inner = resolve_alias(res->ref.inner);

            if (inner->comp != STRUCT) {
                error(ast->line, ast->file, "Cannot use 'new' on a type that is not a struct or array.");
            }
        }
        return ast;
    }
    case AST_CAST: {
        AstCast *cast = ast->cast;
        cast->object = check_semantics(scope, cast->object);
        if (resolve_alias(cast->cast_type) == NULL) {
            error(ast->line, ast->file, "Unknown cast result type '%s'", type_to_string(cast->cast_type));
        }


        if (check_type(cast->object->var_type, cast->cast_type)) {
            error(ast->line, ast->file, "Unnecessary cast, types are both '%s'.", type_to_string(cast->cast_type));
        } else {
            Ast *coerced = coerce_type(scope, cast->cast_type, cast->object, 1);
            if (coerced) {
                // coerce_type does the cast for us 
                cast = coerced->cast;
                ast = coerced;
            } else if (!can_cast(cast->object->var_type, cast->cast_type)) {
                error(ast->line, ast->file, "Cannot cast type '%s' to type '%s'.", type_to_string(cast->object->var_type), type_to_string(cast->cast_type));
            }
        }

        ast->var_type = cast->cast_type;
        break;
    }
    case AST_DECL:
        return check_declaration_semantics(scope, ast);
    case AST_TYPE_DECL: {
        if (lookup_local_var(scope, ast->type_decl->type_name) != NULL) {
            error(ast->line, ast->file, "Type name '%s' already exists as variable.", ast->type_decl->type_name);
        }
        // TODO consider instead just having an "unresolved types" list
        check_for_unresolved(ast, ast->type_decl->target_type);
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
        for (int i = 0; i < array_len(ast->fn_decl->args); i++) {
            if (!resolve_alias(ast->fn_decl->args[i]->type)) {
                error(ast->line, ast->file, "Unknown type '%s' in declaration of extern function '%s'", type_to_string(ast->fn_decl->args[i]->type), ast->fn_decl->var->name);
            }
        }
        if (!resolve_alias(ast->fn_decl->var->type->fn.ret)) {
            error(ast->line, ast->file, "Unknown type '%s' in declaration of extern function '%s'", type_to_string(ast->fn_decl->var->type->fn.ret), ast->fn_decl->var->name);
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
        check_for_unresolved(ast, ast->impl->type);

        if (contains_generic_struct(ast->impl->type)) {
            ast->impl->type = reify_struct(scope, ast, ast->impl->type);
        }
        for (int i = 0; i < array_len(ast->impl->methods); i++) {
            Ast *m = check_semantics(scope, ast->impl->methods[i]);
            Type *recv = m->fn_decl->args[0]->type;
            if (recv->comp == REF) {
                recv = recv->ref.inner;
            }
            if (!is_concrete(ast->impl->type) && recv->comp == PARAMS) {
                recv = recv->params.inner;
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
            check_for_unresolved(ast, lp->index->type);

            Type *t = resolve_alias(lp->index->type);
            assert(t != NULL);

            if (!(t->data->base == INT_T || t->data->base == UINT_T)) {
                error(ast->line, ast->file, "Index variable '%s' has type '%s', which is not an integer type.", lp->index->name, type_to_string(lp->index->type));

            }
            // TODO: check for overflow of index type on static array
            array_push(lp->scope->vars, lp->index);
        }

        Type *it_type = lp->iterable->var_type;
        Type *resolved = resolve_alias(it_type);

        if (resolved->comp == ARRAY || resolved->comp == STATIC_ARRAY) {
            lp->itervar->type = it_type->array.inner;
        } else if (resolved->comp == BASIC && resolved->data->base == STRING_T) {
            lp->itervar->type = base_numeric_type(UINT_T, 8);
        } else {
            error(ast->line, ast->file,
                "Cannot use for loop on non-iterable type '%s'.", type_to_string(it_type));
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

        Type *fn_ret_t = fn_scope->fn_var->type->fn.ret;
        Type *ret_t = base_type(VOID_T);

        if (fn_scope->polymorph != NULL) {
            fn_ret_t = fn_scope->polymorph->ret;
        }

        if (ast->ret->expr != NULL) {
            // TODO: this is wrong in test.vs, why? even after doing
            // resolve_polymorph, the type is still Array(T). Is the scope not
            // being set to the proper one (with the polymorph on it), so that
            // at this point there is nothing to resolve to?
            ast->ret->expr = check_semantics(scope, ast->ret->expr);
            ret_t = resolve_polymorph(ast->ret->expr->var_type);
        }

        if (ret_t->comp == STATIC_ARRAY) {
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
