#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "buffer.h"

void print_usage()
{
    printf("USAGE: jacc lex filename\n");
}

int read_file(const char *filename, char **content, int *size)
{
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        return 1;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    *content = (char*)malloc(*size);
    if (*content == NULL) {
        fclose(f);
        return 1;
    }

    if (*size != fread(*content, 1, *size, f)) {
        fclose(f);
        free(content);
    }

    fclose(f);
    return 0;
}

int lex(const char *filename)
{
    struct token token;
    char token_value[32];
    buffer_t token_text;
    char *content;

    int file_size;

    if (read_file(filename, &content, &file_size) != 0) {
        fprintf(stderr, "Can't open file\n");
        return EXIT_FAILURE;
    }

    token_text = buffer_create(1024);
    if (token_text == NULL) {
        fprintf(stderr, "Can't allocate token buffer\n");
        return EXIT_FAILURE;
    }

    printf("Line\tText\tValue\tType\n");
    lexer_init(content, file_size, stderr);
    while (lexer_next_token(&token)) {
        lexer_token_value(&token, token_value);

        buffer_reset(token_text);
        buffer_append_string(token_text, content + token.start, token.end - token.start + 1);
        buffer_append(token_text, 0);

        printf("%d:%d\t%s\t%s\t%s\n", token.line, token.column, buffer_data(token_text),
            token_value, lexer_token_name(&token));

        lexer_token_free_data(&token);
    }

    if (token.type == TOK_UNKNOWN) {
        fprintf(stderr, "Unexpected character on line %d:%d\n", token.line, token.column);
    }

    lexer_destroy();
    free(content);
    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        print_usage();
        return EXIT_SUCCESS;
    }

    if (argc == 3 && strcmp(argv[1], "lex") == 0) {
        return lex(argv[2]);
    } else {
        print_usage();
        return EXIT_SUCCESS;
    }
    return EXIT_SUCCESS;
}