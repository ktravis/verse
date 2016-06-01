#include "src/compiler.h"
#include "src/util.h"

#include "prelude.h"

static int _indent = 0;
static int _static_array_copy_depth = 0;

void indent() {
    for (int i = 0; i < _indent; i++) {
        printf("    ");
    }
}

void emit_string_comparison(Ast *ast) {
    AstBinaryOp *bin = ast->binary;
    if (bin->op == OP_NEQUALS) {
        printf("!");
    } else if (bin->op != OP_EQUALS) {
        error(ast->line, "Comparison of type '%s' is not valid for type 'string'.", op_to_str(bin->op));
    }
    if (bin->left->type == AST_LITERAL && bin->right->type == AST_LITERAL) {
        printf("%d", strcmp(bin->left->lit->string_val, bin->right->lit->string_val) ? 0 : 1);
        return;
    }
    if (bin->left->type == AST_LITERAL) {
        printf("streq_lit(");
        compile(bin->right);
        printf(",\"");
        print_quoted_string(bin->left->lit->string_val);
        printf("\",%d)", escaped_strlen(bin->left->lit->string_val));
    } else if (bin->right->type == AST_LITERAL) {
        printf("streq_lit(");
        compile(bin->left);
        printf(",\"");
        print_quoted_string(bin->right->lit->string_val);
        printf("\",%d)", escaped_strlen(bin->right->lit->string_val));
    } else {
        printf("streq(");
        compile(bin->left);
        printf(",");
        compile(bin->right);
        printf(")");
    }
}

void emit_comparison(Ast *ast) {
    if (ast->binary->left->var_type->base == STRING_T) {
        emit_string_comparison(ast);
        return;
    }
    printf("(");
    compile(ast->binary->left);
    printf(" %s ", op_to_str(ast->binary->op));
    compile(ast->binary->right);
    printf(")");
}

void emit_string_binop(Ast *ast) {
    switch (ast->binary->right->type) {
    case AST_CALL: // is this right? need to do anything else?
    case AST_IDENTIFIER:
    case AST_DOT:
    case AST_INDEX:
    case AST_UOP:
    case AST_TEMP_VAR:
        printf("append_string(");
        compile(ast->binary->left);
        printf(",");
        compile(ast->binary->right);
        printf(")");
        break;
    case AST_LITERAL:
        printf("append_string_lit(");
        compile(ast->binary->left);
        printf(",\"");
        print_quoted_string(ast->binary->right->lit->string_val);
        printf("\",%d)", (int) escaped_strlen(ast->binary->right->lit->string_val));
        break;
    default:
        error(-1, "Couldn't do the string binop? %d", ast->type);
    }
}

void emit_dot_op(Ast *ast) {
    Type *t = ast->dot->object->var_type;
    if (t->base == STATIC_ARRAY_T || (t->base == PTR_T && t->inner->base == STATIC_ARRAY_T)) {
        if (!strcmp(ast->dot->member_name, "length")) {
            printf("%ld", array_size(t));
        } else if (!strcmp(ast->dot->member_name, "data")) {
            if (t->base == PTR_T) {
                printf("*");
            }
            compile(ast->dot->object);
        }
    } else {
        compile(ast->dot->object);
        if (t->base == PTR_T) {// || (ast->dot_left->type == AST_IDENTIFIER && ast->dot_left->var->type->held)) {
            printf("->%s", ast->dot->member_name);
        } else {
            printf(".%s", ast->dot->member_name);
        }
    }
}

void emit_uop(Ast *ast) {
    switch (ast->unary->op) {
    case OP_NOT:
        printf("!"); break;
    case OP_ADDR:
        printf("&"); break;
    case OP_DEREF:
        printf("*"); break;
    case OP_MINUS:
        printf("-"); break;
    case OP_PLUS:
        printf("+"); break;
    default:
        error(ast->line,"Unkown unary operator '%s' (%s).", op_to_str(ast->unary->op), ast->unary->op);
    }
    compile(ast->unary->object);
}

void emit_assignment(Ast *ast) {
    Type *lt = ast->binary->left->var_type;
    if (lt->base == STATIC_ARRAY_T) {
        printf("{\n");
        _indent++;
        indent();
        
        emit_type(lt);
        printf("l = ");
        compile_static_array(ast->binary->left);
        printf(";\n");
        indent();
        
        emit_type(lt);
        printf("r = ");
        compile_static_array(ast->binary->right);
        printf(";\n");
        indent();

        emit_static_array_copy(lt, "l", "r");
        printf(";\n");

        _indent--;
        indent();
        printf("}");
        return;
    }
    if (is_dynamic(lt)) {
        if (ast->binary->left->type == AST_DOT || ast->binary->left->type == AST_INDEX) {
            compile(ast->binary->right);
            printf(";\n");
            indent();
            printf("SWAP(");
            compile(ast->binary->left);
            printf(",_tmp%d)", ast->binary->right->tempvar->var->id);
        } else {
            Var *l = get_ast_var(ast->binary->left);
            if (l->initialized) {
                compile(ast->binary->right);
                printf(";\n");
                indent();
                printf("SWAP(_vs_%s,_tmp%d)", l->name, ast->binary->right->tempvar->var->id);
            } else {
                printf("_vs_%s = ", l->name);
                compile(ast->binary->right);
                l->initialized = 1;
                if (ast->binary->right->type == AST_TEMP_VAR) {
                    ast->binary->right->tempvar->var->consumed = 1;
                }
            }
        }
    } else {
        compile(ast->binary->left);
        printf(" = ");
        if (lt->base == ARRAY_T) {
            compile_unspecified_array(ast->binary->right);
        } else if (lt->base == STATIC_ARRAY_T) {
            compile_static_array(ast->binary->right);
        } else {
            compile(ast->binary->right);
        }
        if (ast->binary->right->type == AST_TEMP_VAR) {
            ast->binary->right->tempvar->var->consumed = 1;
        }
    }
}

