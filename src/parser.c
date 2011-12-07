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
    ((struct node*)(var_name))->symtable = NULL; \
    ((struct node*)(var_name))->type = (enum_type);

#define EXPECT(token_type) { if (token.type != token_type) { unexpected_token(lexer_token_type_name(token_type)); return NULL; } }
#define CONSUME(token_type) { EXPECT(token_type); next_token(); }

#define PARSE_CHECK(expr) if ((expr) == NULL) return NULL;
#define PARSE(target, rule, ...) { (target) = parse_##rule(__VA_ARGS__); PARSE_CHECK(target); }

#define SYMTABLE_MAX_DEPTH 255
#define SYMTABLE_DEFAULT_SIZE 16

symtable_t symtables[SYMTABLE_MAX_DEPTH];
int current_symtable;
int name_uid = 0;
int parser_flags = 0;
struct symbol sym_null, sym_void, sym_int, sym_float, sym_double, sym_char, sym_char_ptr;

pull_t parser_pull;

struct token token;
struct token token_next;

struct node_info nodes_info[] = {
#define NODE(name, repr, cat, op_cnt) {repr, cat, op_cnt},
#include "nodes.def"
#undef NODE
};

enum declaration_type {
    DT_GLOBAL,
    DT_LOCAL,
    DT_STRUCT,
};

char* generate_name(const char *prefix)
{
    char *buf = jacc_malloc(16);
    sprintf(buf, "%s%d", prefix, ++name_uid);
    return buf;
}

symtable_t get_current_symtable()
{
    return symtables[current_symtable];
}

symtable_t push_symtable_ex(symtable_t symtable)
{
    current_symtable++;
    symtables[current_symtable] = symtable;
    return symtables[current_symtable];
}

symtable_t push_symtable()
{
    symtable_t symtable = symtable_create(SYMTABLE_DEFAULT_SIZE);
    pull_add(parser_pull, symtable);
    return push_symtable_ex(symtable);
}

void pop_symtable()
{
    current_symtable--;
}

void put_symbol(const char *name, struct symbol *symbol, enum symbol_class symclass)
{
    symtable_set(symtables[current_symtable], name, symclass, symbol);
}

struct symbol *get_symbol(const char *name, enum symbol_class symclass)
{
    int i = current_symtable;
    for (; i >= 0; i--) {
        struct symbol *result = symtable_get(symtables[i], name, symclass);
        if (result != NULL) {
            return result;
        }
    }
    return NULL;
}

static int is_type_symbol(struct symbol *symbol)
{
    if (symbol == NULL) {
        return 0;
    }

    switch (symbol->type) {
    case ST_FUNCTION:
    case ST_SCALAR_TYPE:
    case ST_TYPE_ALIAS:
    case ST_STRUCT:
    case ST_UNION:
    case ST_ENUM:
    case ST_ARRAY:
    case ST_POINTER:
        return 1;
    }
    return 0;
}

static int is_var_symbol(struct symbol *symbol)
{
    if (symbol == NULL) {
        return 0;
    }

    switch (symbol->type) {
    case ST_VARIABLE:
    case ST_PARAMETER:
        return 1;
    }
    return 0;
}
static struct node *parse_expr(int level);
static struct node *parse_cast_expr();
static int is_parse_type_specifier();
static int parse_type_qualifier();

static struct symbol *parse_declarator(struct symbol *base_type, const char **name);
static struct symbol *parse_declarator_base(const char **name);
static struct symbol *parse_specifier_qualifier_list();
static struct symbol *parse_type_specifier();

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

static void parser_error(const char *message)
{
    log_set_pos(token.line, token.column);
    log_error(message);
}

