#include "ast.h"

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

Type *var_type(Ast *ast) {
    switch (ast->type) {
    case AST_STRING:
        return base_type(STRING_T);
    case AST_INTEGER:
        return base_type(INT_T);
    case AST_FLOAT:
        return base_type(FLOAT_T);
    case AST_BOOL:
        return base_type(BOOL_T);
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
    case AST_INDEX: {
        Type *t = var_type(ast->left);
        if (t->base == PTR_T) {
            t = t->inner;
        }
        return t->inner;
    }
    case AST_SLICE:
        return make_array_type(var_type(ast->slice_inner)->inner);
    case AST_ANON_FUNC_DECL:
        return ast->fn_decl_var->type;
    case AST_BIND:
        return var_type(ast->bind_expr);
    case AST_UOP:
        switch (ast->op) {
        case OP_NOT:
            return base_type(BOOL_T);
        case OP_ADDR:
            return make_ptr_type(var_type(ast->right));
        case OP_DEREF:
            return var_type(ast->right)->inner;
        case OP_MINUS:
        case OP_PLUS:
            return var_type(ast->right);
        default:
            error(ast->line, "don't know how to infer vartype of operator '%s' (%d).", op_to_str(ast->op), ast->op);
        }
        break;
    case AST_BINOP:
        if (is_comparison(ast->op)) {
            return base_type(BOOL_T);
        } else if (ast->op != OP_ASSIGN && is_numeric(var_type(ast->left))) {
            return promote_number_type(var_type(ast->left), var_type(ast->right));
        }
        return var_type(ast->left);
    case AST_DOT: {
        Type *t = var_type(ast->dot_left);
        if (t->base == PTR_T) {
            t = t->inner;
        }
        if (is_array(t)) {
            if (!strcmp(ast->member_name, "length")) {
                return base_type(INT_T);
            } else if (!strcmp(ast->member_name, "data")) {
                return make_ptr_type(t->inner);
            } else {
                error(ast->line, "Array has no member named '%s'.", ast->member_name);
            }
        }
        if (t->base == STRING_T) {
            if (!strcmp(ast->member_name, "length")) {
                return base_type(INT_T);
            } else if (!strcmp(ast->member_name, "bytes")) {
                return make_ptr_type(base_numeric_type(UINT_T, 8));
            } else {
                error(ast->line, "String has no member named '%s'.", ast->member_name);
            }
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
    return base_type(VOID_T);
}

int is_lvalue(Ast *ast) {
    return ast->type == AST_IDENTIFIER ||
        ast->type == AST_DOT ||
        ast->type == AST_INDEX ||
        (ast->type == AST_UOP && ast->op == OP_DEREF);
}

int is_literal(Ast *ast) {
    switch (ast->type) {
    case AST_STRING:
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_BOOL:
    case AST_STRUCT:
    case AST_ANON_FUNC_DECL:
        return 1;
    case AST_TEMP_VAR:
        return is_literal(ast->expr);
    /*case AST_BINOP:*/
        /*return is_literal(ast->left) && is_literal(ast->right);*/
    }
    return 0;
}

Ast *make_ast_bool(long ival) {
    Ast *ast = ast_alloc(AST_BOOL);
    ast->ival = ival;
    return ast;
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

Ast *make_ast_decl(char *name, Type *type) {
    Ast *ast = ast_alloc(AST_DECL);
    ast->decl_var = make_var(name, type);
    return ast;
}

Ast *make_ast_assign(Ast *left, Ast *right) {
    Ast *ast = ast_alloc(AST_ASSIGN);
    ast->left = left;
    ast->right = right;
    return ast;
}

Ast *make_ast_binop(int op, Ast *left, Ast *right) {
    Ast *binop = ast_alloc(AST_BINOP);
    binop->op = op;
    binop->left = left;
    binop->right = right;
    return binop;
}

Ast *make_ast_slice(Ast *inner, Ast *offset, Ast *length) {
    Ast *slice = ast_alloc(AST_SLICE);
    slice->slice_inner = inner;
    slice->slice_offset = offset;
    slice->slice_length = length;
    return slice;
}

Ast *cast_literal(Type *t, Ast *ast) {
    if (can_cast(t, var_type(ast))) {
        Ast *cast = ast_alloc(AST_CAST);
        cast->cast_left = ast;
        cast->cast_type = t;
        return cast;
    } else if (is_numeric(t) && is_numeric(var_type(ast))) {
        int loss = 0;
        int lit_type = ast->type;
        if (lit_type == AST_INTEGER) {
            if (t->base == UINT_T) {
                if (ast->ival < 0) {
                    error(ast->line, "Cannot implicitly cast negative literal value to integer type '%s'.", t->name);
                }
                loss = precision_loss_uint(t, ast->ival);
            } else {
                loss = precision_loss_int(t, ast->ival);
            }
        } else if (lit_type == AST_FLOAT) {
            if (t->base != FLOAT_T) {
                error(ast->line, "Cannot implicitly cast floating point literal to integer type '%s'.", t->name);
            }
            loss = precision_loss_float(t, ast->fval);
        }
        if (loss) {
            error(ast->line, "Cannot implicitly cast literal value of type '%s' to type '%s' due to precision loss.", var_type(ast)->name, t->name);
        }
        Ast *cast = ast_alloc(AST_CAST);
        cast->cast_left = ast;
        cast->cast_type = t;
        return cast;
    }
    error(ast->line, "Cannot implicitly cast value of type '%s' to type '%s'", var_type(ast)->name, t->name);
    return NULL;
}

Ast *try_implicit_cast(Type *t, Ast *ast) {
    if (is_literal(ast)) {
        return cast_literal(t, ast);
    }
    error(ast->line, "Cannot implicitly cast value of type '%s' to type '%s'", var_type(ast)->name, t->name);
    return NULL;
}

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

AstList *astlist_append(AstList *list, Ast *ast) {
    AstList *l = malloc(sizeof(AstList));
    l->item = ast;
    l->next = list;
    return l;
}

void print_ast(Ast *ast) {
    switch (ast->type) {
    case AST_INTEGER:
        printf("%ld", ast->ival);
        break;
    case AST_FLOAT:
        printf("%f", ast->fval);
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
    case AST_INDEX:
        print_ast(ast->left);
        printf("[");
        print_ast(ast->right);
        printf("]");
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
    case AST_HOLD:
        printf("(hold ");
        print_ast(ast->expr);
        printf(")");
    default:
        error(ast->line, "Cannot print this ast.");
    }
}
