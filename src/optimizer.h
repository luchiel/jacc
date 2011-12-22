#ifndef JACC_OPTIMIZER_H
#define JACC_OPTIMIZER_H

#include "generator.h"

typedef int (*optimization_delegate_t)(struct asm_opcode **list);
struct optimization_pass
{
    optimization_delegate_t func;
    int frame_size;
};

extern void optimizer_optimize(code_t code);

#endif
