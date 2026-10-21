#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashTable HashTable;

HashTable *htCreate(size_t initialCapacity);

void htDestroy(HashTable *table);

bool htInsert(HashTable *table, char *key, void *value);

void *htGet(HashTable *table, char *key);  // NULL == key not found

bool htRemove(HashTable *table, char *key);

size_t htSize(const HashTable *table);

size_t htCapacity(const HashTable *table);

#endif  // HASHTABLE_H
