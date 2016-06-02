#include "parse.h"

#define PUSH_FN_SCOPE(x) \
    Ast *__old_fn_scope = current_fn_scope;\
    current_fn_scope = (x);
#define POP_FN_SCOPE() current_fn_scope = __old_fn_scope;

static int last_tmp_fn_id = 0;

static VarList *global_vars = NULL;
static VarList *global_fn_vars = NULL;
static AstList *global_fn_decls = NULL;
static VarList *global_fn_bindings = NULL;
static TypeList *global_hold_funcs = NULL;

static TypeList *builtin_types = NULL;
static VarList *builtin_vars = NULL;

static AstListList *binding_exprs = NULL;

static int parser_state = PARSE_MAIN; // TODO unnecessary w/ PUSH_FN_SCOPE?
static int loop_state = 0;
static int in_decl = 0;

static Ast *current_fn_scope = NULL;

void print_locals(AstScope *scope) {
    VarList *v = scope->locals;
    fprintf(stderr, "locals: ");
    while (v != NULL) {
        fprintf(stderr, "%s ", v->item->name);
        v = v->next;
    }
    fprintf(stderr, "\n");
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

TypeList *get_builtin_types() {
    return builtin_types;
}

Type *define_type(Type *type, AstScope *scope) {
    for (TypeList *types = scope->local_types; types != NULL; types = types->next) {
        if (!strcmp(types->item->name, type->name)) { // TODO reference site of declaration here
            error(-1, "Type '%s' already declared within this scope.", type->name);
        }
    }

    type = register_type(type);

    if (scope->unresolved_types != NULL) {
        // Move head from previously cleared resolutions
        while (scope->unresolved_types->type == NULL) {
            scope->unresolved_types = scope->unresolved_types->next_in_scope;
            scope->unresolved_types->prev_in_scope = NULL;
        }

        /*fprintf(stderr, "Checking for unresolved types\n");*/

        // If the first one matches, we need to change the value of
        // scope->unresolved_types (head)
        /*if (!strcmp(scope->unresolved_types->type->name, type->name)) {*/
            
            /*// merge types here, return existing type*/
            /**scope->unresolved_types->type = *type;*/
            /*type = scope->unresolved_types->type;*/
            /*fprintf(stderr, "First matched %s\n", scope->unresolved_types->type->name);*/

            /*remove_resolution_parents(scope->unresolved_types);*/
            /*scope->unresolved_types = scope->unresolved_types->next_in_scope;*/
            /*if (scope->unresolved_types != NULL) {*/
                /*scope->unresolved_types->prev_in_scope = NULL;    */
            /*}   */
        /*}*/

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
            error(ast->line, "Cannot perform logical negation on type '%s'.", o->var_type->name);
        }
        ast->var_type = base_type(BOOL_T);
        break;
    case OP_DEREF: // TODO precedence is wrong, see @x.data
        if (o->var_type->base != PTR_T) {
            error(ast->line, "Cannot dereference a non-pointer type (must cast baseptr).");
        }
        ast->var_type = o->var_type->inner;
        break;
    case OP_ADDR:
        if (o->type != AST_IDENTIFIER && o->type != AST_DOT) {
            error(ast->line, "Cannot take the address of a non-variable.");
        }
        ast->var_type = register_type(make_ptr_type(o->var_type));
        break;
    case OP_MINUS:
    case OP_PLUS: {
        Type *t = o->var_type;
        if (!is_numeric(t)) { // TODO try implicit cast to base type
            error(ast->line, "Cannot perform '%s' operation on non-numeric type '%s'.", op_to_str(ast->unary->op), t->name);
        }
        ast->var_type = t;
        break;
    }
    default:
        error(ast->line, "Unknown unary operator '%s' (%d).", op_to_str(ast->unary->op), ast->unary->op);
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
    return NULL;
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
                error(ast->line, "No value '%s' in enum type '%s'.", ast->dot->member_name, t->name);
            } else {
                error(ast->line, "Can't get member '%s' from non-enum type '%s'.", ast->dot->member_name, t->name);
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
            error(ast->line, "Cannot dot access member '%s' on array (only length or data).", ast->dot->member_name);
        }
    } else if (t->base == STRING_T || (t->base == PTR_T && t->inner->base == STRING_T)) {
        if (!strcmp(ast->dot->member_name, "length")) {
            ast->var_type = base_type(INT_T);
        } else if (!strcmp(ast->dot->member_name, "bytes")) {
            ast->var_type = register_type(make_ptr_type(base_numeric_type(UINT_T, 8)));
        } else {
            error(ast->line, "Cannot dot access member '%s' on string (only length or bytes).", ast->dot->member_name);
        }
    } else if (t->base != STRUCT_T && !(t->base == PTR_T && t->inner->base == STRUCT_T)) {
        error(ast->line, "Cannot use dot operator on non-struct type '%s'.", orig->name);
    } else {
        for (int i = 0; i < t->st.nmembers; i++) {
            if (!strcmp(ast->dot->member_name, t->st.member_names[i])) {
                ast->var_type = t->st.member_types[i];
                return ast;
            }
        }
        error(ast->line, "No member named '%s' in struct '%s'.", ast->dot->member_name, orig->name);
    }
    return ast;
}

