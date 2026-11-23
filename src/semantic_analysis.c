/**
 * @file semantic_analysis.c
 * @brief semantic analysis
 * @author David Sobek <xsobekd00>
 *
 * Memory is managed by arena allocator.
 */

#include <stdbool.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "common.h"
#include "symtable.h"

// insert function into symTable
// return FALSE if function already exists
bool insertFunctionSymTable(SymTable *symTable, const char *name, unsigned line, unsigned column,
                            AstDataType returnType, AstDataType *paramTypes, size_t paramCount,
                            bool isBuiltin, bool isMain) {
  StSymbol *symbol = arenaAlloc(sizeof(StSymbol));
  symbol->type = ST_FUNCTION;
  symbol->declarationLocation.line = line;
  symbol->declarationLocation.column = column;
  symbol->data.function.returnType = returnType;
  symbol->data.function.parameterTypes = paramTypes;
  symbol->data.function.parameterCount = paramCount;
  symbol->data.function.isBuiltin = isBuiltin;
  symbol->data.function.isMain = isMain;
  return symtableInsertSymbol(symTable, name, symbol);
}

// insert variable into symTable and assign uniqueID
// return FALSE if variable already exists
bool insertVariableSymTable(SymTable *symTable, const char *name, unsigned line, unsigned column,
                            AstDataType dataType, bool isMutable) {
  StSymbol *symbol = arenaAlloc(sizeof(StSymbol));
  symbol->type = ST_VARIABLE;
  symbol->declarationLocation.line = line;
  symbol->declarationLocation.column = column;
  symbol->uniqueID = newVar(name);  // guarantees unique output from a string
  symbol->data.variable.dataType = dataType;
  symbol->data.variable.isMutable = isMutable;
  return symtableInsertSymbol(symTable, name, symbol);
}

// get function parameter count
size_t getParamCount(const AstFunction *func) {
  size_t i = 0;
  const AstFunctionParameter *param = func->firstParameter;
  while (param != NULL) {
    i++;
    param = param->nextParameter;
  }
  return i;
}

// tests type compatibility
bool isTypeCompatible(AstDataType expected, AstDataType actual) {
  return expected == actual || (expected == DOUBLE && actual == INT);
}

// get ast function* from function name
AstFunction *getAstFunction(AstProgram *ast, const char *functionName) {
  AstFunction *function = ast->firstFunction;
  while (function != NULL) {
    if (strcmp(function->functionName, functionName) == 0) {
      return function;
    }
    function = function->nextFunction;
  }
  die(ERR_INTERNAL, 0, "function not found");
}

