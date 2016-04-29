#include "parse.h"

#define PUSH_FN_SCOPE(x) \
    Ast *__old_fn_scope = current_fn_scope;\
    current_fn_scope = (x);
#define POP_FN_SCOPE() current_fn_scope = __old_fn_scope;

static int last_var_id = 0;
static int last_tmp_fn_id = 0;
static VarList *global_vars = NULL;
static VarList *global_fn_vars = NULL;
static VarList *builtin_vars = NULL;
static AstList *global_fn_decls = NULL;
static VarList *global_fn_bindings = NULL;
static TypeList *global_struct_decls = NULL;
static AstListList *binding_exprs = NULL;
static int parser_state = PARSE_MAIN; // TODO unnecessary w/ PUSH_FN_SCOPE?
static int loop_state = 0;
static int in_decl = 0;
static Ast *current_fn_scope = NULL;

static Type *auto_type = NULL;
static Type *void_type = NULL;
static Type *int_type = NULL;
static Type *uint_type = NULL;
static Type *bool_type = NULL;
static Type *string_type = NULL;
static Type *baseptr_type = NULL;

VarList *varlist_append(VarList *list, Var *v) {
    VarList *vl = malloc(sizeof(VarList));
    vl->item = v;
    vl->next = list;
    return vl;
}

VarList *varlist_remove(VarList *list, char *name) {
    if (list != NULL) {
        if (!strcmp(list->item->name, name)) {
            return list->next;
        }
        VarList *curr = NULL;
        VarList *last = list;
        while (last->next != NULL) {
            curr = last->next;
            if (!strcmp(curr->item->name, name)) {
                last->next = curr->next;
                break;
            }
            last = curr;
        }
    }
    return list;
}

Var *varlist_find(VarList *list, char *name) {
    Var *v = NULL;
    while (list != NULL) {
        if (!strcmp(list->item->name, name)) {
            v = list->item;
            break;
        }
        list = list->next;
    }
    return v;
}

VarList *reverse_varlist(VarList *list) {
    VarList *tail = list;
    if (tail == NULL) {
        return NULL;
    }
    VarList *head = tail;
    VarList *tmp = head->next;
    head->next = NULL;
    while (tmp != NULL) {
        tail = head;
        head = tmp;
        tmp = tmp->next;
        head->next = tail;
    }
    return head;
}

AstList *astlist_append(AstList *list, Ast *ast) {
    AstList *l = malloc(sizeof(AstList));
    l->item = ast;
    l->next = list;
    return l;
}

void print_locals(Ast *scope) {
    VarList *v = scope->locals;
    printf("locals: ");
    while (v != NULL) {
        printf("%s ", v->item->name);
        v = v->next;
    }
    printf("\n");
}

