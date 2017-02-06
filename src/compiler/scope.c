#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "scope.h"
#include "parse.h"
#include "semantics.h"

// TODO: put these guys in a "compiler" section
//

static Package *main_package = NULL;
static Package *current_package = NULL;
static PkgList *pkg_stack = NULL;

Package *init_main_package(char *path) {
    assert(main_package == NULL);
    main_package = calloc(sizeof(Package), 1);
    main_package->path = path;
    main_package->name = "<main>";
    main_package->scope = new_scope(NULL);
    push_current_package(main_package);
    return main_package;
}

void define_global(Var *v) {
    current_package->globals = varlist_append(current_package->globals, v);
}

static VarList *builtin_vars = NULL;

void define_builtin(Var *v) {
    builtin_vars = varlist_append(builtin_vars, v);
}

Var *find_builtin_var(char *name) {
    return varlist_find(builtin_vars, name);
}

static Scope *builtin_types_scope = NULL;
void init_builtin_types() {
    builtin_types_scope = new_scope(NULL);
    init_types(builtin_types_scope);
}
TypeList *builtin_types() {
    assert(builtin_types_scope != NULL);
    return builtin_types_scope->used_types;
}

static PkgList *all_packages = NULL;

static Package *pkglist_find(PkgList *list, char *path);

static Package *package_previously_loaded(char *path) {
    return pkglist_find(all_packages, path);
}

static Package *pkglist_find(PkgList *list, char *path) {
    for (; list != NULL; list = list->next) {
        if (!strcmp(list->item->path, path)) {
            return list->item;
        }
    }
    return NULL;
}

static Package *pkglist_find_by_name(PkgList *list, char *name) {
    for (; list != NULL; list = list->next) {
        if (!strcmp(list->item->name, name)) {
            return list->item;
        }
    }
    return NULL;
}
static PkgList *pkglist_append(PkgList *list, Package *p) {
    PkgList *l = malloc(sizeof(PkgList));
    l->item = p;
    l->next = list;
    if (list != NULL) {
        list->prev = l;
    }
    list = l;
    return list;
}

// TODO: change this
PkgList *all_loaded_packages() {
    return all_packages;
    /*PkgList *head = NULL;*/
    /*while (all_packages != NULL) {*/
        /*head = pkglist_append(head, all_packages->item);*/
        /*all_packages = all_packages->next;*/
    /*}*/
    /*return head;*/
}

void push_current_package(Package *p) {
    pkg_stack = pkglist_append(pkg_stack, p);
    current_package = p;
}

Package *pop_current_package() {
    pkg_stack = pkg_stack->next;
    current_package = pkg_stack->item;
    return current_package;
}

static char *verse_root = NULL;

// TODO: error when path is NULL
Package *load_package(char *current_file, Scope *scope, char *path) {
    // TODO: hardcoded, this should also read env var if available
    if (verse_root == NULL) {
        // if env var is set, use that
        // else:
        verse_root = root_from_binary();
    }
    if (path[0] != '/') {
        int dirlen = strlen(verse_root) + 4; // + "src/"
        int pathlen = strlen(path);
        char *tmp = malloc(sizeof(char) * (dirlen + pathlen + 1));
        snprintf(tmp, dirlen + pathlen + 1, "%ssrc/%s", verse_root, path);
        path = tmp;
    }
    Package *p = package_previously_loaded(path);
    if (p != NULL) {
        if (pkglist_find(scope->packages, path) == NULL) {
            scope->packages = pkglist_append(scope->packages, p);
        }
        return p;
    }

    p = malloc(sizeof(Package));
    p->globals = NULL;
    p->path = path;
    p->scope = new_scope(NULL);
    p->semantics_checked = 0;
    p->name = package_name(path);

    p->files = NULL;

    DIR *d = opendir(path);
    // TODO: better error
    if (d == NULL) {
        error(lineno(), current_file, "Could not load package with path: '%s'", path);
    }

    // push package stack
    push_current_package(p);

    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type != DT_REG) {
            continue;
        }
        int namelen = strlen(ent->d_name);
        if (namelen < 4) {
            continue;
        }
        if (strcmp(ent->d_name + namelen - 3, ".vs")) {
            continue;
        }
        if (namelen > 9 && !strcmp(ent->d_name + namelen - 8, "_test.vs")) {
            continue;
        }

        int len = strlen(path) + namelen;
        char *filepath = malloc(sizeof(char) * (len + 2));
        snprintf(filepath, len + 2, "%s/%s", path, ent->d_name);

        Ast *file_ast = parse_source_file(filepath);
        pop_file_source();
        file_ast = first_pass(p->scope, file_ast);
        PkgFile *f = malloc(sizeof(PkgFile));
        f->name = filepath;
        f->root = file_ast;
        PkgFileList *list = malloc(sizeof(PkgFileList));
        list->item = f;
        if (p->files != NULL) {
            p->files->prev = list;
        }
        list->next = p->files;
        p->files = list;
    }
    if (p->files == NULL) {
        error(lineno(), current_file, "No verse source files found in package '%s' ('%s').", p->name, p->path);
    }
    closedir(d);

    pop_current_package();

    all_packages = pkglist_append(all_packages, p);
    scope->packages = pkglist_append(scope->packages, p);
    return p;
}

