#include "parse.h"

static int last_cond_id = 0;
static Ast *strings = NULL;
static Ast *global_func_decls = NULL;
static int parser_state = PARSE_MAIN;

Var *make_var(char *name, Type *type, Ast *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = malloc(strlen(name)+1);
    strcpy(var->name, name);
    var->type = type;
    var->id = scope->locals != NULL ? scope->locals->id + 1 : 0;
    var->temp = 0;
    var->initialized = 0;
    var->next = scope->locals;
    scope->locals = var;
    return var;
}

Var *make_temp_var(Type *type, Ast *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = "";
    var->type = type;
    var->id = scope->locals != NULL ? scope->locals->id + 1 : 0;
    var->temp = 1;
    var->consumed = 0;
    var->initialized = 0;
    var->next = scope->locals;
    scope->locals = var;
    return var;
}

Var *find_var(char *name, Ast *scope) {
    Var *v = find_local_var(name, scope);
    if (v == NULL && scope->parent != NULL) {
        v = find_var(name, scope->parent);
    }
    return v;
}

Var *find_local_var(char *name, Ast *scope) {
    Var *v = scope->locals;
    while (v != NULL) {
        if (!strcmp(name, v->name)) {
            break;
        }
        v = v->next;
    }
    return v;
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
        /*error("idk how to tell function stuff");*/
        return make_type(INT_T); // this is obviously wrong, need to change AST_CALL first though
    case AST_BINOP:
        if (is_comparison(ast->op)) {
            return make_type(BOOL_T);
        }
        return var_type(ast->left);
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
            if (a->nargs == b->nargs) {
                for (int i = 0; i < a->nargs; i++) {
                    if (!check_type(a->args[i], b->args[i])) {
                        return 0;
                    }
                }
                return 1;
            }
            return 0;
        }
        return 1;
    }
    return 0;
}

int is_dynamic(Type *t) {
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
        printf("%s", ast->var->name);
        break;
    case AST_DECL:
        printf("(decl %s %s", ast->var->name, type_as_str(ast->var->type->base));
        if (ast->init != NULL) {
            printf(" ");
            print_ast(ast->init);
        }
        printf(")");
        break;
    case AST_FUNC_DECL:
        printf("(fn %s (", ast->fn_decl_name);
        for (int i = 0; i < ast->fn_decl_nargs; i++) {
            printf("%s", ast->fn_decl_args[i]->name);
            if (i < ast->fn_decl_nargs - 1) {
                printf(",");
            }
        }
        printf(") "); // print ret type
        print_ast(ast->fn_body);
        printf(")");
        break;
    case AST_RETURN:
        printf("(return ");
        print_ast(ast->expr);
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
    ast->sid = strings == NULL ? 0 : strings->sid + 1;
    ast->snext = strings;
    strings = ast;
    return ast;
}

Ast *find_or_make_string(char *sval) {
    Ast *str = strings;
    for (;;) {
        if (str == NULL || !strcmp(str->sval, sval)) {
            str = make_ast_string(sval);
            break;
        }
        str = str->snext;
    }
    return str;
}

Ast *make_ast_tmpvar(Ast *ast, Var *tmpvar) {
    Ast *tmp = malloc(sizeof(Ast));
    tmp->type = AST_TEMP_VAR;
    tmp->tmpvar = tmpvar;
    tmp->expr = ast;
    return tmp;
}

