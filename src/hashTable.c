#include "hashTable.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct HTEntry {
  char *key;
  int value;
  struct HTEntry *next;
} HTEntry;

struct HashTable {
  HTEntry **buckets;
  size_t capacity;
  size_t size;
};

// A simple string hash function (a variation of djb2)
static size_t hash_string(char *key) {
  size_t hash = 5381;

  while (*key != '\0') {
    hash = hash * 33 + (unsigned char)*key;
    key++;
  }

  return hash;
}

static size_t get_index(char *key, size_t capacity) {
  return hash_string(key) % capacity;
}

static char *copy_string(const char *text) {
  size_t length = strlen(text) + 1;
  char *copy = malloc(length);

  if (copy != NULL) {
    strcpy(copy, text);
  }

  return copy;
}

HashTable *ht_create(size_t initial_capacity) {
  HashTable *table;

  if (initial_capacity == 0) {
    initial_capacity = 4;
  }

  table = malloc(sizeof(HashTable));
  if (table == NULL) {
    return NULL;
  }

  table->buckets = calloc(initial_capacity, sizeof(HTEntry *));
  if (table->buckets == NULL) {
    free(table);
    return NULL;
  }

  table->capacity = initial_capacity;
  table->size = 0;

  return table;
}

// Double the bucket array size and rehash all entries
static bool resize_table(HashTable *table) {
  size_t new_capacity;
  HTEntry **new_buckets;
  size_t i;

  new_capacity = table->capacity * 2;

  new_buckets = calloc(new_capacity, sizeof(HTEntry *));
  if (new_buckets == NULL) {
    return false;
  }

  for (i = 0; i < table->capacity; i++) {
    HTEntry *entry = table->buckets[i];

    while (entry != NULL) {
      HTEntry *next = entry->next;
      size_t index = get_index(entry->key, new_capacity);

      entry->next = new_buckets[index];
      new_buckets[index] = entry;
      entry = next;
    }
  }

  free(table->buckets);
  table->buckets = new_buckets;
  table->capacity = new_capacity;

  return true;
}

void ht_destroy(HashTable *table) {
  size_t i;

  if (table == NULL) {
    return;
  }

  for (i = 0; i < table->capacity; i++) {
    HTEntry *entry = table->buckets[i];

    while (entry != NULL) {
      HTEntry *next = entry->next;
      free(entry->key);
      free(entry);
      entry = next;
    }
  }

  free(table->buckets);
  free(table);
}

bool ht_insert(HashTable *table, char *key, int value) {
  size_t index;
  HTEntry *entry;
  char *key_copy;

  if (table == NULL || key == NULL) {
    return false;
  }

  index = get_index(key, table->capacity);
  entry = table->buckets[index];

  // Update the value if the key is already in the table
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      entry->value = value;
      return true;
    }

    entry = entry->next;
  }

  // Resize when the table is 75% full
  if (table->size >= table->capacity * 3 / 4) {
    if (!resize_table(table)) {
      return false;
    }
  }

  key_copy = copy_string(key);
  if (key_copy == NULL) {
    return false;
  }

  entry = malloc(sizeof(HTEntry));
  if (entry == NULL) {
    free(key_copy);
    return false;
  }

  index = get_index(key, table->capacity);
  entry->key = key_copy;
  entry->value = value;
  entry->next = table->buckets[index];
  table->buckets[index] = entry;
  table->size++;

  return true;
}

int ht_get(HashTable *table, char *key) {
  size_t index;
  HTEntry *entry;

  if (table == NULL || key == NULL) {
    return -1;
  }

  index = get_index(key, table->capacity);
  entry = table->buckets[index];

  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return entry->value;
    }
    entry = entry->next;
  }

  return -1;
}

bool ht_remove(HashTable *table, char *key) {
  size_t index;
  HTEntry *entry;
  HTEntry *previous = NULL;

  if (table == NULL || key == NULL) {
    return false;
  }

  index = get_index(key, table->capacity);
  entry = table->buckets[index];

  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      if (previous == NULL) {
        table->buckets[index] = entry->next;
      } else {
        previous->next = entry->next;
      }

      free(entry->key);
      free(entry);
      table->size--;
      return true;
    }

    previous = entry;
    entry = entry->next;
  }

  return false;
}

size_t ht_size(const HashTable *table) {
  if (table == NULL) {
    return 0;
  }

  return table->size;
}

size_t ht_capacity(const HashTable *table) {
  if (table == NULL) {
    return 0;
  }

  return table->capacity;
}
