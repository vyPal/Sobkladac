#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashTable HashTable;

HashTable *ht_create(size_t initial_capacity);

void ht_destroy(HashTable *table);

bool ht_insert(HashTable *table, char *key, int value);

int ht_get(HashTable *table, char *key); // -1 == key not found

bool ht_remove(HashTable *table, char *key);

size_t ht_size(HashTable *table);

size_t ht_capacity(HashTable *table);

#endif // HASHTABLE_H