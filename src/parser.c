#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "log.h"

#define ALLOC_NODE(type, var_name) struct type *var_name = (struct type*)malloc(sizeof(struct type));
#define EXPECT(token_type) { if (token.type != token_type) { unexpected_token(#token_type + 4); return NULL; } }
#define CONSUME(token_type) { EXPECT(token_type); next_token(); }

struct token token;

const char* node_names[] = {
#define NODE(name, str) str,
#include "nodes.def"
#undef NODE
};

static struct node *parse_expr(int level);
static struct node *parse_cast_expr();

static int next_token()
{
	lexer_token_free_data(&token);
	lexer_next_token(&token);
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
	if (string[0] == 0) {
		sprintf(buf, "unexpected token %s", lexer_token_name(&token));
	} else {
		sprintf(buf, "unexpected token %s, expected %s",
		lexer_token_name(&token), string);
	}
	log_set_pos(token.line, token.column);
	log_error(buf);
	string = string;
}

static struct node *parse_ident()
{
	ALLOC_NODE(string_node, node);
	EXPECT(TOK_IDENT);
	node->base.type = NT_IDENT;
	node->base.cat = NC_ATOM;
	node->value = token.value.str_val;
	token.value.str_val = NULL;
	next_token();
	return (struct node*)node;
}

static struct node *parse_primary_expr()
{
	switch (token.type) {
		case TOK_LPAREN:
		{
			next_token();
			struct node *node = parse_expr(0);
			if (node == NULL) {
				return NULL;
			}
			CONSUME(TOK_RPAREN);
			return node;
		}
		case TOK_STRING_CONST:
		{
			ALLOC_NODE(string_node, node);
			node->base.type = NT_STRING;
			node->base.cat = NC_ATOM;
			node->value = token.value.str_val;
			token.value.str_val = NULL;
			next_token();
			return (struct node*)node;
		}
		case TOK_IDENT:
			return parse_ident();
		case TOK_INT_CONST:
		{
			ALLOC_NODE(int_node, node);
			node->base.type = NT_INT;
			node->base.cat = NC_ATOM;
			node->value = token.value.int_val;
			next_token();
			return (struct node*)node;
		}
		case TOK_FLOAT_CONST:
		{
			ALLOC_NODE(double_node, node);
			node->base.type = NT_DOUBLE;
			node->base.cat = NC_ATOM;
			node->value = token.value.float_val;
			next_token();
			return (struct node*)node;
		}
		default:
			unexpected_token("");
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
	}
	return NT_UNKNOWN;
}

static struct node *nop()
{
	ALLOC_NODE(node, node);
	node->type = NT_NOP;
	node->cat = NC_ATOM;
	return node;
}

