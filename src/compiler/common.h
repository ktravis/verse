#ifndef COMMON_H
#define COMMON_H

typedef struct Package {
    char *path;
    char *name;
    struct Scope *scope;
    struct VarList *globals;
    struct PkgFileList *files;
    int semantics_checked;
} Package;

typedef struct PkgList {
    Package *item;
    struct PkgList *next;
    struct PkgList *prev;
} PkgList;

typedef struct PkgFile {
    char *name;
    struct Ast *root;
} PkgFile;

typedef struct PkgFileList {
    PkgFile *item;
    struct PkgFileList *next;
    struct PkgFileList *prev;
} PkgFileList;

typedef enum TypeComp {
    BASIC,
    ALIAS,
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

typedef struct FnType {
    int nargs;
    struct TypeList *args;
    struct Type *ret;
    int variadic;
} FnType;

typedef struct StructType {
    int nmembers;
    char **member_names;
    struct Type **member_types;
    int generic;
    struct TypeList *arg_params;
    struct Type *generic_base;
} StructType;

typedef struct EnumType {
    int nmembers;
    char **member_names;
    long *member_values;
    struct Type *inner;
} EnumType;

typedef struct Type {
    int id;
    TypeComp comp;
    struct Scope *scope;
    union {
        struct TypeData *data; // basic
        char *name; // alias
        struct {
            struct TypeList *args;
            struct Type *inner;
        } params;
        struct {
            long length;
            struct Type *inner;
        } array;
        // is this good, or do we need additional levels?
        struct {
            char *pkg_name;
            char *type_name;
        } ext;
        struct Type *inner;
        FnType fn;
        StructType st;
        EnumType en;
    };
} Type;

typedef struct TypeDef {
    char *name;
    Type *type;
    Type *defined_type;
    struct TypeDef *next;
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

typedef struct Polymorph {
    int               id;
    struct TypeList  *args;
    TypeDef          *defs;
    struct Polymorph *next;
    struct Scope     *scope;
    struct AstBlock  *body;
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
    struct VarList *vars;
    struct TempVarList *temp_vars;
    TypeDef *types;
    struct TypeList *used_types;
    unsigned char has_return;
    Var *fn_var;
    Polymorph *polymorph;
    struct PkgList *packages;
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
    };
} Ast;

#endif