Ast *parse_assignment_semantics(Ast *ast, AstScope *scope) {
    ast->binary->left = parse_semantics(ast->binary->left, scope);
    ast->binary->right = parse_semantics(ast->binary->right, scope);

    if (!is_lvalue(ast->binary->left)) {
        error(ast->line, "LHS of assignment is not an lvalue.");
    }
    // TODO refactor to "is_constant"
    /*Var *v = get_ast_var(ast->binary->left);*/
    if (ast->binary->left->type == AST_IDENTIFIER && ast->binary->left->ident->var->constant) {
        error(ast->line, "Cannot reassign constant '%s'.", ast->binary->left->ident->var->name);
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
            error(ast->binary->left->line, "LHS of assignment has type '%s', while RHS has type '%s'.",
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
            error(ast->line, "Operator '%s' is valid only for numeric or string arguments, not for type '%s'.",
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
            error(ast->line, "LHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->binary->op), lt->name);
        } else if (!is_numeric(rt)) {
            error(ast->line, "RHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->binary->op), rt->name);
        }
        break;
    case OP_AND:
    case OP_OR:
        if (lt->base != BOOL_T) {
            error(ast->line, "Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(ast->binary->op), lt->name);
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        if (!type_equality_comparable(lt, rt)) {
            error(ast->line, "Cannot compare equality of non-comparable types '%s' and '%s'.", lt->name, rt->name);
        }
        break;
    }
    if (l->type == AST_LITERAL && r->type == AST_LITERAL) {
        Type *t = lt;
        if (is_comparison(ast->binary->op)) {
            t = base_type(BOOL_T);
        } else if (ast->binary->op != OP_ASSIGN && is_numeric(lt)) {
            t = promote_number_type(lt, rt);
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
        ast->var_type = promote_number_type(lt, rt);
    } else {
        ast->var_type = lt;
    }
    if (!is_comparison(ast->binary->op) && is_dynamic(lt)) {
        ast = make_ast_tempvar(ast, make_temp_var(lt, scope));
        ast->var_type = lt;
    }
    return ast;
}

Ast *parse_array_slice(Ast *object, Ast *offset, AstScope *scope) {
    Ast *s = ast_alloc(AST_SLICE);
    s->slice->object = object;
    s->slice->offset = offset;
    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), "Unexpected EOF while parsing array slice.");
    } else if (t->type == TOK_RSQUARE) {
        s->slice->length = NULL;
    } else {
        s->slice->length = parse_expression(t, 0, scope);
        expect(TOK_RSQUARE);
    }
    return s; 
}

Ast *parse_array_index(Ast *object, AstScope *scope) {
    Tok *t = next_token();
    if (t->type == TOK_COLON) {
        return parse_array_slice(object, NULL, scope);
    }
    Ast *ind = ast_alloc(AST_INDEX);
    ind->index->object = object;
    ind->index->index = parse_expression(t, 0, scope);
    t = next_token();
    if (t->type == TOK_COLON) {
        Ast *offset = ind->index->index;
        free(ind);
        return parse_array_slice(object, offset, scope);
    } else if (t->type != TOK_RSQUARE) {
        error(lineno(), "Unexpected token '%s' while parsing array index.", to_string(t));
    }
    return ind; 
}

Ast *parse_arg_list(Ast *left, AstScope *scope) {
    Ast *func = ast_alloc(AST_CALL);
    func->call->args = NULL;
    func->call->fn = left;
    int n = 0;
    Tok *t;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        func->call->args = astlist_append(func->call->args, parse_expression(t, 0, scope));
        n++;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    func->call->args = reverse_astlist(func->call->args);
    func->call->nargs = n;
    return func; 
}

Ast *parse_declaration(Tok *t, AstScope *scope) {
    char *name = t->sval;
    Tok *next = next_token();
    Type *type = NULL;
    if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        type = make_type("auto", AUTO_T, -1);
    } else {
        type = parse_type(next, scope);
        next = next_token();
    }
    Ast *lhs = make_ast_decl(name, type);
    if (next == NULL) {
        error(lhs->line, "Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        lhs->decl->init = parse_expression(next_token(), 0, scope);
    } else if (next->type != TOK_SEMI) {
        error(lineno(), "Unexpected token '%s' while parsing declaration.", to_string(next));
    } else {
        unget_token(next);
    }
    return lhs; 
}

Ast *parse_expression(Tok *t, int priority, AstScope *scope) {
    Ast *ast = parse_primary(t, scope);
    for (;;) {
        t = next_token();
        if (t == NULL) {
            return ast;
        } else if (t->type == TOK_SEMI || t->type == TOK_RPAREN || t->type == TOK_LBRACE || t->type == TOK_RBRACE || t->type == TOK_RSQUARE) {
            unget_token(t);
            return ast;
        }
        int next_priority = priority_of(t);
        if (next_priority < 0 || next_priority < priority) {
            unget_token(t);
            return ast;
        } else if (t->type == TOK_OP) {
            if (t->op == OP_ASSIGN) {
                ast = make_ast_assign(ast, parse_expression(next_token(), 0, scope));
            } else if (t->op == OP_DOT) {
                Tok *next = next_token();
                if (next->type != TOK_ID) {
                    // TODO change this to something like "TOK_KEYWORD"
                    if (next->type == TOK_TYPE) {
                        next->type = TOK_ID;
                        next->sval = "type";
                    } else {
                        error(lineno(), "Unexpected token '%s' while parsing dot operation.", to_string(next));
                    }
                }
                ast = make_ast_dot_op(ast, next->sval);
                ast->type = AST_DOT;
            } else if (t->op == OP_CAST) {
                Ast *c = ast_alloc(AST_CAST);
                c->cast->object = ast;
                c->cast->cast_type = parse_type(next_token(), scope);
                ast = c;
            } else {
                ast = make_ast_binop(t->op, ast, parse_expression(next_token(), next_priority + 1, scope));
            }
        } else if (t->type == TOK_LPAREN) {
            ast = parse_arg_list(ast, scope);
        } else if (t->type == TOK_LSQUARE) {
            ast = parse_array_index(ast, scope);
        } else {
            error(lineno(), "Unexpected token '%s' in expression.", to_string(t));
            return NULL;
        }
    }
}

Type *parse_type(Tok *t, AstScope *scope) {
    if (t == NULL) {
        error(lineno(), "Unexpected EOF while parsing type.");
    }

    unsigned char ptr = 0;
    if (t->type == TOK_CARET) {
        ptr = 1;
        t = next_token();
    }

    if (t->type == TOK_ID) {
        Type *type = find_type_by_name(t->sval, scope, NULL);

        if (ptr) {
            type = make_ptr_type(type);
            type = register_type(type);
        }
        return type;
    } else if (t->type == TOK_LSQUARE) {
        Type *type = NULL;
        t = next_token();

        long length = -1;
        int slice = 1;

        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing type.");
        } else if (t->type != TOK_RSQUARE) {
            if (t->type == TOK_INT) {
                length = t->ival;
                slice = 0;
            } else if (t->type == TOK_OP && t->op == OP_MINUS) {
                slice = 0;
            } else {
                error(lineno(), "Unexpected token '%s' while parsing array type.", to_string(t));
            }

            expect(TOK_RSQUARE);
        }

        type = parse_type(next_token(), scope);

        if (slice) {
            type = make_array_type(type);
            type = register_type(type);
        } else {
            type = make_static_array_type(type, length);
            type = register_type(type);
        }

        if (ptr) {
            type = make_ptr_type(type);
            type = register_type(type);
        }

        return type;
    } else if (t->type == TOK_STRUCT) {
        Type *type = parse_struct_type(scope);

        if (ptr) {
            type = make_ptr_type(type);
            type = register_type(type);
        }

        return type;
    } else if (t->type == TOK_FN) {
        if (ptr) {
            error(lineno(), "Cannot make a pointer to a function.");
        }

        TypeList *args = NULL;
        int nargs = 0;
        int variadic = 0;

        expect(TOK_LPAREN);
        t = next_token();

        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing type.");
        } else if (t->type != TOK_RPAREN) {
            for (;;) {
                args = typelist_append(args, parse_type(t, scope));
                nargs++;
                t = next_token();
                if (t == NULL) {
                    error(lineno(), "Unexpected EOF while parsing type.");
                } else if (t->type == TOK_RPAREN) {
                    break;
                } else if (t->type == TOK_ELLIPSIS) {
                    variadic = 1;
                    t = next_token();
                    if (t->type != TOK_RPAREN) {
                        error(lineno(), "Only the last parameter to a function can be variadic.");
                    }
                    break;
                } else if (t->type == TOK_COMMA) {
                    t = next_token();
                } else {
                    error(lineno(), "Unexpected token '%s' while parsing type.", to_string(t));
                }
            }

            args = reverse_typelist(args);
        }
        t = next_token();

        Type *ret = base_type(VOID_T);
        if (t->type == TOK_COLON) {
            ret = parse_type(next_token(), scope);
        } else {
            unget_token(t);
        }

        Type *type = make_fn_type(nargs, args, ret, variadic);
        type = register_type(type);

        return type;
    } else {
        error(lineno(), "Unexpected token '%s' while parsing type.", to_string(t));
    }

    error(lineno(), "Failed to parse type.");
    return NULL;
}

// TODO factor this with parse_func_decl
Ast *parse_extern_func_decl(AstScope *scope) {
    Tok *t = expect(TOK_FN);
    t = expect(TOK_ID);
    char *fname = t->sval;
    expect(TOK_LPAREN);

    Ast *func = ast_alloc(AST_EXTERN_FUNC_DECL); 

    int n = 0;
    int variadic = 0;
    TypeList* arg_types = NULL;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type == TOK_ELLIPSIS) {
            
        }

        Type *argtype = parse_type(t, scope);
        n++;

        t = next_token();
        if (t->type == TOK_ELLIPSIS) {
            variadic = 1;
            t = next_token();
            if (t->type != TOK_RPAREN) {
                error(lineno(), "Only the last parameter to a function can be variadic.");
            }
            arg_types = typelist_append(arg_types, argtype);
            break;
        }

        arg_types = typelist_append(arg_types, argtype);

        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    arg_types = reverse_typelist(arg_types);
    t = next_token();
    Type *ret = base_type(VOID_T);
    if (t->type == TOK_COLON) {
        ret = parse_type(next_token(), scope);
    } else {
        unget_token(t);
    }
    Type *fn_type = make_fn_type(n, arg_types, ret, variadic);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 1;
    func->fn_decl->var = fn_decl_var;
    return func; 
}

