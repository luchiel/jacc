#ifndef JACC_PARSER_H
#define JACC_PARSER_H

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

#define NODE(name, str, cat, op_cnt) \
struct node_NT_##name { \
	enum node_type type; \
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

extern void parser_free_node(struct node *node);
extern struct node_info *parser_node_info(struct node *node);

#endif