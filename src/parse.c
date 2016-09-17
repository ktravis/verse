#include "parse.h"

static int last_tmp_fn_id = 0;

static AstList *global_fn_decls = NULL;

Ast *parse_array_slice(Scope *scope, Ast *object, Ast *offset) {
    Ast *s = ast_alloc(AST_SLICE);
    s->slice->object = object;
    s->slice->offset = offset;

    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing array slice.");
    } else if (t->type == TOK_RSQUARE) {
        s->slice->length = NULL;
    } else {
        s->slice->length = parse_expression(scope, t, 0);
        expect(TOK_RSQUARE);
    }
    return s; 
}

Ast *parse_array_index(Scope *scope, Ast *object) {
    Tok *t = next_token();
    if (t->type == TOK_COLON) {
        return parse_array_slice(scope, object, NULL);
    }

    Ast *ind = ast_alloc(AST_INDEX);
    ind->index->object = object;
    ind->index->index = parse_expression(scope, t, 0);

    t = next_token();
    if (t->type == TOK_COLON) {
        Ast *offset = ind->index->index;
        free(ind);
        return parse_array_slice(scope, object, offset);
    } else if (t->type != TOK_RSQUARE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing array index.", tok_to_string(t));
    }
    return ind; 
}

Ast *parse_arg_list(Scope *scope, Ast *left) {
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

        func->call->args = astlist_append(func->call->args, parse_expression(scope, t, 0));
        n++;

        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", tok_to_string(t));
        }
    }

    func->call->args = reverse_astlist(func->call->args);
    func->call->nargs = n;
    return func; 
}

Ast *parse_declaration(Scope *scope, Tok *t) {
    char *name = t->sval;
    Type *type = NULL;

    Tok *next = next_token();
    if (!(next->type == TOK_OP && next->op == OP_ASSIGN)) {
        type = parse_type(scope, next, 0);
        next = next_token();
    }

    Ast *lhs = make_ast_decl(name, type);
    if (next == NULL) {
        error(lhs->line, lhs->file, "Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        lhs->decl->init = parse_expression(scope, next_token(), 0);
    } else if (next->type != TOK_SEMI) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing declaration.", tok_to_string(next));
    } else {
        unget_token(next);
    }
    return lhs; 
}

Ast *parse_expression(Scope *scope, Tok *t, int priority) {
    Ast *ast = parse_primary(scope, t);

    for (;;) {
        t = next_token();
        if (t == NULL) {
            return ast;
        } else if (t->type == TOK_SEMI || t->type == TOK_RPAREN ||
                 t->type == TOK_LBRACE || t->type == TOK_RBRACE ||
                 t->type == TOK_RSQUARE) {
            unget_token(t);
            return ast;
        }

        int next_priority = priority_of(t);
        if (next_priority < 0 || next_priority < priority) {
            unget_token(t);
            return ast;
        } else if (t->type == TOK_OPASSIGN) {
            Ast *rhs = parse_expression(scope, next_token(), 0);
            rhs = make_ast_binop(t->op, ast, rhs);
            ast = make_ast_assign(ast, rhs);
        } else if (t->type == TOK_OP) {
            if (t->op == OP_ASSIGN) {
                ast = make_ast_assign(ast, parse_expression(scope, next_token(), 0));
            } else if (t->op == OP_DOT) {
                Tok *next = next_token();
                if (next->type != TOK_ID) {
                    // TODO change this to something like "TOK_KEYWORD"
                    if (next->type == TOK_TYPE) {
                        next->type = TOK_ID;
                        next->sval = "type";
                    } else {
                        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing dot operation.", tok_to_string(next));
                    }
                }
                ast = make_ast_dot_op(ast, next->sval);
                ast->type = AST_DOT;
            } else if (t->op == OP_CAST) {
                Ast *c = ast_alloc(AST_CAST);
                c->cast->object = ast;
                c->cast->cast_type = parse_type(scope, next_token(), 0);
                ast = c;
            } else {
                ast = make_ast_binop(t->op, ast, parse_expression(scope, next_token(), next_priority + 1));
            }
        } else if (t->type == TOK_LPAREN) {
            ast = parse_arg_list(scope, ast);
        } else if (t->type == TOK_LSQUARE) {
            ast = parse_array_index(scope, ast);
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' in expression.", tok_to_string(t));
            return NULL;
        }
    }
}

