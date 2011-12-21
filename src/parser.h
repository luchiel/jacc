#ifndef JACC_PARSER_H
#define JACC_PARSER_H

#include "symtable.h"

#define PF_RESOLVE_NAMES 1
#define PF_ADD_INITIALIZERS 2

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
    struct symbol *type_sym;
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
};

#define NODE(name, str, cat, op_cnt) \
struct node_NT_##name { \
    enum node_type type; \
    symtable_t symtable; \
    struct symbol *type_sym; \
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

extern int is_ptr_type(struct symbol *symbol);
extern int is_compatible_types(struct symbol *s1, struct symbol *s2);
extern struct symbol *resolve_alias(struct symbol *symbol);

extern struct symbol sym_null, sym_void, sym_int, sym_double, sym_char, sym_char_ptr, sym_printf;
#endif
