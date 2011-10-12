#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "buffer.h"
#include "log.h"

char *simple_commands[] = {
    "lex",
};

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

int cmd_lex(FILE *file, const char *filename)
{
    struct token token;
    buffer_t token_value;

    token_value = buffer_create(1024);

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

    return EXIT_SUCCESS;
}

int is_cmd(const char *str)
{
    int i;
    for (i = 0; i < 2; i++) {
        if (strcmp(simple_commands[i], str) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    FILE *file;
    char *filename;
    int status;

    if (argc < 2) {
        print_usage();
        return EXIT_SUCCESS;
    }

    if (is_cmd(argv[1])) {
        if (argc == 3) {
            filename = argv[2];
            file = fopen(filename, "r");
            if (file == NULL) {
                fprintf(stderr, "Cannot open file");
                return EXIT_FAILURE;
            }
        } else {
            filename = ":stdin:";
            file = stdin;
        }

        if (argv[1][0] == 'l') {
            status = cmd_lex(file, filename);
        }

        if (argc == 3) {
            fclose(file);
        }
        return status;
    }
    print_usage();
    return EXIT_SUCCESS;
}