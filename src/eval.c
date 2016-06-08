#include "eval.h"

Ast *eval_float_binop(Ast *ast) {
    AstLiteral *left = ast->binary->left->lit;
    AstLiteral *right = ast->binary->right->lit;
    double fval = left->lit_type == FLOAT ? left->float_val : left->int_val;
    double r = right->lit_type == FLOAT ? right->float_val : right->int_val;
    switch (ast->binary->op) {
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
        default: error(ast->line, ast->file, "Unknown binary operator '%s'.", op_to_str(ast->binary->op));
    }
    Ast *ret = left->lit_type == FLOAT ? ast->binary->left : ast->binary->right;
    ret->lit->float_val = fval;
    return ret;
}

Ast *eval_int_binop(Ast *ast) {
    long ival = ast->binary->left->lit->int_val;
    long r = ast->binary->right->lit->int_val;
    switch (ast->binary->op) {
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
        default: error(ast->line, ast->file, "Unknown binary operator '%s'.", op_to_str(ast->binary->op));
    }
    ast->binary->left->lit->int_val = ival;
    return ast->binary->left;
}

int is_string_literal(Ast *ast) {
    return (ast->type == AST_LITERAL && ast->lit->lit_type == STRING) ||
        (ast->type == AST_TEMP_VAR && ast->tempvar->expr->type == AST_LITERAL &&
         ast->tempvar->expr->lit->lit_type == STRING);
}

Ast *eval_string_binop(Ast *ast) {
    Ast *left = ast->binary->left;
    Ast *right = ast->binary->right;
    AstLiteral *l = left->type == AST_TEMP_VAR ? left->tempvar->expr->lit : left->lit;
    AstLiteral *r = right->type == AST_TEMP_VAR ? right->tempvar->expr->lit : right->lit;
    switch (ast->binary->op) {
        case OP_PLUS: {
            char *s = malloc((strlen(l->string_val) + strlen(r->string_val) + 1) * (sizeof(char)));
            sprintf(s, "%s%s", l->string_val, r->string_val);
            s[strlen(l->string_val) + strlen(r->string_val)] = 0;
            free(l->string_val); // hopefully nothing messed up by this
            free(r->string_val);
            l->string_val = s;
            return left;
        }
        case OP_EQUALS: return make_ast_bool(!strcmp(l->string_val, r->string_val));
        case OP_NEQUALS: return make_ast_bool(strcmp(l->string_val, r->string_val));
        default: error(ast->line, ast->file, "Unknown binary operator '%s' for type string.", op_to_str(ast->binary->op));
    }
    return NULL;
}

Ast *eval_const_uop(Ast *ast) {
    // assume semantics has been checked already, important!
    AstLiteral *lit = ast->unary->object->lit;
    int op = ast->unary->op;

    switch (ast->unary->object->lit->lit_type) {
    case INTEGER:
        if (op == OP_PLUS) {
            lit->int_val = +lit->int_val;
        } else if (op == OP_MINUS) {
            lit->int_val = -lit->int_val;
        }
        return ast->unary->object;
    case FLOAT:
        if (op == OP_PLUS) {
            lit->float_val = +lit->float_val;
        } else if (op == OP_MINUS) {
            lit->float_val = -lit->float_val;
        }
        return ast->unary->object;
    case BOOL:
        if (op == OP_NOT) {
            lit->int_val = !lit->int_val;
        }
        return ast->unary->object;
    default:
        break;
    }
    error(ast->line, ast->file, "Cannot evaluate constant unary op of type %d.", lit->lit_type);
    return NULL;
}

Ast *eval_const_binop(Ast *ast) {
    // Need to make sure types are checked prior to this
    int op = ast->binary->op;
    AstLiteral *left_lit = ast->binary->left->lit;
    AstLiteral *right_lit = ast->binary->right->lit;

    if (left_lit->lit_type == FLOAT || left_lit->lit_type == INTEGER || left_lit->lit_type == CHAR) {
        if (left_lit->lit_type == FLOAT || right_lit->lit_type == FLOAT) {
            return eval_float_binop(ast);
        }
        return eval_int_binop(ast);
    } else if (left_lit->lit_type == BOOL && right_lit->lit_type == BOOL) {
        if (op == OP_AND) {
            if (left_lit->int_val) {
                return ast->binary->right;
            }
            return ast->binary->left;
        } else if (op == OP_OR) {
            if (left_lit->int_val) {
                return ast->binary->left;
            }
            return ast->binary->right;
        }
        error (ast->line, ast->file, "Unrecognized operator for bool types: '%s'.", op_to_str(op));
        return NULL;
    } else if (is_string_literal(ast->binary->left) && is_string_literal(ast->binary->right)) {
        return eval_string_binop(ast);
    }
    error (ast->line, ast->file, "Cannot evaluate constant binop of type %d.", left_lit->lit_type);
    return NULL;
}
