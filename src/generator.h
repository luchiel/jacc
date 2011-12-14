#ifndef JACC_GENERATOR_H
#define JACC_GENERATOR_H

#include "symtable.h"

enum asm_command_type {
#define COMMAND(name, repr, op_count) ASM_##name,
#include "commands.def"
#undef COMMAND
};

enum asm_register_type {
#define REGISTER(name, repr) ART_##name,
#include "registers.def"
#undef REGISTER
};

struct asm_command {
    const char *name;
    int op_count;
};

enum asm_operand_type {
    AOT_REGISTER,
    AOT_MEMORY,
    AOT_CONSTANT,
    AOT_LABEL,
};

struct asm_operand {
    enum asm_operand_type type;
    union {
        struct {
            struct asm_operand *base, *offset, *index;
            int scale;
        } memory;
        const char *register_name;
        const char *label;
        int value;
    } data;
};

enum asm_opcode_type {
    ACT_NOP,
    ACT_TEXT,
    ACT_COMMAND,
};

struct asm_opcode {
    enum asm_opcode_type type;
    union {
        char *text;
        struct {
            struct asm_command *cmd;
            struct asm_operand *ops[3];
        } command;
    } data;
};

typedef struct code {
    struct asm_opcode **opcodes;
    int opcode_count;
    int opcode_list_size;
} *code_t;

extern void generator_init();
extern void generator_destroy();

extern code_t generator_process(symtable_t symtable);
extern void generator_print_code(code_t code);

extern void generator_free_operand_data(struct asm_operand *operand);
extern void generator_free_opcode_data(struct asm_opcode *opcode);
extern void generator_free_code(code_t code);

#endif
