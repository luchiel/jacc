#ifndef JACC_MEMORY_H
#define JACC_MEMORY_H

extern void *jacc_malloc(size_t size);
extern void *jacc_realloc(void *ptr, size_t size);
extern void *jacc_calloc(size_t num, size_t size);
extern void jacc_free(void *ptr);

#endif