#include <stdlib.h>
#include "optimizer.h"

#define ARRAY_LENGTH(array) (int)(sizeof((array)) / sizeof((array)[0]))

int is_eq(void *operand1, void *operand2)
{
    asm_operand_t *op1 = (asm_operand_t *)operand1;
    asm_operand_t *op2 = (asm_operand_t *)operand2;

    if (op1->type != op2->type) {
        return 0;
    }

    switch (op1->type) {
    case AOT_REGISTER:
        return op1->reg == op2->reg;
    case AOT_CONSTANT:
        return op1->value == op2->value;
    case AOT_MEMORY:
        return is_eq(&op1->memory.base, &op2->memory.base)
            && is_eq(&op1->memory.index, &op2->memory.index)
            && op1->memory.offset == op2->memory.offset
            && op1->memory.scale == op2->memory.scale
            && op1->memory.size == op2->memory.size;
    case AOT_LABEL:
        return op1->label.id == op2->label.id
            && op1->label.name == op2->label.name;
    }
    return 0;
}

int push_pop_opt_possible(asm_command_t *a, asm_command_t *b)
{
    return a->type == ASM_PUSH
        && b->type == ASM_POP
        && ( a->ops[0].type != AOT_MEMORY
          || b->ops[0].type != AOT_MEMORY
           );
}

/*
 * push a
 * push b
 * pop c
 * pop d
 * =>
 * mov c, b
 * mov d, a
 */
int opt_push_pop2(asm_command_t *cmd)
{
    if ( push_pop_opt_possible(&cmd[0], &cmd[3])
      && push_pop_opt_possible(&cmd[1], &cmd[2])
      //&& cmd[0]->ops[0] != cmd[1]->ops[1]
      ) {
        cmd[0].type = ASM_MOV;
        cmd[1].type = ASM_MOV;

        cmd[0].ops[1] = cmd[1].ops[0];
        cmd[1].ops[1] = cmd[0].ops[0];

        cmd[0].ops[0] = cmd[2].ops[0];
        cmd[1].ops[0] = cmd[3].ops[0];
        return 2;
    }
    return -1;
}

/*
 * push a
 * pop b
 * =>
 * mov a, b
 */
int opt_push_pop(asm_command_t *cmd)
{
    if (push_pop_opt_possible(&cmd[0], &cmd[1])) {
        cmd[0].type = ASM_MOV;
        cmd[0].ops[1] = cmd[0].ops[0];
        cmd[0].ops[0] = cmd[1].ops[0];
        return 1;
    }
    return -1;
}

/*
 * mov a, a
 * =>
 * nothing
 */
int opt_mov_self(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_MOV
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[0].ops[1].type == AOT_REGISTER
      && cmd[0].ops[0].reg == cmd[0].ops[1].reg
      ) {
        return 0;
    }
    return -1;
}

/*
 * lea reg1, [reg2 + imm1]
 * lea reg1, [reg1 + imm2]
 * =>
 * lea reg1, [reg2 + imm1 + imm2]
 */
int opt_lea_lea(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_LEA
      && cmd[1].type == ASM_LEA
      && cmd[0].ops[0].reg == cmd[1].ops[0].reg
      && is_eq(&cmd[1].ops[1].memory.base, &cmd[1].ops[1].memory.base)
      ) {
        cmd[0].ops[1].memory.offset += cmd[1].ops[1].memory.offset;
        return 1;
    }
    return -1;
}

/*
 * lea reg, []
 * push [reg]
 */
int opt_lea_push(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_LEA
      && cmd[1].type == ASM_PUSH
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[1].ops[0].type == AOT_MEMORY
      && is_eq(&cmd[0].ops[0], &cmd[1].ops[0].memory.base)
      ) {
        cmd[0].type = ASM_PUSH;
        cmd[0].ops[0] = cmd[0].ops[1];
        return 1;
    }
    return -1;
}

/*
 * lea reg1, []
 * mov reg2, reg1
 * =>
 * lea reg, []
 */

