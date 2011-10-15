#ifndef JACC_PARSER_H
#define JACC_PARSER_H

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

	NC_UNKNOWN,
};

struct node {
	enum node_type type;
	enum node_category cat;
	struct node *ops[0];
};

struct unary_node {
	struct node base;
	struct node *ops[1];
};

struct binary_node {
	struct node base;
	struct node *ops[2];
};

struct ternary_node {
	struct node base;
	struct node *ops[3];
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

extern void parser_init();
extern void parser_destroy();

extern struct node *parser_parse_expr();
extern void parser_free_node(struct node *node);
extern enum node_category parser_node_category(struct node *node);
extern const char *parser_node_name(struct node *node);

#endif