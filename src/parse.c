#include "parse.h"

static int last_tmp_fn_id = 0;

static AstList *global_fn_decls = NULL;

Ast *parse_array_slice(Ast *object, Ast *offset, AstScope *scope) {
    Ast *s = ast_alloc(AST_SLICE);
    s->slice->object = object;
    s->slice->offset = offset;
    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing array slice.");
    } else if (t->type == TOK_RSQUARE) {
        s->slice->length = NULL;
    } else {
        s->slice->length = parse_expression(t, 0, scope);
        expect(TOK_RSQUARE);
    }
    return s; 
}

Ast *parse_array_index(Ast *object, AstScope *scope) {
    Tok *t = next_token();
    if (t->type == TOK_COLON) {
        return parse_array_slice(object, NULL, scope);
    }
    Ast *ind = ast_alloc(AST_INDEX);
    ind->index->object = object;
    ind->index->index = parse_expression(t, 0, scope);
    t = next_token();
    if (t->type == TOK_COLON) {
        Ast *offset = ind->index->index;
        free(ind);
        return parse_array_slice(object, offset, scope);
    } else if (t->type != TOK_RSQUARE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing array index.", to_string(t));
    }
    return ind; 
}

Ast *parse_arg_list(Ast *left, AstScope *scope) {
    Ast *func = ast_alloc(AST_CALL);
    func->call->args = NULL;
    func->call->fn = left;
    int n = 0;
    Tok *t;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        func->call->args = astlist_append(func->call->args, parse_expression(t, 0, scope));
        n++;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    func->call->args = reverse_astlist(func->call->args);
    func->call->nargs = n;
    return func; 
}

Ast *parse_declaration(Tok *t, AstScope *scope) {
    char *name = t->sval;
    Tok *next = next_token();
    Type *type = NULL;
    if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        type = make_type("auto", AUTO_T, -1);
    } else {
        type = parse_type(next, scope);
        next = next_token();
    }
    Ast *lhs = make_ast_decl(name, type);
    if (next == NULL) {
        error(lhs->line, lhs->file, "Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        lhs->decl->init = parse_expression(next_token(), 0, scope);
    } else if (next->type != TOK_SEMI) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing declaration.", to_string(next));
    } else {
        unget_token(next);
    }
    return lhs; 
}

Ast *parse_expression(Tok *t, int priority, AstScope *scope) {
    Ast *ast = parse_primary(t, scope);
    for (;;) {
        t = next_token();
        if (t == NULL) {
            return ast;
        } else if (t->type == TOK_SEMI || t->type == TOK_RPAREN || t->type == TOK_LBRACE || t->type == TOK_RBRACE || t->type == TOK_RSQUARE) {
            unget_token(t);
            return ast;
        }
        int next_priority = priority_of(t);
        if (next_priority < 0 || next_priority < priority) {
            unget_token(t);
            return ast;
        } else if (t->type == TOK_OPASSIGN) {
            Ast *rhs = parse_expression(next_token(), 0, scope);
            rhs = make_ast_binop(t->op, ast, rhs);
            rhs->line = ast->line; // are these necessary?
            ast = make_ast_assign(ast, rhs);
            ast->line = rhs->line;
        } else if (t->type == TOK_OP) {
            if (t->op == OP_ASSIGN) {
                ast = make_ast_assign(ast, parse_expression(next_token(), 0, scope));
            } else if (t->op == OP_DOT) {
                Tok *next = next_token();
                if (next->type != TOK_ID) {
                    // TODO change this to something like "TOK_KEYWORD"
                    if (next->type == TOK_TYPE) {
                        next->type = TOK_ID;
                        next->sval = "type";
                    } else {
                        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing dot operation.", to_string(next));
                    }
                }
                ast = make_ast_dot_op(ast, next->sval);
                ast->type = AST_DOT;
            } else if (t->op == OP_CAST) {
                Ast *c = ast_alloc(AST_CAST);
                c->cast->object = ast;
                c->cast->cast_type = parse_type(next_token(), scope);
                ast = c;
            } else {
                ast = make_ast_binop(t->op, ast, parse_expression(next_token(), next_priority + 1, scope));
            }
        } else if (t->type == TOK_LPAREN) {
            ast = parse_arg_list(ast, scope);
        } else if (t->type == TOK_LSQUARE) {
            ast = parse_array_index(ast, scope);
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' in expression.", to_string(t));
            return NULL;
        }
    }
}

