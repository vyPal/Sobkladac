/**
 * @file semantic_analysis.c
 * @brief semantic analysis
 * @author David Sobek <xsobekd00>
 *
 * Memory is managed by arena allocator.
 */

/*
Semantic analysis checklist:
1. Symbol table initialization
- Add built-in functions to the symbol table.
- Add all user-defined functions to the symbol table.
- Check duplicate function names.
- Check that exactly one valid main exists.
2. Declaration and reference checking
- Check variable declarations.
- Check duplicate declarations in the same scope.
- Check that variables are declared before use.
- check function calls.
- Check that immutable variables (val) aren't modified.
3. Type checking
- Infer types of expressions.
- Check operator compatibility.
- Check assignment compatibility.
- Check function argument types.
- Check function return types.
- Check conditions in if and while.
4. Return checking
- Check that return statements match the declared return type.
- Check that functions return on all required execution paths.
- Check that main follows its required return rules.
5. AST resolution for code generation
- Assign unique symbol IDs to variables.
- Assign unique function IDs to functions.
- Store resolved IDs in AST references.
- Ensure the code generator can distinguish variables with identical names in different scopes.
*/

/*
Error codes:
1 chyba v programu v rámci lexikální analýzy (chybná struktura aktuálního lexému, komen-
táře nebo odsazení).
2 chyba v programu v rámci syntaktické analýzy (chybná syntaxe, chybějící prolog, nepo-
volená konstrukce nebo porušení struktury odsazených bloků).
3 sémantická chyba – použití nedefinované funkce, proměnné nebo typu; chybějící funkce
s anotací @main.
4 sémantická chyba – nepovolená redefinice funkce, proměnné, typu.
5 sémantická chyba – nesprávný počet nebo typ argumentů při volání funkce, nepovolené
parametry funkce s anotací @main.
6 sémantická chyba typové kompatibility nebo odvození typu ve výrazu, přiřazení, iniciali-
zaci, návratové hodnotě či řídicí konstrukci.
7 sémantická chyba – pokus o zápis do nemodifikovatelné proměnné nebo formálního pa-
rametru funkce.
10 ostatní sémantické chyby (např. nepovolený return, nevyčerpávající match nebo nulový
krok rozsahu).
*/

#include <stdbool.h>

#include "ast.h"
#include "codegen.h"
#include "common.h"
#include "symtable.h"

typedef struct {
  SymTable *globalSymTable;
} SemanticContext;

// insert function into symTable
bool insertFunctionSymTable(SymTable *symTable, char *name, unsigned line, unsigned column,
                            AstDataType returnType, AstDataType *paramTypes, size_t paramCount,
                            bool isBuiltin, bool isMain) {
  StSymbol *symbol = arenaAlloc(sizeof(StSymbol));
  symbol->type = ST_FUNCTION;
  symbol->declarationLocation.line = line;
  symbol->declarationLocation.column = column;
  symbol->uniqueID = newVar(
      name);  // Calling this method multiple times with the same input provides different outputs
  symbol->data.function.returnType = returnType;
  symbol->data.function.parameterTypes = paramTypes;
  symbol->data.function.parameterCount = paramCount;
  symbol->data.function.isBuiltin = isBuiltin;
  symbol->data.function.isMain = isMain;
  return symtableInsertSymbol(symTable, name, symbol);
}

// insert variable into symTable
bool insertVariableSymTable(SymTable *symTable, char *name, unsigned line, unsigned column,
                            AstDataType dataType, bool isMutable) {
  StSymbol *symbol = arenaAlloc(sizeof(StSymbol));
  symbol->type = ST_VARIABLE;
  symbol->declarationLocation.line = line;
  symbol->declarationLocation.column = column;
  symbol->uniqueID = newVar(
      name);  // Calling this method multiple times with the same input provides different outputs
  symbol->data.variable.dataType = dataType;
  symbol->data.variable.isMutable = isMutable;
  return symtableInsertSymbol(symTable, name, symbol);
}

// get function parameter count
size_t getParamCount(AstFunction *func) {
  size_t i = 0;
  AstFunctionParameter *param = func->firstParameter;
  while (param != NULL) {
    i++;
    param = param->nextParameter;
  }
  return i;
}

