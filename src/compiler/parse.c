#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array/array.h"
#include "eval.h"
#include "parse.h"
#include "semantics.h"
#include "var.h"
#include "util.h"

static int last_tmp_fn_id = 0;

Type *can_be_type_object(Ast *ast) {
    switch (ast->type) {
    // AST_TYPEINFO?
    case AST_IDENTIFIER:
        return make_type(NULL, ast->ident->varname);
    case AST_DOT: {
        Type *lt = can_be_type_object(ast->dot->object);
        if (lt == NULL) {
            return NULL;
        }
        // may need to allow this later, idk
        if (!lt->name) {
            return NULL;
        }
        return make_external_type(lt->name, ast->dot->member_name);
    }
    case AST_CALL: {
        Type *lt = can_be_type_object(ast->dot->object);
        if (lt == NULL) {
            return NULL;
        }
        Type **params = NULL;
        for (int i = 0; i < array_len(ast->call->args); i++) {
            Type *t = can_be_type_object(ast->call->args[i]);
            if (!t) {
                return NULL;
            }
            array_push(params, t);
        }
        return make_params_type(lt, params);
    }
    case AST_TYPE_OBJ:
        return ast->type_obj->t;
    default:
        break;
    }
    return NULL;
}

Ast *parse_array_slice(Ast *object, Ast *offset) {
    Ast *s = ast_alloc(AST_SLICE);
    s->slice->object = object;
    s->slice->offset = offset;

    Tok *t = next_token();
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing array slice.");
    } else if (t->type == TOK_RSQUARE) {
        s->slice->length = NULL;
    } else {
        s->slice->length = parse_expression(t, 0);
        expect(TOK_RSQUARE);
    }
    return s; 
}

Ast *parse_array_index(Ast *object) {
    Tok *t = next_token();
    if (t->type == TOK_COLON) {
        return parse_array_slice(object, NULL);
    }

    Ast *ind = ast_alloc(AST_INDEX);
    ind->index->object = object;
    ind->index->index = parse_expression(t, 0);

    t = next_token();
    if (t->type == TOK_COLON) {
        Ast *offset = ind->index->index;
        free(ind);
        return parse_array_slice(object, offset);
    } else if (t->type != TOK_RSQUARE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing array index.", tok_to_string(t));
    }
    return ind; 
}

Ast *parse_arg_list(Ast *left) {
    Ast *func = ast_alloc(AST_CALL);
    func->call->args = NULL;
    func->call->fn = left;

    Tok *t;
    for (;;) {
        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        }

        array_push(func->call->args, parse_expression(t, 0));

        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", tok_to_string(t));
        }
    }
    return func; 
}

Ast *parse_declaration(Tok *t) {
    char *name = t->sval;
    Type *type = NULL;

    Tok *next = next_token();
    if (!(next->type == TOK_OP && next->op == OP_ASSIGN)) {
        type = parse_type(next, 0);
        next = next_token();
    }

    Ast *lhs = make_ast_decl(name, type);
    if (next == NULL) {
        error(lhs->line, lhs->file, "Unexpected end of input while parsing declaration.");
    } else if (next->type == TOK_OP && next->op == OP_ASSIGN) {
        lhs->decl->init = parse_expression(next_token(), 0);
    } else if (next->type != TOK_SEMI) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing declaration.", tok_to_string(next));
    } else {
        unget_token(next);
    }
    return lhs; 
}

