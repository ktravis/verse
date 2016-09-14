#include "ast.h"

static int next_ast_id = 0;

Ast *ast_alloc(AstType type) {
    Ast *ast = malloc(sizeof(Ast));
    ast->id = next_ast_id++;
    ast->type = type;
    ast->line = lineno();
    ast->file = current_file_name();
    ast->var_type = NULL;

    switch (type) {
    case AST_LITERAL:
        ast->lit = calloc(sizeof(AstLiteral), 1);
        break;
    case AST_DOT:
        ast->dot = calloc(sizeof(AstDot), 1);
        break;
    case AST_ASSIGN:
    case AST_BINOP:
        ast->binary = calloc(sizeof(AstBinaryOp), 1);
        break;
    case AST_UOP:
        ast->unary = calloc(sizeof(AstUnaryOp), 1);
        break;
    case AST_IDENTIFIER:
        ast->ident = calloc(sizeof(AstIdent), 1);
        break;
    case AST_COPY:
        ast->copy = calloc(sizeof(AstCopy), 1);
        break;
    case AST_DECL:
        ast->decl = calloc(sizeof(AstDecl), 1);
        break;
    case AST_ANON_FUNC_DECL:
    case AST_EXTERN_FUNC_DECL:
    case AST_FUNC_DECL:
        ast->fn_decl = calloc(sizeof(AstFnDecl), 1);
        break;
    case AST_CALL:
        ast->call = calloc(sizeof(AstCall), 1);
        break;
    case AST_INDEX:
        ast->index = calloc(sizeof(AstIndex), 1);
        break;
    case AST_SLICE:
        ast->slice = calloc(sizeof(AstSlice), 1);
        break;
    case AST_CONDITIONAL:
        ast->cond = calloc(sizeof(AstConditional), 1);
        break;
    /*case AST_SCOPE:*/
        /*ast->scope = calloc(sizeof(AstScope), 1);*/
        /*break;*/
    case AST_RETURN:
        ast->ret = calloc(sizeof(AstReturn), 1);
        break;
    case AST_TYPE_DECL:
        ast->type_decl = calloc(sizeof(AstTypeDecl), 1);
        break;
    case AST_BLOCK:
        ast->block = calloc(sizeof(AstBlock), 1);
        ast->block->startline = lineno();
        ast->block->file = ast->file;
        break;
    case AST_WHILE:
        ast->while_loop = calloc(sizeof(AstWhile), 1);
        break;
    case AST_FOR:
        ast->for_loop = calloc(sizeof(AstFor), 1);
        break;
    case AST_BREAK:
    case AST_CONTINUE:
        // Don't need any extra stuff
        break;
    case AST_CAST:
        ast->cast = calloc(sizeof(AstCast), 1);
        break;
    case AST_DIRECTIVE:
        ast->directive = calloc(sizeof(AstDirective), 1);
        break;
    case AST_TYPEINFO:
        ast->typeinfo = calloc(sizeof(AstTypeInfo), 1);
        break;
    case AST_ENUM_DECL:
        ast->enum_decl = calloc(sizeof(AstEnumDecl), 1);
        break;
    case AST_USE:
        ast->use = calloc(sizeof(AstUse), 1);
        break;
    }
    return ast;
}

Ast *make_ast_copy(Ast *ast) {
    Ast *cp = ast_alloc(AST_COPY);
    cp->copy->expr = ast;
    cp->var_type = ast->var_type;
    return cp;
}

Type *type_of_directive(Ast *ast) {
    char *n = ast->directive->name;
    /*if (!strcmp(n, "type")) {*/
        /*return typeinfo_t();*/
    /*}*/
    error(ast->line, ast->file, "Cannot determine type of unknown directive '%s'.", n);
    return NULL;
}

int is_lvalue(Ast *ast) {
    return ast->type == AST_IDENTIFIER ||
        ast->type == AST_DOT ||
        ast->type == AST_INDEX ||
        (ast->type == AST_UOP && ast->unary->op == OP_DEREF);
}

Ast *make_ast_directive(char *name, Ast *object) {
    Ast *ast = ast_alloc(AST_DIRECTIVE);
    ast->directive->name = name;
    ast->directive->object = object;
    return ast;
}

Ast *make_ast_bool(long ival) {
    Ast *ast = ast_alloc(AST_LITERAL);
    ast->lit->lit_type = BOOL;
    ast->lit->int_val = ival;
    return ast;
}

Ast *make_ast_string(char *str) {
    Ast *ast = ast_alloc(AST_LITERAL);
    ast->lit->lit_type = STRING;
    ast->lit->string_val = str;
    return ast;
}

