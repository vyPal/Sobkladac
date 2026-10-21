/**
 * @file unit.c
 * Project: IFJ26 compiler
 * @brief Simple test runner
 * @author Jakub Palacky <xpalacj00>
 *
 * Collection of basic unit tests
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "tst.h"

tstsuite("Arena allocator", ) {
  tstcase("Basic allocation", ) {
    void *p1 = arenaAlloc(10);
    tstcheck(p1 != NULL, "arenaAlloc should return pointer");

    tstcheck(((uintptr_t)p1 % 16) == 0, "Allocated memory must be 16-byte aligned");

    void *p2 = arenaAlloc(100);
    tstcheck(p2 != NULL, "Second allocation should return a non-NULL pointer");
    tstcheck(p1 != p2, "Successive allocations should return distinct pointers");

    arenaCleanup();
  }

  tstcase("Zero allocation", ) {
    size_t size = 30;
    char *p = arenaZalloc(size);
    tstcheck(p != NULL, "arenaZalloc must return pointer");

    tstcheck(((uintptr_t)p % 16) == 0, "Allocated memory must be 16-byte aligned");

    int allZero = 1;
    for (size_t i = 0; i < size; i++) {
      if (p[i] != '\0') {
        allZero = 0;
        break;
      }
    }

    tstcheck(allZero == 1, "Memory allocated by arenaZalloc must be zeroed");

    arenaCleanup();
  }

  tstcase("Memory reallocation", ) {
    char *p1 = arenaAlloc(16);
    strcpy(p1, "Hello");

    char *p2 = arenaRealloc(p1, 16, 32);
    tstcheck(p2 != NULL, "Reallocation to larger size should succeed");
    tstcheck(strcmp(p2, "Hello") == 0, "Data must be preserved when expanding memory");

    tstcheck(((uintptr_t)p2 % 16) == 0, "Allocated memory must be 16-byte aligned");

    char *p3 = arenaRealloc(p2, 32, 8);
    tstcheck(p3 != NULL, "Reallocation to smaller size should succeed");
    tstcheck(strncmp(p3, "Hello", 5) == 0, "Data must be preserved when shrinking memory");

    char *p4 = arenaRealloc(NULL, 0, 16);
    tstcheck(p4 != NULL, "Passing NULL to arenaRealloc should act as new allocation");

    arenaCleanup();
  }

  tstcase("String duplication", ) {
    const char *orig = "compiler_test_string";

    char *dup = arenaStrndup(orig, 8);
    tstcheck(dup != NULL, "arena_strndup should return non-NULL pointer");

    tstcheck(((uintptr_t)dup % 16) == 0, "Allocated memory must be 16-byte aligned");

    tstcheck(strcmp(dup, "compiler") == 0,
             "String content and NUL terminator must match the specified length");

    arenaCleanup();
  }
}
