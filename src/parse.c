#include "parse.h"

#define PUSH_FN_SCOPE(x) \
    Ast *__old_fn_scope = current_fn_scope;\
    current_fn_scope = (x);
#define POP_FN_SCOPE() current_fn_scope = __old_fn_scope;

static int last_tmp_fn_id = 0;
static VarList *global_vars = NULL;
static VarList *global_fn_vars = NULL;
static VarList *builtin_vars = NULL;
static AstList *global_fn_decls = NULL;
static VarList *global_fn_bindings = NULL;
static TypeList *global_struct_decls = NULL;
static TypeList *global_hold_funcs = NULL;
static AstListList *binding_exprs = NULL;
static int parser_state = PARSE_MAIN; // TODO unnecessary w/ PUSH_FN_SCOPE?
static int loop_state = 0;
static int in_decl = 0;
static Ast *current_fn_scope = NULL;

void print_locals(Ast *scope) {
    VarList *v = scope->locals;
    printf("locals: ");
    while (v != NULL) {
        printf("%s ", v->item->name);
        v = v->next;
    }
    printf("\n");
}

Var *make_temp_var(Type *type, Ast *scope) {
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

void attach_var(Var *var, Ast *scope) {
    scope->locals = varlist_append(scope->locals, var);
}

void detach_var(Var *var, Ast *scope) {
    scope->locals = varlist_remove(scope->locals, var->name);
}

void release_var(Var *var, Ast *scope) {
    scope->locals = varlist_remove(scope->locals, var->name);
}

Var *find_var(char *name, Ast *scope) {
    Var *v = find_local_var(name, scope);
    if (v == NULL && !scope->is_function && scope->parent != NULL) {
        v = find_var(name, scope->parent);
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

Var *find_local_var(char *name, Ast *scope) {
    return varlist_find(scope->locals, name);
}

Ast *parse_uop_semantics(Ast *ast, Ast *scope) {
    ast->right = parse_semantics(ast->right, scope);
    switch (ast->op) {
    case OP_NOT:
        if (var_type(ast->right)->base != BOOL_T) {
            error(ast->line, "Cannot perform logical negation on type '%s'.", var_type(ast->right)->name);
        }
        break;
    case OP_DEREF: // TODO precedence is wrong, see @x.data
        if (var_type(ast->right)->base != PTR_T) {
            error(ast->line, "Cannot dereference a non-pointer type.");
        }
        break;
    case OP_ADDR:
        if (ast->right->type != AST_IDENTIFIER && ast->right->type != AST_DOT) {
            error(ast->line, "Cannot take the address of a non-variable.");
        }
        break;
    case OP_MINUS:
    case OP_PLUS: {
        Type *t = var_type(ast->right);
        if (!is_numeric(t)) { // TODO try implicit cast to base type
            error(ast->line, "Cannot perform '%s' operation on non-numeric type '%s'.", op_to_str(ast->op), t->name);
        }
        break;
    }
    default:
        error(ast->line, "Unknown unary operator '%s' (%d).", op_to_str(ast->op), ast->op);
    }
    if (is_literal(ast->right)) {
        if (ast->type == AST_TEMP_VAR) {
            detach_var(ast->tmpvar, scope);
        }
        return eval_const_uop(ast);
    }
    if (is_dynamic(var_type(ast->right))) {
        if (ast->op == OP_DEREF) {
            if (ast->right->type != AST_TEMP_VAR) {
                ast->right = make_ast_tmpvar(ast->right, make_temp_var(var_type(ast->right), scope));
            }
            Ast *tmp = make_ast_tmpvar(ast, ast->right->tmpvar);
            return tmp;
        }
    }
    return ast;
}

Ast *parse_dot_op_semantics(Ast *ast, Ast *scope) {
    ast->dot_left = parse_semantics(ast->dot_left, scope);
    Type *t = var_type(ast->dot_left);
    if (is_array(t) || (t->base == PTR_T && is_array(t->inner))) {
        if (strcmp(ast->member_name, "length") && strcmp(ast->member_name, "data")) {
            error(ast->line, "Cannot dot access member '%s' on array (only length or data).", ast->member_name);
        }
    } else if (t->base != STRUCT_T && !(t->base == PTR_T && t->inner->base == STRUCT_T)) {
        error(ast->line, "Cannot use dot operator on non-struct type '%s'.", var_type(ast->dot_left)->name);
    }
    return ast;
}

Ast *parse_assignment_semantics(Ast *ast, Ast *scope) {
    ast->left = parse_semantics(ast->left, scope);
    ast->right = parse_semantics(ast->right, scope);

    if (!is_lvalue(ast->left)) {
        error(ast->line, "LHS of assignment is not an lvalue.");
    }
    if (!(check_type(var_type(ast->left), var_type(ast->right)) || type_can_coerce(var_type(ast->right), var_type(ast->left)))) {
        error(ast->line, "LHS of assignment has type '%s', while RHS has type '%s'.",
                ast->left->var->type->name, var_type(ast->right)->name);
    }
    if (ast->right->type != AST_TEMP_VAR && is_dynamic(var_type(ast->left)) && !is_literal(ast->right)) {
        ast->right = make_ast_tmpvar(ast->right, make_temp_var(var_type(ast->right), scope));
    }
    return ast;
}

Ast *parse_binop_semantics(Ast *ast, Ast *scope) {
    ast->left = parse_semantics(ast->left, scope);
    ast->right = parse_semantics(ast->right, scope);

    Type *lt = var_type(ast->left);
    Type *rt = var_type(ast->right);

    switch (ast->op) {
    case OP_PLUS:
        if (!(is_numeric(lt) && is_numeric(rt)) && !(lt->base == STRING_T && rt->base == STRING_T)) {
            error(ast->line, "Operator '%s' is valid only for numeric or string arguments, not for type '%s'.",
                    op_to_str(ast->op), lt->name);
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
                    op_to_str(ast->op), lt->name);
        } else if (!is_numeric(rt)) {
            error(ast->line, "RHS of operator '%s' has invalid non-numeric type '%s'.",
                    op_to_str(ast->op), rt->name);
        }
        break;
    case OP_AND:
    case OP_OR:
        if (lt->base != BOOL_T) {
            error(ast->line, "Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(ast->op), lt->name);
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        if (!type_equality_comparable(lt, rt)) {
            error(ast->line, "Cannot compare equality of non-comparable types '%s' and '%s'.", lt->name, rt->name);
        }
        break;
    }
    if (is_literal(ast->left) && is_literal(ast->right)) {
        Type *t = var_type(ast);
        // TODO tmpvars are getting left behind when const string binops resolve
        if (ast->left->type == AST_TEMP_VAR) {
            detach_var(ast->left->tmpvar, scope);
        }
        if (ast->right->type == AST_TEMP_VAR) {
            detach_var(ast->right->tmpvar, scope);
        }
        ast = eval_const_binop(ast);
        if (is_dynamic(t)) {
            ast = make_ast_tmpvar(ast, make_temp_var(t, scope));
        }
        return ast;
    }
    if (!is_comparison(ast->op) && is_dynamic(lt)) {
        return make_ast_tmpvar(ast, make_temp_var(lt, scope));
    }
    return ast;
}

Ast *parse_array_slice(Ast *inner, Ast *offset, Ast *scope) {
    Ast *slice = ast_alloc(AST_SLICE);
    slice->slice_inner = inner;
    slice->slice_offset = offset;
    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), "Unexpected EOF while parsing array slice.");
    } else if (t->type == TOK_RSQUARE) {
        slice->slice_length = NULL;
    } else {
        slice->slice_length = parse_expression(t, 0, scope);
        expect(TOK_RSQUARE);
    }
    return slice; 
}

Ast *parse_array_index(Ast *left, Ast *scope) {
    Tok *t = next_token();
    if (t->type == TOK_COLON) {
        return parse_array_slice(left, NULL, scope);
    }
    Ast *ind = ast_alloc(AST_INDEX);
    ind->left = left;
    ind->right = parse_expression(t, 0, scope);
    t = next_token();
    if (t->type == TOK_COLON) {
        Ast *offset = ind->right;
        free(ind);
        return parse_array_slice(left, offset, scope);
    } else if (t->type != TOK_RSQUARE) {
        error(lineno(), "Unexpected token '%s' while parsing array index.", to_string(t));
    }
    return ind; 
}

Ast *parse_arg_list(Ast *left, Ast *scope) {
    Ast *func = ast_alloc(AST_CALL);
    func->args = NULL;
    func->fn = left;
    int n = 0;
    Tok *t;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        func->args = astlist_append(func->args, parse_expression(t, 0, scope));
        n++;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    func->args = reverse_astlist(func->args);
    func->nargs = n;
    return func; 
}

Ast *parse_declaration(Tok *t, Ast *scope) {
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
        lhs->init = parse_expression(next_token(), 0, scope);
    } else if (next->type != TOK_SEMI) {
        error(lineno(), "Unexpected token '%s' while parsing declaration.", to_string(next));
    } else {
        unget_token(next);
    }
    return lhs; 
}

