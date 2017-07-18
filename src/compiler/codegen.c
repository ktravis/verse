#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "array/array.h"
#include "typechecking.h"
#include "codegen.h"
#include "parse.h"
#include "scope.h"

static int _indent = 0;
static int _static_array_copy_depth = 0;

static FILE *output;

void codegen_set_output(FILE *f) {
    output = f;
}

#define write_fmt(...) fprintf(output, __VA_ARGS__)

int write_bytes(const char *b, ...) {
    va_list args;
    va_start(args, b);
    int ret = vfprintf(output, b, args);
    va_end(args);
    return ret;
}

void indent() {
    for (int i = 0; i < _indent; i++) {
        write_fmt("    ");
    }
}

void change_indent(int n) {
    _indent += n;
    if (_indent < 0) {
        _indent = 0;
    }
}

void emit_entrypoint() {
    write_fmt("\nint main(int argc, char** argv) {\n"
              "    _verse_init_typeinfo();\n"
              "    return _verse_init();\n}\n");
}

void emit_init_routine(Package **packages, Scope *root_scope, Ast *root, Var *main_var) {
    write_fmt("int _verse_init() {\n");
    change_indent(1);

    for (int i = 0; i < array_len(packages); i++) {
        Package *p = packages[i];
        emit_scope_start(p->scope);
        for (int j = 0; j < array_len(p->files); j++) {
            compile(p->scope, p->files[j]->root);
        }
        emit_init_scope_end(p->scope);
    }

    emit_scope_start(root_scope);
    compile(root_scope, root);
    emit_scope_end(root_scope);

    if (main_var != NULL) {
        write_fmt("    return _vs_%d();\n}", main_var->id);
    } else {
        write_fmt("    return 0;\n}");
    }
}

void recursively_declare_types(int *declared_type_ids, Scope *root_scope, Type *t) {
    if (t->resolved->comp != STRUCT) {
        return;
    }
    for (int i = 0; i < array_len(declared_type_ids); i++) {
        if (t->id == declared_type_ids[i]) {
            return;
        }
    }
    array_push(declared_type_ids, t->id);

    if (t->resolved->comp == STRUCT) {
        for (int i = 0; i < array_len(t->resolved->st.member_types); i++) {
            recursively_declare_types(declared_type_ids, root_scope, t->resolved->st.member_types[i]);
        }
        emit_struct_decl(root_scope, t);
    }
}

void emit_typeinfo_init_routine(Scope *root_scope, Type **builtins, Type **used_types) {
    int *initted_type_ids = NULL;
    write_fmt("void _verse_init_typeinfo() {\n");
    change_indent(1);
    for (int i = 0; i < array_len(builtins); i++) {
        emit_typeinfo_init(root_scope, builtins[i]);
    }
    for (int i = 0; i < array_len(used_types); i++) {
        char skip = 0;
        for (int j = 0; j < array_len(initted_type_ids); j++) {
            if (used_types[i]->id == initted_type_ids[j]) {
                skip = 1;
                break;
            }
        }
        if (!skip) {
            array_push(initted_type_ids, used_types[i]->id);
            emit_typeinfo_init(root_scope, used_types[i]);
        }
    }
    change_indent(-1);
    write_fmt("}\n");
    array_free(initted_type_ids);
}

void emit_typeinfo_decl(Scope *scope, Type *t) {
    int id = t->id;
    int typeinfo_type_id = get_typeinfo_type_id();
    assert(t->resolved);

    ResolvedType *r = t->resolved;
    int base_id = get_basetype_id(r->comp);

    switch (r->comp) {
    case STRUCT:
        write_fmt("struct _type_vs_%d _type_info%d_members[%d];\n", get_structmember_type_id(), id, array_len(r->st.member_types));
        break;
    case FUNC:
        write_fmt("struct _type_vs_%d *_type_info%d_args[%d];\n", typeinfo_type_id, id, array_len(r->fn.args));
        break;
    case ENUM:
        write_fmt("struct string_type _type_info%d_members[%d] = {\n", id, array_len(r->en.member_values));
        for (int i = 0; i < array_len(r->en.member_names); i++) {
            write_fmt("  {%ld, \"%s\"},\n", strlen(r->en.member_names[i]), r->en.member_names[i]);
        }
        write_fmt("};\n");
        write_fmt("int64_t _type_info%d_values[%d] = {\n", id, array_len(r->en.member_values));
        for (int i = 0; i < array_len(r->en.member_values); i++) {
            write_fmt(" %ld,\n", r->en.member_values[i]);
        }
        write_fmt("};\n");
        break;
    case BASIC:
        switch (r->data->base) {
        case INT_T:
        case UINT_T:
        case FLOAT_T:
            base_id = get_numtype_type_id();
            break;
        case BASEPTR_T:
        case STRING_T:
        case BOOL_T:
            break;
        }
    default:
        break;
    }
    write_fmt("struct _type_vs_%d _type_info%d;\n", base_id, id);
}

void indent();

void emit_string_struct(char *str) {
    write_fmt("{%ld,\"", strlen(str));
    print_quoted_string(output, str);
    write_fmt("\"}");
}

