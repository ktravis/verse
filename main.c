#include "src/compiler.h"
#include "src/util.h"

#include "prelude.h"

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
    Type *t = var_type(ast->dot_left);
    if (t->base == STATIC_ARRAY_T || (t->base == PTR_T && t->inner->base == STATIC_ARRAY_T)) {
        if (!strcmp(ast->member_name, "length")) {
            printf("%ld", array_size(t));
        } else if (!strcmp(ast->member_name, "data")) {
            if (t->base == PTR_T) {
                printf("*");
            }
            compile(ast->dot_left);
        }
    } else {
        compile(ast->dot_left);
        if (t->base == PTR_T) {// || (ast->dot_left->type == AST_IDENTIFIER && ast->dot_left->var->type->held)) {
            printf("->%s", ast->member_name);
        } else {
            printf(".%s", ast->member_name);
        }
    }
}

void emit_uop(Ast *ast) {
    switch (ast->op) {
    case OP_NOT:
        printf("!"); break;
    case OP_ADDR:
        printf("&"); break;
    case OP_AT:
        printf("*"); break;
    case OP_MINUS:
        printf("-"); break;
    case OP_PLUS:
        printf("+"); break;
    default:
        error(ast->line,"Unkown unary operator '%s' (%s).", op_to_str(ast->op), ast->op);
    }
    compile(ast->right);
}

void emit_assignment(Ast *ast) {
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
}