Ast *parse_compound_literal(Type *type) {
    Ast *ast = ast_alloc(AST_LITERAL);

    ast->lit->lit_type = COMPOUND_LIT;
    ast->lit->compound_val.type = type;

    int named = 1;

    if (peek_token()->type == TOK_RBRACE) {
        next_token();
        return ast;
    }

    char **member_names = NULL;
    Ast **member_exprs = NULL;
    Tok *t = NULL;
    for (;;) {
        t = next_token();
        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing compound literal.");
        } else if (t->type == TOK_RBRACE) {
            break;
        }

        char *field_name = NULL;
        if (t->type == TOK_ID) {
            // if the first item is an id, check to see if the next is a colon
            Tok *next = next_token();
            if (next->type == TOK_OP && next->op == OP_ASSIGN) {
                // if we have seen members without names previously, this is
                // invalid
                if (!named) {
                    error(lineno(), current_file_name(), "Cannot mix named and non-named fields in a struct-literal.");
                }
                field_name = t->sval;
                t = next_token();
            } else {
                // undo our peek otherwise
                unget_token(next);

                // if we have seen members with names, this is invalid
                if (named && member_exprs) {
                    error(lineno(), current_file_name(), "Unexpected token '%s' while parsing compound literal (cannot mix named and non-named fields in a struct-literal).", tok_to_string(t));
                }
                named = 0;
            }
        } else {
            if (named && member_exprs) {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing compound literal (cannot mix named and non-named fields in a struct-literal).", tok_to_string(t));
            }
            named = 0;
        }

        array_push(member_names, field_name);
        array_push(member_exprs, parse_expression(t, 0));

        t = next_token();
        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing compound literal.");
        } else if (t->type == TOK_RBRACE) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing compound literal.", tok_to_string(t));
        }
    }

    if (named) {
        ast->lit->lit_type = STRUCT_LIT;
    }
    ast->lit->compound_val.named = (named && member_names != NULL);
    ast->lit->compound_val.member_names = member_names;
    ast->lit->compound_val.member_exprs = member_exprs;

    return ast;
}

Ast *parse_expression(Tok *t, int priority) {
    Ast *ast = parse_primary(t);

    for (;;) {
        t = next_token();
        if (t == NULL) {
            return ast;
        } else if (t->type == TOK_ELLIPSIS) {
            Ast *s = ast_alloc(AST_SPREAD);
            s->spread->object = ast;
            return s;
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
            // TODO: this shouldn't be allowed in expression, why is this here?!
            Ast *rhs = parse_expression(next_token(), 0);
            rhs = make_ast_binop(t->op, ast, rhs);
            ast = make_ast_assign(ast, rhs);
        } else if (t->type == TOK_DCOLON) {
            Type *lt = can_be_type_object(ast);
            if (lt == NULL) {
                error(lineno(), current_file_name(), "Unexpected token '::' (previous expression is not a type).");
            }

            t = next_token();
            if (t->type != TOK_LBRACE) {
                error(lineno(), current_file_name(), "Unexpected token '%s' following '::'.", tok_to_string(t));
            }

            return parse_compound_literal(lt);
        } else if (t->type == TOK_OP) {
            if (t->op == OP_ASSIGN) {
                ast = make_ast_assign(ast, parse_expression(next_token(), 0));
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
            } else if (t->op == OP_CAST) {
                Ast *c = ast_alloc(AST_CAST);
                c->cast->object = ast;
                c->cast->cast_type = parse_type(next_token(), 0);
                ast = c;
            } else {
                ast = make_ast_binop(t->op, ast, parse_expression(next_token(), next_priority + 1));
            }
        } else if (t->type == TOK_LPAREN) {
            ast = parse_arg_list(ast);
        } else if (t->type == TOK_LSQUARE) {
            ast = parse_array_index(ast);
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' in expression.", tok_to_string(t));
            return NULL;
        }
    }
}

Type *parse_fn_type(int poly_ok) {
    Type **args = NULL;
    int variadic = 0;

    expect(TOK_LPAREN);
    Tok *t = next_token();

    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
    } else if (t->type != TOK_RPAREN) {
        for (;;) {
            array_push(args, parse_type(t, poly_ok));
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
    }

    Type *ret = base_type(VOID_T);

    t = next_token();
    if (t->type == TOK_ARROW) {
        ret = parse_type(next_token(), 0);
    } else {
        unget_token(t);
    }

    return make_fn_type(args, ret, variadic);
}

Type **parse_type_params(int in_fn) {
    Tok *t = next_token();

    Type **list = NULL;

    for (;;) {
        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing type parameter list.");
        }
        if (t->type == TOK_RPAREN) {
            break;
        } else {
            array_push(list, parse_type(t, in_fn));
        }

        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type == TOK_COMMA) {
            t = next_token();
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' in type parameter list.", tok_to_string(t));
        }
    }

    if (list == NULL) {
        error(lineno(), current_file_name(), "Empty type parameter list is invalid.");
    }

    return list;
}

