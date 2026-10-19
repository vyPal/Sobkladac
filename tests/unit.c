#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "tst.h"

tstsuite("Arena allocator", ) {
  tstcase("Basic allocation", ) {
    void *p1 = arena_alloc(10);
    tstcheck(p1 != NULL, "arena_alloc should return pointer");

    tstcheck(((uintptr_t)p1 % 16) == 0, "Allocated memory must be 16-byte aligned");

    void *p2 = arena_alloc(100);
    tstcheck(p2 != NULL, "Second allocation should return a non-NULL pointer");
    tstcheck(p1 != p2, "Successive allocations should return distinct pointers");

    arena_cleanup();
  }

  tstcase("Zero allocation", ) {
    size_t size = 30;
    char *p = arena_zalloc(size);
    tstcheck(p != NULL, "arena_zalloc must return pointer");

    tstcheck(((uintptr_t)p % 16) == 0, "Allocated memory must be 16-byte aligned");

    int all_zero = 1;
    for (size_t i = 0; i < size; i++) {
      if (p[i] != '\0') {
        all_zero = 0;
        break;
      }
    }

    tstcheck(all_zero == 1, "Memory allocated by arena_zalloc must be zeroed");

    arena_cleanup();
  }

  tstcase("Memory reallocation", ) {
    char *p1 = arena_alloc(16);
    strcpy(p1, "Hello");

    char *p2 = arena_realloc(p1, 16, 32);
    tstcheck(p2 != NULL, "Reallocation to larger size should succeed");
    tstcheck(strcmp(p2, "Hello") == 0, "Data must be preserved when expanding memory");

    tstcheck(((uintptr_t)p2 % 16) == 0, "Allocated memory must be 16-byte aligned");

    char *p3 = arena_realloc(p2, 32, 8);
    tstcheck(p3 != NULL, "Reallocation to smaller size should succeed");
    tstcheck(strncmp(p3, "Hello", 5) == 0, "Data must be preserved when shrinking memory");

    char *p4 = arena_realloc(NULL, 0, 16);
    tstcheck(p4 != NULL, "Passing NULL to arena_realloc should act as new allocation");

    arena_cleanup();
  }

  tstcase("String duplication", ) {
    const char *orig = "compiler_test_string";

    char *dup = arena_strndup(orig, 8);
    tstcheck(dup != NULL, "arena_strndup should return non-NULL pointer");

    tstcheck(((uintptr_t)dup % 16) == 0, "Allocated memory must be 16-byte aligned");

    tstcheck(strcmp(dup, "compiler") == 0,
             "String content and NUL terminator must match the specified length");

    arena_cleanup();
  }
}
