#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "lexer.h"
#include "parser.h"
#include "buffer.h"
#include "log.h"

int show_indents[255];

char *simple_commands[] = {
    "lex",
    "parse_expr",
    "parse_stmt",
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
            buffer_data(token_value), lexer_token_type_name(token.type));
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
    int i;
    for (i = 0; i < level; i++) {
        if (show_indents[i + 1]) {
            printf(" | ");
        } else {
            printf("   ");
        }
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

void print_branch(struct node *node, int level, int last)
{
    print_indent(level + 1);
    printf("\n");
    if (last) {
        show_indents[level + 1] = 0;
    }
    print_node(node, level + 1);
}

void print_node(struct node *node, int level)
{
    struct node_info *info = parser_node_info(node);
    int i;
    show_indents[level + 1] = 1;

    print_node_indent(level);
    switch (info->cat) {
    case NC_ATOM:
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
                printf("ident %s", ((struct string_node*)node)->value);
                break;
            case NT_NOP:
                printf("nop");
                break;
        }
        printf(")\n");
        break;
    default:
        printf("(%s)\n", info->repr);
        break;
    }

    for (i = 0; i < info->op_count; i++) {
        print_branch(node->ops[i], level, i == info->op_count - 1);
    }
}

int cmd_parse_expr(FILE *file, const char *filename, const char *cmd)
{
    log_set_unit(basename(filename));

    lexer_init(file);
    parser_init();

    struct node* node;

    if (strcmp(cmd, "parse_expr") == 0) {
        node = parser_parse_expr();
    } else if (strcmp(cmd, "parse_stmt") == 0) {
        node = parser_parse_statement();
    }

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
    for (i = 0; i < sizeof(simple_commands) / sizeof(char*); i++) {
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
            status = cmd_parse_expr(file, filename, argv[1]);
        }

        if (argc == 3) {
            fclose(file);
        }
        return status;
    }
    print_usage();
    return EXIT_SUCCESS;
}