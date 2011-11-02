#ifndef JACC_PULL_H
#define JACC_PULL_H

typedef struct pull_data *pull_t;

extern pull_t pull_create();
extern void pull_destroy(pull_t gc);

extern void pull_add(pull_t gc, void *ptr);
extern void pull_clear(pull_t gc);
extern void pull_free_objects(pull_t gc);

#endif