Type **parse_type_param_defs() {
    Tok *t = next_token();

    Type **list = NULL;

    for (;;) {
        if (t == NULL) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing type parameter list.");
        }
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type == TOK_ID) {
            for (int i = 1; i < array_len(list); i++) {
                if (!strcmp(list[i]->name, t->sval)) {
                    error(lineno(), current_file_name(), "Repeated name '%s' in type parameter list.", t->sval);
                }
            }
            array_push(list, make_type(NULL, t->sval));
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing type parameter list.", tok_to_string(t));
        }


        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type == TOK_COMMA) {
            t = next_token();
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' in type parameter list.", tok_to_string(t));
        }
    }

    if (list == NULL) {
        error(lineno(), current_file_name(), "Empty type parameter list is invalid.");
    }

    return list;
}

Type *parse_type(Tok *t, int poly_ok) {
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected EOF while parsing type.");
    }

    Type *type = NULL;
    unsigned char ref = 0;
    unsigned char owned = 0;

    if ((t->type == TOK_UOP && t->op == OP_REF) ||
        (t->type == TOK_OP && t->op == OP_BINAND)) {
        ref = 1;
        t = next_token();
    } else if (t->type == TOK_SQUOTE) {
        if (poly_ok) {
            error(lineno(), current_file_name(), "Onwed reference type not allowed in function arguments.", tok_to_string(t));
        }
        ref = 1;
        owned = 1;
        t = next_token();
    }

    // is single dot sufficient?
    if (t->type == TOK_ID) {
        char *n = t->sval;
        t = next_token();
        if (t->type == TOK_OP && t->op == OP_DOT) {
            t = next_token();
            if (t->type != TOK_ID) {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing package type.", tok_to_string(t));
            }

            type = make_external_type(n, t->sval);
        } else {
            unget_token(t);
            type = make_type(NULL, n);
        }

        t = next_token();
        if (t->type == TOK_LPAREN) {
            type = make_params_type(type, parse_type_params(poly_ok));
        } else {
            unget_token(t);
        }

    } else if (t->type == TOK_POLY) {
        if (!poly_ok) {
            error(lineno(), current_file_name(), "Polymorph type definitions are not allowed outside of function arguments.");
        }
        type = make_polydef(NULL, t->sval);
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

        type = parse_type(next_token(), poly_ok);

        if (slice) {
            type = make_array_type(type);
        } else {
            type = make_static_array_type(type, length);
        }
    } else if (t->type == TOK_STRUCT) {
        type = parse_struct_type(poly_ok);
    } else if (t->type == TOK_FN) {
        if (ref) {
            error(lineno(), current_file_name(), "Cannot make a reference to a function.");
        }
        return parse_fn_type(poly_ok);
    } else {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing type.", tok_to_string(t));
    }

    if (ref) {
        if (owned && type->resolved && type->resolved->comp == ARRAY) {
            type->resolved->array.owned = 1;
        } else {
            type = make_ref_type(type);
            type->resolved->ref.owned = owned;
        }
    }

    return type;
}

// TODO: factor this with parse_func_decl
// TODO: allow using names in arg declaration here (optional? not?)
Ast *parse_extern_func_decl() {
    Tok *t = expect(TOK_FN);
    t = expect(TOK_ID);
    char *fname = t->sval;

    expect(TOK_LPAREN);

    Ast *func = ast_alloc(AST_EXTERN_FUNC_DECL); 

    int n = 0;
    Type **arg_types = NULL;
    for (;;) {
        t = next_token();
        if (t->type == TOK_DIRECTIVE) {
            if (!strcmp(t->sval, "autocast")) {
                func->fn_decl->ext_autocast |= 1 << n;
                t = next_token();
                if (t->type == TOK_RPAREN) {
                    error(lineno(), current_file_name(), "Directive #autocast in extern function declaration without argument type.");
                }
            } else {
                error(lineno(), current_file_name(), "Unexpected directive '#%s' in argument list.", t->sval);
            }
        }
        if (t->type == TOK_RPAREN) {
            break;
        }

        Type *argtype = parse_type(t, 0);
        array_push(arg_types, argtype);

        n++;

        t = next_token();
        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", tok_to_string(t));
        }
    }
    Type *ret = base_type(VOID_T);

    t = next_token();
    if (t->type == TOK_ARROW) {
        ret = parse_type(next_token(), 0);
    } else {
        unget_token(t);
    }

    Type *fn_type = make_fn_type(arg_types, ret, 0);
    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->ext = 1;
    fn_decl_var->constant = 1;
    fn_decl_var->fn_decl = func->fn_decl;
    func->fn_decl->var = fn_decl_var;
    return func; 
}

