# Sobkladač - Řešení projektu IFJ2026 týmu Sobek & Co

## Project documentation

The most important notes and documentation for this project will live here

### Format for AI-generated or AI-assisted code

Code that was generated or modified by AI, or who's creation was greatly
influenced by AI has to be denoted in the following way:
(`<>` - marks a required section, `()` - marks an optional section)

```c
// Multi-line:

// AI-BEGIN [<ai model> (, <tool through which the model was used>)]
// Prompt: "<prompt>"
// Context: "(additional context available to the AI)"
code...
// AI-END

// Single line:

code... // AI [<model>]: "<prompt>"
```

### Memory allocation

Besides specific exceptions, all memory allocated in this project, especially
that which will be passed to other modules, should be allocated via the arena
allocator available in `common.h`.

The main reasons for using an arena allocator over traditional malloc/free are:

- No need to handle freeing memory for data after it is last used
- No risk of double-freeing memory or reading/writing invalid pointers
- Faster O(1) allocations for most types that will be used
- All allocated memory is valid until cleanup (or sometimes until reallocation)
- All memory is guaranteed to be freed once program exits
- Program is short-lived
- No need to check for `NULL` after allocation (since program should die anyway)

Mappings from standard libc methods to the arena allocator:

#### `malloc(size)` -> `arena_alloc(size)`

Allocates `size` bytes of uninitialized 16-byte aligned memory

#### `calloc(count, size)` -> `arena_zalloc(size)`

Allocates `size` bytes of zero-initialized 16-byte aligned memory

> [!NOTE]
> `arena_zalloc()` only accepts a single `size` parameter, if allocating for
> an array (or something else where a count larger than 1 would be used),
> the caller must calculate the size from the type size and count

#### `realloc(ptr, new_size)` -> `arena_realloc(ptr, old_size, new_size)`

Reallocates `ptr` from `old_size` bytes to `new_size` bytes, if `ptr` is `NULL`
`new_size` is allocated using `arena_alloc()`

> [!NOTE]
> `ptr` should not be used after the reallocation

> [!WARNING]
> `old_size` must be specified and **must** be equal to the current size of
> the allocated memory before the reallocation occurs. Providing an incorrect
> `old_size` may result in undefined behavior

#### `strndup(src, size)` -> `arena_strndup(src, size)`

Duplicates `src` string into a new memory allocation in the arena

> [!NOTE]
> This function allocates `n+1` bytes in the arena, always copies exactly
> `n` bytes from `src` to the new allocation and writes a NUL terminator
> to the last byte of the new string

> [!WARNING]
> This method does not stop on an embedded NUL terminator in the source
> string, the caller must guarantee that at least `size` bytes of the
> source string can actually be read

### Terminating the program

To correctly terminate the program, it is necessary to free all memory
allocated by the arena allocators and any future types using direct libc
allocations.

It is recommended that the only way to return from a successful run of the
compiler is for the `main` function to free the allocator and return `ERR_OK`.

If a module needs to terminate the compiler due to an error, it is recommended
that the `die(code, line, fmt, ...)` method is used. This method:

- Prints error to stderr if `code != ERR_OK`
  - Prints the error header (either `error:` or if `line > 0` then `error (line):`)
  - Prints error body based on `fmt` format string (same format as `printf` accepts)
- Frees arena
- Exits with `code` exit code
