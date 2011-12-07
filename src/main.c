#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "lexer.h"
#include "parser.h"
#include "buffer.h"
#include "log.h"
#include "symtable.h"

int show_indents[255];

void print_node(struct node *node, int level, int root);
void print_symtable(symtable_t symtable, int level);

char *simple_commands[] = {
    "lex",
    "parse_expr",
    "parse_stmt",
    "parse",
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

void print_node_indent(int level, int root)
{
    if (!root && level > 0) {
        print_indent(level - 1);
        printf(" +-");
    } else {
        print_indent(level);
    }
}

void print_branch(struct node *node, int level, int last)
{
    print_indent(level + 1);
    printf("\n");
    if (last) {
        show_indents[level + 1] = 0;
    }
    print_node(node, level + 1, 0);
}

void print_symbol(struct symbol *symbol, int level, int depth)
{
    switch (symbol->type) {
    case ST_SCALAR_TYPE:
        printf("%s", symbol->name);
        break;
    case ST_POINTER:
        printf("pointer to ");
        print_symbol(symbol->base_type, level, depth + 1);
        break;
    case ST_ARRAY:
        printf("array");
        if (symbol->expr != NULL) {
            printf(" [\n");
            show_indents[level + 1] = 0;
            print_node(symbol->expr, level + 1, 1);
            print_indent(level);
            printf("]");
        }
        printf(" of ");
        print_symbol(symbol->base_type, level, depth + 1);
        break;
    case ST_FUNCTION:
        if ((symbol->flags & SF_VARIADIC) == SF_VARIADIC) {
            printf("variadic ");
        }
        printf("function ");
        if (symtable_size(symbol->symtable) > 0) {
            printf("taking (\n");
            symtable_iter_t iter = symtable_first(symbol->symtable);
            for (; iter != NULL; iter = symtable_iter_next(iter)) {
                print_indent(level + 1);
                print_symbol(symtable_iter_value(iter), level + 1, 0);
                printf("\n");
            }
            print_indent(level);
            printf(") ");
        }

        printf("returning ");
        if (parser_is_void_symbol(symbol->base_type)) {
            printf("nothing");
        } else {
            printf("<");
            print_symbol(symbol->base_type, level, 0);
            printf(">");
        }
        if (symbol->expr != NULL) {
            printf(" defined as {\n");
            show_indents[level + 1] = 0;
            print_node(symbol->expr, level + 1, 1);
            print_indent(level);
            printf("}");
        }
        break;
    case ST_VARIABLE:
        printf("variable of type <");
        print_symbol(symbol->base_type, level, depth + 1);
        printf(">");
        if (symbol->expr != NULL) {
            printf(" = (\n");
            show_indents[level + 1] = 0;
            print_node(symbol->expr, level + 1, 1);
            print_indent(level);
            printf(")");
        }
        break;
    case ST_PARAMETER:
        printf("<");
        print_symbol(symbol->base_type, level, depth + 1);
        printf(">");
        if (symbol->name != NULL) {
            printf(" as %s", symbol->name);
        }
        break;
    case ST_STRUCT:
    case ST_UNION:
    case ST_ENUM:
        switch (symbol->type) {
            case ST_STRUCT: printf("struct"); break;
            case ST_UNION: printf("union"); break;
            case ST_ENUM: printf("enum"); break;
        }
        printf(" %s", symbol->name);
        if (symbol->symtable != NULL && depth == 0) {
            printf(" defined as {\n");
            print_symtable(symbol->symtable, level + 1);
            print_indent(level);
            printf("}");
        }
        break;
    case ST_TYPE_ALIAS:
        printf("alias for type <");
        print_symbol(symbol->base_type, level, depth + 1);
        printf(">");
        break;
    case ST_ENUM_CONST:
        printf("enum const of type <");
        print_symbol(symbol->base_type, level, depth + 1);
        printf("> = (\n");
        struct node_info *info = parser_node_info(symbol->expr);
        if (info->cat == NC_ATOM) {
            print_node(symbol->expr, level + 1, 1);
        }
        print_indent(level);
        printf(")");
        break;
    default:
        printf("unknown symbol");
    }
}

void print_symtable(symtable_t symtable, int level)
{
    symtable_iter_t iter = symtable_first(symtable);
    show_indents[level] = 0;
    for (; iter != NULL; iter = symtable_iter_next(iter)) {
        print_indent(level);
        switch (symtable_iter_key2(iter)) {
        case SC_TAG:
            printf("tag \"%s\" is ", symtable_iter_key(iter));
            break;
        default:
            printf("%s is ", symtable_iter_key(iter));
        }
        print_symbol(symtable_iter_value(iter), level, 0);
        printf("\n");
    }
}

void print_node(struct node *node, int level, int root)
{
    struct node_info *info = parser_node_info(node);
    int i;
    show_indents[level + 1] = 1;

    print_node_indent(level, root);
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
    case NT_VARIABLE:
        printf("var %s", ((struct var_node*)node)->symbol->name);
        break;
    case NT_CAST:
        printf("cast to <");
        print_symbol(((struct cast_node*)node)->type, level + 1, 0);
        printf(">");
        break;
    default:
        printf("%s", info->repr);
        break;
    }
    printf(")\n");

    int op_count = parser_node_subnodes_count(node);
    if (node->symtable != NULL && symtable_size(node->symtable) > 0) {
        print_indent(level);
        printf("[\n");
        print_symtable(node->symtable, level + 1);
        print_indent(level);
        printf("]");
        if (op_count == 0) {
            printf("\n");
        }
    }

    for (i = 0; i < op_count; i++) {
        print_branch(parser_get_subnode(node, i), level, i == op_count - 1);
    }
}

int cmd_parse_expr(FILE *file, const char *filename, const char *cmd)
{
    log_set_unit(basename(filename));

    lexer_init(file);
    parser_init();

    struct node* node = NULL;
    symtable_t symtable = NULL;

    if (strcmp(cmd, "parse_expr") == 0) {
        parser_flags_set(0);
        node = parser_parse_expr();
    } else if (strcmp(cmd, "parse_stmt") == 0) {
        parser_flags_set(0);
        node = parser_parse_statement();
    } else if (strcmp(cmd, "parse") == 0) {
        symtable = parser_parse();
    }

    if (node != NULL) {
        print_node(node, 0, 0);
        parser_free_node(node);
    } else if (symtable != NULL) {
        print_symtable(symtable, 0);
        symtable_destroy(symtable, 1);
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
