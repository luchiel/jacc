#ifndef JACC_GC_H
#define JACC_GC_H

typedef struct gc_data *gc_t;

extern gc_t gc_create();
extern void gc_destroy(gc_t gc);

extern void gc_add(gc_t gc, void *ptr);
extern void gc_clear(gc_t gc);
extern void gc_free_objects(gc_t gc);

#endif