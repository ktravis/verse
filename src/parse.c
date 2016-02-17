#include "parse.h"

static int last_cond_id = 0;
static Ast *strings = NULL;

Var *make_var(char *name, int type, Ast *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = malloc(strlen(name)+1);
    strcpy(var->name, name);
    var->type = type;
    var->offset = scope->locals == NULL ? 8 : scope->locals->offset + type_offset(scope->locals->type);
    var->temp = 0;
    var->initialized = 0;
    var->next = scope->locals;
    scope->locals = var;
    return var;
}

Var *make_temp_var(int type, Ast *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = "";
    var->type = type;
    var->offset = scope->locals == NULL ? 8 : scope->locals->offset + type_offset(scope->locals->type);
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

char var_type(Ast *ast) {
    switch (ast->type) {
    case AST_STRING:
        return STRING_T;
    case AST_INTEGER:
        return INT_T;
    case AST_BOOL:
        return BOOL_T;
    case AST_IDENTIFIER:
        return ast->var->type;
    case AST_DECL:
        return ast->var->type;
    case AST_CALL:
        /*error("idk how to tell function stuff");*/
        return INT_T;
    case AST_BINOP:
        if (is_comparison(ast->op)) {
            return BOOL_T;
        }
        return var_type(ast->left);
    case AST_TEMP_VAR:
        return ast->tmpvar->type;
    default:
        error("don't know how to infer vartype");
    }
    return VOID_T;
}

int is_dynamic(int t) {
    return t == STRING_T;
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
        printf("(decl %s %s", ast->var->name, type_as_str(ast->var->type));
        if (ast->init != NULL) {
            printf(" ");
            print_ast(ast->init);
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
    if (var_type(left) != var_type(right)) {
        error("LHS of operation '%s' has type '%s', while RHS has type '%s'.",
                op_to_str(op), type_as_str(left->var->type), type_as_str(var_type(right)));
    }
    switch (op) {
    case OP_PLUS:
        if (var_type(left) != INT_T && var_type(left) != STRING_T) {
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
        if (var_type(left) != INT_T) {
            error("Operator '%s' is not valid for non-integer arguments of type '%s'.",
                    op_to_str(op), type_as_str(var_type(left)));
        }
        break;
    case OP_AND:
    case OP_OR:
        if (var_type(left) != BOOL_T) {
            error("Operator '%s' is not valid for non-bool arguments of type '%s'.",
                    op_to_str(op), type_as_str(var_type(left)));
        }
        break;
    case OP_EQUALS:
    case OP_NEQUALS:
        break;
    }
    int lt = var_type(left);
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

Ast *make_ast_decl(char *name, int type, Ast *scope) {
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
    if (next->type != TOK_TYPE) {
        error("Unexpected token '%s' in declaration.", to_string(next));
    }

    Ast *lhs = make_ast_decl(t->sval, next->tval, scope);

    next = next_token();
    if (next == NULL) {
        error("Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        Ast *init = parse_expression(next_token(), 0, scope);
        if (var_type(lhs) != var_type(init)) {
            error("Can't initialize variable '%s' of type '%s' with value of type '%s'.", t->sval, type_as_str(next->tval), type_as_str(var_type(init)));
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

Ast *parse_statement(Tok *t, Ast *scope) {
    Ast *ast;
    if (t->type == TOK_ID && peek_token() != NULL && peek_token()->type == TOK_COLON) {
        next_token();
        ast = parse_declaration(t, scope);
    } else if (t->type == TOK_LBRACE) {
        return parse_scope(scope);
    } else if (t->type == TOK_IF) {
        return parse_conditional(scope);
    } else {
        ast = parse_expression(t, 0, scope);
    }
    Tok *next = next_token();
    if (next == NULL || next->type != TOK_SEMI) {
        error("Invalid end to statement: '%s'.", to_string(next));
    }
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
    if (var_type(cond->condition) != BOOL_T) {
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
        statements[i] = parse_statement(t, scope);
        i++;
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
