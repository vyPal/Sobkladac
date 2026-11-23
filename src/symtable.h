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
#include <stdio.h>

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
  char **parameterNames;
  size_t parameterCount;
  bool isBuiltin;
  bool isMain;
} StFunctionSymbol;

/// @brief Symbol-table Symbol.
typedef struct {
  StSymbolType type;  // variable or function
  AstLocation declarationLocation;
  char *uniqueID;

  union {
    StVariableSymbol variable;
    StFunctionSymbol function;
  } data;
} StSymbol;

/// @brief Symbol-table.
typedef struct SymTable {
  HashTable *symbols;
  struct SymTable *parent;
} SymTable;

/// @brief Create Symbol table. parent = NULL => global scope
SymTable *symtableCreate(SymTable *parent);

/// @brief Look up a symbol in the current scope and its parent scopes.
/// returns NULL if symbol doesnt exist
StSymbol *symtableLookup(SymTable *table, const char *symbolName);

/// @brief Look up a symbol in the current scope.
/// returns NULL if symbol doesnt exist
StSymbol *symtableLookupLocal(SymTable *table, const char *symbolName);

/// @brief Insert a new symbol inside a Symbol table.
/// returns FALSE if symbol already exists
bool symtableInsertSymbol(SymTable *table, const char *symbolName, StSymbol *symbol);

#endif  // SYMTABLE_H

/* TIPS FOR SEMANTIC ANALYSIS
Create the global scope and register built-in functions.
Register all user-defined function names and their declared signatures.
Analyze function bodies, resolving parameter and local-variable names.
Complete inferred return types and perform the remaining semantic checks.
*/
