#include <stdlib.h>
#include "memory.h"
#include "lexer.h"
#include "parser.h"
#include "log.h"
#include "gc.h"

#define ALLOC_NODE(type, var_name) \
	struct type *var_name = malloc(sizeof(*var_name)); \
	gc_add(parser_gc, var_name);
#define ALLOC_NODE_EX(node_type, var_name, enum_type) \
	ALLOC_NODE(node_type, var_name) \
	((struct node*)(var_name))->type = (enum_type);
#define EXPECT(token_type) { if (token.type != token_type) { unexpected_token(#token_type + 4); return NULL; } }
#define CONSUME(token_type) { EXPECT(token_type); next_token(); }

#define PARSE_CHECK(expr) if ((expr) == NULL) return NULL;
#define PARSE(target, rule, ...) { (target) = parse_##rule(__VA_ARGS__); PARSE_CHECK(target); }

gc_t parser_gc;

struct token token;

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
	EXPECT(TOK_IDENT);
	ALLOC_NODE_EX(string_node, node, NT_IDENT);
	node->value = token.value.str_val;
	token.value.str_val = NULL;
	next_token();
	gc_add(parser_gc, node->value);
	return (struct node*)node;
}

static struct node *parse_primary_expr()
{
	switch (token.type) {
		case TOK_LPAREN:
		{
			next_token();
			struct node *node;
			PARSE(node, expr, 0)
			CONSUME(TOK_RPAREN);
			return node;
		}
		case TOK_STRING_CONST:
		{
			ALLOC_NODE_EX(string_node, node, NT_STRING);
			node->value = token.value.str_val;
			token.value.str_val = NULL;
			next_token();
			gc_add(parser_gc, node->value);
			return (struct node*)node;
		}
		case TOK_IDENT:
			return parse_ident();
		case TOK_INT_CONST:
		{
			ALLOC_NODE_EX(int_node, node, NT_INT);
			node->value = token.value.int_val;
			next_token();
			return (struct node*)node;
		}
		case TOK_FLOAT_CONST:
		{
			ALLOC_NODE_EX(double_node, node, NT_DOUBLE);
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
	case TOK_INC_OP: return NT_POSTFIX_INC;
	case TOK_DEC_OP: return NT_POSTFIX_DEC;
	}
	return NT_UNKNOWN;
}

static struct node *parse_nop()
{
	ALLOC_NODE_EX(node, node, NT_NOP);
	return node;
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
				ALLOC_NODE_EX(unary_node, unode, get_postfix_node_type());
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
				ALLOC_NODE_EX(unary_node, unode, get_postfix_node_type());
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
		ALLOC_NODE_EX(unary_node, node, get_unary_node_type());
		next_token();
		switch (node->base.type) {
		case NT_PREFIX_INC:
		case NT_PREFIX_DEC:
			PARSE(node->ops[0], unary_expr)
			break;
		default:
			PARSE(node->ops[0], unary_expr)
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
		ALLOC_NODE_EX(ternary_node, new_node, NT_TERNARY);
		new_node->ops[0] = node;
		PARSE(new_node->ops[1], expr, 0)
		CONSUME(TOK_COLON);
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

	ALLOC_NODE_EX(binary_node, new_node, get_node_type());
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
	PARSE(node, expr_subnode, level);

	while (1) {
		if (!accept_expr_token(level)) {
			return node;
		}
		ALLOC_NODE_EX(binary_node, new_node, get_node_type());
		next_token();
		new_node->ops[0] = node;
		PARSE(new_node->ops[1], expr_subnode, level);
		node = (struct node*)new_node;
	}
	return node;
}

static struct node *parse_stmt()
{
	switch (token.type) {
	case TOK_RETURN:
	{
		ALLOC_NODE_EX(return_node, ret_node, NT_RETURN);
		next_token();
		if (token.type == TOK_SEMICOLON) {
			PARSE(ret_node->ops[0], nop)
		} else {
			PARSE(ret_node->ops[0], expr, 0)
		}
		CONSUME(TOK_SEMICOLON);
		return (struct node*)ret_node;
	}
	default:
		CONSUME(TOK_SEMICOLON);
		return parse_nop();
	}
}

extern void parser_init()
{
	parser_gc = gc_create();
	token.type = TOK_ERROR;
	next_token();
}

extern void parser_destroy()
{
	gc_destroy(parser_gc);
	lexer_token_free_data(&token);
}

extern struct node *parser_parse_expr()
{
	struct node *node = parse_expr(0);
	if (node == NULL) {
		gc_free_objects(parser_gc);
		return NULL;
	}
	EXPECT(TOK_EOS);
	gc_clear(parser_gc);
	return node;
}

extern struct node *parser_parse_statement()
{
	struct node *node = parse_stmt(0);
	if (node == NULL) {
		gc_free_objects(parser_gc);
		return NULL;
	}
	EXPECT(TOK_EOS);
	gc_clear(parser_gc);
	return node;
}

extern void parser_free_node(struct node *node)
{
	int i;
	if (node == NULL) {
		return;
	}

	for (i = 0; i < parser_node_info(node)->op_count; i++) {
		parser_free_node(node->ops[i]);
	}

	switch (node->type)
	{
	case NT_STRING:
	case NT_IDENT:
		free(((struct string_node*)node)->value);
		break;
	}
	free(node);
}

extern struct node_info *parser_node_info(struct node *node)
{
	return &nodes_info[node->type];
}