int opt_lea_mov(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_LEA
      && cmd[1].type == ASM_MOV
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[1].ops[0].type == AOT_REGISTER
      && is_eq(&cmd[0].ops[0], &cmd[1].ops[1])
      ) {
        cmd[0].ops[0] = cmd[1].ops[0];
        return 1;
    }
    return -1;
}


/*
 * lea reg, []
 * push [reg]
 * =>
 * push []
*/

int opt_lea_push_ref(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_LEA
      && cmd[1].type == ASM_PUSH
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[0].ops[1].type == AOT_MEMORY
      && cmd[1].ops[0].type == AOT_MEMORY
      && is_eq(&cmd[1].ops[0].memory.base, &cmd[0].ops[0])
      ) {
        cmd[0].type = ASM_PUSH;
        cmd[0].ops[0] = cmd[0].ops[1];
        cmd[0].ops[0].memory.size = cmd[1].ops[0].memory.size;
        cmd[0].ops[0].memory.offset += cmd[1].ops[0].memory.offset;
        return 1;
    }
    return -1;
}

/*
 * lea reg1, dword []
 * mov dword [reg1], reg2/imm
 * =>
 * mov dword [], reg2/imm
 */

int opt_lea_mov_ref(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_LEA
      && cmd[1].type == ASM_MOV
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[0].ops[1].type == AOT_MEMORY
      && cmd[1].ops[0].type == AOT_MEMORY
      && cmd[1].ops[1].type != AOT_MEMORY
      && is_eq(&cmd[1].ops[0].memory.base, &cmd[0].ops[0])
      ) {
        cmd[0].type = ASM_MOV;
        cmd[0].ops[0] = cmd[0].ops[1];
        cmd[0].ops[1] = cmd[1].ops[1];
        cmd[0].ops[0].memory.size = cmd[1].ops[0].memory.size;
        cmd[0].ops[0].memory.offset += cmd[1].ops[0].memory.offset;
        return 1;
    }
    return -1;
}

/*
 * add/sub reg/mem, imm1
 * add/sub reg/mem, imm2
 * =>
 * add/sub reg/mem, imm1+imm2
 */

int cmd_sign(asm_command_t *cmd)
{
    switch (cmd->type) {
    case ASM_ADD: return 1;
    case ASM_SUB: return -1;
    }
    return 0;
}

int opt_add_sub(asm_command_t *cmd)
{
    if ( (cmd[0].type == ASM_ADD || cmd[0].type == ASM_SUB)
      && (cmd[1].type == ASM_ADD || cmd[1].type == ASM_SUB)
      && is_eq(&cmd[0].ops[0], &cmd[1].ops[0])
      && cmd[0].ops[1].type == AOT_CONSTANT
      && cmd[1].ops[1].type == AOT_CONSTANT
      ) {
        int value = 0;
        value += cmd_sign(&cmd[0]) * cmd[0].ops[1].value;
        value += cmd_sign(&cmd[1]) * cmd[1].ops[1].value;
        if (value < 0) {
            cmd[0].type = ASM_SUB;
            cmd[0].ops[1].value = -value;
        } else if (value > 0) {
            cmd[0].type = ASM_ADD;
            cmd[0].ops[1].value = value;
        } else {
            return 0;
        }
        return 1;
    }
    return -1;
}

int opt_mov(asm_command_t *cmd)
{
    if (cmd[0].type != ASM_MOV) {
        return -1;
    }

    /*
     * mov b, a
     * mov c, b
     * =>
     * mov c, a
     */
    if (cmd[1].type == ASM_MOV
      && cmd[0].ops[0].type != AOT_MEMORY
      && is_eq(&cmd[0].ops[0], &cmd[1].ops[1])
      && ( cmd[0].ops[1].type != AOT_MEMORY
        || cmd[1].ops[0].type != AOT_MEMORY
         )
      ) {
        cmd[0].ops[0] = cmd[1].ops[0];
        return 1;
    }

    /*
     * mov reg1, reg2
     * test reg1, reg1
     * =>
     * test reg2, reg2
     */
    if (cmd[1].type == ASM_TEST
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[0].ops[1].type == AOT_REGISTER
      && is_eq(&cmd[0].ops[0], &cmd[1].ops[1])
      && is_eq(&cmd[1].ops[0], &cmd[1].ops[1])
      ) {
        cmd[0].type = ASM_TEST;
        cmd[0].ops[0] = cmd[0].ops[1];
        return 1;
    }

    /*
     * mov reg1, imm
     * imul imm
     * =>
     * imul eax, imm
     */
    if (cmd[1].type == ASM_IMUL
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[0].ops[1].type == AOT_CONSTANT
      && is_eq(&cmd[0].ops[0], &cmd[1].ops[0])
      ) {
        cmd[0].type = ASM_IMUL2;
        cmd[0].ops[0].reg = AR_EAX;
        return 1;
    }
    return -1;
}

