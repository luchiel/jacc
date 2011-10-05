#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "log.h"

char *content;
int content_size;
int content_pos;

FILE *error_stream;

buffer_t buffer;
char cur_char;
char next_char;

int line, column;
int offset;

/* Keep in sync with lexer.h token_type_t */
const char* token_names[] = {
    "STRING_CONST",
    "INT_CONST",
    "FLOAT_CONST",
    "IDENT",
    "COMMENT",

    "BREAK",
    "CASE",
    "CHAR",
    "CONST",
    "CONTINUE",
    "DEFAULT",
    "DO",
    "DOUBLE",
    "ELSE",
    "ENUM",
    "EXTERN",
    "FLOAT",
    "FOR",
    "IF",
    "INT",
    "REGISTER",
    "RETURN",
    "SIZEOF",
    "STATIC",
    "STRUCT",
    "SWITCH",
    "TYPEDEF",
    "UNION",
    "VOID",
    "WHILE",

    "LBRACKET",
    "RBRACKET",
    "LBRACE",
    "RBRACE",
    "LPAREN",
    "RPAREN",

    "DOT",
    "COMMA",
    "AMP",
    "TILDE",
    "QUESTION",
    "COLON",
    "SEMICOLON",
    "ELLIPSIS",
    "STAR",

    "REF_OP",

    "INC_OP",
    "DEC_OP",

    "ADD_OP",
    "SUB_OP",
    "DIV_OP",
    "MOD_OP",

    "NEG_OP",

    "LSHIFT_OP",
    "RSHIFT_OP",

    "LT_OP",
    "LE_OP",
    "GT_OP",
    "GE_OP",
    "EQUAL_OP",
    "NOT_EQUAL_OP",

    "BIT_XOR_OP",
    "BIT_OR_OP",

    "AND_OP",
    "OR_OP",

    "ASSIGN",
    "MUL_ASSIGN",
    "DIV_ASSIGN",
    "MOD_ASSIGN",
    "ADD_ASSIGN",
    "SUB_ASSIGN",
    "LSHIFT_ASSIGN",
    "RSHIFT_ASSIGN",
    "BIT_OR_ASSIGN",
    "BIT_AND_ASSIGN",
    "BIT_XOR_ASSIGN",

    "EOS",
    "UNKNOWN",
};

const char *idents[] = {
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "if",
    "int",
    "register",
    "return",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "void",
    "while",
};

void get_char()
{
    cur_char = next_char;
    offset++;
    column++;
    if (offset < content_size) {
        next_char = content[offset];
    } else {
        next_char = 0;
    }
}

void lexer_init(char *content_, int size_, FILE *error_stream_)
{
    content = content_;
    content_size = size_;
    error_stream = error_stream_;
    offset = -1;
    line = 1;
    column = -1;
    buffer = buffer_create(1024);

    get_char();
    get_char();
}

void lexer_destroy()
{
    buffer_free(buffer);
}

int is_digit(char chr)
{
    return chr >= '0' && chr <= '9';
}

int is_octdigit(char chr)
{
    return chr >= '0' && chr <= '7';
}

int is_hexdigit(char chr)
{
    return is_digit(chr)
        || (chr >= 'a' && chr <= 'f')
        || (chr >= 'A' && chr <= 'F');
}

int is_digit_ex(char chr, int base)
{
    if (base == 10) {
        return is_digit(chr);
    } else if (base == 8) {
        return is_octdigit(chr);
    } else if (base == 16) {
        return is_hexdigit(chr);
    }
    return 0;
}

int is_alpha(char chr)
{
    return (chr >= 'a' && chr <= 'z')
        || (chr >= 'A' && chr <= 'Z');
}

int is_ident_start(char chr)
{
    return is_alpha(chr) || chr == '_';
}

int is_ident(char chr)
{
    return is_ident_start(chr) || is_digit(chr);
}

int is_newline(char chr)
{
    return chr == '\n' || chr == '\r';
}

int is_whitespace(char chr)
{
    return is_newline(chr)
        || chr == '\t' || chr == '\v'
        || chr == ' ';
}

void skip_ws()
{
    while (is_whitespace(cur_char)) {
        if (cur_char == '\n') {
            column = 0;
            line++;
        }
        get_char();
    }
}

int digit_value(char c)
{
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xA;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 0xA;
    }
    return c - '0';
}

void lexer_error(struct token *token, const char *message)
{
    token->type = TOK_ERROR;
    log_set_pos(token->line, token->column);
    log_error(message);
}