Ast *make_ast_binop(int op, Ast *left, Ast *right, Ast *scope) {
    if (op == '=' && left->type != AST_IDENTIFIER) {
        error("LHS of assignment is not an identifier.");
    }
    if (!check_type(var_type(left), var_type(right))) {
        error("LHS of operation '%s' has type '%s', while RHS has type '%s'.",
                op_to_str(op), type_as_str(left->var->type->base), type_as_str(var_type(right)->base));
    }
    switch (op) {
    case OP_PLUS:
        if (var_type(left)->base != INT_T && var_type(left)->base != STRING_T) {
            error("Operator '%s' is valid only for integer or string arguments, not for type '%s'.",
                    op_to_str(op), type_as_str(var_type(left)->base));
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
                    op_to_str(op), type_as_str(var_type(left)->base));
        }
        break;
    case OP_AND:
    case OP_OR:
        if (var_type(left)->base != BOOL_T) {
            error("Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(op), type_as_str(var_type(left)->base));
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        break;
    }
    Type *lt = var_type(left);
    Ast *binop = malloc(sizeof(Ast));
    binop->type = AST_BINOP;
    binop->op = op;
    binop->left = left;
    binop->right = right;
    if (!is_comparison(op) && is_dynamic(lt)) {
        if (op == OP_ASSIGN) {
            if (right->type != AST_TEMP_VAR) {
                binop->right = make_ast_tmpvar(binop->right, make_temp_var(lt, scope));
            }
        } else {
            if (left->type != AST_TEMP_VAR) {
                binop->left = make_ast_tmpvar(left, make_temp_var(lt, scope));
            }
            Ast *tmp = make_ast_tmpvar(binop, binop->left->tmpvar);
            return tmp;
        }
    }
    return binop;
}

Ast *parse_arg_list(Tok *t, Ast *scope) {
    Ast *func = malloc(sizeof(Ast));
    func->args = malloc(sizeof(Ast*) * (MAX_ARGS + 1));
    func->fn = t->sval;
    func->type = AST_CALL;    
    int i;
    int n = 0;
    Ast *arg;
    for (i = 0; i < MAX_ARGS; i++) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        arg = parse_expression(t, 0, scope);
        if (arg->type != AST_TEMP_VAR && is_dynamic(var_type(arg))) {
            arg = make_ast_tmpvar(arg, make_temp_var(var_type(arg), scope));
        }
        func->args[i] = arg;
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

Ast *make_ast_decl(char *name, Type *type, Ast *scope) {
    Ast *ast = malloc(sizeof(Ast));
    ast->type = AST_DECL;
    ast->decl_var = make_var(name, type, scope);
    return ast;
}

Ast *parse_declaration(Tok *t, Ast *scope) {
    if (find_local_var(t->sval, scope) != NULL) {
        error("Declared variable '%s' already exists.", t->sval);
    }
    Tok *next = next_token();
    Ast *lhs = make_ast_decl(t->sval, parse_type(next, scope), scope);

    next = next_token();
    if (next == NULL) {
        error("Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        Ast *init = parse_expression(next_token(), 0, scope);
        if (!check_type(var_type(lhs), var_type(init))) {
            error("Can't initialize variable '%s' of type '%s' with value of type '%s'.", t->sval, type_as_str(next->tval), type_as_str(var_type(init)->base));
        }
        if (init->type != AST_TEMP_VAR && is_dynamic(var_type(init))) {
            init = make_ast_tmpvar(init, make_temp_var(var_type(init), scope));
        }
        lhs->init = init;
    } else if (next->type != TOK_SEMI) {
        error("Unexpected token '%s' while parsing declaration.", to_string(next));
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
        } else if (t->type == TOK_SEMI || t->type == TOK_RPAREN || t->type == TOK_LBRACE) {
            unget_token(t);
            return ast;
        }
        int next_priority = priority_of(t);
        if (next_priority < 0 || next_priority < priority) {
            unget_token(t);
            return ast;
        } else if (t->type == TOK_OP) {
            ast = make_ast_binop(t->op, ast, parse_expression(next_token(), next_priority + 1, scope), scope);
        } else {
            error("Unexpected token '%s'.", to_string(t));
            return NULL;
        }
    }
}

Type *parse_type(Tok *t, Ast *scope) {
    if (t == NULL) {
        error("Unexpected EOF while parsing type.");
    } else if (t->type == TOK_TYPE) {
        return make_type(t->tval);
    } else if (t->type == TOK_FN) {
        // parse fn type
        Type **args = malloc(sizeof(Type*)*MAX_ARGS);
        int nargs = 0;
        expect(TOK_LPAREN);
        t = next_token();
        if (t == NULL) {
            error("Unexpected token '%s' while parsing type.", to_string(t));
        } else if (t->type != TOK_RPAREN) {
            do {
                args[nargs] = parse_type(next_token(), scope);
                nargs++;
                t = next_token();
            } while (t->type == TOK_COMMA); 
            if (t == NULL || t->type != TOK_RPAREN) {
                error("Unexpected token '%s' while parsing type.", to_string(t));
            }
        }
        Type *ret = parse_type(next_token(), scope);
        return make_fn_type(nargs, args, ret);
    /*} else if (t->type == TOK_ID) {*/
        /*// check for custom type, check for struct*/
    } else {
        error("Unexpected token '%s' while parsing type.", to_string(t));
    }
    error("Failed to parse type.");
    return NULL;
}

Ast *parse_func_decl(Ast *scope) {
    Tok *t = expect(TOK_ID);
    char *fname = t->sval;
    if (find_local_var(fname, scope) != NULL) {
        error("Declared function name '%s' already exists in this scope.", fname);
    }
    expect(TOK_LPAREN);

    Ast *func = malloc(sizeof(Ast));
    func->fn_decl_args = malloc(sizeof(Var*) * (MAX_ARGS + 1));
    func->fn_decl_name = fname;
    func->type = AST_FUNC_DECL;    

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
        func->fn_decl_args[i] = make_var(t->sval, parse_type(next_token(), scope), fn_scope);
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
    func->fn_decl_nargs = n;
    expect(TOK_COLON);
    Type *fn_type = make_fn_type(n, arg_types, parse_type(next_token(), scope));
    expect(TOK_LBRACE);
    make_var(func->fn_decl_name, fn_type, scope);
    func->fn_decl_type = fn_type;
    int prev = parser_state;
    parser_state = PARSE_FUNC;
    fn_scope->body = parse_block(fn_scope, 1);
    func->fn_body = fn_scope;
    parser_state = prev;

    func->next_fn_decl = global_func_decls;
    global_func_decls = func;
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

Ast *parse_statement(Tok *t, Ast *scope) {
    Ast *ast;
    if (t->type == TOK_ID && peek_token() != NULL && peek_token()->type == TOK_COLON) {
        next_token();
        ast = parse_declaration(t, scope);
    } else if (t->type == TOK_FN) {
        return parse_func_decl(scope);
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

Ast *parse_primary(Tok *t, Ast *scope) {
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
        return find_or_make_string(t->sval);
    case TOK_ID: {
        Tok *next = next_token();
        if (next == NULL) {
            error("Unexpected end of input.");
        } else if (next->type == TOK_LPAREN) {
            return parse_arg_list(t, scope);
        }
        unget_token(next);
        Var *v = find_var(t->sval, scope);
        if (v == NULL) {
            error("Undefined identifier '%s' encountered.", t->sval);
        }
        Ast *id = malloc(sizeof(Ast));
        id->type = AST_IDENTIFIER;
        id->var = v;
        return id;
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
        error("Expression of type '%s' is not a valid conditional.", type_as_str(var_type(cond->condition)->base));
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
        Ast *statement = parse_statement(t, scope);
        if (statement->type != AST_FUNC_DECL) {
            statements[i] = statement;
            i++;
        }
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

Ast *get_string_list() {
    return strings;
}

Ast *get_global_funcs() {
    return global_func_decls;
}