void emit_binop(Ast *ast) {
    if (is_comparison(ast->binary->op)) {
        emit_comparison(ast);
        return;
    } else if (ast->binary->left->var_type->base == STRING_T) {
        emit_string_binop(ast);
        return;
    } else if (ast->binary->op == OP_OR) {
        printf("(");
        compile(ast->binary->left);
        printf(") || (");
        compile(ast->binary->right);
        printf(")");
        return;
    } else if (ast->binary->op == OP_AND) {
        printf("(");
        compile(ast->binary->left);
        printf(") && ("); // does this short-circuit?
        compile(ast->binary->right);
        printf(")");
        return;
    }
    printf("(");
    compile(ast->binary->left);
    printf(" %s ", op_to_str(ast->binary->op));
    compile(ast->binary->right);
    printf(")");
}

void emit_copy(Ast *ast) {
    if (ast->copy->expr->var_type->base == FN_T) {
        compile(ast->copy->expr);
    } else {
        Type *t = ast->copy->expr->var_type;
        switch (t->base) {
        case STRING_T:
            printf("copy_string(");
            compile(ast->copy->expr);
            printf(")");
            break;
        case STRUCT_T: {
            if (t->builtin) {
                printf("_copy_%s(", t->name);
            } else {
                printf("_copy_%d(", t->id);
            }
            compile(ast->copy->expr);
            printf(")");
            break;
        }
        }
    }
}

void emit_tempvar(Ast *ast) {
    if (ast->tempvar->var->type->base == FN_T) {
        compile(ast->tempvar->expr);
    } else {
        ast->tempvar->var->initialized = 1;
        printf("(_tmp%d = ", ast->tempvar->var->id);
        compile(ast->tempvar->expr);
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
        printf("struct array_type ");
        break;
    case STATIC_ARRAY_T:
        emit_type(type->inner);
        printf("*");
        break;
    case STRUCT_T: {
        if (type->builtin) {
            printf("struct _vs_%s ", type->name);
        } else {
            printf("struct _vs_%d ", type->id);
        }
        break;
    }
    case ENUM_T:
        emit_type(type->_enum.inner);
        break;
    default:
        error(-1, "wtf type");
    }
}

void compile_unspecified_array(Ast *ast) {
    Type *t = ast->var_type;
    if (t->base == ARRAY_T) {
        compile(ast);
    } else if (t->base == STATIC_ARRAY_T) {
        printf("(struct array_type){.data=");
        compile(ast);
        printf(",.length=%ld}", t->length);
    } else {
        error(ast->line, "Was expecting an array of some kind here, man.");
    }
}

void compile_static_array(Ast *ast) {
    Type *t = ast->var_type;
    if (t->base == STATIC_ARRAY_T) {
        compile(ast);
    } else if (t->base == ARRAY_T) {
        printf("(");
        compile(ast);
        printf(").data");
    } else {
        error(ast->line, "Was expecting a static array here, man.");
    }
}

void emit_static_array_decl(Ast *ast) {
    char *membername = malloc(sizeof(char) * (strlen(ast->decl->var->name) + 5));
    sprintf(membername, "_vs_%s", ast->decl->var->name);
    membername[strlen(ast->decl->var->name) + 5] = 0;
    emit_structmember(membername, ast->decl->var->type);
    free(membername);
    /*emit_type(ast->decl_var->type->inner);*/
    /*printf("_vs_%s[%ld]", ast->decl_var->name, ast->decl_var->type->length);*/
    if (ast->decl->init == NULL) {
        printf(" = {0}");
    } else {
        printf(";\n");
        indent();
        printf("{\n");
        _indent++;
        indent();
        emit_type(ast->decl->var->type);
        printf("_0 = ");
        compile_static_array(ast->decl->init);
        printf(";\n");
        indent();

        char *dname = malloc(sizeof(char) * (strlen(ast->decl->var->name) + 5));
        sprintf(dname, "_vs_%s", ast->decl->var->name);
        dname[strlen(ast->decl->var->name) + 5] = 0;
        emit_static_array_copy(ast->decl->var->type, dname, "_0");
        printf(";\n");
        free(dname);

        _indent--;
        indent();
        printf("}");

        if (ast->decl->init->type == AST_TEMP_VAR) {
            ast->decl->init->tempvar->var->consumed = 1;
        }
    }
    ast->decl->var->initialized = 1;
}

