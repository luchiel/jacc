#ifndef JACC_GENERATOR_H
#define JACC_GENERATOR_H

#include "symtable.h"

enum asm_command_type {
#define COMMAND(name, repr, op_count) ASM_##name,
#include "commands.def"
#undef COMMAND
};

enum asm_register_type {
#define REGISTER(name, varname) ART_##name,
#include "registers.def"
#undef REGISTER
};

struct asm_command {
    const char *name;
    enum asm_command_type type;
    int op_count;
};

enum asm_operand_type {
    AOT_REGISTER,
    AOT_MEMORY,
    AOT_CONSTANT,
    AOT_LABEL,
    AOT_TEXT_LABEL,
};

enum asm_operand_size {
    AOS_BYTE,
    AOS_WORD,
    AOS_DWORD,
    AOS_QWORD,
};

typedef int label_t;

struct asm_operand {
    enum asm_operand_type type;
    union {
        struct {
            struct asm_operand *base, *offset, *index;
            enum asm_operand_size size;
            int scale;
        } memory;
        const char *register_name;
        label_t label;
        const char *text_label;
        int value;
    } data;
};

enum asm_opcode_type {
    ACT_NOP,
    ACT_TEXT,
    ACT_COMMAND,
    ACT_DATA,
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

struct asm_opcode_list {
    struct asm_opcode **data;
    int count;
    int size;
};

typedef struct code {
    struct asm_opcode_list opcode_list;
    struct asm_opcode_list data_list;
} *code_t;

extern void generator_init();
extern void generator_destroy();

extern code_t generator_process(symtable_t symtable);
extern void generator_print_code(code_t code);

extern void generator_free_operand_data(struct asm_operand *operand);
extern void generator_free_opcode_data(struct asm_opcode *opcode);
extern void generator_free_code(code_t code);

extern struct asm_command *generator_get_command(enum asm_command_type type);

extern struct asm_operand *constant(int value);

#endif
