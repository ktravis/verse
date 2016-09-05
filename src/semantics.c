#include "semantics.h"

static VarList *global_vars = NULL;
static VarList *builtin_vars = NULL;

VarList *get_global_vars() {
    return global_vars;
}

void define_builtin(Var *v) {
    builtin_vars = varlist_append(builtin_vars, v);
}

int needs_temp_var(Ast *ast) {
    switch (ast->type) {
    case AST_BINOP:
    case AST_UOP:
    case AST_CALL:
    case AST_LITERAL:
        return is_dynamic(ast->var_type);
    // case AST_DIRECTIVE: // maybe later
    }
    return 0;
}

Ast *parse_uop_semantics(Scope *scope, Ast *ast) {
    ast->unary->object = parse_semantics(scope, ast->unary->object);
    Ast *o = ast->unary->object;
    switch (ast->unary->op) {
    case OP_NOT:
        if (o->var_type->id != base_type(BOOL_T)->id) {
            error(ast->line, ast->file, "Cannot perform logical negation on type '%s'.", o->var_type->name);
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

Ast *parse_dot_op_semantics(Scope *scope, Ast *ast) {
    if (ast->dot->object->type == AST_IDENTIFIER) {
        Type *t = lookup_type(scope, ast->dot->object->ident->varname);
        t = resolve_alias(t);
        if (t != NULL) {
            if (t->comp == ENUM) {
                for (int i = 0; i < t->_enum.nmembers; i++) {
                    if (!strcmp(t->_enum.member_names[i], ast->dot->member_name)) {
                        Ast *a = ast_alloc(AST_LITERAL);
                        a->lit->lit_type = ENUM;
                        a->lit->enum_val.enum_index = i;
                        a->lit->enum_val.enum_type = t;
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
    ast->dot->object = parse_semantics(ast->dot->object, scope);

    Type *orig = ast->dot->object->var_type;
    Type *t = orig;

    Type *inner = ref_inner_type(scope, t);
    if (inner != NULL) {
        t = inner;
    }

    if (is_array(t)) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            ast->var_type = make_ref_type(array_inner_type(scope, t));
        } else {
            error(ast->line, ast->file,
                    "Cannot dot access member '%s' on array (only length or data).", ast->dot->member_name);
        }
    } else if (is_type_base(scope, t, STRING_T)) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "bytes")) {
            ast->var_type = make_ref_type(base_numeric_type(UINT_T, 8));
        } else {
            error(ast->line, ast->file,
                    "Cannot dot access member '%s' on string (only length or bytes).", ast->dot->member_name);
        }
    } else if (is_struct(scope, t)) {
        StructType *st = get_struct_type(scope, t);

        for (int i = 0; i < st.nmembers; i++) {
            if (!strcmp(ast->dot->member_name, st.member_names[i])) {
                ast->var_type = st.member_types[i];
                return ast;
            }
        }
        error(ast->line, ast->file, "No member named '%s' in struct '%s'.",
                ast->dot->member_name, orig->name);
    } else {
        error(ast->line, ast->file,
                "Cannot use dot operator on non-struct type '%s'.", orig->name);
    }
    return ast;
}

Ast *parse_assignment_semantics(Scope *scope, Ast *ast) {
    ast->binary->left = parse_semantics(ast->binary->left, scope);
    ast->binary->right = parse_semantics(ast->binary->right, scope);

    if (!is_lvalue(ast->binary->left)) {
        error(ast->line, ast->file, "LHS of assignment is not an lvalue.");
    } else if (ast->binary->left->type == AST_INDEX &&
                is_type_base(scope, ast->binary->left->index->object->var_type, STRING_T)) {
        error(ast->line, ast->file, "Strings are immutable and cannot be subscripted for assignment.");
    }

    // TODO refactor to "is_constant"
    /*Var *v = get_ast_var(ast->binary->left);*/
    if (ast->binary->left->type == AST_IDENTIFIER && ast->binary->left->ident->var->constant) {
        error(ast->line, ast->file, "Cannot reassign constant '%s'.", ast->binary->left->ident->var->name);
    }

    Type *lt = ast->binary->left->var_type;
    Type *rt = ast->binary->right->var_type;

    if (!(check_type(lt, rt) || can_coerce_type(scope, ast->binary->left, ast->binary->right))) {
        error(ast->binary->left->line, ast->binary->left->file,
            "LHS of assignment has type '%s', while RHS has type '%s'.",
            type_to_string(lt), type_to_string(rt));
    }
    ast->var_type = base_type(VOID_T);
    return ast;
}

Ast *parse_binop_semantics(Scope *scope, Ast *ast) {
    ast->binary->left = parse_semantics(ast->binary->left, scope);
    ast->binary->right = parse_semantics(ast->binary->right, scope);

    Ast *l = ast->binary->left;
    Ast *r = ast->binary->right;

    Type *lt = l->var_type;
    Type *rt = r->var_type;

    switch (ast->binary->op) {
    case OP_PLUS:
        int numeric = is_numeric(scope, lt) && is_numeric(scope, rt);
        int strings = is_type_base(scope, lt, STRING_T) && is_type_base(scope, rt, STRING_T);
        if (!numeric && !strings) {{
            error(ast->line, ast->file, "Operator '%s' is valid only for numeric or string arguments, not for type '%s'.",
                    op_to_str(ast->binary->op), type_to_string(lt));
        }
        break;
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
        if (!is_numeric(scope, lt)) {
            error(ast->line, ast->file, "LHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->binary->op), type_to_string(lt));
        } else if (!is_numeric(scope, rt)) {
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
        if (!type_equality_comparable(lt, rt)) {
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
    } else if (ast->binary->op != OP_ASSIGN && is_numeric(scope, lt)) {
        ast->var_type = promote_number_type(lt, l->type == AST_LITERAL, rt, r->type == AST_LITERAL);
    } else {
        ast->var_type = lt;
    }
    return ast;
}

Ast *parse_enum_decl_semantics(Scope *scope, Ast *ast) {
    Type *et = ast->enum_decl->enum_type; 
    Ast **exprs = ast->enum_decl->exprs;

    long val = 0;
    for (int i = 0; i < et->en.nmembers; i++) {
        if (exprs[i] != NULL) {
            exprs[i] = parse_semantics(exprs[i], scope);
            // TODO allow const other stuff in here
            if (exprs[i]->type != AST_LITERAL) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-constant expression.", et->name, et->en.member_names[i]);
            } else if (exprs[i]->var_type->base != INT_T) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-integer expression.", et->name, et->en.member_names[i]);
            }
            exprs[i] = coerce_literal(exprs[i], et->en.inner);
            val = exprs[i]->lit->int_val;
        }
        et->en.member_values[i] = val;
        val += 1;
        for (int j = 0; j < i; j++) {
            if (!strcmp(et->en.member_names[i], et->en.member_names[j])) {
                error(ast->line, ast->file, "Enum '%s' member name '%s' defined twice.",
                        ast->enum_decl->enum_name, et->en.member_names[i]);
            }
            if (et->en.member_values[i] == et->en.member_values[j]) {
                error(ast->line, ast->file, "Enum '%s' contains duplicate values for members '%s' and '%s'.",
                    ast->enum_decl->enum_name, et->en.member_names[j],
                    et->en.member_names[i]);
            }
        }
    }
    ast->var_type = base_type(VOID_T);
    return ast;
}

AstBlock *parse_block_semantics(Scope *scope, AstBlock *block, int fn_body) {
    Ast *last = NULL;
    int mainline_return_reached = 0;
    for (AstList *list = block->statements; list != NULL; list = list->next) {
        if (last != NULL && last->type == AST_RETURN) {
            error(list->item->line, list->item->file, "Unreachable statements following return.");
        }
        list->item = parse_semantics(list->item, scope);
        if (needs_temp_var(list->item)) {
            allocate_temp_var(scope, list->item);
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
                closest_fn_scope(scope);->fn_var->var->name);
            // TODO: anonymous function ok here?
        }
    }
    return block;
}

Ast *parse_directive_semantics(Scope *scope, Ast *ast) {
    // TODO does this checking need to happen in the first pass? may be a
    // reason to do this later!
    char *n = ast->directive->name;
    if (!strcmp(n, "typeof")) {
        ast->directive->object = parse_semantics(ast->directive->object, scope);
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

Ast *parse_struct_literal_semantics(Scope *scope, Ast *ast) {
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

        Ast *expr = parse_semantics(lit->struct_val.member_exprs[i], scope);
        lit->struct_val.member_exprs[i] = expr;
        Type *t = type->st.member_types[i];
        if (!check_type(t, expr->var_type) && !can_coerce_type(scope, t, expr)) {
            error(ast->line, ast->file, "Type mismatch in struct literal '%s' field '%s', expected %s but got %s.",
                type_to_string(type), lit->struct_val.member_names[i], type_to_string(t), type_to_string(expr->var_type));
        }
    }
    return ast;
}

Ast *parse_use_semantics(Scope *scope, Ast *ast) {
    if (ast->use->object->type == AST_IDENTIFIER) {
        Type *t = lookup_type(scope, ast->dot->object->ident->varname);
        if (t != NULL) {
            Type *resolved = resolve_alias(t);

            if (resolved->comp != ENUM) {
                // TODO: show alias
                error(ast->use->object->line, ast->file,
                        "'use' is not valid on non-enum type '%s'.", type_to_string(t));
            }

            for (int i = 0; i < resolved->en.nmembers; i++) {
                char *name = resolved->en.member_names[i];

                if (find_local_var(scope, name) != NULL) {
                    error(ast->use->object->line, ast->file, "'use' statement on enum '%s' conflicts with local variable named '%s'.", type_to_string(t), name);
                }

                if (varlist_find(builtin_vars, name) != NULL) {
                    error(ast->use->object->line, ast->file, "'use' statement on enum '%s' conflicts with builtin type named '%s'.", type_to_string(t), name);
                }

                Var *v = make_var(name, t);
                v->constant = 1;
                v->proxy = v; // TODO don't do this!
                attach_var(v, scope);
            }
            return ast;
        }
    }

    ast->use->object = parse_semantics(ast->use->object, scope);

    Type *orig = ast->use->object->var_type;
    Type *t = orig;

    Type *inner = ref_inner_type(scope, t);
    if (inner != NULL) {
        t = inner;
    }

    Type *resolved = resolve_alias(t);
    if (resolved->comp != STRUCT) {
        error(ast->use->object->line, ast->file,
            "'use' is not valid on non-struct type '%s'.", type_to_string(orig));
    } else {
        Var *ast_var = get_ast_var_noerror(ast->use->object);
        if (ast_var == NULL) {
            error(ast->use->object->line, ast->use->object->file, "Can't 'use' a non-variable.");
        }

        for (int i = 0; i < resolved->st.nmembers; i++) {
            char *name = t->st.member_names[i];
            if (find_local_var(name, scope) != NULL) {
                error(ast->use->object->line, ast->file,
                    "'use' statement on struct type '%s' conflicts with local variable named '%s'.",
                    type_to_string(t), name);
            }
            if (varlist_find(builtin_vars, name) != NULL) {
                error(ast->use->object->line, ast->file,
                    "'use' statement on struct type '%s' conflicts with builtin type named '%s'.",
                    type_to_string(t), name);
            }

            Var *v = make_var(name, resolved->st.member_types[i]); // should this be 't' or 't->st.member_types[i]'?
            Var *proxy = ast_var->members[i];

            // TODO: names shouldn't do this, this should be done in codegen
            if (inner != NULL) {
                int proxy_name_len = strlen(ast_var->name) + strlen(name) + 2;
                char *proxy_name = malloc(sizeof(char) * (proxy_name_len + 1));
                snprintf(proxy_name, proxy_name_len, "%s->%s", ast_var->name, name);
                proxy_name[proxy_name_len] = 0;
                proxy = make_var(proxy_name, t->st.member_types[i]);
            }
            v->proxy = proxy;
            attach_var(v, scope);
        }
    }
    return ast;
}

Ast *parse_slice_semantics(Scope *scope, Ast *ast) {
    AstSlice *slice = ast->slice;

    slice->object = parse_semantics(slice->object, scope);
    if (needs_temp_var(slice->object)) {
        allocate_temp_var(scope, slice->object);
    }

    Type *a = slice->object->var_type;
    Type *resolved = resolve_alias(a);

    if (resolved->comp == ARRAY) {
        ast->var_type = make_array_type(resolved->inner);
    } else if (resolved->comp == STATIC_ARRAY_T) {
        ast->var_type = make_array_type(resolved->array.inner);
    } else {
        error(ast->line, ast->file, "Cannot slice non-array type '%s'.", type_to_string(a));
    }

    if (slice->offset != NULL) {
        slice->offset = parse_semantics(slice->offset, scope);

        if (slice->offset->type == AST_LITERAL && resolved->comp == STATIC_ARRAY_T) {
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
        slice->length = parse_semantics(slice->length, scope);

        if (slice->length->type == AST_LITERAL && resolved->comp == STATIC_ARRAY_T) {
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

Ast *parse_declaration_semantics(Scope *scope, Ast *ast) {
    AstDecl *decl = ast->decl;
    Ast *init = decl->init;

    if (init == NULL) {
        if (decl->var->type == NULL) {
            error(ast->line, ast->file, "Cannot use type 'auto' for variable '%s' without initialization.", decl->var->name);
        } else if (decl->var->type->comp == STATIC_ARRAY_T && decl->var->type->array.length == -1) {
            error(ast->line, ast->file, "Cannot use unspecified array type for variable '%s' without initialization.", decl->var->name);
    } else {
        init = parse_semantics(scope, init);

        if (decl->var->type == NULL) {
            decl->var->type = init->var_type;
        } else {
            Type *lt = decl->var->type;
            Type *resolved = resolve_alias(lt);

            // TODO: shouldn't this check RHS type first?
            // TODO: do we want to resolve here?
            if (resolved->comp == STATIC_ARRAY && resolved->array.length == -1) {
                resolved->array.length = resolve_alias(init->var_type)->array.length;
            }

            if (init->type == AST_LITERAL && is_numeric(scope, resolved)) {
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
            if (!(check_type(lt, init->var_type) || can_coerce_type(scope, lt, init->var_type))) {
                error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s'.",
                        type_to_string(lt), type_to_string(lt));
            }
            decl->init = init;
        }

    }

    if (find_local_var(scope, decl->var->name) != NULL) {
        error(ast->line, ast->file, "Declared variable '%s' already exists.", decl->var->name);
    }

    if (local_type_name_conflict(scope, decl->var->name)) {
        error(ast->line, ast->file, "Variable name '%s' already in use by type.", decl->var->name);
    }

    ast->var_type = base_type(VOID_T);

    if (scope->parent == NULL) {
        global_vars = varlist_append(global_vars, decl->var);
        ast->decl->global = 1;

        if (decl->init != NULL) {
            Ast *id = ast_alloc(AST_IDENTIFIER);
            id->line = ast->line;
            id->file = ast->file;
            id->ident->varname = decl->var->name;
            id->ident->var = decl->var;

            id = parse_semantics(id, scope);

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

Ast *parse_call_semantics(Scope *scope, Ast *ast) {
    // TODO don't allow auto explicitly?
    ast->call->fn = parse_semantics(ast->call->fn, scope);

    Type *orig = ast->call->fn->var_type;
    Type *resolved = resolve_alias(orig);
    if (resolved->comp != FUNC) {
        error(ast->line, ast->file, "Cannot perform call on non-function type '%s'", type_to_string(orig));
    }
    
    if (resolved->fn.variadic) {
        if (ast->call->nargs < resolved->fn.nargs - 1) {
            error(ast->line, ast->file, "Expected at least %d arguments to variadic function, but only got %d.", resolved->fn.nargs-1, ast->call->nargs);
        } else {
            TypeList *list = resolved->fn.args;
            Type *a = list->item;
            while (list != NULL) {
                a = list->item;
                list = list->next;
            }
            a = make_static_array_type(a, ast->call->nargs - (resolved->fn.nargs - 1));;
            ast->call->variadic_tempvar = allocate_temp_var(scope, a);
        }
    } else if (ast->call->nargs != resolved->fn.nargs) {
        error(ast->line, ast->file, "Incorrect argument count to function (expected %d, got %d)", resolved->fn.nargs, ast->call->nargs);
    }

    AstList *call_args = ast->call->args;
    TypeList *fn_arg_types = resolved->fn.args;

    for (int i = 0; call_args != NULL; i++) {
        Ast *arg = parse_semantics(scope, call_args->item);

        if (is_any(fn_arg_types->item)) {
            if (!is_any(arg->var_type) && !is_lvalue(arg)) {
                if (needs_temp_var(arg)) {
                    allocate_temp_var(scope, arg);
                }
            }
        }

        if (!(check_type(arg->var_type, fn_arg_types->item) || can_coerce_type(scope, fn_arg_types->item, arg))) {
            error(ast->line, ast->file, "Expected argument (%d) of type '%s', but got type '%s'.",
                    i, type_to_string(fn_arg_types->item), type_to_string(arg->var_type));
        }

        call_args->item = arg;
        call_args = call_args->next;

        if (!resolved->fn.variadic || i < resolved->fn.nargs - 1) {
            fn_arg_types = fn_arg_types->next;
        }
    }

    ast->var_type = resolved->fn.ret;
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
        case ENUM_LIT:
            break;
        }
        break;
    }
    case AST_IDENTIFIER: {
        Var *v = find_var(scope, ast->ident->varname);
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
                        // TODO: should this be the resolved type instead?
                        a->lit->enum_val.enum_type = v->type;
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

        if (!can_cast(cast->object->var_type, cast->cast_type)) {
            error(ast->line, ast->file, "Cannot cast type '%s' to type '%s'.", type_to_string(cast->object->var_type), type_to_string(cast->cast_type));
        }

        if (is_any(cast->cast_type) && !is_any(cast->object->var_type)) {
            if (!is_lvalue(cast->object)) {
                allocate_temp_var(scope, cast->object);
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
        break;
    }
    case AST_EXTERN_FUNC_DECL:
        // TODO: fix this
        if (scope->parent != NULL) {
            error(ast->line, ast->file, "Cannot declare an extern inside scope ('%s').", ast->fn_decl->var->name);
        }

        attach_var(scope, ast->fn_decl->var);
        break;
    case AST_ANON_FUNC_DECL: {
    case AST_FUNC_DECL:
        Type *fn_t = ast->fn_decl->var->type;

        // TODO: loop over args, check each for use
        if (use) {
            Type *orig = args->item->type;
            Type *t = orig;
            while (t->base == REF_T) {
                t = t->inner;
            }
            if (t->base != STRUCT_T) {
                error(lineno(), current_file_name(), "'use' is not allowed on args of non-struct type '%s'.", orig->name);
            }
            for (int i = 0; i < t->st.nmembers; i++) {
                char *name = t->st.member_names[i];
                if (lookup_local_var(fn_scope, name) != NULL) {
                    error(lineno(), current_file_name(), "'use' statement on struct type '%s' conflicts with existing argument named '%s'.", orig->name, name);
                } else if (local_type_name_conflict(name)) {
                    error(lineno(), current_file_name(), "'use' statement on struct type '%s' conflicts with builtin type named '%s'.", orig->name, name);
                }
                Var *v = make_var(name, t->st.member_types[i]);
                if (orig->base == PTR_T) {
                    int proxy_name_len = strlen(args->item->name) + strlen(name) + 2;
                    char *proxy_name = malloc(sizeof(char) * (proxy_name_len + 1));
                    sprintf(proxy_name, "%s->%s", args->item->name, name);
                    proxy_name[proxy_name_len] = 0;
                    v->proxy = make_var(proxy_name, t->st.member_types[i]);
                } else {
                    v->proxy = args->item->members[i];
                }
                attach_var(v, fn_scope);
            }
        }

        ast->fn_decl->body = parse_block_semantics(ast->fn_decl->scope, ast->fn_decl->body, 1);

        detach_var(ast->fn_decl->scope, ast->fn_decl->var);

        if (ast->type == AST_ANON_FUNC_DECL) {
            ast->var_type = ast->fn_decl->var->type;
        }
        break;
    }
    case AST_CALL:
        return parse_call_semantics(scope, ast);
    case AST_INDEX: {
        ast->index->object = parse_semantics(scope, ast->index->object);
        ast->index->index = parse_semantics(scope, ast->index->index);
        if (needs_temp_var(ast->index->object)) {
            allocate_temp_var(scope, ast->index->object);
        }
        if (needs_temp_var(ast->index->index)) {
            allocate_temp_var(scope, ast->index->index);
        }

        Type *obj_type = ast->index->object->var_type;
        Type *ind_type = ast->index->index->var_type;

        int size = -1;

        if (obj_type->comp == BASIC && obj_type->data->base == STRING_T) {
            ast->var_type = base_numeric_type(UINT_T, 8);
            if (ast->index->object->type == AST_LITERAL) {
                size = strlen(ast->index->object->lit->string_val);
            }
        } else if (obj_type->comp == ARRAY) {
            size = obj_type->array.size;
            ast->var_type = obj_type->array.inner; // need to call something different?
        } else if (obj_type->comp == SLICE) {
            ast->var_type = obj_type->inner; // need to call something different?
        } else {
            error(ast->index->object->line, ast->index->object->file,
                "Cannot perform index/subscript operation on non-array type (type is '%s').",
                type_to_string(obj_type));
        }

        if (ind_type->comp != BASIC ||
           (ind_type->data->base != INT_T &&
            ind_type->data->base != UINT_T)) {
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
        break;
    }
    case AST_CONDITIONAL: {
        AstConditional *c = ast->cond;
        c->condition = parse_semantics(scope, c->condition);

        // TODO: I don't think typedefs of bool should be allowed here...
        if (c->condition->var_type->id != base_type(BOOL_T)->id) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for if statement.",
                type_to_string(c->condition->var_type));
        }
        
        c->if_body = parse_block_semantics(c->scope, c->if_body, 0);

        if (c->else_body != NULL) {
            c->else_body = parse_block_semantics(c->scope, c->else_body, 0);
        }

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_WHILE: {
        AstWhile *lp = ast->while_loop;
        lp->condition = parse_semantics(scope, lp->condition);

        if (lp->condition->var_type->id != base_type(BOOL_T)->id) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for while loop.",
                    type_to_string(lp->condition->var_type));
        }

        lp->body = parse_block_semantics(lp->scope, lp->body, 0);

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_FOR: {
        // TODO: need to attach new scopes to AstFor etc
        AstFor *lp = ast->for_loop;
        lp->iterable = parse_semantics(scope, lp->iterable);

        Type *it_type = lp->iterable->var_type;
        Type *resolved = resolve_alias(it_type);

        if (resolved->comp == STATIC_ARRAY)
            lp->itervar->type = it_type->array.inner;
        } else if (resolved->comp == ARRAY) {
            lp->itervar->type = it_type->inner;
        } else if (resolved->comp == BASIC && resolved->type->data->base == STRING_T) {
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

        if (!check_type(fn_ret_t, ret_t) && !can_coerce_type(scope, fn_ret_t, ast->ret->expr)) {
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
    default:
        error(-1, "internal", "idk parse semantics %d", ast->type);
    }
    return ast;
}
