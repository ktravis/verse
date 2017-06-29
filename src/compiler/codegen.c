#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../array/array.h"
#include "typechecking.h"
#include "codegen.h"
#include "parse.h"
#include "scope.h"
/*#include "semantics.h"*/

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

static Type **struct_types;

// TODO: make this not be like it is
int get_struct_type_id(Type *type) {
    for (int i = 0; i < array_len(struct_types); i++) {
        Type *item = struct_types[i];
        ResolvedType *r = item->resolved;
        if (array_len(r->st.member_types) != array_len(r->st.member_types)) {
            continue;
        }
        if (item->id == type->id) {
            return type->id;
        }
        int match = 1;
        for (int j = 0; j < array_len(r->st.member_names); j++) {
            if (strcmp(r->st.member_names[j], r->st.member_names[j]) ||
               !check_type(r->st.member_types[j], r->st.member_types[j])) {
                match = 0;
                break;
            }
        }
        if (match) {
            return item->id;
        }
    }
    return type->id;
}

void emit_temp_var(Scope *scope, Ast *ast, int ref) {
    Var *v = find_temp_var(scope, ast);
    v->initialized = 1;
    printf("(_tmp%d = ", v->id);

    ResolvedType *r = v->type->resolved;
    if (r->comp == REF) {
        Type *inner = r->ref.inner;
        if (inner->resolved->comp == STATIC_ARRAY) {
            printf("(");
            emit_type(inner);
            printf("*)");
        }
    }
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
    Type *t = ast->binary->left->var_type;
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
    case AST_SLICE:
    case AST_UOP:
    case AST_BINOP:
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
        error(ast->line, ast->file, "<internal> couldn't do the string binop? %d", ast->type);
    }
}

