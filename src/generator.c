#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "generator.h"
#include "memory.h"
#include "symtable.h"
#include "parser.h"

asm_instruction_info_t instructions[] = {
#define COMMAND(name, repr, op_count) { ASM_##name, #repr, op_count },
#include "commands.def"
#undef COMMAND
};

asm_register_info_t registers[] = {
#define REGISTER(name, varname) #varname,
#include "registers.def"
#undef REGISTER
};

#define REGISTER(name, varname) asm_operand_t varname = { \
    AOT_REGISTER, AR_##name, { 0, NULL }, { \
        { AOT_REGISTER, AR_NONE, { 0, NULL } }, \
        { AOT_REGISTER, AR_NONE, { 0, NULL } }, \
        AOS_DWORD, 0, 0 \
    }, 0 };
#include "registers.def"
#undef REGISTER

code_t cur_code;
int label_counter;
struct symbol *cur_function;

static int print_operand(asm_operand_t *op)
{
    if (op == NULL) {
        return 0;
    }

    switch (op->type) {
    case AOT_MEMORY:
    {
        int cnt = 0;
        switch (op->memory.size) {
        case AOS_BYTE: printf("byte "); break;
        case AOS_WORD: printf("word "); break;
        case AOS_DWORD: printf("dword "); break;
        case AOS_QWORD: printf("qword "); break;
        }

        printf("[");
        cnt += print_operand((asm_operand_t*)&op->memory.base);
        if (op->memory.scale && !(op->memory.index.type == AOT_REGISTER && op->memory.index.reg == AR_NONE))  {
            if (cnt) printf(" + ");
            cnt += print_operand((asm_operand_t*)&op->memory.index);
            if (op->memory.scale != 1) {
                printf("*%d", op->memory.scale);
            }
        }

        if (op->memory.offset) {
            if (cnt) {
                if (op->memory.offset < 0) {
                    printf(" - %d", -op->memory.offset);
                } else {
                    printf(" + %d", op->memory.offset);
                }
            } else {
                printf("%d", op->memory.offset);
            }
        }

        printf("]");
        break;
    }
    case AOT_REGISTER:
        if (op->reg == AR_NONE) {
            return 0;
        }
        printf("%s", registers[op->reg]);
        break;
    case AOT_CONSTANT:
        printf("%d", op->value);
        break;
    case AOT_LABEL:
        if (op->label.name != NULL) {
            printf("_%s", op->label.name);
        } else {
            printf("_@%d", op->label.id);
        }
        break;
    }
    return 1;
}

static void print_command(asm_command_t *command)
{
    if (command->type == ASM_TEXT) {
        printf("%s\n", command->text);
        return;
    }

    asm_instruction_info_t *cmd_info = &instructions[command->type];
    int i, op_count = cmd_info->op_count;
    printf("\t%s", cmd_info->name);
    for (i = 0; i < op_count; i++) {
        printf(i == 0 ? " " : ", ");
        print_operand(&command->ops[i]);
    }
    printf("\n");
}

static void add_opcode(asm_opcode_list_t *list, asm_command_t opcode)
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

static void emit(asm_instruction_t cmd, ...)
{
    asm_command_t command;
    command.type = cmd;

    va_list args;
    va_start(args, cmd);
    int i, op_count = instructions[cmd].op_count;
    for (i = 0; i < op_count; i++) {
        command.ops[i] = va_arg(args, asm_operand_t);
    }
    va_end(args);

    add_opcode(&cur_code->opcode_list, command);
}

static void emit_text(char *format, ...)
{
    asm_command_t command;
    command.type = ASM_TEXT;
    command.text = jacc_malloc(256);

    va_list args;
    va_start(args, format);
    vsprintf(command.text, format, args);
    va_end(args);
    add_opcode(&cur_code->opcode_list, command);
}

static void emit_label(label_t label)
{
    emit_text("_@%d:", label.id);
}

static void emit_data(char *data)
{
    asm_command_t command;
    command.type = ASM_TEXT;
    command.text = data;
    add_opcode(&cur_code->data_list, command);
}

extern asm_operand_t constant(int value)
{
    asm_operand_t operand;
    operand.type = AOT_CONSTANT;
    operand.value = value;
    return operand;
}

static asm_operand_t label(label_t label)
{
    asm_operand_t operand;
    operand.type = AOT_LABEL;
    operand.label = label;
    return operand;
}

static asm_operand_t text_label(const char *label)
{
    asm_operand_t operand;
    operand.type = AOT_LABEL;
    operand.label.id = -1;
    operand.label.name = label;
    return operand;
}