void emit_decl(Ast *ast) {
    Type *t = ast->decl->var->type;
    if (t->base == STATIC_ARRAY_T) {
        emit_static_array_decl(ast);
        return;
    }
    emit_type(t);
    printf("_vs_%s", ast->decl->var->name);
    if (ast->decl->init == NULL) {
        if (t->base == STRING_T) {
            printf(" = (struct string_type){0}");
            ast->decl->var->initialized = 1;
        } else if (t->base == STRUCT_T) {
            printf(";\n");
            indent();
            if (t->builtin) {
                printf("_init_%s(&_vs_%s)", t->name, ast->decl->var->name);
            } else {
                printf("_init_%d(&_vs_%s)", t->id, ast->decl->var->name);
            }
            ast->decl->var->initialized = 1;
        } else if (t->base == BASEPTR_T || t->base == PTR_T)  {
            printf(" = NULL");
        } else if (is_numeric(t)) {
            printf(" = 0");
        } else if (t->base == ARRAY_T) {
            printf(" = {0}");
        }
    } else {
        printf(" = ");
        if (t->base == ARRAY_T) {
            compile_unspecified_array(ast->decl->init);
        } else {
            compile(ast->decl->init);
        }
        if (ast->decl->init->type == AST_TEMP_VAR) {
            ast->decl->init->tempvar->var->consumed = 1;
        }
        ast->decl->var->initialized = 1;
    }
}

void emit_func_decl(Ast *fn) {
    printf("/* %s */\n", fn->fn_decl->var->name);
    indent();
    emit_type(fn->fn_decl->var->type->fn.ret);
    // TODO do this in a non-hacky way
    if (!strcmp(fn->fn_decl->var->name, "main")) {
        printf("_vs_%s(", fn->fn_decl->var->name);
    } else {
        printf("_vs_%d(", fn->fn_decl->var->id);
    }
    VarList *args = fn->fn_decl->args;
    while (args != NULL) {
        emit_type(args->item->type);
        printf("_vs_%s", args->item->name);
        if (args->next != NULL) {
            printf(",");
        }
        args = args->next;
    }
    printf(") ");
    
    compile_scope(fn->fn_decl->scope);
}

void emit_structmember(char *name, Type *st) {
    if (st->base == STATIC_ARRAY_T) {
        emit_structmember(name, st->inner);
        printf("[%ld]", st->length);
    } else {
        emit_type(st);
        printf("%s", name);
    }
}

void emit_static_array_copy(Type *t, char *dest, char *src) {
    if (!is_dynamic(t)) {
        printf("memcpy(%s, %s, %ld)", dest, src, t->length * t->inner->size);
        return;
    }
    int d = _static_array_copy_depth++;
    printf("{\n");
    _indent++;
    indent();

    emit_type(t->inner);
    printf("d%d;\n", d);
    indent();
    emit_type(t->inner);
    printf("s%d;\n", d);
    indent();

    printf("for (int i = 0; i < %ld; i++) {\n", t->length);
    _indent++;
    indent();

    switch (t->inner->base) {
    case STATIC_ARRAY_T: {
        int depth_len = snprintf(NULL, 0, "%d", d);

        char *dname = malloc(sizeof(char) * (depth_len + 2));
        sprintf(dname, "d%d", d);
        dname[depth_len+2] = 0;

        char *sname = malloc(sizeof(char) * (depth_len + 2));
        sprintf(sname, "s%d", d);
        sname[depth_len+2] = 0;

        printf("%s = %s[i], %s = %s[i];\n", dname, dest, sname, src); 
        emit_static_array_copy(t->inner, dname, sname);

        free(dname);
        free(sname);
        break;
    }
    case STRING_T:
        printf("%s[i] = copy_string(%s[i])", dest, src);
        break;
    case STRUCT_T:
        if (t->inner->builtin) {
            printf("%s[i] = _copy_%s(%s[i])", dest, t->inner->name, src);
        } else {
            printf("%s[i] = _copy_%d(%s[i])", dest, t->inner->id, src);
        }
        break;
    /*case ARRAY_T:*/
    default:
        printf("%s[i] = %s[i]", dest, src);
        break;
    }
    printf(";\n"); // TODO move this?

    _indent--;
    indent();
    printf("}\n");

    _indent--;
    indent();
    printf("}\n");
    _static_array_copy_depth--;
}

