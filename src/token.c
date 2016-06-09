#include "token.h"
#include "util.h"

typedef struct FileStack {
    char *name;
    int line;
    FILE *handle;
    struct FileStack *next;
} FileStack;

static TokList *unwind_stack = NULL;
/*static FILE *source_file = NULL;*/
/*static char *source_file_name = NULL;*/
static FileStack *source_stack = NULL;

void set_file_source(char *name, FILE *f) {
    FileStack *stack = malloc(sizeof(struct FileStack));
    stack->name = name;
    stack->line = 1;
    stack->handle = f;
    stack->next = source_stack;
    source_stack = stack;
}

void pop_file_source() {
    if (source_stack->handle != stdin) {
        fclose(source_stack->handle);
    }
    source_stack = source_stack->next;
    /*if (source_stack == NULL) {*/
        /*source_file = source_stack->handle;*/
        /*source_file_name = source_stack->name;*/
    /*} else {*/
        /*source_file = stdin;*/
        /*source_file_name = "<stdin>";*/
    /*}*/
}

char get_char() {
    return fgetc(source_stack->handle);
}

void unget_char(char c) {
    ungetc(c, source_stack->handle);
}

void read_block_comment() {
    int count = 1;
    char c;
    int start = source_stack->line;
    while ((c = get_char()) != EOF) {
        if (c == '/') {
            c = get_char();
            if (c == '*') {
                count++;
            }
        } else if (c == '*') {
            c = get_char();
            if (c == '/') {
                count--;
                if (count == 0) {
                    return;
                }
            }
        }
        if (c == '\n') {
            source_stack->line++;
        }
    }
    error(start, current_file_name(), "Incompleted block comment");
}

char read_non_space() {
    char c, d;
    while ((c = get_char()) != EOF) {
        if (c == '/') {
            d = get_char();
            if (d == '/') {
                while (c != '\n') {
                    c = get_char();
                }
            } else if (d == '*') {
                read_block_comment();
                c = get_char();
            } else {
                unget_char(d);
                break;
            }
        }
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
            break;
        } else if (c == '\n') {
            source_stack->line++;
        }
    }
    return c;
}

TokList *toklist_append(TokList *list, Tok *t) {
    TokList *out = malloc(sizeof(TokList));
    out->next = list;
    out->item = t;
    return out;
}
TokList *reverse_toklist(TokList *list) {
    TokList *tail = list;
    if (tail == NULL) {
        return NULL;
    }
    TokList *head = tail;
    TokList *tmp = head->next;
    head->next = NULL;
    while (tmp != NULL) {
        tail = head;
        head = tmp;
        tmp = tmp->next;
        head->next = tail;
    }
    return head;
}

int is_id_char(char c) {
    return isalpha(c) || isdigit(c) || c == '_';
}

Tok *make_token(int t) {
    Tok *tok = malloc(sizeof(Tok));
    tok->type = t;
    return tok;
}

int expect_line_break() {
    char c = get_char();
    while (c == ' ') {
        c = get_char();
    }
    if (c == '\n') {
        source_stack->line++;
        return 1;
    }
    unget_char(c);
    return 0;
}
int expect_line_break_or_semicolon() {
    if (expect_line_break()) {
        return 1;
    }
    char c = get_char();
    if (c == ';') {
        return 1;
    }
    unget_char(c);
    return 0;
}

