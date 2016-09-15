#ifndef COMMON_H
#define COMMON_H

enum {
    INT_T = 1,
    UINT_T,
    FLOAT_T,
    BOOL_T,
    STRING_T,
    VOID_T,
    BASEPTR_T
    //ANY_T
};

typedef struct TypeData {
    int base;
    int size;
    long length;
} TypeData;

// Structs, enums, and other base types are "primitives"
// (Any is a 'special' struct)
// all others (functions, refs, arrays, aliases) are not primitives, they use
// Type and not TypeData directly
// Structs can have parameters, the bare type is primitive, but the parametrized
// type is not -- it is expressed as
// a Type<PARAMS>(Type<BASIC>(TypeData<STRUCT_T>), ...)
// i.e. Box is a primitive type -- Box<u16> is parametrized, and so is not
// primitive
// Maybe a better definition -> If ANY resolution can potentially happen to make
// a type concrete, it is not considered primitive
// TODO: does this mean structs are not primitive though? what about a struct

typedef struct TypeList {
    struct Type *item;
    struct TypeList *next;
    struct TypeList *prev;
} TypeList;

typedef enum TypeComp {
    BASIC,
    ALIAS,
    POLY,
    POLYDEF,
    PARAMS,
    STATIC_ARRAY,
    ARRAY,
    REF,
    FUNC,
    STRUCT,
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
        TypeData *data; // basic
        char *name; // alias
        TypeList *params;
        struct {
            long length;
            struct Type *inner;
        } array;
        struct Type *inner; // slice / ref
        FnType fn;
        StructType st;
        EnumType en;
    };
} Type;

typedef struct TypeDef {
    char *name;
    Type *type;
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
    //int held;
    int ext;
    struct Var *proxy;
    struct Var **members;
    struct AstFnDecl *fn_decl;
} Var;

typedef struct VarList {
    Var *item;
    struct VarList *next;
} VarList;

typedef enum {
    Root,
    Simple,
    Function,
    Loop
} ScopeType;

typedef struct TempVarList {
    int id;
    Var *var;
    struct TempVarList *next;
} TempVarList;

typedef struct Polymorph {
    int               id;
    TypeList         *args;
    TypeDef          *defs;
    struct Polymorph *next;
} Polymorph;

typedef struct Scope {
    struct Scope *parent;
    ScopeType type;
    VarList *vars;
    struct TempVarList *temp_vars;
    TypeDef *types;
    TypeList *used_types;
    unsigned char has_return;
    Var *fn_var;
    Polymorph *polymorph;
} Scope;

typedef enum {
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
    AST_BREAK,
    AST_CONTINUE,
    AST_CAST,
    AST_DIRECTIVE,
    AST_TYPEINFO,
    AST_ENUM_DECL,
    AST_USE,
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
        struct AstBind          *bind;
        struct AstBlock         *block;
        struct AstConditional   *cond;
        struct AstTypeDecl      *type_decl;
        struct AstEnumDecl      *enum_decl;
        struct AstTempVar       *tempvar;
        struct AstCopy          *copy;
        struct AstReturn        *ret;
        struct AstHold          *hold;
        struct AstRelease       *release;
        struct AstWhile         *while_loop;
        struct AstFor           *for_loop;
        struct AstDirective     *directive;
        struct AstUse           *use;
    };
} Ast;

typedef struct AstList {
    Ast *item;
    struct AstList *next;
} AstList;

typedef enum {
    INTEGER,
    CHAR,
    STRING,
    FLOAT,
    BOOL,
    STRUCT_LIT,
    ENUM_LIT,
} LiteralType;

#endif
