#include "ast.h"

static int next_ast_id = 0;

Ast *ast_alloc(AstType type) {
    Ast *ast = malloc(sizeof(Ast));
    ast->id = last_ast_id++;
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
    case AST_TEMP_VAR:
        ast->tempvar = calloc(sizeof(AstTempVar), 1);
        break;
    case AST_RELEASE:
        ast->release = calloc(sizeof(AstRelease), 1);
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
    case AST_HOLD:
        ast->hold = calloc(sizeof(AstHold), 1);
        break;
    case AST_BIND:
        ast->bind = calloc(sizeof(AstBind), 1);
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

AstList *copy_astlist(AstList *list) {
    AstList *copy = NULL;
    for (; list != NULL; list = list->next) {
        copy = astlist_append(copy, deep_copy(list->item));
    }
    return reverse_astlist(copy);
}

AstBlock *copy_block(AstBlock *block) {
    AstBlock *copy = malloc(sizeof(AstBlock));
    *copy = *block;
    copy->statements = copy_astlist(block->statements);
    return copy;
}

AstScope *copy_scope(AstScope *scope) {
    AstScope *copy = malloc(sizeof(AstScope));
    *copy = *scope;
    copy->body = copy_block(scope->body);
    copy->bindings = copy_astlist(scope->bindings);
    // do locals need to be copied? not all of them?
    return copy;
}

Ast *deep_copy(Ast *ast) {
    Ast *copy = malloc(sizeof(Ast));
    *copy = *ast;

    switch (ast->type) {
    case AST_DOT:
        copy->dot = calloc(sizeof(AstDot), 1);
        copy->dot->member_name = ast->dot->member_name;
        copy->dot->object = deep_copy(ast->dot->object);
        break;
    case AST_ASSIGN:
    case AST_BINOP:
        copy->binary = calloc(sizeof(AstBinaryOp), 1);
        copy->binary->op = ast->binary->op;
        copy->binary->left = deep_copy(ast->binary->left);
        copy->binary->right = deep_copy(ast->binary->right);
        break;
    case AST_UOP:
        copy->unary = calloc(sizeof(AstUnaryOp), 1);
        copy->unary->op = ast->unary->op;
        copy->unary->object = deep_copy(ast->unary->object);
        break;
    case AST_COPY:
        copy->copy = calloc(sizeof(AstCopy), 1);
        copy->copy->expr = deep_copy(ast->copy->expr);
        break;
    case AST_TEMP_VAR:
        copy->tempvar = calloc(sizeof(AstTempVar), 1);
        copy->tempvar->var = ast->tempvar->var;
        copy->tempvar->expr = deep_copy(ast->tempvar->expr);
        break;
    case AST_RELEASE:
        copy->release = calloc(sizeof(AstRelease), 1);
        copy->release->object = deep_copy(ast->release->object);
        break;
    case AST_DECL:
        copy->decl = calloc(sizeof(AstDecl), 1);
        copy->decl->var = ast->decl->var;
        copy->decl->global = ast->decl->global;
        copy->decl->init = deep_copy(ast->decl->init);
        break;
    case AST_ANON_FUNC_DECL:
    case AST_FUNC_DECL:
        copy->fn_decl = calloc(sizeof(AstFnDecl), 1);
        *copy->fn_decl = *ast->fn_decl;
        copy->fn_decl->scope = copy_scope(ast->fn_decl->scope);
        // Keep the same polymorphs!
        break;
    case AST_CALL:
        copy->call = calloc(sizeof(AstCall), 1);
        *copy->call = *ast->call;
        copy->call->fn = deep_copy(ast->call->fn);
        copy->call->args = copy_astlist(ast->call->args);
        break;
    case AST_INDEX:
        copy->index = calloc(sizeof(AstIndex), 1);
        copy->index->object = deep_copy(ast->index->object);
        copy->index->index = deep_copy(ast->index->index);
        break;
    case AST_SLICE:
        copy->slice = calloc(sizeof(AstSlice), 1);
        copy->slice->object = deep_copy(ast->slice->object);
        copy->slice->offset = deep_copy(ast->slice->offset);
        copy->slice->length = deep_copy(ast->slice->length);
        break;
    case AST_CONDITIONAL:
        copy->cond = calloc(sizeof(AstConditional), 1);
        copy->cond->condition = deep_copy(ast->cond->condition);
        copy->cond->if_body = copy_scope(ast->cond->if_body);
        copy->cond->else_body = copy_scope(ast->cond->else_body);
        break;
    /*case AST_SCOPE:*/
        /*copy->scope = copy_scope(ast->scope);*/
        /*break;*/
    case AST_RETURN:
        copy->ret = calloc(sizeof(AstReturn), 1);
        // wut
        copy->ret->scope = ast->ret->scope;
        copy->ret->expr = deep_copy(ast->ret->expr);
        break;
    case AST_BLOCK:
        copy->block = copy_block(ast->block);
        break;
    case AST_WHILE:
        copy->while_loop = calloc(sizeof(AstWhile), 1);
        copy->while_loop->condition = deep_copy(ast->while_loop->condition);
        copy->while_loop->body = copy_scope(ast->while_loop->body);
        break;
    case AST_FOR:
        copy->for_loop = calloc(sizeof(AstFor), 1);
        // need to copy itervar
        copy->for_loop->itervar = malloc(sizeof(Var));
        *copy->for_loop->itervar = *ast->for_loop->itervar;
        copy->for_loop->iterable = deep_copy(ast->for_loop->iterable);
        copy->for_loop->body = copy_scope(ast->for_loop->body);
        break;
    case AST_HOLD:
        copy->hold = calloc(sizeof(AstHold), 1);
        copy->hold->object = deep_copy(ast->hold->object);
        copy->hold->tempvar = malloc(sizeof(Var));
        *copy->hold->tempvar = *ast->hold->tempvar;
        break;
    case AST_BIND:
        copy->bind = calloc(sizeof(AstBind), 1);
        copy->bind->expr = deep_copy(ast->bind->expr);
        break;
    case AST_CAST:
        copy->cast = calloc(sizeof(AstCast), 1);
        copy->cast->cast_type = ast->cast->cast_type;
        copy->cast->object = deep_copy(ast->cast->object);
        break;
    case AST_DIRECTIVE:
        copy->directive = calloc(sizeof(AstDirective), 1);
        copy->directive->name = ast->directive->name;
        copy->directive->object = deep_copy(ast->directive->object);
        break;
    case AST_USE:
        copy->use = calloc(sizeof(AstUse), 1);
        copy->use->object = deep_copy(ast->use->object);
        break;
    default:
        break;
    }
    return copy;
}

Ast *make_ast_copy(Ast *ast) {
    Ast *cp = ast_alloc(AST_COPY);
    cp->copy->expr = ast;
    cp->var_type = ast->var_type;
    return cp;
}

AstScope *new_scope(AstScope *parent) {
    AstScope *scope = malloc(sizeof(AstScope));
    scope->locals = NULL;
    scope->local_types = NULL;
    scope->unresolved_types = NULL;
    scope->parent = parent;
    scope->has_return = 0;
    scope->is_function = 0;
    return scope;
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

Ast *make_ast_tempvar(Ast *ast, Var *tempvar) {
    Ast *tmp = ast_alloc(AST_TEMP_VAR);
    tmp->tempvar->var = tempvar;
    tmp->tempvar->expr = ast;
    tmp->var_type = tempvar->type;
    return tmp;
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
    if (is_any(to) && !is_any(from->var_type) && !is_lvalue(from)) {
        allocate_temp_var(scope, from);
        return 1;
    }
    
    Type *t = resolve_alias(to);
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
                if (t->base != FLOAT_T) {
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

Var *get_ast_var_noerror(Ast *ast) {
    switch (ast->type) {
    case AST_DOT: {
        Var *v = get_ast_var_noerror(ast->dot->object); // should this just error?
        if (v == NULL) {
            return NULL;
        }
        Type *t = v->type;
        for (int i = 0; i < t->st.nmembers; i++) {
            if (!strcmp(t->st.member_names[i], ast->dot->member_name)) {
                return v->members[i];
            }
        }
        error(ast->line, ast->file, "Couldn't get member '%s' in struct '%s' (%d).", ast->dot->member_name, t->name, t->id);
    }
    case AST_IDENTIFIER:
        return ast->ident->var;
    case AST_DECL:
        return ast->decl->var;
    case AST_FUNC_DECL: // is this allowed?
        return ast->fn_decl->var;
    default:
        break;
    }
    return NULL;
}

Var *get_ast_var(Ast *ast) {
    Var *v = get_ast_var_noerror(ast);
    if (v == NULL) {
        error(ast->line, ast->file, "Can't get_ast_var(%d)", ast->type);
    }
    return v;
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
