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

/// Definitions for testing arena allocator when using ASan
// AI-BEGIN [claude-opus-5-5, Claude Code]
// Prompt: "how do asan's poisoning macros work?"
// Context: follow-up to an AI review of the arena; sketch adapted by us
#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION(a, s) ((void)(a), (void)(s))
#define ASAN_UNPOISON_MEMORY_REGION(a, s) ((void)(a), (void)(s))
#endif
// AI-END

/**
 * @brief Internal data structure for arena allocator
 */
typedef struct ArenaChunk {
  struct ArenaChunk *next;               ///< Pointer to next chunk
  size_t size;                           ///< Maximum size of this chunk's data in bytes
  size_t used;                           ///< Currently used bytes
  _Alignas(max_align_t) uint8_t data[];  ///< Actual data
} ArenaChunk;

#define ARENA_ALIGN 16
#define ARENA_CHUNK_SIZE 65536

/// Aligns `n` bytes up to `ARENA_ALIGN` and dies if overflow were possible
static size_t alignUp(size_t n) {
  if (n > SIZE_MAX - (ARENA_ALIGN - 1) - sizeof(ArenaChunk))
    die(ERR_INTERNAL, 0, "allocation size overflow");

  return (n + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1);
}

static ArenaChunk *head;

void *arenaAlloc(size_t n) {
  size_t req = n;
  n = alignUp(n);

  if (!head || n > head->size - head->used) {
    int oversize = n > ARENA_CHUNK_SIZE;
    size_t size = oversize ? n : ARENA_CHUNK_SIZE;
    ArenaChunk *c = malloc(sizeof(ArenaChunk) + size);
    if (!c)
      die(ERR_INTERNAL, 0, "out of memory");
    ASAN_POISON_MEMORY_REGION(c->data, size);
    // If chunk is oversized (nothing else will fit in it anyway) insert it into the chunk list
    // without making it the new head so that we don't waste space that could still be available in
    // the current head
    if (oversize && head) {
      c->used = c->size = size;
      c->next = head->next;
      head->next = c;
      ASAN_UNPOISON_MEMORY_REGION(c->data, size);
      return c->data;
    }
    c->next = head;
    c->size = size;
    c->used = 0;
    head = c;
  }

  void *p = head->data + head->used;
  head->used += n;
  ASAN_UNPOISON_MEMORY_REGION(p, req);
  return p;
}

void *arenaZalloc(size_t n) {
  void *p = arenaAlloc(n);
  memset(p, 0, n);
  return p;
}

void *arenaRealloc(void *p, size_t o, size_t n) {
  if (p == NULL)
    return arenaAlloc(n);

  size_t ro = alignUp(o);
  size_t no = alignUp(n);

  ArenaChunk *prev = NULL;
  // Check other chunks for the base rather than just the head
  for (ArenaChunk *cHead = head; cHead != NULL; cHead = cHead->next) {
    // If p is equal to the last pointer given out in this chunk
    if (cHead->data + cHead->used == (uint8_t *)p + ro) {
      // If p is also equal to the first pointer given out and the whole chunk is used
      //
      // If p is both the first and last pointer given out in this region and it occupies the full
      // size which is larger than or equal to the chunk prealloc size, and the new size is larger
      // than the old size, then this is a fully size or oversized chunk and we can realloc
      if ((uint8_t *)p == cHead->data && cHead->used == cHead->size &&
          cHead->size >= ARENA_CHUNK_SIZE && no > ro) {
        ArenaChunk *new = realloc(cHead, sizeof(ArenaChunk) + no);
        if (new == NULL)
          die(ERR_INTERNAL, 0, "out of memory");
        if (prev != NULL)
          prev->next = new;
        else
          head = new;

        ASAN_POISON_MEMORY_REGION(p, o);

        new->size = no;
        new->used = no;
        return new->data;
      }
      // If the requested size will fit in the chunk
      if (no <= cHead->size - (cHead->used - ro)) {
        if (n > o)
          ASAN_UNPOISON_MEMORY_REGION(p, n);
        else if (n < o)
          ASAN_POISON_MEMORY_REGION((uint8_t *)p + n, o - n);
        cHead->used += no - ro;
        return p;
      }
    }
    prev = cHead;
  }
  if (n < o) {
    ASAN_POISON_MEMORY_REGION((uint8_t *)p + n, o - n);
    return p;
  }

  void *new = arenaAlloc(n);
  memcpy(new, p, o < n ? o : n);
  ASAN_POISON_MEMORY_REGION(p, o);
  return new;
}

char *arenaStrndup(const char *s, size_t n) {
  char *p = arenaAlloc(n + 1);
  memcpy(p, s, n);
  p[n] = '\0';
  return p;
}

void arenaCleanup(void) {
  ArenaChunk *current = head;
  head = NULL;
  while (current != NULL) {
    ArenaChunk *next = current->next;
    free(current);
    current = next;
  }
}

void die(int code, unsigned int line, const char *fmt, ...) {
  if (code == ERR_OK) {
    arenaCleanup();
    exit(code);
  }
  va_list ap;
  if (line > 0)
    fprintf(stderr, "error (line %u): ", line);
  else
    fprintf(stderr, "error: ");

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);

  arenaCleanup();
  exit(code);
}