Ast *parse_func_decl(int anonymous) {
    Tok *t;
    char *fname = NULL;
    if (anonymous) {
        int len = snprintf(NULL, 0, "%d", last_tmp_fn_id);
        fname = malloc(sizeof(char) * (len + 1));
        snprintf(fname, len+1, "%d", last_tmp_fn_id++);
        fname[len] = 0;
    } else {
        t = expect(TOK_ID);
        fname = t->sval;
    }
    expect(TOK_LPAREN);

    Ast *func = ast_alloc(anonymous ? AST_ANON_FUNC_DECL : AST_FUNC_DECL);
    func->fn_decl->args = NULL;

    int variadic = 0;
    Var **args = func->fn_decl->args;
    Type **arg_types = NULL;
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
            if (t->type == TOK_DIRECTIVE) {
                if (!strcmp(t->sval, "autocast")) {
                    error(lineno(), current_file_name(),
                          "Directive '#autocast' not allowed outside extern function declaration.");
                } else {
                    error(lineno(), current_file_name(),
                          "Unrecognized directive '#%s' in function declaration.", t->sval);
                }
            }
            error(lineno(), current_file_name(),
                  "Unexpected token (type '%s') in argument list of function declaration '%s'.",
                  token_type(t->type), fname);
        }

        for (int i = 0; i < array_len(args); i++) {
            if (!strcmp(args[i]->name, t->sval)) {
                error(lineno(), current_file_name(), 
                      "Duplicate argument '%s' in function declaration.", t->sval);
            }
        }

        expect(TOK_COLON);

        char *argname = t->sval;
        Type *argtype = parse_type(next_token(), 1);

        t = next_token();
        if (t->type == TOK_ELLIPSIS) {
            variadic = 1;
            t = next_token();
            if (t->type != TOK_RPAREN) {
                error(lineno(), current_file_name(), "Only the last parameter to a function can be variadic.");
            }

            argtype = make_array_type(argtype);
            Var *v = make_var(argname, argtype);
            v->initialized = 1;
            array_push(args, v);
            array_push(arg_types, argtype->resolved->array.inner);
            break;
        }

        Var *v = make_var(argname, argtype);
        v->use = use;
        v->initialized = 1;
        array_push(args, v);
        array_push(arg_types, v->type);

        if (t->type == TOK_RPAREN) {
            break;
        } else if (t->type != TOK_COMMA) {
            error(lineno(), current_file_name(), "Unexpected token '%s' in argument list.", tok_to_string(t));
        }
    }

    func->fn_decl->args = args;

    Type *ret = base_type(VOID_T);

    t = next_token();
    if (t->type == TOK_ARROW) {
        ret = parse_type(next_token(), 0);
    } else if (t->type == TOK_LBRACE) {
        unget_token(t);
    } else {
        error(lineno(), current_file_name(), "Unexpected token '%s' in function signature.", tok_to_string(t));
    }

    Type *fn_type = make_fn_type(arg_types, ret, variadic);
    expect(TOK_LBRACE);

    Var *fn_decl_var = make_var(fname, fn_type);
    fn_decl_var->constant = !anonymous;
    fn_decl_var->fn_decl = func->fn_decl;

    func->fn_decl->anon = anonymous;
    func->fn_decl->var = fn_decl_var;

    func->fn_decl->body = parse_astblock(1);
    /*if (!is_polydef(fn_type)) {*/
        /*array_push(global_fn_decls, func);*/
    /*}*/
    return func; 
}

Ast *parse_return_statement(Tok *t) {
    Ast *ast = ast_alloc(AST_RETURN);
    ast->ret->expr = NULL;

    t = next_token();
    if (t == NULL) {
        error(lineno(), current_file_name(), "EOF encountered in return statement.");
    } else if (t->type == TOK_SEMI) {
        unget_token(t);
        return ast;
    }
    ast->ret->expr = parse_expression(t, 0);
    return ast;
}

Ast *parse_type_decl() {
    Tok *t = expect(TOK_ID);
    expect(TOK_COLON);

    Ast *ast = ast_alloc(AST_TYPE_DECL);
    ast->type_decl->type_name = t->sval;
    ast->type_decl->target_type = parse_type(next_token(), 0);
    return ast;
}

