#include <stdlib.h>
#include <stdio.h>
#include "memory.h"

inline void *jacc_check_malloc_result(void *ptr)
{
    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

extern void *jacc_malloc(size_t size)
{
    return jacc_check_malloc_result(malloc(size));
}

extern void *jacc_realloc(void *ptr, size_t size)
{
    return jacc_check_malloc_result(realloc(ptr, size));
}

extern void *jacc_calloc(size_t num, size_t size)
{
    return jacc_check_malloc_result(calloc(num, size));
}

extern void jacc_free(void *ptr)
{
    free(ptr);
}
