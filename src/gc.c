#include <stdlib.h>
#include "memory.h"
#include "gc.h"

#define GC_BLOCK_CAPACITY 248

struct gc_block {
	void *ptrs[GC_BLOCK_CAPACITY];
	int size;
	struct gc_block *next;
};

struct gc_data {
	struct gc_block *head;
};

extern gc_t gc_create()
{
	struct gc_data *data = jacc_malloc(sizeof(*data));
	data->head = jacc_malloc(sizeof(*data->head));
	data->head->next = NULL;
	data->head->size = 0;
	return (gc_t)data;
}

extern void gc_destroy(gc_t gc)
{
	gc_clear(gc);
	jacc_free(gc->head);
	jacc_free(gc);
}

extern void gc_add(gc_t gc, void *ptr)
{
	struct gc_block *block = gc->head;
	block->ptrs[block->size] = ptr;
	block->size++;
	if (block->size == GC_BLOCK_CAPACITY) {
		block = jacc_malloc(sizeof(*block));
		block->size = 0;
		block->next = gc->head;
		gc->head = block;
	}
}

extern void gc_clear(gc_t gc)
{
	struct gc_block *block, *next_block;
	block = gc->head->next;
	while (block) {
		next_block = block->next;
		jacc_free(block);
		block = next_block;
	}
	gc->head->size = 0;
	gc->head->next = NULL;
}

extern void gc_free_objects(gc_t gc)
{
	struct gc_block *block = gc->head;
	int i;
	while (block) {
		for (i = 0; i < block->size; i++) {
			jacc_free(block->ptrs[i]);
		}
		block = block->next;
	}
	gc_clear(gc);
}