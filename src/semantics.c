#include "semantics.h"

static VarList *global_vars = NULL;
static VarList *global_fn_vars = NULL;
static VarList *global_fn_bindings = NULL;
static TypeList *global_hold_funcs = NULL;

static TypeList *builtin_types = NULL;
static VarList *builtin_vars = NULL;

static AstListList *binding_exprs = NULL;

static int parser_state = PARSE_MAIN; // TODO unnecessary w/ PUSH_FN_SCOPE?

static int loop_state = 0;
static int in_decl = 0;

static Ast *current_fn_scope = NULL;

void define_builtin(Var *v) {
    builtin_vars = varlist_append(builtin_vars, v);
}

Var *make_temp_var(Type *type, AstScope *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = "";
    var->type = type;
    var->id = new_var_id();
    var->temp = 1;
    var->consumed = 0;
    var->initialized = (type->base == FN_T);
    attach_var(var, scope);
    return var;
}

void attach_var(Var *var, AstScope *scope) {
    scope->locals = varlist_append(scope->locals, var);
}

void detach_var(Var *var, AstScope *scope) {
    scope->locals = varlist_remove(scope->locals, var->name);
}

void release_var(Var *var, AstScope *scope) {
    scope->locals = varlist_remove(scope->locals, var->name);
}

Var *find_var(char *name, AstScope *scope) {
    Var *v = find_local_var(name, scope);
    if (v == NULL) {
        if (scope->parent != NULL) {
            v = find_var(name, scope->parent);
        }
        if (v != NULL && (!scope->is_function || v->constant)) {
            return v;
        }
        v = NULL;
    }
    if (v == NULL) {
        v = varlist_find(global_vars, name);
    }
    if (v == NULL) {
        v = varlist_find(global_fn_vars, name);
    }
    if (v == NULL) {
        v = varlist_find(builtin_vars, name);
    }
    return v;
}

Var *find_local_var(char *name, AstScope *scope) {
    return varlist_find(scope->locals, name);
}

void remove_resolution_parents(ResolutionList *res) {
    if (res == NULL) {
        return;
    }
    res->type = NULL;
    remove_resolution_parents(res->to_parent);
}

void remove_resolution(ResolutionList *res) {
    remove_resolution_parents(res);

    if (res->prev_in_scope != NULL) {
        res->prev_in_scope->next_in_scope = res->next_in_scope;
    }

    if (res->next_in_scope != NULL) {
        res->next_in_scope->prev_in_scope = res->prev_in_scope;
    }
}

Type *define_builtin_type(Type *type) {
    type->builtin = 1;
    type = register_type(type);
    builtin_types = typelist_append(builtin_types, type);
    return type;
}

Type *find_builtin_type(char *name) {
    for (TypeList *list = builtin_types; list != NULL; list = list->next) {
        if (!strcmp(name, list->item->name)) {
            return list->item;
        }
    }
    return NULL;
}

Type *define_type(Type *type, AstScope *scope) {
    for (TypeList *types = scope->local_types; types != NULL; types = types->next) {
        if (!strcmp(types->item->name, type->name)) { // TODO reference site of declaration here
            error(-1, "internal", "Type '%s' already declared within this scope.", type->name);
        }
    }

    type = register_type(type);

    if (scope->unresolved_types != NULL) {
        // Move head from previously cleared resolutions
        while (scope->unresolved_types->type == NULL) {
            scope->unresolved_types = scope->unresolved_types->next_in_scope;
            scope->unresolved_types->prev_in_scope = NULL;
        }

        if (scope->unresolved_types != NULL) {
            // Otherwise loop through the values and remove from the linked list
            ResolutionList *res = scope->unresolved_types;

            while (res != NULL) {
                if (res->type == NULL) {
                    remove_resolution(res);
                    continue;
                }
                if (!strcmp(res->type->name, type->name)) {
                    // merge types here, return existing type
                    *res->type = *type;
                    type = res->type;

                    remove_resolution(res);
                }

                res = res->next_in_scope;
            }
        }
    }
    
    scope->local_types = typelist_append(scope->local_types, type);
    
    return type;
}

int local_type_exists(char *name, AstScope *scope) {
    for (TypeList *list = scope->local_types; list != NULL; list = list->next) {
        if (!strcmp(list->item->name, name) && !list->item->unresolved) {
            return 1;
        }
    }
    return 0;
}

Type *find_type_by_name_no_unresolved(char *name, AstScope *scope) {
    for (TypeList *list = builtin_types; list != NULL; list = list->next) {
        if (!strcmp(name, list->item->name)) {
            return list->item;
        }
    }
    TypeList *types = scope->local_types;
    while (types != NULL) {
        if (!strcmp(types->item->name, name)) {
            return types->item;
        }
        types = types->next;
    }
    AstScope *s = scope->parent;
    while (s != NULL) {
        types = s->local_types;
        while (types != NULL) {
            if (!strcmp(types->item->name, name)) {
                return types->item;
            }
            types = types->next;
        }
        s = s->parent;
    }
    return NULL;
}

