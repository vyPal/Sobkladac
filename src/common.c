/**
 * @file common.c
 * Project: IFJ26 compiler
 * @brief Implementation of common utilities
 * @author Jakub Palacky <xpalacj00>
 *
 * Implementation of common utilities from common.h
 */

#include "common.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Internal data structure for arena allocator
 */
typedef struct arena_chunk_t {
  struct arena_chunk_t *next;            ///< Pointer to next chunk
  size_t size;                           ///< Maximum size of this chunk's data in bytes
  size_t used;                           ///< Currently used bytes
  _Alignas(max_align_t) uint8_t data[];  ///< Actual data
} arena_chunk_t;

#define ARENA_ALIGN 16
#define ARENA_CHUNK_SIZE 65536

/// Aligns `n` bytes up to `ARENA_ALIGN` and dies if overflow were possible
static size_t align_up(size_t n) {
  if (n > SIZE_MAX - (ARENA_ALIGN - 1) - sizeof(arena_chunk_t))
    die(ERR_INTERNAL, 0, "allocation size overflow");

  return (n + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1);
}

static arena_chunk_t *head;

void *arena_alloc(size_t n) {
  n = align_up(n);

  if (!head || n > head->size - head->used) {
    int oversize = n > ARENA_CHUNK_SIZE;
    size_t size = oversize ? n : ARENA_CHUNK_SIZE;
    arena_chunk_t *c = malloc(sizeof(arena_chunk_t) + size);
    if (!c)
      die(ERR_INTERNAL, 0, "out of memory");
    // If chunk is oversized (nothing else will fit in it anyway) insert it into the chunk list
    // without making it the new head so that we don't waste space that could still be available in
    // the current head
    if (oversize && head) {
      c->used = c->size = size;
      c->next = head->next;
      head->next = c;
      return c->data;
    }
    c->next = head;
    c->size = size;
    c->used = 0;
    head = c;
  }

  void *p = head->data + head->used;
  head->used += n;
  return p;
}

void *arena_zalloc(size_t n) {
  void *p = arena_alloc(n);
  memset(p, 0, n);
  return p;
}

void *arena_realloc(void *p, size_t o, size_t n) {
  if (p == NULL)
    return arena_alloc(n);

  size_t ro = align_up(o);
  size_t no = align_up(n);

  if (head && head->data + head->used == (uint8_t *)p + ro &&
      no <= head->size - (head->used - ro)) {
    head->used += no - ro;
    return p;
  }
  if (n < o)
    return p;

  void *new = arena_alloc(n);
  memcpy(new, p, o < n ? o : n);
  return new;
}

char *arena_strndup(const char *s, size_t n) {
  char *p = arena_alloc(n + 1);
  memcpy(p, s, n);
  p[n] = '\0';
  return p;
}

void arena_cleanup(void) {
  arena_chunk_t *current = head;
  head = NULL;
  while (current != NULL) {
    arena_chunk_t *next = current->next;
    free(current);
    current = next;
  }
}

void die(int code, int line, const char *fmt, ...) {
  if (code == ERR_OK) {
    arena_cleanup();
    exit(code);
  }
  va_list ap;
  if (line > 0)
    fprintf(stderr, "error (line %d): ", line);
  else
    fprintf(stderr, "error: ");

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);

  arena_cleanup();
  exit(code);
}