void emit_typeinfo_init(Scope *scope, Type *t) {
    int id = t->id;
    int typeinfo_type_id = get_typeinfo_type_id();

    char *name = type_to_string(t); // eh?
    ResolvedType *r = t->resolved;
    int base_id = get_basetype_id(r->comp);

    switch (r->comp) {
    case ENUM:
        indent();
        write_fmt("_type_info%d = (struct _type_vs_%d){%d, 9, ", id, base_id, id);
        emit_string_struct(name);
        write_fmt(", (struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, r->en.inner->id);
        write_fmt("{%d, _type_info%d_members}, {%d, _type_info%d_values}};\n",
                array_len(r->en.member_names), id, array_len(r->en.member_names), id);
        break;
    case REF:
        indent();
        write_fmt("_type_info%d = (struct _type_vs_%d){%d, 10, ", id, base_id, id);
        emit_string_struct(name);
        write_fmt(", %d, (struct _type_vs_%d *)&_type_info%d};\n", r->ref.owned, typeinfo_type_id, r->ref.inner->id);
        break;
    case STRUCT: {
        int offset = 0;
        for (int i = 0; i < array_len(r->st.member_names); i++) {
            indent();
            write_fmt("_type_info%d_members[%d] = (struct _type_vs_%d){", id, i, get_structmember_type_id());
            emit_string_struct(r->st.member_names[i]);
            write_fmt(", (struct _type_vs_%d *)&_type_info%d, %d};\n",
                   typeinfo_type_id, r->st.member_types[i]->id, offset);
            offset += size_of_type(r->st.member_types[i]);
        }
        indent();
        write_fmt("_type_info%d = (struct _type_vs_%d){%d, 11, ", id, base_id, id);
        emit_string_struct(name);
        write_fmt(", {%d, _type_info%d_members}};\n", array_len(r->st.member_names), id);
        break;
    }
    case STATIC_ARRAY:
        indent();
        write_fmt("_type_info%d = (struct _type_vs_%d){%d, 7, ", id, base_id, id);
        emit_string_struct(name);
        write_fmt(", (struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, r->array.inner->id);
        write_fmt("%ld, %d, %d};\n", r->array.length, 1, 0);
        break;
    case ARRAY: // TODO make this not have a name? switch Type to have enum in name slot for base type
        indent();
        write_fmt("_type_info%d = (struct _type_vs_%d){%d, 7, ", id, base_id, id);
        emit_string_struct(name);
        write_fmt(", (struct _type_vs_%d *)&_type_info%d, %ld, %d, %d};\n",
                typeinfo_type_id, r->array.inner->id, (long)0, 0, r->array.owned);
        break;
    case FUNC: {
        for (int i = 0; i < array_len(r->fn.args); i++) {
            indent();
            write_fmt("_type_info%d_args[%d] = (struct _type_vs_%d *)&_type_info%d;\n",
                id, i, typeinfo_type_id, r->fn.args[i]->id);
        }
        indent();
        write_fmt("_type_info%d = (struct _type_vs_%d){%d, 8, ", id, base_id, id);
        emit_string_struct(name);
        write_fmt(", {%d, (struct _type_vs_%d **)_type_info%d_args}, ", array_len(r->fn.args), typeinfo_type_id, id);

        Type *ret = r->fn.ret;
        if (ret->resolved->comp == BASIC && ret->resolved->data->base == VOID_T) {
            write_fmt("NULL, ");
        } else {
            write_fmt("(struct _type_vs_%d *)&_type_info%d, ", typeinfo_type_id, r->fn.ret->id);
        }
        write_fmt("0};\n");
        break;
    }
    case BASIC: {
        switch (r->data->base) {
        case INT_T:
        case UINT_T:
            indent();
            write_fmt("_type_info%d = (struct _type_vs_%d){%d, 1, ", id, get_numtype_type_id(), id);
            emit_string_struct(name);
            write_fmt(", %d, %d};\n", r->data->size, r->data->base == INT_T);
            break;
        case FLOAT_T:
            indent();
            write_fmt("_type_info%d = (struct _type_vs_%d){%d, 3, ", id, get_numtype_type_id(), id);
            emit_string_struct(name);
            write_fmt(", %d, 1};\n", r->data->size);
            break;
        case BASEPTR_T:
            indent();
            write_fmt("_type_info%d = (struct _type_vs_%d){%d, 12, ", id, typeinfo_type_id, id);
            emit_string_struct(name);
            write_fmt("};\n");
            break;
        case STRING_T:
            indent();
            write_fmt("_type_info%d = (struct _type_vs_%d){%d, 6, ", id, typeinfo_type_id, id);
            emit_string_struct(name);
            write_fmt("};\n");
            break;
        case BOOL_T:
            indent();
            write_fmt("_type_info%d = (struct _type_vs_%d){%d, 2, ", id, typeinfo_type_id, id);
            emit_string_struct(name);
            write_fmt("};\n");
            break;
        }
    }
    default:
        break;
    }
}

static Type **struct_types;

void emit_temp_var(Scope *scope, Ast *ast, int ref) {
    Var *v = find_temp_var(scope, ast);
    v->initialized = 1;
    write_fmt("(_tmp%d = ", v->id);

    ResolvedType *r = v->type->resolved;
    if (r->comp == REF) {
        Type *inner = r->ref.inner;
        if (inner->resolved->comp == STATIC_ARRAY) {
            write_fmt("(");
            emit_type(inner);
            write_fmt("*)");
        }
    }
    compile(scope, ast);
    write_fmt(", %s_tmp%d)", ref ? "&" : "", v->id);
}

void emit_string_comparison(Scope *scope, Ast *ast) {
    AstBinaryOp *bin = ast->binary;
    if (bin->op == OP_NEQUALS) {
        write_fmt("!");
    } else if (bin->op != OP_EQUALS) {
        error(ast->line, ast->file, "Comparison of type '%s' is not valid for type 'string'.", op_to_str(bin->op));
    }
    if (bin->left->type == AST_LITERAL && bin->right->type == AST_LITERAL) {
        write_fmt("%d", strcmp(bin->left->lit->string_val, bin->right->lit->string_val) ? 0 : 1);
        return;
    }
    if (bin->left->type == AST_LITERAL) {
        write_fmt("streq_lit(");
        compile(scope, bin->right);
        write_fmt(",\"");
        print_quoted_string(output, bin->left->lit->string_val);
        write_fmt("\",%d)", escaped_strlen(bin->left->lit->string_val));
    } else if (bin->right->type == AST_LITERAL) {
        write_fmt("streq_lit(");
        compile(scope, bin->left);
        write_fmt(",\"");
        print_quoted_string(output, bin->right->lit->string_val);
        write_fmt("\",%d)", escaped_strlen(bin->right->lit->string_val));
    } else {
        write_fmt("streq(");
        compile(scope, bin->left);
        write_fmt(",");
        compile(scope, bin->right);
        write_fmt(")");
    }
}

void emit_comparison(Scope *scope, Ast *ast) {
    Type *t = ast->binary->left->var_type;
    if (is_string(t)) {
        emit_string_comparison(scope, ast);
        return;
    }
    write_fmt("(");
    compile(scope, ast->binary->left);
    write_fmt(" %s ", op_to_str(ast->binary->op));
    compile(scope, ast->binary->right);
    write_fmt(")");
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
        write_fmt("append_string(");
        compile(scope, ast->binary->left);
        write_fmt(",");
        compile(scope, ast->binary->right);
        write_fmt(")");
        break;
    case AST_LITERAL:
        write_fmt("append_string_lit(");
        compile(scope, ast->binary->left);
        write_fmt(",\"");
        print_quoted_string(output, ast->binary->right->lit->string_val);
        write_fmt("\",%d)", (int) escaped_strlen(ast->binary->right->lit->string_val));
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
        write_fmt("init_string(\"");
        print_quoted_string(output, s);
        write_fmt("\", %d)", (int)strlen(s));
        return;
    }

    if (t->resolved->comp == STATIC_ARRAY) {
        if (!strcmp(ast->dot->member_name, "length")) {
            write_fmt("%ld", t->resolved->array.length);
        } else if (!strcmp(ast->dot->member_name, "data")) {
            compile(scope, ast->dot->object);
        }
    } else {
        compile(scope, ast->dot->object);
        if (t->resolved->comp == REF) {
            write_fmt("->%s", ast->dot->member_name);
        } else {
            write_fmt(".%s", ast->dot->member_name);
        }
    }
}

void emit_uop(Scope *scope, Ast *ast) {
    switch (ast->unary->op) {
    case OP_NOT:
        write_fmt("!"); break;
    case OP_REF:
        write_fmt("&"); break;
    case OP_DEREF:
        write_fmt("*"); break;
    case OP_MINUS:
        write_fmt("-"); break;
    case OP_PLUS:
        write_fmt("+"); break;
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
        write_fmt("{\n");
        change_indent(1);
        indent();

        emit_type(lt);
        write_fmt("l = ");
        compile_static_array(scope, l);
        write_fmt(";\n");
        indent();

        emit_type(lt);
        write_fmt("r = ");
        compile_static_array(scope, r);
        write_fmt(";\n");
        indent();

        emit_static_array_copy(scope, lt, "l", "r");
        write_fmt(";\n");

        change_indent(-1);
        indent();
        write_fmt("}");
        return;
    }

    if (is_dynamic(lt) || r->type == AST_NEW) {
        if (l->type == AST_IDENTIFIER &&
                !l->ident->var->initialized && !is_owned(lt)) {
            write_fmt("_vs_%d = ", l->ident->var->id);
            if (is_lvalue(r)) {
                emit_copy(scope, r);
            } else {
                compile(scope, r);
            }

            l->ident->var->initialized = 1;
        } else {
            Var *temp = find_temp_var(scope, r);
            write_fmt("_tmp%d = ", temp->id);
            temp->initialized = 1;
            if (is_lvalue(r)) {
                emit_copy(scope, r);
            } else {
                compile(scope, r);
            }
            write_fmt(";\n");
            indent();
            write_fmt("SWAP(");

            if (l->type == AST_DOT || l->type == AST_INDEX || l->type == AST_UOP) {
                compile(scope, l);
            } else {
                write_fmt("_vs_%d", l->ident->var->id);
            }
            write_fmt(",_tmp%d)", temp->id);
        }
    } else {
        compile(scope, l);
        write_fmt(" = ");
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
        write_fmt("(");
        compile(scope, ast->binary->left);
        write_fmt(") || (");
        compile(scope, ast->binary->right);
        write_fmt(")");
        return;
    } else if (ast->binary->op == OP_AND) {
        write_fmt("(");
        compile(scope, ast->binary->left);
        write_fmt(") && ("); // does this short-circuit?
        compile(scope, ast->binary->right);
        write_fmt(")");
        return;
    }
    write_fmt("(");
    compile(scope, ast->binary->left);
    write_fmt(" %s ", op_to_str(ast->binary->op));
    compile(scope, ast->binary->right);
    write_fmt(")");
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
        write_fmt("copy_string(");
        compile(scope, ast);
        write_fmt(")");
    } else if (t->resolved->comp == STRUCT) {
        write_fmt("_copy_%d(", t->id);
        compile(scope, ast);
        write_fmt(")");
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
            write_fmt("u");
        case INT_T:
            write_fmt("int%d_t ", r->data->size * 8);
            break;
        case FLOAT_T:
            if (r->data->size == 4) { // TODO double-check these are always the right size
                write_fmt("float ");
            } else if (r->data->size == 8) {
                write_fmt("double ");
            } else {
                error(-1, "internal", "Cannot compile floating-point type of size %d.", r->data->size);
            }
            break;
        case BOOL_T:
            write_fmt("unsigned char ");
            break;
        case STRING_T:
            write_fmt("struct string_type ");
            break;
        case VOID_T:
            write_fmt("void ");
            break;
        case BASEPTR_T:
            write_fmt("ptr_type ");
            break;
        }
        break;
    case FUNC:
        write_fmt("fn_type ");
        break;
    case REF:
        emit_type(r->ref.inner);
        write_fmt("*");
        break;
    case ARRAY:
        write_fmt("struct array_type ");
        break;
    case STATIC_ARRAY:
        emit_type(r->array.inner);
        write_fmt("*");
        break;
    case STRUCT:
        write_fmt("struct _type_vs_%d ", type->id);
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
        write_fmt("string_as_array(");
        compile(scope, ast);
        write_fmt(")");
        return;
    }
    TypeComp c = ast->var_type->resolved->comp;
    if (c == ARRAY) {
        compile(scope, ast);
    } else if (c == STATIC_ARRAY) {
        write_fmt("(struct array_type){.data=");
        compile(scope, ast);
        write_fmt(",.length=%ld}", ast->var_type->resolved->array.length);
    } else {
        error(ast->line, ast->file, "Was expecting an array of some kind here, man.");
    }
}