Ast *parse_expression(Tok *t, int priority, Ast *scope) {
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
                ast = make_ast_assign(ast, parse_expression(next_token(), next_priority + 1, scope));
            } else if (t->op == OP_DOT) {
                Tok *next = expect(TOK_ID);
                ast = make_ast_dot_op(ast, next->sval);
                ast->type = AST_DOT;
            } else if (t->op == OP_CAST) {
                Ast *cast = ast_alloc(AST_CAST);
                cast->cast_left = ast;
                cast->cast_type = parse_type(next_token(), scope);
                ast = cast;
            } else {
                ast = make_ast_binop(t->op, ast, parse_expression(next_token(), next_priority + 1, scope));
            }
        } else if (t->type == TOK_LPAREN) {
            ast = parse_arg_list(ast, scope);
        } else if (t->type == TOK_LSQUARE) {
            ast = parse_array_index(ast, scope);
        } else {
            error(lineno(), "Unexpected token '%s'.", to_string(t));
            return NULL;
        }
    }
}

Type *parse_type(Tok *t, Ast *scope) {
    if (t == NULL) {
        error(lineno(), "Unexpected EOF while parsing type.");
    }
    int parens = 0;
    while (t->type == TOK_LPAREN) {
        parens++;
        t = next_token();
    }
    unsigned char ptr = 0;
    if (t->type == TOK_CARET) {
        ptr = 1;
        t = next_token();
    }
    if (t->type == TOK_ID) {
        Type *type = find_type_by_name(t->sval);
        if (type == NULL) {
            error(lineno(), "Unknown type '%s'.", t->sval);
        }
        if (ptr) {
            type = make_ptr_type(type);
        }
        while (parens--) {
            expect(TOK_RPAREN);
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
        } else {
            type = make_static_array_type(type, length);
        }
        if (ptr) {
            type = make_ptr_type(type);
        }
        while (parens--) {
            expect(TOK_RPAREN);
        }
        return type;
    } else if (t->type == TOK_STRUCT) {
        Type *type = parse_struct_type(scope);
        if (ptr) {
            type = make_ptr_type(type);
        }
        while (parens--) {
            expect(TOK_RPAREN);
        }
        return type;
    } else if (t->type == TOK_FN) {
        if (ptr) {
            error(lineno(), "Cannot make a pointer to a function.");
        }
        TypeList *args = NULL;
        int nargs = 0;
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
        while (parens--) {
            expect(TOK_RPAREN);
        }
        return make_fn_type(nargs, args, ret);
    } else {
        error(lineno(), "Unexpected token '%s' while parsing type.", to_string(t));
    }
    error(lineno(), "Failed to parse type.");
    return NULL;
}

