#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array/array.h"
#include "ast.h"
#include "token.h"
#include "util.h"

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
    case AST_ANON_SCOPE:
        ast->anon_scope = calloc(sizeof(AstAnonScope), 1);
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
    case AST_IMPORT:
        ast->import = calloc(sizeof(AstImport), 1);
        break;
    case AST_TYPE_OBJ:
        ast->type_obj = calloc(sizeof(AstTypeObj), 1);
        break;
    case AST_SPREAD:
        ast->spread = calloc(sizeof(AstSpread), 1);
        break;
    case AST_NEW:
        ast->new = calloc(sizeof(AstNew), 1);
        break;
    case AST_DEFER:
        ast->defer = calloc(sizeof(AstDefer), 1);
        break;
    case AST_IMPL:
        ast->impl = calloc(sizeof(AstImpl), 1);
        break;
    case AST_METHOD:
        ast->method = calloc(sizeof(AstMethod), 1);
        break;
    case AST_TYPE_IDENT:
        ast->type_ident = calloc(sizeof(AstTypeIdent), 1);
        break;
    case AST_PACKAGE:
        ast->pkg = calloc(sizeof(AstPackage), 1);
        break;
    }
    return ast;
}

Ast *copy_ast(Scope *scope, Ast *ast) {
    Ast *cp = ast_alloc(ast->type);
    cp->line = ast->line;
    cp->file = ast->file;
    cp->var_type = ast->var_type;

    switch (ast->type) {
    case AST_LITERAL:
        cp->lit = calloc(sizeof(AstLiteral), 1);
        *cp->lit = *ast->lit;
        switch (cp->lit->lit_type) {
        case ARRAY_LIT:
        case STRUCT_LIT:
        case COMPOUND_LIT:
            if (cp->lit->compound_val.array_tempvar != NULL) {
                cp->lit->compound_val.array_tempvar->var = copy_var(scope, cp->lit->compound_val.array_tempvar->var);
            }
            cp->lit->compound_val.type = copy_type(scope, cp->lit->compound_val.type);
            cp->lit->compound_val.member_exprs = NULL;
            for (int i = 0; i < array_len(ast->lit->compound_val.member_exprs); i++) {
                array_push(cp->lit->compound_val.member_exprs, copy_ast(scope, ast->lit->compound_val.member_exprs[i]));
            }
            break;
        case ENUM_LIT:
            cp->lit->enum_val.enum_type = copy_type(scope, cp->lit->enum_val.enum_type);
            break;
        default:
            break;
        }
        break;
    case AST_DOT:
        cp->dot = calloc(sizeof(AstDot), 1);
        cp->dot->object = copy_ast(scope, ast->dot->object);
        cp->dot->member_name = ast->dot->member_name;
        break;
    case AST_ASSIGN:
    case AST_BINOP:
        cp->binary = calloc(sizeof(AstBinaryOp), 1);
        cp->binary->op = ast->binary->op;
        cp->binary->left = copy_ast(scope, ast->binary->left);
        cp->binary->right = copy_ast(scope, ast->binary->right);
        break;
    case AST_UOP:
        cp->unary = calloc(sizeof(AstUnaryOp), 1);
        cp->unary->op = ast->unary->op;
        cp->unary->object = copy_ast(scope, ast->unary->object);
        break;
    case AST_IDENTIFIER:
        cp->ident = calloc(sizeof(AstIdent), 1);
        cp->ident->varname = ast->ident->varname;
        break;
    case AST_COPY:
        cp->copy = calloc(sizeof(AstCopy), 1);
        cp->copy->expr = copy_ast(scope, ast->copy->expr);
        break;
    case AST_DECL:
        cp->decl = calloc(sizeof(AstDecl), 1);
        cp->decl->global = ast->decl->global;
        cp->decl->var = copy_var(scope, ast->decl->var);
        if (ast->decl->init != NULL) {
            cp->decl->init = copy_ast(scope, ast->decl->init);
        }
        break;
    case AST_ANON_FUNC_DECL:
    case AST_EXTERN_FUNC_DECL:
    case AST_FUNC_DECL:
        cp->fn_decl = calloc(sizeof(AstFnDecl), 1);
        cp->fn_decl->var = copy_var(scope, ast->fn_decl->var);
        cp->fn_decl->anon = ast->fn_decl->anon;
        for (int i = 0; i < array_len(ast->fn_decl->args); i++) {
            array_push(cp->fn_decl->args, copy_var(scope, ast->fn_decl->args[i]));
        }
        cp->fn_decl->body = copy_ast_block(scope, ast->fn_decl->body);
        break;
    case AST_CALL:
        cp->call = calloc(sizeof(AstCall), 1);
        cp->call->fn = copy_ast(scope, ast->call->fn);
        for (int i = 0; i < array_len(ast->call->args); i++) {
            array_push(cp->call->args, copy_ast(scope, ast->call->args[i]));
        }
        if (ast->call->variadic_tempvar != NULL) {
            cp->call->variadic_tempvar->var = copy_var(scope, ast->call->variadic_tempvar->var);
        }
        break;
    case AST_INDEX:
        cp->index = calloc(sizeof(AstIndex), 1);
        cp->index->object = copy_ast(scope, ast->index->object);
        cp->index->index = copy_ast(scope, ast->index->index);
        break;
    case AST_SLICE:
        cp->slice = calloc(sizeof(AstSlice), 1);
        cp->slice->object = copy_ast(scope, ast->slice->object);
        if (ast->slice->offset != NULL) {
            cp->slice->offset = copy_ast(scope, ast->slice->offset);
        }
        if (ast->slice->length != NULL) {
            cp->slice->length = copy_ast(scope, ast->slice->length);
        }
        break;
    case AST_CONDITIONAL:
        cp->cond = calloc(sizeof(AstConditional), 1);
        if (ast->cond->initializer) {
            cp->cond->initializer = copy_ast(scope, ast->cond->initializer);
        }
        cp->cond->condition = copy_ast(scope, ast->cond->condition);
        cp->cond->if_body = copy_ast_block(scope, ast->cond->if_body);
        if (ast->cond->else_body != NULL) {
            cp->cond->else_body = copy_ast_block(scope, ast->cond->else_body);
        }
        break;
    case AST_RETURN:
        cp->ret = calloc(sizeof(AstReturn), 1);
        cp->ret->expr = copy_ast(scope, ast->ret->expr);
        break;
    case AST_TYPE_DECL:
        cp->type_decl = calloc(sizeof(AstTypeDecl), 1);
        // This will have to happen before first_pass
        break;
    case AST_BLOCK:
        cp->block = copy_ast_block(scope, ast->block);
        break;
    case AST_WHILE:
        cp->while_loop = calloc(sizeof(AstWhile), 1);
        if (ast->while_loop->initializer) {
            cp->while_loop->initializer = copy_ast(scope, ast->while_loop->initializer);
        }
        cp->while_loop->condition = copy_ast(scope, ast->while_loop->condition);
        cp->while_loop->body = copy_ast_block(scope, ast->while_loop->body);
        break;
    case AST_FOR:
        cp->for_loop = calloc(sizeof(AstFor), 1);
        cp->for_loop->itervar = copy_var(scope, ast->for_loop->itervar);
        if (ast->for_loop->index != NULL) {
            cp->for_loop->index = copy_var(scope, ast->for_loop->index);
        }
        cp->for_loop->iterable = copy_ast(scope, ast->for_loop->iterable);
        cp->for_loop->body = copy_ast_block(scope, ast->for_loop->body);
        cp->for_loop->by_reference = ast->for_loop->by_reference;
        break;
    case AST_ANON_SCOPE:
        cp->anon_scope->body = copy_ast_block(scope, ast->anon_scope->body);
        break;
    case AST_BREAK:
    case AST_CONTINUE:
        break;
    case AST_CAST:
        cp->cast = calloc(sizeof(AstCast), 1);
        cp->cast->cast_type = copy_type(scope, ast->cast->cast_type);
        cp->cast->object = copy_ast(scope, ast->cast->object);
        break;
    case AST_DIRECTIVE:
        cp->directive = calloc(sizeof(AstDirective), 1);
        cp->directive->name = ast->directive->name;
        if (ast->directive->object != NULL) {
            cp->directive->object = copy_ast(scope, ast->directive->object);
        }
        break;
    case AST_TYPEINFO:
        cp->typeinfo = calloc(sizeof(AstTypeInfo), 1);
        cp->typeinfo->typeinfo_target = copy_type(scope, ast->typeinfo->typeinfo_target);
        break;
    case AST_ENUM_DECL:
        cp->enum_decl = calloc(sizeof(AstEnumDecl), 1);
        cp->enum_decl->enum_name = ast->enum_decl->enum_name;
        cp->enum_decl->enum_type = copy_type(scope, ast->enum_decl->enum_type);
        // TODO: wtf
        break;
    case AST_USE:
        cp->use = calloc(sizeof(AstUse), 1);
        cp->use->object = copy_ast(scope, ast->use->object);
        break;
    case AST_IMPORT:
        cp->import = ast->import;
        break;
        break;
    case AST_TYPE_OBJ:
        cp->type_obj = calloc(sizeof(AstTypeObj), 1);
        cp->type_obj->t = copy_type(scope, ast->type_obj->t);
        break;
    case AST_SPREAD:
        cp->spread = calloc(sizeof(AstSpread), 1);
        cp->spread->object = copy_ast(scope, ast->spread->object);
        break;
    case AST_NEW:
        cp->new = calloc(sizeof(AstNew), 1);
        if (ast->new->count != NULL) {
            cp->new->count = copy_ast(scope, ast->new->count);
        }
        cp->new->type = copy_type(scope, ast->new->type);
        break;
    case AST_DEFER:
        cp->defer = calloc(sizeof(AstDefer), 1);
        cp->defer->call = copy_ast(scope, ast->defer->call);
        break;
    case AST_IMPL:
        cp->impl = calloc(sizeof(AstImpl), 1);
        cp->impl->type = copy_type(scope, ast->impl->type);
        for (int i = 0; i < array_len(ast->impl->methods); i++) {
            array_push(cp->impl->methods, copy_ast(scope, ast->impl->methods[i]));
        }
        break;
    case AST_METHOD:
        cp->method = calloc(sizeof(AstMethod), 1);
        cp->method->recv = copy_ast(scope, ast->method->recv);
        cp->method->name = ast->method->name;
        cp->method->decl = ast->method->decl;
        break;
    case AST_TYPE_IDENT:
        cp->type_ident = calloc(sizeof(AstTypeIdent), 1);
        cp->type_ident->type = copy_type(scope, ast->type_ident->type);
        break;
    case AST_PACKAGE:
        /*ast->pkg = calloc(sizeof(AstPackage), 1);*/
        break;
    }
    return cp;
}

