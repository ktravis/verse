#include "codegen.h"

static int _indent = 0;
static int _static_array_copy_depth = 0;

void indent() {
    for (int i = 0; i < _indent; i++) {
        printf("    ");
    }
}

void change_indent(int n) {
    _indent += n;
    if (_indent < 0) {
        _indent = 0;
    }
}

void emit_temp_var(Scope *scope, Ast *ast, int ref) {
    Var *v = find_temp_var(scope, ast);
    v->initialized = 1;
    printf("(_tmp%d = ", v->id);
    compile(scope, ast);
    printf(", %s_tmp%d)", ref ? "&" : "", v->id);
}

void emit_string_comparison(Scope *scope, Ast *ast) {
    AstBinaryOp *bin = ast->binary;
    if (bin->op == OP_NEQUALS) {
        printf("!");
    } else if (bin->op != OP_EQUALS) {
        error(ast->line, ast->file, "Comparison of type '%s' is not valid for type 'string'.", op_to_str(bin->op));
    }
    if (bin->left->type == AST_LITERAL && bin->right->type == AST_LITERAL) {
        printf("%d", strcmp(bin->left->lit->string_val, bin->right->lit->string_val) ? 0 : 1);
        return;
    }
    if (bin->left->type == AST_LITERAL) {
        printf("streq_lit(");
        compile(scope, bin->right);
        printf(",\"");
        print_quoted_string(bin->left->lit->string_val);
        printf("\",%d)", escaped_strlen(bin->left->lit->string_val));
    } else if (bin->right->type == AST_LITERAL) {
        printf("streq_lit(");
        compile(scope, bin->left);
        printf(",\"");
        print_quoted_string(bin->right->lit->string_val);
        printf("\",%d)", escaped_strlen(bin->right->lit->string_val));
    } else {
        printf("streq(");
        compile(scope, bin->left);
        printf(",");
        compile(scope, bin->right);
        printf(")");
    }
}

void emit_comparison(Scope *scope, Ast *ast) {
    Type *t = resolve_alias(ast->binary->left->var_type);
    if (is_string(t)) {
        emit_string_comparison(scope, ast);
        return;
    }
    printf("(");
    compile(scope, ast->binary->left);
    printf(" %s ", op_to_str(ast->binary->op));
    compile(scope, ast->binary->right);
    printf(")");
}

void emit_string_binop(Scope *scope, Ast *ast) {
    switch (ast->binary->right->type) {
    case AST_CALL: // is this right? need to do anything else?
    case AST_IDENTIFIER:
    case AST_DOT:
    case AST_INDEX:
    case AST_UOP:
        printf("append_string(");
        compile(scope, ast->binary->left);
        printf(",");
        compile(scope, ast->binary->right);
        printf(")");
        break;
    case AST_LITERAL:
        printf("append_string_lit(");
        compile(scope, ast->binary->left);
        printf(",\"");
        print_quoted_string(ast->binary->right->lit->string_val);
        printf("\",%d)", (int) escaped_strlen(ast->binary->right->lit->string_val));
        break;
    default:
        error(-1, "internal", "Couldn't do the string binop? %d", ast->type);
    }
}

void emit_dot_op(Scope *scope, Ast *ast) {
    Type *t = resolve_alias(ast->dot->object->var_type);

    if (ast->dot->object->type == AST_LITERAL && ast->dot->object->lit->lit_type == ENUM_LIT) {
        // TODO this should probably be a tmpvar
        char *s = t->en.member_names[ast->dot->object->lit->enum_val.enum_index];
        printf("init_string(\"");
        print_quoted_string(s);
        printf("\", %d)", (int)strlen(s));
        return;
    }

    if (t->comp == STATIC_ARRAY) {
        if (!strcmp(ast->dot->member_name, "length")) {
            printf("%ld", t->array.length);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            compile(scope, ast->dot->object);
        }
    } else {
        compile(scope, ast->dot->object);
        if (t->comp == REF) {
            printf("->%s", ast->dot->member_name);
        } else {
            printf(".%s", ast->dot->member_name);
        }
    }
}

void emit_uop(Scope *scope, Ast *ast) {
    switch (ast->unary->op) {
    case OP_NOT:
        printf("!"); break;
    case OP_REF:
        printf("&"); break;
    case OP_DEREF:
        printf("*"); break;
    case OP_MINUS:
        printf("-"); break;
    case OP_PLUS:
        printf("+"); break;
    default:
        error(ast->line, ast->file, "Unkown unary operator '%s' (%s).",
            op_to_str(ast->unary->op), ast->unary->op);
    }
    compile(scope, ast->unary->object);
}

