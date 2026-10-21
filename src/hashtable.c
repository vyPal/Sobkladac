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

static char *copyString(const char *text) {
  return arenaStrndup(text, strlen(text));
}

HashTable *htCreate(size_t initialCapacity) {
  HashTable *table;

  if (initialCapacity == 0) {
    initialCapacity = 256;  // default capacity
  }

  table = arenaAlloc(sizeof(HashTable));

  table->buckets = arenaZalloc(initialCapacity * sizeof(HTEntry *));
  table->capacity = initialCapacity;
  table->size = 0;

  return table;
}

/// @brief Double the bucket array size and rehash all entries
static bool resizeTable(HashTable *table) {
  size_t newCapacity;
  HTEntry **newBuckets;
  size_t i;

  newCapacity = table->capacity * 2;
  newBuckets = arenaZalloc(newCapacity * sizeof(HTEntry *));

  for (i = 0; i < table->capacity; i++) {
    HTEntry *entry = table->buckets[i];

    while (entry != NULL) {
      HTEntry *next = entry->next;
      size_t index = getIndex(entry->key, newCapacity);

      entry->next = newBuckets[index];
      newBuckets[index] = entry;
      entry = next;
    }
  }

  table->buckets = newBuckets;
  table->capacity = newCapacity;

  return true;
}

bool htInsert(HashTable *table, const char *key, void *value) {
  size_t index;
  HTEntry *entry;
  char *keyCopy;

  if (table == NULL || key == NULL) {
    return false;
  }

  index = getIndex(key, table->capacity);
  entry = table->buckets[index];

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

  keyCopy = copyString(key);

  entry = arenaAlloc(sizeof(HTEntry));

  index = getIndex(key, table->capacity);
  entry->key = keyCopy;
  entry->value = value;
  entry->next = table->buckets[index];
  table->buckets[index] = entry;
  table->size++;

  return true;
}

bool htContains(HashTable *table, const char *key) {
  size_t index;
  const HTEntry *entry;

  if (table == NULL || key == NULL) {
    return false;
  }

  index = getIndex(key, table->capacity);
  entry = table->buckets[index];

  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return true;
    }

    entry = entry->next;
  }

  return false;
}

void *htGet(HashTable *table, const char *key) {
  size_t index;
  HTEntry *entry;

  if (table == NULL || key == NULL) {
    return NULL;
  }

  index = getIndex(key, table->capacity);
  entry = table->buckets[index];

  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return entry->value;
    }

    entry = entry->next;
  }

  return NULL;
}

bool htRemove(HashTable *table, const char *key) {
  size_t index;
  HTEntry *entry;
  HTEntry *previous = NULL;

  if (table == NULL || key == NULL) {
    return false;
  }

  index = getIndex(key, table->capacity);
  entry = table->buckets[index];

  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      if (previous == NULL) {
        table->buckets[index] = entry->next;
      } else {
        previous->next = entry->next;
      }

      table->size--;
      return true;
    }

    previous = entry;
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
