/**
 * @file ast.c
 * Project: IFJ26 compiler
 * @brief Header file for codegen
 * @author Jakub Palacky <xpalacj00>
 *
 * Implementation of methods defined in ast.h
 */

#include "ast.h"

#include <string.h>

#include "common.h"

AstProgram *astProgramNew(void) {
  AstProgram *new = arenaZalloc(sizeof(AstProgram));
  return new;
}

AstFunction *astFunctionNew(const char *name, AstLocation location) {
  AstFunction *new = arenaZalloc(sizeof(AstFunction));
  new->functionName = arenaStrndup(name, strlen(name));
  new->location = location;
  return new;
}

AstFunctionParameter *astFunctionParameterNew(const char *name, AstDataType type) {
  AstFunctionParameter *new = arenaZalloc(sizeof(AstFunctionParameter));
  new->parameterName = arenaStrndup(name, strlen(name));
  new->dataType = type;
  return new;
}

AstCodeBlock *astCodeBlockNew(void) {
  return arenaZalloc(sizeof(AstCodeBlock));
}

AstCommand *astCodeCommandNew(AstCommandType type, AstLocation location) {
  AstCommand *new = arenaZalloc(sizeof(AstCommand));
  new->type = type;
  new->location = location;
  return new;
}

AstExpression *astExpressionNew(AstExpressionType type, AstLocation location) {
  AstExpression *new = arenaZalloc(sizeof(AstExpression));
  new->expressionType = type;
  new->location = location;
  return new;
}

AstFunctionCall *astFunctionCallNew(const char *name, AstLocation location) {
  AstFunctionCall *new = arenaZalloc(sizeof(AstFunctionCall));
  new->functionName = arenaStrndup(name, strlen(name));
  new->location = location;
  return new;
}

AstFunctionCallArgument *astFunctionCallArgumentNew(AstExpression *expression) {
  AstFunctionCallArgument *new = arenaZalloc(sizeof(AstFunctionCallArgument));
  new->expression = expression;
  return new;
}

AstValue *astValueFromExpr(AstExpression *expression) {
  AstValue *new = arenaZalloc(sizeof(AstValue));
  new->type = VALUE_EXPRESSION;
  new->data.expression = expression;
  return new;
}

AstValue *astValueFromFunctionCall(AstFunctionCall *functionCall) {
  AstValue *new = arenaZalloc(sizeof(AstValue));
  new->type = VALUE_FUNCTION_CALL;
  new->data.functionCall = functionCall;
  return new;
}

void astProgramAppendFunction(AstProgram *program, AstFunction *function) {
  if (program == NULL)
    die(ERR_INTERNAL, 0, "Unable to append function to NULL program");

  if (program->firstFunction == NULL) {
    program->lastFunction = function;
    program->firstFunction = function;
    function->nextFunction = NULL;
  } else {
    program->lastFunction->nextFunction = function;
    program->lastFunction = function;
  }
}

void astFunctionAppendParameter(AstFunction *function, AstFunctionParameter *parameter) {
  if (function == NULL)
    die(ERR_INTERNAL, 0, "Unable to append parameter to NULL function");

  if (function->firstParameter == NULL) {
    function->firstParameter = parameter;
    function->lastParameter = parameter;
  } else {
    function->lastParameter->nextParameter = parameter;
    function->lastParameter = parameter;
  }
}

void astBlockAppendCommand(AstCodeBlock *block, AstCommand *command) {
  if (block == NULL)
    die(ERR_INTERNAL, 0, "Unable to append command to NULL block");

  if (block->firstCommand == NULL) {
    block->firstCommand = command;
    block->lastCommand = command;
  } else {
    block->lastCommand->nextCommand = command;
    block->lastCommand = command;
  }
}

void astCallAppendArgument(AstFunctionCall *call, AstFunctionCallArgument *argument) {
  if (call == NULL)
    die(ERR_INTERNAL, 0, "Unable to append argument to NULL call");

  if (call->firstArgument == NULL) {
    call->firstArgument = argument;
    call->lastArgument = argument;
  } else {
    call->lastArgument->nextArgument = argument;
    call->lastArgument = argument;
  }
}

// Uses arena allocator so no freeing is needed, not sure why this method was defined
void astFreeProgram(AstProgram *program) {
  (void)(program);
}
