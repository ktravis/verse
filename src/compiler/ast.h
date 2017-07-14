#ifndef AST_H
#define AST_H

#include "var.h"
//#include "token.h"
#include "scope.h"
#include "types.h"

typedef enum {
    INTEGER,
    CHAR,
    STRING,
    FLOAT,
    BOOL,
    ENUM_LIT,
    // May not need all 3 of these
    COMPOUND_LIT,
    ARRAY_LIT,
    STRUCT_LIT,
} LiteralType;

typedef struct AstImport {
    char *path;
    char *pkg_name;
    Package *package;
} AstImport;

typedef struct AstLiteral {
    LiteralType lit_type;
    union {
        long long int_val;
        double    float_val;
        char      *string_val;
        struct {
            Type  *type;
            int    named;
            char **member_names;
            Ast  **member_exprs;
            TempVar   *array_tempvar;
        } compound_val; // array or struct
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
    struct Ast **args;
    TempVar *variadic_tempvar;
    Polymorph *polymorph;
    char has_spread;
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
    struct Ast **statements;
    char           *file;
    int             startline;
    int             endline;
} AstBlock;

typedef struct AstFnDecl {
    Var       *var;
    int        anon;
    Var      **args;
    Scope     *scope;
    AstBlock  *body;
    Polymorph **polymorphs;
    struct AstFnDecl *polymorph_of;
    // TODO: sloppy, clean this up
    int        ext_autocast;
} AstFnDecl;

typedef struct AstConditional {
    Ast *condition;
    Ast *initializer;
    Scope *if_scope;
    Scope *else_scope;
    Scope *initializer_scope;
    AstBlock *if_body;
    AstBlock *else_body;
} AstConditional;

typedef struct AstTypeObj {
    Type *t;
} AstTypeObj;

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

typedef struct AstWhile {
    Ast *condition;
    Ast *initializer;
    Scope *scope;
    Scope *inner_scope;
    AstBlock *body;
} AstWhile;

typedef struct AstFor {
    Var *itervar;
    Var *index;
    Scope *scope;
    Ast *iterable;
    AstBlock *body;
    char by_reference;
} AstFor;

typedef struct AstAnonScope {
    Scope *scope;
    AstBlock *body;
} AstAnonScope;

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

typedef struct AstSpread {
    Ast *object;
} AstSpread;

typedef struct AstNew {
    Ast *count; // used for array init
    Type *type;
} AstNew;

typedef struct AstDefer {
    Ast *call;
} AstDefer;

typedef struct AstImpl {
    Type    *type;
    Ast    **methods;
} AstImpl;

typedef struct AstMethod {
    Ast       *recv;
    char      *name;
    AstFnDecl *decl;
} AstMethod;

typedef struct AstTypeIdent {
    Type       *type;
} AstTypeIdent;

typedef struct AstPackage {
    Package *package;
    char    *pkg_name;
} AstPackage;

// TODO: make AstPackage, AstType, AstMethod

Ast *ast_alloc(AstType type);
Ast *deep_copy(Ast *ast);
Ast *copy_ast(Scope *scope, Ast *ast);
AstBlock *copy_ast_block(Scope *scope, AstBlock *block);

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

char *get_varname(Ast *ast);

int needs_temp_var(Ast *ast);
int is_lvalue(Ast *ast);
//int is_literal(Ast *ast);
// TODO: clarify the purpose of these 
Ast *coerce_literal(Ast *ast, Type *t);
Ast *cast_literal(Type *t, Ast *ast);
Ast *try_implicit_cast(Type *t, Ast *ast);
Ast *try_implicit_cast_no_error(Type *t, Ast *ast);

void print_ast(Ast *ast);

#endif
