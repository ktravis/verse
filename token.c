#include "token.h"
#include "util.h"

char read_non_space() {
    char c;
    while ((c = getc(stdin)) != EOF) {
        if (!(c == ' ' || c == '\n' || c == '\r')) {
            break;
        }
    }
    return c;
}

int is_id_char(char c) {
    return isalpha(c) || isdigit(c) || c == '_';
}

Tok *make_token(int t) {
    Tok *tok = malloc(sizeof(Tok));
    tok->type = t;
    return tok;
}

Tok *next_token() {
    if (last != NULL) {
        Tok *t = last;
        last = NULL;
        return t;
    }
    char c = read_non_space();
    if (c == EOF) {
        return NULL;
    }
    if (isdigit(c)) {
        return read_integer(c);
    } else if (isalpha(c) || c == '_') {
        return read_identifier(c);
    } else if (c == '\"' || c == '\'') {
        return read_string(c);
    } else if (c == '(') {
        return make_token(TOK_LPAREN);
    } else if (c == ')') {
        return make_token(TOK_RPAREN);
    } else if (c == '{') {
        return make_token(TOK_LBRACE);
    } else if (c == '}') {
        return make_token(TOK_RBRACE);
    } else if (c == ',') {
        return make_token(TOK_COMMA);
    } else if (c == ';') {
        return make_token(TOK_SEMI);
    } else if (c == ':') {
        return make_token(TOK_COLON);
    } else if (c == '+') {
        Tok *t = make_token(TOK_OP);
        t->op = OP_PLUS;
        return t;
    } else if (c == '-') {
        Tok *t = make_token(TOK_OP);
        t->op = OP_MINUS;
        return t;
    } else if (c == '*') {
        Tok *t = make_token(TOK_OP);
        t->op = OP_MUL;
        return t;
    } else if (c == '/') {
        Tok *t = make_token(TOK_OP);
        t->op = OP_DIV;
        return t;
    } else if (c == '^') {
        Tok *t = make_token(TOK_OP);
        t->op = OP_XOR;
        return t;
    } else if (c == '!') {
        int d = getc(stdin);
        if (d != '=') {
            ungetc(d, stdin);
            // TODO UNARY NOT
        } else {
            Tok *t = make_token(TOK_OP);
            t->op = OP_NEQUALS;
            return t;
        }
    } else if (c == '&' || c == '|' || c == '=') {
        int d = getc(stdin);
        int same = (d == c);
        if (!same) {
            ungetc(d, stdin);
        }
        Tok *t = make_token(TOK_OP);
        switch (c) {
        case '&': t->op = same ? OP_AND : OP_BINAND; break;
        case '|': t->op = same ? OP_OR : OP_BINOR; break;
        case '=': t->op = same ? OP_EQUALS : OP_ASSIGN; break;
        }
        return t;
    }
    error("Unexpected character '%c'.", c);
    return NULL;
}

Tok *peek_token() {
    Tok *t = next_token();
    unget_token(t);
    return t;
}

void unget_token(Tok *tok) {
    if (last != NULL) {
        error("Can't unget twice in a row.");
    }
    last = tok;
}

Tok *read_integer(char c) {
    int n = c - '0';
    for (;;) {
        char c = getc(stdin);
        if (!isdigit(c)) {
            ungetc(c, stdin);
            break;
        }
        n = n * 10 + (c - '0');
    }
    Tok *t = make_token(TOK_INT);
    t->ival = n;
    return t;
}

Tok *read_string(char quote) {
    int alloc = 8;
    char *buf = malloc(alloc);
    int len = 0;
    char c;
    while ((c = getc(stdin)) != EOF) {
        if (c == quote && (len == 0 || buf[len-1] != '\\')) {
            Tok *t = make_token(TOK_STR);
            t->sval = buf;
            return t;
        }
        buf[len++] = c;
        if (len == alloc - 1) {
            alloc *= 2;
            buf = realloc(buf, alloc);
        }
    }
    error("EOF encountered while reading string literal.");
    return NULL;
}