void emit_assignment(Scope *scope, Ast *ast) {
    Ast *l = ast->binary->left;
    Ast *r = ast->binary->right;

    Type *lt = resolve_alias(l->var_type);

    if (lt->comp == STATIC_ARRAY) {
        printf("{\n");
        change_indent(1);
        indent();
        
        emit_type(lt);
        printf("l = ");
        compile_static_array(scope, l);
        printf(";\n");
        indent();
        
        emit_type(lt);
        printf("r = ");
        compile_static_array(scope, r);
        printf(";\n");
        indent();

        emit_static_array_copy(scope, lt, "l", "r");
        printf(";\n");

        change_indent(-1);
        indent();
        printf("}");
        return;
    }

    if (is_dynamic(lt)) {
        Var *v = get_ast_var_noerror(l);
        if (v == NULL || v->initialized) {
            Var *temp = find_temp_var(scope, r);
            printf("_tmp%d = ", temp->id);
            temp->initialized = 1;
            if (is_lvalue(r)) {
                emit_copy(scope, r);
            } else {
                compile(scope, r);
            }
            printf(";\n");
            indent();
            printf("SWAP(");

            if (l->type == AST_DOT || l->type == AST_INDEX) {
                compile(scope, l);
            } else {
                printf("_vs_%s", v->name);
            }
            printf(",_tmp%d)", temp->id);
        } else {
            printf("_vs_%s = ", v->name);
            if (is_lvalue(r)) {
                emit_copy(scope, r);
            } else {
                compile(scope, r);
            }

            if (v != NULL) {
                v->initialized = 1;
            }
        }
    } else {
        compile(scope, l);
        printf(" = ");
        if (lt->comp == ARRAY) {
            compile_unspecified_array(scope, r);
        } else if (lt->comp == STATIC_ARRAY) {
            compile_static_array(scope, r);
        } else if (is_any(lt) && !is_any(r->var_type)) {
            emit_any_wrapper(scope, r);
        } else {
            compile(scope, r);
        }
    }
}

void emit_binop(Scope *scope, Ast *ast) {
    Type *lt = resolve_alias(ast->binary->left->var_type);
    if (is_comparison(ast->binary->op)) {
        emit_comparison(scope, ast);
        return;
    } else if (is_string(lt)) {
        emit_string_binop(scope, ast);
        return;
    } else if (ast->binary->op == OP_OR) {
        printf("(");
        compile(scope, ast->binary->left);
        printf(") || (");
        compile(scope, ast->binary->right);
        printf(")");
        return;
    } else if (ast->binary->op == OP_AND) {
        printf("(");
        compile(scope, ast->binary->left);
        printf(") && ("); // does this short-circuit?
        compile(scope, ast->binary->right);
        printf(")");
        return;
    }
    printf("(");
    compile(scope, ast->binary->left);
    printf(" %s ", op_to_str(ast->binary->op));
    compile(scope, ast->binary->right);
    printf(")");
}

void emit_copy(Scope *scope, Ast *ast) {
    Type *t = resolve_alias(ast->var_type);

    // TODO: should this bail out here, or just never be called?
    if (t->comp == FUNC || !is_dynamic(t)) {
        compile(scope, ast);
        return;
    }

    if (t->comp == BASIC && t->data->base == STRING_T) {
        printf("copy_string(");
        compile(scope, ast);
        printf(")");
    } else if (t->comp == STRUCT) {
        printf("_copy_%d(", t->id);
        compile(scope, ast);
        printf(")");
    } else {
        error(-1, "internal", "wut even");
    }
}

void emit_type(Type *type) {
    assert(type != NULL);

    type = resolve_alias(type);

    switch (type->comp) {
    case BASIC:
        switch (type->data->base) {
        case UINT_T:
            printf("u");
        case INT_T:
            printf("int%d_t ", type->data->size * 8);
            break;
        case FLOAT_T:
            if (type->data->size == 4) { // TODO double-check these are always the right size
                printf("float ");
            } else if (type->data->size == 8) {
                printf("double ");
            } else {
                error(-1, "internal", "Cannot compile floating-point type of size %d.", type->data->size);
            }
            break;
        case BOOL_T:
            printf("unsigned char ");
            break;
        case STRING_T:
            printf("struct string_type ");
            break;
        case VOID_T:
            printf("void ");
            break;
        case BASEPTR_T:
            printf("ptr_type ");
            break;
        }
        break;
    case FUNC:
        printf("fn_type ");
        break;
    case REF:
        emit_type(type->inner);
        printf("*");
        break;
    case ARRAY:
        printf("struct array_type ");
        break;
    case STATIC_ARRAY:
        emit_type(type->array.inner);
        printf("*");
        break;
    case STRUCT:
        printf("struct _vs_%d ", type->id);
        break;
    case ENUM:
        emit_type(type->en.inner);
        break;
    default:
        error(-1, "internal", "wtf type");
    }
}

