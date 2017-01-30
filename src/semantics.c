#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "semantics.h"
#include "eval.h"
#include "parse.h"
#include "polymorph.h"
#include "types.h"
#include "typechecking.h"

static Ast *parse_uop_semantics(Scope *scope, Ast *ast) {
    ast->unary->object = parse_semantics(scope, ast->unary->object);
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
        ast->var_type = o->var_type->inner;
        break;
    case OP_REF:
        if (o->type != AST_IDENTIFIER && o->type != AST_DOT) {
            error(ast->line, ast->file, "Cannot take a reference to a non-variable.");
        }
        ast->var_type = make_ref_type(o->var_type);
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

static Ast *parse_dot_op_semantics(Scope *scope, Ast *ast) {
    if (ast->dot->object->type == AST_IDENTIFIER) {
        Type *t = make_type(scope, ast->dot->object->ident->varname);
        Type *resolved = resolve_alias(t);
        if (resolved != NULL) {
            if (resolved->comp == ENUM) {
                for (int i = 0; i < resolved->en.nmembers; i++) {
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
    } 
    ast->dot->object = parse_semantics(scope, ast->dot->object);

    Type *orig = ast->dot->object->var_type;
    Type *t = orig;

    if (t->comp == REF) {
        t = t->inner;
    }

    // TODO: how much resolving should happen here? does this allow for more
    // indirection than we wanted?
    t = resolve_alias(t);

    if (t->comp == ARRAY || t->comp == STATIC_ARRAY) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            ast->var_type = make_ref_type(t->comp == ARRAY ? t->inner : t->array.inner);
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
        for (int i = 0; i < t->st.nmembers; i++) {
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

static Ast *parse_assignment_semantics(Scope *scope, Ast *ast) {
    ast->binary->left = parse_semantics(scope, ast->binary->left);
    ast->binary->right = parse_semantics(scope, ast->binary->right);

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

    Type *lt = ast->binary->left->var_type;
    Type *rt = ast->binary->right->var_type;

    if (!(check_type(lt, rt) || can_coerce_type(scope, lt, ast->binary->right))) {
        error(ast->binary->left->line, ast->binary->left->file,
            "LHS of assignment has type '%s', while RHS has type '%s'.",
            type_to_string(lt), type_to_string(rt));
    }

    if (is_dynamic(ast->binary->left->var_type)) {
        allocate_ast_temp_var(scope, ast->binary->right);
    }
    ast->var_type = base_type(VOID_T);
    return ast;
}

static Ast *parse_binop_semantics(Scope *scope, Ast *ast) {
    ast->binary->left = parse_semantics(scope, ast->binary->left);
    ast->binary->right = parse_semantics(scope, ast->binary->right);

    Ast *l = ast->binary->left;
    Ast *r = ast->binary->right;

    Type *lt = l->var_type;
    Type *rt = r->var_type;

    switch (ast->binary->op) {
    case OP_PLUS: {
        int numeric = is_numeric(lt) && is_numeric(rt);
        int strings = is_string(lt) && is_string(rt);
        if (!numeric && !strings) {
            error(ast->line, ast->file, "Operator '%s' is valid only for numeric or string arguments, not for type '%s'.",
                    op_to_str(ast->binary->op), type_to_string(lt));
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
        if (!(check_type(lt, rt) || can_coerce_type(scope, lt, ast->binary->right))) {
            error(ast->line, ast->file, "Cannot compare equality of non-comparable types '%s' and '%s'.",
                    type_to_string(lt), type_to_string(rt));
        }
        break;
    }

    if (l->type == AST_LITERAL && r->type == AST_LITERAL) {
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

static Ast *parse_enum_decl_semantics(Scope *scope, Ast *ast) {
    Type *et = ast->enum_decl->enum_type; 
    et = resolve_alias(et);
    assert(et->comp == ENUM);
    Ast **exprs = ast->enum_decl->exprs;

    long val = 0;
    for (int i = 0; i < et->en.nmembers; i++) {
        if (exprs[i] != NULL) {
            exprs[i] = parse_semantics(scope, exprs[i]);
            Type *t = resolve_alias(exprs[i]->var_type);
            // TODO allow const other stuff in here
            if (exprs[i]->type != AST_LITERAL) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-constant expression.", et->name, et->en.member_names[i]);
            } else if (t->data->base != INT_T) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-integer expression.", et->name, et->en.member_names[i]);
            }
            Type *e = exprs[i]->var_type;
            if (!(check_type(et->en.inner, e) || can_coerce_type(scope, et->en.inner, exprs[i]))) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with expression of type '%s' (base is '%s').", et->name, et->en.member_names[i], type_to_string(e), type_to_string(et->en.inner));
            }
            val = exprs[i]->lit->int_val;
        }
        et->en.member_values[i] = val;
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
    switch (t->comp) {
    /*case POLY:*/
    case POLYDEF:
    case ALIAS:
        t->scope = scope;
        break;
    case FUNC:
        for (TypeList *list = t->fn.args; list != NULL; list = list->next) {
            first_pass_type(scope, list->item);
        }
        first_pass_type(scope, t->fn.ret);
        break;
    case STRUCT:
        for (int i = 0; i < t->st.nmembers; i++) {
            first_pass_type(scope, t->st.member_types[i]);
        }
        register_type(scope, t);
        break;
    case ENUM:
        first_pass_type(scope, t->en.inner);
        register_type(scope, t);
        break;
    case REF:
    case ARRAY:
        first_pass_type(scope, t->inner);
        break;
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
        if (ast->lit->lit_type == STRUCT_LIT) {
            first_pass_type(scope, ast->lit->struct_val.type);
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

        if (!ast->fn_decl->anon) {
            if (lookup_local_var(scope, ast->fn_decl->var->name) != NULL) {
                error(ast->line, ast->file, "Declared function name '%s' already exists in this scope.", ast->fn_decl->var->name);
            }

            attach_var(scope, ast->fn_decl->var);
            attach_var(ast->fn_decl->scope, ast->fn_decl->var);
        }

        first_pass_type(ast->fn_decl->scope, ast->fn_decl->var->type);

        // TODO handle case where arg has same name as func
        if (!is_polydef(ast->fn_decl->var->type)) {
            for (VarList *args = ast->fn_decl->args; args != NULL; args = args->next) {
                attach_var(ast->fn_decl->scope, args->item);
            }
        }
        break;
    case AST_ENUM_DECL:
        if (local_type_name_conflict(scope, ast->enum_decl->enum_name)) {
            error(ast->line, ast->file, "Type named '%s' already exists in local scope.", ast->enum_decl->enum_name);
        }
        // TODO: need to do the epxrs here or not?
        first_pass_type(scope, ast->enum_decl->enum_type);
        ast->enum_decl->enum_type = define_type(scope, ast->enum_decl->enum_name, ast->enum_decl->enum_type);
        break;
    case AST_TYPE_DECL:
        ast->type_decl->target_type->scope = scope;
        first_pass_type(scope, ast->type_decl->target_type);
        ast->type_decl->target_type = define_type(scope, ast->type_decl->type_name, ast->type_decl->target_type);
        register_type(scope, ast->type_decl->target_type);
        break;
    case AST_DOT:
        ast->dot->object = first_pass(scope, ast->dot->object);
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
        for (AstList *args = ast->call->args; args != NULL; args = args->next) {
            args->item = first_pass(scope, args->item);
        }
        if (ast->call->variadic_tempvar != NULL) {
            first_pass_type(scope, ast->call->variadic_tempvar->type);
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
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_IDENTIFIER:
    case AST_LOOKUP:
        break;
    }
    return ast;
}

AstBlock *parse_block_semantics(Scope *scope, AstBlock *block, int fn_body) {
    for (AstList *list = block->statements; list != NULL; list = list->next) {
        list->item = first_pass(scope, list->item);
    }

    Ast *last = NULL;
    int mainline_return_reached = 0;
    for (AstList *list = block->statements; list != NULL; list = list->next) {
        if (last != NULL && last->type == AST_RETURN) {
            error(list->item->line, list->item->file, "Unreachable statements following return.");
        }
        list->item = parse_semantics(scope, list->item);
        if (needs_temp_var(list->item)) {
            allocate_ast_temp_var(scope, list->item);
        }
        if (list->item->type == AST_RETURN) {
            mainline_return_reached = 1;
        }
        last = list->item;
    }

    // if it's a function that needs return and return wasn't reached, break
    if (!mainline_return_reached && fn_body) {
        Type *rt = fn_scope_return_type(scope);
        if (rt->data->base != VOID_T) {
            error(block->endline, block->file,
                "Control reaches end of function '%s' without a return statement.",
                closest_fn_scope(scope)->fn_var->name);
            // TODO: anonymous function ok here?
        }
    }
    return block;
}

static Ast *parse_directive_semantics(Scope *scope, Ast *ast) {
    // TODO does this checking need to happen in the first pass? may be a
    // reason to do this later!
    char *n = ast->directive->name;
    if (!strcmp(n, "typeof")) {
        ast->directive->object = parse_semantics(scope, ast->directive->object);
        // TODO should this only be acceptible on identifiers? That would be
        // more consistent (don't have to explain that #type does NOT evaluate
        // arguments -- but is that too restricted / unnecessary?
        Ast *t = ast_alloc(AST_TYPEINFO);
        t->line = ast->line;
        t->typeinfo->typeinfo_target = ast->directive->object->var_type;
        t->var_type = typeinfo_ref();
        /*register_type(scope, t->var_type);*/
        /*errlog("registering: %s", t->var_type);*/
        /*register_type(scope, ast->var_type);*/
        return t;
    } else if (!strcmp(n, "type")) {
        Ast *t = ast_alloc(AST_TYPEINFO);
        t->line = ast->line;
        t->typeinfo->typeinfo_target = ast->var_type;
        t->var_type = typeinfo_ref();
        /*register_type(scope, t->var_type);*/
        /*errlog("registering: %s", type_to_string(ast->var_type));*/
        /*register_type(scope, ast->var_type);*/
        return t;
    }

    error(ast->line, ast->file, "Unrecognized directive '%s'.", n);
    return NULL;
}

static Ast *parse_struct_literal_semantics(Scope *scope, Ast *ast) {
    AstLiteral *lit = ast->lit;
    Type *type = lit->struct_val.type;

    Type *resolved = resolve_alias(type);
    if (resolved->comp != STRUCT) {
        error(ast->line, ast->file, "Type '%s' is not a struct.", lit->struct_val.name);
    }

    ast->var_type = type;

    for (int i = 0; i < lit->struct_val.nmembers; i++) {
        int found = 0;
        for (int j = 0; j < resolved->st.nmembers; j++) {
            if (!strcmp(lit->struct_val.member_names[i], resolved->st.member_names[j])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            // TODO show alias in error
            error(ast->line, ast->file, "Struct '%s' has no member named '%s'.",
                    type_to_string(type), lit->struct_val.member_names[i]);
        }

        Ast *expr = parse_semantics(scope, lit->struct_val.member_exprs[i]);
        lit->struct_val.member_exprs[i] = expr;
        Type *t = resolved->st.member_types[i];
        if (!check_type(t, expr->var_type) && !can_coerce_type(scope, t, expr)) {
            error(ast->line, ast->file, "Type mismatch in struct literal '%s' field '%s', expected %s but got %s.",
                type_to_string(type), lit->struct_val.member_names[i], type_to_string(t), type_to_string(expr->var_type));
        }
    }
    return ast;
}

static Ast *parse_use_semantics(Scope *scope, Ast *ast) {
    // TODO: use on package?
    if (ast->use->object->type == AST_IDENTIFIER) {
        Type *t = make_type(scope, ast->dot->object->ident->varname);
        Type *resolved = resolve_alias(t);
        if (resolved != NULL) {

            if (resolved->comp != ENUM) {
                // TODO: show alias
                error(ast->use->object->line, ast->file,
                        "'use' is not valid on non-enum type '%s'.", type_to_string(t));
            }

            for (int i = 0; i < resolved->en.nmembers; i++) {
                char *name = resolved->en.member_names[i];

                if (lookup_local_var(scope, name) != NULL) {
                    error(ast->use->object->line, ast->file, "'use' statement on enum '%s' conflicts with local variable named '%s'.", type_to_string(t), name);
                }

                if (find_builtin_var(name) != NULL) {
                    error(ast->use->object->line, ast->file, "'use' statement on enum '%s' conflicts with builtin variable named '%s'.", type_to_string(t), name);
                }

                Var *v = make_var(name, t);
                v->constant = 1;
                v->proxy = v; // TODO don't do this!
                attach_var(scope, v);
            }
            return ast;
        }
    }

    ast->use->object = parse_semantics(scope, ast->use->object);

    Type *orig = ast->use->object->var_type;
    Type *t = orig;

    if (t->comp == REF) {
        t = t->inner;
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

    for (int i = 0; i < resolved->st.nmembers; i++) {
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

        Var *v = make_var(member_name, resolved->st.member_types[i]); // should this be 't' or 't->st.member_types[i]'?
        char *proxy_name;
        int proxy_name_len;
        // TODO: names shouldn't do this, this should be done in codegen
        if (orig->comp == REF) {
            proxy_name_len = strlen(name) + strlen(member_name) + 2 + 1;
            proxy_name = malloc(sizeof(char) * proxy_name_len);
            snprintf(proxy_name, proxy_name_len, "%s->%s", name, member_name);
        } else {
            proxy_name_len = strlen(name) + strlen(member_name) + 1 + 1;
            proxy_name = malloc(sizeof(char) * proxy_name_len);
            snprintf(proxy_name, proxy_name_len, "%s.%s", name, member_name);
        }
        Var *proxy = make_var(proxy_name, resolved->st.member_types[i]);
        v->proxy = proxy;
        attach_var(scope, v);
    }
    return ast;
}

static Ast *parse_slice_semantics(Scope *scope, Ast *ast) {
    AstSlice *slice = ast->slice;

    slice->object = parse_semantics(scope, slice->object);
    if (needs_temp_var(slice->object)) {
        allocate_ast_temp_var(scope, slice->object);
    }

    Type *a = slice->object->var_type;
    Type *resolved = resolve_alias(a);

    if (resolved->comp == ARRAY) {
        ast->var_type = make_array_type(resolved->inner);
    } else if (resolved->comp == STATIC_ARRAY) {
        ast->var_type = make_array_type(resolved->array.inner);
    } else {
        error(ast->line, ast->file, "Cannot slice non-array type '%s'.", type_to_string(a));
    }

    if (slice->offset != NULL) {
        slice->offset = parse_semantics(scope, slice->offset);

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
        slice->length = parse_semantics(scope, slice->length);

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

static Ast *parse_declaration_semantics(Scope *scope, Ast *ast) {
    AstDecl *decl = ast->decl;
    Ast *init = decl->init;

    Type *t = resolve_alias(decl->var->type);

    if (init == NULL) {
        if (t == NULL) {
            error(ast->line, ast->file, "Unknown type '%s' in declaration of variable '%s'", type_to_string(decl->var->type), decl->var->name);
        } else if (decl->var->type->comp == STATIC_ARRAY && decl->var->type->array.length == -1) {
            error(ast->line, ast->file, "Cannot use unspecified array type for variable '%s' without initialization.", decl->var->name);
        }
    } else {
        decl->init = parse_semantics(scope, init);
        init = decl->init;

        if (decl->var->type == NULL) {
            decl->var->type = init->var_type;
            if (resolve_alias(decl->var->type)->comp == STRUCT) {
                init_struct_var(decl->var); // need to do this anywhere else?
            }
        } else {
            Type *lt = decl->var->type;
            Type *resolved = resolve_alias(lt);

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

            // TODO: type-checking
            if (!(check_type(lt, init->var_type) || can_coerce_type(scope, lt, init))) {
                error(ast->line, ast->file, "Cannot assign value '%s' to type '%s'.",
                        type_to_string(init->var_type), type_to_string(lt));
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
        define_global(decl->var);
        ast->decl->global = 1;

        if (decl->init != NULL) {
            Ast *id = ast_alloc(AST_IDENTIFIER);
            id->line = ast->line;
            id->file = ast->file;
            id->ident->varname = decl->var->name;
            id->ident->var = decl->var;

            id = parse_semantics(scope, id);

            ast = make_ast_assign(id, decl->init);
            ast->line = id->line;
            ast->file = id->file;
            return ast;
        }
    } else {
        attach_var(scope, ast->decl->var); // should this be after the init parsing?
    }

    return ast;
}

static void verify_arg_types(Scope *scope, Ast *call, TypeList *expected_types, AstList *arg_vals, int variadic) {
    for (int i = 0; arg_vals != NULL; i++) {
        Ast *arg = arg_vals->item;
        Type *expected = expected_types->item;

        if (!(check_type(arg->var_type, expected) || can_coerce_type(scope, expected, arg))) {
            error(call->line, call->file, "Expected argument (%d) of type '%s', but got type '%s'.",
                i, type_to_string(expected), type_to_string(arg->var_type));
        }

        if (is_any(expected)) {
            if (!is_any(arg->var_type) && !is_lvalue(arg)) {
                if (needs_temp_var(arg)) {
                    allocate_ast_temp_var(scope, arg);
                }
            }
        }

        arg_vals = arg_vals->next;
        if (!(variadic && expected_types->next == NULL)) {
            expected_types = expected_types->next;
        }
    }
}

static Ast *parse_poly_call_semantics(Scope *scope, Ast *ast, Type *resolved) {
    AstList *call_args = ast->call->args;
    TypeList *call_arg_types = NULL;

    // collect call arg types
    for (int i = 0; call_args != NULL; i++) {
        Ast *arg = parse_semantics(scope, call_args->item);
        call_args->item = arg;
        call_args = call_args->next;
        Type *t = arg->var_type;
        // TODO: this is super hacky / or is it?
        if (t->comp == STATIC_ARRAY) {
            t = make_array_type(t->array.inner);
        }
        call_arg_types = typelist_append(call_arg_types, t);
    }
    call_arg_types = reverse_typelist(call_arg_types);

    AstFnDecl *decl = NULL;
    if (ast->call->fn->type == AST_IDENTIFIER) {
        decl = ast->call->fn->ident->var->fn_decl;
    } else if (ast->call->fn->type == AST_ANON_FUNC_DECL) {
        decl = ast->call->fn->fn_decl;
    } 
    if (decl == NULL) {
        error(ast->line, "internal", "Uh oh, polymorph func decl was null...");
    }
    
    if (resolved->fn.variadic) {
        if (ast->call->nargs < resolved->fn.nargs - 1) {
            error(ast->line, ast->file, "Expected at least %d arguments to variadic function, but only got %d.", resolved->fn.nargs-1, ast->call->nargs);
            return NULL;
        } else {
            TypeList *list = call_arg_types;
            Type *a = list->item;
            while (list != NULL) {
                a = list->item;
                list = list->next;
            }
            a = make_static_array_type(a, ast->call->nargs - (resolved->fn.nargs - 1));

            // TODO: this should do something else
            ast->call->variadic_tempvar = make_temp_var(scope, a, ast->id);
        }
    } else if (ast->call->nargs != resolved->fn.nargs) {
        error(ast->line, ast->file, "Incorrect argument count to function (expected %d, got %d)", resolved->fn.nargs, ast->call->nargs);
        return NULL;
    }

    Polymorph *match = check_for_existing_polymorph(decl, call_arg_types);
    if (match != NULL) {
        // made a match to existing polymorph
        ast->call->polymorph = match;
        ast->var_type = resolve_polymorph(resolved->fn.ret);
        return ast;
    } else {
        match = create_polymorph(decl, call_arg_types);
    }

    decl->scope->polymorph = match;
    ast->call->polymorph = match;
    
    TypeList *defined_arg_types = NULL;

    call_args = ast->call->args;
    VarList *arg_vars = decl->args;
    for (TypeList *list = resolved->fn.args; list != NULL; list = list->next) {
        Type *arg_type = call_args->item->var_type;
        if (is_polydef(list->item)) {
            if (!match_polymorph(decl->scope, list->item, arg_type)) {
                error(ast->line, ast->file, "Expected polymorphic argument type %s, but got an argument of type %s",
                        type_to_string(list->item), type_to_string(arg_type));
            }
            // TODO: make this better
            if (arg_type->comp == STATIC_ARRAY) {
                Type *t = make_array_type(arg_type->array.inner);
                defined_arg_types = typelist_append(defined_arg_types, t);
            } else {
                defined_arg_types = typelist_append(defined_arg_types, arg_type);
            }
        } else {
            defined_arg_types = typelist_append(defined_arg_types, list->item);
        }
        call_args = call_args->next;

        Var *v = copy_var(match->scope, arg_vars->item);
        v->type = defined_arg_types->item;
        if (resolved->fn.variadic && list->next == NULL) {
            v->type = make_array_type(v->type);
        }
        attach_var(match->scope, v);
        arg_vars = arg_vars->next;
    }
    defined_arg_types = reverse_typelist(defined_arg_types);
    call_args = ast->call->args;

    // we still need this, not every argument is a polydef!!
    verify_arg_types(scope, ast, defined_arg_types, call_args, resolved->fn.variadic);

    match->body = parse_block_semantics(match->scope, match->body, 1);
    ast->var_type = resolve_polymorph(resolved->fn.ret);

    return ast;
}

static Ast *parse_call_semantics(Scope *scope, Ast *ast) {
    ast->call->fn = parse_semantics(scope, ast->call->fn);

    Type *orig = ast->call->fn->var_type;
    Type *resolved = resolve_alias(orig);
    if (resolved->comp != FUNC) {
        error(ast->line, ast->file, "Cannot perform call on non-function type '%s'", type_to_string(orig));
        return NULL;
    }

    if (is_polydef(resolved)) {
        return parse_poly_call_semantics(scope, ast, resolved);
    }
    
    if (resolved->fn.variadic) {
        if (ast->call->nargs < resolved->fn.nargs - 1) {
            error(ast->line, ast->file, "Expected at least %d arguments to variadic function, but only got %d.", resolved->fn.nargs-1, ast->call->nargs);
            return NULL;
        } else {
            TypeList *list = resolved->fn.args;
            Type *a = list->item;
            while (list != NULL) {
                a = list->item;
                list = list->next;
            }
            a = make_static_array_type(a, ast->call->nargs - (resolved->fn.nargs - 1));

            // TODO: this should do something else
            ast->call->variadic_tempvar = make_temp_var(scope, a, ast->id);
        }
    } else if (ast->call->nargs != resolved->fn.nargs) {
        error(ast->line, ast->file, "Incorrect argument count to function (expected %d, got %d)", resolved->fn.nargs, ast->call->nargs);
        return NULL;
    }

    
    // if we make this a part of "verify_arg_types" and remove its use from the
    // polymorph version, there only has to be one loop over the call args
    for (AstList *args = ast->call->args; args != NULL; args = args->next) {
        args->item = parse_semantics(scope, args->item);
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

        for (VarList *args = ast->fn_decl->args; args != NULL; args = args->next) {
            if (is_polydef(args->item->type)) {
                // get alias
                define_polydef_alias(type_check_scope, args->item->type);
            }
        }
    }
    for (VarList *args = ast->fn_decl->args; args != NULL; args = args->next) {
        // TODO: this type-checking doesn't work with polydefs properly
        Scope *tmp = args->item->type->scope;
        args->item->type->scope = type_check_scope;

        if (resolve_alias(args->item->type) == NULL) {
            error(ast->line, ast->file, "Unknown type '%s' in declaration of function '%s'", type_to_string(args->item->type), ast->fn_decl->var->name);
        }

        args->item->type->scope = tmp;

        if (args->item->use) {
            // allow polydef here?
            int ref = 0;
            Type *orig = args->item->type;
            Type *t = resolve_alias(orig);
            while (t->comp == REF) {
                t = resolve_alias(t->inner);
                ref = 1;
            }
            if (t->comp != STRUCT) {
                error(lineno(), current_file_name(),
                    "'use' is not allowed on args of non-struct type '%s'.",
                    type_to_string(orig));
            }
            for (int i = 0; i < t->st.nmembers; i++) {
                char *name = t->st.member_names[i];
                if (lookup_local_var(ast->fn_decl->scope, name) != NULL) {
                    error(lineno(), current_file_name(),
                        "'use' statement on struct type '%s' conflicts with existing argument named '%s'.",
                        type_to_string(orig), name);
                } else if (local_type_name_conflict(scope, name)) {
                    error(lineno(), current_file_name(),
                        "'use' statement on struct type '%s' conflicts with builtin type named '%s'.",
                        type_to_string(orig), name);
                }

                Var *v = make_var(name, t->st.member_types[i]);
                if (ref) {
                    int proxy_name_len = strlen(args->item->name) + strlen(name) + 2;
                    char *proxy_name = malloc(sizeof(char) * (proxy_name_len + 1));
                    sprintf(proxy_name, "%s->%s", args->item->name, name);
                    proxy_name[proxy_name_len] = 0;
                    v->proxy = make_var(proxy_name, t->st.member_types[i]);
                } else {
                    int proxy_name_len = strlen(args->item->name) + strlen(name) + 1;
                    char *proxy_name = malloc(sizeof(char) * (proxy_name_len + 1));
                    sprintf(proxy_name, "%s.%s", args->item->name, name);
                    proxy_name[proxy_name_len] = 0;
                    v->proxy = make_var(proxy_name, t->st.member_types[i]);
                }
                attach_var(ast->fn_decl->scope, v);
            }
        }
    }

    if (!poly) {
        ast->fn_decl->body = parse_block_semantics(ast->fn_decl->scope, ast->fn_decl->body, 1);
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
static Ast *parse_index_semantics(Scope *scope, Ast *ast) {
    ast->index->object = parse_semantics(scope, ast->index->object);
    ast->index->index = parse_semantics(scope, ast->index->index);
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
        ast->var_type = obj_type->array.inner; // need to call something different?
    } else if (obj_type->comp == ARRAY) {
        ast->var_type = obj_type->inner; // need to call something different?
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

Ast *parse_semantics(Scope *scope, Ast *ast) {
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
            ast = parse_struct_literal_semantics(scope, ast);
            break;
        case ENUM_LIT: // eh?
            break;
        }
        break;
    }
    // TODO: duplicate of AST_IDENTIFIER with minor changes
    case AST_LOOKUP: {
        Package *p = lookup_imported_package(scope, ast->lookup->left->ident->varname);
        if (p == NULL) {
            error(ast->line, ast->file, "Unknown package '%s'", ast->lookup->left->ident->varname);
        }

        Var *v = lookup_var(p->scope, ast->lookup->right);
        if (v == NULL) {
            error(ast->line, ast->file, "No declared identifier '%s' in package '%s'.", ast->lookup->right, p->name);
            // TODO better error for an enum here
        }

        if (v->proxy != NULL) { // USE-proxy
            Type *resolved = resolve_alias(v->type);
            if (resolved->comp == ENUM) { // proxy for enum
                for (int i = 0; i < resolved->en.nmembers; i++) {
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
                int i = 0;
                assert(v->proxy != v);
                while (v->proxy) {
                    assert(i++ < 666); // lol
                    v = v->proxy; // this better not loop!
                }
            }
        }
        ast = ast_alloc(AST_IDENTIFIER);
        ast->ident->var = v;
        ast->var_type = v->type;
        break;
    }
    case AST_IDENTIFIER: {
        Var *v = lookup_var(scope, ast->ident->varname);
        if (v == NULL) {
            error(ast->line, ast->file, "Undefined identifier '%s' encountered.", ast->ident->varname);
            // TODO better error for an enum here
        }

        if (v->proxy != NULL) { // USE-proxy
            Type *resolved = resolve_alias(v->type);
            if (resolved->comp == ENUM) { // proxy for enum
                for (int i = 0; i < resolved->en.nmembers; i++) {
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
                int i = 0;
                assert(v->proxy != v);
                while (v->proxy) {
                    assert(i++ < 666); // lol
                    v = v->proxy; // this better not loop!
                }
            }
        }
        ast->ident->var = v;
        ast->var_type = ast->ident->var->type;
        break;
    }
    case AST_DOT:
        return parse_dot_op_semantics(scope, ast);
    case AST_ASSIGN:
        return parse_assignment_semantics(scope, ast);
    case AST_BINOP:
        return parse_binop_semantics(scope, ast);
    case AST_UOP:
        return parse_uop_semantics(scope, ast);
    case AST_USE:
        return parse_use_semantics(scope, ast);
    case AST_SLICE:
        return parse_slice_semantics(scope, ast);
    case AST_CAST: {
        AstCast *cast = ast->cast;
        cast->object = parse_semantics(scope, cast->object);
        if (resolve_alias(cast->cast_type) == NULL) {
            error(ast->line, ast->file, "Unknown cast result type '%s'", type_to_string(cast->cast_type));
        }

        // TODO: might need to check for needing tempvar in codegen
        if (!(can_coerce_type(scope, cast->cast_type, cast->object) || can_cast(cast->object->var_type, cast->cast_type))) {
            error(ast->line, ast->file, "Cannot cast type '%s' to type '%s'.", type_to_string(cast->object->var_type), type_to_string(cast->cast_type));
        }

        if (is_any(cast->cast_type) && !is_any(cast->object->var_type)) {
            if (!is_lvalue(cast->object)) {
                allocate_ast_temp_var(scope, cast->object);
            }
        }

        ast->var_type = cast->cast_type;
        break;
    }
    case AST_DECL:
        return parse_declaration_semantics(scope, ast);
    case AST_TYPE_DECL: {
        if (lookup_local_var(scope, ast->type_decl->type_name) != NULL) {
            error(ast->line, ast->file, "Type name '%s' already exists as variable.", ast->type_decl->type_name);
        }
        // TODO: error about unspecified length
        break;
    }
    case AST_EXTERN_FUNC_DECL:
        // TODO: fix this
        if (scope->parent != NULL) {
            error(ast->line, ast->file, "Cannot declare an extern inside scope ('%s').", ast->fn_decl->var->name);
        }
        for (VarList *args = ast->fn_decl->args; args != NULL; args = args->next) {
            if (resolve_alias(args->item->type) == NULL) {
                error(ast->line, ast->file, "Unknown type '%s' in declaration of extern function '%s'", type_to_string(args->item->type), ast->fn_decl->var->name);
            }
        }
        if (resolve_alias(ast->fn_decl->var->type->fn.ret) == NULL) {
            error(ast->line, ast->file, "Unknown type '%s' in declaration of extern function '%s'", type_to_string(ast->fn_decl->var->type->fn.ret), ast->fn_decl->var->name);
        }

        attach_var(scope, ast->fn_decl->var);
        define_global(ast->fn_decl->var);
        break;
    case AST_ANON_FUNC_DECL:
    case AST_FUNC_DECL:
        return check_func_decl_semantics(scope, ast);
    case AST_CALL:
        return parse_call_semantics(scope, ast);
    case AST_INDEX: 
        return parse_index_semantics(scope, ast);
    case AST_CONDITIONAL: {
        AstConditional *c = ast->cond;
        c->condition = parse_semantics(scope, c->condition);

        // TODO: I don't think typedefs of bool should be allowed here...
        if (!is_bool(c->condition->var_type)) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for if statement.",
                type_to_string(c->condition->var_type));
        }
        
        /*if (c->condition->type == AST_LITERAL) {*/

        /*}*/
        c->if_body = parse_block_semantics(c->scope, c->if_body, 0);

        if (c->else_body != NULL) {
            c->else_body = parse_block_semantics(c->else_scope, c->else_body, 0);
        }

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_WHILE: {
        AstWhile *lp = ast->while_loop;
        lp->condition = parse_semantics(scope, lp->condition);

        if (!is_bool(lp->condition->var_type)) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for while loop.",
                    type_to_string(lp->condition->var_type));
        }

        lp->body = parse_block_semantics(lp->scope, lp->body, 0);

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_FOR: {
        AstFor *lp = ast->for_loop;
        lp->iterable = parse_semantics(scope, lp->iterable);

        Type *it_type = lp->iterable->var_type;
        Type *resolved = resolve_alias(it_type);

        if (resolved->comp == STATIC_ARRAY) {
            lp->itervar->type = it_type->array.inner;
        } else if (resolved->comp == ARRAY) {
            lp->itervar->type = it_type->inner;
        } else if (resolved->comp == BASIC && resolved->data->base == STRING_T) {
            lp->itervar->type = base_numeric_type(UINT_T, 8);
        } else {
            error(ast->line, ast->file,
                "Cannot use for loop on non-interable type '%s'.", type_to_string(it_type));
        }
        // TODO type check for when type of itervar is explicit
        attach_var(lp->scope, lp->itervar);
        lp->body = parse_block_semantics(lp->scope, lp->body, 0);

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_ANON_SCOPE:
        ast->anon_scope->body = parse_block_semantics(ast->anon_scope->scope, ast->anon_scope->body, 0);
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

        if (ast->ret->expr != NULL) {
            ast->ret->expr = parse_semantics(scope, ast->ret->expr);
            ret_t = ast->ret->expr->var_type;
        }

        if (ret_t->comp == STATIC_ARRAY) {
            error(ast->line, ast->file, "Cannot return a static array from a function.");
        }

        if (!(check_type(fn_ret_t, ret_t) || can_coerce_type(scope, fn_ret_t, ast->ret->expr))) {
            error(ast->line, ast->file,
                "Return statement type '%s' does not match enclosing function's return type '%s'.",
                type_to_string(ret_t), type_to_string(fn_ret_t));
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
        ast->block = parse_block_semantics(scope, ast->block, 0);
        break;
    case AST_DIRECTIVE:
        return parse_directive_semantics(scope, ast);
    case AST_ENUM_DECL:
        return parse_enum_decl_semantics(scope, ast);
    case AST_TYPEINFO:
        break;
    case AST_IMPORT:
        if (!ast->import->package->semantics_checked) {
            Package *p = ast->import->package;
            p->semantics_checked = 1;
            /*p->pkg_name = package_name(p->path);*/
            for (PkgFileList *list = p->files; list != NULL; list = list->next) {
                list->item->root = parse_semantics(p->scope, list->item->root);
            }
        }
        break;
    default:
        error(-1, "internal", "idk parse semantics %d", ast->type);
    }
    return ast;
}