Ast *make_ast_id(Var *var, char *name) {
    Ast *id = ast_alloc(AST_IDENTIFIER);
    id->ident->var = NULL;
    id->ident->varname = name;
    return id;
}

Ast *make_ast_dot_op(Ast *object, char *member_name) {
    Ast *ast = ast_alloc(AST_DOT);
    ast->dot->object = object;
    ast->dot->member_name = member_name;
    return ast;
}

Ast *make_ast_decl(char *name, Type *type) {
    Ast *ast = ast_alloc(AST_DECL);
    ast->decl->var = make_var(name, type);
    ast->decl->init = NULL;
    return ast;
}

Ast *make_ast_assign(Ast *left, Ast *right) {
    Ast *ast = ast_alloc(AST_ASSIGN);
    ast->binary->op = OP_ASSIGN;
    ast->binary->left = left;
    ast->binary->right = right;
    return ast;
}

Ast *make_ast_binop(int op, Ast *left, Ast *right) {
    Ast *binop = ast_alloc(AST_BINOP);
    binop->binary->op = op;
    binop->binary->left = left;
    binop->binary->right = right;
    return binop;
}

Ast *make_ast_slice(Ast *object, Ast *offset, Ast *length) {
    Ast *s = ast_alloc(AST_SLICE);
    s->slice->object = object;
    s->slice->offset = offset;
    s->slice->length = length;
    return s;
}

int can_coerce_type(Scope *scope, Type *to, Ast *from) {
    // resolve polymorphs
    if (is_any(to)) {
        if (!is_any(from->var_type) && !is_lvalue(from)) {
            allocate_temp_var(scope, from);
        }
        return 1;
    }
    
    Type *t = resolve_alias(to);
    if (t->comp == ARRAY) {
        if (from->var_type->comp == ARRAY) {
            return check_type(t->inner, from->var_type->inner);
        } else if (from->var_type->comp == STATIC_ARRAY) {
            return check_type(t->inner, from->var_type->array.inner);
        }
    }
    if (from->type == AST_LITERAL) {
        if (is_numeric(t) && is_numeric(from->var_type)) {
            int loss = 0;
            if (from->lit->lit_type == INTEGER) {
                if (t->data->base == UINT_T) {
                    if (from->lit->int_val < 0) {
                        error(from->line, from->file, 
                            "Cannot coerce negative literal value into integer type '%s'.",
                            type_to_string(to));
                    }
                    loss = precision_loss_uint(t, from->lit->int_val);
                } else {
                    loss = precision_loss_int(t, from->lit->int_val);
                }
            } else if (from->lit->lit_type == FLOAT) {
                if (t->data->base != FLOAT_T) {
                    error(from->line, from->file,
                        "Cannot coerce floating point literal into integer type '%s'.",
                        type_to_string(to));
                }
                loss = precision_loss_float(t, from->lit->float_val);
            }
            if (loss) {
                error(from->line, from->file,
                    "Cannot coerce literal value of type '%s' into type '%s' due to precision loss.",
                    type_to_string(from->var_type), type_to_string(to));
            }
            return 1;
        } else {
            if (can_cast(from->var_type, t)) {
                return 1;
            }

            error(from->line, from->file,
                "Cannot coerce literal value of type '%s' into type '%s'.",
                type_to_string(from->var_type), type_to_string(to));
        }
    }
    return 0;
}

