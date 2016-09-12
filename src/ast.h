#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "common.h"
#include "var.h"
#include "token.h"
#include "util.h"
#include "scope.h"
#include "types.h"

typedef struct AstLiteral {
    LiteralType lit_type;
    union {
        long    int_val;
        double  float_val;
        char    *string_val;
        struct {
            char *name;
            Type *type;
            int  nmembers;
            char **member_names;

            Ast **member_exprs;
        } struct_val;
        struct {
            long enum_index;
            Type *enum_type;
        } enum_val;
    };
} AstLiteral;

typedef struct AstTypeInfo {
    Type *typeinfo_target;
} AstTypeInfo;

typedef struct AstIdent {
    Var *var;
    char *varname; // TODO not needed?
} AstIdent;

typedef struct AstDecl {
    Var *var;
    Ast *init;
    unsigned char global;
} AstDecl;

typedef struct AstUnaryOp {
    int op;
    Ast *object;
} AstUnaryOp;

typedef struct AstBinaryOp {
    int op;
    Ast *left;
    Ast *right;
} AstBinaryOp;

typedef struct AstCast {
    Type *cast_type;
    Ast *object;
} AstCast;

typedef struct AstDot {
    Ast *object;
    char *member_name;
} AstDot;

typedef struct AstCall {
    Ast *fn; // obj?
    int nargs;
    struct AstList *args;
    Var *variadic_tempvar;
} AstCall;

typedef struct AstSlice {
    Ast *object;
    Ast *offset;
    Ast *length;
} AstSlice;

typedef struct AstIndex {
    Ast *object;
    Ast *index;
} AstIndex;

typedef struct AstBlock {
    struct AstList *statements;
    char *file;
    int startline;
    int endline;
} AstBlock;

typedef struct AstFnDecl {
    Var      *var;
    int       anon;
    VarList  *args;
    Scope    *scope;
    AstBlock *body;
} AstFnDecl;

typedef struct AstConditional {
    Ast *condition;
    Scope *scope;
    AstBlock *if_body;
    AstBlock *else_body;
} AstConditional;

typedef struct AstTypeDecl {
    char *type_name;
    Type *target_type;
} AstTypeDecl;

typedef struct AstTempVar {
    Var *var;
    Ast *expr;
} AstTempVar;

typedef struct AstCopy {
    Ast *expr;
} AstCopy;

typedef struct AstReturn {
    //Scope *scope; // Needed ?
    Ast *expr;
} AstReturn;

typedef struct AstHold {
    Ast *object;
    Var *tempvar;
} AstHold;

typedef struct AstRelease {
    Ast *object;
} AstRelease;

typedef struct AstWhile {
    Ast *condition;
    Scope *scope;
    AstBlock *body;
} AstWhile;

typedef struct AstFor {
    Var *itervar;
    Scope *scope;
    Ast *iterable;
    AstBlock *body;
} AstFor;

typedef struct AstDirective {
    char *name;
    Ast *object;
} AstDirective;

typedef struct AstUse {
    Ast *object;
} AstUse;

typedef struct AstEnumDecl {
    char *enum_name;
    Type *enum_type;
    Ast **exprs;
} AstEnumDecl;

//typedef struct AstListList {
    //int id;
    //AstList *item;
    //struct AstListList *next;
//} AstListList;

Ast *ast_alloc(AstType type);
Ast *deep_copy(Ast *ast);

Ast *make_ast_copy(Ast *ast);
Ast *make_ast_bool(long ival);
Ast *make_ast_string(char *val);
Ast *make_ast_dot_op(Ast *dot_left, char *member_name);
Ast *make_ast_tempvar(Ast *ast, Var *tempvar);
Ast *make_ast_id(Var *var, char *name);
Ast *make_ast_decl(char *name, Type *type);
Ast *make_ast_assign(Ast *left, Ast *right);
Ast *make_ast_binop(int op, Ast *left, Ast *right);
Ast *make_ast_slice(Ast *inner, Ast *offset, Ast *length);

//Type *var_type(Ast *ast);
char *get_varname(Ast *ast);

int can_coerce_type(Scope *scope, Type *to, Ast *from);

AstList *astlist_append(AstList *list, Ast *ast);
AstList *reverse_astlist();

int is_lvalue(Ast *ast);
//int is_literal(Ast *ast);
Ast *coerce_literal(Ast *ast, Type *t);
Ast *cast_literal(Type *t, Ast *ast);
Ast *try_implicit_cast(Type *t, Ast *ast);
Ast *try_implicit_cast_no_error(Type *t, Ast *ast);

void print_ast(Ast *ast);

#endif
