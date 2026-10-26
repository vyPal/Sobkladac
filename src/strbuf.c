/**
 * @file strbuf.c
 * Project: IFJ26 compiler
 * @brief StringBuilder implementation
 * @author Jakub Palacky <xpalacj00>
 *
 * Implementation of the simple string builder
 */

#include "strbuf.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

StringBuilder sbNew(size_t cap) {
  StringBuilder sb = {.capacity = cap == 0 ? 64 : cap, .len = 0};
  sb.str = arenaAlloc(sb.capacity);
  return sb;
}

static void sbReserve(StringBuilder *sb, size_t extra) {
  size_t newCap = sb->capacity;
  if (newCap == 0)
    newCap = 64;
  while (sb->len + extra >= newCap) {
    newCap *= 2;
  }
  if (newCap != sb->capacity) {
    sb->str = arenaRealloc(sb->str, sb->capacity, newCap);
    sb->capacity = newCap;
  }
}

void sbAppend(StringBuilder *sb, const char *src) {
  size_t len = strlen(src);
  sbReserve(sb, len);
  memcpy(sb->str + sb->len, src, len);
  sb->len += len;
}

void sbAppendChar(StringBuilder *sb, char c) {
  sbReserve(sb, 1);
  sb->str[sb->len++] = c;
}

void sbAppendf(StringBuilder *sb, const char *fmt, ...) {
  va_list ap, ap2;
  va_start(ap, fmt);
  va_copy(ap2, ap);
  int len = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (len < 0)
    die(ERR_INTERNAL, 0, "sbAppendf: bad format");
  size_t n = (size_t)len;
  sbReserve(sb, n);

  vsnprintf(sb->str + sb->len, sb->capacity - sb->len, fmt, ap2);
  va_end(ap2);
  sb->len += n;
}

char *sbBuild(StringBuilder *sb) {
  sb->str[sb->len] = '\0';
  return sb->str;
}