void compile_static_array(Scope *scope, Ast *ast) {
    if (ast->var_type->resolved->comp == STATIC_ARRAY) {
        compile(scope, ast);
    } else if (ast->var_type->resolved->comp == ARRAY) {
        write_fmt("(");
        compile(scope, ast);
        write_fmt(").data");
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
        write_fmt(" = {0}");
    } else if (ast->decl->init->type == AST_LITERAL) {
        assert(ast->decl->init->lit->lit_type == ARRAY_LIT);

        write_fmt(" = {");
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
                write_fmt(",");
            }
        }
        write_fmt("}");
    } else {
        write_fmt(";\n");
        indent();
        write_fmt("{\n");
        change_indent(1);
        indent();
        emit_type(ast->decl->var->type);
        write_fmt("_0 = ");
        compile_static_array(scope, ast->decl->init);
        write_fmt(";\n");
        indent();

        char *dname = malloc(sizeof(char) * (snprintf(NULL, 0, "_vs_%d", ast->decl->var->id) + 1));
        sprintf(dname, "_vs_%d", ast->decl->var->id);
        dname[strlen(ast->decl->var->name) + 5] = 0;
        emit_static_array_copy(scope, ast->decl->var->type, dname, "_0");
        write_fmt(";\n");
        free(dname);

        change_indent(-1);
        indent();
        write_fmt("}");
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
    write_fmt("_vs_%d", ast->decl->var->id);
    if (ast->decl->init == NULL) {
        if (c == BASIC) {
            int b = t->resolved->data->base;
            if (b == STRING_T) {
                write_fmt(" = (struct string_type){0}");
                ast->decl->var->initialized = 1;
            } else if (b == INT_T || b == UINT_T || b == FLOAT_T) {
                write_fmt(" = 0");
            } else if (b == BASEPTR_T) {
                write_fmt(" = NULL");
            }
        } else if (c == STRUCT) {
            write_fmt(";\n");
            indent();
            write_fmt("_init_%d(&_vs_%d)", t->id, ast->decl->var->id);
            ast->decl->var->initialized = 1;
        } else if (c == REF)  {
            write_fmt(" = NULL");
        } else if (c == ARRAY) {
            write_fmt(" = {0}");
        }
    } else {
        write_fmt(" = ");
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
    if (is_polydef(fn_type)) {
        // polymorph not being used
        return;
    }
    if (fn->fn_decl->polymorph_of) {
        write_fmt("/* polymorph %s of %s */\n", type_to_string(fn->fn_decl->var->type), fn->fn_decl->polymorph_of->var->name);
    } else {
        write_fmt("/* %s */\n", fn->fn_decl->var->name);
    }
    indent();
    emit_type(r->fn.ret);

    assert(!fn->fn_decl->var->ext);
    write_fmt("_vs_%d(", fn->fn_decl->var->id);

    Var **args = fn->fn_decl->args;
    int nargs = array_len(args);
    for (int i = 0; i < nargs; i++) {
        if (i > 0) {
            write_fmt(",");
        }
        if (r->fn.variadic && i == (nargs - 1)) {
            write_fmt("struct array_type ");
        } else {
            emit_type(args[i]->type);
        }
        write_fmt("_vs_%d", args[i]->id);
    }
    write_fmt(") ");

    emit_scope_start(fn->fn_decl->scope);
    compile_block(fn->fn_decl->scope, fn->fn_decl->body);
    emit_scope_end(fn->fn_decl->scope);
}

void emit_structmember(Scope *scope, char *name, Type *st) {
    if (st->resolved->comp == STATIC_ARRAY) {
        emit_structmember(scope, name, st->resolved->array.inner);
        long length = 0;
        while (st->resolved->comp == STATIC_ARRAY) {
            length += st->resolved->array.length;
            st = st->resolved->array.inner;
        }
        write_fmt("[%ld]", length);
    } else {
        emit_type(st);
        write_fmt("%s", name);
    }
}

void emit_static_array_copy(Scope *scope, Type *t, char *dest, char *src) {
    Type *inner = t->resolved->array.inner;
    if (!is_dynamic(t)) {
        write_fmt("memcpy(%s, %s, sizeof(", dest, src);
        emit_type(inner);
        write_fmt(") * %ld)", t->resolved->array.length);
        return;
    }

    int d = _static_array_copy_depth++;
    write_fmt("{\n");
    change_indent(1);
    indent();

    emit_type(inner);
    write_fmt("d%d;\n", d);
    indent();
    emit_type(inner);
    write_fmt("s%d;\n", d);
    indent();

    write_fmt("for (int i = 0; i < %ld; i++) {\n", t->resolved->array.length);
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

        write_fmt("%s = %s[i], %s = %s[i];\n", dname, dest, sname, src);
        emit_static_array_copy(scope, t->resolved->array.inner, dname, sname);

        free(dname);
        free(sname);
    } else if (inner->resolved->comp == STRUCT) {
        write_fmt("%s[i] = _copy_%d(%s[i])", dest, inner->id, src);
    } else if (is_string(inner)) {
        write_fmt("%s[i] = copy_string(%s[i])", dest, src);
    } else {
        write_fmt("%s[i] = %s[i]", dest, src);
    }
    write_fmt(";\n"); // TODO move this?

    change_indent(-1);
    indent();
    write_fmt("}\n");

    change_indent(-1);
    indent();
    write_fmt("}\n");
    _static_array_copy_depth--;
}

