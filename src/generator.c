#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "generator.h"
#include "memory.h"
#include "symtable.h"
#include "parser.h"

struct asm_command commands[] = {
#define COMMAND(name, repr, op_count) { #repr, ASM_##name, op_count },
#include "commands.def"
#undef COMMAND
};

struct asm_operand registers[] = {
#define REGISTER(name, varname) { AOT_REGISTER, {}},
#include "registers.def"
#undef REGISTER
};

#define REGISTER(name, varname) struct asm_operand * varname = & registers[ART_##name];
#include "registers.def"
#undef REGISTER

code_t cur_code;
int label_counter;

static int print_operand(struct asm_operand *op)
{
    if (op == NULL) {
        return 0;
    }

    switch (op->type) {
    case AOT_MEMORY:
    {
        int cnt = 0;
        switch (op->data.memory.size) {
        case AOS_BYTE: printf("byte "); break;
        case AOS_WORD: printf("word "); break;
        case AOS_DWORD: printf("dword "); break;
        case AOS_QWORD: printf("qword "); break;
        }

        printf("[");
        cnt += print_operand(op->data.memory.base);
        if (op->data.memory.index != NULL) {
            if (cnt) printf(" + ");
            cnt += print_operand(op->data.memory.index);
            if (op->data.memory.scale != 1) {
                printf("*%d", op->data.memory.scale);
            }
        }

        if (op->data.memory.offset != NULL) {
            if (cnt) {
                if (op->data.memory.offset->type == AOT_CONSTANT) {
                    int value = op->data.memory.offset->data.value;
                    if (value < 0) printf(" - %d", -value);
                    else if (value > 0) printf(" + %d", value);
                } else {
                    printf(" + ");
                    cnt += print_operand(op->data.memory.offset);
                }
            } else {
                cnt += print_operand(op->data.memory.offset);
            }
        }

        printf("]");
        break;
    }
    case AOT_REGISTER:
        printf("%s", op->data.register_name);
        break;
    case AOT_CONSTANT:
        printf("%d", op->data.value);
        break;
    case AOT_LABEL:
        printf("_@%d", op->data.label);
        break;
    case AOT_TEXT_LABEL:
        printf("_%s", op->data.text_label);
        break;
    }
    return 1;
}

static void print_opcode(struct asm_opcode *opcode)
{
    switch (opcode->type) {
    case ACT_COMMAND:
    {
        int i;
        struct asm_command *cmd = opcode->data.command.cmd;
        printf("\t%s", cmd->name);
        for (i = 0; i < cmd->op_count; i++) {
            printf(i == 0 ? " " : ", ");
            print_operand(opcode->data.command.ops[i]);
        }
        printf("\n");
        break;
    }
    case ACT_NOP:
        printf("; nop\n");
        break;
    case ACT_TEXT:
    case ACT_DATA:
        printf("%s\n", opcode->data.text);
        break;
    }
}

static void add_opcode(struct asm_opcode_list *list, struct asm_opcode *opcode)
{
    int new_size = 0;
    if (list->count == 0) {
        new_size = 64;
    } else if (list->count + 1 > list->size) {
        new_size = list->size * 2;
    }

    if (new_size) {
        list->size = new_size;
        list->data = jacc_realloc(list->data, new_size * sizeof(*list->data));
    }
    list->data[list->count] = opcode;
    list->count++;
}

static void emit(enum asm_command_type cmd, ...)
{
    struct asm_opcode *opcode = jacc_malloc(sizeof(*opcode));
    opcode->type = ACT_COMMAND;
    opcode->data.command.cmd = &commands[cmd];

    va_list args;
    va_start(args, cmd);
    int i;
    for (i = 0; i < commands[cmd].op_count; i++) {
        opcode->data.command.ops[i] = va_arg(args, struct asm_operand *);
    }
    va_end(args);

    add_opcode(&cur_code->opcode_list, opcode);
}

static void emit_text(char *format, ...)
{
    struct asm_opcode *opcode = jacc_malloc(sizeof(*opcode));
    opcode->type = ACT_TEXT;

    char *buffer = jacc_malloc(256);
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    opcode->data.text = buffer;
    add_opcode(&cur_code->opcode_list, opcode);
}