Package *lookup_imported_package(Scope *scope, char *name) {
    // go to root
    while (scope->parent != NULL) {
        scope = scope->parent;
    }
    return pkglist_find_by_name(scope->packages, name);
}

static TypeList *used_types = NULL;

TypeList *all_used_types() {
    return used_types;
}
// --

Scope *new_scope(Scope *parent) {
    Scope *s = calloc(sizeof(Scope), 1);
    s->parent = parent;
    s->type = parent != NULL ? Simple : Root;
    return s;
}

Scope *new_fn_scope(Scope *parent) {
    Scope *s = new_scope(parent);
    s->type = Function;
    return s;
}

Scope *new_loop_scope(Scope *parent) {
    Scope *s = new_scope(parent);
    s->type = Loop;
    return s;
}

Scope *closest_loop_scope(Scope *scope) {
    while (scope->parent != NULL && scope->type != Loop) {
        scope = scope->parent;
    }
    return scope;
}

Scope *closest_fn_scope(Scope *scope) {
    while (scope->parent != NULL && scope->type != Function) {
        scope = scope->parent;
    }
    return scope;
}

Type *fn_scope_return_type(Scope *scope) {
    scope = closest_fn_scope(scope);
    if (scope == NULL) {
        return NULL;
    }
    // TODO: don't resolve this?
    return resolve_alias(scope->fn_var->type->fn.ret);
}

Type *lookup_local_type(Scope *s, char *name) {
    if (s->polymorph != NULL) {
        for (TypeDef *td = s->polymorph->defs; td != NULL; td = td->next) {
            if (!strcmp(td->name, name)) {
                return td->type;
            }
        }
    }
    for (TypeDef *td = s->types; td != NULL; td = td->next) {
        if (!strcmp(td->name, name)) {
            return td->type;
        }
    }
    return NULL;
}

Type *lookup_type(Scope *s, char *name) {
    for (; s != NULL; s = s->parent) {
        Type *t = lookup_local_type(s, name);
        if (t != NULL) {
            return t;
        }
    }
    return lookup_local_type(builtin_types_scope, name);
}

void register_type(Type *t) {
    t = resolve_alias(t);
    if (t == NULL) {
        return;
    }
    for (TypeList *list = used_types; list != NULL; list = list->next) {
        if (list->item->id == t->id) {
            return;
        }
    }
    for (TypeList *list = builtin_types_scope->used_types; list != NULL; list = list->next) {
        if (list->item->id == t->id) {
            return;
        }
    }
    used_types = typelist_append(used_types, t);
    switch (t->comp) {
    case STRUCT:
        for (int i = 0; i < t->st.nmembers; i++) {
            register_type(t->st.member_types[i]);
        }
        break;
    case ENUM:
        register_type(t->en.inner);
        break;
    case STATIC_ARRAY:
        register_type(t->array.inner);
        break;
    case ARRAY:
    case REF:
        register_type(t->inner);
        break;
    case FUNC:
        for (TypeList *list = t->fn.args; list != NULL; list = list->next) {
            register_type(list->item);
        }
        register_type(t->fn.ret);
        break;
    default:
        break;
    }
}

Type *define_polymorph(Scope *s, Type *poly, Type *type) {
    assert(poly->comp == POLYDEF);
    assert(s->polymorph != NULL);
    if (lookup_local_type(s, poly->name) != NULL) {
        error(-1, "internal", "Type '%s' already declared within this scope.", poly->name);
    }

    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = poly->name;
    td->type = type;
    td->next = s->polymorph->defs;

    s->polymorph->defs = td;

    return make_type(s, poly->name);
}

