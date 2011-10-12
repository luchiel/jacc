#ifndef JACC_PARSER_H
#define JACC_PARSER_H

enum node_type {
	/* unary */
	NT_REFERENCE,
	NT_DEREFERENCE,
	NT_IDENTITY,
	NT_NEGATION,
	NT_COMPLEMENT,
	NT_LOGICAL_NEGATION,

	NT_PREFIX_INC,
	NT_PREFIX_DEC,
	NT_POSTFIX_INC,
	NT_POSTFIX_DEC,

	/* binary */
	NT_COMMA,

	NT_OR,
	NT_AND,

	NT_BIT_OR,
	NT_BIT_XOR,
	NT_BIT_AND,

	NT_EQ,
	NT_NE,

	NT_LT,
	NT_LE,
	NT_GT,
	NT_GE,

	NT_LSHIFT,
	NT_RSHIFT,

	NT_ADD,
	NT_SUB,

	NT_MUL,
	NT_DIV,
	NT_MOD,

	NT_SUBSCRIPT,
	NT_MEMBER,
	NT_MEMBER_BY_PTR,
	NT_CALL,

	/* other */
	NT_TERNARY,

	NT_INT,
	NT_DOUBLE,
	NT_STRING,
	NT_IDENT,

	NT_UNKNOWN,
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