Ast *parse_extern_func_decl(Ast *scope) {
    Tok *t = expect(TOK_FN);
    t = expect(TOK_ID);
    char *fname = t->sval;
    expect(TOK_LPAREN);

    Ast *func = ast_alloc(AST_EXTERN_FUNC_DECL); 

    int n = 0;
    TypeList* arg_types = NULL;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        arg_types = typelist_append(arg_types, parse_type(t, scope));
        n++;
        t = next_token();
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
    Type *fn_type = make_fn_type(n, arg_types, ret);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 1;
    func->fn_decl_var = fn_decl_var;
    return func; 
}

Ast *parse_func_decl(Ast *scope, int anonymous) {
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
    func->fn_decl_args = NULL;

    Ast *fn_scope = ast_alloc(AST_SCOPE);
    fn_scope->locals = NULL;
    fn_scope->parent = scope;
    fn_scope->has_return = 0;
    fn_scope->is_function = 1;

    int n = 0;
    TypeList* arg_types = NULL;
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
        func->fn_decl_args = varlist_append(func->fn_decl_args, make_var(t->sval, parse_type(next_token(), scope)));
        attach_var(func->fn_decl_args->item, fn_scope);
        func->fn_decl_args->item->initialized = 1;
        arg_types = typelist_append(arg_types, func->fn_decl_args->item->type);
        n++;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    func->fn_decl_args = reverse_varlist(func->fn_decl_args);
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
    Type *fn_type = make_fn_type(n, arg_types, ret);
    expect(TOK_LBRACE);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 0;

    func->anon = anonymous;
    func->fn_decl_var = fn_decl_var;

    fn_scope->body = parse_block(fn_scope, 1);
    func->fn_body = fn_scope;

    if (!anonymous) {
        global_fn_vars = varlist_append(global_fn_vars, fn_decl_var);
    }
    global_fn_decls = astlist_append(global_fn_decls, func);

    return func; 
}

Ast *parse_return_statement(Tok *t, Ast *scope) {
    Ast *ast = ast_alloc(AST_RETURN);
    ast->ret_expr = NULL;
    ast->fn_scope = scope;
    t = next_token();
    if (t == NULL) {
        error(lineno(), "EOF encountered in return statement.");
    } else if (t->type == TOK_SEMI) {
        unget_token(t);
        return ast;
    }
    ast->ret_expr = parse_expression(t, 0, scope);
    return ast;
}

Ast *parse_type_decl(Ast *scope) {
    Tok *t = expect(TOK_ID);
    expect(TOK_COLON);

    Ast *ast = ast_alloc(AST_TYPE_DECL);
    ast->type_name = t->sval;
    Type *val = make_type(t->sval, AUTO_T, -1);
    int id = val->id;
    ast->target_type = define_type(val);
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
    return ast;
}

Type *parse_struct_type(Ast *scope) {
    expect(TOK_LBRACE);
    int alloc = 6;
    int nmembers = 0;
    char **member_names = malloc(sizeof(char*) * alloc);
    Type **member_types = malloc(sizeof(Type*) * alloc);
    // add name to vars before parse_type's
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
    TypeList *decl = global_struct_decls;
    while (decl != NULL) {
        if (check_type(decl->item, st)) {
            return decl->item;
        }
        decl = decl->next;
    }
    global_struct_decls = typelist_append(global_struct_decls, st);
    return st;
}