static void emit_label(label_t label)
{
    emit_text("_@%d:", label);
}

static void emit_data(char *data)
{
    struct asm_opcode *opcode = jacc_malloc(sizeof(*opcode));
    opcode->type = ACT_DATA;
    opcode->data.text = data;
    add_opcode(&cur_code->data_list, opcode);
}

extern struct asm_operand *constant(int value)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_CONSTANT;
    operand->data.value = value;
    return operand;
}

static struct asm_operand *label(label_t label)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_LABEL;
    operand->data.label = label;
    return operand;
}

static struct asm_operand *text_label(const char *label)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_TEXT_LABEL;
    operand->data.text_label = label;
    return operand;
}

static struct asm_operand *memory(struct asm_operand *base, struct asm_operand *offset, struct asm_operand *index, int scale)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_MEMORY;
    operand->data.memory.base = base;
    operand->data.memory.offset = offset;
    operand->data.memory.index = index;
    operand->data.memory.scale = scale;
    operand->data.memory.size = AOS_DWORD;
    return operand;
}

static struct asm_operand *deref(struct asm_operand *base)
{
    return memory(base, NULL, NULL, 1);
}

static struct asm_operand *size_spec(enum asm_operand_size size, struct asm_operand *op)
{
    if (op->type == AOT_MEMORY) {
        op->data.memory.size = size;
    }
    return op;
}

static struct asm_operand *dword(struct asm_operand *subop)
{
    return size_spec(AOS_DWORD, subop);
}

static struct asm_operand *qword(struct asm_operand *subop)
{
    return size_spec(AOS_QWORD, subop);
}

static void push_value(struct asm_operand *op, struct symbol *type, int ret)
{
    if (!ret) {
        return;
    }
    type = resolve_alias(type);
    if (type == &sym_double) {
        emit(ASM_SUB, esp, constant(8));
        emit(ASM_FLD, qword(op));
        emit(ASM_FSTP, qword(deref(esp)));
    } else {
        emit(ASM_PUSH, dword(op));
    }
}

static label_t gen_label()
{
    return ++label_counter;
}

static void generate_expr(struct node *expr, int ret);

static struct asm_operand *lvalue(struct node *expr)
{
    switch (expr->type) {
    case NT_VARIABLE:
    {
        struct var_node *var = (struct var_node*)expr;
        switch (var->symbol->type) {
        case ST_PARAMETER:
            return dword(memory(ebp, constant(var->symbol->offset + 8), NULL, 1));
        case ST_VARIABLE:
            return dword(memory(ebp, constant(var->symbol->offset - var->symbol->size), NULL, 1));
        case ST_GLOBAL_VARIABLE:
            if (var->symbol->label == 0) {
                var->symbol->label = gen_label();
                char *buf = jacc_malloc(30);
                sprintf(buf, "_@%d db %d dup(0)", var->symbol->label, var->symbol->size);
                emit_data(buf);
            }
            return dword(deref(label(var->symbol->label)));
        default:
            emit_text("; unhandled var symbol %d", var->symbol->type);
        }
        break;
    }
    case NT_DEREFERENCE:
        generate_expr(expr->ops[0], 1);
        emit(ASM_POP, eax);
        return deref(eax);
    case NT_MEMBER:
        emit(ASM_LEA, eax, lvalue(expr->ops[0]));
        return memory(eax, constant(expr->ops[1]->type_sym->offset), NULL, 1);
    default:
        emit_text("; '%s' is not lvalue", parser_node_info(expr)->repr);
    }
    return deref(constant(0));
}

static void generate_lvalue(struct node *expr)
{
    emit(ASM_LEA, eax, lvalue(expr));
    emit(ASM_PUSH, eax);
}

static void generate_int_cmp(enum asm_command_type cmd)
{
    emit(ASM_XOR, ecx, ecx);
    emit(ASM_CMP, eax, ebx);
    emit(cmd, cl);
    emit(ASM_MOV, eax, ecx);
}

static void generate_int_logical_op(enum asm_command_type cmd)
{
    label_t lbl = gen_label();
    emit(ASM_XOR, ecx, ecx);
    emit(ASM_TEST, eax, eax);
    emit(cmd, label(lbl));
    emit(ASM_TEST, ebx, ebx);
    emit_label(lbl);
    emit(ASM_SETNZ, cl);
    emit(ASM_MOV, eax, ecx);
}