Type *parse_fn_type(unsigned char ref, AstScope *scope) {
    if (ref) {
        error(lineno(), current_file_name(), "Cannot make a pointer to a function.");
    }

    TypeList *args = NULL;
    int nargs = 0;
    int variadic = 0;

    expect(TOK_LPAREN);
    Tok *t = next_token();

    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
    } else if (t->type != TOK_RPAREN) {
        for (;;) {
            args = typelist_append(args, parse_type(t, scope));

            nargs++;
            t = next_token();
            if (t == NULL) {
                error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
            } else if (t->type == TOK_RPAREN) {
                break;
            } else if (t->type == TOK_ELLIPSIS) {
                variadic = 1;
                t = next_token();
                if (t->type != TOK_RPAREN) {
                    error(lineno(), current_file_name(), "Only the last parameter to a function can be variadic.");
                }
                break;
            } else if (t->type == TOK_COMMA) {
                t = next_token();
            } else {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing type.", to_string(t));
            }
        }

        args = reverse_typelist(args);
    }
    t = next_token();

    Type *ret = base_type(VOID_T);
    if (t->type == TOK_COLON) {
        ret = parse_type(next_token(), scope);
    } else {
        unget_token(t);
    }

    Type *type = make_fn_type(nargs, args, ret, variadic);
    type = register_type(type);

    return type;
}

Type *parse_type(Tok *t, AstScope *scope) {
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
    }

    unsigned char ref = 0;
    if ((t->type == TOK_UOP && t->op == OP_REF) ||
            (t->type == TOK_OP && t->op == OP_BINAND)) {
        ref = 1;
        t = next_token();
    }

    if (t->type == TOK_ID) {
        Type *type = find_type_by_name(t->sval, scope, NULL);

        if (ref) {
            type = make_ptr_type(type);
            type = register_type(type);
        }
        return type;
    } else if (t->type == TOK_POLYTYPE) {
        Type *type = make_type(t->sval, AUTO_T, -1);
        /*type->polymorph = 2;*/
        /*type = define_type(type, scope);*/
        return type;
    } else if (t->type == TOK_LSQUARE) {
        Type *type = NULL;
        t = next_token();

        long length = -1;
        int slice = 1;

        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
        } else if (t->type != TOK_RSQUARE) {
            if (t->type == TOK_INT) {
                length = t->ival;
                slice = 0;
            } else if (t->type == TOK_OP && t->op == OP_MINUS) {
                slice = 0;
            } else {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing array type.", to_string(t));
            }

            expect(TOK_RSQUARE);
        }

        type = parse_type(next_token(), scope);

        if (slice) {
            type = make_array_type(type);
            type = register_type(type);
        } else {
            type = make_static_array_type(type, length);
            type = register_type(type);
        }

        if (ref) {
            type = make_ptr_type(type);
            type = register_type(type);
        }

        return type;
    } else if (t->type == TOK_STRUCT) {
        Type *type = parse_struct_type(scope);

        if (ref) {
            type = make_ptr_type(type);
            type = register_type(type);
        }

        return type;
    } else if (t->type == TOK_FN) {
        return parse_fn_type(ref, scope);
    } else {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing type.", to_string(t));
    }

    error(lineno(), current_file_name(), "Failed to parse type.");
    return NULL;
}

