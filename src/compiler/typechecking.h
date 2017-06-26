#ifndef TYPECHECKING_H
#define TYPECHECKING_H

#include "common.h"
//#include "types.h"

int check_type(Type *a, Type *b);
int can_cast(Type *from, Type *to);
Ast *any_cast(Scope *scope, Ast *ast);
Ast *coerce_type_no_error(Scope *scope, Type *to, Ast *from);
Ast *coerce_type(Scope *scope, Type *to, Ast *from, int raise_error);

int match_polymorph(Scope *scope, Type *expected, Type *got);

Type *promote_number_type(Type *a, int left_lit, Type *b, int right_lit);

#endif
