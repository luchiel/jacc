#ifndef JACC_LEXER_H
#define JACC_LEXER_H

#include <stdio.h>
#include "buffer.h"

/* Keep in sync with lexer.c token_names */
enum token_type {
    TOK_STRING_CONST = 0,
    TOK_INT_CONST,
    TOK_FLOAT_CONST,
    TOK_IDENT,
    TOK_COMMENT,

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

    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LPAREN,
    TOK_RPAREN,

    TOK_DOT,
    TOK_COMMA,
    TOK_AMP,
    TOK_TILDE,
    TOK_QUESTION,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_ELLIPSIS,
    TOK_STAR,

    TOK_REF_OP,

    TOK_INC_OP,
    TOK_DEC_OP,

    TOK_ADD_OP,
    TOK_SUB_OP,
    TOK_DIV_OP,
    TOK_MOD_OP,

    TOK_NEG_OP,

    TOK_LSHIFT_OP,
    TOK_RSHIFT_OP,

    TOK_LT_OP,
    TOK_LE_OP,
    TOK_GT_OP,
    TOK_GE_OP,
    TOK_EQUAL_OP,
    TOK_NOT_EQUAL_OP,

    TOK_BIT_XOR_OP,
    TOK_BIT_OR_OP,

    TOK_AND_OP,
    TOK_OR_OP,

    TOK_ASSIGN,
    TOK_MUL_ASSIGN,
    TOK_DIV_ASSIGN,
    TOK_MOD_ASSIGN,
    TOK_ADD_ASSIGN,
    TOK_SUB_ASSIGN,
    TOK_LSHIFT_ASSIGN,
    TOK_RSHIFT_ASSIGN,
    TOK_BIT_OR_ASSIGN,
    TOK_BIT_AND_ASSIGN,
    TOK_BIT_XOR_ASSIGN,

    TOK_EOS,
    TOK_ERROR,
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