void emit_binop(Ast *ast) {
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

void emit_copy(Ast *ast) {
    if (var_type(ast->copy_expr)->base == FN_T) {
        compile(ast->copy_expr);
    } else if (ast->copy_expr->type == AST_IDENTIFIER) {
        switch (var_type(ast->copy_expr)->base) {
        case STRING_T:
            printf("copy_string(_vs_%s)", ast->copy_expr->var->name);
            break;
        case STRUCT_T: {
            Var *v = ast->copy_expr->var;
            printf("_copy_%s(_vs_%s)", v->type->name, v->name);
            break;
        }
        default:
            error(-1, "wtf %d", var_type(ast->expr)->base);
        }
    } else if (ast->copy_expr->type == AST_DOT || (ast->copy_expr->type == AST_UOP && ast->copy_expr->op == OP_AT)) {
        // TODO this is probably wrong
        switch(var_type(ast->copy_expr)->base) {
        case STRING_T:
            printf("copy_string(");
            compile(ast->copy_expr);
            printf(")");
            break;
        case STRUCT_T: {
            printf("_copy_%s(", var_type(ast->copy_expr)->name);
            compile(ast->copy_expr);
            printf(")");
            break;
        }
        }
    } else {
        switch (var_type(ast->copy_expr)->base) {
        case STRING_T:
            printf("copy_string(");
            compile(ast->copy_expr);
            printf(")");
            break;
        case STRUCT_T: {
            printf("_copy_%s(", var_type(ast->copy_expr)->name);
            compile(ast->copy_expr);
            printf(")");
            break;
        }
        default:
            error(-1, "wtf %d", var_type(ast->copy_expr)->base);
        }
    }
}

void emit_tmpvar(Ast *ast) {
    if (ast->tmpvar->type->base == FN_T) {
        compile(ast->expr);
    } else {
        ast->tmpvar->initialized = 1;
        printf("(_tmp%d = ", ast->tmpvar->id);
        compile(ast->expr);
        printf(")");
    }
}

void emit_type(Type *type) {
    switch (type->base) {
    case UINT_T:
        printf("u");
    case INT_T:
        printf("int%d_t ", type->size * 8);
        break;
    case FLOAT_T:
        if (type->size == 4) { // TODO double-check these are always the right size
            printf("float ");
        } else if (type->size == 8) {
            printf("double ");
        } else {
            error(-1, "Cannot compile floating-point type of size %d.", type->size);
        }
        break;
    case BOOL_T:
        printf("unsigned char ");
        break;
    case STRING_T:
        printf("struct string_type ");
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
        printf("*");
        break;
    case ARRAY_T:
    case STATIC_ARRAY_T:
        printf("struct array_type ");
        break;
    case STRUCT_T: {
        if (type->named) {
            printf("struct _vs_%s ", type->name);
        } else {
            printf("struct _vs_%d ", type->id);
        }
        break;
    }
    default:
        error(-1, "wtf type");
    }
}

void emit_decl(Ast *ast) {
    /*if (ast->decl_var->type->base == STATIC_ARRAY_T && ast->init == NULL) {*/
        /*emit_type(ast->decl_var->type->inner);*/
        /*printf("_vs_%s", ast->decl_var->name);*/
    /*} else {*/
        emit_type(ast->decl_var->type);
        printf("_vs_%s", ast->decl_var->name);
    /*}*/
    if (ast->init == NULL) {
        /*if (ast->decl_var->type->base == STATIC_ARRAY_T) {*/
            /*printf("[%ld]", ast->decl_var->type->length);*/
        /*}*/
        if (ast->decl_var->type->base == STRING_T) {
            printf(" = (struct string_type){0}");
            ast->decl_var->initialized = 1;
        } else if (ast->decl_var->type->base == STRUCT_T) {
            printf(";\n");
            indent();
            if (ast->decl_var->type->named) {
                printf("_init_%s(&_vs_%s)", ast->decl_var->type->name, ast->decl_var->name);
            } else {
                printf("_init_%d(&_vs_%s)", ast->decl_var->type->id, ast->decl_var->name);
            }
            ast->decl_var->initialized = 1;
        } else if (ast->decl_var->type->base == STATIC_ARRAY_T) {
            printf(" = (struct array_type){.data=alloca(%ld),.length=%ld}", ast->decl_var->type->inner->size * ast->decl_var->type->length, ast->decl_var->type->length);
        } else if (ast->decl_var->type->base == BASEPTR_T || ast->decl_var->type->base == PTR_T)  {
            printf(" = NULL");
        } else if (is_numeric(ast->decl_var->type)) {
            printf(" = 0");
        } else if (is_array(ast->decl_var->type)) {
            printf(" = {0}");
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
    VarList *args = fn->fn_decl_args;
    while (args != NULL) {
        emit_type(args->item->type);
        printf("_vs_%s", args->item->name);
        if (args->next != NULL) {
            printf(",");
        }
        args = args->next;
    }
    printf(") ");
    compile(fn->fn_body);
}

void emit_struct_decl(Type *st) {
    emit_type(st);
    printf("{\n");
    _indent++;
    for (int i = 0; i < st->nmembers; i++) {
        indent();
        emit_type(st->member_types[i]);
        printf("%s;\n", st->member_names[i]);
    }
    _indent--;
    indent();
    printf("};\n");
    emit_type(st);
    if (st->named) {
        printf("*_init_%s(", st->name);
    } else {
        printf("*_init_%d(", st->id);
    }
    emit_type(st);
    printf("*x) {\n");
    _indent++;
    indent();
    printf("if (x == NULL) {\n");
    _indent++;
    indent();
    printf("x = malloc(sizeof(");
    emit_type(st);
    printf("));\n");
    _indent--;
    indent();
    printf("}\n");
    for (int i = 0; i < st->nmembers; i++) {
        Type *t = st->member_types[i];
        switch (t->base) {
        case INT_T: case UINT_T:
            indent();
            printf("x->%s = 0;\n", st->member_names[i]);
            break;
        case STRING_T:
            indent();
            printf("x->%s = (struct string_type){0};\n", st->member_names[i]);
            break;
        case STRUCT_T:
            indent();
            if (t->named) {
                printf("x->%s = _init_%s(x->%s);\n", st->member_names[i], t->name, st->member_names[i]);
            } else {
                printf("x->%s = _init_%d(x->%s);\n", st->member_names[i], t->id, st->member_names[i]);
            }
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
    emit_type(st);
    if (st->named) {
        printf("_copy_%s(", st->name);
    } else {
        printf("_copy_%d(", st->id);
    }
    emit_type(st);
    printf("x) {\n");
    _indent++;
    for (int i = 0; i < st->nmembers; i++) {
        if (st->member_types[i]->base == STRING_T) {
            indent();
            printf("x.%s = copy_string(x.%s);\n", st->member_names[i], st->member_names[i]);
        } else if (st->member_types[i]->base == STRUCT_T) {
            indent();
            printf("x.%s = ", st->member_names[i]);
            if (st->member_types[i]->named) {
                printf("_copy_%s(", st->member_types[i]->name);
            } else {
                printf("_copy_%d(", st->member_types[i]->id);
            }
            printf("x.%s);\n", st->member_names[i]);
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
        printf("%ld", ast->ival);
        break;
    case AST_FLOAT: // TODO this is truncated
        printf("%F", ast->fval);
        break;
    case AST_BOOL:
        printf("%d", (unsigned char)ast->ival);
        break;
    // TODO strlen is wrong for escaped chars \t etc
    case AST_STRING:
        printf("init_string(\"");
        print_quoted_string(ast->sval);
        printf("\", %d)", (int)strlen(ast->sval));
        break;
    case AST_STRUCT:
        printf("(struct _vs_%s){", ast->struct_lit_type->name);
        if (ast->nmembers == 0) {
            printf("0");
        } else {
            for (int i = 0; i < ast->nmembers; i++) {
                printf(".%s = ", ast->member_names[i]);
                compile(ast->member_exprs[i]);
                if (ast->member_exprs[i]->type == AST_TEMP_VAR || ast->member_exprs[i]->type == AST_ANON_FUNC_DECL) {
                    ast->member_exprs[i]->tmpvar->consumed = 1;
                }
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
    case AST_ASSIGN:
        emit_assignment(ast);
        break;
    case AST_BINOP:
        emit_binop(ast);
        break;
    case AST_CAST:
        if (ast->cast_type->base == STRUCT_T) {
            printf("*");
        }
        printf("((");
        emit_type(ast->cast_type);
        if (ast->cast_type->base == STRUCT_T) {
            printf("*");
        }
        printf(")");
        if (ast->cast_type->base == STRUCT_T) {
            printf("&");
        }
        compile(ast->cast_left);
        printf(")");
        break;
    case AST_SLICE:
        printf("(struct array_type){.data=");
        compile(ast->slice_inner);
        printf(".data");
        if (ast->slice_offset != NULL) {
            printf("+(");
            compile(ast->slice_offset);
            printf("*%d)", var_type(ast->slice_inner)->inner->size);
        }
        printf(",.length=");
        if (ast->slice_length != NULL) {
            /*printf("(");*/
            compile(ast->slice_length);
            /*printf(")*%d", var_type(ast->slice_inner)->inner->size);*/
        } else if (var_type(ast->slice_inner)->base == STATIC_ARRAY_T) {
            printf("%ld", var_type(ast->slice_inner)->length); 
        } else {
            error(ast->line, "Slicing a slice, uh ohhhhh");
        }
        if (ast->slice_offset != NULL) {
            printf("-");
            compile(ast->slice_offset);
        }
        printf("}");
        break;
    case AST_TEMP_VAR:
        emit_tmpvar(ast);
        break;
    case AST_COPY:
        emit_copy(ast);
        break;
    case AST_HOLD:
        if (ast->type == AST_STRUCT) {
        } else {
            switch (var_type(ast->expr)->base) {
            case STRING_T:
                /*emit_tmpvar(ast->expr);*/
                /*break;*/
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
        if (ast->expr->type == AST_TEMP_VAR) {
            ast->expr->tmpvar->consumed = 1;
        }
        break;
    case AST_IDENTIFIER:
        if (!ast->var->ext) {
            printf("_vs_");
        }
        printf("%s", ast->var->name);
        break;
    case AST_RETURN:
        if (ast->ret_expr != NULL) {
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

            printf("(_cl_%d = malloc(%d),", id, bindings->item->offset + bindings->item->size);
            bindings = reverse_typelist(bindings);
            bind_exprs = reverse_astlist(bind_exprs);
            while (bindings != NULL && bind_exprs != NULL) {
                printf("*((");
                emit_type(bindings->item);
                printf("*)(_cl_%d+%d))=", id, bindings->item->offset);
                compile(bind_exprs->item);
                if (bind_exprs->item->type == AST_TEMP_VAR) {
                    bind_exprs->item->tmpvar->consumed = 1;
                } else if (bind_exprs->item->type == AST_IDENTIFIER) {
                    bind_exprs->item->var->consumed = 1;
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
                TypeList *args = t->args;
                while (args != NULL) {
                    emit_type(args->item);
                    if (args->next != NULL) {
                        printf(",");
                    }
                    args = args->next;
                }
            }
            printf("))(");
            compile(ast->fn);
            printf("))");
        /*} else {*/
            /*compile(ast->fn);*/
        /*}*/
        printf("(");
        AstList *args = ast->args;
        while (args != NULL) {
            Type *t = var_type(args->item);
            if (t->base == STRUCT_T && is_dynamic(t)) {
                printf("_copy_%s(", t->name);
                compile(args->item);
                printf(")");
            } else {
                compile(args->item);
            }
            if (args->item->type == AST_TEMP_VAR && is_dynamic(t)) {
                args->item->tmpvar->consumed = 1;
            }
            if (args->next != NULL) {
                printf(",");
            }
            args = args->next;
        }
        printf(")");
        break;
    }
    case AST_INDEX:
        if (is_array(var_type(ast->left))) {
            printf("((");
            emit_type(var_type(ast->left)->inner);
            printf("*)");
            compile(ast->left);
            printf(".data)[");
            compile(ast->right);
            printf("]");
        } else {
            compile(ast->left);
            printf("[");
            compile(ast->right);
            printf("]");
        }
        break;
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            if (ast->statements[i]->type == AST_FUNC_DECL || ast->statements[i]->type == AST_EXTERN_FUNC_DECL) {
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
            printf("_tmp%d;\n", list->item->id);
            /*printf("= NULL");*/
            /*printf(";\n");*/
        }
        list = list->next;
    }
}

void emit_free_struct(char *name, Type *st, int is_ptr) {
    char *sep = is_ptr ? "->" : ".";
    for (int i = 0; i < st->nmembers; i++) {
        switch (st->member_types[i]->base) {
        case STRING_T:
            indent();
            printf("if (%s%s%s.bytes != NULL) {\n", name, sep, st->member_names[i]);
            _indent++;
            indent();
            printf("free(%s%s%s.bytes);\n", name, sep, st->member_names[i]);
            _indent--;
            indent();
            printf("}\n");
            break;
        case STRUCT_T: {
            char *memname = malloc(sizeof(char) * (strlen(name) + strlen(sep) + strlen(st->member_names[i]) + 2));
            sprintf(memname, "%s%s%s", name, sep, st->member_names[i]);
            emit_free_struct(memname, st->member_types[i], (st->member_types[i]->base == PTR_T || st->member_types[i]->base == BASEPTR_T));
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
            emit_free_struct(name, var->type->inner, 1);
            free(name);
            indent();
            printf("free(_vs_%s);\n", var->name);
        } else if (var->type->inner->base == STRING_T) { // TODO should this behave this way?
            indent();
            printf("free(_vs_%s->bytes);\n", var->name);
        }
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
        emit_free_struct(name, var->type, 0);
        free(name);
        break;
    }
    case STRING_T:
        // TODO if tmpvar is only conditionally initialized this will be invalid
        // while loop needs to be its own scope, otherwise using a string in
        // a while loop will cause potentially massive leak as tmpvar pointer
        // gets overwritten...
        if (var->temp) {
            indent();
            printf("if (_tmp%d.bytes != NULL) {\n", var->id); // maybe skip these
            indent();
            printf("    free(_tmp%d.bytes);\n", var->id);
            indent();
            printf("}\n");
        } else {
            indent();
            printf("if (_vs_%s.bytes != NULL) {\n", var->name);
            indent();
            printf("    free(_vs_%s.bytes);\n", var->name);
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
                char *name = malloc(sizeof(char) * (strlen(t->name) + 12 + sprintf(NULL, "%d", t->offset) + sprintf(NULL, "%d", id)));
                // TODO recount name length
                sprintf(name, "(*((%s*)_cl_%d+%d))", t->name, id, t->offset);
                emit_free_struct(name, t->inner, 1);
                free(name);
            } else if (t->inner->base == STRING_T) { // TODO should this behave this way?
                indent();
                printf("free((*((struct string_type*)_cl_%d+%d))->bytes);\n", id, t->offset);
            }
            indent();
            printf("free(*((void **)_cl_%d+%d));\n", id, t->offset);
            break;
        case STRUCT_T: {
            char *name = malloc(sizeof(char) * (strlen(t->name) + 10 + sprintf(NULL, "%d", t->offset) + sprintf(NULL, "%d", id)));
            sprintf(name, "(*((%s*)_cl_%d+%d))", t->name, id, t->offset);
            emit_free_struct(name, t->inner, 1);
            free(name);
            break;
        }
        case STRING_T:
            indent();
            printf("if ((*(struct string_type *)(_cl_%d+%d)).bytes != NULL) {\n", id, t->offset);
            indent();
            printf("    free(((*(struct string_type *)(_cl_%d+%d))).bytes);\n", id, t->offset);
            /*indent();*/
            /*printf("    free(*((struct string_type *)_cl_%d+%d));\n", id, t->offset);*/
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
    VarList *locals = scope->locals;
    while (locals != NULL) {
        Var *v = locals->item;
        locals = locals->next;
        // TODO got to be a better way to handle this here
        if (v->consumed || (v->type->base != FN_T && ((v->type->base == BASEPTR_T || v->type->base == PTR_T) ||
                v->type->held || !v->initialized || (v->temp && v->consumed)))) {
            continue;
        }
        emit_free(v);
    }
}

void emit_scope_end(Ast *scope) {
    if (!scope->has_return) {
        emit_free_locals(scope);
    }
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
        TypeList *args = v->type->args;
        for (int i = 0; args != NULL; i++) {
            emit_type(args->item);
            printf("a%d", i);
            if (args->next != NULL) {
                printf(",");
            }
            args = args->next;
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
    TypeList *args = v->type->args;
    for (int i = 0; args != NULL; i++) {
        emit_type(args->item);
        printf("a%d", i);
        if (args->next != NULL) {
            printf(",");
        }
        args = args->next;
    }
    printf(");\n");
}

int main(int argc, char **argv) {
    int just_ast = 0;
    if (argc > 1 && !strcmp(argv[1], "-a")) {
        just_ast = 1;
    }
    init_types();
    init_builtins();
    Ast *root = parse_scope(NULL);
    root = parse_semantics(root, root);
    if (just_ast) {
        print_ast(root);
    } else {
        /*printf("#include \"prelude.c\"\n");*/
        printf("%.*s\n", prelude_length, prelude);
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
        TypeList *stlist = get_global_structs();
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