Ast *parse_statement(Tok *t, Ast *scope) {
    Ast *ast;
    if (t->type == TOK_ID && peek_token() != NULL && peek_token()->type == TOK_COLON) {
        next_token();
        ast = parse_declaration(t, scope);
    } else if (t->type == TOK_RELEASE) {
        ast = ast_alloc(AST_RELEASE);
        ast->release_target = parse_expression(next_token(), 0, scope);
    } else if (t->type == TOK_HOLD) {
        error(lineno(), "Cannot start a statement with 'hold';");
    } else if (t->type == TOK_TYPE) {
        ast = parse_type_decl(scope);
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
        return parse_scope(scope);
    } else if (t->type == TOK_WHILE) {
        ast = ast_alloc(AST_WHILE);
        ast->while_condition = parse_expression(next_token(), 0, scope);
        Tok *next = next_token();
        if (next == NULL || next->type != TOK_LBRACE) {
            error(lineno(), "Unexpected token '%s' while parsing while loop.", to_string(next));
        }
        ast->while_body = parse_scope(scope);
        return ast;
    } else if (t->type == TOK_FOR) {
        ast = ast_alloc(AST_FOR);
        Tok *id = expect(TOK_ID);
        ast->for_itervar = make_var(id->sval, make_type("auto", AUTO_T, -1));
        Tok *next = next_token();
        /*if (next->type == TOK_COLON) {*/
            /*Tok *id = expect(TOK_ID);*/

        /*}*/
        if (next->type != TOK_IN) {
            error(ast->line, "Unexpected token '%s' while parsing for loop.", to_string(next));
        }
        ast->for_iterable = parse_expression(next_token(), 0, scope);
        next = next_token();
        // TODO handle empty for body case by trying rollback here
        if (next == NULL || next->type != TOK_LBRACE) {
            error(lineno(), "Unexpected token '%s' while parsing for loop.", to_string(next));
        }
        ast->for_body = parse_scope(scope);
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

Ast *parse_hold(Ast *scope) {
    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), "Unexpected end of file while parsing hold expression.");
    }
    Ast *ast = ast_alloc(AST_HOLD);
    ast->expr = parse_expression(t, 0, scope);
    return ast;
}

Ast *parse_struct_literal(char *name, Ast *scope) {
    UNWIND_SET;

    Tok *t = NEXT_TOKEN_UNWINDABLE;
    Ast *ast = ast_alloc(AST_STRUCT);
    int alloc = 0;
    ast->struct_lit_name = name;
    ast->nmembers = 0;
    if (peek_token()->type == TOK_RBRACE) {
        t = NEXT_TOKEN_UNWINDABLE;
        return ast;
    }
    for (;;) {
        t = NEXT_TOKEN_UNWINDABLE;
        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_ID) {
            UNWIND_TOKENS;
            /*error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));*/
            return NULL;
        }
        if (ast->nmembers >= alloc) {
            alloc += 4;
            ast->member_names = realloc(ast->member_names, sizeof(char *)*alloc);
            ast->member_exprs = realloc(ast->member_exprs, sizeof(Ast *)*alloc);
        }
        ast->member_names[ast->nmembers] = t->sval;
        t = NEXT_TOKEN_UNWINDABLE;
        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_OP || t->op != OP_ASSIGN) {
            UNWIND_TOKENS;
            /*error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));*/
            return NULL;
        }
        ast->member_exprs[ast->nmembers++] = parse_expression(NEXT_TOKEN_UNWINDABLE, 0, scope);
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

Ast *parse_primary(Tok *t, Ast *scope) {
    if (t == NULL) {
        error(lineno(), "Unexpected EOF while parsing primary.");
    }
    switch (t->type) {
    case TOK_INT: {
        Ast *ast = ast_alloc(AST_INTEGER);
        ast->ival = t->ival;
        return ast;
    }
    case TOK_FLOAT: {
        Ast *ast = ast_alloc(AST_FLOAT);
        ast->fval = t->fval;
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
        id->var = NULL;
        id->varname = t->sval;
        return id;
    }
    case TOK_FN:
        return parse_func_decl(scope, 1);
    case TOK_HOLD:
        return parse_hold(scope);
    case TOK_CARET: {
        Ast *ast = ast_alloc(AST_UOP);
        ast->op = OP_ADDR;
        ast->left = NULL;
        ast->right = parse_expression(next_token(), priority_of(t), scope);
        return ast;
    }
    case TOK_OP: {
        if (!valid_unary_op(t->op)) {
            error(lineno(), "'%s' is not a valid unary operator.", op_to_str(t->op));
        }
        Ast *ast = ast_alloc(AST_UOP);
        ast->op = t->op;
        ast->left = NULL;
        ast->right = parse_expression(next_token(), priority_of(t), scope);
        return ast;
    }
    case TOK_UOP: {
        Ast *ast = ast_alloc(AST_UOP);
        ast->op = t->op;
        ast->left = NULL;
        ast->right = parse_expression(next_token(), priority_of(t), scope);
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
        Ast *binding = ast_alloc(AST_BIND);
        binding->bind_expr = parse_expression(next_token(), 0, scope);
        expect(TOK_RBRACE);
        return binding;
    }
    }
    error(lineno(), "Unexpected token '%s'.", to_string(t));
    return NULL;
}

Ast *parse_conditional(Ast *scope) {
    Ast *cond = ast_alloc(AST_CONDITIONAL);
    cond->condition = parse_expression(next_token(), 0, scope);
    Tok *next = next_token();
    if (next == NULL || next->type != TOK_LBRACE) {
        error(lineno(), "Unexpected token '%s' while parsing conditional.", to_string(next));
    }
    cond->if_body = parse_block(scope, 1);
    next = next_token();
    if (next != NULL && next->type == TOK_ELSE) {
        next = next_token();
        if (next == NULL) {
            error(lineno(), "Unexpected EOF while parsing conditional.");
        } else if (next->type == TOK_IF) {
            cond->else_body = parse_conditional(scope);
            return cond;
        } else if (next->type != TOK_LBRACE) {
            error(lineno(), "Unexpected token '%s' while parsing conditional.", to_string(next));
        }
        cond->else_body = parse_block(scope, 1);
    } else {
        cond->else_body = NULL;
        unget_token(next);
    }
    return cond;
}