// TODO factor this with parse_func_decl
Ast *parse_extern_func_decl(AstScope *scope) {
    Tok *t = expect(TOK_FN);
    t = expect(TOK_ID);
    char *fname = t->sval;
    expect(TOK_LPAREN);

    Ast *func = ast_alloc(AST_EXTERN_FUNC_DECL); 

    int n = 0;
    int variadic = 0;
    TypeList* arg_types = NULL;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }

        Type *argtype = parse_type(t, scope);
        n++;

        t = next_token();
        /*if (t->type == TOK_ELLIPSIS) {*/
            /*variadic = 1;*/
            /*t = next_token();*/
            /*if (t->type != TOK_RPAREN) {*/
                /*error(lineno(), "Only the last parameter to a function can be variadic.");*/
            /*}*/
            /*arg_types = typelist_append(arg_types, argtype);*/
            /*break;*/
        /*}*/

        arg_types = typelist_append(arg_types, argtype);

        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    arg_types = reverse_typelist(arg_types);
    t = next_token();
    Type *ret = base_type(VOID_T);
    if (t->type == TOK_COLON) {
        ret = parse_type(next_token(), scope);
    } else {
        unget_token(t);
    }
    Type *fn_type = make_fn_type(n, arg_types, ret, variadic);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 1;
    func->fn_decl->var = fn_decl_var;
    return func; 
}

Ast *parse_func_decl(AstScope *scope, int anonymous) {
    Tok *t;
    char *fname;
    if (anonymous) {
        int len = snprintf(NULL, 0, "%d", last_tmp_fn_id);
        fname = malloc(sizeof(char) * (len + 1));
        snprintf(fname, len+1, "%d", last_tmp_fn_id++);
        fname[len] = 0;
    } else {
        t = expect(TOK_ID);
        fname = t->sval;
        if (find_local_var(fname, scope) != NULL) {
            error(lineno(), current_file_name(), "Declared function name '%s' already exists in this scope.", fname);
        }
    }
    expect(TOK_LPAREN);

    Ast *func = ast_alloc(anonymous ? AST_ANON_FUNC_DECL : AST_FUNC_DECL);
    func->fn_decl->args = NULL;

    AstScope *fn_scope = malloc(sizeof(AstScope));
    fn_scope->locals = NULL;
    fn_scope->parent = scope;
    fn_scope->has_return = 0;
    fn_scope->is_function = 1;
    fn_scope->unresolved_types = NULL;

    int n = 0;
    int variadic = 0;
    int polymorphic = 0;
    VarList *args = func->fn_decl->args;
    TypeList *arg_types = NULL;
    for (;;) {
        char use = 0;
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }
        if (t->type == TOK_USE) {
            use = 1;
            t = next_token();
        }
        if (t->type != TOK_ID) {
            error(lineno(), current_file_name(), "Unexpected token (type '%s') in argument list of function declaration '%s'.", token_type(t->type), fname);
        }
        if (find_local_var(t->sval, fn_scope) != NULL) {
            error(lineno(), current_file_name(), "Declared variable '%s' already exists.", t->sval);
        }
        expect(TOK_COLON);

        char *argname = t->sval;
        Type *argtype = parse_type(next_token(), scope);
        n++;

        t = next_token();
        if (t->type == TOK_ELLIPSIS) {
            variadic = 1;
            t = next_token();
            if (t->type != TOK_RPAREN) {
                error(lineno(), current_file_name(), "Only the last parameter to a function can be variadic.");
            }

            argtype = define_type(make_array_type(argtype), fn_scope); // should this be scope or fn_scope?
            args = varlist_append(args, make_var(argname, argtype));
            attach_var(args->item, fn_scope);
            args->item->initialized = 1;
            arg_types = typelist_append(arg_types, argtype->inner);
            break;
        }

        args = varlist_append(args, make_var(argname, argtype));
        attach_var(args->item, fn_scope);
        args->item->initialized = 1;
        arg_types = typelist_append(arg_types, args->item->type);

        if (use) {
            Type *orig = args->item->type;
            Type *t = orig;
            while (t->base == PTR_T) {
                t = t->inner;
            }
            if (t->base != STRUCT_T) {
                error(lineno(), current_file_name(), "'use' is not allowed on args of non-struct type '%s'.", orig->name);
            }
            for (int i = 0; i < t->st.nmembers; i++) {
                char *name = t->st.member_names[i];
                if (find_local_var(name, fn_scope) != NULL) {
                    error(lineno(), current_file_name(), "'use' statement on struct type '%s' conflicts with existing argument named '%s'.", orig->name, name);
                } else if (find_builtin_type(name) != NULL) {
                    error(lineno(), current_file_name(), "'use' statement on struct type '%s' conflicts with builtin type named '%s'.", orig->name, name);
                }
                Var *v = make_var(name, t->st.member_types[i]);
                if (orig->base == PTR_T) {
                    int proxy_name_len = strlen(args->item->name) + strlen(name) + 2;
                    char *proxy_name = malloc(sizeof(char) * (proxy_name_len + 1));
                    sprintf(proxy_name, "%s->%s", args->item->name, name);
                    proxy_name[proxy_name_len] = 0;
                    v->proxy = make_var(proxy_name, t->st.member_types[i]);
                } else {
                    v->proxy = args->item->members[i];
                }
                attach_var(v, fn_scope);
            }
        }

        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", to_string(t));
        }
    }
    func->fn_decl->args = reverse_varlist(args);
    arg_types = reverse_typelist(arg_types);
    t = next_token();
    Type *ret = base_type(VOID_T);
    if (t->type == TOK_COLON) {
        ret = parse_type(next_token(), fn_scope); // fn_scope or scope?
        /*if (ret->polymorph) {*/
            /*polymorphic = 1;*/
            /*ret = define_type(ret, fn_scope);*/
        /*}*/
    } else if (t->type == TOK_LBRACE) {
        unget_token(t);
    } else {
        error(lineno(), current_file_name(), "Unexpected token '%s' in function signature.", to_string(t));
    }
    Type *fn_type = make_fn_type(n, arg_types, ret, variadic);
    expect(TOK_LBRACE);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 0;
    fn_decl_var->constant = !anonymous;

    func->fn_decl->anon = anonymous;
    func->fn_decl->polymorphic = polymorphic;
    func->fn_decl->var = fn_decl_var;

    if (!anonymous) {
        attach_var(fn_decl_var, scope);
        attach_var(fn_decl_var, fn_scope);
    }

    fn_scope->body = parse_block(fn_scope, 1)->block;
    func->fn_decl->scope = fn_scope;

    global_fn_decls = astlist_append(global_fn_decls, func);

    return func; 
}

