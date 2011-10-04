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

int is_whitespace(char chr)
{
    return chr == '\n' || chr == '\r'
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
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return c - '0';
}

void lexer_error(struct token *token, const char *message)
{
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
        token->type = TOK_UNKNOWN;
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
        buffer_ensure_capacity(buffer, strlen(token->value.str_val) + 1);
        sprintf(buffer_data(buffer), "%s", token->value.str_val);
        break;
    default:
        buffer_append(buffer, 0);
    }
}

extern void lexer_token_free_data(struct token *token)
{
    if (token->type == TOK_STRING_CONST || token->type == TOK_IDENT) {
        free(token->value.str_val);
    }
}