void compile_unspecified_array(Scope *scope, Ast *ast) {
    Type *t = resolve_alias(ast->var_type);

    if (t->comp == ARRAY) {
        compile(scope, ast);
    } else if (t->comp == STATIC_ARRAY) {
        printf("(struct array_type){.data=");
        compile(scope, ast);
        printf(",.length=%ld}", t->array.length);
    } else if (t->comp == BASIC && t->data->base == STRING_T) {
        printf("string_as_array(");
        compile(scope, ast);
        printf(")");
    } else {
        error(ast->line, ast->file, "Was expecting an array of some kind here, man.");
    }
}

void compile_static_array(Scope *scope, Ast *ast) {
    Type *t = resolve_alias(ast->var_type);

    if (t->comp == STATIC_ARRAY) {
        compile(scope, ast);
    } else if (t->comp == ARRAY) {
        printf("(");
        compile(scope, ast);
        printf(").data");
    } else {
        error(ast->line, ast->file, "Was expecting a static array here, man.");
    }
}

void emit_static_array_decl(Scope *scope, Ast *ast) {
    char *membername = malloc(sizeof(char) * (strlen(ast->decl->var->name) + 5));
    sprintf(membername, "_vs_%s", ast->decl->var->name);
    membername[strlen(ast->decl->var->name) + 5] = 0;
    emit_structmember(scope, membername, ast->decl->var->type);
    free(membername);
    /*emit_type(ast->decl_var->type->inner);*/
    /*printf("_vs_%s[%ld]", ast->decl_var->name, ast->decl_var->type->length);*/
    if (ast->decl->init == NULL) {
        printf(" = {0}");
    } else {
        printf(";\n");
        indent();
        printf("{\n");
        change_indent(1);
        indent();
        emit_type(ast->decl->var->type);
        printf("_0 = ");
        compile_static_array(scope, ast->decl->init);
        printf(";\n");
        indent();

        char *dname = malloc(sizeof(char) * (strlen(ast->decl->var->name) + 5));
        sprintf(dname, "_vs_%s", ast->decl->var->name);
        dname[strlen(ast->decl->var->name) + 5] = 0;
        emit_static_array_copy(scope, ast->decl->var->type, dname, "_0");
        printf(";\n");
        free(dname);

        change_indent(-1);
        indent();
        printf("}");
    }
    ast->decl->var->initialized = 1;
}

void emit_decl(Scope *scope, Ast *ast) {
    Type *t = resolve_alias(ast->decl->var->type);
    if (t->comp == STATIC_ARRAY) {
        emit_static_array_decl(scope, ast);
        return;
    }
    emit_type(t);
    printf("_vs_%s", ast->decl->var->name);
    if (ast->decl->init == NULL) {
        if (t->comp == BASIC) {
            int b = t->data->base;
            if (b == STRING_T) {
                printf(" = (struct string_type){0}");
                ast->decl->var->initialized = 1;
            } else if (b == INT_T || b == UINT_T || b == FLOAT_T) {
                printf(" = 0");
            } else if (b == BASEPTR_T) {
                printf(" = NULL");
            }
        } else if (t->comp == STRUCT) {
            printf(";\n");
            indent();
            printf("_init_%d(&_vs_%s)", t->id, ast->decl->var->name);
            ast->decl->var->initialized = 1;
        } else if (t->comp == REF)  {
            printf(" = NULL");
        } else if (t->comp == ARRAY) {
            printf(" = {0}");
        }
    } else {
        printf(" = ");
        if (t->comp == ARRAY) {
            compile_unspecified_array(scope, ast->decl->init);
        /*} else if (lt->comp == STATIC_ARRAY) {*/ // Eh?
            /*compile_static_array(ascope, st->binary->right);*/
        } else if (is_any(t) && !is_any(ast->decl->init->var_type)) {
            emit_any_wrapper(scope, ast->decl->init);
        } else if (is_lvalue(ast->decl->init)) {
            emit_copy(scope, ast->decl->init);
        } else {
            compile(scope, ast->decl->init);
        }
        ast->decl->var->initialized = 1;
    }
}

void emit_func_decl(Scope *scope, Ast *fn) {
    printf("/* %s */\n", fn->fn_decl->var->name);
    indent();
    emit_type(fn->fn_decl->var->type->fn.ret);

    printf("_vs_%d(", fn->fn_decl->var->id);

    VarList *args = fn->fn_decl->args;
    while (args != NULL) {
        if (fn->fn_decl->var->type->fn.variadic && args->next == NULL) {
            printf("struct array_type ");
        } else {
            emit_type(args->item->type);
        }
        printf("_vs_%s", args->item->name);
        if (args->next != NULL) {
            printf(",");
        }
        args = args->next;
    }
    printf(") ");
    
    emit_scope_start(fn->fn_decl->scope);
    compile_block(fn->fn_decl->scope, fn->fn_decl->body);
    emit_scope_end(fn->fn_decl->scope);
}