void emit_struct_decl(Scope *scope, Type *st) {
    ResolvedType *r = st->resolved;
    assert(r->comp == STRUCT);

    for (int i = 0; i < array_len(struct_types); i++) {
        if (struct_types[i]->id == st->id) {
            return;
        }
    }
    array_push(struct_types, st);

    emit_type(st);
    write_fmt("{\n");

    change_indent(1);
    for (int i = 0; i < array_len(r->st.member_names); i++) {
        indent();
        emit_structmember(scope, r->st.member_names[i], r->st.member_types[i]);
        write_fmt(";\n");
    }

    change_indent(-1);
    indent();
    write_fmt("};\n");

    emit_type(st);
    write_fmt("*_init_%d(", st->id);

    emit_type(st);
    write_fmt("*x) {\n");

    change_indent(1);
    indent();

    write_fmt("if (x == NULL) {\n");

    change_indent(1);
    indent();

    write_fmt("x = malloc(sizeof(");
    emit_type(st);
    write_fmt("));\n");

    change_indent(-1);
    indent();
    write_fmt("}\n");

    indent();
    write_fmt("memset(x, 0, sizeof(");
    emit_type(st);
    write_fmt("));\n");

    /*for (int i = 0; i < r->st.nmembers; i++) {*/
        /*Type *t = r->st.member_types[i];*/
        /*if (t->comp == REF && t->ref.owned) {*/
            /*indent();*/
            /*write_fmt("x->%s = calloc(sizeof(", r->st.member_names[i]);*/
            /*emit_type(t->ref.inner);*/
            /*write_fmt("), 1);\n");*/
        /*}*/
    /*}*/

    indent();
    write_fmt("return x;\n");
    change_indent(-1);
    indent();
    write_fmt("}\n");

    emit_type(st);
    write_fmt("_copy_%d(", st->id);

    emit_type(st);
    write_fmt("x) {\n");

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
            write_fmt(";\n");
            free(member);
        } else if (is_string(member_type)) {
            indent();
            write_fmt("x.%s = copy_string(x.%s);\n", member_name, member_name);
        } else if (rm->comp == STRUCT) {
            indent();
            write_fmt("x.%s = _copy_%d(x.%s);\n", member_name,
                    member_type->id, member_name);
        } else if (rm->comp == REF && rm->ref.owned) {
            indent();
            emit_type(rm->ref.inner);
            if (is_string(rm->ref.inner)) {
                write_fmt("tmp%d = copy_string(*x.%s);\n", i, member_name);
            } else if (rm->ref.inner->resolved->comp == STRUCT) {
                write_fmt("tmp%d = _copy_%d(*x.%s);\n", i, rm->ref.inner->id, member_name);
            } else {
                write_fmt("tmp%d = *x.%s;\n", i, member_name);
            }
            indent();
            write_fmt("x.%s = malloc(sizeof(", member_name);
            emit_type(r->st.member_types[i]);
            write_fmt("));\n");
            indent();
            write_fmt("*x.%s = tmp%d;\n", member_name, i);
        }
    }
    indent();
    write_fmt("return x;\n");

    change_indent(-1);
    indent();
    write_fmt("}\n");
}