static void unexpected_token(const char *string)
{
    char buf[255];
    if (string == NULL) {
        sprintf(buf, "unexpected token %s", lexer_token_type_name(token.type));
    } else {
        sprintf(buf, "unexpected token %s, expected %s", lexer_token_type_name(token.type), string);
    }
    parser_error(buf);
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
        struct node *node;
        CONSUME(TOK_LPAREN)
        if (is_parse_type_specifier()) {
            const char *symbol_name = NULL;
            struct symbol *type;
            PARSE(type, specifier_qualifier_list)
            if (token.type != TOK_RPAREN) {
                PARSE(type, declarator, type, &symbol_name)
            }
            if (symbol_name != NULL) {
                parser_error("cast expression: expected abstract declarator");
                return NULL;
            }
            ALLOC_NODE_EX(NT_CAST, cast_node, cast_node)
            cast_node->base.type_sym = type;
            CONSUME(TOK_RPAREN)
            PARSE(cast_node->ops[0], cast_expr)
            node = (struct node*)cast_node;
        } else {
            PARSE(node, expr, 0)
            CONSUME(TOK_RPAREN)
        }

        return node;
    }
    case TOK_STRING_CONST:
    {
        ALLOC_NODE_EX(NT_STRING, node, string_node)
        node->value = token.value.str_val;
        node->base.type_sym = &sym_char_ptr;
        token.value.str_val = NULL;
        next_token();
        pull_add(parser_pull, node->value);
        return (struct node*)node;
    }
    case TOK_IDENT:
    {
        if ((parser_flags & PF_RESOLVE_NAMES) == PF_RESOLVE_NAMES) {
            EXPECT(TOK_IDENT)
            ALLOC_NODE_EX(NT_VARIABLE, var_node, var_node)
            var_node->symbol = get_symbol(token.value.str_val, SC_NAME);
            if (!is_var_symbol(var_node->symbol)) {
                parser_error("Expected variable type");
                return NULL;
            }
            var_node->base.type_sym = var_node->symbol->base_type;
            next_token();
            return (struct node*)var_node;
        } else {
            return parse_ident();
        }
    }
    case TOK_INT_CONST:
    {
        ALLOC_NODE_EX(NT_INT, node, int_node)
        node->value = token.value.int_val;
        node->base.type_sym = &sym_int;
        next_token();
        return (struct node*)node;
    }
    case TOK_FLOAT_CONST:
    {
        ALLOC_NODE_EX(NT_DOUBLE, node, double_node)
        node->value = token.value.float_val;
        node->base.type_sym = &sym_float;
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
        node->base.type_sym = node->ops[0]->type_sym;
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
    new_node->base.type_sym = node->type_sym;
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

int is_compatible_types(struct symbol *s1, struct symbol *s2);

int is_compatible_symtable(symtable_t s1, symtable_t s2)
{
    if (symtable_size(s1) != symtable_size(s2)) {
        return 0;
    }
    symtable_iter_t iter = symtable_first(s1);
    symtable_iter_t iter2 = symtable_first(s2);
    for (; iter != NULL; iter = symtable_iter_next(iter), iter2 = symtable_iter_next(iter2)) {
        if (!is_compatible_types(symtable_iter_value(iter), symtable_iter_value(iter2))) {
            return 0;
        }
    }
    return 1;
}

int is_compatible_types(struct symbol *s1, struct symbol *s2)
{
    if (s1 == NULL || s2 == NULL) {
        return 0;
    }

    if (s1->type == ST_SCALAR_TYPE && s2->type == ST_SCALAR_TYPE) {
        return s1 == s2;
    }

    int s1_is_ptr = s1->type == ST_POINTER || s1->type == ST_ARRAY;
    int s2_is_ptr = s2->type == ST_POINTER || s2->type == ST_ARRAY;
    int s1_is_variable = s1->type == ST_VARIABLE || s1->type == ST_PARAMETER;
    int s2_is_variable = s2->type == ST_VARIABLE || s2->type == ST_PARAMETER;
    if ((s1_is_ptr && s2_is_ptr) || (s1_is_variable && s2_is_variable)) {
        return is_compatible_types(s1->base_type, s2->base_type);
    }

    if (s1->type == ST_FUNCTION && s2->type == ST_FUNCTION) {
        return is_compatible_types(s1->base_type, s2->base_type)
                && s1->flags == s2->flags
                && is_compatible_symtable(s1->symtable, s2->symtable);
    }

    if ((s1->type == ST_STRUCT || s1->type == ST_UNION) && s1->type == s2->type) {
        return is_compatible_symtable(s1->symtable, s2->symtable);
    }
    return 0;
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

        switch (level) {
        case 3: /* bit or */
        case 4: /* bit xor */
        case 5: /* bit and */
        case 8: /* shift */
        case 9: /* additive */
        case 10: /* multiplicative */
        {
            struct symbol *t1 = new_node->ops[0]->type_sym, *t2 = new_node->ops[1]->type_sym;
            if (is_compatible_types(t1, t2)) {
                new_node->base.type_sym = t1;
            }
            break;
        }
        case 0: /* comma */
            new_node->base.type_sym = new_node->ops[1]->type_sym;
            break;
        case 1: /* or */
        case 2: /* and */
        case 6: /* equality */
        case 7: /* relational */
            new_node->base.type_sym = &sym_int;
            break;
        }
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

static struct symbol *parse_declaration(enum declaration_type decl_type);

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
        while_node->symtable = push_symtable();
        PARSE(while_node->ops[1], stmt)
        pop_symtable();
        return (struct node*)while_node;
    }
    case TOK_DO:
    {
        ALLOC_NODE(NT_DO_WHILE, while_node)
        CONSUME(TOK_DO)
        PARSE(while_node->ops[0], stmt)
        CONSUME(TOK_WHILE)
        CONSUME(TOK_LPAREN)
        while_node->symtable = push_symtable();
        PARSE(while_node->ops[1], expr, 0)
        pop_symtable();
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
        for_node->symtable = push_symtable();
        PARSE(for_node->ops[3], stmt)
        pop_symtable();
        return (struct node*)for_node;
    }
    case TOK_IF:
    {
        ALLOC_NODE(NT_IF, if_node)
        CONSUME(TOK_IF)
        CONSUME(TOK_LPAREN)
        PARSE(if_node->ops[0], expr, 0)
        CONSUME(TOK_RPAREN)
        if_node->symtable = push_symtable();
        PARSE(if_node->ops[1], stmt)
        pop_symtable();
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
        switch_node->symtable = push_symtable();
        PARSE(switch_node->ops[1], stmt)
        pop_symtable();
        return (struct node*)switch_node;
    }
    case TOK_LBRACE:
    {
        CONSUME(TOK_LBRACE)
        struct list_node* list_node = alloc_list_node();
        list_node->base.symtable = push_symtable();
        while (!accept(TOK_RBRACE)) {
            list_node->size++;
            list_node_ensure_capacity(list_node, list_node->size);
            PARSE(list_node->items[list_node->size - 1], stmt)
            if (list_node->items[list_node->size - 1]->type == NT_NOP) {
                parser_free_node(list_node->items[list_node->size - 1]);
                list_node->size--;
            }
        }
        pop_symtable();
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
        if (is_parse_type_specifier(DT_LOCAL)) {
            parse_declaration(DT_LOCAL);
            return parse_nop();
        }
        struct node *node;
        PARSE(node, expr, 0)
        CONSUME(TOK_SEMICOLON)
        return (struct node*)node;
    }
    }
}