Ast *parse_func_decl(AstScope *scope, int anonymous) {
    Tok *t;
    char *fname;
    if (anonymous) {
        int len = snprintf(NULL, 0, "%d", last_tmp_fn_id);
        fname = malloc(sizeof(char) * (len + 1));
        snprintf(fname, len+1, "%d", last_tmp_fn_id++);
        fname[len] = 0;
    } else {
        t = expect(TOK_ID);
        fname = t->sval;
        if (find_local_var(fname, scope) != NULL) {
            error(lineno(), "Declared function name '%s' already exists in this scope.", fname);
        }
    }
    expect(TOK_LPAREN);

    Ast *func = ast_alloc(anonymous ? AST_ANON_FUNC_DECL : AST_FUNC_DECL);
    func->fn_decl->args = NULL;

    AstScope *fn_scope = malloc(sizeof(AstScope));
    fn_scope->locals = NULL;
    fn_scope->parent = scope;
    fn_scope->has_return = 0;
    fn_scope->is_function = 1;

    int n = 0;
    int variadic = 0;
    VarList *args = func->fn_decl->args;
    TypeList *arg_types = NULL;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        if (t->type != TOK_ID) {
            error(lineno(), "Unexpected token (type '%s') in argument list of function declaration '%s'.", token_type(t->type), fname);
        }
        if (find_local_var(t->sval, fn_scope) != NULL) {
            error(lineno(), "Declared variable '%s' already exists.", t->sval);
        }
        expect(TOK_COLON);

        char *argname = t->sval;
        Type *argtype = parse_type(next_token(), scope);
        n++;

        t = next_token();
        if (t->type == TOK_ELLIPSIS) {
            variadic = 1;
            t = next_token();
            if (t->type != TOK_RPAREN) {
                error(lineno(), "Only the last parameter to a function can be variadic.");
            }

            argtype = define_type(make_array_type(argtype), scope);
            args = varlist_append(args, make_var(argname, argtype));
            attach_var(args->item, fn_scope);
            args->item->initialized = 1;
            arg_types = typelist_append(arg_types, argtype->inner);
            break;
        }

        args = varlist_append(args, make_var(argname, argtype));
        attach_var(args->item, fn_scope);
        args->item->initialized = 1;
        arg_types = typelist_append(arg_types, args->item->type);

        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    func->fn_decl->args = reverse_varlist(args);
    arg_types = reverse_typelist(arg_types);
    t = next_token();
    Type *ret = base_type(VOID_T);
    if (t->type == TOK_COLON) {
        ret = parse_type(next_token(), scope);
    } else if (t->type == TOK_LBRACE) {
        unget_token(t);
    } else {
        error(lineno(), "Unexpected token '%s' in function signature.", to_string(t));
    }
    Type *fn_type = make_fn_type(n, arg_types, ret, variadic);
    expect(TOK_LBRACE);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 0;
    fn_decl_var->constant = !anonymous;

    func->fn_decl->anon = anonymous;
    func->fn_decl->var = fn_decl_var;

    if (!anonymous) {
        attach_var(fn_decl_var, scope);
        attach_var(fn_decl_var, fn_scope);
    }

    fn_scope->body = parse_block(fn_scope, 1)->block;
    func->fn_decl->scope = fn_scope;

    /*if (!anonymous) {*/
        /*global_fn_vars = varlist_append(global_fn_vars, fn_decl_var);*/
    /*}*/
    global_fn_decls = astlist_append(global_fn_decls, func);

    return func; 
}

