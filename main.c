#include "compiler.h"
#include "util.h"

static int _indent = 0;

void emit(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void endl() {
    printf("\n"); 
}

void indent() {
    for (int i = 0; i < _indent; i++) {
        printf("    ");
    }
}

void label(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void emit_string_comparison(Ast *ast) {
    // check if left or right is a literal
    //
}

void emit_comparison(Ast *ast) {
    if (var_type(ast->left) == STRING_T) {
        emit_string_comparison(ast);
        return;
    }
    printf("(");
    compile(ast->left);
    printf(" %s ", op_to_str(ast->op));
    compile(ast->right);
    printf(" ? 1 : 0)");
}

void emit_string_binop(Ast *ast) {
    compile(ast->left);
    printf(";\n");
    indent();
    switch (ast->right->type) {
    case AST_IDENTIFIER:
        printf("append_string(_tmp%d,_tr_%s->bytes,_tr_%s->len)", ast->left->tmpvar->offset, ast->right->var->name, ast->right->var->name);
        break;
    case AST_TEMP_VAR:
        compile(ast->right);
        printf(";\n");
        indent();
        printf("append_string(_tmp%d,_tmp%d->bytes,_tmp%d->len)", ast->left->tmpvar->offset, ast->right->tmpvar->offset, ast->right->tmpvar->offset); 
        break;
    case AST_STRING:
        printf("append_string(_tmp%d,\"", ast->left->tmpvar->offset);
        print_quoted_string(ast->right->sval);
        printf("\",%d)", (int) escaped_strlen(ast->right->sval));
        break;
    default:
        error("Couldn't do the string binop?");
    }
}

void emit_binop(Ast *ast) {
    if (ast->op == OP_ASSIGN) {
        if (is_dynamic(var_type(ast->left))) {
            if (ast->left->var->initialized) {
                compile(ast->right);
                printf(";\n");
                indent();
                printf("SWAP(_tr_%s,_tmp%d)", ast->left->var->name, ast->right->tmpvar->offset);
            } else {
                printf("_tr_%s = ", ast->left->var->name);
                compile(ast->right);
                ast->left->var->initialized = 1;
                if (ast->right->type == AST_TEMP_VAR) {
                    ast->right->tmpvar->consumed = 1;
                }
            }
        } else {
            printf("_tr_%s = ", ast->left->var->name);
            compile(ast->right);
        }
        return;
    }
    if (is_comparison(ast->op)) {
        emit_comparison(ast);
        return;
    } else if (var_type(ast->left) == STRING_T) {
        emit_string_binop(ast);
        return;
    } else if (ast->op == OP_OR) {
        printf("(");
        compile(ast->left);
        printf(") || (");
        compile(ast->right);
        printf(")");
        return;
    } else if (ast->op == OP_AND) {
        printf("(");
        compile(ast->left);
        printf(") && ("); // does this short-circuit?
        compile(ast->right);
        printf(")");
        return;
    }
    printf("(");
    compile(ast->left);
    printf(" %s ", op_to_str(ast->op));
    compile(ast->right);
    printf(")");
}

void emit_tmpvar(Ast *ast) {
    if (ast->expr->type == AST_STRING) {
        printf("(_tmp%d = init_string(", ast->tmpvar->offset);
        printf("\"");
        print_quoted_string(ast->expr->sval);
        printf("\"");
        printf("))");
    } else if (ast->expr->type == AST_IDENTIFIER) {
        printf("(_tmp%d = copy_string(_tr_%s))", ast->tmpvar->offset, ast->expr->var->name);
    } else if (ast->expr->type == AST_BINOP && var_type(ast->expr) == STRING_T) {
        /*printf("_tmp%d = ", ast->tmpvar->offset);*/
        emit_string_binop(ast->expr);
    } else {
        error("idk");
    }
}

void emit_decl(Ast *ast) {
    switch (ast->decl_var->type) {
    case INT_T:
        printf("int ");
        break;
    case BOOL_T:
        printf("unsigned char ");
        break;
    case STRING_T:
        printf("struct string_type *");
        break;
    default:
        error("wtf");
    }
    printf("_tr_%s", ast->decl_var->name);
    if (ast->init != NULL) {
        printf(" = ");
        compile(ast->init);
        if (ast->init->type == AST_TEMP_VAR) {
            ast->init->tmpvar->consumed = 1;
        }
        ast->decl_var->initialized = 1;
    }
}

void compile(Ast *ast) {
    switch (ast->type) {
    case AST_INTEGER:
        printf("%d", ast->ival);
        break;
    case AST_BOOL:
        printf("%d", ast->ival);
        break;
    case AST_STRING:
        printf("\"");
        print_quoted_string(ast->sval);
        printf("\"");
        break;
    case AST_BINOP:
        emit_binop(ast);
        break;
    case AST_TEMP_VAR:
        emit_tmpvar(ast);
        break;
    case AST_IDENTIFIER: {
        printf("_tr_%s", ast->var->name);
        break;
    }
    case AST_DECL: {
        emit_decl(ast);
        break;
    }
    case AST_CALL:
        printf("%s(", ast->fn);
        for (int i = 0; i < ast->nargs; i++) {
            compile(ast->args[i]);
            if (i != ast->nargs-1) {
                printf(",");
            }
        }
        printf(")");
        break;
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            indent();
            compile(ast->statements[i]);
            if (ast->statements[i]->type != AST_CONDITIONAL) {
                printf(";\n");
            }
        }
        break;
    case AST_SCOPE:
        emit_scope_start(ast);
        compile(ast->body);
        emit_scope_end(ast);
        break;
    case AST_CONDITIONAL:
        printf("if ((");
        compile(ast->condition);
        printf(") == 1) {\n");
        _indent++;
        compile(ast->if_body);
        _indent--;
        if (ast->else_body != NULL) {
            indent();
            printf("} else {\n");
            _indent++;
            compile(ast->else_body);
            _indent--;
        }
        indent();
        printf("}\n");
        break;
    default:
        error("No idea how to deal with this.");
    }
}

void emit_scope_start(Ast *scope) {
    printf("{\n");
    _indent++;
    Var *var = scope->locals;
    while (var != NULL) {
        if (var->temp) {
            indent();
            switch (var->type) {
            case INT_T:
                printf("int ");
                break;
            case BOOL_T:
                printf("unsigned char ");
                break;
            case STRING_T:
                printf("struct string_type *");
                break;
            default:
                error("wtf");
            }
            printf("_tmp%d = NULL;\n", var->offset);
        }
        var = var->next;
    }
}

void emit_free(Var *var) {
    if ((!var->temp && !var->initialized) || (var->temp && var->consumed)) {
        return;
    }
    switch (var->type) {
    case STRING_T:
        if (var->temp) {
            indent();
            printf("if (_tmp%d != NULL) {\n", var->offset); // maybe skip these
            indent();
            printf("    free(_tmp%d->bytes);\n", var->offset);
            indent();
            printf("    free(_tmp%d);\n", var->offset);
            indent();
            printf("}\n");
        } else {
            indent();
            printf("if (_tr_%s != NULL) {\n", var->name);
            indent();
            printf("    free(_tr_%s->bytes);\n", var->name);
            indent();
            printf("    free(_tr_%s);\n", var->name);
            indent();
            printf("}\n");
        }
        break;
    case INT_T:
    case BOOL_T:
    default:
        break;
    }
}

void emit_free_locals(Ast *scope) {
    while (scope->locals != NULL) {
        emit_free(scope->locals);
        scope->locals = scope->locals->next;
    }
}

void emit_scope_end(Ast *scope) {
    emit_free_locals(scope);
    _indent--;
    indent();
    printf("}\n");
}

void emit_func_start() {
}

void emit_func_end() {
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
        printf("#include \"prelude.c\"\n");
        _indent = 0;
        printf("int main(int argc, char** argv) ");
        emit_scope_start(root);
        indent();
        printf("int _tr_exit_code = 0;\n");
        compile(root->body);
        emit_free_locals(root);
        indent();
        printf("return _tr_exit_code;\n");
        _indent--;
        indent();
        printf("}\n");
    }
    return 0;
}
