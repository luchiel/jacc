#include <stdlib.h>
#include "optimizer.h"

#define ARRAY_LENGTH(array) (int)(sizeof((array)) / sizeof((array)[0]))

void set_cmd_type(struct asm_opcode *op, enum asm_command_type cmd_type)
{
    op->data.command.cmd = generator_get_command(cmd_type);
}

struct asm_operand *get_op(struct asm_opcode *opcode, int pos)
{
    return opcode->data.command.ops[pos];
}

void set_op(struct asm_opcode *opcode, int pos, struct asm_operand *op)
{
    opcode->data.command.ops[pos] = op;
}

void set_nop(struct asm_opcode *opcode)
{
    opcode->type = ACT_NOP;
}

void move_op(struct asm_opcode *opcode1, int pos1, struct asm_opcode *opcode2, int pos2)
{
    set_op(opcode2, pos2, get_op(opcode1, pos1));
}

int is_eq_op(struct asm_operand *op1, struct asm_operand *op2)
{
    if (op1->type != op2->type) {
        return 0;
    }
    if (op1->type == AOT_REGISTER) {
        return op1->data.register_name == op2->data.register_name;
    }
    return 0;
}

int has_const_offset(struct asm_operand *op)
{
    return op->type == AOT_MEMORY
        && (
            op->data.memory.offset == NULL
         || op->data.memory.offset->type == AOT_CONSTANT
        );
}

int get_offset(struct asm_operand *op)
{
    if (op->data.memory.offset == NULL) {
        return 0;
    }
    return op->data.memory.offset->data.value;
}

int match_cmd(struct asm_opcode *op, enum asm_command_type cmd_type)
{
    return op->data.command.cmd->type == cmd_type;
}

int match_op_type(struct asm_opcode *opcode, int pos, enum asm_operand_type operand_type)
{
    return get_op(opcode, pos)->type == operand_type;
}

int push_pop_opt_possible(struct asm_opcode *op0, struct asm_opcode *op1)
{
    return match_cmd(op0, ASM_PUSH)
        && match_cmd(op1, ASM_POP)
        && ( !match_op_type(op0, 0, AOT_MEMORY)
          || !match_op_type(op1, 0, AOT_MEMORY)
           );
}

int opt_push_pop2(struct asm_opcode **list)
{
    if ( push_pop_opt_possible(list[0], list[3])
      && push_pop_opt_possible(list[1], list[2])
      && !is_eq_op(get_op(list[0], 0), get_op(list[1], 0))
      ) {
        set_cmd_type(list[0], ASM_MOV);
        move_op(list[0], 0, list[0], 1);
        move_op(list[3], 0, list[0], 0);

        set_cmd_type(list[1], ASM_MOV);
        move_op(list[1], 0, list[1], 1);
        move_op(list[2], 0, list[1], 0);
        return 2;
    }
    return -1;
}

int opt_push_pop(struct asm_opcode **list)
{
    if (push_pop_opt_possible(list[0], list[1])) {
        set_cmd_type(list[0], ASM_MOV);
        move_op(list[0], 0, list[0], 1);
        move_op(list[1], 0, list[0], 0);
        return 1;
    }
    return -1;
}

int opt_mov_self(struct asm_opcode **list)
{
    if ( match_cmd(list[0], ASM_MOV)
      && match_op_type(list[0], 0, AOT_REGISTER)
      && match_op_type(list[0], 1, AOT_REGISTER)
      && is_eq_op(get_op(list[0], 0), get_op(list[0], 1))
      ) {
        return 0;
    }
    return -1;
}

int opt_lea_lea(struct asm_opcode **list)
{
    if ( match_cmd(list[0], ASM_LEA)
      && match_cmd(list[1], ASM_LEA)
      && has_const_offset(get_op(list[0], 1))
      && has_const_offset(get_op(list[1], 1))
      && get_op(list[0], 1)->data.memory.size == get_op(list[1], 1)->data.memory.size
      && get_op(list[1], 0) == get_op(list[1], 1)->data.memory.base
      ) {
        get_op(list[0], 1)->data.memory.offset = constant(get_offset(get_op(list[0], 1)) + get_offset(get_op(list[1], 1)));
        return 1;
    }
    return -1;
}

int opt_lea_push(struct asm_opcode **list)
{
    if ( match_cmd(list[0], ASM_LEA)
      && match_cmd(list[1], ASM_PUSH)
      && match_op_type(list[0], 0, AOT_REGISTER)
      && match_op_type(list[0], 1, AOT_MEMORY)
      && match_op_type(list[1], 0, AOT_MEMORY)
      && is_eq_op(get_op(list[1], 0)->data.memory.base, get_op(list[0], 0))
      && get_op(list[1], 0)->data.memory.index == NULL
      && has_const_offset(get_op(list[0], 1))
      && has_const_offset(get_op(list[1], 0))
      ) {
        set_cmd_type(list[0], ASM_PUSH);
        move_op(list[0], 1, list[0], 0);
        get_op(list[0], 0)->data.memory.size = get_op(list[1], 0)->data.memory.size;
        get_op(list[0], 0)->data.memory.offset = constant(get_offset(get_op(list[0], 0)) + get_offset(get_op(list[1], 0)));
        return 1;
    }
    return -1;
}

struct optimization_pass passes[] = {
    { opt_push_pop2, 4},
    { opt_push_pop, 2},
    { opt_mov_self, 1},
    { opt_lea_lea, 2},
    { opt_lea_push, 2},
};

void optimizer_optimize(code_t code)
{
    struct asm_opcode_list *list = &code->opcode_list;
    int changed = 1, max_frame_size = 0;

    int i = 0;
    for (; i < ARRAY_LENGTH(passes); i++) {
        if (passes[i].frame_size > max_frame_size) {
            max_frame_size = passes[i].frame_size;
        }
    }

    while (changed) {
        int i = 0, pos = 0, count = list->count;
        changed = 0;
        for (; i < count; i++) {
            if (list->data[i]->type != ACT_COMMAND) {
                if (list->data[i]->type != ACT_NOP) {
                    list->data[pos] = list->data[i];
                    pos++;
                }
                continue;
            }
            int j, frame_size = 0;
            for (j = 0; j < max_frame_size && i + j < count; j++) {
                if (list->data[i + j]->type == ACT_NOP) {
                    break;
                }
                frame_size++;
            }
            int match = 0;
            for (j = 0; j < ARRAY_LENGTH(passes); j++) {
                if (frame_size < passes[j].frame_size) {
                    continue;
                }
                int result = passes[j].func(list->data + i);
                if (result != -1) {
                    if (frame_size > result) {
                        frame_size = result;
                    }
                    match = 1;
                    int k = result;
                    for (; k < passes[j].frame_size; k++) {
                        list->data[i + k]->type = ACT_NOP;
                    }
                }
            }
            if (match) {
                for (j = 0; j < frame_size; j++) {
                    list->data[pos + j] = list->data[i + j];
                }
                pos += frame_size;
                i += frame_size - 1;
                changed = 1;
            } else {
                list->data[pos] = list->data[i];
                pos++;
            }
        }
        count = list->count = pos;
    }
}
