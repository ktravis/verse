#include "src/compiler.h"
#include "src/util.h"

static int _indent = 0;

void indent() {
    for (int i = 0; i < _indent; i++) {
        printf("    ");
    }
}

void emit_string_comparison(Ast *ast) {
    if (ast->op == OP_NEQUALS) {
        printf("!");
    } else if (ast->op != OP_EQUALS) {
        error(ast->line, "Comparison of type '%s' is not valid for type 'string'.", op_to_str(ast->op));
    }
    if (ast->left->type == AST_STRING && ast->right->type == AST_STRING) {
        printf("%d", strcmp(ast->left->sval, ast->right->sval) ? 0 : 1);
        return;
    }
    if (ast->left->type == AST_STRING) {
        printf("streq_lit(");
        compile(ast->right);
        printf(",\"");
        print_quoted_string(ast->left->sval);
        printf("\",%d)", escaped_strlen(ast->left->sval));
    } else if (ast->right->type == AST_STRING) {
        printf("streq_lit(");
        compile(ast->left);
        printf(",\"");
        print_quoted_string(ast->right->sval);
        printf("\",%d)", escaped_strlen(ast->right->sval));
    } else {
        printf("streq(");
        compile(ast->left);
        printf(",");
        compile(ast->right);
        printf(")");
    }
}

void emit_comparison(Ast *ast) {
    if (var_type(ast->left)->base == STRING_T) {
        emit_string_comparison(ast);
        return;
    }
    printf("(");
    compile(ast->left);
    printf(" %s ", op_to_str(ast->op));
    compile(ast->right);
    printf(")");
}

void emit_string_binop(Ast *ast) {
    switch (ast->right->type) {
    case AST_CALL: // is this right? need to do anything else?
    case AST_IDENTIFIER:
    case AST_DOT:
    case AST_UOP:
    case AST_TEMP_VAR:
        printf("append_string(");
        compile(ast->left);
        printf(",");
        compile(ast->right);
        printf(")");
        break;
    case AST_STRING:
        printf("append_string_lit(");
        compile(ast->left);
        printf(",\"");
        print_quoted_string(ast->right->sval);
        printf("\",%d)", (int) escaped_strlen(ast->right->sval));
        break;
    default:
        error(-1, "Couldn't do the string binop? %d", ast->type);
    }
}

void emit_dot_op(Ast *ast) {
    compile(ast->dot_left);
    if (var_type(ast->dot_left)->base == PTR_T || (ast->dot_left->type == AST_IDENTIFIER && ast->dot_left->var->type->held)) {
        printf("->%s", ast->member_name);
    } else {
        printf(".%s", ast->member_name);
    }
}

void emit_uop(Ast *ast) {
    if (ast->op == OP_NOT) {
        printf("!");
    } else if (ast->op == OP_AT) {
        if (!is_dynamic(var_type(ast->right)->inner)) {
            printf("*");
        }
    } else if (ast->op == OP_ADDR) {
        if (!(ast->right->type == AST_IDENTIFIER && ast->right->var->type->held) &&
                var_type(ast->right)->base != STRING_T) {
            printf("&");
        }
    } else {
        error(ast->line,"Unkown unary operator '%s' (%s).", op_to_str(ast->op), ast->op);
    }
    compile(ast->right);
}