static void generate_unary_int_op(struct node *expr, int ret)
{
    generate_expr(expr->ops[0], 1);
    emit(ASM_POP, eax);
    switch (expr->type) {
    case NT_LOGICAL_NEGATION:
        emit(ASM_XOR, ecx, ecx);
        emit(ASM_TEST, eax, eax);
        emit(ASM_SETZ, cl);
        emit(ASM_MOV, eax, eax);
        break;
    case NT_COMPLEMENT: emit(ASM_NOT, eax); break;
    case NT_NEGATION: emit(ASM_NEG, eax); break;
    case NT_IDENTITY: break;
    case NT_PREFIX_INC:
    case NT_PREFIX_DEC:
        generate_lvalue(expr->ops[0]);
        emit(ASM_POP, ebx);
        if (expr->type == NT_PREFIX_INC)
            emit(ASM_INC, dword(deref(ebx)));
        else
            emit(ASM_DEC, dword(deref(ebx)));
        if (ret) emit(ASM_MOV, eax, dword(deref(ebx)));
        break;
    case NT_POSTFIX_INC:
    case NT_POSTFIX_DEC:
        generate_lvalue(expr->ops[0]);
        emit(ASM_POP, ebx);
        if (ret) emit(ASM_MOV, eax, dword(deref(ebx)));
        if (expr->type == NT_POSTFIX_INC)
            emit(ASM_INC, dword(deref(ebx)));
        else
            emit(ASM_DEC, dword(deref(ebx)));
        break;
    default:
        emit_text("; unknown unary node %s", parser_node_info(expr)->repr);
    }
    if (ret) emit(ASM_PUSH, eax);
}

static void generate_binary_int_op(struct node *expr, int ret)
{
    generate_expr(expr->ops[0], 1);
    generate_expr(expr->ops[1], 1);
    emit(ASM_POP, ebx);
    emit(ASM_POP, eax);

    switch (expr->type) {
    case NT_ADD: emit(ASM_ADD, eax, ebx); break;
    case NT_SUB: emit(ASM_SUB, eax, ebx); break;
    case NT_MUL: emit(ASM_IMUL, ebx); break;
    case NT_LSHIFT:
        emit(ASM_MOV, ecx, ebx);
        emit(ASM_SAL, eax, cl);
        break;
    case NT_RSHIFT:
        emit(ASM_MOV, ecx, ebx);
        emit(ASM_SAR, eax, cl);
        break;
    case NT_DIV:
    case NT_MOD:
        emit(ASM_CDQ);
        emit(ASM_IDIV, ebx);
        if (expr->type == NT_MOD) emit(ASM_MOV, eax, edx);
        break;
    case NT_EQ: generate_int_cmp(ASM_SETE); break;
    case NT_NE: generate_int_cmp(ASM_SETNE); break;
    case NT_LE: generate_int_cmp(ASM_SETLE); break;
    case NT_LT: generate_int_cmp(ASM_SETL); break;
    case NT_GE: generate_int_cmp(ASM_SETGE); break;
    case NT_GT: generate_int_cmp(ASM_SETG); break;
    case NT_BIT_XOR: emit(ASM_XOR, eax, ebx); break;
    case NT_BIT_OR: emit(ASM_OR, eax, ebx); break;
    case NT_BIT_AND: emit(ASM_AND, eax, ebx); break;
    case NT_AND: generate_int_logical_op(ASM_JZ); break;
    case NT_OR: generate_int_logical_op(ASM_JNZ); break;
    default:
        emit_text("; unknown binary node %s", parser_node_info(expr)->repr);
    }
    if (ret) emit(ASM_PUSH, eax);
}

static void generate_double_cmp(enum asm_command_type cmd)
{
    emit(ASM_ADD, esp, constant(8));
    emit(ASM_XOR, ecx, ecx);
    emit(ASM_FCOMIP, st1);
    emit(cmd, cl);
    emit(ASM_PUSH, ecx);
    emit(ASM_FFREEP, st0);
}