Type *parse_fn_type(Scope *scope, int poly_ok) {
    TypeList *args = NULL;
    int nargs = 0;
    int variadic = 0;

    expect(TOK_LPAREN);
    Tok *t = next_token();

    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
    } else if (t->type != TOK_RPAREN) {
        for (;;) {
            args = typelist_append(args, parse_type(scope, t, poly_ok));

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
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing type.", tok_to_string(t));
            }
        }

        args = reverse_typelist(args);
    }

    Type *ret = base_type(VOID_T);

    t = next_token();
    if (t->type == TOK_COLON) {
        ret = parse_type(scope, next_token(), 0);
    } else {
        unget_token(t);
    }

    return make_fn_type(nargs, args, ret, variadic);
}

Type *parse_type(Scope *scope, Tok *t, int poly_ok) {
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
    }

    Type *type = NULL;
    unsigned char ref = 0;

    if ((t->type == TOK_UOP && t->op == OP_REF) ||
        (t->type == TOK_OP && t->op == OP_BINAND)) {
        ref = 1;
        t = next_token();
    }

    if (t->type == TOK_ID) {
        type = make_type(scope, t->sval);
    } else if (t->type == TOK_POLY) {
        if (!poly_ok) {
            error(lineno(), current_file_name(), "Polymorph type definitions are not allowed outside of function arguments.");
        }
        type = make_polydef(scope, t->sval);
    } else if (t->type == TOK_LSQUARE) {
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
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing array type.", tok_to_string(t));
            }

            expect(TOK_RSQUARE);
        }

        type = parse_type(scope, next_token(), poly_ok);

        if (slice) {
            type = make_array_type(type);
        } else {
            type = make_static_array_type(type, length);
        }
    } else if (t->type == TOK_STRUCT) {
        type = parse_struct_type(scope, poly_ok);
    } else if (t->type == TOK_FN) {
        if (ref) {
            error(lineno(), current_file_name(), "Cannot make a reference to a function.");
        }
        return parse_fn_type(scope, poly_ok);
    } else {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing type.", tok_to_string(t));
    }

    if (ref) {
        type = make_ref_type(type);
    }

    return type;
}

// TODO factor this with parse_func_decl
Ast *parse_extern_func_decl(Scope *scope) {
    Tok *t = expect(TOK_FN);

    t = expect(TOK_ID);
    char *fname = t->sval;
    if (lookup_local_var(scope, fname) != NULL) {
        error(lineno(), current_file_name(), "Declared extern function name '%s' already exists in this scope.", fname);
    }

    expect(TOK_LPAREN);

    Ast *func = ast_alloc(AST_EXTERN_FUNC_DECL); 

    int n = 0;
    TypeList* arg_types = NULL;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }

        Type *argtype = parse_type(scope, t, 0);
        arg_types = typelist_append(arg_types, argtype);

        n++;

        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", tok_to_string(t));
        }
    }
    arg_types = reverse_typelist(arg_types);
    Type *ret = base_type(VOID_T);

    t = next_token();
    if (t->type == TOK_COLON) {
        ret = parse_type(scope, next_token(), 0);
    } else {
        unget_token(t);
    }

    Type *fn_type = make_fn_type(n, arg_types, ret, 0);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 1;
    func->fn_decl->var = fn_decl_var;
    return func; 
}

