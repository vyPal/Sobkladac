/**
 * @file hashtable.h
 * @brief Hash table implementation for string keys and generic values.
 * @author David Sobek <xsobekd00>
 *
 * Provides a hash table that stores string keys and associated values using separate chaining to
 * handle collisions. The table automatically resizes when it reaches a load factor of 75%.
 * Memory is managed by arena allocator.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashTable HashTable;

/**
 * @brief Creates a new hash table.
 * @param initialCapacity If zero, the default capacity of 256 is used.
 */
HashTable *htCreate(size_t initialCapacity);

/// @brief Inserts a key-value pair into the hash table.
bool htInsert(HashTable *table, const char *key, void *value);

/// @brief Checks whether a key exists in the hash table.
bool htContains(HashTable *table, const char *key);

/// @brief Retrieves the value associated with a key.
void *htGet(HashTable *table, const char *key);

/// @brief Removes a key-value pair from the hash table.
bool htRemove(HashTable *table, const char *key);

/// @brief Returns the number of entries in the hash table.
size_t htSize(const HashTable *table);

/// @brief Returns the current number of buckets in the hash table.
size_t htCapacity(const HashTable *table);

#endif  // HASHTABLE_H
