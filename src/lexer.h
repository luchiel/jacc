#ifndef JACC_LEXER_H
#define JACC_LEXER_H

#include <stdio.h>
#include "buffer.h"

/* Keep in sync with lexer.c token_names */
enum token_type {
#define TOKEN(name) TOK_##name,
#define KEYWORD(name, str) TOKEN(name)
#include "tokens.def"
#undef KEYWORD
#undef TOKEN
};

struct token {
    enum token_type type;

    int line;
    int column;
    char *text;

    union {
        int int_val;
        double float_val;
        char *str_val;
    } value;
};

extern void lexer_init(FILE *stream);
extern int lexer_next_token(struct token *token);
extern void lexer_destroy();

extern const char *lexer_token_name(struct token *token);
extern void lexer_token_value(struct token *token, buffer_t buffer);
extern void lexer_token_free_data(struct token *token);

#endif