Ast *parse_return_statement(Tok *t, AstScope *scope) {
    Ast *ast = ast_alloc(AST_RETURN);
    ast->ret->expr = NULL;
    ast->ret->scope = scope;
    t = next_token();
    if (t == NULL) {
        error(lineno(), "EOF encountered in return statement.");
    } else if (t->type == TOK_SEMI) {
        unget_token(t);
        return ast;
    }
    ast->ret->expr = parse_expression(t, 0, scope);
    return ast;
}

Ast *parse_type_decl(AstScope *scope) {
    Tok *t = expect(TOK_ID);
    expect(TOK_COLON);

    Ast *ast = ast_alloc(AST_TYPE_DECL);
    ast->type_decl->type_name = t->sval;

    Type *val = make_type(t->sval, AUTO_T, -1);
    int id = val->id;

    Type *tmp = parse_type(next_token(), scope);

    int l = strlen(t->sval);
    char *name = malloc((l + 1) * sizeof(char));
    strncpy(name, t->sval, l);
    name[l] = 0;

    if (tmp->named) {
        *val = *tmp;
        val->name = name;
        val->named = 1;
        val->id = id;
    } else {
        tmp->name = name;
        tmp->named = 1;
        *val = *tmp;
    }
    
    /*fprintf(stderr, "%d, testing %d\n", ast->line, val->id);*/
    ast->type_decl->target_type = define_type(val, scope);
    /*fprintf(stderr, "%d, testing %d\n", ast->line, ast->type_decl->target_type->id);*/
    return ast;
}

Ast *parse_enum_decl(AstScope *scope) {
    Ast *ast = ast_alloc(AST_ENUM_DECL);
    char *name = expect(TOK_ID)->sval;
    Tok *next = next_token();
    Type *inner = base_type(INT_T);
    if (next->type == TOK_COLON) {
        inner = parse_type(next_token(), scope);
        next = next_token();
    }
    if (next->type != TOK_LBRACE) {
        error(lineno(), "Unexpected token '%s' while parsing enum declaration (expected '{').", to_string(next));
    }
    int nmembers = 0;
    int alloc = 8;
    char **names = malloc(sizeof(char*)*alloc);
    Ast **exprs = malloc(sizeof(Ast)*alloc);
    while ((next = next_token())->type != TOK_RBRACE) {
        if (next->type != TOK_ID) {
            error(lineno(), "Unexpected token '%s' while parsing enum declaration (expected an identifier).", to_string(next));
        }
        if (nmembers >= alloc) {
            alloc *= 2;
            names = realloc(names, sizeof(char*)*alloc);
            exprs = realloc(exprs, sizeof(Ast)*alloc);
        }
        names[nmembers] = next->sval;
        next = next_token();
        if (next->type == TOK_OP && next->op == OP_ASSIGN) {
            exprs[nmembers] = parse_expression(next_token(), 0, scope);
            Tok *n = next_token();
            if (n->type == TOK_RBRACE) {
                break;
            } else if (n->type != TOK_COMMA) {
                error(lineno(), "Unexpected token '%s' while parsing enum declaration (expected ',' or '}').", to_string(next));
            }
        } else if (next->type == TOK_COMMA) {
            exprs[nmembers] = NULL;
        } else {
            error(lineno(), "Unexpected token '%s' while parsing enum declaration (expected ',' or '=').", to_string(next));
        }
        nmembers++;
    }
    long *values = malloc(sizeof(long)*nmembers);
    for (TypeList *list = scope->local_types; list != NULL; list = list->next) {
        if (!strcmp(name, list->item->name)) {
            error(lineno(), "Type named '%s' already exists in local scope.", name);
        }
    }
    ast->enum_decl->enum_name = name;
    ast->enum_decl->enum_type = define_type(make_enum_type(name, inner, nmembers, names, values), scope);
    ast->enum_decl->exprs = exprs;
    return ast;
}

Ast *parse_enum_decl_semantics(Ast *ast, AstScope *scope) {
    Type *et = ast->enum_decl->enum_type; 
    Ast **exprs = ast->enum_decl->exprs;

    long val = 0;
    // TODO check for duplicate name, check for duplicate value
    for (int i = 0; i < et->_enum.nmembers; i++) {
        if (exprs[i] != NULL) {
            exprs[i] = parse_semantics(exprs[i], scope);
            // TODO allow const other stuff in here
            if (exprs[i]->type != AST_LITERAL) {
                error(exprs[i]->line, "Cannot initialize enum '%s' member '%s' with non-constant expression.", et->name, et->_enum.member_names[i]);
            } else if (exprs[i]->var_type->base != INT_T) {
                error(exprs[i]->line, "Cannot initialize enum '%s' member '%s' with non-integer expression.", et->name, et->_enum.member_names[i]);
            }
            exprs[i] = coerce_literal(exprs[i], et->_enum.inner);
            val = exprs[i]->lit->int_val;
        }
        et->_enum.member_values[i] = val;
        val += 1;
    }
    ast->var_type = base_type(VOID_T);
    return ast;
}

Type *parse_struct_type(AstScope *scope) {
    expect(TOK_LBRACE);

    int alloc = 6;
    int nmembers = 0;

    char **member_names = malloc(sizeof(char*) * alloc);
    Type **member_types = malloc(sizeof(Type*) * alloc);

    for (;;) {
        if (nmembers >= alloc) {
            alloc += 6;
            member_names = realloc(member_names, sizeof(char*) * alloc);
            member_types = realloc(member_types, sizeof(char*) * alloc);
        }

        Tok *t = expect(TOK_ID);
        expect(TOK_COLON);

        Type *ty = parse_type(next_token(), scope);

        member_names[nmembers] = t->sval;
        member_types[nmembers++] = ty;

        expect(TOK_SEMI);
        t = next_token();

        if (t != NULL && t->type == TOK_RBRACE) {
            break;
        } else {
            unget_token(t);
        }
    }
    for (int i = 0; i < nmembers-1; i++) {
        for (int j = i + 1; j < nmembers; j++) {
            if (!strcmp(member_names[i], member_names[j])) {
                error(lineno(), "Repeat member name '%s' in struct.", member_names[i]);
            }
        }
    }

    Type *st = make_struct_type(NULL, nmembers, member_names, member_types);
    st = register_type(st);
    return st;
}