static struct node *parse_postfix_expr()
{
	struct node *node = parse_primary_expr();
	while (1) {
		switch (token.type) {
			case TOK_INC_OP:
			case TOK_DEC_OP:
			{
				ALLOC_NODE(unary_node, unode);
				if (token.type == TOK_INC_OP) {
					unode->base.type = NT_POSTFIX_INC;
				} else {
					unode->base.type = NT_POSTFIX_DEC;
				}
				unode->base.cat = NC_UNARY;
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
				ALLOC_NODE(unary_node, unode);
				unode->base.type = get_postfix_node_type();
				unode->base.cat = NC_BINARY;
				unode->ops[0] = node;
				next_token();

				switch (unode->base.type) {
				case NT_SUBSCRIPT:
					unode->ops[1] = parse_expr(0);
					break;
				case NT_CALL: /* TODO should be parse_arg_list */
					if (token.type != TOK_RPAREN) {
						unode->ops[1] = parse_expr(0);
					} else {
						unode->ops[1] = nop();
					}
					break;
				case NT_MEMBER:
				case NT_MEMBER_BY_PTR:
					unode->ops[1] = parse_ident();
					break;
				}

				if (unode->ops[1] == NULL) {
					parser_free_node((struct node*)unode);
					return NULL;
				}

				if (unode->base.type == NT_CALL) {
					CONSUME(TOK_RPAREN);
				} else if (unode->base.type == NT_SUBSCRIPT) {
					CONSUME(TOK_RBRACKET);
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
		ALLOC_NODE(unary_node, node);
		node->base.type = get_unary_node_type();
		node->base.cat = NC_UNARY;
		next_token();
		switch (node->base.type) {
		case NT_PREFIX_INC:
		case NT_PREFIX_DEC:
			node->ops[0] = parse_unary_expr();
			break;
		default:
			node->ops[0] = parse_cast_expr();
		}

		if (node->ops[0] == NULL) {
			parser_free_node((struct node*)node);
			return NULL;
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
	struct node *node = parse_expr(1);
	if (node == NULL) {
		return NULL;
	}

	while (1) {
		if (!accept(TOK_QUESTION)) {
			return node;
		}
		ALLOC_NODE(ternary_node, new_node);
		new_node->base.type = NT_TERNARY;
		new_node->base.cat = NC_TERNARY;
		new_node->ops[0] = node;
		new_node->ops[1] = parse_expr(0);
		if (new_node->ops[1] == NULL) {
			new_node->ops[2] = NULL;
			parser_free_node((struct node*)new_node);
			return NULL;
		}
		CONSUME(TOK_COLON);
		new_node->ops[2] = parse_expr(1);
		if (new_node->ops[2] == NULL) {
			parser_free_node((struct node*)new_node);
			return NULL;
		}
		node = (struct node*)new_node;
	}
	return node;
}

static struct node *parse_assign_expr()
{
	return parse_cond_expr();
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
	}
	return NT_UNKNOWN;
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
			CHECK(TOK_EQUAL_OP);
			CHECK(TOK_NOT_EQUAL_OP);
			break;
		case 7:
			CHECK(TOK_LT_OP);
			CHECK(TOK_LE_OP);
			CHECK(TOK_GT_OP);
			CHECK(TOK_GE_OP);
			break;
		case 8:
			CHECK(TOK_LSHIFT_OP);
			CHECK(TOK_RSHIFT_OP);
			break;
		case 9:
			CHECK(TOK_ADD_OP);
			CHECK(TOK_SUB_OP);
			break;
		case 10:
			CHECK(TOK_STAR);
			CHECK(TOK_DIV_OP);
			CHECK(TOK_MOD_OP);
			break;
		}
	return 0;
}
#undef CHECK

static struct node *parse_expr(int level)
{
	struct node *node = parse_expr_subnode(level);

	if (node == NULL) {
		return NULL;
	}

	while (1) {
		if (!accept_expr_token(level)) {
			return node;
		}
		ALLOC_NODE(binary_node, new_node);
		new_node->base.type = get_node_type();
		new_node->base.cat = NC_BINARY;
		next_token();
		new_node->ops[0] = node;
		new_node->ops[1] = parse_expr_subnode(level);

		if (new_node->ops[1] == NULL) {
			parser_free_node((struct node*)new_node);
			return NULL;
		}

		node = (struct node*)new_node;
	}
	return node;
}

extern void parser_init()
{
	token.type = TOK_ERROR;
	next_token();
}

extern void parser_destroy()
{
	lexer_token_free_data(&token);
}

extern struct node *parser_parse_expr()
{
	struct node *node = parse_expr(0);
	if (node != NULL) {
		EXPECT(TOK_EOS);
	}
	return node;
}

extern void parser_free_node(struct node *node)
{
	if (node == NULL) {
		return;
	}

	switch (node->cat)
	{
	case NC_TERNARY:
		parser_free_node(node->ops[0]);
		parser_free_node(node->ops[1]);
		parser_free_node(node->ops[2]);
		break;
	case NC_BINARY:
		parser_free_node(node->ops[0]);
		parser_free_node(node->ops[1]);
		break;
	case NC_UNARY:
		parser_free_node(node->ops[0]);
		break;
	case NC_ATOM:
		switch (node->type) {
		case NT_STRING:
		case NT_IDENT:
			free(((struct string_node*)node)->value);
		}
		break;
	}
	free(node);
}

extern const char *parser_node_name(struct node *node)
{
	return node_names[node->type];
}