void emit_dot_op(Scope *scope, Ast *ast) {
    Type *t = ast->dot->object->var_type;

    if (ast->dot->object->type == AST_LITERAL && ast->dot->object->lit->lit_type == ENUM_LIT) {
        // TODO this should probably be a tmpvar
        char *s = t->resolved->en.member_names[ast->dot->object->lit->enum_val.enum_index];
        printf("init_string(\"");
        print_quoted_string(s);
        printf("\", %d)", (int)strlen(s));
        return;
    }

    if (t->resolved->comp == STATIC_ARRAY) {
        if (!strcmp(ast->dot->member_name, "length")) {
            printf("%ld", t->resolved->array.length);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            compile(scope, ast->dot->object);
        }
    } else {
        compile(scope, ast->dot->object);
        if (t->resolved->comp == REF) {
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

    Type *lt = l->var_type;

    if (lt->resolved->comp == STATIC_ARRAY) {
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

    if (is_dynamic(lt) || r->type == AST_NEW) {
        if (l->type == AST_IDENTIFIER &&
                !l->ident->var->initialized && !is_owned(lt)) {
            printf("_vs_%d = ", l->ident->var->id);
            if (is_lvalue(r)) {
                emit_copy(scope, r);
            } else {
                compile(scope, r);
            }

            l->ident->var->initialized = 1;
        } else {
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

            if (l->type == AST_DOT || l->type == AST_INDEX || l->type == AST_UOP) {
                compile(scope, l);
            } else {
                printf("_vs_%d", l->ident->var->id);
            }
            printf(",_tmp%d)", temp->id);
        }
    } else {
        compile(scope, l);
        printf(" = ");
        if (lt->resolved->comp == ARRAY) {
            compile_unspecified_array(scope, r);
        } else if (lt->resolved->comp == STATIC_ARRAY) {
            compile_static_array(scope, r);
        } else if (is_any(lt) && !is_any(r->var_type)) {
            emit_any_wrapper(scope, r);
        } else {
            compile(scope, r);
        }
    }
}

void emit_binop(Scope *scope, Ast *ast) {
    Type *lt = ast->binary->left->var_type;
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
    Type *t = ast->var_type;

    // TODO: should this bail out here, or just never be called?
    /*if (t->comp == FUNC || !is_dynamic(t)) {*/
    // TODO: is this good? are we missing places that owned references need to
    // be copied?
    if (t->resolved->comp == FUNC || t->resolved->comp == REF || !is_dynamic(t)) {
        compile(scope, ast);
        return;
    }

    if (is_string(t)) {
        printf("copy_string(");
        compile(scope, ast);
        printf(")");
    } else if (t->resolved->comp == STRUCT) {
        /*printf("_copy_%d(", get_struct_type_id(t));*/
        printf("_copy_%d(", t->id);
        compile(scope, ast);
        printf(")");
    /*} else if (t->comp == STATIC_ARRAY) {*/
        /*emit_static_array_copy(scope, ast->decl->var->type, dname, "_0");*/
    /*} else if (t->comp == ARRAY) {*/
    } else {
        error(-1, "internal", "wut even");
    }
}

void emit_type(Type *type) {
    assert(type != NULL);

    ResolvedType *r = type->resolved;
    switch (r->comp) {
    case BASIC:
        switch (r->data->base) {
        case UINT_T:
            printf("u");
        case INT_T:
            printf("int%d_t ", r->data->size * 8);
            break;
        case FLOAT_T:
            if (r->data->size == 4) { // TODO double-check these are always the right size
                printf("float ");
            } else if (r->data->size == 8) {
                printf("double ");
            } else {
                error(-1, "internal", "Cannot compile floating-point type of size %d.", r->data->size);
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
        emit_type(r->ref.inner);
        printf("*");
        break;
    case ARRAY:
        printf("struct array_type ");
        break;
    case STATIC_ARRAY:
        emit_type(r->array.inner);
        printf("*");
        break;
    case STRUCT:
        printf("struct _type_vs_%d ", type->id);//get_struct_type_id(type));
        break;
    case ENUM:
        emit_type(r->en.inner);
        break;
    default:
        error(-1, "internal", "wtf type");
    }
}

void compile_unspecified_array(Scope *scope, Ast *ast) {
    if (is_string(ast->var_type)) {
        printf("string_as_array(");
        compile(scope, ast);
        printf(")");
        return;
    }
    TypeComp c = ast->var_type->resolved->comp;
    if (c == ARRAY) {
        compile(scope, ast);
    } else if (c == STATIC_ARRAY) {
        printf("(struct array_type){.data=");
        compile(scope, ast);
        printf(",.length=%ld}", ast->var_type->resolved->array.length);
    } else {
        error(ast->line, ast->file, "Was expecting an array of some kind here, man.");
    }
}

void compile_static_array(Scope *scope, Ast *ast) {
    if (ast->var_type->resolved->comp == STATIC_ARRAY) {
        compile(scope, ast);
    } else if (ast->var_type->resolved->comp == ARRAY) {
        printf("(");
        compile(scope, ast);
        printf(").data");
    } else {
        error(ast->line, ast->file, "Was expecting a static array here, man.");
    }
}

void emit_static_array_decl(Scope *scope, Ast *ast) {
    char *membername = malloc(sizeof(char) * (snprintf(NULL, 0, "_vs_%d", ast->decl->var->id) + 1));
    sprintf(membername, "_vs_%d", ast->decl->var->id);
    emit_structmember(scope, membername, ast->decl->var->type);
    free(membername);

    if (ast->decl->init == NULL) {
        printf(" = {0}");
    } else if (ast->decl->init->type == AST_LITERAL) {
        assert(ast->decl->init->lit->lit_type == ARRAY_LIT);

        printf(" = {");
        ResolvedType *r = ast->decl->var->type->resolved;
        for (int i = 0; i < r->array.length; i++) {
            Ast *expr = ast->decl->init->lit->compound_val.member_exprs[i];

            if (is_any(r->array.inner) && !is_any(expr->var_type)) {
                emit_any_wrapper(scope, expr);
            } else if (is_lvalue(expr)) {
                emit_copy(scope, expr);
            } else {
                compile(scope, expr);
            }
            if (i < r->array.length - 1) {
                printf(",");
            }
        }
        printf("}");
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

        char *dname = malloc(sizeof(char) * (snprintf(NULL, 0, "_vs_%d", ast->decl->var->id) + 1));
        sprintf(dname, "_vs_%d", ast->decl->var->id);
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
    Type *t = ast->decl->var->type;
    if (t->resolved->comp == STATIC_ARRAY) {
        emit_static_array_decl(scope, ast);
        return;
    }
    emit_type(t);
    TypeComp c = t->resolved->comp;
    printf("_vs_%d", ast->decl->var->id);
    if (ast->decl->init == NULL) {
        if (c == BASIC) {
            int b = t->resolved->data->base;
            if (b == STRING_T) {
                printf(" = (struct string_type){0}");
                ast->decl->var->initialized = 1;
            } else if (b == INT_T || b == UINT_T || b == FLOAT_T) {
                printf(" = 0");
            } else if (b == BASEPTR_T) {
                printf(" = NULL");
            }
        } else if (c == STRUCT) {
            printf(";\n");
            indent();
            printf("_init_%d(&_vs_%d)", t->id, ast->decl->var->id);//get_struct_type_id(t), ast->decl->var->id);
            ast->decl->var->initialized = 1;
        } else if (c == REF)  {
            printf(" = NULL");
        } else if (c == ARRAY) {
            printf(" = {0}");
        }
    } else {
        printf(" = ");
        if (c == ARRAY) {
            compile_unspecified_array(scope, ast->decl->init);
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
    Type *fn_type = fn->fn_decl->var->type;
    ResolvedType *r = fn_type->resolved;
    if (fn->fn_decl->polymorphs) {
        scope = fn->fn_decl->scope;
        for (int i = 0; i < array_len(fn->fn_decl->polymorphs); i++) {
            Polymorph *p = fn->fn_decl->polymorphs[i];
            scope->polymorph = p;
            printf("/* %s */\n", fn->fn_decl->var->name);
            indent();
            emit_type(p->ret);

            printf("_poly_%d_vs_%d(", p->id, fn->fn_decl->var->id);

            for (int i = 0; i < array_len(p->args); i++) {
                if (i > 0) {
                    printf(",");
                }
                int type_index = (i >= array_len(p->args)) ? array_len(p->args)-1 : i;
                if (r->fn.variadic && i == (array_len(p->args) - 1)) {
                    printf("struct array_type ");
                } else {
                    emit_type(p->args[type_index]);
                }
                printf("_vs_%d", fn->fn_decl->args[i]->id);
            }
            printf(") ");

            emit_scope_start(scope);
            emit_scope_start(p->scope);
            compile_block(p->scope, p->body);
            emit_scope_end(p->scope);
            emit_scope_end(scope);
            scope->polymorph = NULL;
        }
    } else {
        if (is_polydef(fn_type)) {
            // polymorph not being used
            return;
        }
        printf("/* %s */\n", fn->fn_decl->var->name);
        indent();
        emit_type(r->fn.ret);

        assert(!fn->fn_decl->var->ext);
        printf("_vs_%d(", fn->fn_decl->var->id);

        Var **args = fn->fn_decl->args;
        int nargs = array_len(args);
        for (int i = 0; i < nargs; i++) {
            if (i > 0) {
                printf(",");
            }
            if (r->fn.variadic && i == (nargs - 1)) {
                printf("struct array_type ");
            } else {
                emit_type(args[i]->type);
            }
            printf("_vs_%d", args[i]->id);
        }
        printf(") ");

        emit_scope_start(fn->fn_decl->scope);
        compile_block(fn->fn_decl->scope, fn->fn_decl->body);
        emit_scope_end(fn->fn_decl->scope);
    }
}

void emit_structmember(Scope *scope, char *name, Type *st) {
    if (st->resolved->comp == STATIC_ARRAY) {
        emit_structmember(scope, name, st->resolved->array.inner);
        long length = 0;
        while (st->resolved->comp == STATIC_ARRAY) {
            length += st->resolved->array.length;
            st = st->resolved->array.inner;
        }
        printf("[%ld]", length);
    } else {
        emit_type(st);
        printf("%s", name);
    }
}

void emit_static_array_copy(Scope *scope, Type *t, char *dest, char *src) {
    Type *inner = t->resolved->array.inner;
    if (!is_dynamic(t)) {
        printf("memcpy(%s, %s, sizeof(", dest, src);
        emit_type(inner);
        printf(") * %ld)", t->resolved->array.length);
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

    printf("for (int i = 0; i < %ld; i++) {\n", t->resolved->array.length);
    change_indent(1);
    indent();

    if (inner->resolved->comp == STATIC_ARRAY) {
        int depth_len = snprintf(NULL, 0, "%d", d);

        char *dname = malloc(sizeof(char) * (depth_len + 2));
        sprintf(dname, "d%d", d);
        dname[depth_len+2] = 0;

        char *sname = malloc(sizeof(char) * (depth_len + 2));
        sprintf(sname, "s%d", d);
        sname[depth_len+2] = 0;

        printf("%s = %s[i], %s = %s[i];\n", dname, dest, sname, src);
        emit_static_array_copy(scope, t->resolved->array.inner, dname, sname);

        free(dname);
        free(sname);
    } else if (inner->resolved->comp == STRUCT) {
        printf("%s[i] = _copy_%d(%s[i])", dest, inner->id, src);//get_struct_type_id(inner), src);
    } else if (is_string(inner)) {
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
    ResolvedType *r = st->resolved;
    assert(r->comp == STRUCT);

    /*if (st->id != get_struct_type_id(st)) {*/
        /*return;*/
    /*}*/
    for (int i = 0; i < array_len(struct_types); i++) {
        if (struct_types[i]->id == st->id) {
            return;
        }
    }
    array_push(struct_types, st);

    emit_type(st);
    printf("{\n");

    change_indent(1);
    for (int i = 0; i < array_len(r->st.member_names); i++) {
        indent();
        emit_structmember(scope, r->st.member_names[i], r->st.member_types[i]);
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

    /*for (int i = 0; i < r->st.nmembers; i++) {*/
        /*Type *t = r->st.member_types[i];*/
        /*if (t->comp == REF && t->ref.owned) {*/
            /*indent();*/
            /*printf("x->%s = calloc(sizeof(", r->st.member_names[i]);*/
            /*emit_type(t->ref.inner);*/
            /*printf("), 1);\n");*/
        /*}*/
    /*}*/

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
    for (int i = 0; i < array_len(r->st.member_names); i++) {
        char *member_name = r->st.member_names[i];
        Type *member_type = r->st.member_types[i];
        ResolvedType *rm = member_type->resolved;
        // TODO: important, copy owned array slice
        if (rm->comp == STATIC_ARRAY && is_dynamic(rm->array.inner)) {
            indent();
            char *member = malloc(sizeof(char) * (strlen(member_name) + 3));
            sprintf(member, "x.%s", member_name);
            emit_static_array_copy(scope, member_type, member, member);
            printf(";\n");
            free(member);
        } else if (is_string(member_type)) {
            indent();
            printf("x.%s = copy_string(x.%s);\n", member_name, member_name);
        } else if (rm->comp == STRUCT) {
            indent();
            printf("x.%s = _copy_%d(x.%s);\n", member_name,
                    /*get_struct_type_id(member_type), member_name);*/
                    member_type->id, member_name);
        } else if (rm->comp == REF && rm->ref.owned) {
            indent();
            emit_type(rm->ref.inner);
            if (is_string(rm->ref.inner)) {
                printf("tmp%d = copy_string(*x.%s);\n", i, member_name);
            } else if (rm->ref.inner->resolved->comp == STRUCT) {
                printf("tmp%d = _copy_%d(*x.%s);\n", i, rm->ref.inner->id, member_name);//get_struct_type_id(rm->ref.inner), member_name);
            } else {
                printf("tmp%d = *x.%s;\n", i, member_name);
            }
            indent();
            printf("x.%s = malloc(sizeof(", member_name);
            emit_type(r->st.member_types[i]);
            printf("));\n");
            indent();
            printf("*x.%s = tmp%d;\n", member_name, i);
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

    printf("&");
    compile(scope, ast);
}

void compile_block(Scope *scope, AstBlock *block) {
    for (int i = 0; i < array_len(block->statements); i++) {
        Ast *stmt = block->statements[i];
        if (stmt->type == AST_FUNC_DECL || stmt->type == AST_EXTERN_FUNC_DECL ||
            stmt->type == AST_IMPL || stmt->type == AST_USE ||
            stmt->type == AST_TYPE_DECL || stmt->type == AST_DEFER ||
            (stmt->type == AST_DECL && stmt->decl->global)) {
            continue;
        }

        indent();
        if (needs_temp_var(stmt)) {
            Var *v = find_temp_var(scope, stmt);
            printf("_tmp%d = ", v->id);
        }
        compile(scope, stmt);

        if (stmt->type != AST_CONDITIONAL && stmt->type != AST_WHILE &&
            stmt->type != AST_FOR && stmt->type != AST_BLOCK &&
            stmt->type != AST_ANON_SCOPE && stmt->type != AST_IMPORT &&
            stmt->type != AST_TYPE_DECL && stmt->type != AST_ENUM_DECL) {
            printf(";\n");
        }
    }
}

void emit_any_wrapper(Scope *scope, Ast *ast) {
    printf("(struct _type_vs_%d){.value_pointer=", get_any_type_id());
    if (is_lvalue(ast)) {
        compile_ref(scope, ast);
    } else {
        emit_temp_var(scope, ast, 1);
    }
    /*Type *obj_type = resolve_alias(ast->var_type);*/
    printf(",.type=(struct _type_vs_%d *)&_type_info%d}", get_typeinfo_type_id(), ast->var_type->id);
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
    // does this resolution need to happen differently for polymorphs?
    Type *fn_type = ast->call->fn->var_type;
    ResolvedType *r = fn_type->resolved;
    assert(r->comp == FUNC);

    unsigned char needs_wrapper = 1;
    if (ast->call->fn->type == AST_IDENTIFIER) {
        Var *v = ast->call->fn->ident->var;
        needs_wrapper = !v->constant;
    }

    Type **argtypes = r->fn.args;

    if (ast->call->polymorph != NULL) {
        argtypes = ast->call->polymorph->args;
    }

    if (needs_wrapper) {
        printf("((");
        emit_type(r->fn.ret);
        printf("(*)(");
        if (array_len(r->fn.args) == 0) {
            printf("void");
        } else {
            for (int i = 0; i < array_len(argtypes); i++) {
                if (i > 0) {
                    printf(",");
                }
                emit_type(argtypes[i]);
            }
        }
        printf("))(");
    }

    if (ast->call->polymorph != NULL) {
        printf("_poly_%d", ast->call->polymorph->id);
    }
    if (needs_temp_var(ast->call->fn)) {
        emit_temp_var(scope, ast->call->fn, 0);
    } else {
        compile(scope, ast->call->fn);
    }

    if (needs_wrapper) {
        printf("))");
    }

    printf("(");

    TempVar *vt = ast->call->variadic_tempvar;
    int type_index = 0;
    for (int i = 0; i < array_len(ast->call->args); i++) {
        if (i > 0) {
            printf(",");
        }

        if (!r->fn.variadic || i < array_len(argtypes) - 1) {
            type_index = i;
        }
        // we check vt here to help with handling spread, there is probably
        // a nicer way
        Ast *arg_ast = ast->call->args[i];
        Type *defined_arg_type = argtypes[type_index];

        if (r->fn.variadic) {
            if (ast->call->has_spread) {
                if (i == array_len(r->fn.args) - 1) {
                    compile_call_arg(scope, arg_ast, 1);
                    break;
                }
            } else {
                if (i == array_len(r->fn.args) - 1) {
                    printf("(");
                }
                if (i >= array_len(r->fn.args) - 1) {
                    printf("_tmp%d[%d] = ", vt->var->id, i - (array_len(r->fn.args) - 1));
                }
            }
        }

        if (is_any(defined_arg_type) && !is_any(arg_ast->var_type)) {
            printf("(struct _type_vs_%d){.value_pointer=", get_any_type_id());
            if (needs_temp_var(arg_ast)) {
                emit_temp_var(scope, arg_ast, 1);
            } else {
                compile_ref(scope, arg_ast);
            }
            printf(",.type=(struct _type_vs_%d *)&_type_info%d}", get_typeinfo_type_id(), arg_ast->var_type->id);
        } else {
            compile_call_arg(scope, arg_ast, defined_arg_type->resolved->comp == ARRAY);
        }
    }

    if (r->fn.variadic && !ast->call->has_spread) {
        if (array_len(ast->call->args) - array_len(r->fn.args) < 0) {
            printf(", (struct array_type){0, NULL}");
        } else if (array_len(ast->call->args) > array_len(r->fn.args) - 1) {
            printf(", (struct array_type){%ld, _tmp%d})",
                vt->var->type->resolved->array.length, vt->var->id); // this assumes we have set vt correctly
        }
    }
    printf(")");
}

void emit_free_locals_except_return(Scope *scope, Var *returned);

void compile(Scope *scope, Ast *ast) {
    switch (ast->type) {
    case AST_LITERAL:
        switch (ast->lit->lit_type) {
        case INTEGER: {
            ResolvedType *res = ast->var_type->resolved;
            printf("%lld", ast->lit->int_val);
            // This may break somehow but I'm pissed and don't want to make it
            // right
            switch (res->data->size) {
                case 8:
                    if (res->data->base == UINT_T) {
                        printf("U");
                    }
                    printf("LL");
                    break;
                case 4:
                    if (res->data->base == UINT_T) {
                        printf("U");
                    }
                    printf("L");
                    break;
                default:
                    break;
            }
            break;
        }
        case FLOAT: // TODO this is truncated
            printf("%F", ast->lit->float_val);
            break;
        case CHAR:
            printf("'%c'", (unsigned char)ast->lit->int_val);
            break;
        case BOOL:
            printf("%d", (unsigned char)ast->lit->int_val);
            break;
        case STRING:
            printf("init_string(\"");
            print_quoted_string(ast->lit->string_val);
            printf("\", %d)", (int)strlen(ast->lit->string_val));
            break;
        case STRUCT_LIT:
            /*printf("(struct _type_vs_%d){", get_struct_type_id(ast->var_type));*/
            printf("(struct _type_vs_%d){", ast->var_type->id);
            if (array_len(ast->lit->compound_val.member_exprs) == 0) {
                printf("0");
            } else {
                StructType st = ast->var_type->resolved->st;
                for (int i = 0; i < array_len(ast->lit->compound_val.member_exprs); i++) {
                    Ast *expr = ast->lit->compound_val.member_exprs[i];
                    printf(".%s = ", ast->lit->compound_val.member_names[i]);

                    if (is_any(st.member_types[i]) && !is_any(expr->var_type)) {
                        emit_any_wrapper(scope, expr);
                    } else if (is_lvalue(expr)) {
                        emit_copy(scope, expr);
                    } else {
                        compile(scope, expr);
                    }

                    if (i != array_len(st.member_names) - 1) {
                        printf(", ");
                    }
                }
            }
            printf("}");
            break;
        case ARRAY_LIT: {
            TempVar *tmp = ast->lit->compound_val.array_tempvar;

            long n = array_len(ast->lit->compound_val.member_exprs);

            printf("(");
            for (int i = 0; i < n; i++) {
                Ast *expr = ast->lit->compound_val.member_exprs[i];
                printf("_tmp%d[%d] = ", tmp->var->id, i);

                if (is_any(tmp->var->type->resolved->array.inner) && !is_any(expr->var_type)) {
                    emit_any_wrapper(scope, expr);
                } else if (is_lvalue(expr)) {
                    emit_copy(scope, expr);
                } else {
                    compile(scope, expr);
                }

                printf(",");
            }
            if (ast->var_type->resolved->comp == STATIC_ARRAY) {
                printf("_tmp%d)", tmp->var->id);
            } else {
                printf("(struct array_type){%ld, _tmp%d})", n, tmp->var->id);
            }
            break;
        }
        case ENUM_LIT: {
            Type *t = ast->lit->enum_val.enum_type;
            printf("%ld", t->resolved->en.member_values[ast->lit->enum_val.enum_index]);
            break;
        }
        case COMPOUND_LIT:
            error(ast->line, ast->file, "<internal> literal type should be determined at this point");
            break;
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
    case AST_NEW: {
        Var *tmp = find_temp_var(scope, ast);

        // no temp var in case of declaration
        if (tmp != NULL) {
            printf("(_tmp%d = ", tmp->id);
        }

        ResolvedType *r = ast->var_type->resolved;
        if (r->comp == ARRAY) {
            printf("(allocate_array(");
            compile(scope, ast->new->count);
            printf(",sizeof(");
            emit_type(r->array.inner);
            printf(")))");
        } else {
            assert(r->comp == REF);
            /*printf("(_init_%d(NULL))", get_struct_type_id(r->ref.inner));*/
            printf("(_init_%d(NULL))", r->ref.inner->id);
        }
        if (tmp != NULL) {
            printf(")");
        }
        break;
    }
    case AST_CAST: {
        if (is_any(ast->cast->cast_type)) {
            emit_any_wrapper(scope, ast->cast->object);
            break;
        }
        ResolvedType *r = ast->cast->cast_type->resolved;
        if (r->comp == STRUCT) {
            printf("*");
        }

        printf("((");
        emit_type(ast->cast->cast_type);
        if (r->comp == STRUCT) {
            printf("*");
        }

        printf(")");
        if (r->comp == STRUCT) {
            printf("&");
        }

        compile(scope, ast->cast->object);
        printf(")");
        break;
    }
    case AST_SLICE: {
        Type *obj_type = ast->slice->object->var_type;
        ResolvedType *r = obj_type->resolved;

        if (is_string(obj_type)) {
            printf("string_slice(");
            compile(scope, ast->slice->object);
            printf(",");
            if (ast->slice->offset != NULL) {
                compile(scope, ast->slice->offset);
            } else {
                printf("0");
            }
            printf(",");
            if (ast->slice->length != NULL) {
                compile(scope, ast->slice->length);
            } else {
                printf("-1");
            }
            printf(")");
            break;
        }

        if (r->comp == STATIC_ARRAY) {
            printf("(struct array_type){.data=");

            if (ast->slice->offset != NULL) {
                printf("((char *)");
            }

            if (needs_temp_var(ast->slice->object)) {
                emit_temp_var(scope, ast->slice->object, 0);
            } else {
                compile_static_array(scope, ast->slice->object);
            }

            if (ast->slice->offset != NULL) {
                printf(")+(");
                compile(scope, ast->slice->offset);
                printf("*sizeof(");
                emit_type(r->array.inner);
                printf("))");
            }

            printf(",.length=");
            if (ast->slice->length != NULL) {
                compile(scope, ast->slice->length);
            } else {
                printf("%ld", r->array.length);
            }

            if (ast->slice->offset != NULL) {
                printf("-");
                compile(scope, ast->slice->offset);
            }
            printf("}");
        } else { // ARRAY
            printf("array_slice(");

            compile_unspecified_array(scope, ast->slice->object);

            printf(",");
            if (ast->slice->offset != NULL) {
                compile(scope, ast->slice->offset);
                printf(",sizeof(");
                emit_type(r->array.inner);
                printf("),");
            } else {
                printf("0,0,");
            }

            if (ast->slice->length != NULL) {
                compile(scope, ast->slice->length);
            } else {
                printf("-1");
            }
            printf(")");
        }
        break;
    }
    case AST_IDENTIFIER: {
        assert(!ast->ident->var->proxy);
        if (ast->ident->var->ext) {
            printf("_vs_%s", ast->ident->var->name);
        } else {
            printf("_vs_%d", ast->ident->var->id);
        }
        break;
    }
    case AST_RETURN: {
        Var *ret = NULL;
        if (ast->ret->expr != NULL) {
            emit_type(ast->ret->expr->var_type);
            printf("_ret = ");
            /*if (is_any(ast->ret->expr->var_type) && !is_any(ast->ret->expr->var_type)) {*/
                /*emit_any_wrapper(scope, ast->ret->expr);*/
            // TODO: if this doesn't actually need to be copied, we have to make
            // sure it isn't cleaned up on return
            if (is_lvalue(ast->ret->expr)) {
                emit_copy(scope, ast->ret->expr);
            } else {
                compile(scope, ast->ret->expr);
            }
            printf(";");

            // TODO: check the type, only do this for owned?
            if (ast->ret->expr->type == AST_IDENTIFIER) {
                ret = ast->ret->expr->ident->var;
            } if (ast->ret->expr->type == AST_CAST && ast->ret->expr->cast->object->type == AST_IDENTIFIER) {
                // do we even need this part?
                ret = ast->ret->expr->cast->object->ident->var;
            } else if (needs_temp_var(ast->ret->expr)) {
                ret = find_temp_var(scope, ast->ret->expr);
            }
        }
        printf("\n");
        emit_deferred(scope);

        // emit parent defers
        Scope *s = scope;
        while (s->parent != NULL) {
            emit_free_locals_except_return(s, ret);
            if (s->type == Function) {
                break;
            }
            // TODO: I don't think this will work if there are multiple scopes
            // nested and defers are added after a break
            for (int i = s->parent_deferred; i >= 0; --i) { // TODO: confirm direction is correct!
                Ast *d = s->parent->deferred[i];
                indent();
                if (needs_temp_var(d)) {
                    Var *v = find_temp_var(s, d);
                    printf("_tmp%d = ", v->id);
                }
                compile(s, d);
                printf(";\n");
            }
            s = s->parent;
        }

        /*emit_free_locals(scope);*/

        indent();
        printf("return");
        if (ast->ret->expr != NULL) {
            printf(" _ret");
        }
        break;
    }
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
        Type *lt = ast->index->object->var_type;
        if (lt->resolved->comp == ARRAY || lt->resolved->comp == STATIC_ARRAY) {
            printf("(");
            if (lt->resolved->comp == ARRAY) {
                printf("(");
                emit_type(lt->resolved->array.inner);
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
                // TODO: need string_as_array here?
                compile(scope, ast->index->object);
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
    case AST_ANON_SCOPE:
        emit_scope_start(ast->anon_scope->scope);
        compile_block(ast->anon_scope->scope, ast->anon_scope->body);
        emit_scope_end(ast->anon_scope->scope);
        break;
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
            emit_scope_start(ast->cond->else_scope);
            compile_block(ast->cond->else_scope, ast->cond->else_body);
            emit_scope_end(ast->cond->else_scope);
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

        if (ast->for_loop->index != NULL) {
            indent();
            emit_type(ast->for_loop->index->type);
            printf("_vs_%d;\n", ast->for_loop->index->id);
        }

        indent();
        printf("for (long _i = 0; _i < _iter.length; _i++) {\n");

        change_indent(1);
        indent();

        if (ast->for_loop->by_reference) {
            emit_type(ast->for_loop->itervar->type);
            printf("_vs_%d = ", ast->for_loop->itervar->id);
            printf("&");
            printf("((");
            emit_type(ast->for_loop->itervar->type);
            printf(")_iter.data)[_i];\n");
        } else {
            Type *t = ast->for_loop->itervar->type;
            emit_type(t);
            printf("_vs_%d = ", ast->for_loop->itervar->id);
            if (is_string(t)) {
                printf("copy_string");
            } else if (t->resolved->comp == STRUCT && is_dynamic(t)) {
                /*printf("_copy_%d", get_struct_type_id(t));*/
                printf("_copy_%d", t->id);
            }
            printf("(((");
            emit_type(t);
            printf("*)_iter.data)[_i]);\n");
        }

        if (ast->for_loop->index != NULL) {
            indent();
            emit_type(ast->for_loop->index->type);
            printf("_vs_%d = (", ast->for_loop->index->id);
            emit_type(ast->for_loop->index->type);
            printf(")_i;\n");
        }

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
        printf("((struct _type_vs_%d *)&_type_info%d)",
            get_typeinfo_type_id(),
            ast->typeinfo->typeinfo_target->id);
        break;
    case AST_TYPE_DECL:
        break;
    case AST_ENUM_DECL:
        break;
    case AST_IMPORT:
        break;
    default:
        error(ast->line, ast->file, "No idea how to deal with this.");
    }
}

void emit_scope_start(Scope *scope) {
    printf("{\n");
    change_indent(1);
    for (int i = 0; i < array_len(scope->temp_vars); i++) {
        Type *t = scope->temp_vars[i]->var->type;
        indent();
        if (t->resolved->comp == STATIC_ARRAY) {
            emit_type(t->resolved->array.inner);
            printf("_tmp%d[%ld]", scope->temp_vars[i]->var->id, t->resolved->array.length);
        } else {
            emit_type(t);
            printf("_tmp%d", scope->temp_vars[i]->var->id);
            if (t->resolved->comp == STRUCT || is_string(t)) {
                printf(" = {0}");
            }
        }
        printf(";\n");
    }
}

void open_block() {
    indent();
    printf("{\n");
    change_indent(1);
}

void close_block() {
    change_indent(-1);
    indent();
    printf("}\n");
}

void emit_free_struct(Scope *scope, char *name, Type *st, int is_ref) {
    char *sep = is_ref ? "->" : ".";
    ResolvedType *r = st->resolved;
    assert(r->comp == STRUCT);

    for (int i = 0; i < array_len(r->st.member_types); i++) {
        Type *member_type = r->st.member_types[i];
        ResolvedType *member_res = member_type->resolved;
        char *memname = malloc(sizeof(char) * (strlen(name) + strlen(sep) + strlen(r->st.member_names[i]) + 2));
        sprintf(memname, "%s%s%s", name, sep, r->st.member_names[i]);

        if (is_string(member_type)) {
            indent();
            printf("free(%s.bytes);\n", memname);
        } else if (member_res->comp == STRUCT) {
            int ref = (member_res->comp == REF || (member_res->comp == BASIC && member_res->data->base == BASEPTR_T));
            emit_free_struct(scope, memname, r->st.member_types[i], ref);
        } else if (member_res->comp == ARRAY && member_res->array.owned) {
            if (is_dynamic(member_res->array.inner)) {
                open_block();

                indent();
                emit_type(member_res->array.inner);
                printf("*_0 = %s.data;\n", memname);

                indent();
                printf("for (int i = 0; i < %s.length; i++) {\n", memname);

                change_indent(1);
                indent();
                emit_type(member_res->array.inner);

                Var *v = make_var("<i>", member_res->array.inner);
                v->initialized = 1;
                printf("_vs_%d = _0[i];\n", v->id);

                emit_free(scope, v);
                free(v);

                close_block();
                close_block();
            }
            indent();
            printf("free(%s.data);\n", memname);
        } else if (member_res->comp == REF && member_res->ref.owned) {
            Type *inner = member_res->ref.inner;

            if (inner->resolved->comp == STRUCT) {
                emit_free_struct(scope, memname, inner, 1);
            } else if (is_string(inner)) { // TODO should this behave this way?
                indent();
                printf("free(%s->bytes);\n", memname);
            }

            indent();
            printf("free(%s);\n", memname);
        }
        free(memname);
    }
}

void emit_free(Scope *scope, Var *var) {
    char *name_fmt = var->temp ? "_tmp%d" : "_vs_%d";
    ResolvedType *r = var->type->resolved;
    if (r->comp == REF) {
        if (r->ref.owned) {
            Type *inner = r->ref.inner;
            if (inner->resolved->comp == STRUCT) {
                int len = snprintf(NULL, 0, name_fmt, var->id);
                char *name = malloc(sizeof(char) * (len + 1));
                snprintf(name, len+1, name_fmt, var->id);
                emit_free_struct(scope, name, inner, 1);
                free(name);
                indent();
                printf("free(");
                printf(name_fmt, var->id);
                printf(");\n");
            } else if (is_string(inner)) { // TODO should this behave this way?
                indent();
                printf("free(");
                printf(name_fmt, var->id);
                printf("->bytes);\n");
            }
        }
    } else if (r->comp == STATIC_ARRAY) {
        if (is_dynamic(r->array.inner)) {
            open_block();

            indent();
            emit_type(r->array.inner);
            printf("*_0 = ");
            printf(name_fmt, var->id);
            printf(";\n");

            indent();
            printf("for (int i = 0; i < %ld; i++) {\n", r->array.length);

            change_indent(1);
            indent();
            emit_type(r->array.inner);

            Var *v = make_var("<i>", r->array.inner);
            v->initialized = 1;
            printf("_vs_%d = _0[i];\n", v->id);

            emit_free(scope, v);
            free(v);

            close_block();
            close_block();
        }
    } else if (r->comp == ARRAY) {
        if (r->array.owned) {
            if (is_dynamic(r->array.inner)) {
                open_block();

                indent();
                emit_type(r->array.inner);
                printf("*_0 = ");
                printf(name_fmt, var->id);
                printf(".data;\n");

                indent();
                printf("for (int i = 0; i < ");
                printf(name_fmt, var->id);
                printf(".length; i++) {\n");

                change_indent(1);
                indent();
                emit_type(r->array.inner);

                Var *v = make_var("<i>", r->array.inner);
                v->initialized = 1;
                printf("_vs_%d = _0[i];\n", v->id);

                emit_free(scope, v);
                free(v);

                close_block();
                close_block();
            }
            printf("free(");
            printf(name_fmt, var->id);
            printf(".data);\n");
        }
    } else if (r->comp == STRUCT) {
        char *name;
        name = malloc(sizeof(char) * (snprintf(NULL, 0, "%d", var->id) + 5));
        sprintf(name, name_fmt, var->id);
        emit_free_struct(scope, name, var->type, 0);
        free(name);
    } else if (r->comp == BASIC) {
        if (r->data->base == STRING_T) {
            indent();
            printf("free(");
            printf(name_fmt, var->id);
            printf(".bytes);\n");
        }
    }
}

void emit_free_locals_except_return(Scope *scope, Var *returned) {
    for (int i = array_len(scope->vars)-1; i >= 0; --i) {
        Var *v = scope->vars[i];
        if (v->proxy ||
           (returned && (v == returned || v->id == returned->id))) {
            continue;
        }
        // There are extra tempvars being created that aren't being used, and
        // uninitialized memory is being freed. Do this to avoid the issue for
        // now. This shouldn't be a problem if we change the way tempvars work
        // and/or move to a new backend that doesn't have the problems C does.
        if (v->temp && !v->initialized) {
            continue;
        }
        emit_free(scope, v);
    }
}

void emit_free_locals(Scope *scope) {
    /*for (int i = 0; i < array_len(scope->vars); i++) {*/
    for (int i = array_len(scope->vars)-1; i >= 0; --i) {
        Var *v = scope->vars[i];
        if (v->proxy) {
            continue;
        }
        // TODO got to be a better way to handle this here
        /*if (!v->initialized || t->comp == FUNC || t->comp == REF ||*/
           /*(t->comp == BASIC && t->data->base == BASEPTR_T)) {*/
            /*continue;*/
        /*}*/
        emit_free(scope, v);
    }
}

void emit_free_temp(Scope *scope) {
    /*for (int i = 0; i < array_len(scope->vars); i++) {*/
    for (int i = array_len(scope->vars)-1; i >= 0; --i) {
        Var *v = scope->vars[i];
        ResolvedType *r = v->type->resolved;

        // TODO got to be a better way to handle this here
        if (!v->temp || !v->initialized ||
            r->comp == FUNC || (r->comp == BASIC && r->data->base == BASEPTR_T)) {
            continue;
        }
        emit_free(scope, v);
    }
}

void emit_init_scope_end(Scope *scope) {
    emit_free_temp(scope);
    change_indent(-1);
    indent();
    printf("}\n");
}

void emit_scope_end(Scope *scope) {
    if (!scope->has_return) {
        emit_deferred(scope);
        emit_free_locals(scope);
    }
    change_indent(-1);
    indent();
    printf("}\n");
}

void emit_deferred(Scope *scope) {
    for (int i = array_len(scope->deferred)-1; i >= 0; --i) {
        Ast *d = scope->deferred[i];
        indent();
        if (needs_temp_var(d)) {
            Var *v = find_temp_var(scope, d);
            printf("_tmp%d = ", v->id);
        }
        compile(scope, d);
        printf(";\n");
    }
}

void emit_extern_fn_decl(Scope *scope, Var *v) {
    printf("extern ");

    ResolvedType *r = v->type->resolved;
    emit_type(r->fn.ret);
    printf("%s(", v->name);
    for (int i = 0; i < array_len(r->fn.args); i++) {
        if (i > 0) {
            printf(",");
        }
        emit_type(r->fn.args[i]);
    }
    printf(");\n");

    emit_type(r->fn.ret);
    printf("(*_vs_%s)(", v->name);

    for (int i = 0; i < array_len(r->fn.args); i++) {
        if (i > 0) {
            printf(",");
        }
        emit_type(r->fn.args[i]);
    }
    printf(") = %s;\n", v->name);
}

void emit_var_decl(Scope *scope, Var *v) {
    if (v->ext) {
        emit_extern_fn_decl(scope, v);
        return;
    }

    ResolvedType *r = v->type->resolved;
    if (r->comp == FUNC) {
        emit_type(r->fn.ret);
        printf("(*");
    } else if (r->comp == STATIC_ARRAY) {
        emit_type(r->array.inner);
        printf("_vs_%d[%ld] = {0};\n", v->id, r->array.length);
        return;
    } else {
        emit_type(v->type);
    }

    printf("_vs_%d", v->id);
    if (r->comp == FUNC) {
        printf(")(");
        for (int i = 0; i < array_len(r->fn.args); i++) {
            if (i > 0) {
                printf(",");
            }
            emit_type(r->fn.args[i]);
            printf("a%d", i);
        }
        printf(")");
    }
    printf(";\n");
}

void emit_forward_decl(Scope *scope, AstFnDecl *decl) {
    if (decl->polymorphs != NULL) {
        scope = decl->scope;
        for (int i = 0; i < array_len(decl->polymorphs); i++) {
            Polymorph *p = decl->polymorphs[i];
            scope->polymorph = p;

            printf("/* %s */\n", decl->var->name);
            // TODO: integrate var with polymorph specializations so this works
            //  better/more naturally here

            ResolvedType *r = decl->var->type->resolved;
            assert(r->comp == FUNC);
            emit_type(p->ret);

            printf("_poly_%d_vs_%d(", p->id, decl->var->id);

            for (int i = 0; i < array_len(decl->args); i++) {
                if (i > 0) {
                    printf(",");
                }
                int type_index = (i >= array_len(p->args)) ? array_len(p->args)-1 : i;
                if (r->fn.variadic && i == (array_len(decl->args) - 1)) {
                    printf("struct array_type ");
                } else {
                    emit_type(p->args[type_index]);
                }
                printf("_vs_%d", decl->args[i]->id);
            }
            printf(");\n");
            scope->polymorph = NULL;
        }
    } else {
        ResolvedType *r = decl->var->type->resolved;
        if (is_polydef(decl->var->type)) {
            // polymorph not being used
            return;
        }
        printf("/* %s */\n", decl->var->name);
        if (decl->var->ext) {
            printf("extern ");
        }
        assert(r->comp == FUNC);
        emit_type(r->fn.ret);

        printf("_vs_");
        /*if (decl->var->ext) {*/
            // Does this need to change?
            /*printf("%s(", decl->var->name);*/
        /*} else {*/
            printf("%d(", decl->var->id);
        /*}*/

        for (int i = 0; i < array_len(r->fn.args); i++) {
            if (i > 0) {
                printf(",");
            }
            if (r->fn.variadic && i == (array_len(r->fn.args) - 1)) {
                printf("struct array_type ");
            } else {
                emit_type(r->fn.args[i]);
            }
            printf("a%d", i);
        }
        printf(");\n");
    }
}
