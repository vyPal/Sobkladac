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
 *  - `die()` function for killing program with exit code and freeing arena
 */

#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/// Compiler exit codes according to specification
enum {
  ERR_OK = 0,      ///< No error, program ran as intended
  ERR_LEX = 1,     ///< Error during lexical analysis
  ERR_SYNTAX = 2,  ///< Error during syntactic analysis
  /// Error during semantic analysis - undefined symbol or missing `@main`
  ERR_UNDEF = 3,
  /// Error during semantic analysis - redefinition of symbol
  ERR_REDEF = 4,
  /// Error during semantic analysis - incorrect number or type of function arguments
  ERR_ARGS = 5,
  /// Error during semantic analysis - type compatibility or deriving issues
  ERR_TYPE = 6,
  /// Error during semantic analysis - attempting to modify immutable variable
  ERR_CONST = 7,
  /// Error during semantic analysis - other
  ERR_OTHER = 10,
  ERR_INTERNAL = 99,  ///< Internal compiler error
};

/**
 * @brief Terminates program while printing error and freeing arena
 * @param code Exit code according to specification
 * @param line Number of corresponding line in source code
 * @param fmt Format for error message (accepts printf formatting)
 * @param ... Additional arguments to be printed
 *
 * @note If line is `<=0`, no line information is printed
 * @note If code is `ERR_OK`, no error message is printed
 */
void die(int code, unsigned int line, const char *fmt, ...)
    __attribute__((__noreturn__, __format__(__printf__, 3, 4)));

/**
 * @brief Allocates n bytes in the arena allocator
 * @param n Number of bytes to allocate
 * @return Pointer to newly allocated memory
 * @note Function will never return `NULL`, if the allocation fails, the program will exit
 * @note If you plan to reallocate later, keep current size saved somewhere
 * @note Allocated memory is uninitialized
 * @note Allocated memory is 16-byte aligned and is guaranteed to live until cleanup or reallocation
 */
void *arenaAlloc(size_t n);

/**
 * @brief Allocates n bytes of zeroed memory in the arena allocator
 * @param n Number of bytes to allocate
 * @return Pointer to newly allocated memory
 * @note Function will never return `NULL`, if the allocation fails, the program will exit
 * @note If you plan to reallocate later, keep current size saved somewhere
 * @note Allocated memory is 16-byte aligned and is guaranteed to live until cleanup or reallocation
 */
void *arenaZalloc(size_t n);

/**
 * @brief Reallocates pointer from `o` bytes to `n` bytes of memory
 * @param p Existing pointer in arena allocator or `NULL` if new memory should be allocated
 * @param o Old size of allocated memory or `0` if new memory should be allocated
 * @param n New size of allocated memory, can be lower than `o`
 * @return Pointer to newly allocated memory or original pointer `p`
 * @note Function will never return `NULL`, if the allocation fails, the program will exit
 * @note If you plan to reallocate later, keep current size saved somewhere
 * @note Keeps `min(o,n)` bytes, old pointer must not be used afterwards
 * @warning Passing an inaccurate value of `o` may result in undefined behavior
 */
void *arenaRealloc(void *p, size_t o, size_t n);

/**
 * @brief Duplicates a string using the arena allocator for allocating a destination address
 * @param s String to duplicate
 * @param n Length of string not including NUL terminator
 * @return Pointer to copy of string
 * @note Function allocates `n+1` bytes of memory, copies n bytes from `s` and then writes NUL
 * terminator to `n`
 * @note Function doesn't stop on an embedded NUL, the caller must ensure `s` has at least `n`
 * readable bytes
 */
char *arenaStrndup(const char *s, size_t n);

/**
 * @brief Frees all memory held by arena allocator
 * @warning Calling this function makes all previously allocated pointers invalid
 * @note It is recommended to use the `die()` function instead if exiting with non-zero code
 */
void arenaCleanup(void);

#endif  // !COMMON_H
