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

static arena_chunk_t *head;

void *arena_alloc(size_t n) {
  n = (n + 15) & ~(size_t)15;

  if (!head || head->used + n > head->size) {
    size_t size = n > 65536 ? n : 65536;
    arena_chunk_t *c = malloc(sizeof(arena_chunk_t) + size);
    if (!c)
      die(ERR_INTERNAL, 0, "out of memory");
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

  size_t ro = (o + 15) & ~(size_t)15;
  size_t no = (n + 15) & ~(size_t)15;

  if (head && head->data + head->used == (uint8_t *)p + ro &&
      (uint8_t *)p + no <= head->data + head->size) {
    head->used += no - ro;
    return p;
  }

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
