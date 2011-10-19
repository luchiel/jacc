#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "lexer.h"
#include "log.h"

FILE *stream;
char stream_buffer[4096];
int stream_buffer_size;
int stream_buffer_pos;

buffer_t buffer;
buffer_t text_buffer;

char cur_char;
char next_char;

int line, column;

const char* token_names[] = {
#define TOKEN(name) #name,
#define KEYWORD(name, str) TOKEN(name)
#include "tokens.def"
#undef KEYWORD
#undef TOKEN
};

char *keywords[] = {
#define TOKEN(name)
#define KEYWORD(name, str) str,
#include "tokens.def"
#undef KEYWORD
#undef TOKEN
};

#define KEYWORDS_COUNT (TOK_WHILE - TOK_BREAK + 1)
#define KEYWORDS_START TOK_BREAK

static void read_stream()
{
    stream_buffer_size = fread(stream_buffer, 1, sizeof(stream_buffer), stream);
    stream_buffer_pos = 0;
}

static void get_char()
{
    cur_char = next_char;
    buffer_append(text_buffer, cur_char);
    column++;
    if (stream_buffer_pos < stream_buffer_size) {
        next_char = stream_buffer[stream_buffer_pos];
        stream_buffer_pos++;
        if (stream_buffer_pos == stream_buffer_size) {
            read_stream();
        }
    } else {
        next_char = 0;
    }
}

extern void lexer_init(FILE *stream_)
{
    stream = stream_;
    line = 1;
    column = -1;
    buffer = buffer_create(1024);
    text_buffer = buffer_create(1024);

    read_stream();
    get_char();
    get_char();
}

extern void lexer_destroy()
{
    buffer_free(buffer);
    buffer_free(text_buffer);
}

static int is_digit(char chr)
{
    return chr >= '0' && chr <= '9';
}

static int is_octdigit(char chr)
{
    return chr >= '0' && chr <= '7';
}

static int is_hexdigit(char chr)
{
    return is_digit(chr)
        || (chr >= 'a' && chr <= 'f')
        || (chr >= 'A' && chr <= 'F');
}

static int is_digit_ex(char chr, int base)
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

static int is_alpha(char chr)
{
    return (chr >= 'a' && chr <= 'z')
        || (chr >= 'A' && chr <= 'Z');
}

static int is_ident_start(char chr)
{
    return is_alpha(chr) || chr == '_';
}

static int is_ident(char chr)
{
    return is_ident_start(chr) || is_digit(chr);
}

static int is_newline(char chr)
{
    return chr == '\n' || chr == '\r';
}

static int is_whitespace(char chr)
{
    return is_newline(chr)
        || chr == '\t' || chr == '\v'
        || chr == ' ';
}

static void skip_ws()
{
    while (is_whitespace(cur_char)) {
        if (cur_char == '\n') {
            column = 0;
            line++;
        }
        get_char();
    }
}

static int digit_value(char c)
{
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xA;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 0xA;
    }
    return c - '0';
}

static void lexer_error(struct token *token, const char *message)
{
    token->type = TOK_ERROR;
    log_set_pos(token->line, token->column);
    log_error(message);
}

static void read_dec_number(struct token *token, const char *error_msg)
{
    if (!is_digit(cur_char)) {
        lexer_error(token, error_msg);
        return;
    }

    while (is_digit(cur_char)) {
        buffer_append(buffer, cur_char);
        get_char();
    }
}

static void get_float_part(struct token *token)
{
    /* we have integer part in the buffer */
    if (cur_char == '.') {
        buffer_append(buffer, '.');
        get_char();
        if (is_digit(cur_char) || buffer_size(buffer) == 1) {
            read_dec_number(token, "invalid float constant");
            if (token->type == TOK_ERROR) {
                return;
            }
        }
    }

    if (cur_char == 'e' || cur_char == 'E') {
        buffer_append(buffer, 'e');
        get_char();
        if (cur_char == '+' || cur_char == '-') {
            buffer_append(buffer, cur_char);
            get_char();
        }
        read_dec_number(token, "invalid float constant");
        if (token->type == TOK_ERROR) {
            return;
        }
    }
    buffer_append(buffer, 0);
    token->type = TOK_FLOAT_CONST;
    sscanf(buffer_data(buffer), "%lf", &token->value.float_val);
}

