#ifndef COMMON_H
#define COMMON_H

#include "hashmap/hashmap.h"

typedef struct Package {
    char *path;
    char *name;
    struct Scope *scope;
    struct Var **globals;
    struct PkgFile **files;
    struct AstBlock *root;
    //struct Ast **statements;
    int semantics_checked;
    char **top_level_comments;
} Package;

typedef struct PkgFile {
    Package *package;
    char *name;
    int start_index;
    int end_index;
} PkgFile;

typedef enum TypeComp {
    BASIC,
    POLYDEF,
    PARAMS,
    STATIC_ARRAY,
    ARRAY,
    REF,
    FUNC,
    STRUCT,
    EXTERNAL,
    ENUM
} TypeComp;

typedef struct RefType {
    char owned;
    struct Type *inner;
} RefType;

typedef struct ArrayType {
    char owned;
    long length;
    struct Type *inner;
} ArrayType;

typedef struct FnType {
    struct Type **args;
    struct Type **ret;
    int variadic;
} FnType;

typedef struct StructType {
    char **member_names;
    struct Type **member_types;
    int generic;
    struct Type **arg_params;
    struct Type *generic_base;
    struct Ast **methods;
} StructType;

typedef struct EnumType {
    char **member_names;
    long *member_values;
    struct Type *inner;
} EnumType;

typedef struct Type {
    int id;
    char *name;
    struct Scope *scope;
    struct ResolvedType *resolved;
    struct Type *aka;
} Type;

typedef struct ResolvedType {
    TypeComp comp;
    union {
        // basic
        struct TypeData *data;
        struct {
            struct Type **args;
            struct Type *inner;
        } params;
        // is this good, or do we need additional levels?
        struct {
            char *pkg_name;
            Package *package;
            Type *type;
            char *type_name;
        } ext;
        RefType ref;
        ArrayType array;
        FnType fn;
        StructType st;
        EnumType en;
    };
} ResolvedType;

typedef struct TypeDef {
    char *name;
    Type *type;
    struct Ast  *ast;
    struct Package *proxy;
    //Type *defined_type;
} TypeDef;

typedef struct Var {
    char *name;
    Type *type;
    int id;
    int length;
    int temp;
    int initialized;
    unsigned char constant;
    unsigned char use;
    int ext;
    struct Ast *proxy;
    struct Var **members;
    struct AstFnDecl *fn_decl;
} Var;

typedef struct TempVar {
    Var *var;
    int ast_id;
} TempVar;

typedef struct Polymorph {
    int               id;
    Type            **args;
    hashmap_t(TypeDef*)  defs;
    struct Scope     *scope;
    struct AstBlock  *body;
    Type            **ret;
    Var              *var;
} Polymorph;

typedef enum {
    Root,
    Simple,
    Function,
    Loop
} ScopeType;

typedef struct Scope {
    struct Scope *parent;
    ScopeType type;
    struct Var **vars;
    struct TempVar **temp_vars;
    hashmap_t(TypeDef*) types;
    Type **used_types;
    unsigned char has_return;
    Var *fn_var;
    Polymorph *polymorph;
    int parent_deferred;
    char *defining_name;
    struct Ast **deferred;
    struct Package *package;
    struct Package **imported_packages;
} Scope;

typedef enum AstType {
    AST_LITERAL,
    AST_DOT,
    AST_ASSIGN,
    AST_BINOP,
    AST_UOP,
    AST_IDENTIFIER,
    AST_COPY,
    AST_DECL,
    AST_FUNC_DECL,
    AST_ANON_FUNC_DECL,
    AST_EXTERN_FUNC_DECL,
    AST_CALL,
    AST_INDEX,
    AST_SLICE,
    AST_CONDITIONAL,
    AST_RETURN,
    AST_TYPE_DECL,
    AST_BLOCK,
    AST_WHILE,
    AST_FOR,
    AST_ANON_SCOPE,
    AST_BREAK,
    AST_CONTINUE,
    AST_CAST,
    AST_DIRECTIVE,
    AST_TYPEINFO,
    AST_ENUM_DECL,
    AST_USE,
    AST_IMPORT,
    AST_TYPE_OBJ,
    AST_SPREAD,
    AST_NEW,
    AST_DEFER,
    AST_IMPL,
    AST_METHOD,
    AST_TYPE_IDENT,
    AST_PACKAGE,
    AST_COMMENT,
} AstType;

typedef struct Ast {
    int id;
    AstType type;
    int     line;
    char    *file;
    Type    *var_type;
    union {
        struct AstLiteral       *lit;
        struct AstTypeInfo      *typeinfo;
        struct AstIdent         *ident;
        struct AstDecl          *decl;
        struct AstFnDecl        *fn_decl;
        struct AstUnaryOp       *unary;
        struct AstBinaryOp      *binary;
        struct AstCast          *cast;
        struct AstDot           *dot;
        struct AstCall          *call;
        struct AstSlice         *slice;
        struct AstIndex         *index;
        struct AstBlock         *block;
        struct AstConditional   *cond;
        struct AstTypeDecl      *type_decl;
        struct AstEnumDecl      *enum_decl;
        struct AstTempVar       *tempvar;
        struct AstCopy          *copy;
        struct AstReturn        *ret;
        struct AstWhile         *while_loop;
        struct AstFor           *for_loop;
        struct AstAnonScope     *anon_scope;
        struct AstDirective     *directive;
        struct AstUse           *use;
        struct AstImport        *import;
        struct AstTypeObj       *type_obj;
        struct AstSpread        *spread;
        struct AstNew           *new;
        struct AstDefer         *defer;
        struct AstImpl          *impl;
        struct AstMethod        *method;
        struct AstTypeIdent     *type_ident;
        struct AstPackage       *pkg;
        struct AstComment       *comment;
    };
} Ast;

#endif