Ast *parse_func_decl(Scope *scope, int anonymous) {
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
        // TODO: move this to semantics?
        if (lookup_local_var(scope, fname) != NULL) {
            error(lineno(), current_file_name(), "Declared function name '%s' already exists in this scope.", fname);
        }
    }
    expect(TOK_LPAREN);

    Ast *func = ast_alloc(anonymous ? AST_ANON_FUNC_DECL : AST_FUNC_DECL);
    func->fn_decl->args = NULL;

    Scope *fn_scope = new_fn_scope(scope);

    int n = 0;
    int variadic = 0;
    VarList *args = func->fn_decl->args;
    TypeList *arg_types = NULL;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }

        char use = 0;
        if (t->type == TOK_USE) {
            use = 1;
            t = next_token();
        }

        if (t->type != TOK_ID) {
            error(lineno(), current_file_name(),
                "Unexpected token (type '%s') in argument list of function declaration '%s'.",
                token_type(t->type), fname);
        }

        if (lookup_local_var(fn_scope, t->sval) != NULL) {
            error(lineno(), current_file_name(), 
                "Duplicate argument '%s' in function declaration.", t->sval);
        }

        expect(TOK_COLON);

        char *argname = t->sval;
        Type *argtype = parse_type(fn_scope, next_token(), 1);
        n++;

        t = next_token();
        if (t->type == TOK_ELLIPSIS) {
            variadic = 1;
            t = next_token();
            if (t->type != TOK_RPAREN) {
                error(lineno(), current_file_name(), "Only the last parameter to a function can be variadic.");
            }

            argtype = make_array_type(argtype);
            args = varlist_append(args, make_var(argname, argtype));
            attach_var(fn_scope, args->item);
            args->item->initialized = 1;
            arg_types = typelist_append(arg_types, argtype->inner);
            break;
        }

        args = varlist_append(args, make_var(argname, argtype));
        attach_var(fn_scope, args->item);
        args->item->initialized = 1;
        arg_types = typelist_append(arg_types, args->item->type);

        args->item->use = use;

        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", tok_to_string(t));
        }
    }

    func->fn_decl->args = reverse_varlist(args);
    arg_types = reverse_typelist(arg_types);

    Type *ret = base_type(VOID_T);

    t = next_token();
    if (t->type == TOK_COLON) {
        ret = parse_type(fn_scope, next_token(), 0);
    } else if (t->type == TOK_LBRACE) {
        unget_token(t);
    } else {
        error(lineno(), current_file_name(), "Unexpected token '%s' in function signature.", tok_to_string(t));
    }

    Type *fn_type = make_fn_type(n, arg_types, ret, variadic);
    expect(TOK_LBRACE);

    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->constant = !anonymous;
    fn_decl_var->fn_decl = func->fn_decl;

    func->fn_decl->anon = anonymous;
    func->fn_decl->var = fn_decl_var;

    if (!anonymous) {
        attach_var(scope, fn_decl_var);
        attach_var(fn_scope, fn_decl_var);
    }

    fn_scope->fn_var = fn_decl_var;
    func->fn_decl->body = parse_astblock(fn_scope, 1);
    func->fn_decl->scope = fn_scope;

    global_fn_decls = astlist_append(global_fn_decls, func);

    return func; 
}

Ast *parse_return_statement(Scope *scope, Tok *t) {
    Ast *ast = ast_alloc(AST_RETURN);
    ast->ret->expr = NULL;

    t = next_token();
    if (t == NULL) {
        error(lineno(), current_file_name(), "EOF encountered in return statement.");
    } else if (t->type == TOK_SEMI) {
        unget_token(t);
        return ast;
    }
    ast->ret->expr = parse_expression(scope, t, 0);
    return ast;
}

Ast *parse_type_decl(Scope *scope) {
    Tok *t = expect(TOK_ID);
    expect(TOK_COLON);

    Ast *ast = ast_alloc(AST_TYPE_DECL);
    ast->type_decl->type_name = t->sval;

    Type *type = parse_type(scope, next_token(), 0);
    ast->type_decl->target_type = define_type(scope, t->sval, type);
    register_type(scope, type);
    return ast;
}

Ast *parse_enum_decl(Scope *scope) {
    Ast *ast = ast_alloc(AST_ENUM_DECL);
    char *name = expect(TOK_ID)->sval;
    if (local_type_name_conflict(scope, name)) {
        error(lineno(), current_file_name(),
            "Type named '%s' already exists in local scope.", name);
    }

    Type *inner = base_type(INT_T);

    Tok *next = next_token();
    if (next->type == TOK_COLON) {
        inner = parse_type(scope, next_token(), 0);
        next = next_token();
    }
    if (next->type != TOK_LBRACE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing enum declaration (expected '{').", tok_to_string(next));
    }

    int nmembers = 0;
    int alloc = 8;
    char **names = malloc(sizeof(char*)*alloc);
    Ast **exprs = malloc(sizeof(Ast)*alloc);

    while ((next = next_token())->type != TOK_RBRACE) {
        if (next->type != TOK_ID) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing enum declaration (expected an identifier).", tok_to_string(next));
        }

        if (nmembers >= alloc) {
            alloc *= 2;
            names = realloc(names, sizeof(char*)*alloc);
            exprs = realloc(exprs, sizeof(Ast)*alloc);
        }

        int i = nmembers;
        names[i] = next->sval;
        exprs[i] = NULL;
        nmembers++;

        next = next_token();
        if (next->type == TOK_OP && next->op == OP_ASSIGN) {
            exprs[i] = parse_expression(scope, next_token(), 0);
            next = next_token();
        }

        if (next->type == TOK_RBRACE) {
            break;
        } else if (next->type != TOK_COMMA) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing enum declaration (expected ',' '=', or '}').",
                tok_to_string(next));
        }
    }

    long *values = malloc(sizeof(long)*nmembers);
    ast->enum_decl->enum_name = name;
    ast->enum_decl->enum_type = define_type(scope, name, make_enum_type(inner, nmembers, names, values));
    ast->enum_decl->exprs = exprs;
    return ast;
}

