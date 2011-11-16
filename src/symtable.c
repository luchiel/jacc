#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "symtable.h"
#include "memory.h"

struct symtable_node {
    int hash;
    symtable_key_t key;
    symtable_value_t value;
    struct symtable_node *next;
};

struct symtable_list_node {
    struct symtable_list_node *next;
    struct symtable_node *node;
};

struct symtable_data {
    int size;
    struct symtable_node **buckets;
    struct symtable_data *parent;
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
    return hash;
}

extern symtable_t symtable_create(int size, symtable_t parent)
{
    symtable_t symtable = jacc_malloc(sizeof(*symtable));
    symtable->size = size;
    symtable->parent = parent;
    symtable->list_head = NULL;
    symtable->list_tail = NULL;
    symtable->buckets = jacc_malloc(size * sizeof(*symtable->buckets));
    memset(symtable->buckets, 0, size * sizeof(*symtable->buckets));
    return symtable;
}

static void free_symtable_node(struct symtable_node *node)
{
    jacc_free(node);
}

extern void symtable_destroy(symtable_t symtable)
{
    int i;
    struct symtable_node *node, *prev;
    for (i = 0; i < symtable->size; i++) {
        node = symtable->buckets[i];
        while (node != NULL) {
            prev = node;
            node = node->next;
            free_symtable_node(prev);
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

static struct symtable_node *find_node(symtable_t symtable, symtable_key_t key)
{
    int key_hash = compute_hash(key);
    struct symtable_node *node = symtable->buckets[key_hash % symtable->size];

    while (node != NULL) {
        if (node->hash == key_hash && strcmp(node->key, key) == 0) {
            return node;
        }
        node = node->next;
    }

    if (symtable->parent != NULL) {
        return find_node(symtable->parent, key);
    }
    return NULL;
}

extern symtable_value_t symtable_get(symtable_t symtable, symtable_key_t key)
{
    struct symtable_node *node = find_node(symtable, key);
    if (node != NULL) {
        return node->value;
    }
    return NULL;
}

extern void symtable_set(symtable_t symtable, symtable_key_t key, symtable_value_t value)
{
    struct symtable_node *node = find_node(symtable, key);
    if (node != NULL) {
        node->value = value;
        return;
    }

    node = jacc_malloc(sizeof(*node));
    node->hash = compute_hash(key);
    node->key = key;
    node->value = value;
    node->next = symtable->buckets[node->hash % symtable->size];
    symtable->buckets[node->hash % symtable->size] = node;

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