void compile_ref(Scope *scope, Ast *ast) {
    assert(is_lvalue(ast));

    write_fmt("&");
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
            write_fmt("_tmp%d = ", v->id);
        }
        compile(scope, stmt);

        if (stmt->type != AST_CONDITIONAL && stmt->type != AST_WHILE &&
            stmt->type != AST_FOR && stmt->type != AST_BLOCK &&
            stmt->type != AST_ANON_SCOPE && stmt->type != AST_IMPORT &&
            stmt->type != AST_TYPE_DECL && stmt->type != AST_ENUM_DECL) {
            write_fmt(";\n");
        }
    }
}

void emit_any_wrapper(Scope *scope, Ast *ast) {
    write_fmt("(struct _type_vs_%d){.value_pointer=", get_any_type_id());
    if (is_lvalue(ast)) {
        compile_ref(scope, ast);
    } else {
        emit_temp_var(scope, ast, 1);
    }
    write_fmt(",.type=(struct _type_vs_%d *)&_type_info%d}", get_typeinfo_type_id(), ast->var_type->id);
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

    if (needs_wrapper) {
        write_fmt("((");
        emit_type(r->fn.ret);
        write_fmt("(*)(");
        if (array_len(r->fn.args) == 0) {
            write_fmt("void");
        } else {
            for (int i = 0; i < array_len(argtypes); i++) {
                if (i > 0) {
                    write_fmt(",");
                }
                emit_type(argtypes[i]);
            }
        }
        write_fmt("))(");
    }

    if (needs_temp_var(ast->call->fn)) {
        emit_temp_var(scope, ast->call->fn, 0);
    } else {
        compile(scope, ast->call->fn);
    }

    if (needs_wrapper) {
        write_fmt("))");
    }

    write_fmt("(");

    TempVar *vt = ast->call->variadic_tempvar;
    int type_index = 0;
    for (int i = 0; i < array_len(ast->call->args); i++) {
        if (i > 0) {
            write_fmt(",");
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
                    write_fmt("(");
                }
                if (i >= array_len(r->fn.args) - 1) {
                    write_fmt("_tmp%d[%d] = ", vt->var->id, i - (array_len(r->fn.args) - 1));
                }
            }
        }

        if (is_any(defined_arg_type) && !is_any(arg_ast->var_type)) {
            write_fmt("(struct _type_vs_%d){.value_pointer=", get_any_type_id());
            if (needs_temp_var(arg_ast)) {
                emit_temp_var(scope, arg_ast, 1);
            } else {
                compile_ref(scope, arg_ast);
            }
            write_fmt(",.type=(struct _type_vs_%d *)&_type_info%d}", get_typeinfo_type_id(), arg_ast->var_type->id);
        } else {
            compile_call_arg(scope, arg_ast, defined_arg_type->resolved->comp == ARRAY);
        }
    }

    if (r->fn.variadic && !ast->call->has_spread) {
        if (array_len(ast->call->args) - array_len(r->fn.args) < 0) {
            write_fmt(", (struct array_type){0, NULL}");
        } else if (array_len(ast->call->args) > array_len(r->fn.args) - 1) {
            write_fmt(", (struct array_type){%ld, _tmp%d})",
                vt->var->type->resolved->array.length, vt->var->id); // this assumes we have set vt correctly
        }
    }
    write_fmt(")");
}

void emit_scope_start(Scope *scope) {
    write_fmt("{\n");
    change_indent(1);
    for (int i = 0; i < array_len(scope->temp_vars); i++) {
        Type *t = scope->temp_vars[i]->var->type;
        indent();
        if (t->resolved->comp == STATIC_ARRAY) {
            emit_type(t->resolved->array.inner);
            write_fmt("_tmp%d[%ld]", scope->temp_vars[i]->var->id, t->resolved->array.length);
        } else {
            emit_type(t);
            write_fmt("_tmp%d", scope->temp_vars[i]->var->id);
            if (t->resolved->comp == STRUCT || is_string(t)) {
                write_fmt(" = {0}");
            }
        }
        write_fmt(";\n");
    }
}

void open_block() {
    indent();
    write_fmt("{\n");
    change_indent(1);
}

void close_block() {
    change_indent(-1);
    indent();
    write_fmt("}\n");
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
            write_fmt("free(%s.bytes);\n", memname);
        } else if (member_res->comp == STRUCT) {
            int ref = (member_res->comp == REF || (member_res->comp == BASIC && member_res->data->base == BASEPTR_T));
            emit_free_struct(scope, memname, r->st.member_types[i], ref);
        } else if (member_res->comp == ARRAY && member_res->array.owned) {
            if (is_dynamic(member_res->array.inner)) {
                open_block();

                indent();
                emit_type(member_res->array.inner);
                write_fmt("*_0 = %s.data;\n", memname);

                indent();
                write_fmt("for (int i = 0; i < %s.length; i++) {\n", memname);

                change_indent(1);
                indent();
                emit_type(member_res->array.inner);

                Var *v = make_var("<i>", member_res->array.inner);
                v->initialized = 1;
                write_fmt("_vs_%d = _0[i];\n", v->id);

                emit_free(scope, v);
                free(v);

                close_block();
                close_block();
            }
            indent();
            write_fmt("free(%s.data);\n", memname);
        } else if (member_res->comp == REF && member_res->ref.owned) {
            Type *inner = member_res->ref.inner;

            if (inner->resolved->comp == STRUCT) {
                emit_free_struct(scope, memname, inner, 1);
            } else if (is_string(inner)) { // TODO should this behave this way?
                indent();
                write_fmt("free(%s->bytes);\n", memname);
            }

            indent();
            write_fmt("free(%s);\n", memname);
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
                write_fmt("free(");
                write_fmt(name_fmt, var->id);
                write_fmt(");\n");
            } else if (is_string(inner)) { // TODO should this behave this way?
                indent();
                write_fmt("free(");
                write_fmt(name_fmt, var->id);
                write_fmt("->bytes);\n");
            }
        }
    } else if (r->comp == STATIC_ARRAY) {
        if (is_dynamic(r->array.inner)) {
            open_block();

            indent();
            emit_type(r->array.inner);
            write_fmt("*_0 = ");
            write_fmt(name_fmt, var->id);
            write_fmt(";\n");

            indent();
            write_fmt("for (int i = 0; i < %ld; i++) {\n", r->array.length);

            change_indent(1);
            indent();
            emit_type(r->array.inner);

            Var *v = make_var("<i>", r->array.inner);
            v->initialized = 1;
            write_fmt("_vs_%d = _0[i];\n", v->id);

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
                write_fmt("*_0 = ");
                write_fmt(name_fmt, var->id);
                write_fmt(".data;\n");

                indent();
                write_fmt("for (int i = 0; i < ");
                write_fmt(name_fmt, var->id);
                write_fmt(".length; i++) {\n");

                change_indent(1);
                indent();
                emit_type(r->array.inner);

                Var *v = make_var("<i>", r->array.inner);
                v->initialized = 1;
                write_fmt("_vs_%d = _0[i];\n", v->id);

                emit_free(scope, v);
                free(v);

                close_block();
                close_block();
            }
            write_fmt("free(");
            write_fmt(name_fmt, var->id);
            write_fmt(".data);\n");
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
            write_fmt("free(");
            write_fmt(name_fmt, var->id);
            write_fmt(".bytes);\n");
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
    for (int i = array_len(scope->vars)-1; i >= 0; --i) {
        Var *v = scope->vars[i];
        if (v->proxy) {
            continue;
        }
        emit_free(scope, v);
    }
}