void get_scalar(struct token *token)
{
    int result = 0, base = 10;

    if (cur_char == '0') {
        if (next_char == 'x' || next_char == 'X') {
            get_char();
            get_char();
            base = 16;
        } else if (is_digit(next_char)) {
            get_char();
            base = 8;
        }
    }

    if (!is_digit_ex(cur_char, base)) {
        lexer_error(token, "bad integer constant");
        return;
    }

    while (is_digit_ex(cur_char, base)) {
        result = result * base + digit_value(cur_char);
        get_char();
    }

    if (base == 8 && is_digit(cur_char)) {
        lexer_error(token, "invalid digit in octal constant");
        return;
    }

    buffer_reset(buffer);
    while (is_alpha(cur_char)) {
        buffer_append(buffer, cur_char);
        get_char();
    }

    if (buffer_size(buffer) > 0) {
        lexer_error(token, "unknown suffix on integer constant");
        return;
    }

    token->type = TOK_INT_CONST;
    token->value.int_val = result;
}

enum token_type get_ident_type(const char *ident)
{
    /* TODO use hash table instead? */
    int i;
    for (i = 0; i <= TOK_WHILE - TOK_BREAK; i++) {
        if (strcmp(ident, idents[i]) == 0) {
            return i + TOK_BREAK;
        }
    }
    return TOK_IDENT;
}

void get_ident(struct token *token)
{
    buffer_reset(buffer);
    do {
        buffer_append(buffer, cur_char);
        get_char();
    } while (is_ident(cur_char));

    buffer_append(buffer, 0);

    token->type = get_ident_type(buffer_data(buffer));
    if (token->type == TOK_IDENT) {
        token->value.str_val = buffer_data_copy(buffer);
    }
}

enum token_type get_1way_punctuator(enum token_type type)
{
    get_char();
    return type;
}

enum token_type get_2way_punctuator(enum token_type t1,
        enum token_type t2)
{
    if (next_char == '=') {
        get_char();
        return get_1way_punctuator(t2);
    }
    return get_1way_punctuator(t1);
}

enum token_type get_3way_punctuator(enum token_type t1,
        enum token_type t2, enum token_type t3)
{
    if (next_char == cur_char) {
        get_char();
        return get_1way_punctuator(t3);
    }
    return get_2way_punctuator(t1, t2);
}

enum token_type get_punctuator_type()
{
    switch (cur_char) {
        case '(': return get_1way_punctuator(TOK_LPAREN);
        case ')': return get_1way_punctuator(TOK_RPAREN);
        case '[': return get_1way_punctuator(TOK_LBRACKET);
        case ']': return get_1way_punctuator(TOK_RBRACKET);
        case '{': return get_1way_punctuator(TOK_LBRACE);
        case '}': return get_1way_punctuator(TOK_RBRACE);

        case ',': return get_1way_punctuator(TOK_COMMA);
        case '~': return get_1way_punctuator(TOK_TILDE);
        case '?': return get_1way_punctuator(TOK_QUESTION);
        case ';': return get_1way_punctuator(TOK_SEMICOLON);

        case '/': return get_2way_punctuator(TOK_DIV_OP, TOK_DIV_ASSIGN);
        case '*': return get_2way_punctuator(TOK_STAR, TOK_MUL_ASSIGN);

        case '!': return get_2way_punctuator(TOK_NEG_OP, TOK_NOT_EQUAL_OP);
        case '=': return get_2way_punctuator(TOK_ASSIGN, TOK_EQUAL_OP);
        case '^': return get_2way_punctuator(TOK_BIT_XOR_OP, TOK_BIT_XOR_ASSIGN);

        case '+': return get_3way_punctuator(TOK_ADD_OP, TOK_ADD_ASSIGN, TOK_INC_OP);
        case '|': return get_3way_punctuator(TOK_BIT_OR_OP, TOK_BIT_OR_ASSIGN, TOK_OR_OP);
        case '&': return get_3way_punctuator(TOK_AMP, TOK_BIT_AND_ASSIGN, TOK_AND_OP);

        case ':':
            if (next_char == '>') {
                get_char();
                return get_1way_punctuator(TOK_RBRACKET);
            }
            return get_1way_punctuator(TOK_COLON);
        case '%':
            if (next_char == '>') {
                get_char();
                return get_1way_punctuator(TOK_RBRACE);
            }
            return get_2way_punctuator(TOK_MOD_OP, TOK_MOD_ASSIGN);
        case '<':
            if (next_char == '<') {
                get_char();
                if (next_char == '=') {
                    get_char();
                    return get_1way_punctuator(TOK_LSHIFT_ASSIGN);
                } else {
                    return get_1way_punctuator(TOK_LSHIFT_OP);
                }
                return get_1way_punctuator(TOK_LE_OP);
            } else if (next_char == ':') {
                get_char();
                return get_1way_punctuator(TOK_LBRACKET);
            } else if (next_char == '%') {
                get_char();
                return get_1way_punctuator(TOK_LBRACE);
            }
            return get_2way_punctuator(TOK_LT_OP, TOK_LE_OP);
        case '>':
            if (next_char == '>') {
                get_char();
                if (next_char == '=') {
                    get_char();
                    return get_1way_punctuator(TOK_RSHIFT_ASSIGN);
                } else {
                    return get_1way_punctuator(TOK_RSHIFT_OP);
                }
                return get_1way_punctuator(TOK_GE_OP);
            }
            return get_2way_punctuator(TOK_GT_OP, TOK_GE_OP);
        case '-':
            if (next_char == '>') {
                get_char();
                return get_1way_punctuator(TOK_REF_OP);
            }
            return get_3way_punctuator(TOK_SUB_OP, TOK_SUB_ASSIGN, TOK_DEC_OP);
        case '.':
            if (next_char == '.') {
                get_char();
                if (next_char == '.') {
                    get_char();
                    return get_1way_punctuator(TOK_ELLIPSIS);
                }
                return TOK_DOT; /* we want to stay on the second dot */
            }
            return get_1way_punctuator(TOK_DOT);
    }
    return TOK_ERROR;
}