Ast *parse_return_statement(Tok *t, AstScope *scope) {
    Ast *ast = ast_alloc(AST_RETURN);
    ast->ret->expr = NULL;
    ast->ret->scope = scope;
    t = next_token();
    if (t == NULL) {
        error(lineno(), current_file_name(), "EOF encountered in return statement.");
    } else if (t->type == TOK_SEMI) {
        unget_token(t);
        return ast;
    }
    ast->ret->expr = parse_expression(t, 0, scope);
    return ast;
}

Ast *parse_type_decl(AstScope *scope) {
    Tok *t = expect(TOK_ID);
    expect(TOK_COLON);

    Ast *ast = ast_alloc(AST_TYPE_DECL);
    ast->type_decl->type_name = t->sval;

    Type *val = make_type(t->sval, AUTO_T, -1);
    int id = val->id;

    Type *tmp = parse_type(next_token(), scope);

    int l = strlen(t->sval);
    char *name = malloc((l + 1) * sizeof(char));
    strncpy(name, t->sval, l);
    name[l] = 0;

    if (tmp->named) {
        *val = *tmp;
        val->name = name;
        val->named = 1;
        val->id = id;
    } else {
        tmp->name = name;
        tmp->named = 1;
        *val = *tmp;
    }
    
    ast->type_decl->target_type = define_type(val, scope);
    return ast;
}

