/**
 * @file strbuf.h
 * Project: IFJ26 compiler
 * @brief StringBuilder definitions
 * @author Jakub Palacky <xpalacj00>
 *
 * A simple string builder definition
 */

#ifndef STRBUF_H
#define STRBUF_H

#include <stddef.h>

/// Internal StringBuilder definition
typedef struct {
  char *str;
  size_t capacity;
  size_t len;
} StringBuilder;

/**
 * @brief Creates a new string builder and pre-allocates a capacity
 * @param cap Capacity to pre-allocate
 * @return New string builder instance
 * @note If cap is 0, the string builder will allocate a size of 64 chars
 */
StringBuilder sbNew(size_t cap);

/**
 * @brief Appends a string to the string builder
 * @param sb Reference to string builder
 * @param src Reference to other string
 */
void sbAppend(StringBuilder *sb, const char *src);

/**
 * @brief Appends a single character to the string builder
 * @param sb Reference to the string builder
 * @param c Character to append
 */
void sbAppendChar(StringBuilder *sb, char c);

/**
 * @brief Appends a printf-style stirng to the string builder
 * @param sb Reference to the string builder
 * @param fmt printf-style format string
 * @param ... Ohter arguments for printf
 * Uses vsnprintf under the hood
 */
void sbAppendf(StringBuilder *sb, const char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));

/**
 * @brief Builds the string and returns it
 * @param sb Reference to the string builder
 * @return Reference to built string
 * @note This method ensures the string will be NUL-terminated
 * @note String is allocated in the arena so lifetime is guaranteed
 * @warning The string builder should not be used after building
 */
char *sbBuild(StringBuilder *sb);

#endif  // !STRBUF_H