Var *make_var(char *name, Type *type) {
    Var *var = malloc(sizeof(Var));
    var->name = malloc(strlen(name)+1);
    strcpy(var->name, name);
    var->id = last_var_id++;
    var->type = type;
    var->temp = 0;
    var->consumed = 0;
    var->initialized = 0;
    if (type->base == STRUCT_T) {
        var->initialized = 1;
        var->members = malloc(sizeof(Var*)*type->nmembers);
        for (int i = 0; i < type->nmembers; i++) {
            int l = strlen(name)+strlen(type->member_names[i])+1;
            char *member_name;
            if (type->held) {
                member_name = malloc((l+2)*sizeof(char));
                sprintf(member_name, "%s->%s", name, type->member_names[i]);
                member_name[l+1] = 0;
            } else {
                member_name = malloc((l+1)*sizeof(char));
                sprintf(member_name, "%s.%s", name, type->member_names[i]);
                member_name[l] = 0;
            }
            var->members[i] = make_var(member_name, type->member_types[i]); // TODO
            var->members[i]->initialized = 1; // maybe wrong?
        }
    } else {
        var->members = NULL;
    }
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

Ast *ast_alloc(int type) {
    Ast *ast = malloc(sizeof(Ast));
    ast->type = type;
    ast->line = lineno();
    return ast;
}

Ast *make_ast_copy(Ast *ast) {
    Ast *cp = ast_alloc(AST_COPY);
    cp->copy_expr = ast;
    return cp;
}

Var *make_temp_var(Type *type, Ast *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = "";
    var->type = type;
    var->id = last_var_id++;
    var->temp = 1;
    var->consumed = 0;
    var->initialized = (type->base == FN_T);
    attach_var(var, scope);
    return var;
}

Var *find_var(char *name, Ast *scope) {
    Var *v = find_local_var(name, scope);
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

Type *var_type(Ast *ast) {
    switch (ast->type) {
    case AST_STRING:
        return string_type;
    case AST_INTEGER:
        return int_type;
    case AST_BOOL:
        return bool_type;
    case AST_STRUCT:
        return find_type_by_name(ast->struct_lit_name); // TODO just save this
    case AST_IDENTIFIER:
        return ast->var->type;
    case AST_CAST:
        return ast->cast_type;
    case AST_DECL:
        return ast->decl_var->type;
    case AST_CALL:
        return var_type(ast->fn)->ret;
    case AST_ANON_FUNC_DECL:
        return ast->fn_decl_var->type;
    case AST_BIND:
        return var_type(ast->bind_expr);
    case AST_UOP:
        if (ast->op == OP_NOT) {
            return bool_type;
        } else if (ast->op == OP_ADDR) {
            return make_ptr_type(var_type(ast->right));
        } else if (ast->op == OP_AT) {
            return var_type(ast->right)->inner;
        } else {
            error(ast->line, "don't know how to infer vartype of operator '%s' (%d).", op_to_str(ast->op), ast->op);
        }
        break;
    case AST_BINOP:
        if (is_comparison(ast->op)) {
            return bool_type;
        }
        return var_type(ast->left);
    case AST_DOT: {
        Type *t = var_type(ast->dot_left);
        if (t->base == PTR_T) {
            t = t->inner;
        }
        for (int i = 0; i < t->nmembers; i++) {
            if (!strcmp(ast->member_name, t->member_names[i])) {
                return t->member_types[i];
            }
        }
        error(ast->line, "No member named '%s' in struct '%s'.", ast->member_name, t->name);
    }
    case AST_TEMP_VAR:
        return ast->tmpvar->type;
    case AST_COPY:
        return var_type(ast->copy_expr);
    case AST_HOLD: {
        return ast->tmpvar->type;
    }
    default:
        error(ast->line, "don't know how to infer vartype (%d)", ast->type);
    }
    return void_type;
}

int can_cast(Type *from, Type *to) {
    switch (from->base) {
    case BASEPTR_T:
        return (to->base == PTR_T || to->base == BASEPTR_T);
    case PTR_T:
        return to->base == BASEPTR_T || check_type(from->inner, to->inner);
    case FN_T:
        if (from->nargs == to->nargs && check_type(from->ret, to->ret)) {
            TypeList *from_args = from->args;
            TypeList *to_args = to->args;
            while (from_args != NULL) {
                if (!check_type(from_args->item, to_args->item)) {
                    return 0;
                }
                from_args = from_args->next;
                to_args = to_args->next;
            }
            return 1;
        }
        return 0;
    case STRUCT_T:
        for (int i = 0; i < to->nmembers; i++) {
            if (!check_type(from->member_types[i], to->member_types[i])) {
                return 0;
            }
        }
        return 1;
    default:
        return from->base == to->base && from->size <= to->size;
    }
    return 0;
}

int check_type(Type *a, Type *b) {
    if (a->base == BASEPTR_T) {
        return (b->base == PTR_T || b->base == BASEPTR_T);
    } else if (b->base == BASEPTR_T) {
        return (a->base == PTR_T);
    }
    if (a->named || b->named) {
        return a->id == b->id;
    }
    if (a->base == b->base) {
        if (a->base == FN_T) {
            if (a->nargs == b->nargs && check_type(a->ret, b->ret)) {
                TypeList *a_args = a->args;
                TypeList *b_args = b->args;
                while (a_args != NULL) {
                    if (!check_type(a_args->item, b_args->item)) {
                        return 0;
                    }
                    a_args = a_args->next;
                    b_args = b_args->next;
                }
                return 1;
            }
            return 0;
        } else if (a->base == STRUCT_T) {
            if (a->nmembers != b->nmembers) {
                return 0;
            }
            for (int i = 0; i < a->nmembers; i++) {
                if (!check_type(a->member_types[i], b->member_types[i])) {
                    return 0;
                }
            }
            return 1;
        } else if (a->base == PTR_T) {
            return check_type(a->inner, b->inner);
        }
        return 1;
    }
    return 0;
}

int is_dynamic(Type *t) {
    if (t->base == STRUCT_T) {
        for (int i = 0; i < t->nmembers; i++) {
            if (is_dynamic(t->member_types[i])) {
                return 1;
            }
        }
        return 0;
    }
    return t->base == STRING_T || (t->base == FN_T && t->bindings != NULL);
}

int is_literal(Ast *ast) {
    switch (ast->type) {
    case AST_STRING:
    case AST_INTEGER:
    case AST_BOOL:
    case AST_STRUCT:
    case AST_ANON_FUNC_DECL:
        return 1;
    case AST_TEMP_VAR:
        return is_literal(ast->expr);
    }
    return 0;
}

void print_ast(Ast *ast) {
    switch (ast->type) {
    case AST_INTEGER:
        printf("%d", ast->ival);
        break;
    case AST_BOOL:
        printf("%s", ast->ival == 1 ? "true" : "false");
        break;
    case AST_STRING:
        printf("\"");
        print_quoted_string(ast->sval);
        printf("\"");
        break;
    case AST_DOT:
        print_ast(ast->dot_left);
        printf(".%s", ast->member_name);
        break;
    case AST_UOP:
        printf("(%s ", op_to_str(ast->op));
        print_ast(ast->right);
        printf(")");
        break;
    case AST_BINOP:
        printf("(%s ", op_to_str(ast->op));
        print_ast(ast->left);
        printf(" ");
        print_ast(ast->right);
        printf(")");
        break;
    case AST_TEMP_VAR:
        printf("(tmp ");
        print_ast(ast->expr);
        printf(")");
        break;
    case AST_IDENTIFIER:
        printf("%s", ast->varname);
        break;
    case AST_DECL:
        printf("(decl %s %s", ast->decl_var->name, ast->decl_var->type->name);
        if (ast->init != NULL) {
            printf(" ");
            print_ast(ast->init);
        }
        printf(")");
        break;
    case AST_TYPE_DECL:
        printf("(type %s %s)", ast->type_name, ast->target_type->name);
        break;
    case AST_EXTERN_FUNC_DECL:
        printf("(extern fn %s)", ast->fn_decl_var->name);
        break;
    case AST_ANON_FUNC_DECL:
    case AST_FUNC_DECL: {
        VarList *args = ast->fn_decl_args;
        printf("(fn ");
        if (!ast->anon) {
            printf("%s", ast->fn_decl_var->name);
        }
        printf("(");
        while (args != NULL) {
            printf("%s", args->item->name);
            if (args->next != NULL) {
                printf(",");
            }
            args = args->next;
        }
        printf("):%s ", ast->fn_decl_var->type->ret->name);
        print_ast(ast->fn_body);
        printf(")");
        break;
    }
    case AST_RETURN:
        printf("(return");
        if (ast->ret_expr != NULL) {
            printf(" ");
            print_ast(ast->ret_expr);
        }
        printf(")");
        break;
    case AST_CALL: {
        AstList *args = ast->args;
        print_ast(ast->fn);
        printf("(");
        while (args != NULL) {
            print_ast(args->item);
            if (args->next != NULL) {
                printf(",");
            }
            args = args->next;
        }
        printf(")");
        break;
    }
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            print_ast(ast->statements[i]);
        }
        break;
    case AST_SCOPE:
        printf("{ ");
        print_ast(ast->body);
        printf(" }");
        break;
    case AST_CONDITIONAL:
        printf("(if ");
        print_ast(ast->condition);
        printf(" ");
        print_ast(ast->if_body);
        if (ast->else_body != NULL) {
            printf(" ");
            print_ast(ast->else_body);
        }
        printf(")");
        break;
    case AST_HOLD:
        printf("(hold ");
        print_ast(ast->expr);
        printf(")");
    default:
        error(ast->line, "Cannot print this ast.");
    }
}

Ast *make_ast_string(char *str) {
    Ast *ast = ast_alloc(AST_STRING);
    ast->sval = str;
    return ast;
}

Ast *make_ast_tmpvar(Ast *ast, Var *tmpvar) {
    Ast *tmp = ast_alloc(AST_TEMP_VAR);
    tmp->tmpvar = tmpvar;
    tmp->expr = ast;
    return tmp;
}

Ast *parse_uop_semantics(Ast *ast, Ast *scope) {
    ast->right = parse_semantics(ast->right, scope);
    switch (ast->op) {
    case OP_NOT:
        if (var_type(ast->right)->base != BOOL_T) {
            error(ast->line, "Cannot perform logical negation on type '%s'.", var_type(ast->right)->name);
        }
        break;
    case OP_AT:
        if (var_type(ast->right)->base != PTR_T) {
            error(ast->line, "Cannot dereference a non-pointer type.");
        }
        break;
    case OP_ADDR:
        if (ast->right->type != AST_IDENTIFIER && ast->right->type != AST_DOT) {
            error(ast->line, "Cannot take the address of a non-variable.");
        }
        break;
    default:
        error(ast->line, "Unknown unary operator '%s' (%d).", op_to_str(ast->op), ast->op);
    }
    if (is_dynamic(var_type(ast->right))) {
        if (ast->op == OP_AT) {
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
    if (t->base != STRUCT_T && !(t->base == PTR_T && t->inner->base == STRUCT_T)) {
        error(ast->line, "Cannot use dot operator on non-struct type '%s'.", var_type(ast->dot_left)->name);
    }
    return ast;
}

// TODO nameof function that prepends _vs_, _tmp, _fn_, etc

Var *get_ast_var(Ast *ast) {
    switch (ast->type) {
    case AST_DOT: {
        Var *v = get_ast_var(ast->dot_left);
        Type *t = v->type;
        for (int i = 0; i < t->nmembers; i++) {
            if (!strcmp(t->member_names[i], ast->member_name)) {
                return v->members[i];
            }
        }
        error(ast->line, "Couldn't get member '%s' in struct '%s' (%d).", ast->member_name, t->name, t->id);
    }
    case AST_IDENTIFIER:
        return ast->var;
    case AST_TEMP_VAR:
        return ast->tmpvar;
    case AST_DECL:
        return ast->decl_var;
    }
    error(ast->line, "Can't get_ast_var(%d)", ast->type);
    return NULL;
}

Ast *parse_binop_semantics(Ast *ast, Ast *scope) {
    int op = ast->op;
    Ast *left = parse_semantics(ast->left, scope);
    Ast *right = parse_semantics(ast->right, scope);
    ast->left = left;
    ast->right = right;
    if (op == '=' && left->type != AST_IDENTIFIER && left->type != AST_DOT) {
        error(ast->line, "LHS of assignment is not an identifier.");
    }
    if (!check_type(var_type(left), var_type(right))) {
        error(ast->line, "LHS of operation '%s' has type '%s', while RHS has type '%s'.",
                op_to_str(op), left->var->type->name,
                var_type(right)->name);
    }
    switch (op) {
    case OP_PLUS:
        if (var_type(left)->base != INT_T && var_type(left)->base != STRING_T) {
            error(ast->line, "Operator '%s' is valid only for integer or string arguments, not for type '%s'.",
                    op_to_str(op), var_type(left)->name);
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
        if (var_type(left)->base != INT_T) {
            error(ast->line, "Operator '%s' is not valid for non-integer arguments of type '%s'.",
                    op_to_str(op), var_type(left)->name);
        }
        break;
    case OP_AND:
    case OP_OR:
        if (var_type(left)->base != BOOL_T) {
            error(ast->line, "Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(op), var_type(left)->name);
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        break;
    }
    if (!is_comparison(ast->op) && is_dynamic(var_type(ast->left))) {
        if (ast->op == OP_ASSIGN) {
            if (ast->right->type != AST_TEMP_VAR && !is_literal(ast->right)) {
                ast->right = make_ast_tmpvar(ast->right, make_temp_var(var_type(ast->right), scope));
            }
            /*get_ast_var(ast->left)->initialized = 1;*/
        } else {
            /*if (ast->left->type != AST_TEMP_VAR) {*/
                /*ast->left = make_ast_tmpvar(ast->left, make_temp_var(var_type(ast->left), scope));*/
            /*}*/
            /*Ast *tmp = make_ast_tmpvar(ast, ast->left->tmpvar);*/
            /*return tmp;*/
            return make_ast_tmpvar(ast, make_temp_var(var_type(ast->left), scope));
        }
    }
    return ast;
}

Ast *make_ast_binop(int op, Ast *left, Ast *right) {
    Ast *binop = ast_alloc(AST_BINOP);
    binop->op = op;
    binop->left = left;
    binop->right = right;
    return binop;
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

Ast *make_ast_decl(char *name, Type *type) {
    Ast *ast = ast_alloc(AST_DECL);
    ast->decl_var = make_var(name, type);
    return ast;
}

Ast *parse_declaration(Tok *t, Ast *scope) {
    Ast *lhs = make_ast_decl(t->sval, parse_type(next_token(), scope));
    Tok *next = next_token();
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
        } else if (t->type == TOK_SEMI || t->type == TOK_RPAREN || t->type == TOK_LBRACE || t->type == TOK_RBRACE) {
            unget_token(t);
            return ast;
        }
        int next_priority = priority_of(t);
        if (next_priority < 0 || next_priority < priority) {
            unget_token(t);
            return ast;
        } else if (t->type == TOK_OP) {
            if (t->op == OP_DOT) {
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
            ast = parse_arg_list(ast, scope);
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
        Type *ret = void_type;
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
    Type *ret = void_type;
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
    Type *ret = void_type;
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
        ast->while_body = parse_block(scope, 1);
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

Ast *make_ast_id(Var *var, char *name) {
    Ast *id = ast_alloc(AST_IDENTIFIER);
    id->var = NULL;
    id->varname = name;
    return id;
}

Ast *make_ast_dot_op(Ast *dot_left, char *member_name) {
    Ast *ast = ast_alloc(AST_DOT);
    ast->dot_left = dot_left;
    ast->member_name = member_name;
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
    Tok *t = expect(TOK_LBRACE);
    Ast *ast = ast_alloc(AST_STRUCT);
    int alloc = 0;
    ast->struct_lit_name = name;
    ast->nmembers = 0;
    if (peek_token()->type == TOK_RBRACE) {
        next_token();
        return ast;
    }
    for (;;) {
        t = next_token();
        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_ID) {
            error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));
        }
        if (ast->nmembers >= alloc) {
            alloc += 4;
            ast->member_names = realloc(ast->member_names, sizeof(char *)*alloc);
            ast->member_exprs = realloc(ast->member_exprs, sizeof(Ast *)*alloc);
        }
        ast->member_names[ast->nmembers] = t->sval;
        t = next_token();
        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_OP || t->op != OP_ASSIGN) {
            error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));
        }
        ast->member_exprs[ast->nmembers++] = parse_expression(next_token(), 0, scope);
        t = next_token();
        if (t == NULL) {
            error(lineno(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type == TOK_RBRACE) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), "Unexpected token '%s' while parsing struct literal.", to_string(t));
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
    case TOK_BOOL: {
        Ast *ast = ast_alloc(AST_BOOL);
        ast->ival = t->ival;
        return ast;
    }
    case TOK_STR:
        return make_ast_string(t->sval);
    case TOK_ID: {
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), "Unexpected end of input.");
        }
        unget_token(next);
        if (peek_token()->type == TOK_LBRACE) {
            return parse_struct_literal(t->sval, scope);
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
        if (next == NULL || next->type != TOK_LBRACE) {
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
    scope->body = parse_block(scope, parent == NULL ? 0 : 1);
    /*scope->bindings = NULL;*/
    /*scope->anon_funcs = NULL;*/
    return scope;
}

Ast *parse_semantics(Ast *ast, Ast *scope) {
    switch (ast->type) {
    case AST_INTEGER:
    case AST_BOOL:
        break;
    case AST_STRING:
        return make_ast_tmpvar(ast, make_temp_var(string_type, scope));
    case AST_DOT:
        return parse_dot_op_semantics(ast, scope);
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
        // TODO consider only BASEPTR_T being a valid release target
        ast->release_target = parse_semantics(ast->release_target, scope);
        Type *t = var_type(ast->release_target);
        if (ast->release_target->type == AST_IDENTIFIER) {
            if (!t->held && t->base != PTR_T && t->base != BASEPTR_T) {
                error(ast->line, "Cannot release the non-held variable '%s'.", ast->release_target->var->name);
            }
            // TODO instead mark that var has been released for better errors in
            // the future
            release_var(ast->release_target->var, scope);
        } else if (ast->release_target->type == AST_DOT) {
            if (t->base != PTR_T && t->base != BASEPTR_T) {
                error(ast->line, "Struct member release target must be a pointer.");
            }
        } else {
            error(ast->line, "Unexpected target of release statement (must be variable or dot op).");
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
        Type *tp = make_ptr_type(t);
        tp->held = 1;

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
            } else if (!check_type(ast->decl_var->type, var_type(init))) {
                error(ast->line, "Can't initialize variable '%s' of type '%s' with value of type '%s'.",
                        ast->decl_var->name, ast->decl_var->type->name, var_type(init)->name);
            }
            if (init->type != AST_TEMP_VAR && is_dynamic(var_type(init))) {
                if (!is_literal(init)) {
                    init = make_ast_copy(init);
                }
                init = make_ast_tmpvar(init, make_temp_var(var_type(init), scope));
            }
            ast->init = init;
        } else if (ast->decl_var->type->base == AUTO_T) {
            error(ast->line, "Cannot use type 'auto' for variable '%s' without initialization.", ast->decl_var->name);
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
                ast = make_ast_binop(OP_ASSIGN, id, ast->init);
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
        attach_var(ast->fn_decl_var, scope);
        attach_var(ast->fn_decl_var, ast->fn_body);
    case AST_ANON_FUNC_DECL: {
        global_fn_vars = varlist_append(global_fn_vars, ast->fn_decl_var);
        Var *bindings_var = NULL;
        if (ast->type == AST_ANON_FUNC_DECL) {
            bindings_var = malloc(sizeof(Var));
            bindings_var->name = "";
            bindings_var->type = baseptr_type;
            bindings_var->id = last_var_id++;
            bindings_var->temp = 1;
            bindings_var->consumed = 0;
            ast->fn_decl_var->type->bindings_id = bindings_var->id;
        }

        int prev = parser_state;
        parser_state = PARSE_FUNC;
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
            if (!check_type(var_type(arg), arg_types->item)) {
                error(arg->line, "Incorrect argument to function, expected type '%s', and got '%s'.", arg_types->item->name, var_type(arg)->name);
            }
            if (arg->type != AST_TEMP_VAR && var_type(arg)->base != STRUCT_T && is_dynamic(var_type(arg))) {
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
        if (current_fn_scope == NULL || parser_state != PARSE_FUNC) {
            error(ast->line, "Return statement outside of function body.");
        }
        Type *fn_ret_t = current_fn_scope->fn_decl_var->type->ret;
        Type *ret_t = NULL;
        if (ast->ret_expr == NULL) {
            ret_t = void_type;
        } else {
            ast->ret_expr = parse_semantics(ast->ret_expr, scope);
            ret_t = var_type(ast->ret_expr);
            if (is_dynamic(ret_t) && !is_literal(ast->ret_expr)) {
                ast->ret_expr = make_ast_copy(ast->ret_expr);
            }
        }
        if (fn_ret_t->base == AUTO_T) {
            current_fn_scope->fn_decl_var->type->ret = ret_t;
        } else if (!check_type(fn_ret_t, ret_t)) {
            error(ast->line, "Return statement type '%s' does not match enclosing function's return type '%s'.", ret_t->name, fn_ret_t->name);
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

AstList *reverse_astlist(AstList *list) {
    AstList *tail = list;
    if (tail == NULL) {
        return NULL;
    }
    AstList *head = tail;
    AstList *tmp = head->next;
    head->next = NULL;
    while (tmp != NULL) {
        tail = head;
        head = tmp;
        tmp = tmp->next;
        head->next = tail;
    }
    return head;
}

VarList *get_global_vars() {
    return global_vars;
}

VarList *get_global_bindings() {
    return global_fn_bindings;
}

void init_builtins() {
    Var *v = make_var("assert", make_fn_type(1, typelist_append(NULL, bool_type), void_type));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("print_str", make_fn_type(1, typelist_append(NULL, string_type), void_type));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("println", make_fn_type(1, typelist_append(NULL, string_type), void_type));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("itoa", make_fn_type(1, typelist_append(NULL, int_type), string_type));
    builtin_vars = varlist_append(builtin_vars, v);

    v = make_var("validptr", make_fn_type(1, typelist_append(NULL, baseptr_type), bool_type));
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

void init_types() {
    auto_type = define_type(make_type("auto", AUTO_T, -1));
    auto_type->named = 1;
    void_type = define_type(make_type("void", VOID_T, 0));
    void_type->named = 1;

    int_type = define_type(make_type("int", INT_T, 4));
    int_type->named = 1;
    uint_type = define_type(make_type("uint", UINT_T, 4));
    uint_type->named = 1;

    bool_type = define_type(make_type("bool", BOOL_T, 1));
    bool_type->named = 1;

    string_type = define_type(make_type("string", STRING_T, 16)); // should be checked that non-pointer is used in bindings
    string_type->named = 1;

    baseptr_type = define_type(make_type("ptr", BASEPTR_T, 8)); // should be checked that non-pointer is used in bindings
    baseptr_type->named = 1;
}