Type *find_type_by_name(char *name, AstScope *scope, ResolutionList *child) {
    for (TypeList *list = builtin_types; list != NULL; list = list->next) {
        if (!strcmp(name, list->item->name)) {
            return list->item;
        }
    }
    Type *t = make_type(name, AUTO_T, -1); // placeholder for "waiting"
    t->unresolved = 1;

    TypeList *types = scope->local_types;
    while (types != NULL) {
        if (!strcmp(types->item->name, name)) {
            return types->item;
        }
        types = types->next;
    }
    ResolutionList *res = malloc(sizeof(ResolutionList));

    res->to_parent = NULL;
    res->prev_in_scope = NULL;
    res->next_in_scope = scope->unresolved_types;

    res->type = t;

    if (scope->unresolved_types != NULL) {
        scope->unresolved_types->prev_in_scope = res;
    }
    if (child != NULL) {
        child->to_parent = res;
    }
    scope->unresolved_types = res;

    if (scope->parent != NULL) {
        *t = *find_type_by_name(name, scope->parent, res);
    }

    return t;
}

Ast *parse_uop_semantics(Ast *ast, AstScope *scope) {
    ast->unary->object = parse_semantics(ast->unary->object, scope);
    Ast *o = ast->unary->object;
    switch (ast->unary->op) {
    case OP_NOT:
        if (o->var_type->base != BOOL_T) {
            error(ast->line, ast->file, "Cannot perform logical negation on type '%s'.", o->var_type->name);
        }
        ast->var_type = base_type(BOOL_T);
        break;
    case OP_DEREF: // TODO precedence is wrong, see @x.data
        if (o->var_type->base != PTR_T) {
            error(ast->line, ast->file, "Cannot dereference a non-reference type (must cast baseptr).");
        }
        ast->var_type = o->var_type->inner;
        break;
    case OP_REF:
        if (o->type != AST_IDENTIFIER && o->type != AST_DOT) {
            error(ast->line, ast->file, "Cannot take a reference to a non-variable.");
        }
        ast->var_type = register_type(make_ptr_type(o->var_type));
        break;
    case OP_MINUS:
    case OP_PLUS: {
        Type *t = o->var_type;
        if (!is_numeric(t)) { // TODO try implicit cast to base type
            error(ast->line, ast->file, "Cannot perform '%s' operation on non-numeric type '%s'.", op_to_str(ast->unary->op), t->name);
        }
        ast->var_type = t;
        break;
    }
    default:
        error(ast->line, ast->file, "Unknown unary operator '%s' (%d).", op_to_str(ast->unary->op), ast->unary->op);
    }
    if (o->type == AST_LITERAL) {
        if (ast->type == AST_TEMP_VAR) {
            detach_var(o->tempvar->var, scope);
        }
        return eval_const_uop(ast);
    }
    if (is_dynamic(o->var_type)) {
        if (ast->unary->op == OP_DEREF) {
            if (o->type != AST_TEMP_VAR) {
                ast->unary->object = make_ast_tempvar(o, make_temp_var(o->var_type, scope));
            }
            Ast *tmp = make_ast_tempvar(ast, ast->unary->object->tempvar->var);
            return tmp;
        }
    }
    return ast;
}

Ast *parse_dot_op_semantics(Ast *ast, AstScope *scope) {
    if (ast->dot->object->type == AST_IDENTIFIER) {
        Type *t = find_type_by_name_no_unresolved(ast->dot->object->ident->varname, scope);
        if (t != NULL) {
            if (t->base == ENUM_T) {
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
                error(ast->line, ast->file, "No value '%s' in enum type '%s'.", ast->dot->member_name, t->name);
            } else {
                error(ast->line, ast->file, "Can't get member '%s' from non-enum type '%s'.", ast->dot->member_name, t->name);
            }
        }
    } 
    ast->dot->object = parse_semantics(ast->dot->object, scope);

    Type *orig = ast->dot->object->var_type;
    Type *t = orig->base == PTR_T ? orig->inner : orig;
    if (is_array(t) || (t->base == PTR_T && is_array(t->inner))) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            ast->var_type = register_type(make_ptr_type(t->inner));
        } else {
            error(ast->line, ast->file, "Cannot dot access member '%s' on array (only length or data).", ast->dot->member_name);
        }
    } else if (t->base == STRING_T || (t->base == PTR_T && t->inner->base == STRING_T)) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "bytes")) {
            ast->var_type = register_type(make_ptr_type(base_numeric_type(UINT_T, 8)));
        } else {
            error(ast->line, ast->file, "Cannot dot access member '%s' on string (only length or bytes).", ast->dot->member_name);
        }
    } else if (t->base != STRUCT_T && !(t->base == PTR_T && t->inner->base == STRUCT_T)) {
        error(ast->line, ast->file, "Cannot use dot operator on non-struct type '%s'.", orig->name);
    } else {
        for (int i = 0; i < t->st.nmembers; i++) {
            if (!strcmp(ast->dot->member_name, t->st.member_names[i])) {
                ast->var_type = t->st.member_types[i];
                return ast;
            }
        }
        error(ast->line, ast->file, "No member named '%s' in struct '%s'.", ast->dot->member_name, orig->name);
    }
    return ast;
}