AstBlock *copy_ast_block(Scope *scope, AstBlock *block) {
    AstBlock *b = calloc(sizeof(AstBlock), 1);
    b->startline = block->startline;
    b->endline = block->endline;
    b->file = block->file;
    for (int i = 0; i < array_len(block->statements); i++) {
        array_push(b->statements, copy_ast(scope, block->statements[i]));
    }
    return b;
}

Ast *make_ast_copy(Ast *ast) {
    Ast *cp = ast_alloc(AST_COPY);
    cp->copy->expr = ast;
    cp->var_type = ast->var_type;
    return cp;
}

int needs_temp_var(Ast *ast) {
    switch (ast->type) {
    case AST_NEW:
        return 1;
    case AST_BINOP:
    case AST_UOP:
    case AST_CALL:
    case AST_LITERAL:
    case AST_SLICE:
    case AST_INDEX:
        return is_dynamic(ast->var_type);
    default:
        break;
    }
    return 0;
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
    id->ident->var = var;
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

char *get_varname(Ast *ast) {
    switch (ast->type) {
    case AST_DOT: {
        char *name = get_varname(ast->dot->object);
        if (name == NULL) {
            return NULL;
        }
        Type *t = ast->dot->object->var_type;
        ResolvedType *r = t->resolved;
        for (int i = 0; i < array_len(r->st.member_names); i++) {
            char *member_name = r->st.member_names[i];
            if (!strcmp(r->st.member_names[i], ast->dot->member_name)) {
                char *proxy_name;
                int proxy_name_len;
                if (r->comp == REF) {
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