// add functions to global symtable
void initializeSymtable(AstProgram *ast, SemanticContext *context) {
  context->globalSymTable = symtableCreate(NULL);

  // add built-in functions
  AstDataType *printParams = arenaAlloc(sizeof(AstDataType));
  printParams[0] = ANY;
  insertFunctionSymTable(context->globalSymTable, "print", 0, 0, UNIT, printParams, 1, true, false);

  insertFunctionSymTable(context->globalSymTable, "readLine", 0, 0, STRING, NULL, 0, true, false);

  insertFunctionSymTable(context->globalSymTable, "readInt", 0, 0, INT, NULL, 0, true, false);

  insertFunctionSymTable(context->globalSymTable, "readDouble", 0, 0, DOUBLE, NULL, 0, true, false);

  AstDataType *toStrParams = arenaAlloc(sizeof(AstDataType));
  toStrParams[0] = ANY;
  insertFunctionSymTable(context->globalSymTable, "toStr", 0, 0, STRING, toStrParams, 1, true,
                         false);

  AstDataType *toIntParams = arenaAlloc(sizeof(AstDataType));
  toIntParams[0] = DOUBLE;
  insertFunctionSymTable(context->globalSymTable, "toInt", 0, 0, INT, toIntParams, 1, true, false);

  AstDataType *strLengthParams = arenaAlloc(sizeof(AstDataType));
  strLengthParams[0] = STRING;
  insertFunctionSymTable(context->globalSymTable, "strLength", 0, 0, INT, strLengthParams, 1, true,
                         false);

  AstDataType *subStrParams = arenaAlloc(3 * sizeof(AstDataType));
  subStrParams[0] = STRING;
  subStrParams[1] = INT;
  subStrParams[2] = INT;
  insertFunctionSymTable(context->globalSymTable, "subStr", 0, 0, STRING, subStrParams, 3, true,
                         false);

  AstDataType *ordParams = arenaAlloc(2 * sizeof(AstDataType));
  ordParams[0] = STRING;
  ordParams[1] = INT;
  insertFunctionSymTable(context->globalSymTable, "ord", 0, 0, INT, ordParams, 2, true, false);

  AstDataType *chrParams = arenaAlloc(sizeof(AstDataType));
  chrParams[0] = INT;
  insertFunctionSymTable(context->globalSymTable, "chr", 0, 0, STRING, chrParams, 1, true, false);

  // add user functions
  AstFunction *func = ast->firstFunction;
  bool foundMain = false;
  while (func != NULL) {
    // check correct main function declaration
    if (func->isMain) {
      if (foundMain == true) {
        die(ERR_REDEF, func->location.line, "second @main definition");
      }
      if (func->firstParameter != NULL) {
        die(ERR_ARGS, func->location.line, "@main parameters not NULL");
      }
      if (func->declaredReturnType != UNIT) {
        die(ERR_TYPE, func->location.line, "@main return type not UNIT");
      }
      foundMain = true;
    }

    // add function to SymTable
    size_t paramCount = getParamCount(func);
    AstDataType *paramTypes = NULL;
    if (paramCount > 0) {
      paramTypes = arenaAlloc(paramCount * sizeof(AstDataType));
      AstFunctionParameter *param = func->firstParameter;
      unsigned i = 0;
      while (param != NULL) {
        paramTypes[i] = param->dataType;
        i++;
        param = param->nextParameter;
      }
    }
    if (!insertFunctionSymTable(context->globalSymTable, func->functionName, func->location.line,
                                func->location.column, func->declaredReturnType, paramTypes,
                                paramCount, false, func->isMain)) {
      die(ERR_REDEF, func->location.line, "function redefinition");
    }
    func = func->nextFunction;
  }
  // check if main function was found
  if (foundMain == false) {
    die(ERR_UNDEF, 0, "@main not found");
  }
}

// check codeBlock variable declarations
// codeBlock always contains commands
void checkDeclarationsCodeBlock(AstCodeBlock *block, SymTable *scope) {
  AstCommand *command = block->firstCommand;
  while (command != NULL) {
    if (command->type == VARIABLE_DECLARATION) {
    } else if (command->type == ASSIGNMENT) {
    } else if (command->type == FUNCTION_CALL) {
    } else if (command->type == RETURN) {
    } else if (command->type == IF) {
    } else if (command->type == WHILE) {
    } else if (command->type == EXPRESSION) {
    }
  }
}

// check variable declarations
void checkDeclarations(AstProgram *ast, SemanticContext *context) {
  // for all functions
  AstFunction *function = ast->firstFunction;
  while (function != NULL) {
    function->scope = symtableCreate(context->globalSymTable);

    // check function params
    AstFunctionParameter *param = function->firstParameter;
    while (param != NULL) {
      if (!insertVariableSymTable(function->scope, param->parameterName, function->location.line,
                                  function->location.column, param->dataType, false)) {
        die(ERR_REDEF, function->location.line, "duplicate function parameter names");
      }
      param = param->nextParameter;
    }

    // check function body
    if (function->bodyType == CODE_BLOCK) {
      checkDeclarationsCodeBlock(function->body.codeBlock, function->scope);
    }

    function = function->nextFunction;
  }
}

void checkTypes(AstProgram *ast, SemanticContext *context) {
  return;
}

void checkReturns(AstProgram *ast, SemanticContext *context) {
  return;
}

void uniqueNameResolution(AstProgram *ast, SemanticContext *context) {
  return;
}

void semanticAnalysis(AstProgram *ast) {
  SemanticContext *context = arenaAlloc(sizeof(SemanticContext));

  initializeSymtable(ast, context);
  checkDeclarations(ast, context);
  checkTypes(ast, context);
  checkReturns(ast, context);
  uniqueNameResolution(ast, context);
}