void emit_structmember(Scope *scope, char *name, Type *st) {
    st = resolve_alias(st);
    if (st->comp == STATIC_ARRAY) {
        emit_structmember(scope, name, st->array.inner);
        printf("[%ld]", st->array.length);
    } else {
        emit_type(st);
        printf("%s", name);
    }
}

void emit_static_array_copy(Scope *scope, Type *t, char *dest, char *src) {
    t = resolve_alias(t);
    Type *inner = resolve_alias(t->array.inner);
    if (!is_dynamic(t)) {
        printf("memcpy(%s, %s, sizeof(", dest, src);
        emit_type(inner); 
        printf(") * %ld)", t->array.length);
        return;
    }

    int d = _static_array_copy_depth++;
    printf("{\n");
    change_indent(1);
    indent();

    emit_type(inner);
    printf("d%d;\n", d);
    indent();
    emit_type(inner);
    printf("s%d;\n", d);
    indent();

    printf("for (int i = 0; i < %ld; i++) {\n", t->array.length);
    change_indent(1);
    indent();

    if (inner->comp == STATIC_ARRAY) {
        int depth_len = snprintf(NULL, 0, "%d", d);

        char *dname = malloc(sizeof(char) * (depth_len + 2));
        sprintf(dname, "d%d", d);
        dname[depth_len+2] = 0;

        char *sname = malloc(sizeof(char) * (depth_len + 2));
        sprintf(sname, "s%d", d);
        sname[depth_len+2] = 0;

        printf("%s = %s[i], %s = %s[i];\n", dname, dest, sname, src); 
        emit_static_array_copy(scope, t->inner, dname, sname);

        free(dname);
        free(sname);
    } else if (inner->comp == STRUCT) {
        printf("%s[i] = _copy_%d(%s[i])", dest, inner->id, src);
    } else if (inner->comp == BASIC && inner->data->base == STRING_T) {
        printf("%s[i] = copy_string(%s[i])", dest, src);
    } else {
        printf("%s[i] = %s[i]", dest, src);
    }
    printf(";\n"); // TODO move this?

    change_indent(-1);
    indent();
    printf("}\n");

    change_indent(-1);
    indent();
    printf("}\n");
    _static_array_copy_depth--;
}

void emit_struct_decl(Scope *scope, Type *st) {
    assert(st->comp == STRUCT);

    emit_type(st);
    printf("{\n");

    change_indent(1);
    for (int i = 0; i < st->st.nmembers; i++) {
        indent();
        emit_structmember(scope, st->st.member_names[i], st->st.member_types[i]);
        printf(";\n");
    }

    change_indent(-1);
    indent();
    printf("};\n");

    emit_type(st);
    printf("*_init_%d(", st->id);

    emit_type(st);
    printf("*x) {\n");

    change_indent(1);
    indent();

    printf("if (x == NULL) {\n");

    change_indent(1);
    indent();

    printf("x = malloc(sizeof(");
    emit_type(st);
    printf("));\n");

    change_indent(-1);
    indent();
    printf("}\n");

    indent();
    printf("memset(x, 0, sizeof(");
    emit_type(st);
    printf("));\n");

    indent();
    printf("return x;\n");
    change_indent(-1);
    indent();
    printf("}\n");

    emit_type(st);
    printf("_copy_%d(", st->id);

    emit_type(st);
    printf("x) {\n");

    change_indent(1);
    for (int i = 0; i < st->st.nmembers; i++) {
        Type *t = resolve_alias(st->st.member_types[i]);
        if (t->comp == STATIC_ARRAY && is_dynamic(t->array.inner)) {
            indent();
            char *member = malloc(sizeof(char) * (strlen(st->st.member_names[i]) + 3));
            sprintf(member, "x.%s", st->st.member_names[i]);
            member[strlen(st->st.member_names[i])] = 0;
            emit_static_array_copy(scope, t, member, member);
            printf(";\n");
            free(member);
        } else if (t->comp == BASIC && t->data->base == STRING_T) {
            indent();
            printf("x.%s = copy_string(x.%s);\n", st->st.member_names[i], st->st.member_names[i]);
        } else if (t->comp == STRUCT) {
            indent();
            printf("x.%s = _copy_%d(x.%s);\n", st->st.member_names[i],
                    t->id, st->st.member_names[i]);
        }
    }
    indent();
    printf("return x;\n");

    change_indent(-1);
    indent();
    printf("}\n");
}

void compile_ref(Scope *scope, Ast *ast) {
    assert(is_lvalue(ast));

    if (is_dynamic(ast->var_type)) {
        Type *t = resolve_alias(ast->var_type);
        if (t->comp == FUNC) {
            printf("&");
            compile(scope, ast);
        } else {
            emit_temp_var(scope, ast, 1);
        }
    } else {
        printf("&");
        compile(scope, ast);
    }
}