Tok *check_reserved(char *buf) {
    if (!strcmp(buf, "true")) {
        Tok *t = make_token(TOK_BOOL);
        t->ival = 1;
        return t;
    } else if (!strcmp(buf, "false")) {
        Tok *t = make_token(TOK_BOOL);
        t->ival = 0;
        return t;
    } else if (!strcmp(buf, "if")) {
        return make_token(TOK_IF);
    } else if (!strcmp(buf, "else")) {
        return make_token(TOK_ELSE);
    }
    return NULL;
}

Tok *read_identifier(char c) {
    int alloc = 8;
    char *buf = malloc(alloc);
    buf[0] = c;
    int len = 1;
    for (;;) {
        c = getc(stdin);
        if (!is_id_char(c)) {
            ungetc(c, stdin);
            break;
        }
        buf[len++] = c;
        if (len == alloc - 1) {
            alloc *= 2;
            buf = realloc(buf, alloc);
        }
    }
    Tok *t = check_reserved(buf);
    if (t == NULL) {
        t = malloc(sizeof(Tok));
        int tid = type_id(buf);
        if (tid) {
            t->type = TOK_TYPE;
            t->tval = tid;
        } else {
            t->type = TOK_ID;
            t->sval = buf;
        }
    }
    return t;
}

int type_id(char *buf) {
    if (!strcmp(buf, "int")) {
        return INT_T;
    } else if (!strcmp(buf, "bool")) {
        return BOOL_T;
    } else if (!strcmp(buf, "string")) {
        return STRING_T;
    } else if (!strcmp(buf, "void")) {
        return VOID_T;
    }
    return 0;
}

int priority_of(Tok *t) {
    if (t->type == TOK_OP) {
        switch (t->op) {
        case OP_ASSIGN:
            return 1;
        case OP_OR:
            return 2;
        case OP_AND:
            return 3;
        case OP_BINOR:
            return 4;
        case OP_XOR:
            return 5;
        case OP_BINAND:
            return 6;
        case OP_EQUALS: case OP_NEQUALS:
            return 7;
        case OP_PLUS: case OP_MINUS:
            return 8;
        case OP_MUL: case OP_DIV:
            return 9;
        default:
            return -1;
        }
    }
    return -1;
}

const char *to_string(Tok *t) {
    if (t == NULL) {
        return "NULL";
    }
    switch (t->type) {
    case TOK_STR: case TOK_ID:
        return t->sval;
    case TOK_INT: {
        int n = 1;
        if (t->ival < 0) n++;
        while ((n /= 10) > 0) {
            n++;
        }
        char *c = malloc(n);
        snprintf(c, n, "%d", t->ival);
        return c;
    }
    case TOK_SEMI:
        return ";";
    case TOK_COLON:
        return ":";
    case TOK_COMMA:
        return ",";
    case TOK_LPAREN:
        return "(";
    case TOK_RPAREN:
        return ")";
    case TOK_LBRACE:
        return "{";
    case TOK_RBRACE:
        return "}";
    case TOK_OP: {
        return op_to_str(t->op);
    }
    default:
        return NULL;
    }
}

const char *token_type(int type) {
    switch (type) {
    case TOK_STR:
        return "STR";
    case TOK_ID:
        return "ID";
    case TOK_INT:
        return "INT";
    case TOK_SEMI:
        return "SEMI";
    case TOK_COLON:
        return "COLON";
    case TOK_COMMA:
        return "COMMA";
    case TOK_LPAREN:
        return "LPAREN";
    case TOK_RPAREN:
        return "RPAREN";
    case TOK_OP:
        return "OP";
    default:
        return "BAD TOKEN";
    }
}

const char *op_to_str(int op) {
    switch (op) {
    case OP_PLUS: return "+";
    case OP_MINUS: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_XOR: return "^";
    case OP_BINAND: return "&";
    case OP_BINOR: return "|";
    case OP_ASSIGN: return "=";
    case OP_AND: return "&&";
    case OP_OR: return "||";
    case OP_EQUALS: return "==";
    case OP_NEQUALS: return "!=";
    default:
        return "BAD OP";
    }
}