int define_polydef_alias(Scope *scope, Type *t) {
    int count = 0;
    switch (t->comp) {
    case POLYDEF:
        define_type(scope, t->name, get_any_type());
        count++;
        break;
    case REF:
    case ARRAY:
        count += define_polydef_alias(scope, t->inner);
        break;
    case STATIC_ARRAY:
        count += define_polydef_alias(scope, t->array.inner);
        break;
    case STRUCT:
        for (int i = 0; i < t->st.nmembers; i++) {
            if (is_polydef(t->st.member_types[i])) {
                count += define_polydef_alias(scope, t->st.member_types[i]);
            }
        }
        break;
    case FUNC:
        for (TypeList *args = t->fn.args; args != NULL; args = args->next) {
            if (is_polydef(args->item)) {
                count += define_polydef_alias(scope, args->item);
            }
        }
        if (is_polydef(t->fn.ret)) {
            count += define_polydef_alias(scope, t->fn.ret);
        }
        break;
    default:
        break;
    }
    return count;
}

Type *define_type(Scope *s, char *name, Type *type) {
    if (lookup_local_type(s, name) != NULL) {
        error(-1, "internal", "Type '%s' already declared within this scope.", name);
    }

    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = name;
    td->type = type;
    td->next = s->types;

    s->types = td;

    type = make_type(s, name);

    if (!(td->type->comp == STRUCT && td->type->st.generic)) {
        register_type(type);
    }
    return type;
}

int local_type_name_conflict(Scope *scope, char *name) {
    return lookup_local_type(scope, name) != NULL;
}

TypeDef *find_type_definition(Type *t) {
    assert(t->comp == ALIAS || t->comp == POLYDEF);
    Scope *scope = t->scope;
    while (scope != NULL) {
        // eh?
        if (scope->polymorph != NULL) {
            for (TypeDef *td = scope->polymorph->defs; td != NULL; td = td->next) {
                if (!strcmp(td->name, t->name)) {
                    return find_type_definition(td->type);
                }
            }
        }
        for (TypeDef *td = scope->types; td != NULL; td = td->next) {
            if (!strcmp(td->name, t->name)) {
                return td;
            }
        }
        scope = scope->parent;
    }
    for (TypeDef *td = builtin_types_scope->types; td != NULL; td = td->next) {
        if (!strcmp(td->name, t->name)) {
            return td;
        }
    }
    return NULL;
}

void attach_var(Scope *scope, Var *var) {
    scope->vars = varlist_append(scope->vars, var);
}

Var *lookup_local_var(Scope *scope, char *name) {
    return varlist_find(scope->vars, name);
}

static Var *_lookup_var(Scope *scope, char *name) {
    Var *v = lookup_local_var(scope, name);
    int in_function = scope->type == Function;
    while (v == NULL && scope->parent != NULL) {
        v = lookup_var(scope->parent, name);
        if (v != NULL) {
            if (!in_function || v->constant) {
                return v;
            }
            v = NULL;
        }
        scope = scope->parent;
        in_function = in_function || scope->type == Function;
    }
    return v;
}

Var *lookup_var(Scope *scope, char *name) {
    Var *v = _lookup_var(scope, name);
    if (v != NULL) {
        return v;
    }

    // package global
    while (scope->parent != NULL) {
        scope = scope->parent;
    }
    v = lookup_local_var(scope, name);
    if (v != NULL) {
        return v;
    }

    v = varlist_find(builtin_vars, name);
    return v;
}

Var *allocate_ast_temp_var(Scope *scope, struct Ast *ast) {
    return make_temp_var(scope, ast->var_type, ast->id);
}

Var *make_temp_var(Scope *scope, Type *t, int id) {
    Var *v = make_var("", t);
    v->temp = 1;
    TempVarList *list = malloc(sizeof(TempVarList));
    list->id = id;
    list->var = v;
    list->next = scope->temp_vars;
    scope->temp_vars = list;
    attach_var(scope, v);
    return v;
}

Var *find_temp_var(Scope *scope, struct Ast *ast) {
    for (TempVarList *list = scope->temp_vars; list != NULL; list = list->next) {
        if (list->id == ast->id) {
            return list->var;
        }
    }
    return NULL;
}