Type *parse_struct_type(Scope *scope, int poly_ok) {
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

        Type *ty = parse_type(scope, next_token(), poly_ok);

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
                error(lineno(), current_file_name(),
                    "Repeat member name '%s' in struct.", member_names[i]);
            }
        }
    }

    Type *t = make_struct_type(nmembers, member_names, member_types);
    register_type(scope, t);

    return t;
}

Ast *parse_statement(Scope *scope, Tok *t) {
    Ast *ast = NULL;

    switch (t->type) {
    case TOK_ID:
        if (peek_token() != NULL && peek_token()->type == TOK_COLON) {
            next_token();
            ast = parse_declaration(scope, t);
        } else {
            ast = parse_expression(scope, t, 0);
        }
        break;
    case TOK_TYPE:
        ast = parse_type_decl(scope);
        break;
    case TOK_ENUM:
        ast = parse_enum_decl(scope);
        break;
    case TOK_USE:
        ast = ast_alloc(AST_USE);
        ast->use->object = parse_expression(scope, next_token(), 0);
        break;
    case TOK_DIRECTIVE:
        if (!strcmp(t->sval, "import")) {
            t = next_token();
            if (t->type != TOK_STR || !expect_line_break_or_semicolon()) {
                error(lineno(), current_file_name(),
                    "Unexpected token '%s' while parsing import directive.",
                    tok_to_string(t));
            }
            Ast *ast = parse_source_file(scope, t->sval);
            pop_file_source();
            return ast;
        }
        // If not #import, default to normal behavior
        ast = parse_expression(scope, t, 0);
        break;
    case TOK_FN:
        if (peek_token()->type == TOK_ID) {
            return parse_func_decl(scope, 0);
        }
        ast = parse_expression(scope, t, 0);
        break;
    case TOK_EXTERN:
        ast = parse_extern_func_decl(scope);
        break;
    case TOK_LBRACE:
        unget_token(t);
        return parse_block(new_scope(scope), 1);
    case TOK_WHILE:
        ast = ast_alloc(AST_WHILE);
        ast->while_loop->condition = parse_expression(scope, next_token(), 0);
        t = next_token();
        if (t == NULL || t->type != TOK_LBRACE) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing while loop.",
                tok_to_string(peek_token()));
        }
        ast->while_loop->scope = new_loop_scope(scope);
        ast->while_loop->body = parse_astblock(ast->while_loop->scope, 1);
        return ast;
    case TOK_FOR:
        ast = ast_alloc(AST_FOR);
        t = expect(TOK_ID);
        ast->for_loop->itervar = make_var(t->sval, NULL);
        
        t = next_token();
        if (t->type != TOK_IN) {
            error(ast->line, ast->file,
                "Unexpected token '%s' while parsing for loop.", tok_to_string(t));
        }

        t = next_token();
        ast->for_loop->iterable = parse_expression(scope, t, 0);

        // TODO handle empty for body case by trying rollback here
        t = next_token();
        if (t == NULL || t->type != TOK_LBRACE) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing for loop.", tok_to_string(t));
        }
        ast->for_loop->scope = new_loop_scope(scope);
        ast->for_loop->body = parse_astblock(ast->for_loop->scope, 1);
        return ast;
    case TOK_IF:
        return parse_conditional(scope);
    case TOK_RETURN:
        ast = parse_return_statement(scope, t);
        break;
    case TOK_BREAK:
        ast = ast_alloc(AST_BREAK);
        break;
    case TOK_CONTINUE:
        ast = ast_alloc(AST_CONTINUE);
        break;
    default:
        ast = parse_expression(scope, t, 0);
    }
    expect(TOK_SEMI);
    return ast;
}

