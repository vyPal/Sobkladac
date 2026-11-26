/**
 * @file hashtable.c
 * @brief Hash table implementation for string keys and generic values.
 * @author David Sobek <xsobekd00>
 *
 * Hash table implementation using the arena allocator.
 */

#include "hashtable.h"

#include <string.h>

#include "common.h"

typedef struct HTEntry {
  char *key;
  void *value;
  struct HTEntry *next;
} HTEntry;

struct HashTable {
  HTEntry **buckets;
  size_t capacity;
  size_t size;
};

/// @brief A simple string hash function (a variation of djb2)
static size_t hashString(const char *key) {
  size_t hash = 5381;
  while (*key != '\0') {
    hash = hash * 33 + (unsigned char)*key;
    key++;
  }
  return hash;
}

static size_t getIndex(const char *key, size_t capacity) {
  return hashString(key) % capacity;
}

HashTable *htCreate(size_t initialCapacity) {
  if (initialCapacity == 0) {
    initialCapacity = 256;  // default capacity
  }
  HashTable *table = arenaAlloc(sizeof(HashTable));
  table->buckets = (HTEntry **)arenaZalloc(initialCapacity * sizeof(HTEntry *));
  table->capacity = initialCapacity;
  table->size = 0;
  return table;
}

/// @brief Double the bucket array size and rehash all entries
static bool resizeTable(HashTable *table) {
  size_t newCapacity = table->capacity * 2;
  HTEntry **newBuckets = (HTEntry **)arenaZalloc(newCapacity * sizeof(HTEntry *));
  for (size_t i = 0; i < table->capacity; i++) {
    HTEntry *entry = table->buckets[i];
    while (entry != NULL) {
      HTEntry *nextEntry = entry->next;
      size_t index = getIndex(entry->key, newCapacity);
      entry->next = newBuckets[index];
      newBuckets[index] = entry;
      entry = nextEntry;
    }
  }
  table->buckets = newBuckets;
  table->capacity = newCapacity;
  return true;
}

bool htInsert(HashTable *table, const char *key, void *value) {
  if (table == NULL || key == NULL) {
    return false;
  }
  size_t index = getIndex(key, table->capacity);
  HTEntry *entry = table->buckets[index];
  // Update the value if the key is already in the table.
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      entry->value = value;
      return true;
    }
    entry = entry->next;
  }
  // Resize when the table is 75% full.
  if (table->size >= table->capacity * 3 / 4) {
    resizeTable(table);
  }
  entry = arenaAlloc(sizeof(HTEntry));
  index = getIndex(key, table->capacity);
  entry->key = arenaStrndup(key, strlen(key));
  entry->value = value;
  entry->next = table->buckets[index];
  table->buckets[index] = entry;
  table->size++;
  return true;
}

bool htContains(HashTable *table, const char *key) {
  if (table == NULL || key == NULL) {
    return false;
  }
  size_t index = getIndex(key, table->capacity);
  const HTEntry *entry = table->buckets[index];
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return true;
    }
    entry = entry->next;
  }
  return false;
}

void *htGet(HashTable *table, const char *key) {
  if (table == NULL || key == NULL) {
    return NULL;
  }
  size_t index = getIndex(key, table->capacity);
  const HTEntry *entry = table->buckets[index];
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return entry->value;
    }
    entry = entry->next;
  }
  return NULL;
}

bool htRemove(HashTable *table, const char *key) {
  if (table == NULL || key == NULL) {
    return false;
  }
  size_t index = getIndex(key, table->capacity);
  HTEntry *entry = table->buckets[index];
  HTEntry *previousEntry = NULL;
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      if (previousEntry == NULL) {
        table->buckets[index] = entry->next;
      } else {
        previousEntry->next = entry->next;
      }
      table->size--;
      return true;
    }
    previousEntry = entry;
    entry = entry->next;
  }
  return false;
}

size_t htSize(const HashTable *table) {
  if (table == NULL) {
    return 0;
  }
  return table->size;
}

size_t htCapacity(const HashTable *table) {
  if (table == NULL) {
    return 0;
  }
  return table->capacity;
}
