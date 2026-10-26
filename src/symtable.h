/**
 * @file symtable.h
 * @brief Symbol table header.
 * @author David Sobek <xsobekd00>
 *
 * Memory is managed by arena allocator.
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "hashtable.h"

/// @brief Symbols can be variables or function.
typedef enum { ST_VARIABLE, ST_FUNCTION } StSymbolType;

/// @brief Symbol as a variable.
typedef struct {
  AstDataType dataType;
  bool isMutable;
} StVariableSymbol;

/// @brief Symbol as a function.
typedef struct {
  AstDataType returnType;
  AstDataType *parameterTypes;
  size_t parameterCount;
  bool isBuiltin;
  bool isMain;
} StFunctionSymbol;

/// @brief Symbol-table Symbol.
typedef struct {
  StSymbolType type;  // variable or function
  AstLocation declarationLocation;

  union {
    StVariableSymbol variable;
    StFunctionSymbol function;
  } data;
} StSymbol;

/// @brief Symbol-table.
typedef struct StSymTable {
  HashTable *symbols;
  struct StSymTable *parent;
} StSymTable;

/// @brief Create Symbol table. parent = NULL => global scope
StSymTable *symtableCreate(StSymTable *parent);

/// @brief Look up a symbol in the current scope and its parent scopes.
StSymbol *symtableLookup(StSymTable *table, const char *name);

/// @brief Look up a symbol in the current scope.
StSymbol *symtableLookupLocal(StSymTable *table, const char *name);

/// @brief Insert a new symbol inside a Symbol table.
bool symtableInsertSymbol(StSymTable *table, const char *name, StSymbol *symbol);

#endif  // SYMTABLE_H

/* TIPS FOR SEMATIC ANALYSIS
Create the global scope and register built-in functions.
Register all user-defined function names and their declared signatures.
Analyze function bodies, resolving parameter and local-variable names.
Complete inferred return types and perform the remaining semantic checks.
*/