static struct symbol *parse_array_declarator(struct symbol *base_type)
{
    struct symbol *array = jacc_malloc(sizeof(*array));
    array->type = ST_ARRAY;
    array->expr = NULL;
    array->flags = 0;

    CONSUME(TOK_LBRACKET)
    if (!accept(TOK_RBRACKET)) {
        PARSE(array->expr, const_expr)
        CONSUME(TOK_RBRACKET)
    }

    if (token.type == TOK_LBRACKET) {
        PARSE(array->base_type, array_declarator, base_type)
    } else {
        array->base_type = base_type;
    }
    return array;
}

static struct symbol *parse_function_declarator(struct symbol *base_type)
{
    struct symbol *func = jacc_malloc(sizeof(*func));
    func->type = ST_FUNCTION;
    func->base_type = base_type;
    func->flags = 0;

    push_symtable();
    func->symtable = get_current_symtable();

    CONSUME(TOK_LPAREN)
    if (token.type != TOK_RPAREN) {
        do {
            if (accept(TOK_ELLIPSIS)) {
                func->flags |= SF_VARIADIC;
                break;
            }

            struct symbol *parameter = jacc_malloc(sizeof(*parameter));

            parameter->name = NULL;
            parameter->type = ST_PARAMETER;

            PARSE(parameter->base_type, type_specifier)
            PARSE(parameter->base_type, declarator, parameter->base_type, &parameter->name)

            if (parameter->name == NULL) {
                parameter->name = generate_name("@arg");
            }
            put_symbol(parameter->name, parameter, SC_NAME);
        } while (accept(TOK_COMMA));
    }
    CONSUME(TOK_RPAREN)
    pop_symtable();
    return func;
}

static struct symbol *get_root_type(struct symbol *symbol)
{
    while (symbol->base_type != &sym_null) {
        symbol = symbol->base_type;
    }
    return symbol;
}