void emit_free_temp(Scope *scope) {
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
    write_fmt("}\n");
}

void emit_scope_end(Scope *scope) {
    if (!scope->has_return) {
        emit_deferred(scope);
        emit_free_locals(scope);
    }
    change_indent(-1);
    indent();
    write_fmt("}\n");
}

void emit_deferred(Scope *scope) {
    for (int i = array_len(scope->deferred)-1; i >= 0; --i) {
        Ast *d = scope->deferred[i];
        indent();
        if (needs_temp_var(d)) {
            Var *v = find_temp_var(scope, d);
            write_fmt("_tmp%d = ", v->id);
        }
        compile(scope, d);
        write_fmt(";\n");
    }
}

void emit_extern_fn_decl(Scope *scope, Var *v) {
    write_fmt("extern ");

    ResolvedType *r = v->type->resolved;
    emit_type(r->fn.ret);
    write_fmt("%s(", v->name);
    for (int i = 0; i < array_len(r->fn.args); i++) {
        if (i > 0) {
            write_fmt(",");
        }
        emit_type(r->fn.args[i]);
    }
    write_fmt(");\n");

    emit_type(r->fn.ret);
    write_fmt("(*_vs_%s)(", v->name);

    for (int i = 0; i < array_len(r->fn.args); i++) {
        if (i > 0) {
            write_fmt(",");
        }
        emit_type(r->fn.args[i]);
    }
    write_fmt(") = %s;\n", v->name);
}

void emit_var_decl(Scope *scope, Var *v) {
    if (v->ext) {
        emit_extern_fn_decl(scope, v);
        return;
    }

    ResolvedType *r = v->type->resolved;
    if (r->comp == FUNC) {
        emit_type(r->fn.ret);
        write_fmt("(*");
    } else if (r->comp == STATIC_ARRAY) {
        emit_type(r->array.inner);
        write_fmt("_vs_%d[%ld] = {0};\n", v->id, r->array.length);
        return;
    } else {
        emit_type(v->type);
    }

    write_fmt("_vs_%d", v->id);
    if (r->comp == FUNC) {
        write_fmt(")(");
        for (int i = 0; i < array_len(r->fn.args); i++) {
            if (i > 0) {
                write_fmt(",");
            }
            emit_type(r->fn.args[i]);
            write_fmt("a%d", i);
        }
        write_fmt(")");
    }
    write_fmt(";\n");
}

void emit_forward_decl(Scope *scope, AstFnDecl *decl) {
    ResolvedType *r = decl->var->type->resolved;
    if (is_polydef(decl->var->type)) {
        // polymorph not being used
        return;
    }
    if (decl->polymorph_of) {
        write_fmt("/* polymorph %s of %s */\n", type_to_string(decl->var->type), decl->polymorph_of->var->name);
    } else {
        write_fmt("/* %s */\n", decl->var->name);
    }
    if (decl->var->ext) {
        write_fmt("extern ");
    }
    assert(r->comp == FUNC);
    emit_type(r->fn.ret);

    write_fmt("_vs_%d(", decl->var->id);

    for (int i = 0; i < array_len(r->fn.args); i++) {
        if (i > 0) {
            write_fmt(",");
        }
        if (r->fn.variadic && i == (array_len(r->fn.args) - 1)) {
            write_fmt("struct array_type ");
        } else {
            emit_type(r->fn.args[i]);
        }
        write_fmt("a%d", i);
    }
    write_fmt(");\n");
}

void emit_slice(Scope *scope, Ast *ast) {
    Type *obj_type = ast->slice->object->var_type;
    ResolvedType *r = obj_type->resolved;

    if (is_string(obj_type)) {
        write_fmt("string_slice(");
        compile(scope, ast->slice->object);
        write_fmt(",");
        if (ast->slice->offset != NULL) {
            compile(scope, ast->slice->offset);
        } else {
            write_fmt("0");
        }
        write_fmt(",");
        if (ast->slice->length != NULL) {
            compile(scope, ast->slice->length);
        } else {
            write_fmt("-1");
        }
        write_fmt(")");
        return;
    }

    if (r->comp == STATIC_ARRAY) {
        write_fmt("(struct array_type){.data=");

        if (ast->slice->offset != NULL) {
            write_fmt("((char *)");
        }

        if (needs_temp_var(ast->slice->object)) {
            emit_temp_var(scope, ast->slice->object, 0);
        } else {
            compile_static_array(scope, ast->slice->object);
        }

        if (ast->slice->offset != NULL) {
            write_fmt(")+(");
            compile(scope, ast->slice->offset);
            write_fmt("*sizeof(");
            emit_type(r->array.inner);
            write_fmt("))");
        }

        write_fmt(",.length=");
        if (ast->slice->length != NULL) {
            compile(scope, ast->slice->length);
        } else {
            write_fmt("%ld", r->array.length);
        }

        if (ast->slice->offset != NULL) {
            write_fmt("-");
            compile(scope, ast->slice->offset);
        }
        write_fmt("}");
    } else { // ARRAY
        write_fmt("array_slice(");

        compile_unspecified_array(scope, ast->slice->object);

        write_fmt(",");
        if (ast->slice->offset != NULL) {
            compile(scope, ast->slice->offset);
            write_fmt(",sizeof(");
            emit_type(r->array.inner);
            write_fmt("),");
        } else {
            write_fmt("0,0,");
        }

        if (ast->slice->length != NULL) {
            compile(scope, ast->slice->length);
        } else {
            write_fmt("-1");
        }
        write_fmt(")");
    }
}

void emit_return(Scope *scope, Ast *ast) {
    Var *ret = NULL;
    if (ast->ret->expr != NULL) {
        emit_type(ast->ret->expr->var_type);
        write_fmt("_ret = ");
        // TODO: if this doesn't actually need to be copied, we have to make
        // sure it isn't cleaned up on return
        if (is_lvalue(ast->ret->expr)) {
            emit_copy(scope, ast->ret->expr);
        } else {
            compile(scope, ast->ret->expr);
        }
        write_fmt(";");

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
    write_fmt("\n");
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
        for (int i = s->parent_deferred; i >= 0; --i) {
            Ast *d = s->parent->deferred[i];
            indent();
            if (needs_temp_var(d)) {
                Var *v = find_temp_var(s, d);
                write_fmt("_tmp%d = ", v->id);
            }
            compile(s, d);
            write_fmt(";\n");
        }
        s = s->parent;
    }

    indent();
    write_fmt("return");
    if (ast->ret->expr != NULL) {
        write_fmt(" _ret");
    }
}

