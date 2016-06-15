#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "parse.h"
#include "types.h"
#include "var.h"

#define PUSH_FN_SCOPE(x) \
    Ast *__old_fn_scope = current_fn_scope;\
    current_fn_scope = (x);
#define POP_FN_SCOPE() current_fn_scope = __old_fn_scope;

void define_builtin(Var *v);

Var *make_temp_var(Type *type, AstScope *scope);

void attach_var(Var *var, AstScope *scope);
void detach_var(Var *var, AstScope *scope);
void release_var(Var *var, AstScope *scope);

Var *find_var(char *name, AstScope *scope);
Var *find_local_var(char *name, AstScope *scope);

void remove_resolution_parents(ResolutionList *res);
void remove_resolution(ResolutionList *res);

Type *define_builtin_type(Type *type);
Type *find_builtin_type(char *name);
Type *define_type(Type *type, AstScope *scope);

int local_type_exists(char *name, AstScope *scope);

Type *find_type(int id);
Type *find_type_by_name(char *name, AstScope *scope, ResolutionList *child);
Type *find_type_by_name_no_unresolved(char *name, AstScope *scope);

Ast *parse_uop_semantics(Ast *ast, AstScope *scope);
Ast *parse_dot_op_semantics(Ast *ast, AstScope *scope);
Ast *parse_assignment_semantics(Ast *ast, AstScope *scope);
Ast *parse_binop_semantics(Ast *ast, AstScope *scope);
Ast *parse_enum_decl_semantics(Ast *ast, AstScope *scope);

AstBlock *parse_block_semantics(AstBlock *block, AstScope *scope, int fn_body);
AstScope *parse_scope_semantics(AstScope *scope, AstScope *parent, int fn);

Ast *parse_directive_semantics(Ast *ast, AstScope *scope);
Ast *parse_struct_literal_semantics(Ast *ast, AstScope *scope);
Ast *parse_use_semantics(Ast *ast, AstScope *scope);
Ast *parse_slice_semantics(Ast *ast, AstScope *scope);
Ast *parse_declaration_semantics(Ast *ast, AstScope *scope);
Ast *parse_call_semantics(Ast *ast, AstScope *scope);

Ast *parse_semantics(Ast *ast, AstScope *scope);

AstList *get_binding_exprs(int id);
void add_binding_expr(int id, Ast *expr);

TypeList *get_global_hold_funcs();
VarList *get_global_vars();
VarList *get_global_bindings();

#endif
