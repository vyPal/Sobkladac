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

  AstExpression lexp3 = {.expressionType = VARIABLE, .data.variable = "a"};
  AstExpression rexp3 = {.expressionType = VARIABLE, .data.variable = "b"};
  AstExpression exp3 = {
      .expressionType = BINARY,
      .data.binary = {
          .leftExpression = &lexp3, .rightExpression = &rexp3, .operatorAst = ADDITION}};
  AstValue val3 = {.type = VALUE_EXPRESSION, .data.expression = &exp3};
  AstCommand cmd3 = {.type = RETURN, .data.returnAst.value = &val3, .nextCommand = NULL};
  cmd2.nextCommand = &cmd3;

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