static void generate_unary_double_op(struct node *expr, int ret)
{
    switch (expr->type) {
    case NT_LOGICAL_NEGATION:
        generate_expr(expr->ops[0], ret);
        emit(ASM_FLD, qword(deref(esp)));
        emit(ASM_FLDZ);
        emit(ASM_XOR, ecx, ecx);
        emit(ASM_FCOMIP, st1);
        emit(ASM_SETE, cl);
        emit(ASM_FFREEP, st0);
        if (ret) {
            emit(ASM_MOV, memory(esp, constant(4), NULL, 0), ecx);
            emit(ASM_ADD, esp, constant(4));
        } else {
            emit(ASM_ADD, esp, constant(8));
        }
        break;
    case NT_NEGATION:
        generate_expr(expr->ops[0], ret);
        if (ret) {
            emit(ASM_FLD, qword(deref(esp)));
            emit(ASM_FCHS);
            emit(ASM_FSTP, qword(deref(esp)));
        } else {
            emit(ASM_ADD, esp, constant(8));
        }
        break;
    case NT_IDENTITY:
        generate_expr(expr->ops[0], ret);
        if (!ret) {
            emit(ASM_ADD, esp, constant(8));
        }
        break;
    case NT_PREFIX_INC:
    case NT_PREFIX_DEC:
        generate_lvalue(expr->ops[0]);
        emit(ASM_POP, eax);
        emit(ASM_FLD, qword(deref(eax)));
        emit(ASM_FLD1);
        if (expr->type == NT_PREFIX_INC)
            emit(ASM_FADDP);
        else
            emit(ASM_FSUBP);
        if (ret) {
            emit(ASM_FST, qword(deref(eax)));
            emit(ASM_SUB, esp, constant(8));
            emit(ASM_FSTP, qword(deref(esp)));
        } else {
            emit(ASM_FSTP, qword(deref(eax)));
        }
        break;
    case NT_POSTFIX_INC:
    case NT_POSTFIX_DEC:
        generate_lvalue(expr->ops[0]);
        emit(ASM_POP, eax);
        emit(ASM_FLD, qword(deref(eax)));
        if (ret) {
            emit(ASM_SUB, esp, constant(8));
            emit(ASM_FST, qword(deref(esp)));
        }
        emit(ASM_FLD1);
        if (expr->type == NT_POSTFIX_INC)
            emit(ASM_FADDP);
        else
            emit(ASM_FSUBP);
        emit(ASM_FSTP, qword(deref(eax)));
        break;
    default:
        emit_text("; unknown unary node %s", parser_node_info(expr)->repr);
    }
}

static void generate_binary_double_op(struct node *expr, int ret)
{
    generate_expr(expr->ops[0], 1);
    emit(ASM_FLD, qword(deref(esp)));
    generate_expr(expr->ops[1], 1);
    emit(ASM_FLD, qword(deref(esp)));

    emit(ASM_ADD, esp, constant(ret ? 8 : 16));

    switch (expr->type) {
    case NT_ADD: emit(ASM_FADDP); break;
    case NT_SUB: emit(ASM_FSUBP); break;
    case NT_MUL: emit(ASM_FMULP); break;
    case NT_DIV: emit(ASM_FDIVP); break;
    case NT_EQ: generate_double_cmp(ASM_SETE); return;
    case NT_NE: generate_double_cmp(ASM_SETNE); return;
    case NT_LE: generate_double_cmp(ASM_SETAE); return;
    case NT_LT: generate_double_cmp(ASM_SETA); return;
    case NT_GE: generate_double_cmp(ASM_SETBE); return;
    case NT_GT: generate_double_cmp(ASM_SETB); return;
    default:
        emit_text("; unknown binary node %s", parser_node_info(expr)->repr);
    }

    if (ret) {
        emit(ASM_FSTP, qword(deref(esp)));
    }
    emit(ASM_FFREEP, st0);
}