int is_power_of_2(int v)
{
    return (v & (v - 1)) == 0;
}

int int_log2(int v)
{
    int r = 0;
    while (v >= 2) {
        v /= 2;
        r++;
    }
    return r;
}

/*
 * imul2 reg, 2**p
 * =>
 * shl reg, p
 */

int opt_imul2(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_IMUL2
      && cmd[0].ops[1].type == AOT_CONSTANT
      && is_power_of_2(cmd[0].ops[1].value)
      ) {
        cmd[0].type = ASM_SHL;
        cmd[0].ops[1].value = int_log2(cmd[0].ops[1].value);
        return 1;
    }
    return -1;
}

/*
 * mov reg, 2**p
 * cdq
 * idiv reg
 * =>
 * sar eax, p
 */

int opt_idiv(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_MOV
      && cmd[1].type == ASM_CDQ
      && cmd[2].type == ASM_IDIV
      && cmd[0].ops[0].type == AOT_REGISTER
      && cmd[0].ops[1].type == AOT_CONSTANT
      && is_power_of_2(cmd[0].ops[1].value)
      && is_eq(&cmd[0].ops[0], &cmd[2].ops[0])
      ) {
        cmd[0].type = ASM_SAR;
        cmd[0].ops[0].reg = AR_EAX;
        cmd[0].ops[1].value = int_log2(cmd[0].ops[1].value);
        return 1;
    }
    return -1;
}

/*
 * fstp reg/mem
 * fld reg/mem
 * =>
 * nothing
 */

int opt_fstp_fld(asm_command_t *cmd)
{
    if ( cmd[0].type == ASM_FSTP
      && cmd[1].type == ASM_FLD
      && is_eq(&cmd[0].ops[0], &cmd[1].ops[0])
      ) {
        return 0;
    }
    return -1;
}

typedef int (*optimization_delegate_t)(asm_command_t *list);
struct optimization_pass
{
    optimization_delegate_t func;
    int frame_size;
};

struct optimization_pass passes[] = {
    { opt_push_pop2, 4 },
    { opt_push_pop, 2 },
    { opt_mov_self, 1 },
    { opt_lea_lea, 2 },
    { opt_lea_mov, 2 },
    { opt_lea_push, 2 },
    { opt_lea_push_ref, 2 },
    { opt_lea_mov_ref, 2 },
    { opt_add_sub, 2 },
    { opt_mov, 2 },
    { opt_imul2, 1 },
    { opt_idiv, 3 },
    { opt_fstp_fld, 2 },
};

void optimizer_optimize(code_t code)
{
    asm_opcode_list_t *list = &code->opcode_list;
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
            if (list->data[i].type == ASM_TEXT) {
                list->data[pos] = list->data[i];
                pos++;
                continue;
            } else if (list->data[i].type == ASM_NOP) {
                continue;
            }

            int j, frame_size = 0;
            for (j = 0; j < max_frame_size && i + j < count; j++) {
                if (list->data[i + j].type == ASM_NOP) {
                    break;
                }
                frame_size++;
            }
            int match = 0;
            for (j = 0; j < ARRAY_LENGTH(passes); j++) {
                if (frame_size < passes[j].frame_size) {
                    continue;
                }
                int result = passes[j].func(&list->data[i]);
                if (result != -1) {
                    if (frame_size > result) {
                        frame_size = result;
                    }
                    match = 1;
                    int k = result;
                    for (; k < passes[j].frame_size; k++) {
                        list->data[i + k].type = ASM_NOP;
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

