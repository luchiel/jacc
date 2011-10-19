#ifndef JACC_HASH_H
#define JACC_HASH_H

typedef struct hash_data *hash_t;
typedef char *hash_key_t;
typedef void *hash_value_t;

extern hash_t hash_create();
extern void hash_destroy(hash_t hash);

extern hash_value_t hash_get(hash_t hash, hash_key_t key);
extern void hash_set(hash_t hash, hash_key_t key, hash_value_t value);
extern void hash_delete(hash_t hash, hash_key_t key);
#endif