Ast *parse_statement(Tok *t, AstScope *scope) {
    Ast *ast;
    if (t->type == TOK_ID && peek_token() != NULL && peek_token()->type == TOK_COLON) {
        next_token();
        ast = parse_declaration(t, scope);
    } else if (t->type == TOK_RELEASE) {
        ast = ast_alloc(AST_RELEASE);
        ast->release->object = parse_expression(next_token(), 0, scope);
    } else if (t->type == TOK_HOLD) {
        error(lineno(), "Cannot start a statement with 'hold';");
    } else if (t->type == TOK_TYPE) {
        ast = parse_type_decl(scope);
    } else if (t->type == TOK_ENUM) {
        ast = parse_enum_decl(scope);
    } else if (t->type == TOK_FN) {
        Tok *next = next_token();
        unget_token(next);
        if (next->type == TOK_ID) {
            return parse_func_decl(scope, 0);
        }
        ast = parse_expression(t, 0, scope);
    } else if (t->type == TOK_EXTERN) {
        ast = parse_extern_func_decl(scope);
    } else if (t->type == TOK_LBRACE) {
        return parse_scope(NULL, scope);
    } else if (t->type == TOK_WHILE) {
        ast = ast_alloc(AST_WHILE);
        ast->while_loop->condition = parse_expression(next_token(), 0, scope);
        Tok *next = next_token();
        if (next == NULL || next->type != TOK_LBRACE) {
            error(lineno(), "Unexpected token '%s' while parsing while loop.", to_string(next));
        }
        ast->while_loop->body = parse_scope(NULL, scope)->scope;
        return ast;
    } else if (t->type == TOK_FOR) {
        ast = ast_alloc(AST_FOR);
        Tok *id = expect(TOK_ID);
        ast->for_loop->itervar = make_var(id->sval, make_type("auto", AUTO_T, -1));
        Tok *next = next_token();
        /*if (next->type == TOK_COLON) {*/
            /*Tok *id = expect(TOK_ID);*/

        /*}*/
        if (next->type != TOK_IN) {
            error(ast->line, "Unexpected token '%s' while parsing for loop.", to_string(next));
        }
        ast->for_loop->iterable = parse_expression(next_token(), 0, scope);
        next = next_token();
        // TODO handle empty for body case by trying rollback here
        if (next == NULL || next->type != TOK_LBRACE) {
            error(lineno(), "Unexpected token '%s' while parsing for loop.", to_string(next));
        }
        ast->for_loop->body = parse_scope(NULL, scope)->scope;
        return ast;
    } else if (t->type == TOK_IF) {
        return parse_conditional(scope);
    } else if (t->type == TOK_RETURN) {
        ast = parse_return_statement(t, scope);
    } else if (t->type == TOK_BREAK) {
        ast = ast_alloc(AST_BREAK);
    } else if (t->type == TOK_CONTINUE) {
        ast = ast_alloc(AST_CONTINUE);
    } else {
        ast = parse_expression(t, 0, scope);
    }
    expect(TOK_SEMI);
    return ast;
}

Ast *parse_hold(AstScope *scope) {
    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), "Unexpected end of file while parsing hold expression.");
    }
    Ast *ast = ast_alloc(AST_HOLD);
    ast->hold->object = parse_expression(t, 0, scope);
    return ast;
}

Ast *parse_struct_literal(char *name, AstScope *scope) {
    UNWIND_SET;

    Tok *t = NEXT_TOKEN_UNWINDABLE;

    Ast *ast = ast_alloc(AST_LITERAL);

    ast->lit->lit_type = STRUCT;
    ast->lit->struct_val.name = name;
    ast->lit->struct_val.nmembers = 0;

    if (peek_token()->type == TOK_RBRACE) {
        t = NEXT_TOKEN_UNWINDABLE;
        return ast;
    }

    int alloc = 0;
    for (;;) {
        t = NEXT_TOKEN_UNWINDABLE;

        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_ID) {
            UNWIND_TOKENS;
            /*error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));*/
            return NULL;
        }
        if (ast->lit->struct_val.nmembers >= alloc) {
            alloc += 4;
            ast->lit->struct_val.member_names = realloc(ast->lit->struct_val.member_names, sizeof(char *)*alloc);
            ast->lit->struct_val.member_exprs = realloc(ast->lit->struct_val.member_exprs, sizeof(Ast *)*alloc);
        }
        ast->lit->struct_val.member_names[ast->lit->struct_val.nmembers] = t->sval;
        t = NEXT_TOKEN_UNWINDABLE;
        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_OP || t->op != OP_ASSIGN) {
            UNWIND_TOKENS;
            /*error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));*/
            return NULL;
        }
        ast->lit->struct_val.member_exprs[ast->lit->struct_val.nmembers++] = parse_expression(NEXT_TOKEN_UNWINDABLE, 0, scope);
        t = NEXT_TOKEN_UNWINDABLE;
        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type == TOK_RBRACE) {
            break;
        } else if (t->type != TOK_COMMA) {
            UNWIND_TOKENS;
            // eh?
            /*error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));*/
            return NULL;
        }
    }
    return ast;
}

Ast *parse_directive(Tok *t, AstScope *scope) {
    // TODO handle other types of directives here
    Ast *dir = ast_alloc(AST_DIRECTIVE);
    dir->directive->name = t->sval;
    Tok *next = next_token();
    if (next == NULL) {
        error(lineno(), "Unexpected end of input.");
    }
    if (!strcmp(t->sval, "type")) {
        dir->directive->object = NULL;
        dir->var_type = parse_type(next, scope);
        return dir;
    }
    if (next->type != TOK_LPAREN) {
        error(lineno(), "Unexpected token '%s' while parsing directive '%s'", to_string(next), t->sval);
    }
    dir->directive->object = parse_expression(next_token(), 0, scope);
    expect(TOK_RPAREN);
    return dir;
}

