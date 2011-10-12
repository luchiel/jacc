#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "buffer.h"
#include "log.h"

void print_usage()
{
    printf("USAGE: jacc lex filename\n");
}

const char *basename(const char *path)
{
    const char *ptr = path;
    while (*path != 0) {
        if (*path == '/' || *path == '\\') {
            ptr = path + 1;
        }
        path++;
    }
    return ptr;
}

int lex(const char *filename)
{
    struct token token;
    buffer_t token_value;
    FILE *file;

    token_value = buffer_create(1024);

    file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Cannot open file");
        return EXIT_FAILURE;
    }
    lexer_init(file);
    log_set_unit(basename(filename));

    printf("Line\tText\tValue\tType\n");
    fflush(stdout);

    while (lexer_next_token(&token)) {
        lexer_token_value(&token, token_value);

        printf("%d:%d\t%s\t%s\t%s\n", token.line, token.column, token.text,
            buffer_data(token_value), lexer_token_name(&token));
        fflush(stdout);

        lexer_token_free_data(&token);
    }

    buffer_free(token_value);
    lexer_destroy();
    log_close();
    fclose(file);

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