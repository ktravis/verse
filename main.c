#include "compiler.h"
#include "util.h"
#include "token.h"

void print_quoted_string(char *val) {
    for (char *c = val; *c; c++) {
        if (*c == '\"') {// || *c == '\\') {
            printf("\\");
        }
        printf("%c", *c);
    }
}

Var *make_var(char *name, int type, Ast *scope) {
    Var *var = malloc(sizeof(Var));
    var->name = malloc(strlen(name)+1);
    strcpy(var->name, name);
    var->type = type;
    var->offset = scope->locals == NULL ? 8 : scope->locals->offset + type_offset(scope->locals->type);
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
    case AST_CALL:
        error("idk how to tell function stuff");
    case AST_BINOP:
        return var_type(ast->left);
    default:
        error("don't know how to infer vartype");
    }
    return VOID_T;
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
        print_quoted_string(ast->sval);
        break;
    case AST_BINOP:
        printf("(%s ", op_to_str(ast->op));
        print_ast(ast->left);
        printf(" ");
        print_ast(ast->right);
        printf(")");
        break;
    case AST_IDENTIFIER:
        printf("%s", ast->var->name);
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

Ast *make_ast_binop(int op, Ast *left, Ast *right) {
    if (op == '=' && left->type != AST_IDENTIFIER) {
        error("LHS of assignment is not an identifier.");
    }
    if (var_type(left) != var_type(right)) {
        error("LHS of expression has type '%s', while RHS has type '%s'.",
                type_as_str(left->var->type), type_as_str(var_type(right)));
    }
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

Ast *make_ast_id(char *name, int type, Ast *scope) {
    Ast *ast = malloc(sizeof(Ast));
    ast->type = AST_IDENTIFIER;
    ast->var = make_var(name, type, scope);
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

    Ast *lhs = make_ast_id(t->sval, next->tval, scope);

    next = next_token();
    if (next == NULL) {
        error("Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        return make_ast_binop(OP_ASSIGN, lhs, parse_expression(next_token(), 0, scope));
    } else if (next->type != TOK_SEMI) {
        error("Unexpected token '%s' while parsing declaration.", to_string(next));
    }
    unget_token(next);
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

void emit_data_section() {
    printf("\t.data\n");
    Ast *str = strings;
    while (str != NULL) {
        printf(".s%d:\n\t", str->sid);
        printf(".string \"");
        print_quoted_string(str->sval);
        printf("\"\n");
        str = str->snext;
    }
    printf("\t");
}

void emit_binop(Ast *ast) {
    if (ast->op == OP_ASSIGN) {
        compile(ast->right);
        char *reg = "rax";
        if (ast->left->var->type == INT_T) {
            reg = "eax";
        } else if (ast->left->var->type == BOOL_T) {
            reg = "al";
        }
        printf("mov %%%s, -%d(%%rbp)\n\t", reg, ast->left->var->offset);
        return;
    }
    if (ast->left->type == AST_STRING) {
        error("Invalid operation '%c' for strings.", ast->op);
    }
    char *asm_op = "";
    switch (ast->op) {
    case OP_PLUS: asm_op = "add"; break;
    case OP_MINUS: asm_op = "sub"; break;
    case OP_MUL: asm_op = "imul"; break;
    case OP_DIV: break;
    default:
        error("Unknown operator '%s'.", op_to_str(ast->op));
    }
    compile(ast->left);
    printf("push %%rax\n\t");
    compile(ast->right);
    if (ast->op == OP_DIV) {
        printf("mov %%rax, %%rbx\n\t");
        printf("pop %%rax\n\t");
        printf("mov $0, %%rdx\n\t");
        printf("idiv %%rbx\n\t");
    } else {
        printf("mov %%rax, %%rbx\n\t");
        printf("pop %%rax\n\t");
        printf("%s %%rbx, %%rax\n\t", asm_op);
    }
}

void compile(Ast *ast) {
    switch (ast->type) {
    case AST_INTEGER:
        printf("mov $%d, %%eax\n\t", ast->ival);
        break;
    case AST_BOOL:
        printf("mov $%d, %%al\n\t", ast->ival);
        break;
    case AST_STRING:
        printf("lea .s%d(%%rip), %%rax\n\t", ast->sid);
        break;
    case AST_BINOP:
        emit_binop(ast);
        break;
    case AST_IDENTIFIER: {
        char *s = "rax";
        /*int size = type_offset(ast->var->type);*/
        /*if (size == 8) {*/
        /*} else if (size == 4) {*/
            /*s = "eax";*/
        /*} else if (size == 2) {*/
            /*s = "ax";*/
        /*} else if (size == 1) {*/
            /*s = "al";*/
        /*}*/
        printf("mov -%d(%%rbp), %%%s\n\t", ast->var->offset, s);
        break;
    }
    case AST_CALL:
        // save regs
        /*for (int i = 1; i < ast->nargs; i++) {*/
            /*printf("push %s\n\t", ARG_REGS[i]);*/
        /*}*/
        // put args onto stack
        for (int i = 0; i < ast->nargs; i++) {
            compile(ast->args[i]);
            printf("push %%rax\n\t");
        }
        // pop stack into regs
        for (int i = ast->nargs - 1; i >= 0; i--) {
            printf("pop %s\n\t", ARG_REGS[i]);
        }
        printf("mov $0, %%eax\n\t");
        printf("call _%s\n\t", ast->fn);
        // restore regs
        /*for (int i = ast->nargs - 1; i >= 1; i--) {*/
            /*printf("pop %s\n\t", ARG_REGS[i]);*/
        /*}*/
        break;
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            compile(ast->statements[i]);
        }
        break;
    case AST_SCOPE:
        emit_scope_start(ast);
        compile(ast->body);
        emit_scope_end(ast);
        break;
    case AST_CONDITIONAL:
        compile(ast->condition);
        printf("testb $1, %%al\n\t");
        printf("je _else%d\n\t", ast->cond_id);
        compile(ast->if_body);
        printf("jmp _endif%d\n\t", ast->cond_id);
        printf("\n_else%d:\n\t", ast->cond_id);
        if (ast->else_body != NULL) {
            compile(ast->else_body);
        }
        printf("\n_endif%d:\n\t", ast->cond_id);
        break;
    default:
        error("No idea how to deal with this.");
    }
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
            realloc(statements, sizeof(Ast*)*n);
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

void emit_scope_start(Ast *scope) {
    printf("push %%rbp\n\t");
    printf("mov %%rsp, %%rbp\n\t");
    if (scope->locals != NULL) {
        int offset = (scope->locals->offset + type_offset(scope->locals->type))/16;
        printf("subq $%d, %%rsp\n\t", offset < 1 ? 16 : 16 * offset);
    }
}

void emit_scope_end(Ast *scope) {
    /*if (scope->locals != NULL) {*/
        /*int offset = (scope->locals->offset + type_offset(scope->locals->type))/16;*/
        /*printf("addq $%d, %%rsp\n\t", offset < 1 ? 16 : 16 * offset);*/
    /*}*/
    /*printf("leave\n\t");*/
    printf("mov %%rbp, %%rsp\n\t");
    printf("pop %%rbp\n\t");
}

void emit_func_start() {
}

void emit_func_end() {
    printf("ret\n");
}

int main(int argc, char **argv) {
    int just_ast = 0;
    if (argc > 1 && !strcmp(argv[1], "-a")) {
        just_ast = 1;
    }
    Ast *root = parse_scope(NULL);
    if (just_ast) {
        print_ast(root);
    } else {
        emit_data_section();
        printf(".text\n\t"
               ".global _asm_main\n"
               "_asm_main:\n\t");
        emit_func_start();
        compile(root);
        emit_func_end();
    }
    return 0;
}
