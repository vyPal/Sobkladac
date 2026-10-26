/**
 * @file test_symtable.c
 * Project: IFJ26 compiler
 * @brief Symbol table unit tests
 * @author David Sobek <xsobekd00>
 *
 * Tests for the symbol table in symtable.c
 */

#include <stdbool.h>
#include <stddef.h>

#include "common.h"
#include "symtable.h"
#include "tst.h"

tstsuite("SymTable", ) {
  tstcase("symtableCreate", ) {
    SymTable *global = symtableCreate(NULL);

    tstcheck(global != NULL, "global symbol table should be created");
    tstcheck(global->symbols != NULL, "global table should have a hash table");
    tstcheck(global->parent == NULL, "global table should have no parent");

    SymTable *local = symtableCreate(global);

    tstcheck(local != NULL, "local symbol table should be created");
    tstcheck(local->symbols != NULL, "local table should have a hash table");
    tstcheck(local->parent == global, "local table should reference its parent");
  }

  tstcase("symtableInsertSymbol", ) {
    SymTable *table = symtableCreate(NULL);

    StSymbol symbol = {0};

    tstcheck(symtableInsertSymbol(table, "symbol", &symbol) == true, "symbol should be inserted");

    tstcheck(symtableLookup(table, "symbol") == &symbol, "inserted symbol should be found");

    tstcheck(symtableLookupLocal(table, "symbol") == &symbol,
             "inserted symbol should be found locally");

    tstcheck(symtableInsertSymbol(table, "symbol", &symbol) == false,
             "duplicate symbol should not be inserted");
  }

  tstcase("symtableLookup", ) {
    SymTable *global = symtableCreate(NULL);
    SymTable *local = symtableCreate(global);

    StSymbol symbol = {0};

    symtableInsertSymbol(global, "symbol", &symbol);

    tstcheck(symtableLookup(global, "symbol") == &symbol, "symbol should be found in global scope");

    tstcheck(symtableLookup(local, "symbol") == &symbol, "symbol should be found from local scope");

    tstcheck(symtableLookup(local, "nothing") == NULL, "nonexistent symbol should not be found");
  }

  tstcase("symtableLookupLocal", ) {
    SymTable *global = symtableCreate(NULL);
    SymTable *local = symtableCreate(global);

    StSymbol symbol = {0};

    symtableInsertSymbol(global, "symbol", &symbol);

    tstcheck(symtableLookupLocal(global, "symbol") == &symbol,
             "symbol should be found in its own scope");

    tstcheck(symtableLookupLocal(local, "symbol") == NULL,
             "local lookup should not search parent scopes");

    tstcheck(symtableLookupLocal(local, "nothing") == NULL,
             "nonexistent local symbol should not be found");
  }

  tstcase("symtableShadowing", ) {
    SymTable *global = symtableCreate(NULL);
    SymTable *local = symtableCreate(global);

    StSymbol globalSymbol = {0};

    StSymbol localSymbol = {0};

    symtableInsertSymbol(global, "symbol", &globalSymbol);

    tstcheck(symtableInsertSymbol(local, "symbol", &localSymbol) == true,
             "local scope should allow shadowing a parent symbol");

    tstcheck(symtableLookupLocal(local, "symbol") == &localSymbol,
             "local lookup should return the shadowing symbol");

    tstcheck(symtableLookup(local, "symbol") == &localSymbol,
             "general lookup should prefer the nearest scope");

    tstcheck(symtableLookupLocal(global, "symbol") == &globalSymbol,
             "shadowing should not change the parent symbol");
  }
}