void emit_struct_decl(Type *st) {
    emit_type(st);
    printf("{\n");
    _indent++;
    for (int i = 0; i < st->st.nmembers; i++) {
        indent();
        emit_structmember(st->st.member_names[i], st->st.member_types[i]);
        printf(";\n");
    }
    _indent--;
    indent();
    printf("};\n");
    emit_type(st);
    if (st->builtin) {
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
    indent();
    printf("memset(x, 0, sizeof(");
    emit_type(st);
    printf("));\n");
    indent();
    printf("return x;\n");
    _indent--;
    indent();
    printf("}\n");
    emit_type(st);
    if (st->builtin) {
        printf("_copy_%s(", st->name);
    } else {
        printf("_copy_%d(", st->id);
    }
    emit_type(st);
    printf("x) {\n");
    _indent++;
    for (int i = 0; i < st->st.nmembers; i++) {
        Type *t = st->st.member_types[i];
        if (t->base == STATIC_ARRAY_T && is_dynamic(t->inner)) {
            indent();
            char *member = malloc(sizeof(char) * (strlen(st->st.member_names[i]) + 3));
            sprintf(member, "x.%s", st->st.member_names[i]);
            member[strlen(st->st.member_names[i])] = 0;
            emit_static_array_copy(t, member, member);
            printf(";\n");
            free(member);
        } else if (t->base == STRING_T) {
            indent();
            printf("x.%s = copy_string(x.%s);\n", st->st.member_names[i], st->st.member_names[i]);
        } else if (t->base == STRUCT_T) {
            indent();
            printf("x.%s = ", st->st.member_names[i]);
            if (t->builtin) {
                printf("_copy_%s(", t->name);
            } else {
                printf("_copy_%d(", t->id);
            }
            printf("x.%s);\n", st->st.member_names[i]);
        }
    }
    indent();
    printf("return x;\n");
    _indent--;
    indent();
    printf("}\n");
}

void compile_pointer(Ast *ast) {
    assert(is_lvalue(ast) || ast->type == AST_TEMP_VAR);

    if (ast->type == AST_TEMP_VAR) {
        if (ast->tempvar->var->type->base == FN_T) {
            printf("&");
            compile(ast->tempvar->expr);
        } else {
            ast->tempvar->var->initialized = 1;
            printf("(_tmp%d = ", ast->tempvar->var->id);
            compile(ast->tempvar->expr);
            printf(", &_tmp%d)", ast->tempvar->var->id);
        }
    } else {
        printf("&");
        compile(ast);
    }
}

void compile_scope(AstScope *scope) {
    emit_scope_start(scope);
    compile_block(scope->body);
    emit_scope_end(scope);
}

void compile_block(AstBlock *block) {
    for (AstList *statements = block->statements; statements != NULL; statements = statements->next) {
        if (statements->item->type == AST_FUNC_DECL || statements->item->type == AST_EXTERN_FUNC_DECL ||
            statements->item->type == AST_TYPE_DECL) {
            continue;
        }
        if (statements->item->type != AST_RELEASE) {
            indent();
        }
        compile(statements->item);
        if (statements->item->type != AST_CONDITIONAL && statements->item->type != AST_RELEASE && 
            statements->item->type != AST_WHILE && statements->item->type != AST_FOR &&
            statements->item->type != AST_TYPE_DECL) {
            printf(";\n");
        }
    }
}

void compile(Ast *ast) {
    switch (ast->type) {
    case AST_LITERAL:
        switch (ast->lit->lit_type) {
        case INTEGER:
            printf("%ld", ast->lit->int_val);
            break;
        case FLOAT: // TODO this is truncated
            printf("%F", ast->lit->float_val);
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
        case STRUCT:
            if (ast->lit->struct_val.type->builtin) {
                printf("(struct _vs_%s){", ast->lit->struct_val.type->name);
            } else {
                printf("(struct _vs_%d){", ast->lit->struct_val.type->id);
            }
            if (ast->lit->struct_val.nmembers == 0) {
                printf("0");
            } else {
                for (int i = 0; i < ast->lit->struct_val.nmembers; i++) {
                    printf(".%s = ", ast->lit->struct_val.member_names[i]);
                    compile(ast->lit->struct_val.member_exprs[i]);
                    if (ast->lit->struct_val.member_exprs[i]->type == AST_TEMP_VAR || ast->lit->struct_val.member_exprs[i]->type == AST_ANON_FUNC_DECL) {
                        ast->lit->struct_val.member_exprs[i]->tempvar->var->consumed = 1;
                    }
                    if (i != ast->lit->struct_val.nmembers - 1) {
                        printf(", ");
                    }
                }
            }
            printf("}");
            break;
        case ENUM:
            printf("%ld", ast->lit->enum_val.enum_type->_enum.member_values[ast->lit->enum_val.enum_index]);
            break;
        }
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
        if (is_any(ast->cast->cast_type)) {
            // TODO how do I deal with passing like an int or something
            printf("(struct _vs_Any){.value_pointer=");
            compile_pointer(ast->cast->object);
            printf(",.type=(struct _vs_Type *)&_type_info%d}", ast->cast->object->var_type->id);
            break;
        }
        if (ast->cast->cast_type->base == STRUCT_T) {
            printf("*");
        }
        printf("((");
        emit_type(ast->cast->cast_type);
        if (ast->cast->cast_type->base == STRUCT_T) {
            printf("*");
        }
        printf(")");
        if (ast->cast->cast_type->base == STRUCT_T) {
            printf("&");
        }
        compile(ast->cast->object);
        printf(")");
        break;
    case AST_SLICE:
        printf("(struct array_type){.data=");
        compile_static_array(ast->slice->object);
        /*printf(".data");*/
        if (ast->slice->offset != NULL) {
            printf("+(");
            compile(ast->slice->offset);
            /*printf("*%d)", var_type(ast->slice_inner)->inner->size);*/
            printf(")");
        }
        printf(",.length=");
        if (ast->slice->length != NULL) {
            compile(ast->slice->length);
        } else if (ast->slice->object->var_type->base == STATIC_ARRAY_T) {
            printf("%ld", ast->slice->object->var_type->length); 
        } else {
            // TODO make this work (store tmpvar of thing being sliced to avoid
            // re-running whatever it is)
            error(ast->line, "Slicing a slice, uh ohhhhh");
        }
        if (ast->slice->offset != NULL) {
            printf("-");
            compile(ast->slice->offset);
        }
        printf("}");
        break;
    case AST_TEMP_VAR:
        emit_tempvar(ast);
        break;
    case AST_COPY:
        emit_copy(ast);
        break;
    case AST_HOLD:
        switch (ast->hold->object->var_type->base) {
        case STATIC_ARRAY_T: {
            Type *t = ast->hold->object->var_type;
            if (is_dynamic(t->inner)) {
                printf("(struct array_type){.data=_hold_%d(", t->id);
                compile(ast->hold->object);
                printf("),.length=%ld}", t->length);
            } else { // TODO this isn't copying, duh!
                printf("(struct array_type){.data=calloc(%ld, sizeof(", t->length);
                emit_type(t->inner);
                printf(")),.length=%ld}", t->length);
            }
            break;
        }
        case STRING_T:
            /*emit_tmpvar(ast->expr);*/
            /*break;*/
        default:
            printf("(_tmp%d = ", ast->hold->tempvar->id);
            printf("malloc(sizeof(");
            emit_type(ast->hold->object->var_type);

            printf(")), *_tmp%d = ", ast->hold->tempvar->id);
            compile(ast->hold->object);
            printf(", _tmp%d", ast->hold->tempvar->id);
            printf(")");
        }
        if (ast->hold->object->type == AST_TEMP_VAR) {
            ast->hold->object->tempvar->var->consumed = 1;
        }
        break;
    case AST_IDENTIFIER:
        if (ast->ident->var->ext) {
            printf("%s", ast->ident->var->name);
        } else if (ast->ident->var->type->base == FN_T && ast->ident->var->constant) {
            printf("_vs_%d", ast->ident->var->id);
        } else {
            printf("_vs_%s", ast->ident->var->name);
        }
        break;
    case AST_RETURN:
        if (ast->ret->expr != NULL) {
            if (ast->ret->expr->type == AST_TEMP_VAR || ast->ret->expr->type == AST_ANON_FUNC_DECL) {
                ast->ret->expr->tempvar->var->consumed = 1;
            }
            emit_type(ast->ret->expr->var_type);
            printf("_ret = ");
            compile(ast->ret->expr); // need to do something with tmpvar instead
            printf(";");
        }
        printf("\n");
        emit_free_locals(ast->ret->scope);
        indent();
        printf("return");
        if (ast->ret->expr != NULL) {
            if (ast->ret->expr->type == AST_IDENTIFIER) {
                printf(" ");
                compile(ast->ret->expr);
            } else {
                printf(" _ret");
            }
        }
        break;
    case AST_RELEASE:
        if (ast->release->object->type == AST_IDENTIFIER) {
            emit_free(ast->release->object->ident->var);
        } else if (ast->release->object->type == AST_DOT) {
            indent();
            printf ("{\n");
            _indent++;
            indent();
            Var *v = make_var("0", ast->release->object->var_type);
            emit_type(v->type);
            printf("_vs_0 = ");
            compile(ast->release->object);
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
        if (ast->fn_decl->var->type->bindings != NULL) {
            int id = ast->fn_decl->var->type->bindings_id;
            TypeList *bindings = ast->fn_decl->var->type->bindings;
            AstList *bind_exprs = get_binding_exprs(id);

            printf("(_cl_%d = malloc(%d),", id, bindings->item->offset + bindings->item->size);
            bindings = reverse_typelist(bindings);
            bind_exprs = reverse_astlist(bind_exprs);
            while (bindings != NULL && bind_exprs != NULL) {
                // TODO fix array bindings? see binds.vs
                printf("*((");
                emit_type(bindings->item);
                printf("*)(_cl_%d+%d))=", id, bindings->item->offset);
                compile(bind_exprs->item);
                if (bind_exprs->item->type == AST_TEMP_VAR) {
                    bind_exprs->item->tempvar->var->consumed = 1;
                } else if (bind_exprs->item->type == AST_IDENTIFIER) {
                    bind_exprs->item->ident->var->consumed = 1;
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
        printf("_vs_%d", ast->fn_decl->var->id);
        if (ast->fn_decl->var->type->bindings != NULL) {
            printf(")");
        }
        break;
    case AST_EXTERN_FUNC_DECL: 
        break;
    case AST_CALL: {
        Type *t = ast->call->fn->var_type;
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
            compile(ast->call->fn);
            printf("))");
        } else {
            compile(ast->call->fn);
        }

        printf("(");
        AstList *args = ast->call->args;
        TypeList *argtypes = t->fn.args;
        while (args != NULL) {
            Type *t = args->item->var_type;
            if (t->base == STRUCT_T && is_dynamic(t)) {
                if (t->builtin) {
                    printf("_copy_%s(", t->name);
                } else {
                    printf("_copy_%d(", t->id);
                }
                compile(args->item);
                printf(")");
            } else {
                if (argtypes->item->base == ARRAY_T) {
                    compile_unspecified_array(args->item); 
                } else {
                    compile(args->item);
                }
            }
            if (args->item->type == AST_TEMP_VAR && is_dynamic(t)) {
                args->item->tempvar->var->consumed = 1;
            }
            if (args->next != NULL) {
                printf(",");
            }
            args = args->next;
            argtypes = argtypes->next;
        }
        printf(")");
        break;
    }
    case AST_INDEX: {
        /*int ptr = 0;*/
        Type *lt = ast->index->object->var_type;
        /*if (lt->base == PTR_T) {*/
            /*lt = lt->inner;*/
            /*ptr = 1;*/
        /*}*/
        if (is_array(lt)) {
            printf("(");
            if (lt->base == ARRAY_T) {
                printf("(");
                emit_type(ast->index->object->var_type->inner);
                printf("*)");
            }
            /*if (ptr) {*/
                /*printf("*");*/
            /*}*/
            compile_static_array(ast->index->object);
            printf(")[");
            compile(ast->index->index);
            printf("]");
        } else {
            // TODO why is this here? strings? ptrs?
            compile(ast->index->object);
            printf("[");
            compile(ast->index->index);
            printf("]");
        }
        break;
    }
    case AST_BLOCK:
        compile_block(ast->block);
        break;
    case AST_SCOPE:
        compile_scope(ast->scope);
        break;
    case AST_CONDITIONAL:
        printf("if (");
        compile(ast->cond->condition);
        printf(") {\n");
        _indent++;
        compile_block(ast->cond->if_body);
        _indent--;
        if (ast->cond->else_body != NULL) {
            indent();
            printf("} else {\n");
            _indent++;
            compile_block(ast->cond->else_body);
            _indent--;
        }
        indent();
        printf("}\n");
        break;
    case AST_WHILE:
        printf("while (");
        compile(ast->while_loop->condition);
        printf(") ");
        compile_scope(ast->while_loop->body);
        break;
    case AST_FOR:
        // TODO loop depth should change iter var name?
        printf("{\n");
        _indent++;
        indent();
        printf("struct array_type _iter = ");
        compile_unspecified_array(ast->for_loop->iterable);
        printf(";\n");
        indent();
        printf("for (long _i = 0; _i < _iter.length; _i++) {\n");
        _indent++;
        indent();
        emit_type(ast->for_loop->itervar->type);
        printf("_vs_%s = ((", ast->for_loop->itervar->name);
        emit_type(ast->for_loop->itervar->type);
        printf("*)_iter.data)[_i];\n");

        indent();
        emit_scope_start(ast->for_loop->body);
        // TODO don't clear these vars!
        compile_block(ast->for_loop->body->body);
        emit_scope_end(ast->for_loop->body);

        _indent--;
        indent();
        printf("}\n");

        _indent--;
        indent();
        printf("}\n");
        break;
    case AST_BIND:
        printf("*((");
        emit_type(ast->bind->expr->var_type);
        printf("*)_cl_%d+%d)", ast->bind->bind_id, ast->bind->offset);
        break;
    case AST_TYPEINFO:
        printf("((struct _vs_Type *)&_type_info%d)", ast->typeinfo->typeinfo_target->id);
        break;
    case AST_TYPE_DECL:
        break;
    case AST_ENUM_DECL:
        break;
    default:
        error(ast->line, "No idea how to deal with this.");
    }
}

void emit_scope_start(AstScope *scope) {
    printf("{\n");
    _indent++;
    VarList *list = scope->locals;
    while (list != NULL) {
        if (list->item->temp) {
            indent();
            emit_type(list->item->type);
            printf("_tmp%d", list->item->id);
            if (list->item->type->base == STRING_T || list->item->type->base == STRUCT_T) {
                printf(" = {0}");
            }
            printf(";\n");
        }
        list = list->next;
    }
}

void emit_free_struct(char *name, Type *st, int is_ptr) {
    char *sep = is_ptr ? "->" : ".";
    for (int i = 0; i < st->st.nmembers; i++) {
        switch (st->st.member_types[i]->base) {
        case STRING_T:
            indent();
            printf("if (%s%s%s.bytes != NULL) {\n", name, sep, st->st.member_names[i]);
            _indent++;
            indent();
            printf("free(%s%s%s.bytes);\n", name, sep, st->st.member_names[i]);
            _indent--;
            indent();
            printf("}\n");
            break;
        case STRUCT_T: {
            char *memname = malloc(sizeof(char) * (strlen(name) + strlen(sep) + strlen(st->st.member_names[i]) + 2));
            sprintf(memname, "%s%s%s", name, sep, st->st.member_names[i]);
            emit_free_struct(memname, st->st.member_types[i], (st->st.member_types[i]->base == PTR_T || st->st.member_types[i]->base == BASEPTR_T));
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
    case STATIC_ARRAY_T:
        if (is_dynamic(var->type->inner)) {
            indent();
            printf("{\n");
            _indent++;
            indent();
            emit_type(var->type->inner);
            printf("*_0 = _vs_%s;\n", var->name);
            indent();
            printf("for (int i = 0; i < %ld; i++) {\n", var->type->length);
            _indent++;
            indent();
            emit_type(var->type->inner);
            printf("_vs_i = _0[i];\n");
            Var v = {.name="i",.type=var->type->inner,.temp=0,.initialized=1};
            emit_free(&v);
            _indent--;
            indent();
            printf("}\n");
            _indent--;
            indent();
            printf("}\n");
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

void emit_free_locals(AstScope *scope) {
    // TODO change this to be "initialized in scope"
    VarList *locals = scope->locals;
    while (locals != NULL) {
        Var *v = locals->item;
        locals = locals->next;
        // TODO got to be a better way to handle this here
        if (v->consumed ||
                (v->type->base != FN_T &&
                    ((v->type->base == BASEPTR_T || v->type->base == PTR_T) ||
                     v->type->held ||
                     !v->initialized ||
                     (v->temp && v->consumed)))) {
            continue;
        }
        emit_free(v);
    }
}

void emit_scope_end(AstScope *scope) {
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
        emit_type(v->type->fn.ret);
        printf("(*");
    } else if (v->type->base == STATIC_ARRAY_T) {
        emit_type(v->type->inner);
        if (!v->ext) {
            printf("_vs_");
        }
        printf("%s[%ld] = {0};\n", v->name, v->type->length);
        return;
    } else {
        emit_type(v->type);
    }
    if (!v->ext) {
        printf("_vs_");
    }
    printf("%s", v->name);
    if (v->type->base == FN_T) {
        printf(")(");
        TypeList *args = v->type->fn.args;
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
    printf("/* %s */\n", v->name);
    if (v->ext) {
        printf("extern ");
    }
    emit_type(v->type->fn.ret);
    if (!v->ext) {
        printf("_vs_");
    }
    // TODO do this in a non-hacky way
    if (!strcmp(v->name, "main")) {
        printf("%s(", v->name);
    } else {
        printf("%d(", v->id);
    }
    TypeList *args = v->type->fn.args;
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

void emit_hold_func_decl(Type *t) {
    emit_type(t); // should this be void *?
    printf("_hold_%d(", t->id);
    emit_type(t);
    printf("other) {\n");
    _indent++;
    indent();

    emit_type(t);
    printf("held = malloc(sizeof(");
    emit_type(t->inner);
    printf(") * %ld);\n", t->length);
    indent();
    emit_static_array_copy(t, "held", "other");
    indent();
    printf("return held;\n");
    
    _indent--;
    indent();
    printf("}\n");
}

void emit_typeinfo_decl(Type *t) {
    switch (t->base) {
    case PTR_T:
        printf("struct _vs_PtrType _type_info%d;\n", t->id);
        break;
    case INT_T:
    case UINT_T:
    case FLOAT_T:
        printf("struct _vs_NumType _type_info%d;\n", t->id);
        break;
    case ENUM_T:
        printf("struct string_type _type_info%d_members[%d] = {\n", t->id, t->_enum.nmembers);
        for (int i = 0; i < t->_enum.nmembers; i++) {
            printf("  {%ld, \"%s\"},", strlen(t->_enum.member_names[i]), t->_enum.member_names[i]);
        }
        printf("};\n");
        /*emit_type(t->_enum.inner);*/
        /*printf("_type_info%d_values[%d] = {\n", t->id, t->_enum.nmembers);*/
        /*for (int i = 0; i < t->_enum.nmembers; i++) {*/
            /*printf("  %ld,", t->_enum.member_values[i]);*/
        /*}*/
        /*printf("};\n");*/
        printf("struct _vs_EnumType _type_info%d;", t->id);
        break;
    case STRUCT_T:
        printf("struct _vs_StructMember _type_info%d_members[%d];\n", t->id, t->st.nmembers);
        printf("struct _vs_StructType _type_info%d;", t->id);
        break;
    case STATIC_ARRAY_T:
    case ARRAY_T:
        printf("struct _vs_ArrayType _type_info%d;\n", t->id);
        break;
    case FN_T:
        printf("struct _vs_Type *_type_info%d_args[%d];\n", t->id, t->fn.nargs);
        printf("struct _vs_FnType _type_info%d;\n", t->id);
        break;
    case BASEPTR_T:
    case STRING_T:
    case BOOL_T:
    default:
        printf("struct _vs_Type _type_info%d;\n", t->id);
        break;
    }
}

void emit_typeinfo_init(Type *t) {
    switch (t->base) {
    case ENUM_T:
        /*printf("_type_info%d = (struct _vs_EnumType){%d, {%ld, \"%s\"}, _type_info%d, {%d, _type_info%d_members}, {%d, _type_info%d_values}};\n",*/
                /*t->id, t->id, strlen(t->name), t->name, t->_enum.inner->id, t->st.nmembers,*/
                /*t->id, t->st.nmembers, t->id);*/
        printf("_type_info%d = (struct _vs_EnumType){%d, {%ld, \"%s\"}, (struct _vs_Type *)&_type_info%d, {%d, _type_info%d_members}};\n",
                t->id, t->id, strlen(t->name), t->name, t->_enum.inner->id, t->st.nmembers,
                t->id);
        break;
    case PTR_T:
        printf("_type_info%d = (struct _vs_PtrType){%d, {%ld, \"%s\"}, (struct _vs_Type *)&_type_info%d};\n", t->id, t->id, strlen(t->name), t->name, t->inner->id);
        break;
    case INT_T:
    case UINT_T:
    case FLOAT_T:
        printf("_type_info%d = (struct _vs_NumType){%d, {%ld, \"%s\"}, %d};\n", t->id, t->id, strlen(t->name), t->name, t->size);
        break;
    case STRUCT_T:
        for (int i = 0; i < t->st.nmembers; i++) {
            printf("_type_info%d_members[%d] = (struct _vs_StructMember){{%ld, \"%s\"}, (struct _vs_Type *)&_type_info%d};\n",
                    t->id, i, strlen(t->st.member_names[i]), t->st.member_names[i],
                    t->st.member_types[i]->id);
            indent();
        }
        printf("_type_info%d = (struct _vs_StructType){%d, {%ld, \"%s\"}, {%d, _type_info%d_members}};\n", t->id, t->id, strlen(t->name), t->name, t->st.nmembers, t->id);
        break;
    case STATIC_ARRAY_T:
    case ARRAY_T: // TODO make this not have a name? switch Type to have enum in name slot for base type
        printf("_type_info%d = (struct _vs_ArrayType){%d, {%ld, \"%s\"}, (struct _vs_Type *)&_type_info%d, %ld, %d};\n", t->id, t->id, strlen(t->name), t->name, t->inner->id,
                t->base == STATIC_ARRAY_T ? t->length : 0, t->base == STATIC_ARRAY_T);
        break;
    case FN_T: {
        int i = 0;
        for (TypeList *list = t->fn.args; list != NULL; list = list->next) {
            printf("_type_info%d_args[%d] = (struct _vs_Type *)&_type_info%d;\n", t->id, i, list->item->id);
            i++;
            indent();
        }
        printf("_type_info%d = (struct _vs_FnType){%d, {%ld, \"%s\"}, {%d, (struct _vs_Type **)_type_info%d_args}, (struct _vs_Type *)&_type_info%d, %d};\n", t->id, t->id, strlen(t->name), t->name, t->fn.nargs, t->id, t->fn.ret->id, !t->named);
        break;
    }
    case BASEPTR_T:
    case STRING_T:
    case BOOL_T:
    default:
        printf("_type_info%d = (struct _vs_Type){%d, {%ld, \"%s\"}};\n", t->id, t->id, strlen(t->name), t->name);
        break;
    }
}

int main(int argc, char **argv) {
    int just_ast = 0;
    if (argc > 1 && !strcmp(argv[1], "-a")) {
        just_ast = 1;
    }
    
    AstScope *root_scope = new_scope(NULL);
    init_types(root_scope);
    Ast *root = parse_scope(root_scope, NULL);
    init_builtins();
    root = parse_semantics(root, root->scope);
    if (just_ast) {
        print_ast(root);
    } else {
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

        TypeList *reg = get_used_types();
        TypeList *tmp = reg;
        while (tmp != NULL) {
            if (tmp->item->base == STRUCT_T) {
                emit_struct_decl(tmp->item);
            }
            tmp = tmp->next;
        }
        if (reg != NULL) {
            /*fprintf(stderr, "Types in use:\n");*/
            while (reg != NULL) {
                if (reg->item->unresolved) {
                    error(-1, "[INTERNAL] Undefined type '%s'.", reg->item->name);
                }
                emit_typeinfo_decl(reg->item);
                reg = reg->next;
            }
        }

        printf("void _verse_init_typeinfo() {\n");
        _indent++;
        reg = get_used_types();
        while (reg != NULL) {
            indent();
            emit_typeinfo_init(reg->item);
            reg = reg->next;
        }
        _indent--;
        printf("}\n");

        TypeList *holds = get_global_hold_funcs();
        while (holds != NULL) {
            emit_hold_func_decl(holds->item);
            holds = holds->next;
        }

        AstList *fnlist = get_global_funcs();
        while (fnlist != NULL) {
            emit_forward_decl(fnlist->item->fn_decl->var);
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
               "    _verse_init_typeinfo();\n"
               "    _verse_init();\n"
               "    return _vs_main();\n"
               "}");
    }
    return 0;
}
