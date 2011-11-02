#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "memory.h"

typedef char int8_t;
typedef unsigned char uint8_t;
typedef unsigned long uint32_t;

struct hash_node {
	uint32_t hash;
	hash_key_t key;
	hash_value_t value;
	struct hash_node *next;
};

struct hash_data {
	int size;
	struct hash_node **buckets;
	struct hash_data *parent;
};

/*
 * MurmurHash3
*/

static inline uint32_t rotl32(uint32_t x, int8_t r)
{
	return (x << r) | (x >> (32 - r));
}

static uint32_t compute_hash(const void *key)
{
	const uint8_t * data = (const uint8_t*)key;
	int i, len = strlen(key);
	int nblocks = len / 4;

	uint32_t h1 = 0xc98c391f;

	uint32_t c1 = 0xcc9e2d51;
	uint32_t c2 = 0x1b873593;

	const uint32_t * blocks = (const uint32_t *)(data + nblocks * 4);

	for(i = -nblocks; i; i++) {
		uint32_t k1 = blocks[i];

		k1 *= c1;
		k1 = rotl32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = rotl32(h1, 13);
		h1 = h1 * 5 + 0xe6546b64;
	}

	const uint8_t * tail = (const uint8_t*)(data + nblocks * 4);

	uint32_t k1 = 0;

	switch(len & 3) {
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1:
		k1 ^= tail[0];
		h1 ^= rotl32(k1 * c1, 15) * c2;
	};

	h1 ^= len;
	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;

	return h1;
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

static struct hash_node *find_node(hash_t hash, hash_key_t key, struct hash_node **prev)
{
	uint32_t key_hash = compute_hash(key);
	struct hash_node *node = hash->buckets[key_hash % hash->size];

	if (prev != NULL) {
		*prev = NULL;
	}

	while (node != NULL) {
		if (node->hash == key_hash && strcmp(node->key, key) == 0) {
			return node;
		}
		if (prev != NULL) {
			*prev = node;
		}
		node = node->next;
	}

	if (hash->parent != NULL) {
		return find_node(hash->parent, key, prev);
	}

	if (prev != NULL) {
		*prev = NULL;
	}

	return NULL;
}

extern hash_value_t hash_get(hash_t hash, hash_key_t key)
{
	struct hash_node *node = find_node(hash, key, NULL);
	if (node != NULL) {
		return node->value;
	}
	return NULL;
}

extern void hash_set(hash_t hash, hash_key_t key, hash_value_t value)
{
	struct hash_node *node = find_node(hash, key, NULL);
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

extern void hash_delete(hash_t hash, hash_key_t key)
{
	struct hash_node *prev, *node = find_node(hash, key, &prev);
	if (node == NULL) {
		return;
	}

	if (prev == NULL) {
		hash->buckets[node->hash % hash->size] = node->next;
	} else {
		prev->next = node->next;
	}
	free_hash_node(node);
}