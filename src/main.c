#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "buffer.h"
#include "log.h"

char *simple_commands[] = {
    "lex",
    "parse_expr",
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

void print_indent(int level)
{
    while (level--) {
        printf(" | ");
    }
}

void print_node_indent(int level)
{
    if (level > 0) {
        print_indent(level - 1);
        printf(" +-");
    }
}

void print_node(struct node *node, int level);

void print_branch(struct node *node, int level)
{
    print_indent(level + 1);
    printf("\n");
    print_node(node, level + 1);
}

void print_node(struct node *node, int level)
{
    const char *node_name = parser_node_name(node);
    int node_count = 0, i;
    print_node_indent(level);
    switch (node->cat) {
    case NC_TERNARY:
        printf("(%s)\n", node_name);
        node_count = 3;
        break;
    case NC_BINARY:
        printf("(%s)\n", node_name);
        node_count = 2;
        break;
    case NC_UNARY:
        printf("(%s)\n", node_name);
        node_count = 1;
        break;
    case NC_ATOM:
        node_count = 0;
        printf("(");
        switch (node->type) {
            case NT_INT:
                printf("%d", ((struct int_node*)node)->value);
                break;
            case NT_DOUBLE:
                printf("%f", ((struct double_node*)node)->value);
                break;
            case NT_STRING:
                printf("\"%s\"", ((struct string_node*)node)->value);
                break;
            case NT_IDENT:
                printf("%s", ((struct string_node*)node)->value);
                break;
        }
        printf(")\n");
        break;
    }

    for (i = 0; i < node_count; i++) {
        print_branch(node->ops[i], level);
    }
}

int cmd_parse_expr(FILE *file, const char *filename)
{
    log_set_unit(basename(filename));

    lexer_init(file);
    parser_init();

    struct node* node = parser_parse_expr();

    if (node != NULL) {
        print_node(node, 0);
        parser_free_node(node);
    }
    parser_destroy();
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
        } else {
            status = cmd_parse_expr(file, filename);
        }

        if (argc == 3) {
            fclose(file);
        }
        return status;
    }
    print_usage();
    return EXIT_SUCCESS;
}