Tok *next_token() {
    if (unwind_stack != NULL) {
        Tok *t = unwind_stack->item;
        unwind_stack = unwind_stack->next;
        return t;
    }
    char c = read_non_space();
    if (c == EOF) {
        return NULL;
    }
    if (isdigit(c)) {
        return read_number(c);
    } else if (isalpha(c) || c == '_') {
        return read_identifier(c);
    } else if (c == '\'') {
        Tok *t = make_token(TOK_CHAR);
        c = get_char();
        if (c == '\\') {
            c = get_char();
        }
        t->ival = c;
        c = get_char();
        if (c != '\'') {
            error(lineno(), current_file_name(), "Invalid character literal sequence.");
        }
        return t;
    } else if (c == '\"') {
        Tok *t = make_token(TOK_STR);
        t->sval = read_string();
        return t;
    } else if (c == '(') {
        return make_token(TOK_LPAREN);
    } else if (c == ')') {
        return make_token(TOK_RPAREN);
    } else if (c == '[') {
        return make_token(TOK_LSQUARE);
    } else if (c == ']') {
        return make_token(TOK_RSQUARE);
    } else if (c == '#') {
        c = get_char();
        if (c == '{') {
            return make_token(TOK_STARTBIND);
        } else if (isalpha(c) || c == '_') {
            // TODO refactor this
            Tok *t = make_token(TOK_DIRECTIVE);
            int alloc = 8;
            char *buf = malloc(alloc);
            buf[0] = c;
            int len = 1;
            for (;;) {
                c = get_char();
                if (!is_id_char(c)) {
                    unget_char(c);
                    break;
                }
                buf[len++] = c;
                if (len == alloc - 1) {
                    alloc *= 2;
                    buf = realloc(buf, alloc);
                }
            }
            buf[len] = 0;
            t->sval = buf;
            return t;
        } else {
            error(lineno(), current_file_name(), "Unexpected character sequence '#%c'", c);
        }
    } else if (c == '{') {
        return make_token(TOK_LBRACE);
    } else if (c == '}') {
        return make_token(TOK_RBRACE);
    } else if (c == ',') {
        return make_token(TOK_COMMA);
    } else if (c == '.') {
        if ((c = get_char()) == '.') { // TODO are multiple dots in a row (but not three) ever valid?
            if ((c = get_char()) == '.') {
                return make_token(TOK_ELLIPSIS); 
            }
            error(lineno(), current_file_name(), "Unexected character '%c' following '..'", c);
        } else {
            unget_char(c);
        }
        Tok *t = make_token(TOK_OP);
        t->op = OP_DOT;
        return t;
    } else if (c == ';') {
        return make_token(TOK_SEMI);
    } else if (c == ':') {
        return make_token(TOK_COLON);
    } else if (c == '+') {
        char n = get_char();
        Tok *t = NULL;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_PLUS;
        return t;
    } else if (c == '-') {
        char n = get_char();
        Tok *t = NULL;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_MINUS;
        return t;
    } else if (c == '*') {
        char n = get_char();
        Tok *t = NULL;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_MUL;
        return t;
    } else if (c == '/') {
        char n = get_char();
        Tok *t = NULL;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_DIV;
        return t;
    } else if (c == '^') {
        char n = get_char();
        Tok *t = NULL;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_XOR;
        return t;
    } else if (c == '&') {
        char n = get_char();
        Tok *t = NULL;
        int op = OP_BINAND;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else if (n == '&') {
            t = make_token(TOK_OP);
            op = OP_AND;
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = op;
        return t;
    } else if (c == '>') {
        Tok *t = make_token(TOK_OP);
        int d = get_char();
        if (d == '=') {
            t->op = OP_GTE;
        } else {
            unget_char(d);
            t->op = OP_GT;
        } // TODO binary shift
        return t;
    } else if (c == '<') {
        Tok *t = make_token(TOK_OP);
        int d = get_char();
        if (d == '=') {
            t->op = OP_LTE;
        } else {
            unget_char(d);
            t->op = OP_LT;
        } // TODO binary shift
        return t;
    } else if (c == '!') {
        Tok *t;
        int d = get_char();
        if (d == '=') {
            t = make_token(TOK_OP);
            t->op = OP_NEQUALS;
        } else {
            unget_char(d);
            t = make_token(TOK_UOP);
            t->op = OP_NOT;
        }
        return t;
    } else if (c == '|' || c == '=') {
        int d = get_char();
        int same = (d == c);
        if (!same) {
            unget_char(d);
        }
        Tok *t = make_token(TOK_OP);
        switch (c) {
        case '&': t->op = same ? OP_AND : OP_BINAND; break;
        case '|': t->op = same ? OP_OR : OP_BINOR; break;
        case '=': t->op = same ? OP_EQUALS : OP_ASSIGN; break;
        }
        return t;
    }
    error(lineno(), current_file_name(), "Unexpected character '%c'.", c);
    return NULL;
}

Tok *peek_token() {
    Tok *t = next_token();
    unget_token(t);
    return t;
}

void unget_token(Tok *tok) {
    unwind_stack = toklist_append(unwind_stack, tok);
}

double read_decimal(char c) {
    double d = 0;
    double n = 10;

    for (;;) {
        if (!isdigit(c)) {
            unget_char(c);
            break;
        }
        d += (c - '0') / n;
        n *= 10;
        c = get_char();
    }
    return d;
}

int is_hex_digit(char c) {
    return isdigit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

int hex_val(char c) {
    if ('a' <= c && c < 'f') {
        return c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    }
    return c - '0';
}

Tok *read_hex_number(char c) {
    long n = hex_val(c);
    for (;;) {
        char c = get_char();
        if (!is_hex_digit(c)) {
            unget_char(c);
            break;
        }
        n = n * 16 + hex_val(c);
    }
    Tok *t = make_token(TOK_INT);
    t->ival = n;
    return t;
}

Tok *read_number(char c) {
    if (c == '0') {
        c = get_char();
        if (c == 'x') {
            c = get_char();
            if (!is_hex_digit(c)) {
                error(lineno(), current_file_name(), "Invalid hexadecimal literal '0x%c'", c);
            }
            return read_hex_number(c);
        }
        unget_char(c);
        c = '0';
    }
    long n = c - '0';
    for (;;) {
        char c = get_char();
        if (!isdigit(c)) {
            if (c == '.') {
                Tok *t = make_token(TOK_FLOAT);
                c = get_char();
                if (!isdigit(c)) {
                    if (c == '.') { // reading a possible ellipsis?
                        unget_char(c);
                        unget_char('.');
                        break;
                    }
                    error(lineno(), current_file_name(), "Unexpected non-numeric character '%c' while reading float.", c);
                }
                t->fval = n + read_decimal(c);
                return t;
            }
            unget_char(c);
            break;
        }
        n = n * 10 + (c - '0');
    }
    Tok *t = make_token(TOK_INT);
    t->ival = n;
    return t;
}

char *read_string() {
    int alloc = 8;
    char *buf = malloc(alloc);
    int len = 0;
    char c;
    int start = source_stack->line;
    while ((c = get_char()) != EOF) {
        if (c == '\"' && (len == 0 || buf[len-1] != '\\')) {
            buf[len] = 0;
            return buf;
        }
        buf[len++] = c;
        if (len == alloc - 1) {
            alloc *= 2;
            buf = realloc(buf, alloc);
        }
    }
    error(start, current_file_name(), "EOF encountered while reading string literal.");
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
    } else if (!strcmp(buf, "fn")) {
        return make_token(TOK_FN);
    } else if (!strcmp(buf, "return")) {
        return make_token(TOK_RETURN);
    } else if (!strcmp(buf, "if")) {
        return make_token(TOK_IF);
    } else if (!strcmp(buf, "else")) {
        return make_token(TOK_ELSE);
    } else if (!strcmp(buf, "while")) {
        return make_token(TOK_WHILE);
    } else if (!strcmp(buf, "for")) {
        return make_token(TOK_FOR);
    } else if (!strcmp(buf, "in")) {
        return make_token(TOK_IN);
    } else if (!strcmp(buf, "extern")) {
        return make_token(TOK_EXTERN);
    } else if (!strcmp(buf, "type")) {
        return make_token(TOK_TYPE);
    } else if (!strcmp(buf, "struct")) {
        return make_token(TOK_STRUCT);
    } else if (!strcmp(buf, "hold")) {
        return make_token(TOK_HOLD);
    } else if (!strcmp(buf, "release")) {
        return make_token(TOK_RELEASE);
    } else if (!strcmp(buf, "break")) {
        return make_token(TOK_BREAK);
    } else if (!strcmp(buf, "continue")) {
        return make_token(TOK_CONTINUE);
    } else if (!strcmp(buf, "enum")) {
        return make_token(TOK_ENUM);
    } else if (!strcmp(buf, "as")) {
        Tok *t = make_token(TOK_OP);
        t->op = OP_CAST;
        return t;
    }
    return NULL;
}

Tok *read_identifier(char c) {
    int alloc = 8;
    char *buf = malloc(alloc);
    buf[0] = c;
    int len = 1;
    for (;;) {
        c = get_char();
        if (!is_id_char(c)) {
            unget_char(c);
            break;
        }
        buf[len++] = c;
        if (len == alloc - 1) {
            alloc *= 2;
            buf = realloc(buf, alloc);
        }
    }
    buf[len] = 0;
    Tok *t = check_reserved(buf);
    if (t == NULL) {
        t = make_token(TOK_ID);
        t->sval = buf;
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
    } else if (!strcmp(buf, "auto")) {
        return AUTO_T;
    } else if (!strcmp(buf, "ptr")) {
        return BASEPTR_T;
    }
    return 0;
}

int valid_unary_op(int op) {
    switch (op) {
    case OP_NOT:
    case OP_PLUS:
    case OP_MINUS:
    case OP_REF:
    case OP_MUL:
    case OP_BINAND:
        return 1;
    }
    return 0;
}

int priority_of(Tok *t) {
    if (t->type == TOK_LPAREN || t->type == TOK_LSQUARE) {
        return 13;
    } else if (t->type == TOK_OPASSIGN) {
        return 1;
    } else if (t->type == TOK_OP || t->type == TOK_UOP) {
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
        case OP_GT: case OP_GTE: case OP_LT: case OP_LTE:
            return 8;
        case OP_PLUS: case OP_MINUS:
            return 9;
        case OP_MUL: case OP_DIV:
            return 10;
        case OP_NOT:
            return 11;
        case OP_CAST:
            return 12; // TODO this priority might be wrong
        case OP_REF: case OP_DEREF:
            return 13;
        case OP_DOT:
            return 14;
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
    case TOK_LSQUARE:
        return "[";
    case TOK_RSQUARE:
        return "]";
    case TOK_LBRACE:
        return "{";
    case TOK_RBRACE:
        return "}";
    case TOK_FN:
        return "fn";
    case TOK_OP:
    case TOK_UOP:
        return op_to_str(t->op);
    case TOK_OPASSIGN: {
        const char *op = op_to_str(t->op);
        char *m = malloc(sizeof(char) * (strlen(op) + 1));
        sprintf(m, "%s=", op);
        m[strlen(op)+1] = 0;
        return m;
    }
    case TOK_RETURN:
        return "return";
    case TOK_STRUCT:
        return "struct";
    case TOK_TYPE:
        return "type";
    /*case TOK_TYPE:*/
        /*return type_as_str(make_type(t->tval));*/
    case TOK_HOLD:
        return "hold";
    case TOK_RELEASE:
        return "release";
    case TOK_WHILE:
        return "while";
    case TOK_FOR:
        return "for";
    case TOK_IN:
        return "in";
    case TOK_BREAK:
        return "break";
    case TOK_CONTINUE:
        return "continue";
    case TOK_ELLIPSIS:
        return "...";
    case TOK_ENUM:
        return "enum";
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
    case TOK_LSQUARE:
        return "LSQURE";
    case TOK_RSQUARE:
        return "RSQURE";
    case TOK_OPASSIGN:
        return "OPASSIGN";
    case TOK_OP:
        return "OP";
    case TOK_UOP:
        return "UOP";
    case TOK_FN:
        return "FN";
    case TOK_RETURN:
        return "RETURN";
    case TOK_STRUCT:
        return "STRUCT";
    case TOK_TYPE:
        return "TYPE";
    case TOK_HOLD:
        return "HOLD";
    case TOK_RELEASE:
        return "RELEASE";
    case TOK_WHILE:
        return "WHILE";
    case TOK_FOR:
        return "FOR";
    case TOK_IN:
        return "IN";
    case TOK_BREAK:
        return "BREAK";
    case TOK_CONTINUE:
        return "CONTINUE";
    case TOK_ELLIPSIS:
        return "ELLIPSIS";
    case TOK_ENUM:
        return "ENUM";
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
    case OP_NOT: return "!";
    case OP_GT: return ">";
    case OP_GTE: return ">=";
    case OP_LT: return "<";
    case OP_LTE: return "<=";
    case OP_DOT: return ".";
    case OP_REF: return "&";
    case OP_DEREF: return "*";
    case OP_CAST: return "as";
    default:
        return "BAD OP";
    }
}

Tok *expect(int type) {
    Tok *t = next_token();
    if (t == NULL || t->type != type) {
        error(lineno(), current_file_name(), "Expected token of type '%s', got '%s'.", token_type(type), to_string(t));
    }
    return t;
}

int is_comparison(int op) {
    return op == OP_EQUALS || op == OP_NEQUALS || op == OP_GT ||
        op == OP_GTE || op == OP_LT || op == OP_LTE;
}

int lineno() {
    if (source_stack == NULL) {
        error(-1, "internal", "Trying to get line number with no source file");
    }
    return source_stack->line;
}

char *current_file_name() {
    if (source_stack == NULL) {
        error(-1, "internal", "No file on the source stack!");
    }
    return source_stack->name;
}
