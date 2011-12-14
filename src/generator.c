#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "generator.h"
#include "memory.h"
#include "symtable.h"

struct asm_command commands[] = {
#define COMMAND(name, repr, op_count) { #repr, op_count },
#include "commands.def"
#undef COMMAND
};

struct asm_operand registers[] = {
#define REGISTER(name, repr) { AOT_REGISTER, {}},
#include "registers.def"
#undef REGISTER
};

#define REGISTER(name, repr) struct asm_operand * repr = & registers[ART_##name];
#include "registers.def"
#undef REGISTER

code_t cur_code;

static int print_operand(struct asm_operand *op)
{
    if (op == NULL) {
        return 0;
    }

    switch (op->type) {
    case AOT_MEMORY:
    {
        int cnt = 0;
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
            if (cnt) printf(" + ");
            cnt += print_operand(op->data.memory.offset);
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
        printf("%s", op->data.label);
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
        break;
    case ACT_TEXT:
        printf("%s\n", opcode->data.text);
        break;
    }
}

static void add_opcode(struct asm_opcode *opcode)
{
    int new_size = 0;
    if (cur_code->opcode_count == 0) {
        new_size = 64;
    } else if (cur_code->opcode_count + 1 > cur_code->opcode_list_size) {
        new_size = cur_code->opcode_list_size * 2;
    }

    if (new_size) {
        cur_code->opcode_list_size = new_size;
        cur_code->opcodes = jacc_realloc(cur_code->opcodes, new_size * sizeof(*cur_code->opcodes));
    }
    cur_code->opcodes[cur_code->opcode_count] = opcode;
    cur_code->opcode_count++;
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

    add_opcode(opcode);
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
    add_opcode(opcode);
}

static struct asm_operand *constant(int value)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_CONSTANT;
    operand->data.value = value;
    return operand;
}

static struct asm_operand *label(const char *label)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_LABEL;
    operand->data.label = label;
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
    return operand;
}

static void generate_expr(struct node *expr)
{

}

static void generate_function(struct symbol *func)
{
    if ((func->flags & SF_EXTERN) == SF_EXTERN) {
        emit_text("extrn _%s", func->name);
        return;
    }

    int is_main = strcmp(func->name, "main") == 0;

    emit_text("; start %s", func->name);
    if ((func->flags & SF_STATIC) != SF_STATIC) {
        emit_text("public _%s", func->name);
        if (is_main) {
            emit_text("public _main as 'main'");
        }
    }

    emit_text("_%s:", func->name);
    if (!is_main) {
        emit(ASM_PUSH, ebp);
        emit(ASM_MOV, ebp, esp);
    }

    generate_expr(func->expr);

    if (!is_main) {
        emit(ASM_MOV, esp, ebp);
        emit(ASM_POP, ebp);
        emit(ASM_RET);
    } else {
        emit(ASM_PUSH, constant(0));
        emit_text("\textrn exit");
        emit(ASM_CALL, label("exit"));
    }

    emit_text("; end %s\n", func->name);
}

extern void generator_init()
{
#define REGISTER(name, repr) registers[ART_##name].data.register_name = #repr;
#include "registers.def"
#undef REGISTER
}

extern void generator_destroy()
{

}

extern code_t generator_process(symtable_t symtable)
{
    code_t code = jacc_malloc(sizeof(*code));
    code->opcode_count = 0;
    code->opcode_list_size = 0;
    code->opcodes = NULL;

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
    printf("format ELF\n\n");
    for (i = 0; i < code->opcode_count; i++) {
        print_opcode(code->opcodes[i]);
    }
}

static void free_memory_subop(struct asm_operand *operand)
{
    if (operand != NULL && operand->type == AOT_CONSTANT) {
        jacc_free(operand);
    }
}

extern void generator_free_operand_data(struct asm_operand *operand)
{
    if (operand->type == AOT_MEMORY) {
        free_memory_subop(operand->data.memory.base);
        free_memory_subop(operand->data.memory.index);
        free_memory_subop(operand->data.memory.offset);
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
        jacc_free(opcode->data.text);
        break;
    }
}

extern void generator_free_code(code_t code)
{
    if (code == NULL) {
        return;
    }

    int i;
    for (i = 0; i < code->opcode_count; i++) {
        generator_free_opcode_data(code->opcodes[i]);
        jacc_free(code->opcodes[i]);
    }
    jacc_free(code->opcodes);
    jacc_free(code);
}