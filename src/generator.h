#ifndef JACC_GENERATOR_H
#define JACC_GENERATOR_H

#include "symtable.h"

typedef struct code *code_t;

code_t generator_process(symtable_t symtable);
void generator_free_code(code_t code);

#endif
