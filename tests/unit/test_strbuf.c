/**
 * @file test_strbuf.c
 * Project: IFJ26 compiler
 * @brief StringBuilder unit tests
 * @author Jakub Palacky <xpalacj00>
 *
 * Tests for the string builder in strbuf.c
 */

#include <string.h>

#include "common.h"
#include "strbuf.h"
#include "tst.h"

tstsuite("StringBuilder", ) {
  tstcase("Empty build", ) {
    StringBuilder sb = sbNew(0);
    tstcheck(sb.capacity == 64, "sbNew(0) should default to a capacity of 64");
    tstcheck(sb.len == 0, "New string builder must be empty");
    tstcheck(strcmp(sbBuild(&sb), "") == 0, "Building an empty builder must result in \"\"");

    arenaCleanup();
  }

  tstcase("Append", ) {
    StringBuilder sb = sbNew(0);
    sbAppend(&sb, "Hello");
    sbAppend(&sb, "");
    sbAppend(&sb, ", world");
    tstcheck(sb.len == 12, "Length must equal the sum of appended lengths");
    tstcheck(strcmp(sbBuild(&sb), "Hello, world") == 0, "Appended strings must be concatenated");

    arenaCleanup();
  }

  tstcase("Append larger than double capacity", ) {
    char big[200];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    StringBuilder sb = sbNew(4);
    sbAppend(&sb, big);
    tstcheck(sb.capacity > sizeof(big) - 1, "Capacity must grow enough to fit the whole string");
    tstcheck(strcmp(sbBuild(&sb), big) == 0, "Large append must be stored intact");

    arenaCleanup();
  }

  tstcase("Exact fill keeps room for NUL", ) {
    StringBuilder sb = sbNew(8);
    sbAppend(&sb, "1234567");
    tstcheck(sb.capacity == 8, "7 chars + NUL fit into capacity 8 without growing");
    sbAppendChar(&sb, '8');
    tstcheck(sb.capacity > 8, "8 chars + NUL must not fit into capacity 8");
    tstcheck(strcmp(sbBuild(&sb), "12345678") == 0, "Content must survive growth");

    arenaCleanup();
  }

  tstcase("Append char across growth", ) {
    StringBuilder sb = sbNew(1);
    for (int i = 0; i < 100; i++) sbAppendChar(&sb, (char)('a' + i % 26));
    tstcheck(sb.len == 100, "Length must equal number of appended chars");

    const char *s = sbBuild(&sb);
    int ok = 1;
    for (int i = 0; i < 100; i++) {
      if (s[i] != (char)('a' + i % 26)) {
        ok = 0;
        break;
      }
    }
    tstcheck(ok == 1, "Characters must be stored in order");
    tstcheck(s[100] == '\0', "Built string must be NUL-terminated");

    arenaCleanup();
  }

  tstcase("Appendf", ) {
    StringBuilder sb = sbNew(0);
    sbAppendf(&sb, "%d-%s", 42, "abc");
    tstcheck(sb.len == 6, "Length must match vsnprintf output length");
    sbAppendf(&sb, "%c", '!');
    tstcheck(strcmp(sbBuild(&sb), "42-abc!") == 0,
             "Consecutive appendf calls must not drop or overwrite characters");

    arenaCleanup();
  }

  tstcase("Appendf with empty output", ) {
    StringBuilder sb = sbNew(0);
    sbAppendf(&sb, "%s", "");
    tstcheck(sb.len == 0, "Empty formatted output must not change length");
    tstcheck(strcmp(sbBuild(&sb), "") == 0, "Empty formatted output must result in \"\"");

    arenaCleanup();
  }

  tstcase("Appendf exact fill and growth", ) {
    StringBuilder sb = sbNew(8);
    sbAppendf(&sb, "%s", "1234567");
    tstcheck(sb.capacity == 8, "7 chars + NUL fit into capacity 8 without growing");
    sbAppendf(&sb, "%d", 8);
    tstcheck(strcmp(sbBuild(&sb), "12345678") == 0,
             "appendf must not truncate when output reaches capacity");

    StringBuilder big = sbNew(2);
    sbAppendf(&big, "%0100d", 7);
    tstcheck(big.len == 100, "Large formatted output must be fully appended");
    tstcheck(sbBuild(&big)[99] == '7', "Last formatted character must be present");

    arenaCleanup();
  }

  tstcase("Mixed appends", ) {
    StringBuilder sb = sbNew(2);
    sbAppend(&sb, "LABEL");
    sbAppendChar(&sb, ' ');
    sbAppendf(&sb, "$%s%d", "loop", 3);
    sbAppendChar(&sb, '\n');
    tstcheck(strcmp(sbBuild(&sb), "LABEL $loop3\n") == 0,
             "Mixed appends must concatenate in order");

    arenaCleanup();
  }
}