static struct symbol *parse_declarator_base(const char **name)
{
    struct symbol *inner_symbol = &sym_null, *outer_symbol = &sym_null;
    while (token.type == TOK_STAR) {
        struct symbol *pointer = jacc_malloc(sizeof(*pointer));
        pointer->type = ST_POINTER;
        pointer->base_type = outer_symbol;
        pointer->flags = 0;
        outer_symbol = pointer;
        next_token();
    }

    switch (token.type) {
    case TOK_IDENT:
        *name = token.value.str_val;
        token.value.str_val = NULL;
        next_token();
        break;
    case TOK_LPAREN:
        CONSUME(TOK_LPAREN)
        PARSE(inner_symbol, declarator_base, name)
        CONSUME(TOK_RPAREN)
        break;
    }

    if (token.type == TOK_LBRACKET) {
        outer_symbol = parse_array_declarator(outer_symbol);
    } else if (token.type == TOK_LPAREN) {
        outer_symbol = parse_function_declarator(outer_symbol);
    }

    if (inner_symbol != &sym_null) {
        get_root_type(inner_symbol)->base_type = outer_symbol;
        return inner_symbol;
    }
    return outer_symbol;
}

static struct symbol *parse_structured_specifier_start(enum symbol_type symbol_type, const char *name_prefix)
{
    struct symbol *symbol = jacc_malloc(sizeof(*symbol));
    symbol->type = symbol_type;
    symbol->flags = SF_INCOMPLETE;
    symbol->base_type = NULL;
    symbol->name = NULL;
    symbol->symtable = NULL;

    next_token();
    if (token.type == TOK_IDENT) {
        symbol->name = token.value.str_val;
        token.value.str_val = NULL;
        next_token();

        struct symbol *struct_tag = get_symbol(symbol->name, SC_TAG);
        if (struct_tag != NULL) {
            jacc_free((char*)symbol->name);
            jacc_free(symbol);
            return struct_tag;
        }
    } else {
        symbol->name = generate_name(name_prefix);
    }
    put_symbol(symbol->name, symbol, SC_TAG);
    return symbol;
}

static struct symbol *parse_struct_or_union_specifier()
{
    struct symbol *symbol = parse_structured_specifier_start(token.type == TOK_STRUCT ? ST_STRUCT : ST_UNION, "@struct");
    if (accept(TOK_LBRACE)) {
        push_symtable();
        symbol->symtable = get_current_symtable();

        do {
            struct symbol *result;
            PARSE(result, declaration, DT_STRUCT)
        } while (!accept(TOK_RBRACE));
        pop_symtable();
    }
    return symbol;
}

static struct symbol *parse_enum_specifier()
{
    struct symbol *symbol = parse_structured_specifier_start(ST_ENUM, "@enum");
    if (accept(TOK_LBRACE)) {
        int counter = 0;
        while (token.type != TOK_RBRACE) {
            EXPECT(TOK_IDENT)

            ALLOC_NODE_EX(NT_INT, value_node, int_node)
            value_node->value = counter;

            struct symbol *enum_const = jacc_malloc(sizeof(*enum_const));
            enum_const->type = ST_ENUM_CONST;
            enum_const->base_type = symbol;
            enum_const->expr = (struct node*)value_node;
            enum_const->name = token.value.str_val;
            token.value.str_val = NULL;

            put_symbol(enum_const->name, enum_const, SC_NAME);

            next_token();
            if (!accept(TOK_COMMA)) {
                break;
            }
            counter++;
        };
        CONSUME(TOK_RBRACE)
    }
    return symbol;
}

static struct symbol *parse_declarator(struct symbol *base_type, const char **name)
{
    struct symbol *symbol;
    PARSE(symbol, declarator_base, name);
    if (symbol == &sym_null) {
        return base_type;
    }
    get_root_type(symbol)->base_type = base_type;
    return symbol;
}

static int is_parse_type_specifier()
{
    switch (token.type) {
    case TOK_VOID:
    case TOK_CHAR:
    case TOK_INT:
    case TOK_FLOAT:
    case TOK_DOUBLE:
    case TOK_STRUCT:
    case TOK_UNION:
    case TOK_ENUM:
        return 1;
    case TOK_IDENT:
        return is_type_symbol(get_symbol(token.value.str_val, SC_NAME));
    }
    return 0;
}

