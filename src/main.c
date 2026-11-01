/**
 * @file main.c
 * Project: IFJ26 compiler
 * @author Jakub Palacky <xpalacj00>
 */

#include <stdio.h>

#include "ast.h"
#include "codegen.h"
#include "common.h"

int main(void) {
  AstExpression exp1 = {.expressionType = INT_LITERAL, .data.intValue = 1};
  AstValue val1 = {.type = VALUE_EXPRESSION, .data.expression = &exp1};
  AstCommand cmd1 = {.type = VARIABLE_DECLARATION,
                     .data.variableDeclaration = {.variableName = "a", .value = &val1}};

  AstExpression exp2 = {.expressionType = INT_LITERAL, .data.intValue = 2};
  AstValue val2 = {.type = VALUE_EXPRESSION, .data.expression = &exp2};
  AstCommand cmd2 = {.type = VARIABLE_DECLARATION,
                     .data.variableDeclaration = {.variableName = "b", .value = &val2}};
  cmd1.nextCommand = &cmd2;

  AstExpression lexp3 = {.expressionType = VARIABLE, .data.variable = "a", .deducedDataType = INT};
  AstExpression rexp3 = {.expressionType = VARIABLE, .data.variable = "b", .deducedDataType = INT};
  AstExpression exp3 = {
      .expressionType = BINARY,
      .data.binary = {
          .leftExpression = &lexp3, .rightExpression = &rexp3, .operatorAst = ADDITION}};

  AstFunctionCallArgument arg3 = {.expression = &exp3};
  AstFunctionCall call3 = {.functionName = "print", .firstArgument = &arg3};
  AstCommand cmd3 = {.type = FUNCTION_CALL, .data.call.functionCall = &call3};
  cmd2.nextCommand = &cmd3;

  AstExpression exp4 = {.expressionType = UNIT_LITERAL};
  AstValue val4 = {.type = VALUE_EXPRESSION, .data.expression = &exp4};
  AstCommand cmd4 = {.type = RETURN, .data.returnAst.value = &val4, .nextCommand = NULL};
  cmd3.nextCommand = &cmd4;

  AstCodeBlock block1 = {.firstCommand = &cmd1};
  AstFunction func1 = {.functionName = "main",
                       .isMain = true,
                       .firstParameter = NULL,
                       .bodyType = CODE_BLOCK,
                       .body.codeBlock = &block1,
                       .nextFunction = NULL};
  AstProgram test = {.firstFunction = &func1};

  printf("%s", generateCode(test));

  arenaCleanup();
  return ERR_OK;
}