void compile_block(Scope *scope, AstBlock *block) {
    for (AstList *st = block->statements; st != NULL; st = st->next) {
        if (st->item->type == AST_FUNC_DECL || st->item->type == AST_EXTERN_FUNC_DECL ||
            st->item->type == AST_USE || st->item->type == AST_TYPE_DECL ||
            (st->item->type == AST_DECL && st->item->decl->global)) {
            continue;
        }

        indent();
        if (needs_temp_var(st->item)) {
            Var *v = find_temp_var(scope, st->item);
            printf("_tmp%d = ", v->id);
        }
        compile(scope, st->item);

        if (st->item->type != AST_CONDITIONAL && st->item->type != AST_WHILE &&
            st->item->type != AST_FOR && st->item->type != AST_BLOCK &&
            st->item->type != AST_TYPE_DECL && st->item->type != AST_ENUM_DECL) {
            printf(";\n");
        }
    }
}

void emit_any_wrapper(Scope *scope, Ast *ast) {
    printf("(struct _vs_%d){.value_pointer=", get_any_type_id());
    if (is_lvalue(ast)) {
        compile_ref(scope, ast);
    } else {
        emit_temp_var(scope, ast, 1);
    }
    Type *obj_type = resolve_alias(ast->var_type);
    printf(",.type=(struct _vs_%d *)&_type_info%d}", get_typeinfo_type_id(), obj_type->id);
}

void compile_call_arg(Scope *scope, Ast *ast, int arr) {
    if (arr) {
        compile_unspecified_array(scope, ast); 
    } else if (is_lvalue(ast)) {
        emit_copy(scope, ast);
    } else {
        compile(scope, ast);
    }
}

void compile_fn_call(Scope *scope, Ast *ast) {
    Type *t = resolve_alias(ast->call->fn->var_type);
    assert(t->comp == FUNC);

    unsigned char needs_wrapper = 1;
    if (ast->call->fn->type == AST_IDENTIFIER) {
        Var *v = ast->call->fn->ident->var;
        needs_wrapper = !v->ext && !v->constant;
    }

    if (needs_wrapper) {
        printf("((");
        emit_type(t->fn.ret);
        printf("(*)(");
        if (t->fn.nargs == 0) {
            printf("void");
        } else {
            TypeList *args = t->fn.args;
            while (args != NULL) {
                emit_type(args->item);
                if (args->next != NULL) {
                    printf(",");
                }
                args = args->next;
            }
        }
        printf("))(");
        if (needs_temp_var(ast->call->fn)) {
            emit_temp_var(scope, ast->call->fn, 0);
        } else {
            compile(scope, ast->call->fn);
        }
        printf("))");
    } else {
        if (needs_temp_var(ast->call->fn)) {
            emit_temp_var(scope, ast->call->fn, 0);
        } else {
            compile(scope, ast->call->fn);
        }
    }

    printf("(");

    AstList *args = ast->call->args;
    TypeList *argtypes = t->fn.args;
    Var *vt = ast->call->variadic_tempvar;

    int i = 0;
    while (args != NULL) {
        if (t->fn.variadic) {
            if (i == t->fn.nargs - 1) {
                printf("(");
            }
            if (i >= t->fn.nargs - 1) {
                printf("_tmp%d[%d] = ", vt->id, i - (t->fn.nargs - 1));
            }
        }

        Type *a = resolve_alias(args->item->var_type);
        if (is_any(argtypes->item) && !is_any(a)) {
            printf("(struct _vs_%d){.value_pointer=", get_any_type_id());
            if (needs_temp_var(args->item)) {
                emit_temp_var(scope, args->item, 1);
            } else {
                compile_ref(scope, args->item);
            }
            printf(",.type=(struct _vs_%d *)&_type_info%d}", get_typeinfo_type_id(), a->id);
        } else {
            compile_call_arg(scope, args->item, argtypes->item->comp == ARRAY);
        }

        if (args->next != NULL) {
            printf(",");
        }
        args = args->next;
        i++;

        if (!t->fn.variadic || argtypes->next != NULL) {
            argtypes = argtypes->next;
        }
    }

    if (t->fn.variadic) {
        if (ast->call->nargs - (t->fn.nargs) < 0) {
            printf(", (struct array_type){0, NULL}");
        } else if (ast->call->nargs > t->fn.nargs - 1) {
            printf(", (struct array_type){%ld, _tmp%d})",
                vt->type->array.length, vt->id); // this assumes we have set vt correctly
        }
    }
    printf(")");
}