static struct symbol *parse_type_specifier()
{
    switch (token.type) {
    case TOK_VOID:
        next_token();
        return (struct symbol*)&sym_void;
    case TOK_CHAR:
        next_token();
        return (struct symbol*)&sym_char;
    case TOK_INT:
        next_token();
        return (struct symbol*)&sym_int;
    case TOK_FLOAT:
        next_token();
        return (struct symbol*)&sym_float;
    case TOK_DOUBLE:
        next_token();
        return (struct symbol*)&sym_double;
    case TOK_IDENT:
    {
        struct symbol *symbol = get_symbol(token.value.str_val, SC_NAME);
        if (!is_type_symbol(symbol)) {
            parser_error("typename expected");
            return NULL;
        }
        next_token();
        return symbol;
    }
    case TOK_STRUCT:
    case TOK_UNION:
        return parse_struct_or_union_specifier();
    case TOK_ENUM:
        return parse_enum_specifier();
    }
    return NULL;
}

static int parse_type_qualifier()
{
    return accept(TOK_CONST);
}

static struct symbol *parse_specifier_qualifier_list()
{
    parse_type_qualifier();
    return parse_type_specifier();
}

static struct symbol *parse_declaration_specifiers()
{
    accept(TOK_EXTERN) || accept(TOK_STATIC);
    return parse_specifier_qualifier_list();
}

static struct node *parse_initializer()
{
    return parse_assign_expr();
}

static struct symbol *parse_declaration(enum declaration_type decl_type)
{
    struct symbol *base_type;
    int is_typedef = 0;
    if (decl_type == DT_GLOBAL) {
        is_typedef = accept(TOK_TYPEDEF);
        PARSE(base_type, declaration_specifiers)
    } else {
        PARSE(base_type, specifier_qualifier_list)
    }

    if (accept(TOK_SEMICOLON)) {
        return &sym_null;
    }

    do {
        const char *symbol_name = NULL;
        struct symbol *symbol, *declarator;
        PARSE(declarator, declarator, base_type, &symbol_name)
        if (declarator->type == ST_FUNCTION) {
            symbol = declarator;
        } else {
            symbol = jacc_malloc(sizeof(*symbol));
            symbol->type = is_typedef ? ST_TYPE_ALIAS : ST_VARIABLE;
            symbol->expr = NULL;
            symbol->base_type = declarator;
        }
        symbol->name = symbol_name;

        if (symbol->name == NULL) {
            parser_error("expected non-abstract declarator");
            return NULL;
        }

        if (symbol->type == ST_FUNCTION) {
            if (token.type == TOK_LBRACE) {
                push_symtable_ex(symbol->symtable);
                PARSE(symbol->expr, stmt);
                pop_symtable();
            }
        } else {
            if (accept(TOK_ASSIGN)) {
                PARSE(symbol->expr, initializer)
            }
        }
        put_symbol(symbol->name, symbol, SC_NAME);

        if (symbol->type == ST_FUNCTION && symbol->expr != NULL) {
            return &sym_null;
        }
    } while (accept(TOK_COMMA));

    CONSUME(TOK_SEMICOLON)
    return &sym_null;
}

static void init_type(const char *name, struct symbol *symbol)
{
    symbol->name = name;
    symbol->type = ST_SCALAR_TYPE;
    put_symbol(name, (struct symbol*)symbol, SC_NAME);
}

extern void parser_init()
{
    parser_pull = pull_create();
    current_symtable = 0;
    parser_flags = PF_RESOLVE_NAMES;
    symtables[0] = symtable_create(SYMTABLE_DEFAULT_SIZE);

    init_type("void", &sym_void);
    init_type("int", &sym_int);
    init_type("float", &sym_float);
    init_type("double", &sym_double);
    init_type("char", &sym_char);

    sym_char_ptr.type = ST_POINTER;
    sym_char_ptr.base_type = &sym_char;

    token.type = TOK_ERROR;
    token_next.type = TOK_ERROR;
    next_token();
    next_token();
}

extern void parser_destroy()
{
    symtable_destroy(symtables[0], 0);
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

extern symtable_t parser_parse()
{
    current_symtable = 0;
    push_symtable();
    while (!accept(TOK_EOS)) {
        if (parse_declaration(DT_GLOBAL) == NULL) {
            printf("FAIL on line %d\n", token.line);
            pull_clear(parser_pull);
            return NULL;
        }
    }
    EXPECT(TOK_EOS);
    pull_clear(parser_pull);
    return symtables[1];
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

extern int parser_is_void_symbol(struct symbol *symbol)
{
    return symbol == &sym_void;
}

extern void parser_flags_set(int new_flags)
{
    parser_flags = new_flags;
}

extern int parser_flags_get()
{
    return parser_flags;
}