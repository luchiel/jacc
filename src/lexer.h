#ifndef JACC_LEXER_H
#define JACC_LEXER_H

#include <stdio.h>

/* Keep in sync with lexer.c token_names */
enum token_type {
    TOK_STRING_CONST = 0,
    TOK_INT_CONST,
    TOK_FLOAT_CONST,
    TOK_IDENT,

    TOK_BREAK,
    TOK_CASE,
    TOK_CHAR,
    TOK_CONST,
    TOK_CONTINUE,
    TOK_DEFAULT,
    TOK_DO,
    TOK_DOUBLE,
    TOK_ELSE,
    TOK_ENUM,
    TOK_EXTERN,
    TOK_FLOAT,
    TOK_FOR,
    TOK_IF,
    TOK_INT,
    TOK_REGISTER,
    TOK_RETURN,
    TOK_SIZEOF,
    TOK_STATIC,
    TOK_STRUCT,
    TOK_SWITCH,
    TOK_TYPEDEF,
    TOK_UNION,
    TOK_VOID,
    TOK_WHILE,

    TOK_EOS,
    TOK_UNKNOWN,
};

struct token {
    enum token_type type;

    int line;
    int column;
    int start;
    int end;

    union {
        int int_val;
        double float_val;
        char *str_val;
    } value;
};

extern void lexer_init(char *content, int size, FILE *error_stream);
extern int lexer_next_token(struct token *token);
extern void lexer_destroy();

extern const char *lexer_token_name(struct token *token);
extern void lexer_token_value(struct token *token, char *buffer);
extern void lexer_token_free_data(struct token *token);

#endif