void compile(Scope *scope, Ast *ast) {
    switch (ast->type) {
    case AST_LITERAL:
        switch (ast->lit->lit_type) {
        case INTEGER:
            printf("%ld", ast->lit->int_val);
            break;
        case FLOAT: // TODO this is truncated
            printf("%F", ast->lit->float_val);
            break;
        case CHAR:
            printf("'%c'", (unsigned char)ast->lit->int_val);
            break;
        case BOOL:
            printf("%d", (unsigned char)ast->lit->int_val);
            break;
        // TODO strlen is wrong for escaped chars \t etc
        case STRING:
            printf("init_string(\"");
            print_quoted_string(ast->lit->string_val);
            printf("\", %d)", (int)strlen(ast->lit->string_val));
            break;
        case STRUCT_LIT:
            printf("(struct _vs_%d){", ast->lit->struct_val.type->id);
            if (ast->lit->struct_val.nmembers == 0) {
                printf("0");
            } else {
                StructType st = resolve_alias(ast->lit->struct_val.type)->st;
                for (int i = 0; i < ast->lit->struct_val.nmembers; i++) {
                    Ast *expr = ast->lit->struct_val.member_exprs[i];
                    printf(".%s = ", st.member_names[i]);

                    if (is_any(st.member_types[i]) && !is_any(expr->var_type)) {
                        emit_any_wrapper(scope, expr);
                    } else if (is_lvalue(expr)) {
                        emit_copy(scope, expr);
                    } else {
                        compile(scope, expr);
                    }

                    if (i != st.nmembers - 1) {
                        printf(", ");
                    }
                }
            }
            printf("}");
            break;
        case ENUM_LIT: {
            Type *t = resolve_alias(ast->lit->enum_val.enum_type);
            printf("%ld", t->en.member_values[ast->lit->enum_val.enum_index]);
            break;
        }
        }
        break;     
    case AST_DOT:
        emit_dot_op(scope, ast);
        break;
    case AST_UOP:
        emit_uop(scope, ast);
        break;
    case AST_ASSIGN:
        emit_assignment(scope, ast);
        break;
    case AST_BINOP:
        emit_binop(scope, ast);
        break;
    case AST_CAST: {
        Type *t = resolve_alias(ast->cast->cast_type);
        if (is_any(t)) {
            emit_any_wrapper(scope, ast->cast->object);
            break;
        }
        if (t->comp == STRUCT) {
            printf("*");
        }

        printf("((");
        emit_type(t);
        if (t->comp == STRUCT) {
            printf("*");
        }

        printf(")");
        if (t->comp == STRUCT) {
            printf("&");
        }

        compile(scope, ast->cast->object);
        printf(")");
        break;
    }
    case AST_SLICE: {
        printf("(struct array_type){.data=");
        if (needs_temp_var(ast->slice->object)) {
            emit_temp_var(scope, ast->slice->object, 0);
        } else {
            compile_static_array(scope, ast->slice->object);
        }

        Type *obj_type = resolve_alias(ast->slice->object->var_type);
        if (ast->slice->offset != NULL) {
            printf("+(");
            compile(scope, ast->slice->offset);
            printf(")");
        }
        printf(",.length=");

        if (ast->slice->length != NULL) {
            compile(scope, ast->slice->length);
        } else if (obj_type->comp == STATIC_ARRAY) {
            printf("%ld", obj_type->array.length); 
        } else {
            // TODO make this work (store tmpvar of thing being sliced to avoid
            // re-running whatever it is)
            error(ast->line, ast->file, "Slicing a slice, uh ohhhhh");
        }
        if (ast->slice->offset != NULL) {
            printf("-");
            compile(scope, ast->slice->offset);
        }
        printf("}");
        break;
    }
    case AST_IDENTIFIER: {
        Type *t = resolve_alias(ast->ident->var->type);
        assert(!ast->ident->var->proxy);
        if (ast->ident->var->ext) {
            printf("%s", ast->ident->var->name);
        } else if (t->comp == FUNC && ast->ident->var->constant) {
            printf("_vs_%d", ast->ident->var->id);
        } else {
            printf("_vs_%s", ast->ident->var->name);
        }
        break;
    }
    case AST_RETURN:
        if (ast->ret->expr != NULL) {
            emit_type(ast->ret->expr->var_type);
            printf("_ret = ");
            if (is_lvalue(ast->ret->expr)) {
                emit_copy(scope, ast->ret->expr);
            } else {
                compile(scope, ast->ret->expr);
            }
            printf(";");
        }
        printf("\n");
        emit_free_locals(scope);

        indent();
        printf("return");
        if (ast->ret->expr != NULL) {
            if (ast->ret->expr->type == AST_IDENTIFIER) {
                printf(" ");
                compile(scope, ast->ret->expr);
            } else {
                printf(" _ret");
            }
        }
        break;
    case AST_BREAK:
        printf("break");
        break;
    case AST_CONTINUE:
        printf("continue");
        break;
    case AST_DECL:
        emit_decl(scope, ast);
        break;
    case AST_FUNC_DECL: 
    case AST_EXTERN_FUNC_DECL: 
        break;
    case AST_ANON_FUNC_DECL: 
        printf("_vs_%d", ast->fn_decl->var->id);
        break;
    case AST_CALL:
        compile_fn_call(scope, ast);
        break;
    case AST_INDEX: {
        Type *lt = resolve_alias(ast->index->object->var_type);
        if (lt->comp == ARRAY || lt->comp == STATIC_ARRAY) {
            printf("(");
            if (lt->comp == ARRAY) {
                printf("(");
                emit_type(lt->inner);
                printf("*)");
            }

            if (needs_temp_var(ast->index->object)) {
                emit_temp_var(scope, ast->index->object, 0);
            } else {
                compile_static_array(scope, ast->index->object);
            }

            printf(")[");

            if (needs_temp_var(ast->index->index)) {
                emit_temp_var(scope, ast->index->index, 0);
            } else {
                compile(scope, ast->index->index);
            }

            printf("]");
        } else { // string
            printf("((uint8_t*)");

            if (needs_temp_var(ast->index->object)) {
                emit_temp_var(scope, ast->index->object, 0);
            } else {
                compile_static_array(scope, ast->index->object);
            }

            printf(".bytes)[");

            if (needs_temp_var(ast->index->index)) {
                emit_temp_var(scope, ast->index->index, 0);
            } else {
                compile(scope, ast->index->index);
            }

            printf("]");
        }
        break;
    }
    case AST_BLOCK: // If this is used on root, does emit_scope_start need to happen?
        compile_block(scope, ast->block);
        break;
    case AST_CONDITIONAL:
        printf("if (");
        compile(scope, ast->cond->condition);
        printf(") ");
        emit_scope_start(ast->cond->scope);
        compile_block(ast->cond->scope, ast->cond->if_body);
        emit_scope_end(ast->cond->scope);
        if (ast->cond->else_body != NULL) {
            indent();
            printf("else ");
            emit_scope_start(ast->cond->scope);
            compile_block(ast->cond->scope, ast->cond->else_body);
            emit_scope_end(ast->cond->scope);
        }
        break;
    case AST_WHILE:
        printf("while (");
        compile(scope, ast->while_loop->condition);
        printf(") ");
        emit_scope_start(ast->while_loop->scope);
        compile_block(ast->while_loop->scope, ast->while_loop->body);
        emit_scope_end(ast->while_loop->scope);
        break;
    case AST_FOR:
        // TODO: loop depth should change iter var name?
        printf("{\n");
        change_indent(1);
        indent();

        printf("struct array_type _iter = ");
        compile_unspecified_array(scope, ast->for_loop->iterable);
        printf(";\n");

        indent();
        printf("for (long _i = 0; _i < _iter.length; _i++) {\n");

        change_indent(1);
        indent();
        emit_type(ast->for_loop->itervar->type);
        printf("_vs_%s = ((", ast->for_loop->itervar->name);
        emit_type(ast->for_loop->itervar->type);
        printf("*)_iter.data)[_i];\n");

        indent();
        emit_scope_start(ast->for_loop->scope);
        // TODO don't clear these vars!
        compile_block(ast->for_loop->scope, ast->for_loop->body);
        emit_scope_end(ast->for_loop->scope);

        change_indent(-1);
        indent();
        printf("}\n");

        change_indent(-1);
        indent();
        printf("}\n");
        break;
    case AST_TYPEINFO:
        printf("((struct _vs_%d *)&_type_info%d)",
            get_typeinfo_type_id(),
            resolve_alias(ast->typeinfo->typeinfo_target)->id);
        break;
    case AST_TYPE_DECL:
        break;
    case AST_ENUM_DECL:
        break;
    default:
        error(ast->line, ast->file, "No idea how to deal with this.");
    }
}