static void get_scalar(struct token *token)
{
    int result = 0, base = 10;

    buffer_reset(buffer);
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
        buffer_append(buffer, cur_char);
        result = result * base + digit_value(cur_char);
        get_char();
    }

    if (base == 8 && is_digit(cur_char)) {
        lexer_error(token, "invalid digit in octal constant");
        return;
    }

    if (base == 10 && (cur_char == '.' || cur_char == 'e' || cur_char == 'E')) {
        get_float_part(token);
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

static enum token_type get_ident_type(const char *ident)
{
    /* TODO use hash table instead? */
    int i;
    for (i = 0; i < KEYWORDS_COUNT; i++) {
        if (strcmp(ident, keywords[i]) == 0) {
            return i + KEYWORDS_START;
        }
    }
    return TOK_IDENT;
}

static void get_ident(struct token *token)
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

static enum token_type get_1way_punctuator(enum token_type type)
{
    get_char();
    return type;
}

static enum token_type get_2way_punctuator(enum token_type t1,
        enum token_type t2)
{
    if (next_char == '=') {
        get_char();
        return get_1way_punctuator(t2);
    }
    return get_1way_punctuator(t1);
}

static enum token_type get_3way_punctuator(enum token_type t1,
        enum token_type t2, enum token_type t3)
{
    if (next_char == cur_char) {
        get_char();
        return get_1way_punctuator(t3);
    }
    return get_2way_punctuator(t1, t2);
}

static enum token_type get_punctuator_type()
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
                return get_2way_punctuator(TOK_LSHIFT_OP, TOK_LSHIFT_ASSIGN);
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
                return get_2way_punctuator(TOK_RSHIFT_OP, TOK_RSHIFT_ASSIGN);
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

static void get_line_comment(struct token *token)
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

static void get_multiline_comment(struct token *token)
{
    get_char();
    get_char();
    buffer_reset(buffer);
    while ((cur_char != '*' || next_char != '/') && cur_char) {
        if (cur_char == '\n') {
            line++;
        }
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

static void get_single_string(struct token *token, char quote)
{
    int i, value;
    get_char();
    token->type = TOK_STRING_CONST;
    while (cur_char != quote && !is_newline(cur_char)) {
        if (cur_char == '\\') {
            get_char();
            switch (cur_char) {
            case '\'': buffer_append(buffer, '\''); break;
            case '\"': buffer_append(buffer, '\"'); break;
            case '\\': buffer_append(buffer, '\\'); break;
            case '?':  buffer_append(buffer, '\?'); break;
            case 'a':  buffer_append(buffer, '\a'); break;
            case 'b':  buffer_append(buffer, '\b'); break;
            case 'f':  buffer_append(buffer, '\f'); break;
            case 'n':  buffer_append(buffer, '\n'); break;
            case 'r':  buffer_append(buffer, '\r'); break;
            case 't':  buffer_append(buffer, '\t'); break;
            case 'v':  buffer_append(buffer, '\v'); break;
            case 'x':
                get_char();
                if (is_hexdigit(cur_char)) {
                    if (is_hexdigit(next_char)) {
                        buffer_append(buffer, digit_value(cur_char) * 16 + digit_value(next_char));
                        get_char();
                        get_char();
                    } else {
                        buffer_append(buffer, digit_value(cur_char));
                        get_char();
                    }
                } else {
                    lexer_error(token, "\\x used with no following hex digits");
                    return;
                }
                continue;
            default:
                if (is_octdigit(cur_char)) {
                    value = 0;
                    for (i = 0; i < 3 && is_octdigit(cur_char); i++) {
                        value = value * 8 + digit_value(cur_char);
                        get_char();
                    }
                    if (value > 255) {
                        lexer_error(token, "octal escape sequence out of range");
                        return;
                    }
                    buffer_append(buffer, value);
                } else {
                    lexer_error(token, "unknown escape sequence");
                    return;
                }
                continue;
            }
        } else if (!cur_char) {
            lexer_error(token, "unexpected end of stream");
            return;
        } else {
            buffer_append(buffer, cur_char);
        }
        get_char();
    }

    if (is_newline(cur_char)) {
        lexer_error(token, "missing terminating character");
        return;
    }
    get_char();
}

static void get_string(struct token *token, char quote)
{
    buffer_reset(buffer);
    while (cur_char == quote) {
        get_single_string(token, quote);
        if (token->type == TOK_ERROR) {
            return;
        }
        skip_ws();
    }
    buffer_append(buffer, 0);

    if (quote == '"') {
        token->type = TOK_STRING_CONST;
        token->value.str_val = buffer_data_copy(buffer);
    } else {
        if (buffer_size(buffer) == 1) {
            lexer_error(token, "empty character constant");
            return;
        } else if (buffer_size(buffer) > 2) {
            lexer_error(token, "multi-character character constant");
            return;
        }
        token->type = TOK_INT_CONST;
        token->value.int_val = buffer_data(buffer)[0];
    }
}

static char *get_token_text()
{
    char *text = buffer_data_copy(text_buffer);
    text[buffer_size(text_buffer) - 1] = 0;
    return text;
}

extern int lexer_next_token(struct token *token)
{
    skip_ws();

    buffer_reset(text_buffer);
    buffer_append(text_buffer, cur_char);

    token->line = line;
    token->column = column;

    if (is_digit(cur_char)) {
        get_scalar(token);
    } else if (is_ident_start(cur_char)) {
        get_ident(token);
    } else if (cur_char == '"') {
        get_string(token, '"');
    } else if (cur_char == '\'') {
        get_string(token, '\'');
    } else if (cur_char == '.' && is_digit(next_char)) {
        buffer_reset(buffer);
        get_float_part(token);
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
    if (token->type == TOK_ERROR || token->type == TOK_EOS) {
        token->text = NULL;
        return 0;
    }
    token->text = get_token_text();
    return 1;
}

extern const char *lexer_token_type_name(enum token_type type)
{
    if (type > TOK_EOS) {
        return "UNKNOWN";
    }
    return token_names[type];
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
        jacc_free(token->value.str_val);
    }
    jacc_free(token->text);
}