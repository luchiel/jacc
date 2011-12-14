#include <stdlib.h>
#include "generator.h"

code_t generator_process(symtable_t symtable)
{
    return NULL;
}

void generator_free_code(code_t code)
{
    if (code == NULL) {
        return;
    }
    free(code);
}