void emit_for_loop(Scope *scope, Ast *ast) {
    // TODO: loop depth should change iter var name?
    write_fmt("{\n");
    change_indent(1);
    indent();

    write_fmt("struct array_type _iter = ");
    compile_unspecified_array(scope, ast->for_loop->iterable);
    write_fmt(";\n");

    if (ast->for_loop->index != NULL) {
        indent();
        emit_type(ast->for_loop->index->type);
        write_fmt("_vs_%d;\n", ast->for_loop->index->id);
    }

    indent();
    write_fmt("for (long _i = 0; _i < _iter.length; _i++) {\n");

    change_indent(1);
    indent();

    if (ast->for_loop->by_reference) {
        emit_type(ast->for_loop->itervar->type);
        write_fmt("_vs_%d = ", ast->for_loop->itervar->id);
        write_fmt("&");
        write_fmt("((");
        emit_type(ast->for_loop->itervar->type);
        write_fmt(")_iter.data)[_i];\n");
    } else {
        Type *t = ast->for_loop->itervar->type;
        emit_type(t);
        write_fmt("_vs_%d = ", ast->for_loop->itervar->id);
        if (is_string(t)) {
            write_fmt("copy_string");
        } else if (t->resolved->comp == STRUCT && is_dynamic(t)) {
            write_fmt("_copy_%d", t->id);
        }
        write_fmt("(((");
        emit_type(t);
        write_fmt("*)_iter.data)[_i]);\n");
    }

    if (ast->for_loop->index != NULL) {
        indent();
        emit_type(ast->for_loop->index->type);
        write_fmt("_vs_%d = (", ast->for_loop->index->id);
        emit_type(ast->for_loop->index->type);
        write_fmt(")_i;\n");
    }

    indent();
    emit_scope_start(ast->for_loop->scope);
    // TODO don't clear these vars!
    compile_block(ast->for_loop->scope, ast->for_loop->body);
    emit_scope_end(ast->for_loop->scope);

    change_indent(-1);
    indent();
    write_fmt("}\n");

    change_indent(-1);
    indent();
    write_fmt("}\n");
}

void emit_integer_literal(Scope *scope, Ast *ast) {
    ResolvedType *res = ast->var_type->resolved;
    write_fmt("%lld", ast->lit->int_val);
    // This may break somehow but I'm pissed and don't want to make it
    // right
    switch (res->data->size) {
        case 8:
            if (res->data->base == UINT_T) {
                write_fmt("U");
            }
            write_fmt("LL");
            break;
        case 4:
            if (res->data->base == UINT_T) {
                write_fmt("U");
            }
            write_fmt("L");
            break;
        default:
            break;
    }
}

void emit_struct_literal(Scope *scope, Ast *ast) {
    write_fmt("(struct _type_vs_%d){", ast->var_type->id);
    if (array_len(ast->lit->compound_val.member_exprs) == 0) {
        write_fmt("0");
    } else {
        StructType st = ast->var_type->resolved->st;
        for (int i = 0; i < array_len(ast->lit->compound_val.member_exprs); i++) {
            Ast *expr = ast->lit->compound_val.member_exprs[i];
            write_fmt(".%s = ", ast->lit->compound_val.member_names[i]);

            if (is_any(st.member_types[i]) && !is_any(expr->var_type)) {
                emit_any_wrapper(scope, expr);
            } else if (is_lvalue(expr)) {
                emit_copy(scope, expr);
            } else {
                compile(scope, expr);
            }

            if (i != array_len(st.member_names) - 1) {
                write_fmt(", ");
            }
        }
    }
    write_fmt("}");
}

void emit_array_literal(Scope *scope, Ast *ast) {
    TempVar *tmp = ast->lit->compound_val.array_tempvar;
    long n = array_len(ast->lit->compound_val.member_exprs);

    write_fmt("(");
    for (int i = 0; i < n; i++) {
        Ast *expr = ast->lit->compound_val.member_exprs[i];
        write_fmt("_tmp%d[%d] = ", tmp->var->id, i);

        if (is_any(tmp->var->type->resolved->array.inner) && !is_any(expr->var_type)) {
            emit_any_wrapper(scope, expr);
        } else if (is_lvalue(expr)) {
            emit_copy(scope, expr);
        } else {
            compile(scope, expr);
        }

        write_fmt(",");
    }
    if (ast->var_type->resolved->comp == STATIC_ARRAY) {
        write_fmt("_tmp%d)", tmp->var->id);
    } else {
        write_fmt("(struct array_type){%ld, _tmp%d})", n, tmp->var->id);
    }
}

void emit_literal(Scope *scope, Ast *ast) {
    switch (ast->lit->lit_type) {
    case INTEGER:
        emit_integer_literal(scope, ast);
        break;
    case FLOAT: // TODO this is truncated
        write_fmt("%F", ast->lit->float_val);
        break;
    case CHAR:
        write_fmt("'%c'", (unsigned char)ast->lit->int_val);
        break;
    case BOOL:
        write_fmt("%d", (unsigned char)ast->lit->int_val);
        break;
    case STRING:
        write_fmt("init_string(\"");
        print_quoted_string(output, ast->lit->string_val);
        write_fmt("\", %d)", (int)strlen(ast->lit->string_val));
        break;
    case STRUCT_LIT:
        emit_struct_literal(scope, ast);
        break;
    case ARRAY_LIT:
        emit_array_literal(scope, ast);
        break;
    case ENUM_LIT:
        write_fmt("%ld", enum_type_val(ast->lit->enum_val.enum_type, ast->lit->enum_val.enum_index));
        break;
    case COMPOUND_LIT:
        error(ast->line, ast->file, "<internal> literal type should be determined at this point");
        break;
    }
}

void emit_array_index(Scope *scope, Ast *ast) {
    Type *lt = ast->index->object->var_type;
    write_fmt("(");
    if (lt->resolved->comp == ARRAY) {
        write_fmt("(");
        emit_type(lt->resolved->array.inner);
        write_fmt("*)");
    }

    if (needs_temp_var(ast->index->object)) {
        emit_temp_var(scope, ast->index->object, 0);
    } else {
        compile_static_array(scope, ast->index->object);
    }

    write_fmt(")[");

    if (needs_temp_var(ast->index->index)) {
        emit_temp_var(scope, ast->index->index, 0);
    } else {
        compile(scope, ast->index->index);
    }

    write_fmt("]");
}