Ast *parse_enum_decl() {
    Ast *ast = ast_alloc(AST_ENUM_DECL);
    char *name = expect(TOK_ID)->sval;

    Type *inner = base_type(INT_T);

    Tok *next = next_token();
    if (next->type == TOK_COLON) {
        inner = parse_type(next_token(), 0);
        next = next_token();
    }
    if (next->type != TOK_LBRACE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing enum declaration (expected '{').", tok_to_string(next));
    }

    ast->enum_decl->enum_name = name;
    ast->enum_decl->enum_type = make_enum_type(inner, NULL, NULL);

    while ((next = next_token())->type != TOK_RBRACE) {
        if (next->type != TOK_ID) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing enum declaration (expected an identifier).", tok_to_string(next));
        }

        Ast *expr = NULL;
        char *member_name = next->sval;

        int old_line = next->line;

        next = next_token_or_newline();
        if (next->type == TOK_OP && next->op == OP_ASSIGN) {
            expr = parse_expression(next_token(), 0);
            next = next_token_or_newline();
        }
        array_push(ast->enum_decl->enum_type->resolved->en.member_names, member_name);
        array_push(ast->enum_decl->exprs, expr);

        if (next->type == TOK_RBRACE) {
            break;
        } else if (next->line == old_line) {
            if (next->type != TOK_COMMA && next->type != TOK_NL) {
                error(lineno(), current_file_name(),
                    "Unexpected token '%s' while parsing enum declaration (expected ',' '=', or '}').",
                    tok_to_string(next));
            }
        } else {
            unget_token(next);
        }
    }

    return ast;
}

Type *parse_struct_type(int poly_ok) {
    Tok *t = next_token();

    int generic = 0;
    Type **params = NULL;
    if (t->type == TOK_LPAREN) {
        params = parse_type_param_defs();
        t = next_token();
        generic = 1;
    }
    if (t->type != TOK_LBRACE) {
        error(lineno(), current_file_name(), "Unexpected token '%s' while parsing struct type.", tok_to_string(t));
    }

    char **member_names = NULL;
    Type **member_types = NULL;

    Ast **methods = NULL;

    for (;;) {
        Tok *t = next_token();
        
        if (t->type == TOK_ID) {
            expect(TOK_COLON);

            char *name = t->sval;

            Type *ty = parse_type(next_token(), poly_ok);

            array_push(member_names, name);
            array_push(member_types, ty);

            /*expect(TOK_SEMI);*/
            /*expect_eol();*/
            int last_line = t->line;
            t = next_token();
            if (t->line == last_line) {
                if (t->type != TOK_SEMI) {
                    error(t->line, current_file_name(), "Expected semicolon or line ending in struct definition.");
                }
            } else {
                unget_token(t);
            }
        } else if (t->type == TOK_RBRACE) {
            break;
        } else if (t->type == TOK_FN) {
            array_push(methods, parse_func_decl(0));
        } else {
            error(lineno(), current_file_name(), "Unexpected token '%s' in struct definition.", tok_to_string(t));
        }
    }

    int n = array_len(member_names);
    for (int i = 0; i < n-1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!strcmp(member_names[i], member_names[j])) {
                error(lineno(), current_file_name(),
                    "Repeat member name '%s' in struct.", member_names[i]);
            }
        }
    }

    Type *tp;

    if (generic) {
        tp = make_generic_struct_type(member_names, member_types, params);
    } else {
        tp = make_struct_type(member_names, member_types);
    }
    // TODO: disallow methods in struct body
    /*tp->st.methods = methods;*/
    
    return tp;
}

Ast *parse_anon_scope();