Ast *parse_enum_decl(AstScope *scope) {
    Ast *ast = ast_alloc(AST_ENUM_DECL);
    char *name = expect(TOK_ID)->sval;
    Tok *next = next_token();
    Type *inner = base_type(INT_T);
    if (next->type == TOK_COLON) {
        inner = parse_type(next_token(), scope);
        next = next_token();
    }
    if (next->type != TOK_LBRACE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing enum declaration (expected '{').", to_string(next));
    }
    int nmembers = 0;
    int alloc = 8;
    char **names = malloc(sizeof(char*)*alloc);
    Ast **exprs = malloc(sizeof(Ast)*alloc);
    while ((next = next_token())->type != TOK_RBRACE) {
        if (next->type != TOK_ID) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing enum declaration (expected an identifier).", to_string(next));
        }
        if (nmembers >= alloc) {
            alloc *= 2;
            names = realloc(names, sizeof(char*)*alloc);
            exprs = realloc(exprs, sizeof(Ast)*alloc);
        }
        int i = nmembers;
        names[i] = next->sval;
        nmembers++;
        next = next_token();
        if (next->type == TOK_OP && next->op == OP_ASSIGN) {
            exprs[i] = parse_expression(next_token(), 0, scope);
            Tok *n = next_token();
            if (n->type == TOK_RBRACE) {
                break;
            } else if (n->type != TOK_COMMA) {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing enum declaration (expected ',' or '}').", to_string(next));
            }
        } else if (next->type == TOK_RBRACE) {
            exprs[i] = NULL;
            break;
        } else if (next->type == TOK_COMMA) {
            exprs[i] = NULL;
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing enum declaration (expected ',' '=', or '}').", to_string(next));
        }
    }
    long *values = malloc(sizeof(long)*nmembers);
    for (TypeList *list = scope->local_types; list != NULL; list = list->next) {
        if (!strcmp(name, list->item->name)) {
            error(lineno(), current_file_name(), "Type named '%s' already exists in local scope.", name);
        }
    }
    ast->enum_decl->enum_name = name;
    ast->enum_decl->enum_type = define_type(make_enum_type(name, inner, nmembers, names, values), scope);
    ast->enum_decl->exprs = exprs;
    return ast;
}

Type *parse_struct_type(AstScope *scope) {
    expect(TOK_LBRACE);

    int alloc = 6;
    int nmembers = 0;

    char **member_names = malloc(sizeof(char*) * alloc);
    Type **member_types = malloc(sizeof(Type*) * alloc);

    for (;;) {
        if (nmembers >= alloc) {
            alloc += 6;
            member_names = realloc(member_names, sizeof(char*) * alloc);
            member_types = realloc(member_types, sizeof(char*) * alloc);
        }

        Tok *t = expect(TOK_ID);
        expect(TOK_COLON);

        Type *ty = parse_type(next_token(), scope);

        member_names[nmembers] = t->sval;
        member_types[nmembers++] = ty;

        expect(TOK_SEMI);
        t = next_token();

        if (t != NULL && t->type == TOK_RBRACE) {
            break;
        } else {
            unget_token(t);
        }
    }
    for (int i = 0; i < nmembers-1; i++) {
        for (int j = i + 1; j < nmembers; j++) {
            if (!strcmp(member_names[i], member_names[j])) {
                error(lineno(), current_file_name(), "Repeat member name '%s' in struct.", member_names[i]);
            }
        }
    }

    Type *st = make_struct_type(NULL, nmembers, member_names, member_types);
    st = register_type(st);
    return st;
}

