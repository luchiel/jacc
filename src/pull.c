#include <stdlib.h>
#include "memory.h"
#include "pull.h"

#define pull_BLOCK_CAPACITY 248

struct pull_block {
    void *ptrs[pull_BLOCK_CAPACITY];
    int size;
    struct pull_block *next;
};

struct pull_data {
    struct pull_block *head;
};

extern pull_t pull_create()
{
    struct pull_data *data = jacc_malloc(sizeof(*data));
    data->head = jacc_malloc(sizeof(*data->head));
    data->head->next = NULL;
    data->head->size = 0;
    return (pull_t)data;
}

extern void pull_destroy(pull_t gc)
{
    pull_clear(gc);
    jacc_free(gc->head);
    jacc_free(gc);
}

extern void pull_add(pull_t gc, void *ptr)
{
    struct pull_block *block = gc->head;
    block->ptrs[block->size] = ptr;
    block->size++;
    if (block->size == pull_BLOCK_CAPACITY) {
        block = jacc_malloc(sizeof(*block));
        block->size = 0;
        block->next = gc->head;
        gc->head = block;
    }
}

extern void pull_clear(pull_t gc)
{
    struct pull_block *block, *next_block;
    block = gc->head->next;
    while (block) {
        next_block = block->next;
        jacc_free(block);
        block = next_block;
    }
    gc->head->size = 0;
    gc->head->next = NULL;
}

extern void pull_free_objects(pull_t gc)
{
    struct pull_block *block = gc->head;
    int i;
    while (block) {
        for (i = 0; i < block->size; i++) {
            jacc_free(block->ptrs[i]);
        }
        block = block->next;
    }
    pull_clear(gc);
}