static void generate_statement(struct node *expr)
{
    switch (expr->type) {
    case NT_IF:
    {
        label_t l1 = gen_label(), l2 = gen_label();
        generate_expr(expr->ops[0], 1);
        emit(ASM_POP, eax);
        emit(ASM_TEST, eax, eax);
        emit(ASM_JZ, label(l1));
        generate_expr(expr->ops[1], 0);
        emit(ASM_JMP, label(l2));
        emit_label(l1);
        generate_expr(expr->ops[2], 0);
        emit_label(l2);
        break;
    }
    case NT_WHILE:
    {
        label_t l1 = gen_label(), l2 = gen_label();
        emit_label(l1);
        generate_expr(expr->ops[0], 1);
        emit(ASM_POP, eax);
        emit(ASM_TEST, eax, eax);
        emit(ASM_JZ, label(l2));
        generate_expr(expr->ops[1], 0);
        emit(ASM_JMP, label(l1));
        emit_label(l2);
        break;
    }
    case NT_DO_WHILE:
    {
        label_t l1 = gen_label();
        emit_label(l1);
        generate_expr(expr->ops[0], 0);
        generate_expr(expr->ops[1], 1);
        emit(ASM_POP, eax);
        emit(ASM_TEST, eax, eax);
        emit(ASM_JNZ, label(l1));
        break;
    }
    case NT_FOR:
    {
        label_t l1 = gen_label(), l2 = gen_label();
        generate_expr(expr->ops[0], 0);
        emit_label(l1);
        generate_expr(expr->ops[1], 1);
        emit(ASM_POP, eax);
        emit(ASM_TEST, eax, eax);
        emit(ASM_JZ, label(l2));
        generate_expr(expr->ops[3], 0);
        generate_expr(expr->ops[2], 0);
        emit(ASM_JMP, label(l1));
        emit_label(l2);
        break;
    }
    default:
        emit_text("; unknown statement %s", parser_node_info(expr)->repr);
    }
}

static label_t emit_data_array(const char *data_ptr, int size)
{
    label_t str_label = gen_label();
    char *buf = jacc_malloc(15 + 4 * size);
    char *ptr = buf;

    ptr += sprintf(ptr, "_@%d db ", (int)str_label);
    int i = 0;
    for (; i < size; i++) {
        ptr += sprintf(ptr, i == 0 ? "%d" : ",%d", data_ptr[i] < 0 ? 256 + data_ptr[i] : data_ptr[i]);
    }

    emit_data(buf);
    return str_label;
}