Ast *parse_statement(Tok *t, int eat_semi) {
    Ast *ast = NULL;
    int needs_semi = 1;

    switch (t->type) {
    case TOK_ID:
        if (peek_token() != NULL && peek_token()->type == TOK_COLON) {
            next_token();
            ast = parse_declaration(t);
        } else {
            ast = parse_expression(t, 0);
        }
        break;
    case TOK_TYPE:
        ast = parse_type_decl();
        needs_semi = 0;
        break;
    case TOK_ENUM:
        ast = parse_enum_decl();
        needs_semi = 0;
        break;
    case TOK_USE:
        ast = ast_alloc(AST_USE);
        ast->use->object = parse_expression(next_token(), 0);
        break;
    case TOK_DEFER:
        ast = ast_alloc(AST_DEFER);
        ast->defer->call = parse_expression(next_token(), 0);
        break;
    case TOK_IMPL:
        ast = ast_alloc(AST_IMPL);
        ast->impl->type = parse_type(next_token(), 0);
        expect(TOK_LBRACE);
        ast->impl->methods = parse_statement_list();
        expect(TOK_RBRACE);
        needs_semi = 0;
        break;
    case TOK_COMMENT:
        ast = ast_alloc(AST_COMMENT);
        ast->comment->text = t->sval;
        return ast;
    case TOK_DIRECTIVE:
        if (!strcmp(t->sval, "include")) {
            t = next_token();
            int line = lineno();
            if (t->type != TOK_STR || !expect_line_break_or_semicolon()) {
                error(line, current_file_name(),
                    "Unexpected token '%s' while parsing include directive.",
                    tok_to_string(t));
            }
            char *path = t->sval;
            if (path[0] != '/') {
                char *dir = dir_name(current_file_name());
                int dirlen = strlen(dir);
                int pathlen = strlen(path);
                char *tmp = malloc(sizeof(char) * (dirlen + pathlen + 1));
                snprintf(tmp, dirlen + pathlen + 1, "%s%s", dir, path);
                path = tmp;
            }
            Ast *ast = parse_source_file(line, current_file_name(), path);
            pop_file_source();
            return ast;
        }
        if (!strcmp(t->sval, "import")) {
            t = next_token();
            int line = lineno();
            if (t->type != TOK_STR || !expect_line_break_or_semicolon()) {
                error(line, current_file_name(),
                    "Unexpected token '%s' while parsing import directive.",
                    tok_to_string(t));
            }
            // TODO: pass more context for error 
            Ast *ast = ast_alloc(AST_IMPORT);
            ast->line = line;
            ast->import->path = t->sval;
            return ast;
        }

        if (!strcmp(t->sval, "autocast")) {
            // TODO: expect an extern fn definition, set all args to autocast
        }
        // If not #import, default to normal behavior
        ast = parse_expression(t, 0);
        break;
    case TOK_FN:
        if (peek_token()->type == TOK_ID) {
            return parse_func_decl(0);
        }
        ast = parse_expression(t, 0);
        break;
    case TOK_EXTERN:
        ast = parse_extern_func_decl();
        break;
    case TOK_LBRACE:
        return parse_anon_scope();
    case TOK_WHILE: {
        ast = ast_alloc(AST_WHILE);
        Ast *condition = parse_statement(next_token(), 0);

        Tok *next = next_token();
        if (next->type != TOK_LBRACE) {
            if (next->type != TOK_SEMI) {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing while loop condition, expected semicolon.", tok_to_string(next));
            }
            ast->while_loop->initializer = condition;
            next = next_token();
            condition = parse_expression(next, 0);

            next = next_token();
            if (next == NULL || next->type != TOK_LBRACE) {
                error(lineno(), current_file_name(), "Unexpected token '%s' while parsing while loop condition.", tok_to_string(next));
            }
        }
        ast->while_loop->condition = condition;
        ast->while_loop->body = parse_astblock(1);
        return ast;
    }
    case TOK_FOR:
        ast = ast_alloc(AST_FOR);
        t = next_token();
        // TODO: OP_REF and OP_BINAND? whoops
        if (t->type == TOK_OP && t->op == OP_BINAND) {
            ast->for_loop->by_reference = 1;
            t = next_token();
        }
        if (t->type != TOK_ID) {
            error(ast->line, ast->file, 
                "Unexpected token '%s' while parsing for loop.", tok_to_string(t));
        }
        ast->for_loop->itervar = make_var(t->sval, NULL);
        
        t = next_token();
        if (t->type == TOK_COMMA) {
            t = expect(TOK_ID);
            char *name = t->sval;
            Type *index_type = base_type(INT_T);

            t = next_token();
            if (t->type == TOK_COLON) {
                index_type = parse_type(next_token(), 0);
                t = next_token();
            }

            ast->for_loop->index = make_var(name, index_type);
        }
        if (t->type != TOK_IN) {
            error(ast->line, ast->file,
                "Unexpected token '%s' while parsing for loop.", tok_to_string(t));
        }

        t = next_token();
        ast->for_loop->iterable = parse_expression(t, 0);

        t = next_token();
        if (t == NULL || t->type != TOK_LBRACE) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing for loop.", tok_to_string(t));
        }
        ast->for_loop->body = parse_astblock(1);
        return ast;
    case TOK_IF:
        return parse_conditional();
    case TOK_RETURN:
        ast = parse_return_statement(t);
        break;
    case TOK_BREAK:
        ast = ast_alloc(AST_BREAK);
        break;
    case TOK_CONTINUE:
        ast = ast_alloc(AST_CONTINUE);
        break;
    default:
        ast = parse_expression(t, 0);
    }
    if (needs_semi || peek_token()->type == TOK_SEMI) {
        if (eat_semi) {
            expect(TOK_SEMI);
        }
    }
    return ast;
}

