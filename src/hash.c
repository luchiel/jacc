#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "hash.h"
#include "memory.h"

struct hash_node {
	int hash;
	hash_key_t key;
	hash_value_t value;
	struct hash_node *next;
};

struct hash_data {
	int size;
	struct hash_node **buckets;
	struct hash_data *parent;
};

static int compute_hash(const char *key)
{
	int hash = 17;
	while (*key != '\0') {
		hash = hash * 31 + *key + CHAR_MIN;
		key++;
	}
	return hash;
}

extern hash_t hash_create(int size, hash_t parent)
{
	hash_t hash = jacc_malloc(sizeof(*hash));
	hash->size = size;
	hash->parent = parent;
	hash->buckets = jacc_malloc(size * sizeof(*hash->buckets));
	memset(hash->buckets, 0, size * sizeof(*hash->buckets));
	return hash;
}

static void free_hash_node(struct hash_node *node)
{
	jacc_free(node);
}

extern void hash_destroy(hash_t hash)
{
	int i;
	struct hash_node *node, *prev;
	for (i = 0; i < hash->size; i++) {
		node = hash->buckets[i];
		while (node != NULL) {
			prev = node;
			node = node->next;
			free_hash_node(prev);
		}
	}
	free(hash->buckets);
	free(hash);
}

static struct hash_node *find_node(hash_t hash, hash_key_t key)
{
	int key_hash = compute_hash(key);
	struct hash_node *node = hash->buckets[key_hash % hash->size];

	while (node != NULL) {
		if (node->hash == key_hash && strcmp(node->key, key) == 0) {
			return node;
		}
		node = node->next;
	}

	if (hash->parent != NULL) {
		return find_node(hash->parent, key);
	}

	return NULL;
}

extern hash_value_t hash_get(hash_t hash, hash_key_t key)
{
	struct hash_node *node = find_node(hash, key);
	if (node != NULL) {
		return node->value;
	}
	return NULL;
}

extern void hash_set(hash_t hash, hash_key_t key, hash_value_t value)
{
	struct hash_node *node = find_node(hash, key);
	if (node != NULL) {
		node->value = value;
		return;
	}
	node = jacc_malloc(sizeof(*node));
	node->hash = compute_hash(key);
	node->key = key;
	node->value = value;
	node->next = hash->buckets[node->hash % hash->size];
	hash->buckets[node->hash % hash->size] = node;
}
