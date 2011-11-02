#include <stdlib.h>
#include "memory.h"
#include "lexer.h"
#include "parser.h"
#include "log.h"
#include "pull.h"

#define ALLOC_NODE(enum_type, var_name) \
	ALLOC_NODE_EX(enum_type, var_name, node_##enum_type)

#define ALLOC_NODE_EX(enum_type, var_name, struct_name) \
	struct struct_name *var_name = jacc_malloc(sizeof(*var_name)); \
	pull_add(parser_pull, var_name); \
	((struct node*)(var_name))->type = (enum_type);

#define EXPECT(token_type) { if (token.type != token_type) { unexpected_token(lexer_token_type_name(token_type)); return NULL; } }
#define CONSUME(token_type) { EXPECT(token_type); next_token(); }

#define PARSE_CHECK(expr) if ((expr) == NULL) return NULL;
#define PARSE(target, rule, ...) { (target) = parse_##rule(__VA_ARGS__); PARSE_CHECK(target); }

pull_t parser_pull;

struct token token;
struct token token_next;

struct node_info nodes_info[] = {
#define NODE(name, repr, cat, op_cnt) {repr, cat, op_cnt},
#include "nodes.def"
#undef NODE
};

static struct node *parse_expr(int level);
static struct node *parse_cast_expr();

static int next_token()
{
	lexer_token_free_data(&token);
	token = token_next;
	lexer_next_token(&token_next);
	while (token_next.type == TOK_COMMENT) {
		lexer_token_free_data(&token_next);
		lexer_next_token(&token_next);
	}
	return token.type != TOK_ERROR;
}

static int accept(enum token_type type)
{
	if (token.type == type) {
		next_token();
		return 1;
	}
	return 0;
}

static void unexpected_token(const char *string)
{
	char buf[255];
	if (string == NULL) {
		sprintf(buf, "unexpected token %s", lexer_token_type_name(token.type));
	} else {
		sprintf(buf, "unexpected token %s, expected %s", lexer_token_type_name(token.type), string);
	}
	log_set_pos(token.line, token.column);
	log_error(buf);
	string = string;
}

static void list_node_ensure_capacity(struct list_node* list, int new_capacity)
{
	if (list->capacity < new_capacity) {
		list->capacity *= 2;
		if (list->capacity < new_capacity) {
			list->capacity = new_capacity;
		}
		list->items = jacc_realloc(list->items, list->capacity * sizeof(*list->items));
	}
}

struct list_node* alloc_list_node()
{
	ALLOC_NODE_EX(NT_LIST, node, list_node)
	node->size = 0;
	node->capacity = 0;
	node->items = NULL;
	list_node_ensure_capacity(node, 4);
	return node;
}

static struct node *parse_ident()
{
	EXPECT(TOK_IDENT)
	ALLOC_NODE_EX(NT_IDENT, node, string_node)
	node->value = token.value.str_val;
	token.value.str_val = NULL;
	next_token();
	pull_add(parser_pull, node->value);
	return (struct node*)node;
}

static struct node *parse_primary_expr()
{
	switch (token.type) {
	case TOK_LPAREN:
	{
		CONSUME(TOK_LPAREN)
		struct node *node;
		PARSE(node, expr, 0)
		CONSUME(TOK_RPAREN)
		return node;
	}
	case TOK_STRING_CONST:
	{
		ALLOC_NODE_EX(NT_STRING, node, string_node)
		node->value = token.value.str_val;
		token.value.str_val = NULL;
		next_token();
		pull_add(parser_pull, node->value);
		return (struct node*)node;
	}
	case TOK_IDENT:
		return parse_ident();
	case TOK_INT_CONST:
	{
		ALLOC_NODE_EX(NT_INT, node, int_node)
		node->value = token.value.int_val;
		next_token();
		return (struct node*)node;
	}
	case TOK_FLOAT_CONST:
	{
		ALLOC_NODE_EX(NT_DOUBLE, node, double_node)
		node->value = token.value.float_val;
		next_token();
		return (struct node*)node;
	}
	default:
		unexpected_token(NULL);
		return NULL;
	}
}

static enum node_type get_postfix_node_type()
{
	switch (token.type) {
	case TOK_DOT: return NT_MEMBER;
	case TOK_REF_OP: return NT_MEMBER_BY_PTR;
	case TOK_LPAREN: return NT_CALL;
	case TOK_LBRACKET: return NT_SUBSCRIPT;
	case TOK_INC_OP: return NT_POSTFIX_INC;
	case TOK_DEC_OP: return NT_POSTFIX_DEC;
	}
	return NT_UNKNOWN;
}

static struct node *parse_nop()
{
	ALLOC_NODE(NT_NOP, node);
	return (struct node *)node;
}

static struct node *parse_postfix_expr()
{
	struct node *node;
	PARSE(node, primary_expr);
	while (1) {
		switch (token.type) {
		case TOK_INC_OP:
		case TOK_DEC_OP:
		{
			ALLOC_NODE_EX(get_postfix_node_type(), unode, unary_node)
			unode->ops[0] = node;
			node = (struct node*)unode;
			next_token();
			break;
		}
		case TOK_DOT:
		case TOK_REF_OP:
		case TOK_LPAREN:
		case TOK_LBRACKET:
		{
			ALLOC_NODE_EX(get_postfix_node_type(), unode, binary_node)
			unode->ops[0] = node;
			next_token();

			switch (unode->base.type) {
			case NT_SUBSCRIPT:
				PARSE(unode->ops[1], expr, 0)
				break;
			case NT_CALL: /* TODO should be parse_arg_list */
				if (token.type != TOK_RPAREN) {
					PARSE(unode->ops[1], expr, 0)
				} else {
					PARSE(unode->ops[1], nop)
				}
				break;
			case NT_MEMBER:
			case NT_MEMBER_BY_PTR:
				PARSE(unode->ops[1], ident)
				break;
			}

			if (unode->base.type == NT_CALL) {
				CONSUME(TOK_RPAREN)
			} else if (unode->base.type == NT_SUBSCRIPT) {
				CONSUME(TOK_RBRACKET)
			}

			node = (struct node*)unode;
			break;
		}
		default:
			return node;
		}
	}
	return NULL;
}

static enum node_type get_unary_node_type()
{
	switch (token.type) {
	case TOK_AMP: return NT_REFERENCE;
	case TOK_STAR: return NT_DEREFERENCE;
	case TOK_ADD_OP: return NT_IDENTITY;
	case TOK_SUB_OP: return NT_NEGATION;
	case TOK_TILDE: return NT_COMPLEMENT;
	case TOK_NEG_OP: return NT_LOGICAL_NEGATION;
	case TOK_INC_OP: return NT_PREFIX_INC;
	case TOK_DEC_OP: return NT_PREFIX_DEC;
	}
	return NT_UNKNOWN;
}

static struct node *parse_unary_expr()
{
	switch (token.type) {
	case TOK_AMP:
	case TOK_STAR:
	case TOK_ADD_OP:
	case TOK_SUB_OP:
	case TOK_TILDE:
	case TOK_NEG_OP:
	case TOK_INC_OP:
	case TOK_DEC_OP:
	{
		ALLOC_NODE_EX(get_unary_node_type(), node, unary_node);
		next_token();
		switch (node->base.type) {
		case NT_PREFIX_INC:
		case NT_PREFIX_DEC:
			PARSE(node->ops[0], unary_expr)
			break;
		default:
			PARSE(node->ops[0], cast_expr)
		}
		return (struct node*)node;
	}
	}
	return parse_postfix_expr();
}

static struct node *parse_cast_expr()
{
	return parse_unary_expr();
}

static struct node *parse_cond_expr()
{
	struct node *node;
	PARSE(node, expr, 1)
	while (1) {
		if (!accept(TOK_QUESTION)) {
			return node;
		}
		ALLOC_NODE(NT_TERNARY, new_node)
		new_node->ops[0] = node;
		PARSE(new_node->ops[1], expr, 0)
		CONSUME(TOK_COLON)
		PARSE(new_node->ops[2], expr, 1)
		node = (struct node*)new_node;
	}
	return node;
}

static enum node_type get_node_type()
{
	switch (token.type) {
	case TOK_COMMA: return NT_COMMA;

	case TOK_OR_OP: return NT_OR;
	case TOK_AND_OP: return NT_AND;

	case TOK_BIT_OR_OP: return NT_BIT_OR;
	case TOK_BIT_XOR_OP: return NT_BIT_XOR;
	case TOK_AMP: return NT_BIT_AND;

	case TOK_EQUAL_OP: return NT_EQ;
	case TOK_NOT_EQUAL_OP: return NT_NE;

	case TOK_LT_OP: return NT_LT;
	case TOK_LE_OP: return NT_LE;
	case TOK_GT_OP: return NT_GT;
	case TOK_GE_OP: return NT_GE;

	case TOK_LSHIFT_OP: return NT_LSHIFT;
	case TOK_RSHIFT_OP: return NT_RSHIFT;

	case TOK_ADD_OP: return NT_ADD;
	case TOK_SUB_OP: return NT_SUB;

	case TOK_STAR: return NT_MUL;
	case TOK_DIV_OP: return NT_DIV;
	case TOK_MOD_OP: return NT_MOD;

	case TOK_ASSIGN: return NT_ASSIGN;
	case TOK_ADD_ASSIGN: return NT_ADD_ASSIGN;
	case TOK_SUB_ASSIGN: return NT_SUB_ASSIGN;
	case TOK_MUL_ASSIGN: return NT_MUL_ASSIGN;
	case TOK_DIV_ASSIGN: return NT_DIV_ASSIGN;
	case TOK_MOD_ASSIGN: return NT_MOD_ASSIGN;
	case TOK_LSHIFT_ASSIGN: return NT_LSHIFT_ASSIGN;
	case TOK_RSHIFT_ASSIGN: return NT_RSHIFT_ASSIGN;
	case TOK_BIT_OR_ASSIGN: return NT_OR_ASSIGN;
	case TOK_BIT_AND_ASSIGN: return NT_AND_ASSIGN;
	case TOK_BIT_XOR_ASSIGN: return NT_XOR_ASSIGN;
	}
	return NT_UNKNOWN;
}

#define CHECK(token_type) { if (token.type == token_type) return 1; }
static int accept_expr_token(int level)
{
	switch (level) {
	case 0: CHECK(TOK_COMMA); break;
	case 1: CHECK(TOK_OR_OP); break;
	case 2: CHECK(TOK_AND_OP); break;
	case 3: CHECK(TOK_BIT_OR_OP); break;
	case 4: CHECK(TOK_BIT_XOR_OP); break;
	case 5: CHECK(TOK_AMP); break;
	case 6:
		CHECK(TOK_EQUAL_OP)
		CHECK(TOK_NOT_EQUAL_OP)
		break;
	case 7:
		CHECK(TOK_LT_OP)
		CHECK(TOK_LE_OP)
		CHECK(TOK_GT_OP)
		CHECK(TOK_GE_OP)
		break;
	case 8:
		CHECK(TOK_LSHIFT_OP)
		CHECK(TOK_RSHIFT_OP)
		break;
	case 9:
		CHECK(TOK_ADD_OP)
		CHECK(TOK_SUB_OP)
		break;
	case 10:
		CHECK(TOK_STAR)
		CHECK(TOK_DIV_OP)
		CHECK(TOK_MOD_OP)
		break;
	}
	return 0;
}

static int accept_assign_expr_token()
{
	CHECK(TOK_ASSIGN)
	CHECK(TOK_ADD_ASSIGN)
	CHECK(TOK_SUB_ASSIGN)
	CHECK(TOK_MUL_ASSIGN)
	CHECK(TOK_DIV_ASSIGN)
	CHECK(TOK_MOD_ASSIGN)
	CHECK(TOK_LSHIFT_ASSIGN)
	CHECK(TOK_RSHIFT_ASSIGN)
	CHECK(TOK_BIT_OR_ASSIGN)
	CHECK(TOK_BIT_AND_ASSIGN)
	CHECK(TOK_BIT_XOR_ASSIGN)
	return 0;
}
#undef CHECK

static struct node *parse_assign_expr()
{
	struct node *node;
	PARSE(node, cond_expr)

	if (!accept_assign_expr_token()) {
		return node;
	}

	ALLOC_NODE_EX(get_node_type(), new_node, binary_node)
	next_token();
	new_node->ops[0] = node;
	PARSE(new_node->ops[1], assign_expr)
	return (struct node*)new_node;
}

static struct node *parse_expr_subnode(int level)
{
	switch (level) {
	case 0: /* comma */
		return parse_assign_expr();
	case 1: /* or */
	case 2: /* and */
	case 3: /* bit or */
	case 4: /* bit xor */
	case 5: /* bit and */
	case 6: /* equality */
	case 7: /* relational */
	case 8: /* shift */
	case 9: /* additive */
		return parse_expr(level + 1);
	case 10: /* multiplicative */
		return parse_cast_expr();
	}
	return NULL;
}

static struct node *parse_expr(int level)
{
	struct node *node;
	PARSE(node, expr_subnode, level)

	while (1) {
		if (!accept_expr_token(level)) {
			return node;
		}
		ALLOC_NODE_EX(get_node_type(), new_node, binary_node)
		next_token();
		new_node->ops[0] = node;
		PARSE(new_node->ops[1], expr_subnode, level)
		node = (struct node*)new_node;
	}
	return node;
}

static struct node *parse_const_expr()
{
	return parse_cond_expr();
}

static struct node *parse_opt_expr_with(enum token_type type)
{
	struct node *node;
	if (accept(type)) {
		PARSE(node, nop)
	} else {
		PARSE(node, expr, 0)
		CONSUME(type)
	}
	return node;
}

static struct node *parse_stmt()
{
	switch (token.type) {
	case TOK_RETURN:
	{
		ALLOC_NODE(NT_RETURN, ret_node)
		next_token();
		if (token.type == TOK_SEMICOLON) {
			PARSE(ret_node->ops[0], nop)
		} else {
			PARSE(ret_node->ops[0], expr, 0)
		}
		CONSUME(TOK_SEMICOLON)
		return (struct node*)ret_node;
	}
	case TOK_WHILE:
	{
		ALLOC_NODE(NT_WHILE, while_node)
		CONSUME(TOK_WHILE)
		CONSUME(TOK_LPAREN)
		PARSE(while_node->ops[0], expr, 0)
		CONSUME(TOK_RPAREN)
		PARSE(while_node->ops[1], stmt)
		return (struct node*)while_node;
	}
	case TOK_DO:
	{
		ALLOC_NODE(NT_DO_WHILE, while_node)
		CONSUME(TOK_DO)
		PARSE(while_node->ops[0], stmt)
		CONSUME(TOK_WHILE)
		CONSUME(TOK_LPAREN)
		PARSE(while_node->ops[1], expr, 0)
		CONSUME(TOK_RPAREN)
		CONSUME(TOK_SEMICOLON)
		return (struct node*)while_node;
	}
	case TOK_FOR:
	{
		ALLOC_NODE(NT_FOR, for_node)
		CONSUME(TOK_FOR)
		CONSUME(TOK_LPAREN)
		PARSE(for_node->ops[0], opt_expr_with, TOK_SEMICOLON)
		PARSE(for_node->ops[1], opt_expr_with, TOK_SEMICOLON)
		PARSE(for_node->ops[2], opt_expr_with, TOK_RPAREN)
		PARSE(for_node->ops[3], stmt)
		return (struct node*)for_node;
	}
	case TOK_IF:
	{
		ALLOC_NODE(NT_IF, if_node)
		CONSUME(TOK_IF)
		CONSUME(TOK_LPAREN)
		PARSE(if_node->ops[0], expr, 0)
		CONSUME(TOK_RPAREN)
		PARSE(if_node->ops[1], stmt)
		if (accept(TOK_ELSE)) {
			PARSE(if_node->ops[2], stmt)
		} else {
			PARSE(if_node->ops[2], nop)
		}
		return (struct node*)if_node;
	}
	case TOK_SWITCH:
	{
		ALLOC_NODE(NT_SWITCH, switch_node)
		CONSUME(TOK_SWITCH)
		CONSUME(TOK_LPAREN)
		PARSE(switch_node->ops[0], expr, 0)
		CONSUME(TOK_RPAREN)
		PARSE(switch_node->ops[1], stmt)
		return (struct node*)switch_node;
	}
	case TOK_LBRACE:
	{
		CONSUME(TOK_LBRACE)
		struct list_node* list_node = alloc_list_node();
		while (!accept(TOK_RBRACE)) {
			list_node->size++;
			list_node_ensure_capacity(list_node, list_node->size);
			PARSE(list_node->items[list_node->size - 1], stmt)
		}
		return (struct node*)list_node;
	}
	case TOK_BREAK:
	{
		CONSUME(TOK_BREAK)
		CONSUME(TOK_SEMICOLON)
		ALLOC_NODE(NT_BREAK, break_node)
		return (struct node*)break_node;
	}
	case TOK_CONTINUE:
	{
		CONSUME(TOK_CONTINUE)
		CONSUME(TOK_SEMICOLON)
		ALLOC_NODE(NT_CONTINUE, continue_node)
		return (struct node*)continue_node;
	}
	case TOK_GOTO:
	{
		ALLOC_NODE(NT_GOTO, goto_node)
		CONSUME(TOK_GOTO)
		PARSE(goto_node->ops[0], ident)
		CONSUME(TOK_SEMICOLON)
		return (struct node*)goto_node;
	}
	case TOK_DEFAULT:
	{
		ALLOC_NODE(NT_DEFAULT, default_node)
		CONSUME(TOK_DEFAULT)
		CONSUME(TOK_COLON)
		PARSE(default_node->ops[0], stmt)
		return (struct node*)default_node;
	}
	case TOK_CASE:
	{
		ALLOC_NODE(NT_CASE, case_node)
		CONSUME(TOK_CASE)
		PARSE(case_node->ops[0], const_expr)
		CONSUME(TOK_COLON)
		PARSE(case_node->ops[1], stmt)
		return (struct node*)case_node;
	}
	default:
	{
		if (accept(TOK_SEMICOLON)) {
			return parse_nop();
		} else if (token.type == TOK_IDENT && token_next.type == TOK_COLON) {
			ALLOC_NODE(NT_LABEL, label_node)
			PARSE(label_node->ops[0], ident)
			CONSUME(TOK_COLON)
			PARSE(label_node->ops[1], stmt)
			return (struct node*)label_node;
		}
		struct node *node;
		PARSE(node, expr, 0)
		CONSUME(TOK_SEMICOLON)
		return (struct node*)node;
	}
	}
}

extern void parser_init()
{
	parser_pull = pull_create();
	token.type = TOK_ERROR;
	token_next.type = TOK_ERROR;
	next_token();
	next_token();
}

extern void parser_destroy()
{
	pull_destroy(parser_pull);
	lexer_token_free_data(&token);
}

static struct node *safe_parsing(struct node *node)
{
	if (node == NULL) {
		pull_free_objects(parser_pull);
		return NULL;
	}
	EXPECT(TOK_EOS)
	pull_clear(parser_pull);
	return node;
}

extern struct node *parser_parse_expr()
{
	return safe_parsing(parse_expr(0));
}

extern struct node *parser_parse_statement()
{
	return safe_parsing(parse_stmt());
}

extern void parser_free_node(struct node *node)
{
	int i;
	if (node == NULL) {
		return;
	}

	for (i = 0; i < parser_node_info(node)->op_count; i++) {
		parser_free_node(parser_get_subnode(node, i));
	}

	switch (node->type)
	{
	case NT_STRING:
	case NT_IDENT:
		jacc_free(((struct string_node*)node)->value);
		break;
	case NT_LIST:
		free(((struct list_node*)node)->items);
		break;
	}
	jacc_free(node);
}

extern struct node_info *parser_node_info(struct node *node)
{
	return &nodes_info[node->type];
}

extern int parser_node_subnodes_count(struct node *node)
{
	if (node->type == NT_LIST) {
		return ((struct list_node*)node)->size;
	}
	return parser_node_info(node)->op_count;
}

extern struct node *parser_get_subnode(struct node *node, int index)
{
	if (node->type == NT_LIST) {
		return ((struct list_node*)node)->items[index];
	}
	return node->ops[index];
}