static void generate_expr(struct node *expr, int ret)
{
    if (expr == NULL) {
        return;
    }

    switch (expr->type) {
    case NT_NOP:
        return;
    case NT_LIST:
    {
        struct list_node *list = (struct list_node*)expr;
        int i = 0;
        for (; i < list->size; i++) {
            generate_expr(list->items[i], 0);
        }
        return;
    }
    case NT_CALL:
    {
        struct list_node *list = (struct list_node*)expr->ops[1];
        int i = list->size - 1, size = 0;
        for (; i >= 0; i--) {
            generate_expr(list->items[i], 1);
            size += list->items[i]->type_sym->size;
        }
        struct asm_operand *target = text_label(expr->ops[0]->type_sym->name);
        if ((expr->ops[0]->type_sym->flags & SF_EXTERN) == SF_EXTERN) {
            target = deref(target);
        }
        emit(ASM_CALL, target); // @todo function pointers
        emit(ASM_ADD, esp, constant(size));
        return;
    }
    case NT_STRING:
    {
        char *str = ((struct string_node*)expr)->value;
        label_t str_label = emit_data_array(str, strlen(str) + 1);
        if (ret) emit(ASM_PUSH, label(str_label));
        return;
    }
    case NT_DOUBLE:
    {
        double value = ((struct double_node*)expr)->value;
        label_t data_label = emit_data_array((char *)&value, 8);
        push_value(deref(label(data_label)), expr->type_sym, ret);
        return;
    }
    case NT_MEMBER:
        emit(ASM_LEA, eax, lvalue(expr));
        push_value(deref(eax), expr->type_sym, ret);
        return;
    case NT_TERNARY:
    {
        label_t l1 = gen_label(), l2 = gen_label();
        generate_expr(expr->ops[0], 1);
        emit(ASM_POP, eax);
        emit(ASM_TEST, eax, eax);
        emit(ASM_JZ, label(l1));
        generate_expr(expr->ops[1], ret);
        emit(ASM_JMP, label(l2));
        emit_label(l1);
        generate_expr(expr->ops[2], ret);
        emit_label(l2);
        return;
    }
    case NT_ASSIGN:
    {
        struct symbol *s0 = resolve_alias(expr->ops[0]->type_sym);
        if (s0 == &sym_double) {
            generate_expr(expr->ops[1], 1);
            emit(ASM_FLD, qword(deref(esp)));
            generate_lvalue(expr->ops[0]);
            emit(ASM_POP, eax);
            if (ret) {
                emit(ASM_FST, qword(deref(esp)));
            } else {
                emit(ASM_ADD, esp, constant(8));
            }
            emit(ASM_FSTP, qword(deref(eax)));
        } else {
            generate_lvalue(expr->ops[0]);
            generate_expr(expr->ops[1], 1);
            emit(ASM_POP, ebx);
            emit(ASM_POP, eax);
            emit(ASM_MOV, deref(eax), ebx);
            if (ret) emit(ASM_PUSH, ebx);
        }
        return;
    }
    case NT_CAST:
    {
        struct symbol *ret_type = resolve_alias(expr->type_sym), *s0 = resolve_alias(expr->ops[0]->type_sym);
        generate_expr(expr->ops[0], ret);
        if (ret) {
            if ((is_compatible_types(s0, &sym_int) || is_ptr_type(s0)) && ret_type == &sym_double) {
                emit(ASM_FILD, dword(deref(esp)));
                emit(ASM_SUB, esp, constant(4));
                emit(ASM_FSTP, qword(deref(esp)));
            } else if (s0 == &sym_double && (is_compatible_types(ret_type, &sym_int) || is_ptr_type(ret_type))) {
                emit(ASM_FLD, qword(deref(esp)));
                emit(ASM_FISTTP, dword(memory(esp, constant(4), NULL, 0)));
                emit(ASM_ADD, esp, constant(4));
            }
        }
        return;
    }

    case NT_REFERENCE:
        emit(ASM_LEA, eax, lvalue(expr->ops[0]));
        if (ret) emit(ASM_PUSH, eax);
        return;
    case NT_DEREFERENCE:
        generate_expr(expr->ops[0], 1);
        emit(ASM_POP, eax);
        emit_text("; ");
        push_value(dword(deref(eax)), expr->type_sym, ret);
        return;
    case NT_VARIABLE:
        push_value(lvalue(expr), expr->type_sym, ret);
        return;
    case NT_INT:
        if (ret) emit(ASM_PUSH, constant(((struct int_node*)expr)->value));
        return;
    case NT_ADD:
    {
        struct symbol *s1 = resolve_alias(expr->ops[0]->type_sym);
        if (is_ptr_type(s1)) {
            generate_expr(expr->ops[0], 1);
            generate_expr(expr->ops[1], 1);
            emit(ASM_POP, eax);
            emit(ASM_MOV, ebx, constant(s1->base_type->size));
            emit(ASM_MUL, ebx);
            emit(ASM_POP, ebx);
            emit(ASM_ADD, eax, ebx);
            if (ret) emit(ASM_PUSH, eax);
            return;
        }
        break;
    }
    }

    enum node_category cat = parser_node_info(expr)->cat;
    switch (cat) {
    case NC_UNARY:
    {
        struct symbol *s0 = resolve_alias(expr->ops[0]->type_sym);
        if (s0 == &sym_double) {
            generate_unary_double_op(expr, ret);
            return;
        } else if (is_compatible_types(s0, &sym_int) || s0->type == ST_POINTER) {
            generate_unary_int_op(expr, ret);
            return;
        }
        return;
    }
    case NC_BINARY:
    {
        struct symbol *s0 = resolve_alias(expr->ops[0]->type_sym);
        if (s0 == &sym_double) {
            generate_binary_double_op(expr, ret);
            return;
        } else if (is_compatible_types(s0, &sym_int) || s0->type == ST_POINTER) {
            generate_binary_int_op(expr, ret);
            return;
        }
    }
    case NC_STATEMENT:
        generate_statement(expr);
        return;
    }
    emit_text("; unknown node %s", parser_node_info(expr)->repr);
}