void get_line_comment(struct token *token)
{
    get_char();
    get_char();
    buffer_reset(buffer);
    while (cur_char && !is_newline(cur_char)) {
        buffer_append(buffer, cur_char);
        get_char();
    }
    buffer_append(buffer, 0);

    token->type = TOK_COMMENT;
    token->value.str_val = buffer_data_copy(buffer);
}

void get_multiline_comment(struct token *token)
{
    get_char();
    get_char();
    buffer_reset(buffer);
    while ((cur_char != '*' || next_char != '/') && cur_char) {
        buffer_append(buffer, cur_char);
        get_char();
    }

    if (cur_char != '*') {
        lexer_error(token, "unexpected end of stream");
        return;
    }

    get_char();
    get_char();
    buffer_append(buffer, 0);

    token->type = TOK_COMMENT;
    token->value.str_val = buffer_data_copy(buffer);
}

int lexer_next_token(struct token *token)
{
    skip_ws();

    token->line = line;
    token->column = column;
    token->start = offset - 1;

    if (is_digit(cur_char)) {
        get_scalar(token);
    } else if (is_ident_start(cur_char)) {
        get_ident(token);
    } else if (cur_char == 0) {
        token->type = TOK_EOS;
    } else {
        if (cur_char == '/' && next_char == '/') {
            get_line_comment(token);
        } else if (cur_char == '/' && next_char == '*') {
            get_multiline_comment(token);
        } else {
            token->type = get_punctuator_type();
            if (token->type == TOK_ERROR) {
               lexer_error(token, "unexpected character");
            }
        }
    }

    token->end = offset - 2;
    return token->type < TOK_EOS;
}

extern const char *lexer_token_name(struct token *token)
{
    if (token->type > TOK_EOS) {
        return "UNKNOWN";
    }
    return token_names[token->type];
}

extern void lexer_token_value(struct token *token, buffer_t buffer)
{
    buffer_reset(buffer);
    switch (token->type) {
    case TOK_INT_CONST:
        buffer_ensure_capacity(buffer, 16);
        sprintf(buffer_data(buffer), "%d", token->value.int_val);
        break;
    case TOK_FLOAT_CONST:
        buffer_ensure_capacity(buffer, 16);
        sprintf(buffer_data(buffer), "%lf", token->value.float_val);
        break;
    case TOK_STRING_CONST:
    case TOK_IDENT:
    case TOK_COMMENT:
        buffer_ensure_capacity(buffer, strlen(token->value.str_val) + 1);
        sprintf(buffer_data(buffer), "%s", token->value.str_val);
        break;
    default:
        buffer_append(buffer, 0);
    }
}

extern void lexer_token_free_data(struct token *token)
{
    switch (token->type) {
    case TOK_STRING_CONST:
    case TOK_IDENT:
    case TOK_COMMENT:
        free(token->value.str_val);
    }
}