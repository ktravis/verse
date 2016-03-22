#include "parse.h"

#define PUSH_FN_SCOPE(x) \
    Ast *__old_fn_scope = current_fn_scope;\
    current_fn_scope = (x);
#define POP_FN_SCOPE() current_fn_scope = __old_fn_scope;

static int last_var_id = 0;
static int last_cond_id = 0;
static int last_tmp_fn_id = 0;
static VarList *global_vars = NULL;
static VarList *global_fn_vars = NULL;
static AstList *global_fn_decls = NULL;
static AstList *global_struct_decls = NULL;
static int parser_state = PARSE_MAIN;
static Ast *current_fn_scope = NULL;

VarList *varlist_append(VarList *list, Var *v) {
    VarList *vl = malloc(sizeof(VarList));
    vl->item = v;
    vl->next = list;
    return vl;
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
    var->initialized = 0;
    if (type->base == STRUCT_T) {
        var->initialized = 1;
        StructType *st = get_struct_type(type->struct_id);
        var->members = malloc(sizeof(Var*)*st->nmembers);
        for (int i = 0; i < st->nmembers; i++) {
            int l = strlen(name)+strlen(st->member_names[i])+1;
            char *member_name = malloc((l+1)*sizeof(char));
            sprintf(member_name, "%s.%s", name, st->member_names[i]);
            member_name[l] = 0;
            var->members[i] = make_var(member_name, st->member_types[i]);
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

Var *make_temp_var(Type *type, Ast *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = "";
    var->type = type;
    var->id = last_var_id++;
    var->temp = 1;
    var->consumed = 0;
    var->initialized = 0;
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
    return v;
}

Var *find_local_var(char *name, Ast *scope) {
    return varlist_find(scope->locals, name);
}

Type *var_type(Ast *ast) {
    switch (ast->type) {
    case AST_STRING:
        return make_type(STRING_T);
    case AST_INTEGER:
        return make_type(INT_T);
    case AST_BOOL:
        return make_type(BOOL_T);
    case AST_IDENTIFIER:
        return ast->var->type;
    case AST_DECL:
        return ast->decl_var->type;
    case AST_CALL:
        if (ast->fn_var != NULL) {
            return ast->fn_var->type->ret;
        }
        error("cannot do: %s", ast->fn);
        return make_type(INT_T);
    case AST_ANON_FUNC_DECL:
        return ast->fn_decl_var->type;
    case AST_UOP:
        if (ast->op == OP_NOT) {
            return make_type(BOOL_T);
        } else if (ast->op == OP_ADDR) {
            Type *t = make_type(PTR_T);
            t->inner = var_type(ast->right);
            return t;
        } else if (ast->op == OP_AT) {
            return var_type(ast->right)->inner;
        } else {
            error("don't know how to infer vartype of operator '%s' (%d).", op_to_str(ast->op), ast->op);
        }
        break;
    case AST_BINOP:
        if (is_comparison(ast->op)) {
            return make_type(BOOL_T);
        }
        return var_type(ast->left);
    case AST_DOT: {
        Type *t = var_type(ast->dot_left);
        StructType *st = get_struct_type(t->struct_id);
        for (int i = 0; i < st->nmembers; i++) {
            if (!strcmp(ast->member_name, st->member_names[i])) {
                return st->member_types[i];
            }
        }
        error("No member named '%s' in struct '%s'.", ast->member_name, st->name);
    }
    case AST_TEMP_VAR:
        return ast->tmpvar->type;
    default:
        error("don't know how to infer vartype");
    }
    return make_type(VOID_T);
}

int check_type(Type *a, Type *b) {
    if (a->base == b->base) {
        if (a->base == FN_T) {
            if (a->nargs == b->nargs && check_type(a->ret, b->ret)) {
                for (int i = 0; i < a->nargs; i++) {
                    if (!check_type(a->args[i], b->args[i])) {
                        return 0;
                    }
                }
                return 1;
            }
            return 0;
        } else if (a->base == STRUCT_T) {
            return a->struct_id == b->struct_id;
        } else if (a->base == PTR_T) {
            return check_type(a->inner, b->inner);
        }
        return 1;
    }
    return 0;
}

int is_dynamic(Type *t) {
    if (t->base == STRUCT_T) {
        StructType *st = get_struct_type(t->struct_id);
        for (int i = 0; i < st->nmembers; i++) {
            if (is_dynamic(st->member_types[i])) {
                return 1;
            }
        }
        return 0;
    }
    return t->base == STRING_T;
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
        /*printf("(tmp ");*/
        print_ast(ast->expr);
        /*printf(")");*/
        break;
    case AST_IDENTIFIER:
        printf("%s", ast->varname);
        break;
    case AST_DECL:
        printf("(decl %s %s", ast->decl_var->name, type_as_str(ast->decl_var->type));
        if (ast->init != NULL) {
            printf(" ");
            print_ast(ast->init);
        }
        printf(")");
        break;
    case AST_STRUCT_DECL:
        printf("(struct %s)", ast->struct_name);
        break;
    case AST_EXTERN_FUNC_DECL:
        printf("(extern fn %s)", ast->fn_decl_var->name);
        break;
    case AST_ANON_FUNC_DECL:
    case AST_FUNC_DECL:
        printf("(fn ");
        if (!ast->anon) {
            printf("%s", ast->fn_decl_var->name);
        }
        printf("(");
        for (int i = 0; i < ast->fn_decl_var->type->nargs; i++) {
            printf("%s", ast->fn_decl_args[i]->name);
            if (i < ast->fn_decl_var->type->nargs - 1) {
                printf(",");
            }
        }
        printf("):%s ", type_as_str(ast->fn_decl_var->type->ret));
        print_ast(ast->fn_body);
        printf(")");
        break;
    case AST_RETURN:
        printf("(return");
        if (ast->ret_expr != NULL) {
            printf(" ");
            print_ast(ast->ret_expr);
        }
        printf(")");
        break;
    case AST_CALL:
        printf("%s(", ast->fn);
        for (int i = 0; i < ast->nargs; i++) {
            print_ast(ast->args[i]);
            if (i + 1 < ast->nargs) {
                printf(",");
            }
        }
        printf(")");
        break;
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
    default:
        error("Cannot print this ast.");
    }
}

Ast *make_ast_string(char *str) {
    Ast *ast = malloc(sizeof(Ast));
    ast->type = AST_STRING;
    ast->sval = str;
    return ast;
}

Ast *make_ast_tmpvar(Ast *ast, Var *tmpvar) {
    Ast *tmp = malloc(sizeof(Ast));
    tmp->type = AST_TEMP_VAR;
    tmp->tmpvar = tmpvar;
    tmp->expr = ast;
    return tmp;
}

Ast *parse_uop_semantics(Ast *ast, Ast *scope) {
    ast->right = parse_semantics(ast->right, scope);
    switch (ast->op) {
    case OP_NOT:
        if (var_type(ast->right)->base != BOOL_T) {
            error("Cannot perform logical negation on type '%s'.", type_as_str(var_type(ast->right)));
        }
        break;
    case OP_AT:
        if (var_type(ast->right)->base != PTR_T) {
            error("Cannot dereference a non-pointer type.");
        }
        break;
    case OP_ADDR:
        if (ast->right->type != AST_IDENTIFIER && ast->right->type != AST_DOT) {
            error("Cannot take the address of a non-variable.");
        }
        break;
    default:
        error("Unknown unary operator '%s' (%d).", op_to_str(ast->op), ast->op);
    }
    return ast;
}

Ast *parse_dot_op_semantics(Ast *ast, Ast *scope) {
    ast->dot_left = parse_semantics(ast->dot_left, scope);
    if (var_type(ast->dot_left)->base != STRUCT_T) {
        error("Cannot use dot operator on non-struct type '%s'.", type_as_str(var_type(ast->dot_left)));
    }
    return ast;
}

// TODO nameof function that prepends _vs_, _tmp, _fn_, etc
// this will allow DOT to work on tmpvars

Var *get_ast_var(Ast *ast) {
    switch (ast->type) {
    case AST_DOT: {
        Var *v = get_ast_var(ast->dot_left);
        StructType *st = get_struct_type(v->type->struct_id);
        for (int i = 0; i < st->nmembers; i++) {
            if (!strcmp(st->member_names[i], ast->member_name)) {
                return v->members[i];
            }
        }
        error("Couldn't get member '%s' in struct %d.", ast->member_name, v->type->struct_id);
    }
    case AST_IDENTIFIER:
        return ast->var;
    case AST_TEMP_VAR:
        return ast->tmpvar;
    case AST_DECL:
        return ast->decl_var;
    }
    error("Can't get_ast_var(%d)", ast->type);
    return NULL;
}

Ast *parse_binop_semantics(Ast *ast, Ast *scope) {
    int op = ast->op;
    Ast *left = parse_semantics(ast->left, scope);
    Ast *right = parse_semantics(ast->right, scope);
    ast->left = left;
    ast->right = right;
    if (op == '=' && left->type != AST_IDENTIFIER && left->type != AST_DOT) {
        error("LHS of assignment is not an identifier.");
    }
    if (!check_type(var_type(left), var_type(right))) {
        error("LHS of operation '%s' has type '%s', while RHS has type '%s'.",
                op_to_str(op), type_as_str(left->var->type),
                type_as_str(var_type(right)));
    }
    switch (op) {
    case OP_PLUS:
        if (var_type(left)->base != INT_T && var_type(left)->base != STRING_T) {
            error("Operator '%s' is valid only for integer or string arguments, not for type '%s'.",
                    op_to_str(op), type_as_str(var_type(left)));
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
            error("Operator '%s' is not valid for non-integer arguments of type '%s'.",
                    op_to_str(op), type_as_str(var_type(left)));
        }
        break;
    case OP_AND:
    case OP_OR:
        if (var_type(left)->base != BOOL_T) {
            error("Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(op), type_as_str(var_type(left)));
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        break;
    }
    if (!is_comparison(ast->op) && is_dynamic(var_type(ast->left))) {
        if (ast->op == OP_ASSIGN) {
            if (ast->right->type != AST_TEMP_VAR) {
                ast->right = make_ast_tmpvar(ast->right, make_temp_var(var_type(ast->right), scope));
            }
            /*get_ast_var(ast->left)->initialized = 1;*/
        } else {
            if (ast->left->type != AST_TEMP_VAR) {
                ast->left = make_ast_tmpvar(ast->left, make_temp_var(var_type(ast->left), scope));
            }
            Ast *tmp = make_ast_tmpvar(ast, ast->left->tmpvar);
            return tmp;
        }
    }
    return ast;
}

Ast *make_ast_binop(int op, Ast *left, Ast *right) {
    Ast *binop = malloc(sizeof(Ast));
    binop->type = AST_BINOP;
    binop->op = op;
    binop->left = left;
    binop->right = right;
    return binop;
}

Ast *parse_arg_list(Tok *t, Ast *scope) {
    Ast *func = malloc(sizeof(Ast));
    func->args = malloc(sizeof(Ast*) * (MAX_ARGS + 1));
    func->fn = t->sval;
    func->type = AST_CALL;    
    int i;
    int n = 0;
    for (i = 0; i < MAX_ARGS; i++) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        func->args[i] = parse_expression(t, 0, scope);
        n++;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error("Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    if (i == MAX_ARGS) {
        error("OH NO THE ARGS");
    }
    func->nargs = n;
    return func; 
}

Ast *make_ast_decl(char *name, Type *type) {
    Ast *ast = malloc(sizeof(Ast));
    ast->type = AST_DECL;
    ast->decl_var = make_var(name, type);
    return ast;
}

Ast *parse_declaration(Tok *t, Ast *scope) {
    Ast *lhs = make_ast_decl(t->sval, parse_type(next_token(), scope));
    Tok *next = next_token();
    if (next == NULL) {
        error("Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        lhs->init = parse_expression(next_token(), 0, scope);
    } else if (next->type != TOK_SEMI) {
        error("Unexpected token '%s' while parsing declaration.", to_string(next));
    } else {
        unget_token(next);
    }
    if (parser_state == PARSE_MAIN && lhs->init != NULL) {
        global_vars = varlist_append(global_vars, lhs->decl_var);
        Ast *id = malloc(sizeof(Ast));
        id->type = AST_IDENTIFIER;
        id->varname = lhs->decl_var->name;
        id->var = lhs->decl_var;
        if (lhs->decl_var->type->base == AUTO_T) {
            lhs->decl_var->type = var_type(lhs->init); // TODO probably wrong
        }
        lhs = make_ast_binop(OP_ASSIGN, id, lhs->init);
    }
    return lhs; 
}

Ast *parse_expression(Tok *t, int priority, Ast *scope) {
    Ast *ast = parse_primary(t, scope);
    for (;;) {
        t = next_token();
        if (t == NULL) {
            return ast;
        } else if (t->type == TOK_SEMI || t->type == TOK_RPAREN || t->type == TOK_LBRACE) {
            unget_token(t);
            return ast;
        }
        int next_priority = priority_of(t);
        if (next_priority < 0 || next_priority < priority) {
            unget_token(t);
            return ast;
        } else if (t->type == TOK_OP) {
            ast = make_ast_binop(t->op, ast, parse_expression(next_token(), next_priority + 1, scope));
        } else {
            error("Unexpected token '%s'.", to_string(t));
            return NULL;
        }
    }
}

Type *parse_type(Tok *t, Ast *scope) {
    if (t == NULL) {
        error("Unexpected EOF while parsing type.");
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
    if (t->type == TOK_TYPE) {
        Type *type = make_type(t->tval);
        if (ptr) {
            Type *tmp = type;
            type = make_type(PTR_T);
            type->inner = tmp;
        }
        while (parens--) {
            expect(TOK_RPAREN);
        }
        return type;
    } else if (t->type == TOK_ID) {
        Type *type = find_struct_type(t->sval);
        if (type == NULL) {
            error("Unknown type '%s'.", t->sval);
        }
        if (ptr) {
            Type *tmp = type;
            type = make_type(PTR_T);
            type->inner = tmp;
        }
        while (parens--) {
            expect(TOK_RPAREN);
        }
        return type;
    } else if (t->type == TOK_FN) {
        if (ptr) {
            error("Cannot make a pointer to a function.");
        }
        Type **args = malloc(sizeof(Type*)*MAX_ARGS);
        int nargs = 0;
        expect(TOK_LPAREN);
        t = next_token();
        if (t == NULL) {
            error("Unexpected EOF while parsing type.");
        } else if (t->type != TOK_RPAREN) {
            for (nargs = 0; nargs < MAX_ARGS;) {
                args[nargs++] = parse_type(t, scope);
                t = next_token();
                if (t == NULL) {
                    error("Unexpected EOF while parsing type.");
                } else if (t->type == TOK_RPAREN) {
                    break;
                } else if (t->type != TOK_COMMA) {
                    error("Unexpected token '%s' while parsing type.", to_string(t));
                }
            }
        }
        expect(TOK_COLON);
        Type *ret = parse_type(next_token(), scope);
        while (parens--) {
            expect(TOK_RPAREN);
        }
        return make_fn_type(nargs, args, ret);
    /*} else if (t->type == TOK_ID) {*/
        /*// check for custom type, check for struct*/
    } else {
        error("Unexpected token '%s' while parsing type.", to_string(t));
    }
    error("Failed to parse type.");
    return NULL;
}

Ast *parse_extern_func_decl(Ast *scope) {
    Tok *t = expect(TOK_FN);
    t = expect(TOK_ID);
    char *fname = t->sval;
    expect(TOK_LPAREN);

    Ast *func = malloc(sizeof(Ast));
    func->type = AST_EXTERN_FUNC_DECL;    

    int i;
    int n = 0;
    Type** arg_types = malloc(sizeof(Type*) * (MAX_ARGS + 1));
    for (i = 0; i < MAX_ARGS; i++) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        arg_types[i] = parse_type(t, scope);
        n++;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error("Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    if (i == MAX_ARGS) {
        error("OH NO THE ARGS");
    }
    expect(TOK_COLON);
    Type *fn_type = make_fn_type(n, arg_types, parse_type(next_token(), scope));
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
            error("Declared function name '%s' already exists in this scope.", fname);
        }
    }
    expect(TOK_LPAREN);

    Ast *func = malloc(sizeof(Ast));
    func->fn_decl_args = malloc(sizeof(Var*) * (MAX_ARGS + 1));
    func->type = anonymous ? AST_ANON_FUNC_DECL : AST_FUNC_DECL;

    Ast *fn_scope = malloc(sizeof(Ast));
    fn_scope->type = AST_SCOPE;
    fn_scope->locals = NULL;
    fn_scope->parent = NULL;

    int i;
    int n = 0;
    Type** arg_types = malloc(sizeof(Type*) * (MAX_ARGS + 1));
    for (i = 0; i < MAX_ARGS; i++) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        if (t->type != TOK_ID) {
            error("Unexpected token (type '%s') in argument list of function declaration '%s'.", token_type(t->type), fname);
        }
        if (find_local_var(t->sval, fn_scope) != NULL) {
            error("Declared variable '%s' already exists.", t->sval);
        }
        expect(TOK_COLON);
        func->fn_decl_args[i] = make_var(t->sval, parse_type(next_token(), scope));
        attach_var(func->fn_decl_args[i], fn_scope);
        func->fn_decl_args[i]->initialized = 1;
        arg_types[i] = func->fn_decl_args[i]->type;
        n++;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error("Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    if (i == MAX_ARGS) {
        error("OH NO THE ARGS");
    }
    expect(TOK_COLON);
    Type *fn_type = make_fn_type(n, arg_types, parse_type(next_token(), scope));
    expect(TOK_LBRACE);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 0;

    func->anon = anonymous;
    func->fn_decl_var = fn_decl_var;

    int prev = parser_state;
    parser_state = PARSE_FUNC;
    fn_scope->body = parse_block(fn_scope, 1);
    func->fn_body = fn_scope;
    parser_state = prev;

    global_fn_decls = astlist_append(global_fn_decls, func);

    return func; 
}

Ast *parse_return_statement(Tok *t, Ast *scope) {
    Ast *ast = malloc(sizeof(Ast));
    ast->type = AST_RETURN;
    ast->ret_expr = NULL;
    ast->fn_scope = scope;
    t = next_token();
    if (t == NULL) {
        error("EOF encountered in return statement.");
    } else if (t->type == TOK_SEMI) {
        unget_token(t);
        return ast;
    }
    ast->ret_expr = parse_expression(t, 0, scope);
    return ast;
}

Ast *parse_struct_decl(Ast *scope) {
    Tok *t = expect(TOK_ID);
    expect(TOK_LBRACE);
    int alloc = 6;
    StructType *st = make_struct_type(t->sval, 0, malloc(sizeof(char*) * alloc), malloc(sizeof(Type*) * alloc));
    // add name to vars before parse_type's
    for (;;) {
        if (st->nmembers >= alloc) {
            alloc += 6;
            st->member_names = realloc(st->member_names, sizeof(char*) * alloc);
            st->member_types = realloc(st->member_types, sizeof(char*) * alloc);
        }
        t = expect(TOK_ID);
        expect(TOK_COLON);
        Type *ty = parse_type(next_token(), scope);
        st->member_names[st->nmembers] = t->sval;
        st->member_types[st->nmembers++] = ty;
        expect(TOK_SEMI);
        t = next_token();
        if (t != NULL && t->type == TOK_RBRACE) {
            break;
        } else {
            unget_token(t);
        }
    }
    Ast *ast = malloc(sizeof(Ast));
    ast->type = AST_STRUCT_DECL;
    ast->struct_name = st->name;
    Type *ty = make_type(STRUCT_T);
    ty->struct_id = st->id;
    ast->struct_type = ty;
    global_struct_decls = astlist_append(global_struct_decls, ast);
    return ast;
}

Ast *parse_statement(Tok *t, Ast *scope) {
    Ast *ast;
    if (t->type == TOK_ID && peek_token() != NULL && peek_token()->type == TOK_COLON) {
        next_token();
        ast = parse_declaration(t, scope);
    } else if (t->type == TOK_FN) {
        return parse_func_decl(scope, 0);
    } else if (t->type == TOK_EXTERN) {
        ast = parse_extern_func_decl(scope);
    } else if (t->type == TOK_STRUCT) {
        return parse_struct_decl(scope);
    } else if (t->type == TOK_LBRACE) {
        return parse_scope(scope);
    } else if (t->type == TOK_IF) {
        return parse_conditional(scope);
    } else if (t->type == TOK_RETURN) {
        if (parser_state != PARSE_FUNC) {
            error("Return statement outside of function body.");
        }
        ast = parse_return_statement(t, scope);
    } else {
        ast = parse_expression(t, 0, scope);
    }
    expect(TOK_SEMI);
    return ast;
}

Ast *make_ast_id(Var *var, char *name) {
    Ast *id = malloc(sizeof(Ast));
    id->type = AST_IDENTIFIER;
    id->var = NULL;
    id->varname = name;
    return id;
}

Ast *make_ast_dot_op(Ast *dot_left, char *member_name) {
    Ast *ast = malloc(sizeof(Ast));
    ast->type = AST_DOT;
    ast->dot_left = dot_left;
    ast->member_name = member_name;
    return ast;
}

Ast *parse_primary(Tok *t, Ast *scope) {
    if (t == NULL) {
        error("Unexpected EOF while parsing primary.");
    }
    switch (t->type) {
    case TOK_INT: {
        Ast *ast = malloc(sizeof(Ast));
        ast->type = AST_INTEGER;
        ast->ival = t->ival;
        return ast;
    }
    case TOK_BOOL: {
        Ast *ast = malloc(sizeof(Ast));
        ast->type = AST_BOOL;
        ast->ival = t->ival;
        return ast;
    }
    case TOK_STR:
        return make_ast_string(t->sval);
    case TOK_ID: {
        Tok *next = next_token();
        if (next == NULL) {
            error("Unexpected end of input.");
        } else if (next->type == TOK_LPAREN) {
            return parse_arg_list(t, scope);
        } else if (next->type == TOK_DOT) {
            Ast *id = make_ast_id(NULL, t->sval);
            next = expect(TOK_ID);
            Ast *dot = make_ast_dot_op(id, next->sval);
            dot->type = AST_DOT;
            for (;;) {
                next = next_token();
                if (next == NULL || next->type != TOK_DOT) {
                    unget_token(next);
                    break;
                }
                dot = make_ast_dot_op(dot, expect(TOK_ID)->sval);
                next = peek_token();
                if (next == NULL || next->type != TOK_DOT) {
                    break;
                }
            }
            return dot;
        }
        unget_token(next);
        Ast *id = malloc(sizeof(Ast));
        id->type = AST_IDENTIFIER;
        id->var = NULL;
        id->varname = t->sval;
        return id;
    }
    case TOK_FN:
        return parse_func_decl(scope, 1);
    case TOK_CARET: {
        Ast *ast = malloc(sizeof(Ast));
        ast->type = AST_UOP;
        ast->op = OP_ADDR;
        ast->left = NULL;
        ast->right = parse_primary(next_token(), scope);
        return ast;
    }
    case TOK_UOP: {
        Ast *ast = malloc(sizeof(Ast));
        ast->type = AST_UOP;
        ast->op = t->op;
        ast->left = NULL;
        ast->right = parse_primary(next_token(), scope);
        return ast;
    }
    case TOK_LPAREN: {
        Tok *next = next_token();
        if (next == NULL) {
            error("Unexpected end of input.");
        }
        Ast *ast = parse_expression(next, 0, scope);
        next = next_token();
        if (next == NULL) {
            error("Unexpected end of input.");
        }
        if (next->type != TOK_RPAREN) {
            error("Unexpected token '%s' encountered while parsing parenthetical expression.", to_string(next));
        }
        return ast;
    }
    }
    error("Unexpected token '%s'.", to_string(t));
    return NULL;
}

Ast *parse_conditional(Ast *scope) {
    Ast *cond = malloc(sizeof(Ast));
    cond->type = AST_CONDITIONAL;
    cond->cond_id = last_cond_id++;
    cond->condition = parse_expression(next_token(), 0, scope);
    if (var_type(cond->condition)->base != BOOL_T) {
        error("Expression of type '%s' is not a valid conditional.", type_as_str(var_type(cond->condition)));
    }
    Tok *next = next_token();
    if (next == NULL || next->type != TOK_LBRACE) {
        error("Unexpected token '%s' while parsing conditional.", to_string(next));
    }
    cond->if_body = parse_block(scope, 1);
    next = next_token();
    if (next != NULL && next->type == TOK_ELSE) {
        next = next_token();
        if (next == NULL || next->type != TOK_LBRACE) {
            error("Unexpected token '%s' while parsing conditional.", to_string(next));
        }
        cond->else_body = parse_block(scope, 1);
    } else {
        cond->else_body = NULL;
        unget_token(next);
    }
    return cond;
}

Ast *parse_block(Ast *scope, int bracketed) {
    Ast *block = malloc(sizeof(Ast));
    block->type = AST_BLOCK;
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
                error("Unexpected token '%s' while parsing statement block.", to_string(t));
            }
        } else if (t->type == TOK_RBRACE) {
            if (bracketed) {
                break;
            } else {
                error("Unexpected token '%s' while parsing statement block.", to_string(t));
            }
        }
        if (i >= n) {
            n *= 2;
            statements = realloc(statements, sizeof(Ast*)*n);
        }
        Ast *stmt = parse_statement(t, scope);
        /*if (stmt->type != AST_EXTERN_FUNC_DECL) {*/
            statements[i++] = stmt;
        /*}*/
    }
    block->num_statements = i;
    block->statements = statements;
    return block;
}

Ast *parse_scope(Ast *parent) {
    Ast *scope = malloc(sizeof(Ast));
    scope->type = AST_SCOPE;
    scope->locals = NULL;
    scope->parent = parent;
    scope->body = parse_block(scope, parent == NULL ? 0 : 1);
    return scope;
}

Ast *generate_ast() {
    Ast *scope = malloc(sizeof(Ast));
    scope->type = AST_SCOPE;
    scope->locals = NULL;
    scope->parent = NULL;
    scope->body = parse_block(scope, 0);
    return scope;
}

Ast *parse_semantics(Ast *ast, Ast *scope) {
    switch (ast->type) {
    case AST_INTEGER:
    case AST_BOOL:
    case AST_STRING:
        break;
    case AST_DOT:
        return parse_dot_op_semantics(ast, scope);
    case AST_BINOP:
        return parse_binop_semantics(ast, scope);
    case AST_UOP:
        return parse_uop_semantics(ast, scope);
    case AST_IDENTIFIER: {
        Var *v = find_var(ast->varname, scope);
        if (v == NULL) {
            error("Undefined identifier '%s' encountered.", ast->varname);
        }
        ast->var = v;
        break;
    }
    /*case AST_DOT:*/
        
    /*case AST_TEMP_VAR: // shouldn't happen*/
        /*break;*/
    case AST_DECL: {
        Ast *init = ast->init;
        if (ast->decl_var->type->base == AUTO_T && init == NULL) {
            error("Cannot use type 'auto' for variable '%s' without initialization.", ast->decl_var->name);
        }
        if (find_local_var(ast->decl_var->name, scope) != NULL) {
            error("Declared variable '%s' already exists.", ast->decl_var->name);
        }
        attach_var(ast->decl_var, scope); // should this be after the init parsing?
        if (init != NULL) {
            init = parse_semantics(init, scope);
            if (ast->decl_var->type->base == AUTO_T) {
                ast->decl_var->type = var_type(ast->init);
            } else if (!check_type(ast->decl_var->type, var_type(init))) {
                error("Can't initialize variable '%s' of type '%s' with value of type '%s'.",
                        ast->decl_var->name, type_as_str(ast->decl_var->type), type_as_str(var_type(init)));
            }
            if (init->type != AST_TEMP_VAR && is_dynamic(var_type(init))) {
                init = make_ast_tmpvar(init, make_temp_var(var_type(init), scope));
            }
            ast->init = init;
        }
        break;
    }
    case AST_STRUCT_DECL: {
        StructType *st = get_struct_type(ast->struct_type->struct_id);
        if (parser_state != PARSE_MAIN) {
            error("Cannot declare a struct inside scope ('%s').", st->name);
        }
        for (int i = 0; i < st->nmembers-1; i++) {
            for (int j = i + 1; j < st->nmembers; j++) {
                if (!strcmp(st->member_names[i], st->member_names[j])) {
                    error("Repeat member name '%s' in struct '%s'.", st->member_names[i], st->name);
                }
            }
        }
        break;
    }
    case AST_EXTERN_FUNC_DECL:
        if (parser_state != PARSE_MAIN) {
            error("Cannot declare an extern inside scope ('%s').", ast->fn_decl_var->name);
        }
        attach_var(ast->fn_decl_var, scope);
        global_fn_vars = varlist_append(global_fn_vars, ast->fn_decl_var);
        break;
    case AST_FUNC_DECL:
    case AST_ANON_FUNC_DECL:
        attach_var(ast->fn_decl_var, scope);
        attach_var(ast->fn_decl_var, ast->fn_body);
        global_fn_vars = varlist_append(global_fn_vars, ast->fn_decl_var);

        PUSH_FN_SCOPE(ast);
        ast->fn_body = parse_semantics(ast->fn_body, scope);
        POP_FN_SCOPE();
        break;
    case AST_CALL: {
        Ast *arg;
        ast->fn_var = find_var(ast->fn, scope);
        if (ast->fn_var == NULL) {
            error("Undefined identifier '%s' encountered.", ast->fn);
        }
        for (int i = 0; i < ast->nargs; i++) {
            arg = ast->args[i];
            arg = parse_semantics(ast->args[i], scope);
            if (arg->type != AST_TEMP_VAR && var_type(arg)->base != STRUCT_T && is_dynamic(var_type(arg))) {
                arg = make_ast_tmpvar(arg, make_temp_var(var_type(arg), scope));
            }
            ast->args[i] = arg;
        }
        break;
    }
    case AST_CONDITIONAL:
        ast->condition = parse_semantics(ast->condition, scope);
        if (var_type(ast->condition)->base != BOOL_T) {
            error("Non-boolean ('%s') condition for if statement.", type_as_str(var_type(ast->condition)));
        }
        ast->if_body = parse_semantics(ast->if_body, scope);
        if (ast->else_body != NULL) {
            ast->else_body = parse_semantics(ast->else_body, scope);
        }
        break;
    case AST_SCOPE:
        ast->body = parse_semantics(ast->body, ast);
        break;
    case AST_RETURN: {
        if (current_fn_scope == NULL) {
            error("Return statement outside of function body.");
        }
        Type *fn_ret_t = current_fn_scope->fn_decl_var->type->ret;
        Type *ret_t = NULL;
        if (ast->ret_expr == NULL) {
            ret_t = make_type(VOID_T);
        } else {
            ast->ret_expr = parse_semantics(ast->ret_expr, scope);
            ret_t = var_type(ast->ret_expr);
        }
        if (fn_ret_t->base == AUTO_T) {
            current_fn_scope->fn_decl_var->type->ret = ret_t;
        } else if (!check_type(fn_ret_t, ret_t)) {
            error("Return statement type '%s' does not match enclosing function's return type '%s'.", type_as_str(ret_t), type_as_str(fn_ret_t));
        }
        break;
    }
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            ast->statements[i] = parse_semantics(ast->statements[i], scope);
        }
        break;
    default:
        error("idk parse semantics");
    }
    return ast;
}

AstList *get_global_funcs() {
    return global_fn_decls;
}

AstList *get_global_structs() {
    AstList *tail = global_struct_decls;
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
