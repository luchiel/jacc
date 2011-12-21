#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "symtable.h"
#include "memory.h"

struct symtable_node {
    int hash;
    symtable_key_t key;
    symtable_key2_t key2;
    symtable_value_t value;
    struct symtable_node *next;
};

struct symtable_list_node {
    struct symtable_list_node *next;
    struct symtable_node *node;
};

struct symtable_data {
    int capacity;
    int size;
    struct symtable_node **buckets;
    struct symtable_list_node *list_head;
    struct symtable_list_node *list_tail;
};

static int compute_hash(const char *key)
{
    int hash = 17;
    while (*key != '\0') {
        hash = hash * 31 + *key + CHAR_MIN;
        key++;
    }
    return abs(hash);
}

extern symtable_t symtable_create(int capacity)
{
    symtable_t symtable = jacc_malloc(sizeof(*symtable));
    symtable->capacity = capacity;
    symtable->list_head = NULL;
    symtable->list_tail = NULL;
    symtable->size = 0;
    symtable->buckets = jacc_malloc(capacity * sizeof(*symtable->buckets));
    memset(symtable->buckets, 0, capacity * sizeof(*symtable->buckets));
    return symtable;
}

static void free_symtable_node(struct symtable_node *node)
{
    jacc_free(node);
}

extern void symtable_destroy(symtable_t symtable, int free_nodes)
{
    int i;
    struct symtable_node *node, *prev;

    if (symtable == NULL) {
        return;
    }

    for (i = 0; i < symtable->capacity; i++) {
        node = symtable->buckets[i];
        while (node != NULL) {
            prev = node;
            node = node->next;
            if (free_nodes) {
                free_symtable_node(prev);
            }
        }
    }
    struct symtable_list_node *lprev, *lnode;
    lnode = symtable->list_head;
    while (lnode != NULL) {
        lprev = lnode;
        lnode = lnode->next;
        jacc_free(lprev);
    }
    free(symtable->buckets);
    free(symtable);
}

extern int symtable_size(symtable_t symtable)
{
    return symtable->size;
}

static struct symtable_node *find_node(symtable_t symtable, symtable_key_t key, symtable_key2_t key2)
{
    int key_hash = compute_hash(key);
    struct symtable_node *node = symtable->buckets[key_hash % symtable->capacity];

    while (node != NULL) {
        if (node->hash == key_hash && node->key2 == key2 && strcmp(node->key, key) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

extern symtable_value_t symtable_get(symtable_t symtable, symtable_key_t key, symtable_key2_t key2)
{
    struct symtable_node *node = find_node(symtable, key, key2);
    if (node != NULL) {
        return node->value;
    }
    return NULL;
}

extern void symtable_set(symtable_t symtable, symtable_key_t key, symtable_key2_t key2, symtable_value_t value)
{
    struct symtable_node *node = find_node(symtable, key, key2);
    if (node != NULL) {
        node->value = value;
        return;
    }

    node = jacc_malloc(sizeof(*node));
    node->hash = compute_hash(key);
    node->key = key;
    node->key2 = key2;
    node->value = value;
    node->next = symtable->buckets[node->hash % symtable->capacity];
    symtable->buckets[node->hash % symtable->capacity] = node;

    struct symtable_list_node *list_node = jacc_malloc(sizeof(*list_node));
    list_node->next = NULL;
    list_node->node = node;
    if (symtable->list_head == NULL) {
        symtable->list_head = list_node;
        symtable->list_tail = list_node;
    } else {
        symtable->list_tail->next = list_node;
        symtable->list_tail = list_node;
    }
    symtable->size++;
}

extern symtable_iter_t symtable_first(symtable_t symtable)
{
    if (symtable->list_head == NULL) {
        return NULL;
    }
    return symtable->list_head;
}

extern symtable_iter_t symtable_iter_next(symtable_iter_t iter)
{
    return iter->next;
}

extern symtable_value_t symtable_iter_value(symtable_iter_t iter)
{
    return iter->node->value;
}

extern symtable_key_t symtable_iter_key(symtable_iter_t iter)
{
    return iter->node->key;
}

extern symtable_key2_t symtable_iter_key2(symtable_iter_t iter)
{
    return iter->node->key2;
}