#ifndef JACC_GENERATOR_H
#define JACC_GENERATOR_H

#include "symtable.h"

typedef enum {
#define COMMAND(name, repr, op_count) ASM_##name,
#include "commands.def"
#undef COMMAND
} asm_instruction_t;

typedef enum {
#define REGISTER(name, varname) AR_##name,
#include "registers.def"
#undef REGISTER
} asm_register_t;

typedef struct {
    asm_instruction_t type;
    const char *name;
    int op_count;
} asm_instruction_info_t;

typedef const char *asm_register_info_t;

typedef enum {
    AOT_REGISTER,
    AOT_MEMORY,
    AOT_CONSTANT,
    AOT_LABEL,
} asm_operand_type_t;

typedef enum {
    AOS_BYTE,
    AOS_WORD,
    AOS_DWORD,
    AOS_QWORD,
} asm_operand_size_t;

typedef struct {
    asm_operand_type_t type;
    asm_register_t reg;
    label_t label;
} asm_reg_or_label_operand_t;

typedef struct {
    asm_operand_type_t type;
    asm_register_t reg;
    label_t label;
    struct {
        asm_reg_or_label_operand_t base;
        asm_reg_or_label_operand_t index;
        asm_operand_size_t size;
        int offset, scale;
    } memory;
    int value;
} asm_operand_t;

typedef struct {
    asm_instruction_t type;
    asm_operand_t ops[2];
    char *text;
} asm_command_t;

typedef struct {
    asm_command_t *data;
    int count;
    int size;
} asm_opcode_list_t;

typedef struct code {
    asm_opcode_list_t opcode_list;
    asm_opcode_list_t data_list;
} *code_t;

extern void generator_init();
extern void generator_destroy();

extern code_t generator_process(symtable_t symtable);
extern void generator_print_code(code_t code);

extern void generator_free_code(code_t code);

//extern struct asm_operand *constant(int value);

#endif
