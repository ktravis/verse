#include "eval.h"

Ast *eval_float_binop(Ast *ast) {
    double fval = ast->left->type == AST_FLOAT ? ast->left->fval : ast->left->ival;
    double r = ast->right->type == AST_FLOAT ? ast->right->fval : ast->right->ival;
    switch (ast->op) {
        case OP_PLUS: fval += r; break;
        case OP_MINUS: fval -= r; break;
        case OP_MUL: fval *= r; break;
        case OP_DIV: fval /= r; break;
        case OP_EQUALS: return make_ast_bool(fval == r);
        case OP_NEQUALS: return make_ast_bool(fval != r);
        case OP_GT: return make_ast_bool(fval > r);
        case OP_GTE: return make_ast_bool(fval >= r);
        case OP_LT: return make_ast_bool(fval < r);
        case OP_LTE: return make_ast_bool(fval <= r);
        default: error(ast->line, "Unknown binary operator '%s'.", op_to_str(ast->op));
    }
    Ast *ret = ast->left->type == AST_FLOAT ? ast->left : ast->right;
    ret->fval = fval;
    return ret;
}

Ast *eval_int_binop(Ast *ast) {
    long ival = ast->left->ival;
    long r = ast->right->ival;
    switch (ast->op) {
        case OP_PLUS: ival += r; break;
        case OP_MINUS: ival -= r; break;
        case OP_MUL: ival *= r; break;
        case OP_DIV: ival /= r; break;
        case OP_XOR: ival ^= r; break;
        case OP_BINAND: ival &= r; break;
        case OP_BINOR: ival |= r; break;
        case OP_EQUALS: return make_ast_bool(ival == r);
        case OP_NEQUALS: return make_ast_bool(ival != r);
        case OP_GT: return make_ast_bool(ival > r);
        case OP_GTE: return make_ast_bool(ival >= r);
        case OP_LT: return make_ast_bool(ival < r);
        case OP_LTE: return make_ast_bool(ival <= r);
        default: error(ast->line, "Unknown binary operator '%s'.", op_to_str(ast->op));
    }
    ast->left->ival = ival;
    return ast->left;
}

int is_string_literal(Ast *ast) {
    return ast->type == AST_STRING || (ast->type == AST_TEMP_VAR && ast->expr->type == AST_STRING);
}

Ast *eval_string_binop(Ast *ast) {
    Ast *l = ast->left->type == AST_TEMP_VAR ? ast->left->expr : ast->left;
    Ast *r = ast->right->type == AST_TEMP_VAR ? ast->right->expr : ast->right;
    switch (ast->op) {
        case OP_PLUS: {
            char *s = malloc((strlen(l->sval) + strlen(r->sval) + 1) * (sizeof(char)));
            sprintf(s, "%s%s", l->sval, r->sval);
            s[strlen(l->sval) + strlen(r->sval)] = 0;
            free(l->sval); // hopefully nothing messed up by this
            free(r->sval);
            l->sval = s;
            return l;
        }
        case OP_EQUALS: return make_ast_bool(!strcmp(l->sval, r->sval));
        case OP_NEQUALS: return make_ast_bool(strcmp(l->sval, r->sval));
        default: error(ast->line, "Unknown binary operator '%s' for type string.", op_to_str(ast->op));
    }
    return NULL;
}

Ast *eval_const_uop(Ast *ast) {
    // assume semantics has been checked already, important!
    switch (ast->right->type) {
    case AST_INTEGER:
        if (ast->op == OP_PLUS) {
            ast->right->ival = +ast->right->ival;
        } else if (ast->op == OP_MINUS) {
            ast->right->ival = -ast->right->ival;
        }
        return ast->right;
    case AST_FLOAT:
        if (ast->op == OP_PLUS) {
            ast->right->fval = +ast->right->fval;
        } else if (ast->op == OP_MINUS) {
            ast->right->fval = -ast->right->fval;
        }
        return ast->right;
    case AST_BOOL:
        if (ast->op == OP_NOT) {
            ast->right->ival = !ast->right->ival;
        }
        return ast->right;
    }
    error(ast->line, "Cannot evaluate constant unary op of type %d.", ast->right->type);
    return NULL;
}

// TODO static array .length is const!

Ast *eval_const_binop(Ast *ast) {
    // Need to make sure types are checked prior to this
    if (ast->left->type == AST_FLOAT || ast->left->type == AST_INTEGER) {
        if (ast->left->type == AST_FLOAT || ast->right->type == AST_FLOAT) {
            return eval_float_binop(ast);
        }
        return eval_int_binop(ast);
    } else if (ast->left->type == AST_BOOL && ast->right->type == AST_BOOL) {
        if (ast->op == OP_AND) {
            if (ast->left->ival) {
                return ast->right;
            }
            return ast->left;
        } else if (ast->op == OP_OR) {
            if (ast->left->ival) {
                return ast->left;
            }
            return ast->right;
        }
        error (ast->line, "Unrecognized operator for bool types: '%s'.", op_to_str(ast->op));
        return NULL;
    } else if (is_string_literal(ast->left) && is_string_literal(ast->right)) {
        return eval_string_binop(ast);
    }
    error (ast->line, "Cannot evaluate constant binop of type %d.", ast->left->type);
    return NULL;
}