Ast *parse_directive(Tok *t) {
    Ast *dir = ast_alloc(AST_DIRECTIVE);

    dir->directive->name = t->sval;

    // autocast
    if (!strcmp(t->sval, "autocast")) {
        return dir;
    }

    Tok *next = next_token();
    if (next == NULL) {
        error(dir->line, dir->file, "Unexpected end of input.");
    }

    // type
    if (!strcmp(t->sval, "type")) {
        dir->directive->object = NULL;
        dir->var_type = parse_type(next, 0);
        return dir;
    } else if (!strcmp(t->sval, "import")) {
        error(dir->line, dir->file, "#import directive must be a statement.");
    }

    // typeof
    if (next->type != TOK_LPAREN) {
        error(dir->line, dir->file, "Unexpected token '%s' while parsing directive '%s'",
            tok_to_string(next), t->sval);
    }

    dir->directive->object = parse_expression(next_token(), 0);
    expect(TOK_RPAREN);
    return dir;
}

Ast *parse_primary(Tok *t) {
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
        Ast *id = ast_alloc(AST_IDENTIFIER);
        id->ident->var = NULL;
        id->ident->varname = t->sval;
        return id;
    }
    case TOK_DIRECTIVE: 
        return parse_directive(t);
    case TOK_FN:
        return parse_func_decl(1);
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
        ast->unary->object = parse_expression(next_token(), priority_of(t));
        return ast;
    }
    case TOK_UOP: {
        Ast *ast = ast_alloc(AST_UOP);
        ast->unary->op = t->op;
        ast->unary->object = parse_expression(next_token(), priority_of(t));
        return ast;
    }
    case TOK_LPAREN: {
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected end of input.");
        }
        Ast *ast = parse_expression(next, 0);

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
    case TOK_LSQUARE: {
        Ast *ast = ast_alloc(AST_TYPE_OBJ);
        ast->type_obj->t = parse_type(t, 0);
        return ast;
    }
    case TOK_NEW: {
        Ast *ast = ast_alloc(AST_NEW);
        Tok *next = next_token();
        if (next == NULL) {
            error(lineno(), current_file_name(), "Unexpected end of input.");
        }
        if (next->type == TOK_LSQUARE) {
            next = next_token();
            if (next->type == TOK_RSQUARE) {
                error(lineno(), current_file_name(), "Must provide a count for 'new' array type.");
            }
            Ast *count = parse_expression(next, 0);
            expect(TOK_RSQUARE);
            ast->new->count = count;
            next = next_token();
        }
        Type *t = parse_type(next, 0);
        if (ast->new->count != NULL) {
            t = make_array_type(t);
            t->resolved->array.owned = 1;
        } else {
            t = make_ref_type(t);
            t->resolved->ref.owned = 1;
        }
        ast->new->type = t;
        return ast;
    }
    default:
        break;
    }
    error(lineno(), current_file_name(),
        "Unexpected token '%s' while parsing primary expression.", tok_to_string(t));
    return NULL;
}