void emit_scope_start(Scope *scope) {
    printf("{\n");
    change_indent(1);
    TempVarList *list = scope->temp_vars;
    while (list != NULL) {
        Type *t = resolve_alias(list->var->type);
        indent();
        emit_type(t);
        printf("_tmp%d", list->var->id);
        if (t->comp == STRUCT ||
           (t->comp == BASIC && t->data->base == STRING_T)) {
            printf(" = {0}");
        } else if (t->comp == STATIC_ARRAY) {
            printf(" = alloca(%ld * sizeof(", t->array.length);
            emit_type(t->array.inner);
            printf("))");
        }
        printf(";\n");
        list = list->next;
    }
}

void emit_free_struct(Scope *scope, char *name, Type *st, int is_ref) {
    char *sep = is_ref ? "->" : ".";

    for (int i = 0; i < st->st.nmembers; i++) {
        Type *t = st->st.member_types[i];
        if (t->comp == BASIC && t->data->base == STRING_T) {
            indent();
            printf("if (%s%s%s.bytes != NULL) {\n", name, sep, st->st.member_names[i]);

            change_indent(1);
            indent();
            printf("free(%s%s%s.bytes);\n", name, sep, st->st.member_names[i]);

            change_indent(-1);
            indent();
            printf("}\n");
        } else if (t->comp == STRUCT) {
            char *memname = malloc(sizeof(char) * (strlen(name) + strlen(sep) + strlen(st->st.member_names[i]) + 2));
            sprintf(memname, "%s%s%s", name, sep, st->st.member_names[i]);

            int ref = (t->comp == REF || (t->comp == BASIC && t->data->base == BASEPTR_T));
            emit_free_struct(scope, memname, st->st.member_types[i], ref);
            free(memname);
        }
    }
}