char *get_varname(Ast *ast) {
    switch (ast->type) {
    case AST_DOT: {
        char *name = get_varname(ast->dot->object);
        if (name == NULL) {
            return NULL;
        }
        Type *orig = ast->dot->object->var_type;
        Type *t = resolve_alias(orig);
        for (int i = 0; i < t->st.nmembers; i++) {
            char *member_name = t->st.member_names[i];
            if (!strcmp(t->st.member_names[i], ast->dot->member_name)) {
                char *proxy_name;
                int proxy_name_len;
                if (orig->comp == REF) {
                    proxy_name_len = strlen(name) + strlen(member_name) + 2 + 1;
                    proxy_name = malloc(sizeof(char) * proxy_name_len);
                    snprintf(proxy_name, proxy_name_len, "%s->%s", name, member_name);
                } else {
                    proxy_name_len = strlen(name) + strlen(member_name) + 1 + 1;
                    proxy_name = malloc(sizeof(char) * proxy_name_len);
                    snprintf(proxy_name, proxy_name_len, "%s.%s", name, member_name);
                }
                return proxy_name;
            }
        }
        // shouldn't get here
        error(ast->line, ast->file, "Couldn't get member '%s' in struct '%s' (%d).", ast->dot->member_name, t->name, t->id);
    }
    case AST_IDENTIFIER:
        return ast->ident->var->name;
    default:
        break;
    }
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
    /*switch (ast->type) {*/
    /*case AST_INTEGER:*/
        /*printf("%ld", ast->ival);*/
        /*break;*/
    /*case AST_FLOAT:*/
        /*printf("%f", ast->fval);*/
        /*break;*/
    /*case AST_BOOL:*/
        /*printf("%s", ast->ival == 1 ? "true" : "false");*/
        /*break;*/
    /*case AST_STRING:*/
        /*printf("\"");*/
        /*print_quoted_string(ast->sval);*/
        /*printf("\"");*/
        /*break;*/
    /*case AST_DOT:*/
        /*print_ast(ast->dot_left);*/
        /*printf(".%s", ast->member_name);*/
        /*break;*/
    /*case AST_UOP:*/
        /*printf("(%s ", op_to_str(ast->op));*/
        /*print_ast(ast->right);*/
        /*printf(")");*/
        /*break;*/
    /*case AST_BINOP:*/
        /*printf("(%s ", op_to_str(ast->op));*/
        /*print_ast(ast->left);*/
        /*printf(" ");*/
        /*print_ast(ast->right);*/
        /*printf(")");*/
        /*break;*/
    /*case AST_TEMP_VAR:*/
        /*printf("(tmp ");*/
        /*print_ast(ast->expr);*/
        /*printf(")");*/
        /*break;*/
    /*case AST_IDENTIFIER:*/
        /*printf("%s", ast->varname);*/
        /*break;*/
    /*case AST_DECL:*/
        /*printf("(decl %s %s", ast->decl_var->name, ast->decl_var->type->name);*/
        /*if (ast->init != NULL) {*/
            /*printf(" ");*/
            /*print_ast(ast->init);*/
        /*}*/
        /*printf(")");*/
        /*break;*/
    /*case AST_TYPE_DECL:*/
        /*printf("(type %s %s)", ast->type_name, ast->target_type->name);*/
        /*break;*/
    /*case AST_EXTERN_FUNC_DECL:*/
        /*printf("(extern fn %s)", ast->fn_decl_var->name);*/
        /*break;*/
    /*case AST_ANON_FUNC_DECL:*/
    /*case AST_FUNC_DECL: {*/
        /*VarList *args = ast->fn_decl_args;*/
        /*printf("(fn ");*/
        /*if (!ast->anon) {*/
            /*printf("%s", ast->fn_decl_var->name);*/
        /*}*/
        /*printf("(");*/
        /*while (args != NULL) {*/
            /*printf("%s", args->item->name);*/
            /*if (args->next != NULL) {*/
                /*printf(",");*/
            /*}*/
            /*args = args->next;*/
        /*}*/
        /*printf("):%s ", ast->fn_decl_var->type->ret->name);*/
        /*print_ast(ast->fn_body);*/
        /*printf(")");*/
        /*break;*/
    /*}*/
    /*case AST_RETURN:*/
        /*printf("(return");*/
        /*if (ast->ret_expr != NULL) {*/
            /*printf(" ");*/
            /*print_ast(ast->ret_expr);*/
        /*}*/
        /*printf(")");*/
        /*break;*/
    /*case AST_CALL: {*/
        /*AstList *args = ast->args;*/
        /*print_ast(ast->fn);*/
        /*printf("(");*/
        /*while (args != NULL) {*/
            /*print_ast(args->item);*/
            /*if (args->next != NULL) {*/
                /*printf(",");*/
            /*}*/
            /*args = args->next;*/
        /*}*/
        /*printf(")");*/
        /*break;*/
    /*}*/
    /*case AST_INDEX:*/
        /*print_ast(ast->left);*/
        /*printf("[");*/
        /*print_ast(ast->right);*/
        /*printf("]");*/
        /*break;*/
    /*case AST_BLOCK:*/
        /*for (int i = 0; i < ast->num_statements; i++) {*/
            /*print_ast(ast->statements[i]);*/
        /*}*/
        /*break;*/
    /*case AST_SCOPE:*/
        /*printf("{ ");*/
        /*print_ast(ast->body);*/
        /*printf(" }");*/
        /*break;*/
    /*case AST_CONDITIONAL:*/
        /*printf("(if ");*/
        /*print_ast(ast->condition);*/
        /*printf(" ");*/
        /*print_ast(ast->if_body);*/
        /*if (ast->else_body != NULL) {*/
            /*printf(" ");*/
            /*print_ast(ast->else_body);*/
        /*}*/
        /*printf(")");*/
        /*break;*/
    /*case AST_HOLD:*/
        /*printf("(hold ");*/
        /*print_ast(ast->expr);*/
        /*printf(")");*/
    /*default:*/
        /*error(ast->line, "Cannot print this ast.");*/
    /*}*/
}