Ast *parse_block(Ast *scope, int bracketed) {
    Ast *block = ast_alloc(AST_BLOCK);
    int n = 20;
    Ast **statements = malloc(sizeof(Ast*)*n);
    Tok *t;
    int i = 0;
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
        if (i >= n) {
            n *= 2;
            statements = realloc(statements, sizeof(Ast*)*n);
        }
        Ast *stmt = parse_statement(t, scope);
        if (stmt->type != AST_TYPE_DECL) {
            statements[i++] = stmt;
        }
    }
    block->num_statements = i;
    block->statements = statements;
    return block;
}

Ast *parse_scope(Ast *parent) {
    Ast *scope = ast_alloc(AST_SCOPE);
    scope->locals = NULL;
    scope->parent = parent;
    scope->has_return = 0;
    scope->is_function = 0;
    scope->body = parse_block(scope, parent == NULL ? 0 : 1);
    return scope;
}

Ast *parse_semantics(Ast *ast, Ast *scope) {
    switch (ast->type) {
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_BOOL:
        break;
    case AST_STRING:
        return make_ast_tmpvar(ast, make_temp_var(base_type(STRING_T), scope));
    case AST_DOT:
        return parse_dot_op_semantics(ast, scope);
    case AST_ASSIGN:
        return parse_assignment_semantics(ast, scope);
    case AST_BINOP:
        return parse_binop_semantics(ast, scope);
    case AST_UOP:
        return parse_uop_semantics(ast, scope);
    case AST_IDENTIFIER: {
        Var *v = find_var(ast->varname, scope);
        if (v == NULL) {
            error(ast->line, "Undefined identifier '%s' encountered.", ast->varname);
        }
        ast->var = v;
        break;
    }
    case AST_SLICE: {
        ast->slice_inner = parse_semantics(ast->slice_inner, scope);
        Type *a = var_type(ast->slice_inner);
        if (!is_array(a)) {
            error(ast->line, "Cannot slice non-array type '%s'.", a->name);
        }
        if (ast->slice_offset != NULL) {
            ast->slice_offset = parse_semantics(ast->slice_offset, scope);
            if (is_literal(ast->slice_offset) && a->base == STATIC_ARRAY_T) {
                long o = ast->slice_offset->ival;
                if (o < 0) {
                    error(ast->line, "Negative slice start is not allowed.");
                } else if (o >= a->length) {
                    error(ast->line, "Slice offset outside of array bounds (offset %ld to array length %ld).", o, a->length);
                }
            }
        }
        if (ast->slice_length != NULL) {
            ast->slice_length = parse_semantics(ast->slice_length, scope);
            if (is_literal(ast->slice_length) && a->base == STATIC_ARRAY_T) {
                long l = ast->slice_length->ival;
                if (l > a->length) {
                    error(ast->line, "Slice length outside of array bounds (%ld to array length %ld).", l, a->length);
                }
            }
        }
        break;
    }
    case AST_CAST:
        ast->cast_left = parse_semantics(ast->cast_left, scope);
        if (!can_cast(var_type(ast->cast_left), ast->cast_type)) {
            error(ast->line, "Cannot cast type '%s' to type '%s'.", var_type(ast->cast_left)->name, ast->cast_type->name);
        }
        break;
    case AST_STRUCT: {
        Type *t = find_type_by_name(ast->struct_lit_name);
        if (t == NULL) {
            error(ast->line, "Undefined struct type '%s' encountered.", ast->struct_lit_name);
        } else if (t->base != STRUCT_T) {
            error(ast->line, "Type '%s' is not a struct.", ast->struct_lit_name);
        }
        ast->struct_lit_type = t;
        for (int i = 0; i < ast->nmembers; i++) {
            int found = 0;
            for (int j = 0; j < t->nmembers; j++) {
                if (!strcmp(ast->member_names[i], t->member_names[j])) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                error(ast->line, "Struct '%s' has no member named '%s'.", t->name, ast->member_names[i]);
            }
            ast->member_exprs[i] = parse_semantics(ast->member_exprs[i], scope);
            if (is_dynamic(t->member_types[i]) && !is_literal(ast->member_exprs[i])) {
                ast->member_exprs[i] = make_ast_copy(ast->member_exprs[i]);
            }
        }
        break;
    }
    case AST_RELEASE: {
        ast->release_target = parse_semantics(ast->release_target, scope);
        Type *t = var_type(ast->release_target);
        if (t->base != PTR_T && t->base != BASEPTR_T && t->base != ARRAY_T) {
            error(ast->line, "Struct member release target must be a pointer.");
        /*} else {*/
            /*error(ast->line, "Unexpected target of release statement (must be variable or dot op).");*/
        }
        if (ast->release_target->type == AST_IDENTIFIER) {
            // TODO is held even being set?
            /*if (!t->held && t->base != PTR_T && t->base != BASEPTR_T) {*/
                /*error(ast->line, "Cannot release the non-held variable '%s'.", ast->release_target->var->name);*/
            /*}*/
            // TODO instead mark that var has been released for better errors in
            // the future
            release_var(ast->release_target->var, scope);
        /*} else if (ast->release_target->type == AST_DOT) {*/
            /*if (t->base != PTR_T && t->base != BASEPTR_T) {*/
                /*error(ast->line, "Struct member release target must be a pointer.");*/
            /*}*/
        }
        break;
    }
    case AST_HOLD: {
        ast->expr = parse_semantics(ast->expr, scope);
        Type *t = var_type(ast->expr);
        if (t->base == VOID_T || t->base == FN_T) {
            error(ast->line, "Cannot hold a value of type '%s'.", t->name);
        }

        if (ast->expr->type != AST_TEMP_VAR && is_dynamic(t)) {
            ast->expr = make_ast_tmpvar(ast->expr, make_temp_var(t, scope));
        }
        Type *tp = NULL;
        if (t->base == STATIC_ARRAY_T) {
            // If the static array has a dynamic inner type, we need to add the
            // type to the list of those for which "hold" functions will be
            // created
            // This is a bit of a hack, specifically for the c backed...
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
            tp = make_array_type(t->inner);
        } else {
            tp = make_ptr_type(t);
        }
        tp->held = 1; // eh?

        ast->tmpvar = make_temp_var(tp, scope);
        break;
    }
    case AST_DECL: {
        Ast *init = ast->init;
        if (init != NULL) {
            if (init->type != AST_ANON_FUNC_DECL) {
                in_decl = 1;
            }
            init = parse_semantics(init, scope);
            in_decl = 0;
            if (ast->decl_var->type->base == AUTO_T) {
                ast->decl_var->type = var_type(init);
            } else if (ast->decl_var->type->length == -1) {
                ast->decl_var->type->length = var_type(init)->length;
            }
            if (is_literal(init) && is_numeric(ast->decl_var->type)) {
                int b = ast->decl_var->type->base;
                if (init->type == AST_FLOAT && (b == UINT_T || b == INT_T)) {
                    error(ast->line, "Cannot implicitly cast float literal '%f' to integer type '%s'.", init->fval, ast->decl_var->type->name);
                }
                if (b == UINT_T) {
                    if (init->ival < 0) {
                        error(ast->line, "Cannot assign negative integer literal '%d' to unsigned type '%s'.", init->ival, ast->decl_var->type->name);
                    }
                    if (precision_loss_uint(ast->decl_var->type, init->ival)) {
                        error(ast->line, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->ival, ast->decl_var->type->name);
                    }
                } else if (b == INT_T) {
                    if (precision_loss_int(ast->decl_var->type, init->ival)) {
                        error(ast->line, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->ival, ast->decl_var->type->name);
                    }
                } else if (b == FLOAT_T) {
                    if (precision_loss_float(ast->decl_var->type, init->type == AST_INTEGER ? init->ival : init->fval)) {
                        error(ast->line, "Cannot assign value '%ld' to type '%s' without cast, loss of precision will occur.", init->fval, ast->decl_var->type->name);
                    }
                } else {
                    error(-1, "wtf");
                }
            }
            if (!(check_type(ast->decl_var->type, var_type(init)) || type_can_coerce(var_type(init), ast->decl_var->type))) {
                // TODO only for literal?
                init = try_implicit_cast(ast->decl_var->type, init);
            }
            if (init->type != AST_TEMP_VAR && is_dynamic(ast->decl_var->type)) {
                if (!is_literal(init)) {
                    init = make_ast_copy(init);
                }
                init = make_ast_tmpvar(init, make_temp_var(var_type(init), scope));
            }
            ast->init = init;
        } else if (ast->decl_var->type->base == AUTO_T) {
            error(ast->line, "Cannot use type 'auto' for variable '%s' without initialization.", ast->decl_var->name);
        } else if (ast->decl_var->type->base == STATIC_ARRAY_T && ast->decl_var->type->length == -1) {
            error(ast->line, "Cannot use unspecified array type for variable '%s' without initialization.", ast->decl_var->name);
        }
        if (find_local_var(ast->decl_var->name, scope) != NULL) {
            error(ast->line, "Declared variable '%s' already exists.", ast->decl_var->name);
        }
        Type *t = find_type_by_name(ast->decl_var->name);
        if (t != NULL) { // TODO maybe don't do this here?
            error(ast->line, "Variable name '%s' already in use by type.", ast->decl_var->name);
        }
        if (parser_state == PARSE_MAIN) {
            global_vars = varlist_append(global_vars, ast->decl_var);
            if (ast->init != NULL) {
                Ast *id = ast_alloc(AST_IDENTIFIER);
                id->varname = ast->decl_var->name;
                id->var = ast->decl_var;
                ast = make_ast_assign(id, ast->init);
                return ast;
            }
        } else {
            attach_var(ast->decl_var, scope); // should this be after the init parsing?
        }
        break;
    }
    case AST_TYPE_DECL: {
        if (parser_state != PARSE_MAIN) {
            error(ast->line, "Cannot declare a type inside scope ('%s').", ast->target_type->name);
        }
        Var *v = find_var(ast->type_name, scope);
        if (v != NULL) {
            error(ast->line, "Type name '%s' already exists.", ast->type_name);
        }
        Type *t = find_type_by_name(ast->type_name);
        if (t != NULL && t->id != ast->target_type->id) { // TODO maybe don't do this here?
            error(ast->line, "Type name '%s' already exists.", ast->type_name);
        }
        break;
    }
    case AST_EXTERN_FUNC_DECL:
        if (parser_state != PARSE_MAIN) {
            error(ast->line, "Cannot declare an extern inside scope ('%s').", ast->fn_decl_var->name);
        }
        attach_var(ast->fn_decl_var, scope);
        global_fn_vars = varlist_append(global_fn_vars, ast->fn_decl_var);
        break;
    case AST_FUNC_DECL:
        if (parser_state != PARSE_MAIN) {
            error(ast->line, "Cannot declare a named function inside scope ('%s').", ast->fn_decl_var->name);
        }
        /*attach_var(ast->fn_decl_var, scope);*/
        /*attach_var(ast->fn_decl_var, ast->fn_body);*/
    case AST_ANON_FUNC_DECL: {
        /*global_fn_vars = varlist_append(global_fn_vars, ast->fn_decl_var);*/
        Var *bindings_var = NULL;
        if (ast->type == AST_ANON_FUNC_DECL) {
            bindings_var = malloc(sizeof(Var));
            bindings_var->name = "";
            bindings_var->type = base_type(BASEPTR_T);
            bindings_var->id = new_var_id();
            bindings_var->temp = 1;
            bindings_var->consumed = 0;
            ast->fn_decl_var->type->bindings_id = bindings_var->id;
        }

        int prev = parser_state;
        parser_state = PARSE_FUNC;
        // TODO need a check here for no return or return of wrong type
        PUSH_FN_SCOPE(ast);
        ast->fn_body = parse_semantics(ast->fn_body, scope);
        POP_FN_SCOPE();
        parser_state = prev;
        detach_var(ast->fn_decl_var, ast->fn_body);
        if (ast->fn_decl_var->type->bindings != NULL) {
            global_fn_bindings = varlist_append(global_fn_bindings, bindings_var);
        }
        break;
    }
    case AST_CALL: {
        Ast *arg;
        ast->fn = parse_semantics(ast->fn, scope);
        Type *t = var_type(ast->fn);
        if (t->base != FN_T) {
            error(ast->line, "Cannot perform call on non-function type '%s'", t->name);
        }
        if (ast->fn->type == AST_ANON_FUNC_DECL) {
            ast->fn = make_ast_tmpvar(ast->fn, make_temp_var(t, scope));
        }
        if (ast->nargs != t->nargs) {
            error(ast->line, "Incorrect argument count to function (expected %d, got %d)", t->nargs, ast->nargs);
        }
        AstList *args = ast->args;
        TypeList *arg_types = t->args;
        for (int i = 0; args != NULL; i++) {
            arg = args->item;
            arg = parse_semantics(arg, scope);
            if (!(check_type(var_type(arg), arg_types->item) || type_can_coerce(var_type(arg), arg_types->item))) {
                if (is_literal(arg)) {
                    arg = try_implicit_cast(arg_types->item, arg);
                } else {
                    error(arg->line, "Incorrect argument to function, expected type '%s', and got '%s'.", arg_types->item->name, var_type(arg)->name);
                }
            }
            if (arg->type != AST_TEMP_VAR && var_type(arg)->base != STRUCT_T && is_dynamic(arg_types->item)) {
                if (!is_literal(arg)) {
                    arg = make_ast_copy(arg);
                }
                arg = make_ast_tmpvar(arg, make_temp_var(var_type(arg), scope));
            }
            args->item = arg;
            args = args->next;
            arg_types = arg_types->next;
        }
        if (is_dynamic(var_type(ast->fn)->ret)) {
            return make_ast_tmpvar(ast, make_temp_var(var_type(ast->fn)->ret, scope));
        }
        break;
    }
    case AST_INDEX: {
        ast->left = parse_semantics(ast->left, scope);
        ast->right = parse_semantics(ast->right, scope);
        Type *l = var_type(ast->left);
        Type *r = var_type(ast->right);
        /*if (!(is_array(l) || (l->base == PTR_T && is_array(l->inner)))) {*/
        if (!is_array(l)) {
            error(ast->left->line, "Cannot perform index/subscript operation on non-array type (type is '%s').", l->name);
        }
        if (r->base != INT_T && r->base != UINT_T) {
            error(ast->right->line, "Cannot index array with non-integer type '%s'.", r->name);
        }
        int _static = l->base == STATIC_ARRAY_T; // || (l->base == PTR_T && l->inner->base == STATIC_ARRAY_T);
        if (is_literal(ast->right)) {
            int i = ast->right->ival;
            // r must be integer
            if (i < 0) {
                error(ast->line, "Negative array index is larger than array length (%ld vs length %ld).", ast->right->ival, array_size(l));
            } else if (_static && i >= array_size(l)) {
                error(ast->line, "Array index is larger than array length (%ld vs length %ld).", ast->right->ival, array_size(l));
            }
            ast->right->ival = i;
        }
        break;
    }
    case AST_CONDITIONAL:
        ast->condition = parse_semantics(ast->condition, scope);
        if (var_type(ast->condition)->base != BOOL_T) {
            error(ast->line, "Non-boolean ('%s') condition for if statement.", var_type(ast->condition)->name);
        }
        ast->if_body = parse_semantics(ast->if_body, scope);
        if (ast->else_body != NULL) {
            ast->else_body = parse_semantics(ast->else_body, scope);
        }
        break;
    case AST_WHILE:
        ast->while_condition = parse_semantics(ast->while_condition, scope);
        if (var_type(ast->while_condition)->base != BOOL_T) {
            error(ast->line, "Non-boolean ('%s') condition for while loop.", var_type(ast->while_condition)->name);
        }
        int _old = loop_state;
        loop_state = 1;
        ast->while_body = parse_semantics(ast->while_body, scope);
        loop_state = _old;
        break;
    case AST_FOR: {
      /*printf("HEEREYYY %s\n", ast->for_iterable->var->name);*/
        ast->for_iterable = parse_semantics(ast->for_iterable, scope);
        Type *it_type = var_type(ast->for_iterable);
        if (!is_array(it_type)) {
            error(ast->line, "Cannot use for loop to iterator over non-array type '%s'.", it_type->name);
        }
        ast->for_itervar->type = it_type->inner;
        // TODO type check for when type of itervar is explicit
        int _old = loop_state;
        loop_state = 1;
        attach_var(ast->for_itervar, ast->for_body);
        ast->for_body = parse_semantics(ast->for_body, scope);
        loop_state = _old;
        break;
    }
    case AST_SCOPE:
        ast->body = parse_semantics(ast->body, ast);
        break;
    case AST_BIND: {
        if (current_fn_scope == NULL || current_fn_scope->type != AST_ANON_FUNC_DECL) {
            error(ast->line, "Cannot make bindings outside of an inner function."); 
        }
        ast->bind_expr = parse_semantics(ast->bind_expr, current_fn_scope->fn_body->parent);
        ast->bind_offset = add_binding(current_fn_scope->fn_decl_var->type, var_type(ast->bind_expr));
        ast->bind_id = current_fn_scope->fn_decl_var->type->bindings_id;
        add_binding_expr(ast->bind_id, ast->bind_expr);
        if (is_dynamic(var_type(ast->bind_expr))) {
            return make_ast_tmpvar(ast, make_temp_var(var_type(ast->bind_expr), scope));
        }
        break;
    }
    case AST_RETURN: {
        // TODO don't need to copy string being returned?
        if (current_fn_scope == NULL || parser_state != PARSE_FUNC) {
            error(ast->line, "Return statement outside of function body.");
        }
        scope->has_return = 1;
        Type *fn_ret_t = current_fn_scope->fn_decl_var->type->ret;
        Type *ret_t = NULL;
        if (ast->ret_expr == NULL) {
            ret_t = base_type(VOID_T);
        } else {
            ast->ret_expr = parse_semantics(ast->ret_expr, scope);
            ret_t = var_type(ast->ret_expr);
            if (is_dynamic(ret_t) && !is_literal(ast->ret_expr)) {
                ast->ret_expr = make_ast_copy(ast->ret_expr);
            }
        }
        if (ret_t->base == STATIC_ARRAY_T) {
            error(ast->line, "Cannot return a static array from a function.");
        }
        if (fn_ret_t->base == AUTO_T) {
            current_fn_scope->fn_decl_var->type->ret = ret_t;
        } else if (!check_type(fn_ret_t, ret_t)) {
            if (is_literal(ast->ret_expr)) {
                ast->ret_expr = try_implicit_cast(fn_ret_t, ast->ret_expr);
            } else {
                error(ast->line, "Return statement type '%s' does not match enclosing function's return type '%s'.", ret_t->name, fn_ret_t->name);
            }
        }
        break;
    }
    case AST_BREAK:
        if (!loop_state) {
            error(lineno(), "Break statement outside of loop.");
        }
        break;
    case AST_CONTINUE:
        if (!loop_state) {
            error(lineno(), "Continue statement outside of loop.");
        }
        break;
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            ast->statements[i] = parse_semantics(ast->statements[i], scope);
        }
        break;
    default:
        error(-1, "idk parse semantics");
    }
    return ast;
}

AstList *get_global_funcs() {
    return global_fn_decls;
}

TypeList *get_global_structs() {
    return reverse_typelist(global_struct_decls);
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
    Var *v = make_var("assert", make_fn_type(1, typelist_append(NULL, base_type(BOOL_T)), base_type(VOID_T)));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("print_str", make_fn_type(1, typelist_append(NULL, base_type(STRING_T)), base_type(VOID_T)));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("println", make_fn_type(1, typelist_append(NULL, base_type(STRING_T)), base_type(VOID_T)));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("itoa", make_fn_type(1, typelist_append(NULL, base_type(INT_T)), base_type(STRING_T)));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("validptr", make_fn_type(1, typelist_append(NULL, base_type(BASEPTR_T)), base_type(BOOL_T)));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("getc", make_fn_type(0, NULL, base_numeric_type(UINT_T, 8)));
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