Ast *parse_statement(Tok *t, AstScope *scope) {
    Ast *ast;
    if (t->type == TOK_ID && peek_token() != NULL && peek_token()->type == TOK_COLON) {
        next_token();
        ast = parse_declaration(t, scope);
    } else if (t->type == TOK_RELEASE) {
        ast = ast_alloc(AST_RELEASE);
        ast->release->object = parse_expression(next_token(), 0, scope);
    } else if (t->type == TOK_HOLD) {
        error(lineno(), current_file_name(), "Cannot start a statement with 'hold';");
    } else if (t->type == TOK_TYPE) {
        ast = parse_type_decl(scope);
    } else if (t->type == TOK_ENUM) {
        ast = parse_enum_decl(scope);
    } else if (t->type == TOK_USE) {
        Ast *ast_use = ast_alloc(AST_USE);
        ast_use->use->object = parse_expression(next_token(), 0, scope);
        ast = ast_use;
    } else if (t->type == TOK_DIRECTIVE) {
        if (!strcmp(t->sval, "import")) {
            t = next_token();
            if (t->type != TOK_STR || !expect_line_break_or_semicolon()) {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing import directive.", to_string(t));
            }
            Ast *ast = parse_source_file(t->sval, scope);
            pop_file_source();
            return ast;
        }
        // If not #import, default to normal behavior
        ast = parse_expression(t, 0, scope);
    } else if (t->type == TOK_FN) {
        Tok *next = next_token();
        unget_token(next);
        if (next->type == TOK_ID) {
            return parse_func_decl(scope, 0);
        }
        ast = parse_expression(t, 0, scope);
    } else if (t->type == TOK_EXTERN) {
        ast = parse_extern_func_decl(scope);
    } else if (t->type == TOK_LBRACE) {
        return parse_scope(NULL, scope);
    } else if (t->type == TOK_WHILE) {
        ast = ast_alloc(AST_WHILE);
        ast->while_loop->condition = parse_expression(next_token(), 0, scope);
        Tok *next = next_token();
        if (next == NULL || next->type != TOK_LBRACE) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing while loop.", to_string(next));
        }
        ast->while_loop->body = parse_scope(NULL, scope)->scope;
        return ast;
    } else if (t->type == TOK_FOR) {
        ast = ast_alloc(AST_FOR);
        Tok *id = expect(TOK_ID);
        ast->for_loop->itervar = make_var(id->sval, make_type("auto", AUTO_T, -1));
        Tok *next = next_token();
        /*if (next->type == TOK_COLON) {*/
            /*Tok *id = expect(TOK_ID);*/

        /*}*/
        if (next->type != TOK_IN) {
            error(ast->line, ast->file, "Unexpected token '%s' while parsing for loop.", to_string(next));
        }
        ast->for_loop->iterable = parse_expression(next_token(), 0, scope);
        next = next_token();
        // TODO handle empty for body case by trying rollback here
        if (next == NULL || next->type != TOK_LBRACE) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing for loop.", to_string(next));
        }
        ast->for_loop->body = parse_scope(NULL, scope)->scope;
        return ast;
    } else if (t->type == TOK_IF) {
        return parse_conditional(scope);
    } else if (t->type == TOK_RETURN) {
        ast = parse_return_statement(t, scope);
    } else if (t->type == TOK_BREAK) {
        ast = ast_alloc(AST_BREAK);
    } else if (t->type == TOK_CONTINUE) {
        ast = ast_alloc(AST_CONTINUE);
    } else {
        ast = parse_expression(t, 0, scope);
    }
    expect(TOK_SEMI);
    return ast;
}

Ast *parse_hold(AstScope *scope) {
    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected end of file while parsing hold expression.");
    }
    Ast *ast = ast_alloc(AST_HOLD);
    ast->hold->object = parse_expression(t, 0, scope);
    return ast;
}

Ast *parse_struct_literal(char *name, AstScope *scope) {
    UNWIND_SET;

    Tok *t = NEXT_TOKEN_UNWINDABLE;

    Ast *ast = ast_alloc(AST_LITERAL);

    ast->lit->lit_type = STRUCT;
    ast->lit->struct_val.name = name;
    ast->lit->struct_val.nmembers = 0;

    if (peek_token()->type == TOK_RBRACE) {
        t = NEXT_TOKEN_UNWINDABLE;
        return ast;
    }

    int alloc = 0;
    for (;;) {
        t = NEXT_TOKEN_UNWINDABLE;

        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_ID) {
            UNWIND_TOKENS;
            return NULL;
        }
        if (ast->lit->struct_val.nmembers >= alloc) {
            alloc += 4;
            ast->lit->struct_val.member_names = realloc(ast->lit->struct_val.member_names, sizeof(char *)*alloc);
            ast->lit->struct_val.member_exprs = realloc(ast->lit->struct_val.member_exprs, sizeof(Ast *)*alloc);
        }
        ast->lit->struct_val.member_names[ast->lit->struct_val.nmembers] = t->sval;
        t = NEXT_TOKEN_UNWINDABLE;
        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type != TOK_OP || t->op != OP_ASSIGN) {
            UNWIND_TOKENS;
            return NULL;
        }
        ast->lit->struct_val.member_exprs[ast->lit->struct_val.nmembers++] = parse_expression(NEXT_TOKEN_UNWINDABLE, 0, scope);
        t = NEXT_TOKEN_UNWINDABLE;
        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing struct literal.");
        } else if (t->type == TOK_RBRACE) {
            break;
        } else if (t->type != TOK_COMMA) {
            UNWIND_TOKENS;
            return NULL;
        }
    }
    return ast;
}