Ast *parse_assignment_semantics(Ast *ast, AstScope *scope) {
    ast->binary->left = parse_semantics(ast->binary->left, scope);
    ast->binary->right = parse_semantics(ast->binary->right, scope);

    if (!is_lvalue(ast->binary->left)) {
        error(ast->line, ast->file, "LHS of assignment is not an lvalue.");
    } else if (ast->binary->left->type == AST_INDEX && ast->binary->left->index->object->var_type->base == STRING_T) {
        error(ast->line, ast->file, "Strings are immutable and cannot be subscripted for assignment.");
    }
    // TODO refactor to "is_constant"
    /*Var *v = get_ast_var(ast->binary->left);*/
    if (ast->binary->left->type == AST_IDENTIFIER && ast->binary->left->ident->var->constant) {
        error(ast->line, ast->file, "Cannot reassign constant '%s'.", ast->binary->left->ident->var->name);
    }
    Type *lt = ast->binary->left->var_type;
    Type *rt = ast->binary->right->var_type;
    if (!(check_type(lt, rt) || type_can_coerce(rt, lt))) {
        if (is_any(lt)) {
            if (!(is_lvalue(ast->binary->right) || ast->binary->right->type == AST_TEMP_VAR)) {
                ast->binary->right = make_ast_tempvar(ast->binary->right, make_temp_var(rt, scope));
            }
        }
        Ast *c = try_implicit_cast_no_error(lt, ast->binary->right);
        if (c != NULL) {
            ast->binary->right = c;
        } else {
            error(ast->binary->left->line, ast->binary->left->file, "LHS of assignment has type '%s', while RHS has type '%s'.",
                    lt->name, rt->name);
        }
    }
    if (ast->binary->right->type != AST_TEMP_VAR &&
            is_dynamic(lt) && ast->binary->right->type != AST_LITERAL) {
        ast->binary->right = make_ast_tempvar(ast->binary->right, make_temp_var(rt, scope));
    }
    ast->var_type = base_type(VOID_T);
    return ast;
}