// TODO: make this take a type instead
Ast *parse_struct_literal(Scope *scope, char *name) {
    UNWIND_SET;

    Tok *t = NEXT_TOKEN_UNWINDABLE;

    Ast *ast = ast_alloc(AST_LITERAL);

    ast->lit->lit_type = STRUCT_LIT;
    ast->lit->struct_val.name = name;
    ast->lit->struct_val.nmembers = 0;
    ast->lit->struct_val.type = make_type(scope, name);

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

        ast->lit->struct_val.member_exprs[ast->lit->struct_val.nmembers++] = parse_expression(scope, NEXT_TOKEN_UNWINDABLE, 0);

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

Ast *parse_directive(Scope *scope, Tok *t) {
    Ast *dir = ast_alloc(AST_DIRECTIVE);

    dir->directive->name = t->sval;

    Tok *next = next_token();
    if (next == NULL) {
        error(lineno(), current_file_name(), "Unexpected end of input.");
    }

    if (!strcmp(t->sval, "type")) {
        dir->directive->object = NULL;
        dir->var_type = parse_type(scope, next, 0);
        return dir;
    } else if (!strcmp(t->sval, "import")) {
        error(lineno(), current_file_name(), "#import directive must be a statement.");
    }

    if (next->type != TOK_LPAREN) {
        error(lineno(), current_file_name(),
            "Unexpected token '%s' while parsing directive '%s'",
            tok_to_string(next), t->sval);
    }

    dir->directive->object = parse_expression(scope, next_token(), 0);
    expect(TOK_RPAREN);
    return dir;
}

Ast *parse_primary(Scope *scope, Tok *t) {
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
            Ast *ast = parse_struct_literal(scope, t->sval);
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
        return parse_directive(scope, t);
    case TOK_FN:
        return parse_func_decl(scope, 1);
    case TOK_OP: {
        if (!valid_unary_op(t->op)) {
            error(lineno(), current_file_name(), "'%s' is not a valid unary operator.",
                op_to_str(t->op));
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
        ast->unary->object = parse_expression(scope, next_token(), priority_of(t));
        return ast;
    }
    case TOK_UOP: {
        Ast *ast = ast_alloc(AST_UOP);
        ast->unary->op = t->op;
        ast->unary->object = parse_expression(scope, next_token(), priority_of(t));
        return ast;
    }
    case TOK_LPAREN: {
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected end of input.");
        }
        Ast *ast = parse_expression(scope, next, 0);

        next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected end of input.");
        }
        if (next->type != TOK_RPAREN) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' encountered while parsing parenthetical expression (starting line %d).",
                tok_to_string(next), ast->line);
        }
        return ast;
    }
    }
    error(lineno(), current_file_name(),
        "Unexpected token '%s' while parsing primary expression.", tok_to_string(t));
    return NULL;
}

Ast *parse_conditional(Scope *scope) {
    Ast *c = ast_alloc(AST_CONDITIONAL);

    c->cond->condition = parse_expression(scope, next_token(), 0);

    Tok *next = next_token();
    if (next == NULL || next->type != TOK_LBRACE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing conditional.", tok_to_string(next));
    }

    c->cond->scope = new_scope(scope);
    c->cond->if_body = parse_astblock(c->cond->scope, 1);

    next = next_token();
    if (next != NULL && next->type == TOK_ELSE) {
        next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing conditional.");
        } else if (next->type == TOK_IF) {
            AstBlock *block = calloc(sizeof(AstBlock), 1);

            block->file = current_file_name();
            block->startline = lineno();
            Ast *tmp = parse_conditional(scope);
            block->endline = lineno();
            block->statements = astlist_append(block->statements, tmp);
            c->cond->else_body = block;
            return c;
        } else if (next->type != TOK_LBRACE) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing conditional.", tok_to_string(next));
        }
        c->cond->else_body = parse_astblock(c->cond->scope, 1);
    } else {
        c->cond->else_body = NULL;
        unget_token(next);
    }
    return c;
}

AstList *parse_statement_list(Scope *scope) {
    AstList *list = NULL;
    AstList *head = NULL;
    Tok *t;

    for (;;) {
        t = next_token();
        if (t == NULL || t->type == TOK_RBRACE) {
            break;
        }
        Ast *stmt = parse_statement(scope, t);
        if (head == NULL) {
            list = astlist_append(NULL, stmt);
            head = list;
        } else {
            list->next = astlist_append(NULL, stmt);
            list = list->next;
        }
    }
    unget_token(t);
    return head;
}

AstBlock *parse_astblock(Scope *scope, int bracketed) {
    AstBlock *block = calloc(sizeof(AstBlock), 1);

    block->startline = lineno();
    block->file = current_file_name();
    block->statements = parse_statement_list(scope);

    Tok *t = next_token();
    if (t == NULL) {
        if (bracketed) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing statement block.", tok_to_string(t));
        }
    } else if (t->type == TOK_RBRACE) {
        if (!bracketed) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing statement block.", tok_to_string(t));
        }
    }

    block->endline = lineno();
    return block;
}

Ast *parse_block(Scope *scope, int bracketed) {
    Ast *b = ast_alloc(AST_BLOCK);
    b->block = parse_astblock(scope, bracketed);
    return b;
}

Ast *parse_source_file(Scope *scope, char *filename) {
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

    v = make_var("print_buf", make_fn_type(1, typelist_append(NULL, make_ref_type(base_numeric_type(UINT_T, 8))), base_type(VOID_T), 0));
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
