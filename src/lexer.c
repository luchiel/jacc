#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "buffer.h"

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

int is_digit()
{
    return cur_char >= '0' && cur_char <= '9';
}

int is_ident_start()
{
    return cur_char >= 'a' && cur_char <= 'z'
        || cur_char >= 'A' && cur_char <= 'Z'
        || cur_char == '_';
}

int is_ident()
{
    return is_ident_start() || is_digit();
}

int is_whitespace()
{
    return cur_char == '\n' || cur_char == '\r'
        || cur_char == '\t' || cur_char == '\v'
        || cur_char == ' ';
}

void skip_ws()
{
    while (is_whitespace()) {
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
        return c - 'a';
    }
    return c - '0';
}

void get_scalar(struct token *token)
{
    int result = 0;

    do {
        result = result * 10 + digit_value(cur_char);
        get_char();
    } while (is_digit());

    token->type = TOK_INT_CONST;
    token->value.int_val = result;
}

enum token_type get_ident_type(const char *ident)
{
    // TODO use hash table instead?
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
    } while (is_ident());

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

    if (is_digit()) {
        get_scalar(token);
    } else if (is_ident_start()) {
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

extern void lexer_token_value(struct token *token, char *buffer)
{
    /* TODO buffer overflow? */
    switch (token->type) {
    case TOK_INT_CONST:
        sprintf(buffer, "%d", token->value.int_val);
        break;
    case TOK_FLOAT_CONST:
        sprintf(buffer, "%lf", token->value.float_val);
        break;
    case TOK_STRING_CONST:
    case TOK_IDENT:
        sprintf(buffer, "%s", token->value.str_val);
        break;
    default:
        buffer[0] = 0;
    }
}

extern void lexer_token_free_data(struct token *token)
{
    if (token->type == TOK_STRING_CONST || token->type == TOK_IDENT) {
        free(token->value.str_val);
    }
}