Ast *parse_binop_semantics(Ast *ast, AstScope *scope) {
    ast->binary->left = parse_semantics(ast->binary->left, scope);
    ast->binary->right = parse_semantics(ast->binary->right, scope);

    Ast *l = ast->binary->left;
    Ast *r = ast->binary->right;

    Type *lt = l->var_type;
    Type *rt = r->var_type;

    switch (ast->binary->op) {
    case OP_PLUS:
        if (!(is_numeric(lt) && is_numeric(rt)) && !(lt->base == STRING_T && rt->base == STRING_T)) {
            error(ast->line, ast->file, "Operator '%s' is valid only for numeric or string arguments, not for type '%s'.",
                    op_to_str(ast->binary->op), lt->name);
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
        if (!is_numeric(lt)) {
            error(ast->line, ast->file, "LHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->binary->op), lt->name);
        } else if (!is_numeric(rt)) {
            error(ast->line, ast->file, "RHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->binary->op), rt->name);
        }
        break;
    case OP_AND:
    case OP_OR:
        if (lt->base != BOOL_T) {
            error(ast->line, ast->file, "Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(ast->binary->op), lt->name);
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        if (!type_equality_comparable(lt, rt)) {
            error(ast->line, ast->file, "Cannot compare equality of non-comparable types '%s' and '%s'.", lt->name, rt->name);
        }
        break;
    }
    if (l->type == AST_LITERAL && r->type == AST_LITERAL) {
        Type *t = lt;
        if (is_comparison(ast->binary->op)) {
            t = base_type(BOOL_T);
        } else if (ast->binary->op != OP_ASSIGN && is_numeric(lt)) {
            t = promote_number_type(lt, 1, rt, 1);
        }
        
        // TODO tmpvars are getting left behind when const string binops resolve
        if (l->type == AST_TEMP_VAR) {
            detach_var(l->tempvar->var, scope);
        }
        if (r->type == AST_TEMP_VAR) {
            detach_var(r->tempvar->var, scope);
        }
        ast = eval_const_binop(ast);
        ast->var_type = t;
        if (is_dynamic(t)) {
            int line = ast->line;
            ast = make_ast_tempvar(ast, make_temp_var(t, scope));
            ast->line = line;
        }
        return ast;
    }
    if (is_comparison(ast->binary->op)) {
        ast->var_type = base_type(BOOL_T);
    } else if (ast->binary->op != OP_ASSIGN && is_numeric(lt)) {
        ast->var_type = promote_number_type(lt, l->type == AST_LITERAL, rt, r->type == AST_LITERAL);
    } else {
        ast->var_type = lt;
    }
    if (!is_comparison(ast->binary->op) && is_dynamic(lt)) {
        ast = make_ast_tempvar(ast, make_temp_var(lt, scope));
        ast->var_type = lt;
    }
    return ast;
}

Ast *parse_enum_decl_semantics(Ast *ast, AstScope *scope) {
    Type *et = ast->enum_decl->enum_type; 
    Ast **exprs = ast->enum_decl->exprs;

    long val = 0;
    for (int i = 0; i < et->_enum.nmembers; i++) {
        if (exprs[i] != NULL) {
            exprs[i] = parse_semantics(exprs[i], scope);
            // TODO allow const other stuff in here
            if (exprs[i]->type != AST_LITERAL) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-constant expression.", et->name, et->_enum.member_names[i]);
            } else if (exprs[i]->var_type->base != INT_T) {
                error(exprs[i]->line, exprs[i]->file, "Cannot initialize enum '%s' member '%s' with non-integer expression.", et->name, et->_enum.member_names[i]);
            }
            exprs[i] = coerce_literal(exprs[i], et->_enum.inner);
            val = exprs[i]->lit->int_val;
        }
        et->_enum.member_values[i] = val;
        val += 1;
        for (int j = 0; j < i; j++) {
            if (!strcmp(et->_enum.member_names[i], et->_enum.member_names[j])) {
                error(ast->line, ast->file, "Enum '%s' member name '%s' defined twice.",
                        ast->enum_decl->enum_name, et->_enum.member_names[i]);
            }
            if (et->_enum.member_values[i] == et->_enum.member_values[j]) {
                error(ast->line, ast->file, "Enum '%s' contains duplicate values for members '%s' and '%s'.",
                    ast->enum_decl->enum_name, et->_enum.member_names[j],
                    et->_enum.member_names[i]);
            }
        }
    }
    ast->var_type = base_type(VOID_T);
    return ast;
}

AstBlock *parse_block_semantics(AstBlock *block, AstScope *scope, int fn_body) {
    Ast *last = NULL;
    int mainline_return_reached = 0;
    for (AstList *list = block->statements; list != NULL; list = list->next) {
        if (last != NULL && last->type == AST_RETURN) {
            error(list->item->line, list->item->file, "Unreachable statements following return.");
        }
        list->item = parse_semantics(list->item, scope);
        if (list->item->type == AST_RETURN) {
            mainline_return_reached = 1;
        }
        last = list->item;
    }

    // if it's a function that needs return and return wasn't reached, break
    if (!mainline_return_reached && fn_body &&
      current_fn_scope->fn_decl->var->type->fn.ret->base != VOID_T) {
        error(block->endline, block->file, "Control reaches end of function '%s' without a return statement.",
            current_fn_scope->fn_decl->anon ? "(anonymous)" :
            current_fn_scope->fn_decl->var->name);
    }
    return block;
}


AstScope *parse_scope_semantics(AstScope *scope, AstScope *parent, int fn) {
    scope->body = parse_block_semantics(scope->body, scope, fn);
    return scope;
}

Ast *parse_directive_semantics(Ast *ast, AstScope *scope) {
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
        t->var_type = register_type(typeinfo_ptr());
        return t;
    } else if (!strcmp(n, "type")) {
        Ast *t = ast_alloc(AST_TYPEINFO);
        t->line = ast->line;
        t->typeinfo->typeinfo_target = ast->var_type;
        t->var_type = register_type(typeinfo_ptr());
        return t;
    }
    error(ast->line, ast->file, "Unrecognized directive '%s'.", n);
    return NULL;
}

Ast *parse_struct_literal_semantics(Ast *ast, AstScope *scope) {
    // TODO make a tmpvar?
    AstLiteral *lit = ast->lit;
    Type *t = find_type_by_name(lit->struct_val.name, scope, NULL);

    if (t->unresolved) {
        error(ast->line, ast->file, "Undefined struct type '%s' encountered.", lit->struct_val.name);
    } else if (t->base != STRUCT_T) {
        error(ast->line, ast->file, "Type '%s' is not a struct.", lit->struct_val.name);
    }

    lit->struct_val.type = t;
    ast->var_type = t;

    for (int i = 0; i < lit->struct_val.nmembers; i++) {
        int found = 0;
        for (int j = 0; j < t->st.nmembers; j++) {
            if (!strcmp(lit->struct_val.member_names[i], t->st.member_names[j])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            error(ast->line, ast->file, "Struct '%s' has no member named '%s'.", t->name, lit->struct_val.member_names[i]);
        }

        Ast *expr = parse_semantics(lit->struct_val.member_exprs[i], scope);
        if (is_dynamic(t->st.member_types[i]) && expr->type != AST_LITERAL) {
            expr = make_ast_copy(expr);
        }
        lit->struct_val.member_exprs[i] = expr;
    }
    return ast;
}

Ast *parse_use_semantics(Ast *ast, AstScope *scope) {
    if (ast->use->object->type == AST_IDENTIFIER) {
        Type *t = find_type_by_name_no_unresolved(ast->dot->object->ident->varname, scope);
        if (t != NULL) {
            if (t->base != ENUM_T) {
                error(ast->use->object->line, ast->file, "'use' is not valid on non-enum type '%s'.", t->name);
            }
            for (int i = 0; i < t->_enum.nmembers; i++) {
                char *name = t->_enum.member_names[i];
                if (find_local_var(name, scope) != NULL) {
                    error(ast->use->object->line, ast->file, "'use' statement on enum '%s' conflicts with local variable named '%s'.", t->name, name);
                }
                if (varlist_find(builtin_vars, name) != NULL) {
                    error(ast->use->object->line, ast->file, "'use' statement on enum '%s' conflicts with builtin type named '%s'.", t->name, name);
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
    if (t->base == PTR_T) {
        t = t->inner;
    }
    if (t->base != STRUCT_T) {
        error(ast->use->object->line, ast->file, "'use' is not valid on non-struct type '%s'.", ast->use->object->var_type->name);
    } else {
        Var *ast_var = get_ast_var_noerror(ast->use->object);
        if (ast_var == NULL) {
            error(ast->use->object->line, ast->use->object->file, "Can't 'use' a non-variable.");
        }
        for (int i = 0; i < t->st.nmembers; i++) {
            char *name = t->st.member_names[i];
            if (find_local_var(name, scope) != NULL) {
                error(ast->use->object->line, ast->file, "'use' statement on struct type '%s' conflicts with local variable named '%s'.", t->name, name);
            }
            if (varlist_find(builtin_vars, name) != NULL) {
                error(ast->use->object->line, ast->file, "'use' statement on struct type '%s' conflicts with builtin type named '%s'.", t->name, name);
            }
            Var *v = make_var(name, t->st.member_types[i]); // should this be 't' or 't->st.member_types[i]'?
            Var *proxy = ast_var->members[i];
            if (orig->base == PTR_T) {
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

Ast *parse_slice_semantics(Ast *ast, AstScope *scope) {
    AstSlice *slice = ast->slice;
    slice->object = parse_semantics(slice->object, scope);
    Type *a = slice->object->var_type;
    if (!is_array(a)) {
        error(ast->line, ast->file, "Cannot slice non-array type '%s'.", a->name);
    }
    ast->var_type = register_type(make_array_type(a->inner));
    if (slice->offset != NULL) {
        slice->offset = parse_semantics(slice->offset, scope);
        if (slice->offset->type == AST_LITERAL && a->base == STATIC_ARRAY_T) {
            // TODO check that it's an int?
            long o = slice->offset->lit->int_val;
            if (o < 0) {
                error(ast->line, ast->file, "Negative slice start is not allowed.");
            } else if (o >= a->length) {
                error(ast->line, ast->file, "Slice offset outside of array bounds (offset %ld to array length %ld).", o, a->length);
            }
        }
    }
    if (slice->length != NULL) {
        slice->length = parse_semantics(slice->length, scope);
        if (slice->length->type == AST_LITERAL && a->base == STATIC_ARRAY_T) {
            // TODO check that it's an int?
            long l = slice->length->lit->int_val;
            if (l > a->length) {
                error(ast->line, ast->file, "Slice length outside of array bounds (%ld to array length %ld).", l, a->length);
            }
        }
    }
    return ast;
}

Ast *parse_declaration_semantics(Ast *ast, AstScope *scope) {
    AstDecl *decl = ast->decl;
    Ast *init = decl->init;

    if (init != NULL) {
        if (init->type != AST_ANON_FUNC_DECL) {
            in_decl = 1;
        }
        init = parse_semantics(init, scope);
        in_decl = 0;

        if (decl->var->type->base == AUTO_T) {
            decl->var->type = init->var_type;
        } else if (decl->var->type->length == -1) {
            decl->var->type->length = init->var_type->length;
        }

        if (init->type == AST_LITERAL && is_numeric(decl->var->type)) {
            int b = decl->var->type->base;
            if (init->lit->lit_type == FLOAT && (b == UINT_T || b == INT_T)) {
                error(ast->line, ast->file, "Cannot implicitly cast float literal '%f' to integer type '%s'.", init->lit->float_val, decl->var->type->name);
            }
            if (b == UINT_T) {
                if (init->lit->int_val < 0) {
                    error(ast->line, ast->file, "Cannot assign negative integer literal '%d' to unsigned type '%s'.", init->lit->int_val, decl->var->type->name);
                }
                if (precision_loss_uint(decl->var->type, init->lit->int_val)) {
                    error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, decl->var->type->name);
                }
            } else if (b == INT_T) {
                if (precision_loss_int(decl->var->type, init->lit->int_val)) {
                    error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, decl->var->type->name);
                }
            } else if (b == FLOAT_T) {
                if (precision_loss_float(decl->var->type, init->lit->lit_type == FLOAT ? init->lit->float_val : init->lit->int_val)) {
                    error(ast->line, ast->file, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->float_val, decl->var->type->name);
                }
            } else {
                error(-1, "internal", "wtf");
            }
        }

        if (!(check_type(decl->var->type, init->var_type) || type_can_coerce(init->var_type, decl->var->type))) {
            // TODO only for literal?
            decl->init = try_implicit_cast(decl->var->type, init);
            init = decl->init;
        }

        if (init->type != AST_TEMP_VAR && is_dynamic(decl->var->type)) {
            if (init->type != AST_LITERAL) {
                init = make_ast_copy(init);
            }
            init = make_ast_tempvar(init, make_temp_var(init->var_type, scope));
        }

        decl->init = init;
        ast->var_type = decl->var->type;

    } else if (decl->var->type->base == AUTO_T) {
        error(ast->line, ast->file, "Cannot use type 'auto' for variable '%s' without initialization.", decl->var->name);
    } else if (decl->var->type->base == STATIC_ARRAY_T && decl->var->type->length == -1) {
        error(ast->line, ast->file, "Cannot use unspecified array type for variable '%s' without initialization.", decl->var->name);
    }

    if (find_local_var(decl->var->name, scope) != NULL) {
        error(ast->line, ast->file, "Declared variable '%s' already exists.", decl->var->name);
    }

    if (find_builtin_type(decl->var->name) != NULL || local_type_exists(decl->var->name, scope)) {
        error(ast->line, ast->file, "Variable name '%s' already in use by type.", decl->var->name);
    }

    ast->var_type = base_type(VOID_T);

    if (parser_state == PARSE_MAIN) {
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
        attach_var(ast->decl->var, scope); // should this be after the init parsing?
    }
    return ast;
}

Ast *parse_call_semantics(Ast *ast, AstScope *scope) {
    // TODO don't allow auto explicitly?
    ast->call->fn = parse_semantics(ast->call->fn, scope);

    Type *t = ast->call->fn->var_type;
    if (t->base != FN_T) {
        error(ast->line, ast->file, "Cannot perform call on non-function type '%s'", t->name);
    }
    if (ast->call->fn->type == AST_ANON_FUNC_DECL) {
        ast->call->fn = make_ast_tempvar(ast->call->fn, make_temp_var(t, scope));
    }
    if (t->fn.variadic) {
        if (ast->call->nargs < t->fn.nargs - 1) {
            error(ast->line, ast->file, "Expected at least %d arguments to variadic function, but only got %d.", t->fn.nargs-1, ast->call->nargs);
        } else {
            TypeList *list = t->fn.args;
            Type *a = list->item;
            while (list != NULL) {
                a = list->item;
                list = list->next;
            }
            a = make_static_array_type(a, ast->call->nargs - (t->fn.nargs - 1));;
            ast->call->variadic_tempvar = make_temp_var(a, scope);
        }
    } else if (ast->call->nargs != t->fn.nargs) {
        error(ast->line, ast->file, "Incorrect argument count to function (expected %d, got %d)", t->fn.nargs, ast->call->nargs);
    }
    AstList *args = ast->call->args;
    TypeList *arg_types = t->fn.args;
    for (int i = 0; args != NULL; i++) {
        Ast *arg = args->item;
        arg = parse_semantics(arg, scope);

        if (is_any(arg_types->item)) {
            if (!is_any(arg->var_type) && (arg->type != AST_TEMP_VAR && !is_lvalue(arg))) {
                if (arg->type != AST_LITERAL) {
                    arg = make_ast_copy(arg);
                }
                arg = make_ast_tempvar(arg, make_temp_var(arg->var_type, scope));
            }
        }

        if (!(check_type(arg->var_type, arg_types->item) || type_can_coerce(arg->var_type, arg_types->item))) {
            arg = try_implicit_cast(arg_types->item, arg);
        }

        // TODO NOT struct? why?
        if (arg->type != AST_TEMP_VAR && arg->var_type->base != STRUCT_T && is_dynamic(arg_types->item)) {
            if (arg->type != AST_LITERAL) {
                arg = make_ast_copy(arg);
            }
            arg = make_ast_tempvar(arg, make_temp_var(arg->var_type, scope));
        }
        args->item = arg;
        args = args->next;
        if (!t->fn.variadic || i < t->fn.nargs - 1) {
            arg_types = arg_types->next;
        }
    }

    ast->var_type = t->fn.ret;
    if (is_dynamic(t->fn.ret)) {
        return make_ast_tempvar(ast, make_temp_var(t->fn.ret, scope));
    }
    return ast;
}

Ast *parse_semantics(Ast *ast, AstScope *scope) {
    switch (ast->type) {
    case AST_LITERAL: {
        switch (ast->lit->lit_type) {
        case STRING: 
            ast->var_type = base_type(STRING_T);
            return make_ast_tempvar(ast, make_temp_var(ast->var_type, scope));
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
        case STRUCT:
            ast = parse_struct_literal_semantics(ast, scope);
            break;
        case ENUM:
            break;
        }
        break;
    }
    case AST_IDENTIFIER: {
        Var *v = find_var(ast->ident->varname, scope);
        if (v == NULL) {
            error(ast->line, ast->file, "Undefined identifier '%s' encountered.", ast->ident->varname);
            // TODO better error for an enum here
        }
        if (v->proxy != NULL) {
            if (v->type->base == ENUM_T) {
                for (int i = 0; i < v->type->_enum.nmembers; i++) {
                    if (!strcmp(v->type->_enum.member_names[i], v->name)) {
                        Ast *a = ast_alloc(AST_LITERAL);
                        a->lit->lit_type = ENUM;
                        a->lit->enum_val.enum_index = i;
                        a->lit->enum_val.enum_type = v->type;
                        a->var_type = v->type;
                        return a;
                    }
                }
                error(-1, "<internal>", "How'd this happen");
            } else {
                int i = 0;
                assert(v->proxy != v);
                while (v->proxy) {
                    assert(i < 666); // lol
                    v = v->proxy; // this better not loop!
                }
            }
        }
        ast->ident->var = v;
        ast->var_type = ast->ident->var->type;
        break;
    }
    case AST_DOT:
        return parse_dot_op_semantics(ast, scope);
    case AST_ASSIGN:
        return parse_assignment_semantics(ast, scope);
    case AST_BINOP:
        return parse_binop_semantics(ast, scope);
    case AST_UOP:
        return parse_uop_semantics(ast, scope);
    case AST_USE:
        return parse_use_semantics(ast, scope);
    case AST_SLICE:
        return parse_slice_semantics(ast, scope);
    case AST_CAST: {
        AstCast *cast = ast->cast;
        cast->object = parse_semantics(cast->object, scope);
        if (cast->object->type == AST_LITERAL) {
            cast->object = cast_literal(cast->cast_type, cast->object);
        } else {
            if (!can_cast(cast->object->var_type, cast->cast_type)) {
                error(ast->line, ast->file, "Cannot cast type '%s' to type '%s'.", cast->object->var_type->name, cast->cast_type->name);
            }
        }
        if (is_any(cast->cast_type)) {
            if (!is_lvalue(cast->object)) {
                cast->object = make_ast_tempvar(cast->object, make_temp_var(cast->object->var_type, scope));
            }
        }
        ast->var_type = cast->cast_type;
        break;
    }
    case AST_RELEASE: {
        AstRelease *rel = ast->release;
        rel->object = parse_semantics(rel->object, scope);

        Type *t = rel->object->var_type;

        if (t->base != PTR_T && t->base != BASEPTR_T && t->base != ARRAY_T) {
            error(ast->line, ast->file, "Struct member release target must be a pointer.");
        }
        if (rel->object->type == AST_IDENTIFIER) {
            // TODO instead mark that var has been released for better errors in
            // the future
            release_var(rel->object->ident->var, scope);
        }
        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_HOLD: {
        AstHold *hold = ast->hold;

        hold->object = parse_semantics(hold->object, scope);

        Type *t = hold->object->var_type;

        if (t->base == VOID_T || t->base == FN_T) {
            error(ast->line, ast->file, "Cannot hold a value of type '%s'.", t->name);
        }

        if (hold->object->type != AST_TEMP_VAR && is_dynamic(t)) {
            hold->object = make_ast_tempvar(hold->object, make_temp_var(t, scope));
        }

        Type *tp = NULL;
        if (t->base == STATIC_ARRAY_T) {
            // If the static array has a dynamic inner type, we need to add the
            // type to the list of those for which "hold" functions will be
            // created
            // This is a bit of a hack, specifically for the c backend...
            if (is_dynamic(t->inner)) {
                TypeList *h = global_hold_funcs;
                while (h != NULL) {
                    if (h->item->id == t->id) {
                        break;
                   }
                    h = h->next;
                }
                if (h == NULL) {
                    global_hold_funcs = typelist_append(global_hold_funcs, t);
                }
            }
            tp = register_type(make_array_type(t->inner));
        } else {
            tp = register_type(make_ptr_type(t));
        }
        tp->held = 1; // eh?

        hold->tempvar = make_temp_var(tp, scope);
        ast->var_type = tp;
        break;
    }
    case AST_DECL:
        return parse_declaration_semantics(ast, scope);
    case AST_TYPE_DECL: {
        Var *v = find_local_var(ast->type_decl->type_name, scope);
        if (v != NULL) {
            error(ast->line, ast->file, "Type name '%s' already exists as variable.", ast->type_decl->type_name);
        }
        if (find_builtin_type(ast->type_decl->type_name) != NULL) {
            error(ast->line, ast->file, "Cannot shadow builtin type named '%s'.", ast->type_decl->type_name);
        }
        break;
    }
    case AST_EXTERN_FUNC_DECL:
        // TODO fix this
        if (parser_state != PARSE_MAIN) {
            error(ast->line, ast->file, "Cannot declare an extern inside scope ('%s').", ast->fn_decl->var->name);
        }
        attach_var(ast->fn_decl->var, scope);
        global_fn_vars = varlist_append(global_fn_vars, ast->fn_decl->var);
        register_type(ast->fn_decl->var->type);
        break;
    case AST_FUNC_DECL:
    case AST_ANON_FUNC_DECL: {
        Var *bindings_var = NULL;
        Type *fn_t = ast->fn_decl->var->type;

        /*if (ast->type == AST_ANON_FUNC_DECL) {*/
            /*bindings_var = malloc(sizeof(Var));*/
            /*bindings_var->name = "";*/
            /*bindings_var->type = base_type(BASEPTR_T);*/
            /*bindings_var->id = new_var_id();*/
            /*bindings_var->temp = 1;*/
            /*bindings_var->consumed = 0;*/
            /*fn_t->bindings_id = bindings_var->id;*/
        /*}*/

        for (TypeList *args = fn_t->fn.args; args != NULL; args = args->next) {
            args->item = register_type(args->item);
        }

        int prev = parser_state;
        parser_state = PARSE_FUNC;

        PUSH_FN_SCOPE(ast);
        ast->fn_decl->scope = parse_scope_semantics(ast->fn_decl->scope, scope, 1);
        POP_FN_SCOPE();
        parser_state = prev;

        detach_var(ast->fn_decl->var, ast->fn_decl->scope);

        ast->fn_decl->var->type = register_type(ast->fn_decl->var->type);

        if (ast->type == AST_ANON_FUNC_DECL) {
            ast->var_type = ast->fn_decl->var->type;
            /*ast->fn_decl->var->type->bindings_id = bindings_var->id;*/
        }

        if (fn_t->bindings != NULL) {
            global_fn_bindings = varlist_append(global_fn_bindings, bindings_var);
        }
        break;
    }
    case AST_CALL:
        return parse_call_semantics(ast, scope);
    case AST_INDEX: {
        ast->index->object = parse_semantics(ast->index->object, scope);
        ast->index->index = parse_semantics(ast->index->index, scope);

        Type *obj_type = ast->index->object->var_type;
        Type *ind_type = ast->index->index->var_type;

        int size = -1;

        if (obj_type->base == STRING_T) {
            ast->var_type = base_numeric_type(UINT_T, 8);
            if (ast->index->object->type == AST_LITERAL) {
                size = strlen(ast->index->object->lit->string_val);
            }
        } else if (is_array(obj_type)) {
            if (obj_type->base == STATIC_ARRAY_T) {
                size = array_size(obj_type);
            }

            ast->var_type = obj_type->inner; // need to call something different?
        } else {
            error(ast->index->object->line, ast->index->object->file, "Cannot perform index/subscript operation on non-array type (type is '%s').", obj_type->name);
        }

        if (ind_type->base != INT_T && ind_type->base != UINT_T) {
            error(ast->line, ast->file, "Cannot index array with non-integer type '%s'.", ind_type->name);
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
        c->condition = parse_semantics(c->condition, scope);
        if (c->condition->var_type->base != BOOL_T) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for if statement.", c->condition->var_type->name);
        }
        c->if_body = parse_scope_semantics(c->if_body, scope, 0);
        if (c->else_body != NULL) {
            c->else_body = parse_scope_semantics(c->else_body, scope, 0);
        }
        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_WHILE: {
        AstWhile *lp = ast->while_loop;
        lp->condition = parse_semantics(lp->condition, scope);

        if (lp->condition->var_type->base != BOOL_T) {
            error(ast->line, ast->file, "Non-boolean ('%s') condition for while loop.", lp->condition->var_type->name);
        }

        int _old = loop_state;
        loop_state = 1;
        lp->body = parse_scope_semantics(lp->body, scope, 0);
        loop_state = _old;

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_FOR: {
        AstFor *lp = ast->for_loop;
        lp->iterable = parse_semantics(lp->iterable, scope);
        Type *it_type = lp->iterable->var_type;
        if (is_array(it_type)) {
            lp->itervar->type = it_type->inner;
        } else if (it_type->base == STRING_T) {
            lp->itervar->type = base_numeric_type(UINT_T, 8);
        } else {
            error(ast->line, ast->file, "Cannot use for loop on non-interable type '%s'.", it_type->name);
        }
        // TODO type check for when type of itervar is explicit
        int _old = loop_state;
        loop_state = 1;
        attach_var(lp->itervar, lp->body);
        lp->body = parse_scope_semantics(lp->body, scope, 0);
        loop_state = _old;

        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_SCOPE:
        ast->scope = parse_scope_semantics(ast->scope, scope, 0);
        break;
    case AST_BIND: {
        if (current_fn_scope == NULL || current_fn_scope->type != AST_ANON_FUNC_DECL) {
            error(ast->line, ast->file, "Cannot make bindings outside of an inner function."); 
        }
        ast->bind->expr = parse_semantics(ast->bind->expr, current_fn_scope->fn_decl->scope->parent);
        ast->bind->offset = add_binding(current_fn_scope->fn_decl->var->type, ast->bind->expr->var_type);
        ast->bind->bind_id = current_fn_scope->fn_decl->var->type->bindings_id;
        add_binding_expr(ast->bind->bind_id, ast->bind->expr);
        ast->var_type = ast->bind->expr->var_type;
        if (is_dynamic(ast->var_type)) {
            return make_ast_tempvar(ast, make_temp_var(ast->var_type, scope));
        }
        break;
    }
    case AST_RETURN: {
        // TODO don't need to copy string being returned?
        if (current_fn_scope == NULL || parser_state != PARSE_FUNC) {
            error(ast->line, ast->file, "Return statement outside of function body.");
        }
        scope->has_return = 1;
        Type *fn_ret_t = current_fn_scope->fn_decl->var->type->fn.ret;
        Type *ret_t = NULL;
        if (ast->ret->expr == NULL) {
            ret_t = base_type(VOID_T);
        } else {
            ast->ret->expr = parse_semantics(ast->ret->expr, scope);
            ret_t = ast->ret->expr->var_type;
            if (is_dynamic(ret_t) && ast->ret->expr->type != AST_LITERAL) {
                ast->ret->expr = make_ast_copy(ast->ret->expr);
            }
        }
        if (ret_t->base == STATIC_ARRAY_T) {
            error(ast->line, ast->file, "Cannot return a static array from a function.");
        }
        if (fn_ret_t->base == AUTO_T) {
            current_fn_scope->fn_decl->var->type->fn.ret = ret_t;
        } else if (!check_type(fn_ret_t, ret_t)) {
            if (ast->ret->expr->type == AST_LITERAL) {
                ast->ret->expr = try_implicit_cast(fn_ret_t, ast->ret->expr);
            } else {
                error(ast->line, ast->file, "Return statement type '%s' does not match enclosing function's return type '%s'.", ret_t->name, fn_ret_t->name);
            }
        }
        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_BREAK:
        if (!loop_state) {
            error(ast->line, ast->file, "Break statement outside of loop.");
        }
        ast->var_type = base_type(VOID_T);
        break;
    case AST_CONTINUE:
        if (!loop_state) {
            error(ast->line, ast->file, "Continue statement outside of loop.");
        }
        ast->var_type = base_type(VOID_T);
        break;
    case AST_BLOCK:
        ast->block = parse_block_semantics(ast->block, scope, 0);
        break;
    case AST_DIRECTIVE:
        return parse_directive_semantics(ast, scope);
    case AST_ENUM_DECL:
        return parse_enum_decl_semantics(ast, scope);
    default:
        error(-1, "internal", "idk parse semantics %d", ast->type);
    }
    return ast;
}

AstList *get_binding_exprs(int id) {
    AstListList *l = binding_exprs;
    while (l != NULL) {
        if (l->id == id) {
            return l->item;
        }
        l = l->next;
    }
    error(-1, "internal", "Couldn't find binding exprs %d.", id);
    return NULL;
}

void add_binding_expr(int id, Ast *expr) {
    AstListList *l = binding_exprs;
    while (l != NULL) {
        if (l->id == id) {
            l->item = astlist_append(l->item, expr);
            return;
        }
        l = l->next;
    }
    l = malloc(sizeof(AstListList));
    l->id = id;
    l->item = astlist_append(NULL, expr);
    l->next = binding_exprs;
    binding_exprs = l;
}

TypeList *get_global_hold_funcs() {
    return global_hold_funcs;
}

VarList *get_global_vars() {
    return global_vars;
}

VarList *get_global_bindings() {
    return global_fn_bindings;
}

/*TypeList *get_builtin_types() {*/
    /*return builtin_types;*/
/*}*/