void emit_free(Scope *scope, Var *var) {
    Type *t = resolve_alias(var->type);
    if (t->comp == REF) {
        Type *inner = resolve_alias(var->type->inner);
        if (inner->comp == STRUCT) {
            char *name = malloc(sizeof(char) * (strlen(var->name) + 5));
            sprintf(name, "_vs_%s", var->name);
            emit_free_struct(scope, name, inner, 1);
            free(name);
            indent();
            printf("free(_vs_%s);\n", var->name);
        } else if (inner->comp == BASIC && inner->data->base == STRING_T) { // TODO should this behave this way?
            indent();
            printf("free(_vs_%s->bytes);\n", var->name);
        }
    } else if (t->comp == STATIC_ARRAY) {
        if (is_dynamic(t->array.inner)) {
            indent();
            printf("{\n");

            change_indent(1);
            indent();
            emit_type(t->array.inner);
            printf("*_0 = _vs_%s;\n", var->name);

            indent();
            printf("for (int i = 0; i < %ld; i++) {\n", t->array.length);

            change_indent(1);
            indent();
            emit_type(t->array.inner);
            printf("_vs_i = _0[i];\n");

            Var v = {
                .name= "i",
                .type= t->array.inner,
                .temp= 0,
                .initialized= 1
            };
            emit_free(scope, &v);

            change_indent(-1);
            indent();
            printf("}\n");

            change_indent(-1);
            indent();
            printf("}\n");
        }
    } else if (t->comp == STRUCT) {
        char *name;
        if (var->temp) {
            name = malloc(sizeof(char) * (snprintf(NULL, 0, "%d", var->id) + 5));
            sprintf(name, "_tmp%d", var->id);
        } else {
            name = malloc(sizeof(char) * (strlen(var->name) + 5));
            sprintf(name, "_vs_%s", var->name);
        }
        emit_free_struct(scope, name, var->type, 0);
        free(name);
    } else if (t->comp == BASIC) {
        indent();
        if (t->data->base == STRING_T) {
            if (var->temp) {
                printf("free(_tmp%d.bytes);\n", var->id);
            } else {
                printf("free(_vs_%s.bytes);\n", var->name);
            }
        } else if (t->data->base == BASEPTR_T) {
            printf("free(_vs_%s);\n", var->name);
        }
    }
}

void emit_free_locals(Scope *scope) {
    VarList *locals = scope->vars;
    while (locals != NULL) {
        Var *v = locals->item;
        Type *t = resolve_alias(v->type);

        locals = locals->next;
        // TODO got to be a better way to handle this here
        if (!v->initialized || t->comp == FUNC || t->comp == REF ||
           (t->comp == BASIC && t->data->base == BASEPTR_T)) {
            continue;
        }
        emit_free(scope, v);
    }
}

void emit_scope_end(Scope *scope) {
    if (!scope->has_return) {
        emit_free_locals(scope);
    }
    change_indent(-1);
    indent();
    printf("}\n");
}

void emit_var_decl(Scope *scope, Var *v) {
    if (v->ext) {
        printf("extern ");
    }

    Type *t = resolve_alias(v->type);
    if (t->comp == FUNC) {
        emit_type(t->fn.ret);
        printf("(*");
    } else if (t->comp == STATIC_ARRAY) {
        emit_type(t->array.inner);
        if (!v->ext) {
            printf("_vs_");
        }
        printf("%s[%ld] = {0};\n", v->name, t->array.length);
        return;
    } else {
        emit_type(t);
    }

    if (!v->ext) {
        printf("_vs_");
    }

    printf("%s", v->name);
    if (t->comp == FUNC) {
        printf(")(");
        TypeList *args = t->fn.args;
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
void emit_forward_decl(Scope *scope, Var *v) {
    printf("/* %s */\n", v->name);
    if (v->ext) {
        printf("extern ");
    }

    Type *t = resolve_alias(v->type);
    assert(t->comp == FUNC);
    emit_type(t->fn.ret);

    if (!v->ext) {
        printf("_vs_");
    }
    printf("%d(", v->id);

    TypeList *args = t->fn.args;
    for (int i = 0; args != NULL; i++) {
        if (t->fn.variadic && args->next == NULL) {
            printf("struct array_type ");
        } else {
            emit_type(args->item);
        }
        printf("a%d", i);
        if (args->next != NULL) {
            printf(",");
        }
        args = args->next;
    }
    printf(");\n");
}
