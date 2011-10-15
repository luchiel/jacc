#ifndef JACC_PARSER_H
#define JACC_PARSER_H

#define DEFINE_NODE_TYPE(name, op_count) \
	struct name { \
 		struct node base; \
		struct node *ops[op_count]; \
 	};

enum node_type {
#define NODE(name, str) NT_##name,
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

struct node {
	enum node_type type;
	enum node_category cat;
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

DEFINE_NODE_TYPE(unary_node, 1)
DEFINE_NODE_TYPE(binary_node, 2)
DEFINE_NODE_TYPE(ternary_node, 3)

DEFINE_NODE_TYPE(return_node, 1)

extern void parser_init();
extern void parser_destroy();

extern struct node *parser_parse_expr();
extern struct node *parser_parse_statement();

extern void parser_free_node(struct node *node);
extern enum node_category parser_node_category(struct node *node);
extern int parser_subnodes_count(struct node *node);
extern const char *parser_node_name(struct node *node);

#endif