// adds built in functions
// adds user defined functions
// check propper @main definition
void initializeFunctions(const AstProgram *ast, SymTable *globalSymTable) {
  // add built-in functions
  AstDataType *printParams = arenaAlloc(sizeof(AstDataType));
  printParams[0] = ANY;
  insertFunctionSymTable(globalSymTable, "print", 0, 0, UNIT, printParams, 1, true, false);

  insertFunctionSymTable(globalSymTable, "readLine", 0, 0, STRING, NULL, 0, true, false);

  insertFunctionSymTable(globalSymTable, "readInt", 0, 0, INT, NULL, 0, true, false);

  insertFunctionSymTable(globalSymTable, "readDouble", 0, 0, DOUBLE, NULL, 0, true, false);

  insertFunctionSymTable(globalSymTable, "toStr", 0, 0, STRING, printParams, 1, true, false);

  AstDataType *toIntParams = arenaAlloc(sizeof(AstDataType));
  toIntParams[0] = DOUBLE;
  insertFunctionSymTable(globalSymTable, "toInt", 0, 0, INT, toIntParams, 1, true, false);

  AstDataType *strLengthParams = arenaAlloc(sizeof(AstDataType));
  strLengthParams[0] = STRING;
  insertFunctionSymTable(globalSymTable, "strLength", 0, 0, INT, strLengthParams, 1, true, false);

  AstDataType *subStrParams = arenaAlloc(3 * sizeof(AstDataType));
  subStrParams[0] = STRING;
  subStrParams[1] = INT;
  subStrParams[2] = INT;
  insertFunctionSymTable(globalSymTable, "subStr", 0, 0, STRING, subStrParams, 3, true, false);

  AstDataType *ordParams = arenaAlloc(2 * sizeof(AstDataType));
  ordParams[0] = STRING;
  ordParams[1] = INT;
  insertFunctionSymTable(globalSymTable, "ord", 0, 0, INT, ordParams, 2, true, false);

  AstDataType *chrParams = arenaAlloc(sizeof(AstDataType));
  chrParams[0] = INT;
  insertFunctionSymTable(globalSymTable, "chr", 0, 0, STRING, chrParams, 1, true, false);

  // add user functions
  const AstFunction *func = ast->firstFunction;
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
      if (func->declaredReturnType != UNKNOWN && func->declaredReturnType != UNIT) {
        die(ERR_TYPE, func->location.line, "@main return type not UNIT");
      }
      foundMain = true;
    }

    // add function to SymTable
    size_t paramCount = getParamCount(func);
    AstDataType *paramTypes = NULL;
    if (paramCount > 0) {
      // create functtion paramType list
      paramTypes = arenaAlloc(paramCount * sizeof(AstDataType));
      const AstFunctionParameter *param = func->firstParameter;
      size_t i = 0;
      while (param != NULL) {
        paramTypes[i] = param->dataType;
        i++;
        param = param->nextParameter;
      }
    }
    if (!insertFunctionSymTable(globalSymTable, func->functionName, func->location.line,
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

// checkRecursion() helper function
// gets functionList function position from function name
size_t getFunctionPosition(AstFunction **functionList, size_t functionCount,
                           const char *functionName) {
  for (size_t i = 0; i < functionCount; i++) {
    if (strcmp(functionList[i]->functionName, functionName) == 0) {
      return i;
    }
  }
  die(ERR_INTERNAL, 0, "function recursion checking error");
}

// checkRecursion() helper function
// registers function dependency from a function call
void addFunctionDependency(size_t functionCount, AstFunction **functionList,
                           bool **functionDependenciesArray, size_t functionPosition,
                           SymTable *globalSymTable, const AstFunctionCall *functionCall) {
  const StSymbol *symbol = symtableLookup(globalSymTable, functionCall->functionName);
  if (symbol == NULL || symbol->type != ST_FUNCTION) {
    die(ERR_UNDEF, functionCall->location.line, "undefined function");
  }
  // ignore built in functions
  if (symbol->data.function.isBuiltin) {
    return;
  }
  functionDependenciesArray[functionPosition][getFunctionPosition(
      functionList, functionCount, functionCall->functionName)] = true;
}

// checkRecursion() helper function
// registers function dependencies from a code block
void getFunctionCodeBlockDependencies(size_t functionCount, AstFunction **functionList,
                                      bool **functionDependenciesArray, size_t functionPosition,
                                      SymTable *globalSymTable, AstCodeBlock *codeBlock) {
  AstCommand *command = codeBlock->firstCommand;
  const AstFunctionCall *functionCall = NULL;
  while (command != NULL) {
    // variable declaration using a function call
    if (command->type == VARIABLE_DECLARATION &&
        command->data.variableDeclaration.value->type == VALUE_FUNCTION_CALL) {
      functionCall = command->data.variableDeclaration.value->data.functionCall;
      addFunctionDependency(functionCount, functionList, functionDependenciesArray,
                            functionPosition, globalSymTable, functionCall);
      // assignment using a function call
    } else if (command->type == ASSIGNMENT &&
               command->data.assignment.value->type == VALUE_FUNCTION_CALL) {
      functionCall = command->data.assignment.value->data.functionCall;
      addFunctionDependency(functionCount, functionList, functionDependenciesArray,
                            functionPosition, globalSymTable, functionCall);
      // function call
    } else if (command->type == FUNCTION_CALL) {
      functionCall = command->data.call.functionCall;
      addFunctionDependency(functionCount, functionList, functionDependenciesArray,
                            functionPosition, globalSymTable, functionCall);
      // return using a function call
    } else if (command->type == RETURN && command->data.returnAst.value != NULL &&
               command->data.returnAst.value->type == VALUE_FUNCTION_CALL) {
      functionCall = command->data.returnAst.value->data.functionCall;
      addFunctionDependency(functionCount, functionList, functionDependenciesArray,
                            functionPosition, globalSymTable, functionCall);
      // IF statement condition cant contain a function call according to specifications
    } else if (command->type == IF) {
      getFunctionCodeBlockDependencies(functionCount, functionList, functionDependenciesArray,
                                       functionPosition, globalSymTable,
                                       command->data.ifAst.thenBlock);
      getFunctionCodeBlockDependencies(functionCount, functionList, functionDependenciesArray,
                                       functionPosition, globalSymTable,
                                       command->data.ifAst.elseBlock);
      // WHILE statement condition cant contain a function call according to specifications
    } else if (command->type == WHILE) {
      getFunctionCodeBlockDependencies(functionCount, functionList, functionDependenciesArray,
                                       functionPosition, globalSymTable,
                                       command->data.whileAst.codeBlock);
    }
    // EXPRESSION statement cant contain a function call according to specifications

    command = command->nextCommand;
  }
}

// checks if a function is recursive (even indirectly) and has an implicit return type (forbidden)
// prevents infinite data type search loops
// makes no changes to ast or symtable
void checkRecursion(AstProgram *ast, SymTable *globalSymTable) {
  // count user defined functions
  size_t functionCount = 0;
  AstFunction *function = ast->firstFunction;
  while (function != NULL) {
    functionCount++;
    function = function->nextFunction;
  }

  // create a list of all user functions
  AstFunction **functionList = (AstFunction **)arenaAlloc(functionCount * sizeof(AstFunction *));
  function = ast->firstFunction;
  for (size_t functionPosition = 0; functionPosition < functionCount; functionPosition++) {
    functionList[functionPosition] = function;
    function = function->nextFunction;
  }

  // create a 2D function dependency array
  bool **functionDependenciesArray = (bool **)arenaAlloc(functionCount * sizeof(bool *));
  for (size_t functionPosition = 0; functionPosition < functionCount; functionPosition++) {
    functionDependenciesArray[functionPosition] = arenaZalloc(functionCount * sizeof(bool));
  }

  // fill in function dependency array
  for (size_t functionPosition = 0; functionPosition < functionCount; functionPosition++) {
    function = functionList[functionPosition];
    // check function body
    if (function->bodyType == VALUE) {
      // check one line function body
      if (function->body.value->type == VALUE_FUNCTION_CALL) {
        // add called function
        const AstFunctionCall *functionCall = function->body.value->data.functionCall;
        addFunctionDependency(functionCount, functionList, functionDependenciesArray,
                              functionPosition, globalSymTable, functionCall);
      }
    } else {  // check function code blocks
      getFunctionCodeBlockDependencies(functionCount, functionList, functionDependenciesArray,
                                       functionPosition, globalSymTable, function->body.codeBlock);
    }
  }

  // calculate indirect function dependencies
  // Floyd–Warshall loop
  for (size_t k = 0; k < functionCount; k++) {
    for (size_t i = 0; i < functionCount; i++) {
      for (size_t j = 0; j < functionCount; j++) {
        if (functionDependenciesArray[i][k] && functionDependenciesArray[k][j]) {
          functionDependenciesArray[i][j] = true;
        }
      }
    }
  }

  // check recursive functions have explicit return types
  for (size_t i = 0; i < functionCount; i++) {
    if (functionDependenciesArray[i][i] && functionList[i]->declaredReturnType == UNKNOWN) {
      die(ERR_TYPE, functionList[i]->location.line,
          "recursive function with an implicit return type");
    }
  }
}

// analyzeFunctions() helper function
// analyze expression and return data type (recursive)
// saves data type to ast
AstDataType analyzeExpression(AstExpression *expression, SymTable *symTable) {
  expression->deducedDataType = UNKNOWN;
  if (expression->expressionType == INT_LITERAL) {
    expression->deducedDataType = INT;
  } else if (expression->expressionType == DOUBLE_LITERAL) {
    expression->deducedDataType = DOUBLE;
  } else if (expression->expressionType == STRING_LITERAL) {
    expression->deducedDataType = STRING;
  } else if (expression->expressionType == BOOL_LITERAL) {
    expression->deducedDataType = BOOL;
  } else if (expression->expressionType == UNIT_LITERAL) {
    expression->deducedDataType = UNIT;
  } else if (expression->expressionType == VARIABLE) {
    StSymbol *symbol = symtableLookup(symTable, expression->data.variable);
    if (symbol == NULL || symbol->type != ST_VARIABLE) {
      die(ERR_UNDEF, expression->location.line, "undefined variable");
    }
    expression->deducedDataType = symbol->data.variable.dataType;
    // rename variable
    expression->data.variable = symbol->uniqueID;
    // unary operators keep data type
  } else if (expression->expressionType == UNARY) {
    AstOperator op = expression->data.unary.operatorAst;
    AstDataType type = analyzeExpression(expression->data.unary.expression, symTable);
    // numerical NEGATION
    if ((type == INT || type == DOUBLE) && op == NEGATION) {
      expression->deducedDataType = type;
    }
    // LOGICAL_NEGATION
    else if (type == BOOL && op == LOGICAL_NEGATION) {
      expression->deducedDataType = type;
    }
  } else if (expression->expressionType == BINARY) {
    AstOperator op = expression->data.binary.operatorAst;
    AstDataType typeL = analyzeExpression(expression->data.binary.leftExpression, symTable);
    AstDataType typeR = analyzeExpression(expression->data.binary.rightExpression, symTable);
    // concantenation
    if (op == ADDITION && typeL == STRING && typeR == STRING) {
      expression->deducedDataType = STRING;
    }
    // numerical operation
    else if ((op == MULTIPLICATION || op == DIVISION || op == ADDITION || op == SUBTRACTION) &&
             ((typeL == INT || typeL == DOUBLE) && (typeR == INT || typeR == DOUBLE))) {
      // conversion to double
      if (typeL == DOUBLE || typeR == DOUBLE) {
        expression->deducedDataType = DOUBLE;
      } else {  // both int
        expression->deducedDataType = INT;
      }
    }
    // equality
    else if ((op == EQUAL || op == NOT_EQUAL) &&
             ((typeL == typeR) ||
              ((typeL == INT || typeL == DOUBLE) && (typeR == INT || typeR == DOUBLE)))) {
      expression->deducedDataType = BOOL;
    }
    // number and string comparassion
    else if (op == LESSER || op == LESSER_EQUAL || op == GREATER || op == GREATER_EQUAL) {
      // numbers or strings
      if (((typeL == INT || typeL == DOUBLE) && (typeR == INT || typeR == DOUBLE)) ||
          (typeL == STRING && typeR == STRING)) {
        expression->deducedDataType = BOOL;
      }
    }
    // logicall operation
    else if ((op == LOGICAL_AND || op == LOGICAL_OR) && typeL == BOOL && typeR == BOOL) {
      expression->deducedDataType = BOOL;
    }
  } else {
    die(ERR_INTERNAL, expression->location.line, "unknown expression type");
  }
  if (expression->deducedDataType == UNKNOWN) {
    die(ERR_TYPE, expression->location.line, "wrong data type");
  }
  return expression->deducedDataType;
}

// prototype
AstDataType analyzeFunctionBody(AstProgram *ast, SymTable *globalSymTable, AstFunction *function);
// analyzeFunctions() helper function
// analyze function call and return its data type (indirectly recursive)
// if function return type is unknown, call analyzeFunctionBody() on called function
AstDataType analyzeFunctionCall(AstProgram *ast, SymTable *globalSymTable, AstFunctionCall *call,
                                SymTable *symTable) {
  StSymbol *symbol = symtableLookup(globalSymTable, call->functionName);
  if (symbol == NULL || symbol->type != ST_FUNCTION) {
    die(ERR_UNDEF, call->location.line, "undefined function");
  }
  // check all arguments
  AstFunctionCallArgument *argument = call->firstArgument;
  size_t argCount = 0;
  while (argument != NULL) {
    argCount++;
    if (argCount > symbol->data.function.parameterCount) {
      die(ERR_ARGS, call->location.line, "too many function call parameters");
    }
    if (!isTypeCompatible(symbol->data.function.parameterTypes[argCount - 1],
                          analyzeExpression(argument->expression, symTable)) &&
        symbol->data.function.parameterTypes[argCount - 1] != ANY) {
      die(ERR_ARGS, call->location.line, "incorrect function call parameter type");
    }
    argument = argument->nextArgument;
  }
  if (argCount != symbol->data.function.parameterCount) {
    die(ERR_ARGS, call->location.line, "too few function call parameters");
  }

  // check return type
  if (symbol->data.function.returnType == UNKNOWN) {
    return analyzeFunctionBody(ast, globalSymTable, getAstFunction(ast, call->functionName));
  } else {
    return symbol->data.function.returnType;
  }
}

// analyzeFunctions() helper function
// analyze code block body and return whether it contains an explicit return (recursive)
// last code block needs to check returns
// resolves function return type
// resolves variable types
// codeBlock always contains commands
// unreachable code still gets analyzed
/* if hasToReturn and last command == IF:
if declaredReturnType == UNIT return Unit
if declaredReturnType != UNIT all if end with return
if declaredReturnType == UNKNOWN all if end with Unit
*/
bool analyzeCodeBlock(AstProgram *ast, SymTable *globalSymTable, AstFunction *function,
                      AstCodeBlock *block, SymTable *parentSymTable, bool hasToReturn) {
  block->symTable = symtableCreate(parentSymTable);
  bool explicitReturnFound = false;
  AstCommand *command = block->firstCommand;
  AstCommand *previousCommand = NULL;
  AstDataType lastFunctionCallType = UNKNOWN;
  while (true) {
    // VARIABLE_DECLARATION
    if (command->type == VARIABLE_DECLARATION) {
      command->data.variableDeclaration.resolvedType =
          command->data.variableDeclaration.declaredType;
      // inicialized by an expression
      if (command->data.variableDeclaration.value->type == VALUE_EXPRESSION) {
        // find variable type
        if (command->data.variableDeclaration.resolvedType == UNKNOWN) {
          command->data.variableDeclaration.resolvedType = analyzeExpression(
              command->data.variableDeclaration.value->data.expression, block->symTable);
        } else {  // check correct assignment type
          if (!isTypeCompatible(
                  command->data.variableDeclaration.resolvedType,
                  analyzeExpression(command->data.variableDeclaration.value->data.expression,
                                    block->symTable))) {
            die(ERR_TYPE, command->location.line, "wrong initialization type");
          }
        }
      } else {  // inicialized by a function call
        // find variable type
        if (command->data.variableDeclaration.resolvedType == UNKNOWN) {
          command->data.variableDeclaration.resolvedType = analyzeFunctionCall(
              ast, globalSymTable, command->data.variableDeclaration.value->data.functionCall,
              block->symTable);
        } else {  // check correct assignment type
          if (!isTypeCompatible(
                  command->data.variableDeclaration.resolvedType,
                  analyzeFunctionCall(ast, globalSymTable,
                                      command->data.variableDeclaration.value->data.functionCall,
                                      block->symTable))) {
            die(ERR_TYPE, command->location.line, "wrong initialization type");
          }
        }
      }
      // add variable to current symTable
      if (!insertVariableSymTable(block->symTable, command->data.variableDeclaration.variableName,
                                  command->location.line, command->location.column,
                                  command->data.variableDeclaration.resolvedType,
                                  command->data.variableDeclaration.isMutable)) {
        die(ERR_REDEF, command->location.line, "variable alredy defined");
      }
      // rename variable
      StSymbol *symbol =
          symtableLookup(block->symTable, command->data.variableDeclaration.variableName);
      command->data.variableDeclaration.variableName = symbol->uniqueID;
      // ASSIGNMENT
    } else if (command->type == ASSIGNMENT) {
      // check if variable exists
      const StSymbol *symbol =
          symtableLookup(block->symTable, command->data.assignment.variableName);
      if (symbol == NULL || symbol->type != ST_VARIABLE) {
        die(ERR_UNDEF, command->location.line, "undefined variable");
      }

      // check if variable isnt immutable
      if (!symbol->data.variable.isMutable) {
        die(ERR_CONST, command->location.line, "immutable variable assignment attemp");
      }

      // assignment by an expression
      if (command->data.assignment.value->type == VALUE_EXPRESSION) {
        // check correct assignment type
        if (!isTypeCompatible(symbol->data.variable.dataType,
                              analyzeExpression(command->data.assignment.value->data.expression,
                                                block->symTable))) {
          die(ERR_TYPE, command->location.line, "incompatible assignment type");
        }
      } else {  // assignment by a function call
        // check correct assignment type
        if (!isTypeCompatible(symbol->data.variable.dataType,
                              analyzeFunctionCall(ast, globalSymTable,
                                                  command->data.assignment.value->data.functionCall,
                                                  block->symTable))) {
          die(ERR_TYPE, command->location.line, "incompatible assignment type");
        }
      }
      // rename variable
      command->data.assignment.variableName = symbol->uniqueID;
      command->data.assignment.resolvedType = symbol->data.variable.dataType;
      // FUNCTION_CALL
    } else if (command->type == FUNCTION_CALL) {
      lastFunctionCallType = analyzeFunctionCall(ast, globalSymTable,
                                                 command->data.call.functionCall, block->symTable);
      // RETURN
    } else if (command->type == RETURN) {
      explicitReturnFound = true;

      // functions with return have to have explicit return type
      if (function->declaredReturnType == UNKNOWN) {
        die(ERR_TYPE, function->location.line,
            "functions with return have to have explicit return type");
      }

      // UNIT functions evaluate returns but return ()
      if (function->declaredReturnType == UNIT) {
        // empty return
        if (command->data.returnAst.value == NULL) {
          // new UNIT expression
          AstExpression *unitExpression = astExpressionNew(UNIT_LITERAL, command->location);
          unitExpression->deducedDataType = UNIT;
          // create return() command
          command->data.returnAst.value = astValueFromExpr(unitExpression);
        } else {  // non empty return
          bool unitReturn = false;
          if (command->data.returnAst.value->type == VALUE_EXPRESSION) {
            if (command->data.returnAst.value->data.expression->expressionType == UNIT_LITERAL) {
              unitReturn = true;
            }
          }
          // split return into expression or function call and return() if not done already
          if (!unitReturn) {
            AstCommand *newCommand;
            // split into an expression
            if (command->data.returnAst.value->type == VALUE_EXPRESSION) {
              newCommand = astCodeCommandNew(EXPRESSION, command->location);
              newCommand->data.expressionAst.expression =
                  command->data.returnAst.value->data.expression;
            }
            // split into a function call
            else {
              newCommand = astCodeCommandNew(FUNCTION_CALL, command->location);
              newCommand->data.call.functionCall = command->data.returnAst.value->data.functionCall;
            }
            // insert expression or function call in front of return
            newCommand->nextCommand = command;
            if (previousCommand != NULL) {
              previousCommand->nextCommand = newCommand;
            } else {
              block->firstCommand = newCommand;
            }
            // new UNIT expression
            AstExpression *unitExpression = astExpressionNew(UNIT_LITERAL, command->location);
            unitExpression->deducedDataType = UNIT;
            // rewrite return
            command->data.returnAst.value->type = VALUE_EXPRESSION;
            command->data.returnAst.value->data.expression = unitExpression;
            // move analysis back to newCommand
            command = newCommand;
            continue;
          }
        }
      }
      // return()
      else if (command->data.returnAst.value == NULL) {
        die(ERR_OTHER, command->location.line,
            "return UNIT forbidden outside explicit UNIT functions");
      }
      // return by expression
      else if (command->data.returnAst.value->type == VALUE_EXPRESSION) {
        // check correct return type
        if (!isTypeCompatible(function->resolvedReturnType,
                              analyzeExpression(command->data.returnAst.value->data.expression,
                                                block->symTable))) {
          die(ERR_TYPE, command->location.line, "incompatible return type");
        }
        // return by function call
      } else {
        // check correct return type
        if (!isTypeCompatible(function->resolvedReturnType,
                              analyzeFunctionCall(ast, globalSymTable,
                                                  command->data.returnAst.value->data.functionCall,
                                                  block->symTable))) {
          die(ERR_TYPE, command->location.line, "incompatible return type");
        }
      }

      // IF
    } else if (command->type == IF) {
      // check condition
      if (analyzeExpression(command->data.ifAst.condition, block->symTable) != BOOL) {
        die(ERR_TYPE, command->location.line, "if statement condition is not boolean");
      }
      // doesnt have to return
      if (command->nextCommand != NULL || !hasToReturn || explicitReturnFound ||
          function->declaredReturnType == UNIT) {
        bool explicitReturnFoundLeft = analyzeCodeBlock(
            ast, globalSymTable, function, command->data.ifAst.thenBlock, block->symTable, false);
        // has else block
        if (command->data.ifAst.elseBlock != NULL) {
          bool explicitReturnFoundRight = analyzeCodeBlock(
              ast, globalSymTable, function, command->data.ifAst.elseBlock, block->symTable, false);
          if (explicitReturnFoundLeft && explicitReturnFoundRight) {
            explicitReturnFound = true;
          }
        }
        // no else block
        else {
          if (explicitReturnFoundLeft) {
            explicitReturnFound = true;
          }
        }
      }
      // WHILE
    } else if (command->type == WHILE) {
      // check condition
      if (analyzeExpression(command->data.whileAst.condition, block->symTable) != BOOL) {
        die(ERR_TYPE, command->location.line, "while statement condition is not boolean");
      }

      // check code blocks
      analyzeCodeBlock(ast, globalSymTable, function, command->data.whileAst.codeBlock,
                       block->symTable, false);

      // EXPRESSION
    } else if (command->type == EXPRESSION) {
      analyzeExpression(command->data.expressionAst.expression, block->symTable);
    }

    if (command->nextCommand != NULL) {
      previousCommand = command;
      command = command->nextCommand;
    } else {
      break;
    }
  }

  // resolve an implicit return and replace it with an explicit return if necessary
  // command = last command
  if (hasToReturn && !explicitReturnFound) {
    AstExpression *newExpression = arenaAlloc(sizeof(AstExpression));
    AstValue *newValue = arenaAlloc(sizeof(AstValue));
    AstCommand *newCommand = arenaAlloc(sizeof(AstCommand));
    newCommand->type = RETURN;
    newCommand->location.line = command->location.line;
    newCommand->location.column = command->location.column;
    newCommand->nextCommand = NULL;
    newCommand->data.returnAst.value = newValue;

    // return UNIT
    if (command->type == VARIABLE_DECLARATION || command->type == ASSIGNMENT ||
        command->type == WHILE || function->declaredReturnType == UNIT) {
      // set return
      if (function->resolvedReturnType == UNKNOWN) {
        function->resolvedReturnType = UNIT;
      }
      // check correct return
      else {
        if (function->resolvedReturnType != UNIT) {
          die(ERR_TYPE, command->location.line, "incompatible implicit return type");
        }
      }
      // create return UNIT command
      newExpression->expressionType = UNIT_LITERAL;
      newExpression->location.line = command->location.line;
      newExpression->location.column = command->location.column;
      newExpression->deducedDataType = UNIT;
      newValue->type = VALUE_EXPRESSION;
      newValue->data.expression = newExpression;
      command->nextCommand = newCommand;
      block->lastCommand = newCommand;
    }
    // return FUNCTION_CALL
    else if (command->type == FUNCTION_CALL) {
      // set return
      if (function->resolvedReturnType == UNKNOWN) {
        function->resolvedReturnType = lastFunctionCallType;
      }
      // check correct return
      else if (!isTypeCompatible(function->resolvedReturnType, lastFunctionCallType)) {
        die(ERR_TYPE, command->location.line, "incompatible implicit return type");
      }
      // replace function call with return function call
      newValue->type = VALUE_FUNCTION_CALL;
      newValue->data.functionCall = command->data.call.functionCall;
      if (previousCommand != NULL) {
        previousCommand->nextCommand = newCommand;
      } else {
        block->firstCommand = newCommand;
      }
      block->lastCommand = newCommand;
    }
    // return EXPRESSION
    else if (command->type == EXPRESSION) {
      // set return
      if (function->resolvedReturnType == UNKNOWN) {
        function->resolvedReturnType = command->data.expressionAst.expression->deducedDataType;
      }
      // check correct return
      else if (!isTypeCompatible(function->resolvedReturnType,
                                 command->data.expressionAst.expression->deducedDataType)) {
        die(ERR_TYPE, command->location.line, "incompatible implicit return type");
      }
      // replace expression with return expression
      newValue->type = VALUE_EXPRESSION;
      newValue->data.expression = command->data.expressionAst.expression;
      if (previousCommand != NULL) {
        previousCommand->nextCommand = newCommand;
      } else {
        block->firstCommand = newCommand;
      }
      block->lastCommand = newCommand;
    }
    // IF return
    else if (command->type == IF) {
      bool explicitReturnFoundLeft = analyzeCodeBlock(
          ast, globalSymTable, function, command->data.ifAst.thenBlock, block->symTable, true);
      // has else block
      if (command->data.ifAst.elseBlock != NULL) {
        bool explicitReturnFoundRight = analyzeCodeBlock(
            ast, globalSymTable, function, command->data.ifAst.elseBlock, block->symTable, true);
        if (explicitReturnFoundLeft && explicitReturnFoundRight) {
          explicitReturnFound = true;
        }
      }
      // no else block
      else {
        if (explicitReturnFoundLeft) {
          explicitReturnFound = true;
        }
      }
      if (!explicitReturnFound) {  // all "if" end with implicit return UNIT
        if (function->resolvedReturnType != UNIT) {
          die(ERR_TYPE, command->location.line,
              "function ends with an implicit return if statement that does not return UNIT");
        }
      }
    }
  }
  return explicitReturnFound;
}

// analyzeFunctions() helper function
// analyze function body and return its data type (indirectly recursive)
// checks correct returns
// updates symTable function return type at the end
AstDataType analyzeFunctionBody(AstProgram *ast, SymTable *globalSymTable, AstFunction *function) {
  // create a new symTable for the function body
  function->symTable = symtableCreate(globalSymTable);
  if (function->isMain) {
    function->resolvedReturnType = UNIT;
  } else {
    function->resolvedReturnType = function->declaredReturnType;
  }
  // check all function params and add them to the current symTable

  AstFunctionParameter *param = function->firstParameter;
  StSymbol *symbol;
  while (param != NULL) {
    if (!insertVariableSymTable(function->symTable, param->parameterName, function->location.line,
                                function->location.column, param->dataType, false)) {
      die(ERR_REDEF, function->location.line, "duplicate function parameter names");
    }
    // rename function params
    symbol = symtableLookup(function->symTable, param->parameterName);
    param->parameterName = symbol->uniqueID;
    param = param->nextParameter;
  }

  // check function body
  if (function->bodyType == VALUE) {
    // check expression
    if (function->body.value->type == VALUE_EXPRESSION) {
      // find return type
      if (function->resolvedReturnType == UNKNOWN) {
        function->resolvedReturnType =
            analyzeExpression(function->body.value->data.expression, function->symTable);
      }
      // check correct return type
      else if (!isTypeCompatible(
                   function->resolvedReturnType,
                   analyzeExpression(function->body.value->data.expression, function->symTable)) &&
               function->declaredReturnType != UNIT) {
        die(ERR_TYPE, function->location.line, "wrong return type");
      }
    } else {  // check function call
      // find return type
      if (function->resolvedReturnType == UNKNOWN) {
        function->resolvedReturnType = analyzeFunctionCall(
            ast, globalSymTable, function->body.value->data.functionCall, function->symTable);
      }
      // check correct return type
      else if (!isTypeCompatible(
                   function->resolvedReturnType,
                   analyzeFunctionCall(ast, globalSymTable, function->body.value->data.functionCall,
                                       function->symTable)) &&
               function->declaredReturnType != UNIT) {
        die(ERR_TYPE, function->location.line, "wrong return type");
      }
    }
  } else {  // check code blocks
    analyzeCodeBlock(ast, globalSymTable, function, function->body.codeBlock, function->symTable,
                     true);
  }

  if (function->resolvedReturnType == UNKNOWN) {
    die(ERR_TYPE, function->location.line, "cannot resolve function return type");
  }
  if (function->resolvedReturnType == BOOL) {
    die(ERR_TYPE, function->location.line, "bool initialization type is forbidden");
  }
  symbol = symtableLookup(globalSymTable, function->functionName);
  symbol->data.function.returnType = function->resolvedReturnType;
  return function->resolvedReturnType;
}

// analyzes function and variable declarations and data types
// cehck immutable variables arent modified
void analyzeFunctions(AstProgram *ast, SymTable *globalSymTable) {
  // check all functions
  AstFunction *function = ast->firstFunction;
  while (function != NULL) {
    // if function hasnt been already checked due to dependencies
    if (function->resolvedReturnType == UNKNOWN) {
      analyzeFunctionBody(ast, globalSymTable, function);
    }
    function = function->nextFunction;
  }
}

// main function
void semanticAnalysis(AstProgram *ast) {
  SymTable *globalSymTable = symtableCreate(NULL);

  initializeFunctions(ast, globalSymTable);
  checkRecursion(ast, globalSymTable);
  analyzeFunctions(ast, globalSymTable);
}