void emit_binop(Ast *ast) {
    if (ast->op == OP_ASSIGN) {
        if (is_dynamic(var_type(ast->left))) {
            if (ast->left->type == AST_DOT) {
                compile(ast->right);
                printf(";\n");
                indent();
                printf("SWAP(");
                compile(ast->left);
                printf(",_tmp%d)", ast->right->tmpvar->id);
            } else {
                Var *l = get_ast_var(ast->left);
                if (l->initialized) {
                    compile(ast->right);
                    printf(";\n");
                    indent();
                    printf("SWAP(_vs_%s,_tmp%d)", l->name, ast->right->tmpvar->id);
                } else {
                    printf("_vs_%s = ", l->name);
                    compile(ast->right);
                    l->initialized = 1;
                }
            }
        } else {
            compile(ast->left);
            printf(" = ");
            compile(ast->right);
        }
        if (ast->right->type == AST_TEMP_VAR) {
            ast->right->tmpvar->consumed = 1;
        }
        return;
    }
    if (is_comparison(ast->op)) {
        emit_comparison(ast);
        return;
    } else if (var_type(ast->left)->base == STRING_T) {
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
    // TODO this seems wrong
    if (ast->tmpvar->type->base == FN_T) {
        compile(ast->expr);
    } else if (ast->expr->type == AST_STRING) {
        printf("(_tmp%d = init_string(", ast->tmpvar->id);
        printf("\"");
        print_quoted_string(ast->expr->sval);
        printf("\"");
        printf("))");
    } else if (ast->expr->type == AST_IDENTIFIER) {
        switch (var_type(ast->expr)->base) {
        case STRING_T:
            printf("(_tmp%d = copy_string(_vs_%s))", ast->tmpvar->id, ast->expr->var->name);
            break;
        case STRUCT_T: {
            StructType *st = get_struct_type(ast->expr->var->type->struct_id);
            printf("_copy_%s(_vs_%s)", st->name, ast->expr->var->name);
            break;
        }
        default:
            error(-1, "wtf %d", var_type(ast->expr)->base);
        }
    } else if (ast->expr->type == AST_BINOP && var_type(ast->expr)->base == STRING_T) {
        emit_string_binop(ast->expr);
    } else if (ast->expr->type == AST_CALL) {
        printf("(_tmp%d = ", ast->tmpvar->id);
        if (ast->tmpvar->type->base != STRING_T && ast->tmpvar->type->base != FN_T) {
            printf("&");
        }
        compile(ast->expr);
        printf(")");
    } else if (ast->expr->type == AST_DOT || (ast->expr->type == AST_UOP && ast->expr->op == OP_AT)) {
        // TODO this is probably wrong
        switch(var_type(ast->expr)->base) {
        case STRING_T:
            printf("(_tmp%d = copy_string(", ast->tmpvar->id);
            compile(ast->expr);
            printf("))");
            break;
        case STRUCT_T: {
            StructType *st = get_struct_type(var_type(ast->expr)->struct_id);
            printf("(_tmp%d = &_copy_%s(", ast->tmpvar->id, st->name);
            compile(ast->expr);
            printf("))");
            break;
        }
        }
    } else {
        switch (var_type(ast->expr)->base) {
        case STRING_T:
            printf("(_tmp%d = copy_string(", ast->tmpvar->id);
            compile(ast->expr);
            printf("))");
            break;
        case STRUCT_T: {
            StructType *st = get_struct_type(var_type(ast->expr)->struct_id);
            printf("_copy_%s(", st->name);
            compile(ast->expr);
            printf(")");
            break;
        }
        default:
            error(-1, "wtf %d", var_type(ast->expr)->base);
        }
    }
}

void emit_type(Type *type) {
    switch (type->base) {
    case INT_T:
        printf("int ");
        break;
    case BOOL_T:
        printf("unsigned char ");
        break;
    case STRING_T:
        printf("struct string_type *");
        break;
    case FN_T:
        printf("fn_type ");
        break;
    case VOID_T:
        printf("void ");
        break;
    case BASEPTR_T:
        printf("ptr_type ");
        break;
    case PTR_T:
        emit_type(type->inner);
        if (type->inner->base != STRING_T) {
            printf("*");
        }
        break;
    case STRUCT_T: {
        StructType *st = get_struct_type(type->struct_id);
        if (st == NULL) {
            error(-1, "Cannot find struct '%d'.", type->struct_id);
        }
        printf("struct _vs_%s ", st->name);
        break;
    }
    default:
        error(-1, "wtf type");
    }
}

void emit_decl(Ast *ast) {
    if (ast->decl_var->type->base == FN_T) {
        printf("fn_type _vs_%s", ast->decl_var->name);
    } else {
        emit_type(ast->decl_var->type);
        printf("_vs_%s", ast->decl_var->name);
    }
    if (ast->init == NULL) {
        if (ast->decl_var->type->base == STRING_T) {
            printf(" = init_string(\"\")");
            ast->decl_var->initialized = 1;
        } else if (ast->decl_var->type->base == STRUCT_T) {
            StructType *st = get_struct_type(ast->decl_var->type->struct_id);
            printf(";\n");
            indent();
            printf("_init_%s(&_vs_%s)", st->name, ast->decl_var->name);
            ast->decl_var->initialized = 1;
        } else if (ast->decl_var->type->base == BASEPTR_T) {
            printf(" = NULL");
        }
    } else {
        printf(" = ");
        compile(ast->init);
        if (ast->init->type == AST_TEMP_VAR) {
            ast->init->tmpvar->consumed = 1;
        }
        ast->decl_var->initialized = 1;
    }
}

void emit_func_decl(Ast *fn) {
    emit_type(fn->fn_decl_var->type->ret);
    printf("_vs_%s(", fn->fn_decl_var->name);
    for (int i = 0; i < fn->fn_decl_var->type->nargs; i++) {
        emit_type(fn->fn_decl_args[i]->type);
        printf("_vs_%s", fn->fn_decl_args[i]->name);
        if (i < fn->fn_decl_var->type->nargs - 1) {
            printf(",");
        }
    }
    printf(") ");
    compile(fn->fn_body);
}

void emit_struct_decl(Ast *ast) {
    StructType *st = get_struct_type(ast->struct_type->struct_id);
    printf("struct _vs_%s {\n", ast->struct_name);
    _indent++;
    for (int i = 0; i < st->nmembers; i++) {
        indent();
        emit_type(st->member_types[i]);
        printf("%s;\n", st->member_names[i]);
    }
    _indent--;
    indent();
    printf("};\n");
    printf("struct _vs_%s *_init_%s(struct _vs_%s *x) {\n", ast->struct_name, ast->struct_name, ast->struct_name);
    _indent++;
    indent();
    printf("if (x == NULL) {\n");
    _indent++;
    indent();
    printf("x = malloc(sizeof(struct _vs_%s));\n", ast->struct_name);
    _indent--;
    indent();
    printf("}\n");
    for (int i = 0; i < st->nmembers; i++) {
        Type *t = st->member_types[i];
        switch (t->base) {
        case STRING_T:
            indent();
            printf("x->%s = NULL;\n", st->member_names[i]);
            break;
        case STRUCT_T:
            indent();
            StructType *m = get_struct_type(t->struct_id);
            printf("x->%s = _init_%s(x->%s);\n", st->member_names[i], m->name, st->member_names[i]);
            break;
        case PTR_T: case BASEPTR_T:
            indent();
            printf("x->%s = NULL;\n", st->member_names[i]);
            break;
        }
    }
    indent();
    printf("return x;\n");
    _indent--;
    indent();
    printf("}\n");
    printf("struct _vs_%s _copy_%s(struct _vs_%s x) {\n", ast->struct_name, ast->struct_name, ast->struct_name);
    _indent++;
    for (int i = 0; i < st->nmembers; i++) {
        if (st->member_types[i]->base == STRING_T) {
            indent();
            printf("x.%s = copy_string(x.%s);\n", st->member_names[i], st->member_names[i]);
        } else if (st->member_types[i]->base == STRUCT_T) {
            indent();
            StructType *m = get_struct_type(st->member_types[i]->struct_id);
            printf("x.%s = _copy_%s(x.%s);\n", st->member_names[i], m->name, st->member_names[i]);
        }
    }
    indent();
    printf("return x;\n");
    _indent--;
    indent();
    printf("}\n");
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
    case AST_STRUCT:
        printf("(struct _vs_%s){", ast->struct_lit_type->name);
        if (ast->nmembers == 0) {
            printf("0");
        } else {
            for (int i = 0; i < ast->nmembers; i++) {
                printf(".%s = ", ast->member_names[i]);
                compile(ast->member_exprs[i]);
                if (i != ast->nmembers - 1) {
                    printf(", ");
                }
            }
        }
        printf("}");
        break;
    case AST_DOT:
        emit_dot_op(ast);
        break;
    case AST_UOP:
        emit_uop(ast);
        break;
    case AST_BINOP:
        emit_binop(ast);
        break;
    case AST_STRUCT_DECL:
        emit_struct_decl(ast);
        break;
    case AST_TEMP_VAR:
        emit_tmpvar(ast);
        break;
    case AST_HOLD:
        if (ast->type == AST_STRUCT) {
        } else {
            switch (var_type(ast->expr)->base) {
            case STRING_T:
                emit_tmpvar(ast->expr);
                break;
            default:
                printf("(_tmp%d = ", ast->tmpvar->id);
                printf("malloc(sizeof(");
                emit_type(var_type(ast->expr));
                printf(")), *_tmp%d = ", ast->tmpvar->id);
                compile(ast->expr);
                printf(", _tmp%d", ast->tmpvar->id);
                printf(")");
            }
        }
        break;
    case AST_IDENTIFIER:
        if (!ast->var->ext) {
            printf("_vs_");
        }
        printf("%s", ast->var->name);
        break;
    case AST_RETURN:
        if (ast->ret_expr != NULL) {// && ast->ret_expr->type != AST_IDENTIFIER) {
            if (ast->ret_expr->type == AST_TEMP_VAR || ast->ret_expr->type == AST_ANON_FUNC_DECL) {
                ast->ret_expr->tmpvar->consumed = 1;
            }
            emit_type(var_type(ast->ret_expr));
            printf("_ret = ");
            compile(ast->ret_expr); // need to do something with tmpvar instead
            printf(";");
        }
        printf("\n");
        emit_free_locals(ast->fn_scope);
        indent();
        printf("return");
        if (ast->ret_expr != NULL) {
            if (ast->ret_expr->type == AST_IDENTIFIER) {
                printf(" ");
                compile(ast->ret_expr);
            } else {
                printf(" _ret");
            }
        }
        break;
    case AST_RELEASE:
        if (ast->release_target->type == AST_IDENTIFIER) {
            emit_free(ast->release_target->var);
        } else if (ast->release_target->type == AST_DOT) {
            indent();
            printf ("{\n");
            _indent++;
            indent();
            Var *v = make_var("0", var_type(ast->release_target));
            emit_type(v->type);
            printf("_vs_0 = ");
            compile(ast->release_target);
            printf(";\n");
            emit_free(v);
            _indent--;
            printf("}");
        }
        break;
    case AST_BREAK:
        printf("break");
        break;
    case AST_CONTINUE:
        printf("continue");
        break;
    case AST_DECL:
        emit_decl(ast);
        break;
    case AST_FUNC_DECL: 
        break;
    case AST_ANON_FUNC_DECL: 
        if (ast->tmpvar->type->bindings != NULL) {
            int id = ast->fn_decl_var->type->bindings_id;
            TypeList *bindings = ast->fn_decl_var->type->bindings;
            AstList *bind_exprs = get_binding_exprs(id);

            printf("(_cl_%d = malloc(%d),", id, bindings->item->offset + var_size(bindings->item));
            bindings = reverse_typelist(bindings);
            bind_exprs = reverse_astlist(bind_exprs);
            while (bindings != NULL && bind_exprs != NULL) {
                printf("*((");
                emit_type(bindings->item);
                printf("*)(_cl_%d+%d))=", id, bindings->item->offset);
                compile(bind_exprs->item);
                if (bind_exprs->item->type == AST_TEMP_VAR) {
                    bind_exprs->item->tmpvar->consumed = 1;
                }
                printf(",");
                bindings = bindings->next;
                bind_exprs = bind_exprs->next;
            }
            if (bindings != NULL || bind_exprs != NULL) {
                error(-1, "Length mismatch between binding types and expressions.");
            }
        }
        /*compile(ast->expr);*/
        printf("_vs_%s", ast->fn_decl_var->name);
        if (ast->tmpvar->type->bindings != NULL) {
            printf(")");
        }
        break;
    case AST_EXTERN_FUNC_DECL: 
        break;
    case AST_CALL: {
        /*if (ast->fn->type != AST_IDENTIFIER) {*/
            Type *t = var_type(ast->fn);
            printf("((");
            emit_type(t->ret);
            printf("(*)(");
            if (t->nargs == 0) {
                printf("void");
            } else {
                for (int i = 0; i < t->nargs; i++) {
                    emit_type(t->args[i]);
                    if (i < t->nargs-1) {
                        printf(",");
                    }
                }
            }
            printf("))(");
            compile(ast->fn);
            printf("))");
        /*} else {*/
            /*compile(ast->fn);*/
        /*}*/
        printf("(");
        for (int i = 0; i < ast->nargs; i++) {
            Type *t = var_type(ast->args[i]);
            if (t->base == STRUCT_T && is_dynamic(t)) {
                StructType *st = get_struct_type(t->struct_id);
                printf("_copy_%s(", st->name);
                compile(ast->args[i]);
                printf(")");
            } else {
                compile(ast->args[i]);
            }
            if (ast->args[i]->type == AST_TEMP_VAR && is_dynamic(t)) {
                ast->args[i]->tmpvar->consumed = 1;
            }
            if (i != ast->nargs-1) {
                printf(",");
            }
        }
        printf(")");
        break;
    }
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            if (ast->statements[i]->type == AST_FUNC_DECL || ast->statements[i]->type == AST_EXTERN_FUNC_DECL || ast->statements[i]->type == AST_STRUCT_DECL) {
                continue;
            }
            if (ast->statements[i]->type != AST_RELEASE) {
                indent();
            }
            compile(ast->statements[i]);
            if (ast->statements[i]->type != AST_CONDITIONAL && ast->statements[i]->type != AST_RELEASE && ast->statements[i]->type != AST_WHILE) {
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
        printf("if (");
        compile(ast->condition);
        printf(") {\n");
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
    case AST_WHILE:
        printf("while (");
        compile(ast->while_condition);
        printf(") {\n");
        _indent++;
        compile(ast->while_body);
        _indent--;
        indent();
        printf("}\n");
        break;
    case AST_BIND:
        printf("*((");
        emit_type(var_type(ast->bind_expr));
        printf("*)_cl_%d+%d)", ast->bind_id, ast->bind_offset);
        break;
    default:
        error(ast->line, "No idea how to deal with this.");
    }
}

void emit_scope_start(Ast *scope) {
    printf("{\n");
    _indent++;
    VarList *list = scope->locals;
    while (list != NULL) {
        if (list->item->temp) {
            indent();
            emit_type(list->item->type);
            printf("_tmp%d", list->item->id);
            printf("= NULL");
            printf(";\n");
        }
        list = list->next;
    }
}

void emit_free_struct(char *name, StructType *st, int is_ptr) {
    char *sep = is_ptr ? "->" : ".";
    for (int i = 0; i < st->nmembers; i++) {
        switch (st->member_types[i]->base) {
        case STRING_T:
            indent();
            printf("if (%s%s%s != NULL) {\n", name, sep, st->member_names[i]);
            _indent++;
            indent();
            printf("free(%s%s%s->bytes);\n", name, sep, st->member_names[i]);
            indent();
            printf("free(%s%s%s);\n", name, sep, st->member_names[i]);
            _indent--;
            indent();
            printf("}\n");
            break;
        case STRUCT_T: {
            char *memname = malloc(sizeof(char) * (strlen(name) + strlen(sep) + strlen(st->member_names[i]) + 2));
            sprintf(memname, "%s%s%s", name, sep, st->member_names[i]);
            emit_free_struct(memname, get_struct_type(st->member_types[i]->struct_id), (st->member_types[i]->base == PTR_T || st->member_types[i]->base == BASEPTR_T));
            free(memname);
            break;
        }
        }
    }
}

void emit_free(Var *var) {
    if (var->type->bindings != NULL) {
        emit_free_bindings(var->type->bindings_id, var->type->bindings);
        indent();
        printf("free(_cl_%d);\n", var->type->bindings_id);
        return;
    }
    switch (var->type->base) {
    case BASEPTR_T:
        indent();
        printf("free(_vs_%s);\n", var->name);
        break;
    case PTR_T:
        if (var->type->inner->base == STRUCT_T) {
            char *name = malloc(sizeof(char) * (strlen(var->name) + 5));
            sprintf(name, "_vs_%s", var->name);
            emit_free_struct(name, get_struct_type(var->type->inner->struct_id), 1);
            free(name);
        } else if (var->type->inner->base == STRING_T) { // TODO should this behave this way?
            indent();
            printf("free(_vs_%s->bytes);\n", var->name);
        }
        indent();
        printf("free(_vs_%s);\n", var->name);
        break;
    case STRUCT_T: {
        char *name;
        if (var->temp) {
            name = malloc(sizeof(char) * (snprintf(NULL, 0, "%d", var->id) + 5));
            sprintf(name, "_tmp%d", var->id);
        } else {
            name = malloc(sizeof(char) * (strlen(var->name) + 5));
            sprintf(name, "_vs_%s", var->name);
        }
        emit_free_struct(name, get_struct_type(var->type->struct_id), 0);
        free(name);
        break;
    }
    case STRING_T:
        if (var->temp) {
            indent();
            printf("if (_tmp%d != NULL) {\n", var->id); // maybe skip these
            indent();
            printf("    free(_tmp%d->bytes);\n", var->id);
            indent();
            printf("    free(_tmp%d);\n", var->id);
            indent();
            printf("}\n");
        } else {
            indent();
            printf("if (_vs_%s != NULL) {\n", var->name);
            indent();
            printf("    free(_vs_%s->bytes);\n", var->name);
            indent();
            printf("    free(_vs_%s);\n", var->name);
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

// TODO cleanup all emit_free_* funcs
void emit_free_bindings(int id, TypeList *bindings) {
    while (bindings != NULL) {
        Type *t = bindings->item;
        switch (t->base) {
        case BASEPTR_T:
            indent();
            printf("free(*((void **)_cl_%d+%d));\n", id, t->offset);
            break;
        case PTR_T:
            if (t->inner->base == STRUCT_T) {
                char *tname = type_as_str(t);
                char *name = malloc(sizeof(char) * (strlen(tname) + 12 + sprintf(NULL, "%d", t->offset) + sprintf(NULL, "%d", id)));
                // TODO recount name length
                sprintf(name, "(*((%s*)_cl_%d+%d))", tname, id, t->offset);
                emit_free_struct(name, get_struct_type(t->inner->struct_id), 1);
                free(tname);
                free(name);
            } else if (t->inner->base == STRING_T) { // TODO should this behave this way?
                indent();
                printf("free((*((struct string_type**)_cl_%d+%d))->bytes);\n", id, t->offset);
            }
            indent();
            printf("free(*((void **)_cl_%d+%d));\n", id, t->offset);
            break;
        case STRUCT_T: {
            char *tname = type_as_str(t);
            char *name = malloc(sizeof(char) * (strlen(tname) + 10 + sprintf(NULL, "%d", t->offset) + sprintf(NULL, "%d", id)));
            sprintf(name, "(*((%s*)_cl_%d+%d))", tname, id, t->offset);
            emit_free_struct(name, get_struct_type(t->inner->struct_id), 1);
            free(name);
            free(tname);
            break;
        }
        case STRING_T:
            indent();
            printf("if (*((struct string_type **)_cl_%d+%d) != NULL) {\n", id, t->offset);
            indent();
            printf("    free((*((struct string_type **)_cl_%d+%d))->bytes);\n", id, t->offset);
            indent();
            printf("    free(*((struct string_type **)_cl_%d+%d));\n", id, t->offset);
            indent();
            printf("}\n");
            break;
        default:
            break;
        }
        bindings = bindings->next;
    }
}

void emit_free_locals(Ast *scope) {
    while (scope->locals != NULL) {
        Var *v = scope->locals->item;
        scope->locals = scope->locals->next;
        // TODO got to be a better way to handle this here
        // TODO maybe remove the detach_var and check for temp?
        if (v->consumed || (v->type->base != FN_T && ((v->type->base == BASEPTR_T || v->type->base == PTR_T) ||
                v->type->held || (!v->temp && !v->initialized) || (v->temp && v->consumed)))) {
            continue;
        }
        emit_free(v);
    }
}

void emit_scope_end(Ast *scope) {
    emit_free_locals(scope);
    _indent--;
    indent();
    printf("}\n");
}

void emit_var_decl(Var *v) {
    if (v->ext) {
        printf("extern ");
    }
    if (v->type->base == FN_T) {
        emit_type(v->type->ret);
        printf("(*");
    } else {
        emit_type(v->type);
    }
    if (!v->ext) {
        printf("_vs_");
    }
    printf("%s", v->name);
    if (v->type->base == FN_T) {
        printf(")(");
        for (int i = 0; i < v->type->nargs; i++) {
            emit_type(v->type->args[i]);
            printf("a%d", i);
            if (i < v->type->nargs - 1) {
                printf(",");
            }
        }
        printf(")");
    }
    printf(";\n");
}
void emit_forward_decl(Var *v) {
    if (v->ext) {
        printf("extern ");
    }
    emit_type(v->type->ret);
    if (!v->ext) {
        printf("_vs_");
    }
    printf("%s(", v->name);
    for (int i = 0; i < v->type->nargs; i++) {
        emit_type(v->type->args[i]);
        printf("a%d", i);
        if (i < v->type->nargs - 1) {
            printf(",");
        }
    }
    printf(");\n");
}

int main(int argc, char **argv) {
    int just_ast = 0;
    if (argc > 1 && !strcmp(argv[1], "-a")) {
        just_ast = 1;
    }
    init_builtins();
    Ast *root = parse_scope(NULL);
    root = parse_semantics(root, root);
    if (just_ast) {
        print_ast(root);
    } else {
        printf("#include \"prelude.c\"\n");
        _indent = 0;
        VarList *varlist = get_global_vars();
        while (varlist != NULL) {
            emit_var_decl(varlist->item);
            varlist = varlist->next;
        }
        varlist = get_global_bindings();
        while (varlist != NULL) {
            printf("char *_cl_%d = NULL;\n", varlist->item->id);
            varlist = varlist->next;
        }
        AstList *stlist = get_global_structs();
        while (stlist != NULL) {
            emit_struct_decl(stlist->item);
            stlist = stlist->next;
        }
        AstList *fnlist = get_global_funcs();
        while (fnlist != NULL) {
            emit_forward_decl(fnlist->item->fn_decl_var);
            fnlist = fnlist->next;
        }
        fnlist = get_global_funcs();
        while (fnlist != NULL) {
            emit_func_decl(fnlist->item);
            fnlist = fnlist->next;
        }
        printf("void _verse_init() ");
        compile(root);
        printf("int main(int argc, char** argv) {\n"
               "    _verse_init();\n"
               "    return _vs_main();\n"
               "}");
    }
    return 0;
}
