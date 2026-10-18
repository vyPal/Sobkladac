/**
 * @file common.h
 * Project: IFJ26 compiler
 * @brief Common utilities
 * @author Jakub Palacky <xpalacj00>
 *
 * Contains common utilities to be used in this project.
 * Current components:
 *  - Arena allocator
 *  - Exit codes according to specification
 *  - `die()` method for killing program with exit code and freeing arena
 */

#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

enum {
  ERR_OK = 0,
  ERR_LEX = 1,
  ERR_SYNTAX = 2,
  ERR_UNDEF = 3,
  ERR_REDEF = 4,
  ERR_ARGS = 5,
  ERR_TYPE = 6,
  ERR_CONST = 7,
  ERR_OTHER = 10,
  ERR_INTERNAL = 99,
};

void die(int code, int line, const char *fmt, ...)
    __attribute__((__noreturn__, __format__(__printf__, 3, 4)));

typedef struct arena_chunk_t {
  struct arena_chunk_t *next;
  size_t size;
  size_t used;
  uint8_t data[];
} arena_chunk_t;

void *arena_alloc(size_t n);
void *arena_zalloc(size_t n);
void *arena_realloc(void *p, size_t o, size_t n);
char *arena_strndup(const char *s, size_t n);
void arena_cleanup(void);

#endif  // !COMMON_H
