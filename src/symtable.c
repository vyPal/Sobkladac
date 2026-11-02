/**
 * @file symtable.c
 * @brief Symbol table implementation using hashtable.
 * @author David Sobek <xsobekd00>
 */

#include "symtable.h"

#include "common.h"

SymTable *symtableCreate(SymTable *parent) {
  SymTable *newStSymTable = arenaAlloc(sizeof(SymTable));
  newStSymTable->parent = parent;
  newStSymTable->symbols = htCreate(16);
  return newStSymTable;
}

StSymbol *symtableLookup(SymTable *table, const char *symbolName) {
  SymTable *currentTable = table;

  if (symbolName == NULL) {
    return NULL;
  }

  while (currentTable != NULL) {
    if (htContains(currentTable->symbols, symbolName)) {
      return htGet(currentTable->symbols, symbolName);
    }
    currentTable = currentTable->parent;
  }
  return NULL;
}

StSymbol *symtableLookupLocal(SymTable *table, const char *symbolName) {
  if (table == NULL || symbolName == NULL) {
    return NULL;
  }

  if (htContains(table->symbols, symbolName)) {
    return htGet(table->symbols, symbolName);
  }
  return NULL;
}

bool symtableInsertSymbol(SymTable *table, const char *symbolName, StSymbol *symbol) {
  if (table == NULL || symbolName == NULL || symbol == NULL) {
    return false;
  }

  if (symtableLookupLocal(table, symbolName) != NULL) {
    return false;
  }
  return htInsert(table->symbols, symbolName, symbol);
}
