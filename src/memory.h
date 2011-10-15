#ifndef JACC_MEMORY_H
#define JACC_MEMORY_H

#define malloc(size) jacc_check_malloc_result(malloc(size))
#define calloc(num, size) jacc_check_malloc_result(calloc(num, size))
#define realloc(ptr, size) jacc_check_malloc_result(realloc(ptr, size))

extern void *jacc_check_malloc_result(void *ptr);

#endif