Ast *parse_primary(Tok *t, AstScope *scope) {
    if (t == NULL) {
        error(lineno(), "Unexpected EOF while parsing primary.");
    }
    switch (t->type) {
    case TOK_INT: {
        Ast *ast = ast_alloc(AST_LITERAL);
        ast->lit->lit_type = INTEGER;
        ast->lit->int_val = t->ival;
        return ast;
    }
    case TOK_FLOAT: {
        Ast *ast = ast_alloc(AST_LITERAL);
        ast->lit->lit_type = FLOAT;
        ast->lit->float_val = t->fval;
        return ast;
    }
    case TOK_BOOL:
        return make_ast_bool(t->ival);
    case TOK_STR:
        return make_ast_string(t->sval);
    case TOK_ID: {
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), "Unexpected end of input.");
        }
        unget_token(next);
        if (peek_token()->type == TOK_LBRACE) {
            Ast *ast = parse_struct_literal(t->sval, scope);
            if (ast != NULL) {
                return ast;
            }
        }
        Ast *id = ast_alloc(AST_IDENTIFIER);
        id->ident->var = NULL;
        id->ident->varname = t->sval;
        return id;
    }
    case TOK_DIRECTIVE: 
        return parse_directive(t, scope);
    case TOK_FN:
        return parse_func_decl(scope, 1);
    case TOK_HOLD:
        return parse_hold(scope);
    case TOK_CARET: {
        Ast *ast = ast_alloc(AST_UOP);
        ast->unary->op = OP_ADDR;
        ast->unary->object = parse_expression(next_token(), priority_of(t), scope);
        return ast;
    }
    case TOK_OP: {
        if (!valid_unary_op(t->op)) {
            error(lineno(), "'%s' is not a valid unary operator.", op_to_str(t->op));
        }
        Ast *ast = ast_alloc(AST_UOP);
        ast->unary->op = t->op;
        ast->unary->object = parse_expression(next_token(), priority_of(t), scope);
        return ast;
    }
    case TOK_UOP: {
        Ast *ast = ast_alloc(AST_UOP);
        ast->unary->op = t->op;
        ast->unary->object = parse_expression(next_token(), priority_of(t), scope);
        return ast;
    }
    case TOK_LPAREN: {
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), "Unexpected end of input.");
        }
        Ast *ast = parse_expression(next, 0, scope);
        next = next_token();
        if (next == NULL) {
            error(lineno(), "Unexpected end of input.");
        }
        if (next->type != TOK_RPAREN) {
            error(lineno(), "Unexpected token '%s' encountered while parsing parenthetical expression (starting line %d).", to_string(next), ast->line);
        }
        return ast;
    }
    case TOK_STARTBIND: {
        Ast *b = ast_alloc(AST_BIND);
        b->bind->expr = parse_expression(next_token(), 0, scope);
        expect(TOK_RBRACE);
        return b;
    }
    }
    error(lineno(), "Unexpected token '%s' (primary).", to_string(t));
    return NULL;
}

Ast *parse_conditional(AstScope *scope) {
    Ast *c = ast_alloc(AST_CONDITIONAL);
    c->cond->condition = parse_expression(next_token(), 0, scope);
    Tok *next = next_token();
    if (next == NULL || next->type != TOK_LBRACE) {
        error(lineno(), "Unexpected token '%s' while parsing conditional.", to_string(next));
    }
    c->cond->if_body = parse_block(scope, 1)->block;
    next = next_token();
    if (next != NULL && next->type == TOK_ELSE) {
        next = next_token();
        if (next == NULL) {
            error(lineno(), "Unexpected EOF while parsing conditional.");
        } else if (next->type == TOK_IF) {
            Ast *tmp = parse_conditional(scope);
            c->cond->else_body = malloc(sizeof(AstBlock));
            c->cond->else_body->statements = astlist_append(c->cond->else_body->statements, tmp);
            return c;
        } else if (next->type != TOK_LBRACE) {
            error(lineno(), "Unexpected token '%s' while parsing conditional.", to_string(next));
        }
        c->cond->else_body = parse_block(scope, 1)->block;
    } else {
        c->cond->else_body = NULL;
        unget_token(next);
    }
    return c;
}

Ast *parse_block(AstScope *scope, int bracketed) {
    Ast *b = ast_alloc(AST_BLOCK);
    b->block->startline = lineno();

    AstList *statements = NULL;
    Tok *t;

    for (;;) {
        t = next_token();
        if (t == NULL) {
            if (!bracketed) {
                break;
            } else {
                error(lineno(), "Unexpected token '%s' while parsing statement block.", to_string(t));
            }
        } else if (t->type == TOK_RBRACE) {
            if (bracketed) {
                break;
            } else {
                error(lineno(), "Unexpected token '%s' while parsing statement block.", to_string(t));
            }
        }
        Ast *stmt = parse_statement(t, scope);
        if (b->block->statements == NULL) {
            statements = astlist_append(NULL, stmt);
            b->block->statements = statements;
        } else {
            statements->next = astlist_append(NULL, stmt);
            statements = statements->next;
            // b->block->statements->next = stmt;
        }
        // statements = astlist_append(statements, stmt); 
    }
    b->block->endline = lineno();
    // b->block->statements = reverse_astlist(statements);
    return b;
}

AstScope *new_scope(AstScope *parent) {
    AstScope *scope = malloc(sizeof(AstScope));
    scope->locals = NULL;
    scope->local_types = NULL;
    scope->unresolved_types = NULL;
    scope->parent = parent;
    scope->has_return = 0;
    scope->is_function = 0;
    return scope;
}

Ast *parse_scope(AstScope *scope, AstScope *parent) {
    Ast *s = ast_alloc(AST_SCOPE);
    s->scope = scope == NULL ? new_scope(parent) : scope;
    s->scope->body = parse_block(s->scope, parent == NULL ? 0 : 1)->block;
    return s;
}