Ast *parse_directive(Tok *t, AstScope *scope) {
    Ast *dir = ast_alloc(AST_DIRECTIVE);
    dir->directive->name = t->sval;
    Tok *next = next_token();
    if (next == NULL) {
        error(lineno(), current_file_name(), "Unexpected end of input.");
    }
    if (!strcmp(t->sval, "type")) {
        dir->directive->object = NULL;
        dir->var_type = parse_type(next, scope);
        return dir;
    } else if (!strcmp(t->sval, "import")) {
        error(lineno(), current_file_name(), "#import directive used outside of statement.");
    }
    if (next->type != TOK_LPAREN) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing directive '%s'", to_string(next), t->sval);
    }
    dir->directive->object = parse_expression(next_token(), 0, scope);
    expect(TOK_RPAREN);
    return dir;
}

Ast *parse_primary(Tok *t, AstScope *scope) {
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing primary.");
    }
    switch (t->type) {
    case TOK_CHAR: {
        Ast *ast = ast_alloc(AST_LITERAL);
        ast->lit->lit_type = CHAR;
        ast->lit->int_val = t->ival;
        return ast;
    }
    case TOK_INT: {
        Ast *ast = ast_alloc(AST_LITERAL);
        ast->lit->lit_type = INTEGER;
        ast->lit->int_val = t->ival;
        return ast;
    }
    case TOK_FLOAT: {
        Ast *ast = ast_alloc(AST_LITERAL);
        ast->lit->lit_type = FLOAT;
        ast->lit->float_val = t->fval;
        return ast;
    }
    case TOK_BOOL:
        return make_ast_bool(t->ival);
    case TOK_STR:
        return make_ast_string(t->sval);
    case TOK_ID: {
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected end of input.");
        }
        unget_token(next);
        if (peek_token()->type == TOK_LBRACE) {
            Ast *ast = parse_struct_literal(t->sval, scope);
            if (ast != NULL) {
                return ast;
            }
        }
        Ast *id = ast_alloc(AST_IDENTIFIER);
        id->ident->var = NULL;
        id->ident->varname = t->sval;
        return id;
    }
    case TOK_DIRECTIVE: 
        return parse_directive(t, scope);
    case TOK_FN:
        return parse_func_decl(scope, 1);
    case TOK_HOLD:
        return parse_hold(scope);
    case TOK_OP: {
        if (!valid_unary_op(t->op)) {
            error(lineno(), current_file_name(), "'%s' is not a valid unary operator.", op_to_str(t->op));
        }
        Ast *ast = ast_alloc(AST_UOP);
        ast->unary->op = t->op;
        if (t->op == OP_BINAND) {
            ast->unary->op = OP_REF;
        } else if (t->op == OP_MUL) {
            ast->unary->op = OP_DEREF;
        }
        t = make_token(TOK_UOP);
        t->op = ast->unary->op;
        ast->unary->object = parse_expression(next_token(), priority_of(t), scope);
        return ast;
    }
    case TOK_UOP: {
        Ast *ast = ast_alloc(AST_UOP);
        ast->unary->op = t->op;
        ast->unary->object = parse_expression(next_token(), priority_of(t), scope);
        return ast;
    }
    case TOK_LPAREN: {
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected end of input.");
        }
        Ast *ast = parse_expression(next, 0, scope);
        next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected end of input.");
        }
        if (next->type != TOK_RPAREN) {
            error(lineno(), current_file_name(), "Unexpected token '%s' encountered while parsing parenthetical expression (starting line %d).", to_string(next), ast->line);
        }
        return ast;
    }
    case TOK_STARTBIND: {
        Ast *b = ast_alloc(AST_BIND);
        b->bind->expr = parse_expression(next_token(), 0, scope);
        expect(TOK_RBRACE);
        return b;
    }
    }
    error(lineno(), current_file_name(), "Unexpected token '%s' (primary).", to_string(t));
    return NULL;
}

