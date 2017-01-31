#ifndef POLYMORPH_H
#define POLYMORPH_H

#include "ast.h"
#include "types.h"

Polymorph *create_polymorph(AstFnDecl *decl, TypeList *arg_types);
Polymorph *check_for_existing_polymorph(AstFnDecl *decl, TypeList *arg_types);

#endif