void emit_string_index(Scope *scope, Ast *ast) {
    write_fmt("((uint8_t*)");
    if (needs_temp_var(ast->index->object)) {
        emit_temp_var(scope, ast->index->object, 0);
    } else {
        // TODO: need string_as_array here?
        compile(scope, ast->index->object);
    }

    write_fmt(".bytes)[");

    if (needs_temp_var(ast->index->index)) {
        emit_temp_var(scope, ast->index->index, 0);
    } else {
        compile(scope, ast->index->index);
    }

    write_fmt("]");
}

void emit_index(Scope *scope, Ast *ast) {
    Type *lt = ast->index->object->var_type;
    if (is_array(lt)) {
        emit_array_index(scope, ast);
    } else if (is_string(lt)) { // string
        emit_string_index(scope, ast);
    } else {
        error(ast->line, ast->file, "<internal> bad type %s for index", type_to_string(lt));
    }
}

void emit_parent_defers_recursively(Scope *scope) {
    for (Scope *s = scope; s->parent != NULL; s = s->parent) {
        emit_free_locals(s);
        if (s->type == Loop) {
            break;
        }
        // TODO: I don't think this will work if there are multiple scopes
        // nested and defers are added after a break
        for (int i = s->parent_deferred; i >= 0; --i) {
            Ast *d = s->parent->deferred[i];
            indent();
            if (needs_temp_var(d)) {
                Var *v = find_temp_var(s, d);
                write_fmt("_tmp%d = ", v->id);
            }
            compile(s, d);
            write_fmt(";\n");
        }
    }
}

void emit_cast(Scope *scope, Ast *ast) {
    if (is_any(ast->cast->cast_type)) {
        emit_any_wrapper(scope, ast->cast->object);
        return;
    }
    ResolvedType *r = ast->cast->cast_type->resolved;
    if (r->comp == STRUCT) {
        write_fmt("*");
    }

    write_fmt("((");
    emit_type(ast->cast->cast_type);
    if (r->comp == STRUCT) {
        write_fmt("*");
    }

    write_fmt(")");
    if (r->comp == STRUCT) {
        write_fmt("&");
    }

    compile(scope, ast->cast->object);
    write_fmt(")");
}

void compile(Scope *scope, Ast *ast) {
    switch (ast->type) {
    case AST_LITERAL:
        emit_literal(scope, ast);
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
        if (tmp) {
            write_fmt("(_tmp%d = ", tmp->id);
        }

        ResolvedType *r = ast->var_type->resolved;
        if (r->comp == ARRAY) {
            write_fmt("(allocate_array(");
            compile(scope, ast->new->count);
            write_fmt(",sizeof(");
            emit_type(r->array.inner);
            write_fmt(")))");
        } else {
            assert(r->comp == REF);
            write_fmt("(_init_%d(NULL))", r->ref.inner->id);
        }
        if (tmp) {
            write_fmt(")");
        }
        break;
    }
    case AST_CAST:
        emit_cast(scope, ast);
        break;
    case AST_SLICE:
        emit_slice(scope, ast);
        break;
    case AST_IDENTIFIER:
        assert(!ast->ident->var->proxy);
        if (ast->ident->var->ext) {
            write_fmt("_vs_%s", ast->ident->var->name);
        } else {
            write_fmt("_vs_%d", ast->ident->var->id);
        }
        break;
    case AST_RETURN:
        emit_return(scope, ast);
        break;
    case AST_BREAK:
        emit_deferred(scope);
        emit_parent_defers_recursively(scope);
        write_fmt("break");
        break;
    case AST_CONTINUE:
        emit_deferred(scope);
        emit_parent_defers_recursively(scope);
        write_fmt("continue");
        break;
    case AST_DECL:
        emit_decl(scope, ast);
        break;
    case AST_FUNC_DECL:
    case AST_EXTERN_FUNC_DECL:
        break;
    case AST_ANON_FUNC_DECL:
        write_fmt("_vs_%d", ast->fn_decl->var->id);
        break;
    case AST_CALL:
        compile_fn_call(scope, ast);
        break;
    case AST_INDEX:
        emit_index(scope, ast);
        break;
    case AST_ANON_SCOPE:
        emit_scope_start(ast->anon_scope->scope);
        compile_block(ast->anon_scope->scope, ast->anon_scope->body);
        emit_scope_end(ast->anon_scope->scope);
        break;
    case AST_BLOCK: // If this is used on root, does emit_scope_start need to happen?
        compile_block(scope, ast->block);
        break;
    case AST_CONDITIONAL:
        emit_scope_start(ast->cond->initializer_scope);
        if (ast->cond->initializer) {
            compile(ast->cond->initializer_scope, ast->cond->initializer);
            write_fmt(";\n");
            indent();
        }
        write_fmt("if (");
        compile(ast->cond->initializer_scope, ast->cond->condition);
        write_fmt(") ");
        emit_scope_start(ast->cond->if_scope);
        compile_block(ast->cond->if_scope, ast->cond->if_body);
        emit_scope_end(ast->cond->if_scope);
        if (ast->cond->else_body != NULL) {
            indent();
            write_fmt("else ");
            emit_scope_start(ast->cond->else_scope);
            compile_block(ast->cond->else_scope, ast->cond->else_body);
            emit_scope_end(ast->cond->else_scope);
        }
        emit_scope_end(ast->cond->initializer_scope);
        break;
    case AST_WHILE:
        emit_scope_start(ast->while_loop->scope);
        if (ast->while_loop->initializer) {
            compile(ast->while_loop->scope, ast->while_loop->initializer);
            write_fmt(";\n");
            indent();
        }
        write_fmt("while (");
        compile(ast->while_loop->inner_scope, ast->while_loop->condition);
        write_fmt(") ");
        emit_scope_start(ast->while_loop->inner_scope);
        compile_block(ast->while_loop->inner_scope, ast->while_loop->body);
        emit_scope_end(ast->while_loop->inner_scope);
        emit_scope_end(ast->while_loop->scope);
        break;
    case AST_FOR:
        emit_for_loop(scope, ast);
        break;
    case AST_TYPEINFO:
        write_fmt("((struct _type_vs_%d *)&_type_info%d)",
            get_typeinfo_type_id(),
            ast->typeinfo->typeinfo_target->id);
        break;
    case AST_TYPE_DECL:
        break;
    case AST_ENUM_DECL:
        break;
    case AST_IMPORT:
        break;
    case AST_COMMENT:
        break;
    default:
        error(ast->line, ast->file, "No idea how to deal with this.");
    }
}