AstBlock *parse_block_semantics(AstBlock *block, AstScope *scope, int fn_body) {
    Ast *last = NULL;
    int mainline_return_reached = 0;
    for (AstList *list = block->statements; list != NULL; list = list->next) {
        if (last != NULL && last->type == AST_RETURN) {
            error(list->item->line, "Unreachable statements following return.");
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
        error(block->endline, "Control reaches end of function '%s' without a return statement.",
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
    error(ast->line, "Unrecognized directive '%s'.", n);
    return NULL;
}

Ast *parse_semantics(Ast *ast, AstScope *scope) {
    switch (ast->type) {
    case AST_LITERAL: {
        AstLiteral *lit = ast->lit;
        switch (lit->lit_type) {
        case STRING: 
            ast->var_type = base_type(STRING_T);
            return make_ast_tempvar(ast, make_temp_var(ast->var_type, scope));
        case INTEGER: 
            ast->var_type = base_type(INT_T);
            break;
        case FLOAT: 
            ast->var_type = base_type(FLOAT_T);
            break;
        case BOOL: 
            ast->var_type = base_type(BOOL_T);
            break;
        case STRUCT: {
            // TODO make a tmpvar?
            AstLiteral *lit = ast->lit;
            Type *t = find_type_by_name(lit->struct_val.name, scope, NULL);

            if (t->unresolved) {
                error(ast->line, "Undefined struct type '%s' encountered.", lit->struct_val.name);
            } else if (t->base != STRUCT_T) {
                error(ast->line, "Type '%s' is not a struct.", lit->struct_val.name);
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
                    error(ast->line, "Struct '%s' has no member named '%s'.", t->name, lit->struct_val.member_names[i]);
                }

                Ast *expr = parse_semantics(lit->struct_val.member_exprs[i], scope);
                if (is_dynamic(t->st.member_types[i]) && expr->type != AST_LITERAL) {
                    expr = make_ast_copy(expr);
                }
                lit->struct_val.member_exprs[i] = expr;
            }
            break;
        }
        case ENUM: {
            break;
        }
        }
        break;
    }
    case AST_IDENTIFIER: {
        Var *v = find_var(ast->ident->varname, scope);
        if (v == NULL) {
            error(ast->line, "Undefined identifier '%s' encountered.", ast->ident->varname);
            // TODO better error for an enum here
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
    case AST_SLICE: {
        AstSlice *slice = ast->slice;
        slice->object = parse_semantics(slice->object, scope);
        Type *a = slice->object->var_type;
        if (!is_array(a)) {
            error(ast->line, "Cannot slice non-array type '%s'.", a->name);
        }
        ast->var_type = register_type(make_array_type(a->inner));
        if (slice->offset != NULL) {
            slice->offset = parse_semantics(slice->offset, scope);
            if (slice->offset->type == AST_LITERAL && a->base == STATIC_ARRAY_T) {
                // TODO check that it's an int?
                long o = slice->offset->lit->int_val;
                if (o < 0) {
                    error(ast->line, "Negative slice start is not allowed.");
                } else if (o >= a->length) {
                    error(ast->line, "Slice offset outside of array bounds (offset %ld to array length %ld).", o, a->length);
                }
            }
        }
        if (slice->length != NULL) {
            slice->length = parse_semantics(slice->length, scope);
            if (slice->length->type == AST_LITERAL && a->base == STATIC_ARRAY_T) {
                // TODO check that it's an int?
                long l = slice->length->lit->int_val;
                if (l > a->length) {
                    error(ast->line, "Slice length outside of array bounds (%ld to array length %ld).", l, a->length);
                }
            }
        }
        break;
    }
    case AST_CAST: {
        AstCast *cast = ast->cast;
        cast->object = parse_semantics(cast->object, scope);
        if (cast->object->type == AST_LITERAL) {
            cast->object = coerce_literal(cast->object, cast->cast_type);
        } else {
            if (!can_cast(cast->object->var_type, cast->cast_type)) {
                error(ast->line, "Cannot cast type '%s' to type '%s'.", cast->object->var_type->name, cast->cast_type->name);
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
            error(ast->line, "Struct member release target must be a pointer.");
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
            error(ast->line, "Cannot hold a value of type '%s'.", t->name);
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
    case AST_DECL: {
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
                    error(ast->line, "Cannot implicitly cast float literal '%f' to integer type '%s'.", init->lit->float_val, decl->var->type->name);
                }
                if (b == UINT_T) {
                    if (init->lit->int_val < 0) {
                        error(ast->line, "Cannot assign negative integer literal '%d' to unsigned type '%s'.", init->lit->int_val, decl->var->type->name);
                    }
                    if (precision_loss_uint(decl->var->type, init->lit->int_val)) {
                        error(ast->line, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, decl->var->type->name);
                    }
                } else if (b == INT_T) {
                    if (precision_loss_int(decl->var->type, init->lit->int_val)) {
                        error(ast->line, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->int_val, decl->var->type->name);
                    }
                } else if (b == FLOAT_T) {
                    if (precision_loss_float(decl->var->type, init->lit->lit_type == INTEGER ? init->lit->int_val : init->lit->float_val)) {
                        error(ast->line, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->lit->float_val, decl->var->type->name);
                    }
                } else {
                    error(-1, "wtf");
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
            error(ast->line, "Cannot use type 'auto' for variable '%s' without initialization.", decl->var->name);
        } else if (decl->var->type->base == STATIC_ARRAY_T && decl->var->type->length == -1) {
            error(ast->line, "Cannot use unspecified array type for variable '%s' without initialization.", decl->var->name);
        }

        if (find_local_var(decl->var->name, scope) != NULL) {
            error(ast->line, "Declared variable '%s' already exists.", decl->var->name);
        }

        if (find_builtin_type(decl->var->name) != NULL || local_type_exists(decl->var->name, scope)) {
            error(ast->line, "Variable name '%s' already in use by type.", decl->var->name);
        }

        ast->var_type = base_type(VOID_T);

        if (parser_state == PARSE_MAIN) {
            global_vars = varlist_append(global_vars, decl->var);
            if (decl->init != NULL) {
                Ast *id = ast_alloc(AST_IDENTIFIER);
                id->line = ast->line;
                id->ident->varname = decl->var->name;
                id->ident->var = decl->var;
                id = parse_semantics(id, scope);
                ast = make_ast_assign(id, decl->init);
                ast->line = id->line;
                return ast;
            }
        } else {
            attach_var(ast->decl->var, scope); // should this be after the init parsing?
        }
        break;
    }
    case AST_TYPE_DECL: {
        Var *v = find_local_var(ast->type_decl->type_name, scope);
        if (v != NULL) {
            error(ast->line, "Type name '%s' already exists as variable.", ast->type_decl->type_name);
        }
        if (find_builtin_type(ast->type_decl->type_name) != NULL) {
            error(ast->line, "Cannot shadow builtin type named '%s'.", ast->type_decl->type_name);
        }
        break;
    }
    case AST_EXTERN_FUNC_DECL:
        // TODO fix this
        if (parser_state != PARSE_MAIN) {
            error(ast->line, "Cannot declare an extern inside scope ('%s').", ast->fn_decl->var->name);
        }
        attach_var(ast->fn_decl->var, scope);
        global_fn_vars = varlist_append(global_fn_vars, ast->fn_decl->var);
        break;
    case AST_FUNC_DECL:
        /*if (parser_state != PARSE_MAIN) {*/
            /*error(ast->line, "Cannot declare a named function inside scope ('%s').", ast->fn_decl->var->name);*/
        /*}*/
    case AST_ANON_FUNC_DECL: {
        Var *bindings_var = NULL;
        Type *fn_t = ast->fn_decl->var->type;

        if (ast->type == AST_ANON_FUNC_DECL) {
            bindings_var = malloc(sizeof(Var));
            bindings_var->name = "";
            bindings_var->type = base_type(BASEPTR_T);
            bindings_var->id = new_var_id();
            bindings_var->temp = 1;
            bindings_var->consumed = 0;
            fn_t->bindings_id = bindings_var->id;
            ast->var_type = fn_t;
        }

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

        if (fn_t->bindings != NULL) {
            global_fn_bindings = varlist_append(global_fn_bindings, bindings_var);
        }
        break;
    }
    case AST_CALL: {
        Ast *arg;
        ast->call->fn = parse_semantics(ast->call->fn, scope);
        Type *t = ast->call->fn->var_type;
        if (t->base != FN_T) {
            error(ast->line, "Cannot perform call on non-function type '%s'", t->name);
        }
        if (ast->call->fn->type == AST_ANON_FUNC_DECL) {
            ast->call->fn = make_ast_tempvar(ast->call->fn, make_temp_var(t, scope));
        }
        if (t->fn.variadic) {
            if (ast->call->nargs < t->fn.nargs - 1) {
                error(ast->line, "Expected at least %d arguments to variadic function, but only got %d.", t->fn.nargs-1, ast->call->nargs);
            } else {
                TypeList *list = t->fn.args;
                Type *a = list->item;
                while (list != NULL) {
                    a = list->item;
                    list = list->next;
                }
                a = make_static_array_type(a, ast->call->nargs - (t->fn.nargs - 1));;
                if (!(ast->call->fn->type == AST_IDENTIFIER && ast->call->fn->ident->var)) {
                    ast->call->variadic_tempvar = make_temp_var(a, scope);
                }
            }
        } else if (ast->call->nargs != t->fn.nargs) {
            error(ast->line, "Incorrect argument count to function (expected %d, got %d)", t->fn.nargs, ast->call->nargs);
        }
        AstList *args = ast->call->args;
        TypeList *arg_types = t->fn.args;
        for (int i = 0; args != NULL; i++) {
            arg = args->item;
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
        break;
    }
    case AST_INDEX: {
        ast->index->object = parse_semantics(ast->index->object, scope);
        ast->index->index = parse_semantics(ast->index->index, scope);
        Type *obj_type = ast->index->object->var_type;
        Type *ind_type = ast->index->index->var_type;
        /*if (!(is_array(l) || (l->base == PTR_T && is_array(l->inner)))) {*/
        if (!is_array(obj_type)) {
            error(ast->index->object->line, "Cannot perform index/subscript operation on non-array type (type is '%s').", obj_type->name);
        }
        if (ind_type->base != INT_T && ind_type->base != UINT_T) {
            error(ast->line, "Cannot index array with non-integer type '%s'.", ind_type->name);
        }
        int _static = obj_type->base == STATIC_ARRAY_T; // || (l->base == PTR_T && l->inner->base == STATIC_ARRAY_T);
        if (ast->index->index->type == AST_LITERAL) {
            int i = ast->index->index->lit->int_val;
            // ind must be integer
            if (i < 0) {
                error(ast->line, "Negative array index is larger than array length (%ld vs length %ld).", i, array_size(obj_type));
            } else if (_static && i >= array_size(obj_type)) {
                error(ast->line, "Array index is larger than array length (%ld vs length %ld).", i, array_size(obj_type));
            }
            ast->index->index->lit->int_val = i; // ?
        }
        ast->var_type = obj_type->inner; // need to call something different?
        break;
    }
    case AST_CONDITIONAL: {
        AstConditional *c = ast->cond;
        c->condition = parse_semantics(c->condition, scope);
        if (c->condition->var_type->base != BOOL_T) {
            error(ast->line, "Non-boolean ('%s') condition for if statement.", c->condition->var_type->name);
        }
        c->if_body = parse_block_semantics(c->if_body, scope, 0);
        if (c->else_body != NULL) {
            c->else_body = parse_block_semantics(c->else_body, scope, 0);
        }
        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_WHILE: {
        AstWhile *lp = ast->while_loop;
        lp->condition = parse_semantics(lp->condition, scope);

        if (lp->condition->var_type->base != BOOL_T) {
            error(ast->line, "Non-boolean ('%s') condition for while loop.", lp->condition->var_type->name);
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
        if (!is_array(it_type)) {
            error(ast->line, "Cannot use for loop to iterator over non-array type '%s'.", it_type->name);
        }
        lp->itervar->type = it_type->inner;
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
            error(ast->line, "Cannot make bindings outside of an inner function."); 
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
            error(ast->line, "Return statement outside of function body.");
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
            error(ast->line, "Cannot return a static array from a function.");
        }
        if (fn_ret_t->base == AUTO_T) {
            current_fn_scope->fn_decl->var->type->fn.ret = ret_t;
        } else if (!check_type(fn_ret_t, ret_t)) {
            if (ast->ret->expr->type == AST_LITERAL) {
                ast->ret->expr = try_implicit_cast(fn_ret_t, ast->ret->expr);
            } else {
                error(ast->line, "Return statement type '%s' does not match enclosing function's return type '%s'.", ret_t->name, fn_ret_t->name);
            }
        }
        ast->var_type = base_type(VOID_T);
        break;
    }
    case AST_BREAK:
        if (!loop_state) {
            error(ast->line, "Break statement outside of loop.");
        }
        ast->var_type = base_type(VOID_T);
        break;
    case AST_CONTINUE:
        if (!loop_state) {
            error(ast->line, "Continue statement outside of loop.");
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
        error(-1, "idk parse semantics");
    }
    return ast;
}

AstList *get_global_funcs() {
    return global_fn_decls;
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

void init_builtins() {
    Var *v = make_var("assert", make_fn_type(1, typelist_append(NULL, base_type(BOOL_T)), base_type(VOID_T), 0));
    v->ext = 1;
    v->constant = 1;
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("print_str", make_fn_type(1, typelist_append(NULL, base_type(STRING_T)), base_type(VOID_T), 0));
    v->ext = 1;
    v->constant = 1;
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("println", make_fn_type(1, typelist_append(NULL, base_type(STRING_T)), base_type(VOID_T), 0));
    v->ext = 1;
    v->constant = 1;
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("itoa", make_fn_type(1, typelist_append(NULL, base_type(INT_T)), base_type(STRING_T), 0));
    v->ext = 1;
    v->constant = 1;
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("validptr", make_fn_type(1, typelist_append(NULL, base_type(BASEPTR_T)), base_type(BOOL_T), 0));
    v->ext = 1;
    v->constant = 1;
    builtin_vars = varlist_append(builtin_vars, v);
}

AstList *get_binding_exprs(int id) {
    AstListList *l = binding_exprs;
    while (l != NULL) {
        if (l->id == id) {
            return l->item;
        }
        l = l->next;
    }
    error(-1, "Couldn't find binding exprs %d.", id);
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