Ast *parse_conditional(AstScope *scope) {
    Ast *c = ast_alloc(AST_CONDITIONAL);
    c->cond->condition = parse_expression(next_token(), 0, scope);
    Tok *next = next_token();
    if (next == NULL || next->type != TOK_LBRACE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing conditional.", to_string(next));
    }
    c->cond->if_body = parse_scope(NULL, scope)->scope;
    next = next_token();
    if (next != NULL && next->type == TOK_ELSE) {
        next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing conditional.");
        } else if (next->type == TOK_IF) {
            Ast *tmp = parse_conditional(scope);
            c->cond->else_body = new_scope(scope);
            c->cond->else_body->body = ast_alloc(AST_BLOCK)->block;
            c->cond->else_body->body->statements = astlist_append(c->cond->else_body->body->statements, tmp);
            return c;
        } else if (next->type != TOK_LBRACE) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing conditional.", to_string(next));
        }
        c->cond->else_body = parse_scope(NULL, scope)->scope;
    } else {
        c->cond->else_body = NULL;
        unget_token(next);
    }
    return c;
}

Ast *parse_block(AstScope *scope, int bracketed) {
    Ast *b = ast_alloc(AST_BLOCK);

    AstList *statements = NULL;
    Tok *t;

    for (;;) {
        t = next_token();
        if (t == NULL) {
            if (!bracketed) {
                break;
            } else {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing statement block.", to_string(t));
            }
        } else if (t->type == TOK_RBRACE) {
            if (bracketed) {
                break;
            } else {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing statement block.", to_string(t));
            }
        }
        Ast *stmt = parse_statement(t, scope);
        if (b->block->statements == NULL) {
            statements = astlist_append(NULL, stmt);
            b->block->statements = statements;
        } else {
            statements->next = astlist_append(NULL, stmt);
            statements = statements->next;
        }
    }
    b->block->endline = lineno();
    return b;
}

Ast *parse_scope(AstScope *scope, AstScope *parent) {
    Ast *s = ast_alloc(AST_SCOPE);
    s->scope = scope == NULL ? new_scope(parent) : scope;
    s->scope->body = parse_block(s->scope, parent == NULL ? 0 : 1)->block;
    return s;
}

Ast *parse_source_file(char *filename, AstScope *scope) {
    if (scope == NULL) {
        scope = new_scope(NULL);
    }
    set_file_source(filename, fopen(filename, "r"));
    return parse_block(scope, 0);
}

AstList *get_global_funcs() {
    return global_fn_decls;
}

void init_builtins() {
    Var *v = make_var("assert", make_fn_type(1, typelist_append(NULL, base_type(BOOL_T)), base_type(VOID_T), 0));
    v->ext = 1;
    v->constant = 1;
    define_builtin(v);

    v = make_var("print_str", make_fn_type(1, typelist_append(NULL, base_type(STRING_T)), base_type(VOID_T), 0));
    v->ext = 1;
    v->constant = 1;
    define_builtin(v);

    v = make_var("print_buf", make_fn_type(1, typelist_append(NULL, register_type(make_ptr_type(base_numeric_type(UINT_T, 8)))), base_type(VOID_T), 0));
    v->ext = 1;
    v->constant = 1;
    define_builtin(v);

    v = make_var("println", make_fn_type(1, typelist_append(NULL, base_type(STRING_T)), base_type(VOID_T), 0));
    v->ext = 1;
    v->constant = 1;
    define_builtin(v);

    v = make_var("itoa", make_fn_type(1, typelist_append(NULL, base_type(INT_T)), base_type(STRING_T), 0));
    v->ext = 1;
    v->constant = 1;
    define_builtin(v);

    v = make_var("validptr", make_fn_type(1, typelist_append(NULL, base_type(BASEPTR_T)), base_type(BOOL_T), 0));
    v->ext = 1;
    v->constant = 1;
    define_builtin(v);
}
