#ifndef JACC_PARSER_H
#define JACC_PARSER_H

#include "symtable.h"

#define PF_RESOLVE_NAMES 1

#define DEFINE_NODE_TYPE(name, op_count) \
    struct name { \
        struct node base; \
        struct node *ops[op_count]; \
    };

enum node_type {
#define NODE(name, str, cat, op_cnt) NT_##name,
#include "nodes.def"
#undef NODE
};

enum node_category {
    NC_UNARY,
    NC_BINARY,
    NC_TERNARY,
    NC_ATOM,
    NC_STATEMENT,

    NC_UNKNOWN,
};

struct node_info {
    const char *repr;
    enum node_category cat;
    int op_count;
};

struct node {
    enum node_type type;
    symtable_t symtable;
    struct node *ops[0];
};

struct int_node {
    struct node base;
    int value;
};

struct double_node {
    struct node base;
    double value;
};

struct string_node {
    struct node base;
    char *value;
};

struct list_node {
    struct node base;
    struct node **items;
    int size;
    int capacity;
};

struct var_node {
    struct node base;
    struct symbol *symbol;
};

struct cast_node {
    struct node base;
    struct node *ops[1];
    struct symbol *type;
};

#define NODE(name, str, cat, op_cnt) \
struct node_NT_##name { \
    enum node_type type; \
    symtable_t symtable; \
    struct node *ops[op_cnt]; \
};
#include "nodes.def"
#undef NODE

DEFINE_NODE_TYPE(unary_node, 1)
DEFINE_NODE_TYPE(binary_node, 2)

extern void parser_init();
extern void parser_destroy();

extern struct node *parser_parse_expr();
extern struct node *parser_parse_statement();
extern symtable_t parser_parse();

extern void parser_free_node(struct node *node);
extern struct node_info *parser_node_info(struct node *node);
extern int parser_node_subnodes_count(struct node *node);
extern struct node *parser_get_subnode(struct node *node, int index);

extern int parser_is_void_symbol(struct symbol *symbol);

extern void parser_flags_set(int new_flags);
extern int parser_flags_get();

#endif
