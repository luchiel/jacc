#ifndef JACC_SYMTABLE_H
#define JACC_SYMTABLE_H

#define SF_VARIADIC 1
#define SF_INCOMPLETE 2
#define SF_STATIC 4
#define SF_EXTERN 8

enum symbol_type {
    ST_VARIABLE,
    ST_GLOBAL_VARIABLE,
    ST_FIELD,
    ST_FUNCTION,
    ST_SCALAR_TYPE,
    ST_TYPE_ALIAS,
    ST_STRUCT,
    ST_UNION,
    ST_ENUM,
    ST_ENUM_CONST,
    ST_ARRAY,
    ST_POINTER,
    ST_PARAMETER,
};

enum symbol_class {
    SC_NAME,
    SC_TAG,
    SC_LABEL,
};

typedef struct symtable_data *symtable_t;

typedef struct {
    int id;
    const char *name;
} label_t;

struct symbol {
    enum symbol_type type;
    const char *name;
    struct symbol *base_type;
    struct node *expr;
    symtable_t symtable;
    int flags;
    int size;
    int offset;
    int locals_size;
    label_t label;
    label_t return_label;
};

typedef const char *symtable_key_t;
typedef enum symbol_class symtable_key2_t;
typedef struct symbol *symtable_value_t;
typedef struct symtable_list_node *symtable_iter_t;

extern symtable_t symtable_create(int capacity);
extern void symtable_destroy(symtable_t symtable, int free_nodes);

extern int symtable_size(symtable_t symtable);

extern symtable_value_t symtable_get(symtable_t symtable, symtable_key_t key, symtable_key2_t key2);
extern void symtable_set(symtable_t symtable, symtable_key_t key, symtable_key2_t key2, symtable_value_t value);

extern symtable_iter_t symtable_first(symtable_t symtable);
extern symtable_iter_t symtable_iter_next(symtable_iter_t iter);
extern symtable_value_t symtable_iter_value(symtable_iter_t iter);
extern symtable_key_t symtable_iter_key(symtable_iter_t iter);
extern symtable_key2_t symtable_iter_key2(symtable_iter_t iter);
#endif