static void generate_function(struct symbol *func)
{
    if ((func->flags & SF_EXTERN) == SF_EXTERN) {
        return;
    }

    int is_main = strcmp(func->name, "main") == 0;

    emit_text("; start %s", func->name);

    emit_text("_%s:", func->name);

    if (is_main) {
        emit(ASM_MOV, dword(deref(text_label("@main_esp"))), esp);
    }
    emit(ASM_PUSH, ebp);
    emit(ASM_MOV, ebp, esp);
    if (func->locals_size != 0) {
        emit(ASM_SUB, esp, constant(func->locals_size));
    }

    generate_expr(func->expr, 0);

    if (func->locals_size != 0) {
        emit(ASM_ADD, esp, constant(func->locals_size));
    }

    emit(ASM_MOV, esp, ebp);
    emit(ASM_POP, ebp);

    if (is_main) {
        label_t l1 = gen_label();
        emit(ASM_CMP, esp, dword(deref(text_label("@main_esp"))));
        emit(ASM_JE, label(l1));
        emit(ASM_PUSH, text_label("@stack_corruption_msg"));
        emit(ASM_CALL, deref(text_label("printf")));
        emit(ASM_ADD, esp, constant(4));

        emit_label(l1);
        emit(ASM_PUSH, constant(0));
        emit(ASM_CALL, deref(text_label("ExitProcess")));
    } else {
        emit(ASM_RET);
    }

    emit_text("; end %s\n", func->name);
}

extern void generator_init()
{
#define REGISTER(name, varname) registers[ART_##name].data.register_name = #varname;
#include "registers.def"
#undef REGISTER

    label_counter = 0;
}

extern void generator_destroy()
{

}

extern code_t generator_process(symtable_t symtable)
{
    code_t code = jacc_malloc(sizeof(*code));
    memset(code, 0, sizeof(*code));

    cur_code = code;

    symtable_iter_t iter = symtable_first(symtable);
    for (; iter != NULL; iter = symtable_iter_next(iter)) {
        struct symbol *symbol = symtable_iter_value(iter);
        if (symbol->type == ST_FUNCTION) {
            generate_function(symbol);
        }
    }
    return code;
}

extern void generator_print_code(code_t code)
{
    int i;
    printf("format PE console\nentry _main\n");
    printf("include '%%fasm%%/include/win32a.inc'\n\n");
    printf("section '.text' code executable\n");
    for (i = 0; i < code->opcode_list.count; i++) {
        print_opcode(code->opcode_list.data[i]);
    }

    printf("section '.data' data readable writable\n\n");
    printf("_@main_esp dd ?\n");
    printf("_@stack_corruption_msg db \"Stack corruption\",10,0\n");

    for (i = 0; i < code->data_list.count; i++) {
        print_opcode(code->data_list.data[i]);
    }

    printf("\nsection '.idata' data readable import\n");
    printf("library kernel32, 'kernel32.dll', msvcrt, 'msvcrt.dll'\n");
    printf("import kernel32, _ExitProcess, 'ExitProcess'\n");
    printf("import msvcrt, _printf, 'printf'\n");
}

static void free_memory_subop(struct asm_operand *operand)
{
    if (operand != NULL && operand->type == AOT_CONSTANT) {
        jacc_free(operand);
    }
}

extern void generator_free_operand_data(struct asm_operand *operand)
{
    switch (operand->type) {
    case AOT_MEMORY:
        free_memory_subop(operand->data.memory.base);
        free_memory_subop(operand->data.memory.index);
        free_memory_subop(operand->data.memory.offset);
        break;
    }
}

extern void generator_free_opcode_data(struct asm_opcode *opcode)
{
    switch (opcode->type) {
    case ACT_COMMAND:
    {
        int i;
        for (i = 0; i < opcode->data.command.cmd->op_count; i++) {
            generator_free_operand_data(opcode->data.command.ops[i]);
        }
        break;
    }
    case ACT_TEXT:
    case ACT_DATA:
        jacc_free(opcode->data.text);
        break;
    }
}

static void free_opcode_list_data(struct asm_opcode_list *list)
{
    int i;
    for (i = 0; i < list->count; i++) {
        generator_free_opcode_data(list->data[i]);
        jacc_free(list->data[i]);
    }
    jacc_free(list->data);
}

extern void generator_free_code(code_t code)
{
    if (code == NULL) {
        return;
    }
    free_opcode_list_data(&code->opcode_list);
    free_opcode_list_data(&code->data_list);
    jacc_free(code);
}

extern struct asm_command *generator_get_command(enum asm_command_type type)
{
    return &commands[type];
}