Ast *parse_conditional() {
    Ast *c = ast_alloc(AST_CONDITIONAL);

    Ast *condition = parse_statement(next_token(), 0);

    Tok *next = next_token();
    if (next->type != TOK_LBRACE) {
        if (next->type != TOK_SEMI) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing conditional, expected semicolon.", tok_to_string(next));
        }
        c->cond->initializer = condition;
        next = next_token();
        condition = parse_expression(next, 0);

        next = next_token();
        if (next == NULL || next->type != TOK_LBRACE) {
            error(lineno(), current_file_name(), "Unexpected token '%s' while parsing conditional.", tok_to_string(next));
        }
    }
    c->cond->condition = condition;

    c->cond->if_body = parse_astblock(1);

    next = next_token();
    if (next && next->type == TOK_ELSE) {
        next = next_token();
        if (!next) {
            error(lineno(), current_file_name(), "Unexpected EOF while parsing conditional.");
        } else if (next->type == TOK_IF) {
            AstBlock *block = calloc(sizeof(AstBlock), 1);

            block->file = current_file_name();
            block->startline = lineno();
            Ast *tmp = parse_conditional();
            block->endline = lineno();
            array_push(block->statements, tmp);
            c->cond->else_body = block;
            return c;
        } else if (next->type != TOK_LBRACE) {
            error(lineno(), current_file_name(),
                "Unexpected token '%s' while parsing conditional.", tok_to_string(next));
        }
        c->cond->else_body = parse_astblock(1);
    } else {
        c->cond->else_body = NULL;
        unget_token(next);
    }
    return c;
}

Ast **parse_statement_list() {
    Ast **stmts = NULL;
    Tok *t;
    for (;;) {
        t = next_token_or_comment();
        if (t == NULL || t->type == TOK_RBRACE) {
            break;
        }
        if (t->type == TOK_NL) {
            continue;
        }
        Ast *stmt = parse_statement(t, 1);
        if (stmt) {
            array_push(stmts, stmt);
        }
    }
    unget_token(t);
    return stmts;
}

AstBlock *parse_astblock(int bracketed) {
    AstBlock *block = calloc(sizeof(AstBlock), 1);

    block->startline = lineno();
    block->file = current_file_name();
    block->statements = parse_statement_list();

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

Ast *parse_anon_scope() {
    Ast *b = ast_alloc(AST_ANON_SCOPE);
    b->anon_scope->body = parse_astblock(1);
    return b;
}

Ast *parse_block(int bracketed) {
    Ast *b = ast_alloc(AST_BLOCK);
    b->block = parse_astblock(bracketed);
    return b;
}

Ast *parse_source_file(int line, char *source_file, char *filename) {
    char *err;
    FILE *f = open_file_or_error(filename, "r", &err);
    if (!f) {
        error(line, source_file, "Could not open source file '%s': %s", filename, err);
    }
    push_file_source(filename, f);
    return parse_block(0);
}

// TODO: why is this here
void init_builtins() {
    {
        Type **arg_types = NULL;
        array_push(arg_types, base_type(BOOL_T));
        Var *v = make_var("assert", make_fn_type(arg_types, base_type(VOID_T), 0));
        v->ext = 1;
        v->constant = 1;
        define_builtin(v);
    }

    {
        Type **arg_types = NULL;
        array_push(arg_types, base_type(STRING_T));
        Var *v = make_var("print_str", make_fn_type(arg_types, base_type(VOID_T), 0));
        v->ext = 1;
        v->constant = 1;
        define_builtin(v);
    }

    {
        Type **arg_types = NULL;
        array_push(arg_types, make_ref_type(base_numeric_type(UINT_T, 8)));
        Var *v = make_var("print_buf", make_fn_type(arg_types, base_type(VOID_T), 0));
        v->ext = 1;
        v->constant = 1;
        define_builtin(v);
    }

    {
        Type **arg_types = NULL;
        array_push(arg_types, base_type(STRING_T));
        Var *v = make_var("println", make_fn_type(arg_types, base_type(VOID_T), 0));
        v->ext = 1;
        v->constant = 1;
        define_builtin(v);
    }

    {
        Type **arg_types = NULL;
        array_push(arg_types, base_type(INT_T));
        Var *v = make_var("itoa", make_fn_type(arg_types, base_type(STRING_T), 0));
        v->ext = 1;
        v->constant = 1;
        define_builtin(v);
    }

    {
        Type **arg_types = NULL;
        array_push(arg_types, base_type(UINT_T));
        Var *v = make_var("utoa", make_fn_type(arg_types, base_type(STRING_T), 0));
        v->ext = 1;
        v->constant = 1;
        define_builtin(v);
    }

    {
        Type **arg_types = NULL;
        array_push(arg_types, base_type(BASEPTR_T));
        Var *v = make_var("validptr", make_fn_type(arg_types, base_type(BOOL_T), 0));
        v->ext = 1;
        v->constant = 1;
        define_builtin(v);
    }
}