static asm_operand_t memory(asm_operand_t base, int offset, asm_operand_t index)
{
    assert(base.type == AOT_REGISTER || base.type == AOT_LABEL);
    assert(index.type == AOT_REGISTER);
    asm_operand_t operand;
    operand.type = AOT_MEMORY;
    operand.memory.base.type = base.type;
    operand.memory.base.reg = base.reg;
    operand.memory.base.label = base.label;
    operand.memory.index.type = index.type;
    operand.memory.index.reg = index.reg;
    operand.memory.index.label = index.label;
    operand.memory.offset = offset;
    operand.memory.scale = 1;
    operand.memory.size = AOS_DWORD;
    return operand;
}

static asm_operand_t deref(asm_operand_t base)
{
    return memory(base, 0, none_reg);
}

static asm_operand_t size_spec(asm_operand_size_t size, asm_operand_t op)
{
    if (op.type == AOT_MEMORY) {
        op.memory.size = size;
    }
    return op;
}

static asm_operand_t dword(asm_operand_t subop)
{
    return size_spec(AOS_DWORD, subop);
}

static asm_operand_t qword(asm_operand_t subop)
{
    return size_spec(AOS_QWORD, subop);
}

static void push_value(asm_operand_t op, struct symbol *type, int ret)
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
    label_t label;
    label.id = ++label_counter;
    label.name = NULL;
    return label;
}

static void generate_expr(struct node *expr, int ret);

static asm_operand_t lvalue(struct node *expr)
{
    switch (expr->type) {
    case NT_VARIABLE:
    {
        struct var_node *var = (struct var_node*)expr;
        switch (var->symbol->type) {
        case ST_PARAMETER:
            return dword(memory(ebp, var->symbol->offset + 8, none_reg));
        case ST_VARIABLE:
            return dword(memory(ebp, var->symbol->offset - var->symbol->size, none_reg));
        case ST_GLOBAL_VARIABLE:
            if (var->symbol->label.id == 0) {
                var->symbol->label = gen_label();
                char *buf = jacc_malloc(30);
                sprintf(buf, "_@%d db %d dup(0)", var->symbol->label.id, var->symbol->size);
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
        return memory(eax, expr->ops[1]->type_sym->offset, none_reg);
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

static void generate_int_cmp(asm_instruction_t cmd)
{
    emit(ASM_XOR, ecx, ecx);
    emit(ASM_CMP, eax, ebx);
    emit(cmd, cl);
    emit(ASM_MOV, eax, ecx);
}

static void generate_int_logical_op(asm_instruction_t cmd)
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
        emit(ASM_MOV, eax, ecx);
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

static void generate_double_cmp(asm_instruction_t cmd)
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
            emit(ASM_MOV, memory(esp, 4, none_reg), ecx);
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
    case NT_RETURN:
    {
        if (is_compatible_types(expr->type_sym, &sym_int)) {
            generate_expr(expr->ops[0], 1);
            emit(ASM_POP, eax);
        } else if (expr->type_sym == &sym_double) {
            generate_expr(expr->ops[0], 1);
            emit(ASM_FLD, qword(deref(esp)));
            emit(ASM_ADD, esp, constant(8));
        }
        emit(ASM_JMP, label(cur_function->return_label));
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

    ptr += sprintf(ptr, "_@%d db ", str_label.id);
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
        asm_operand_t target = text_label(expr->ops[0]->type_sym->name);
        if ((expr->ops[0]->type_sym->flags & SF_EXTERN) == SF_EXTERN) {
            target = deref(target);
        }
        emit(ASM_CALL, target); // @todo function pointers
        emit(ASM_ADD, esp, constant(size));

        if (is_compatible_types(expr->type_sym, &sym_int) || expr->type_sym->type == ST_POINTER) {
            if (ret) emit(ASM_PUSH, eax);
        } else if (expr->type_sym == &sym_double) {
            if (ret) {
                emit(ASM_SUB, esp, constant(8));
                emit(ASM_FSTP, qword(deref(esp)));
            } else {
                emit(ASM_FFREEP, st0);
            }
        }
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
                emit(ASM_FISTTP, dword(memory(esp, 4, none_reg)));
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
            emit(ASM_IMUL, ebx);
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

    cur_function = func;
    func->return_label = gen_label();
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

    emit_label(func->return_label);
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
        print_command(&code->opcode_list.data[i]);
    }

    printf("section '.data' data readable writable\n\n");
    printf("_@main_esp dd ?\n");
    printf("_@stack_corruption_msg db \"Stack corruption\",10,0\n");

    for (i = 0; i < code->data_list.count; i++) {
        print_command(&code->data_list.data[i]);
    }

    printf("\nsection '.idata' data readable import\n");
    printf("library kernel32, 'kernel32.dll', msvcrt, 'msvcrt.dll'\n");
    printf("import kernel32, _ExitProcess, 'ExitProcess'\n");
    printf("import msvcrt, _printf, 'printf'\n");
}

static void free_opcode_list_data(asm_opcode_list_t *list)
{
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
