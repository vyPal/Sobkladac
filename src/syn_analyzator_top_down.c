/**
 * @file syn_analyzator_top_down.c
 * @brief Implementation of top-down syntax analyzer.
 * @author Martin Stovicek <xstovim00>
 */
#include "syn_analyzator_top_down.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "layout.h"
#include "syn_analyzator_bottom_up.h"

bool prolog(LinkedTlist *tokenList);                             // DONE
bool funDefs(LinkedTlist *tokenList, AstProgram *asTree);        // DONE
bool funDefsTail(LinkedTlist *tokenList, AstProgram *asTree);    // DONE
bool funDef(LinkedTlist *tokenList, AstProgram *asTree);         // DONE
bool mainAnnot(LinkedTlist *tokenList);                          // DONE
void mainNL(LinkedTlist *tokenList);                             // DONE
bool funBody(AstFunction *function, LinkedTlist *tokenList);     // DONE
bool params(AstFunction *function, LinkedTlist *tokenList);      // DONE
bool paramsTail(AstFunction *function, LinkedTlist *tokenList);  // DONE
bool param(AstFunction *function, LinkedTlist *tokenList);       // DONE
AstDataType optType(LinkedTlist *tokenList);                     // DONE
AstDataType dType(LinkedTlist *tokenList);                       // DONE

bool codeBlock(AstCodeBlock *block, LinkedTlist *tokenList);  // DONE
bool linesTail(AstCodeBlock *block, LinkedTlist *tokenList);  // DONE
bool line(AstCodeBlock *block, LinkedTlist *tokenList);       // DONE

bool simpleTail(AstCodeBlock *block, LinkedTlist *tokenList);       // DONE
bool simpleStatement(AstCodeBlock *block, LinkedTlist *tokenList);  // DONE
bool assignment(AstCommand *cmd, LinkedTlist *tokenList);           // DONE
AstFunctionCall *functionCall(LinkedTlist *tokenList);              // DONE
bool variableDeclaration(AstCommand *cmd, LinkedTlist *tokenList);  // DONE
bool varKW(LinkedTlist *tokenList, bool *isMutable);                // DONE
bool returnStatement(AstCommand *cmd, LinkedTlist *tokenList);      // DONE
bool returnTail(AstCommand *cmd, LinkedTlist *tokenList);           // DONE
AstValue *rValue(LinkedTlist *tokenList);                           // DONE
bool args(AstFunctionCall *funCall, LinkedTlist *tokenList);        // DONE
bool argsTail(AstFunctionCall *funCall, LinkedTlist *tokenList);    // DONE
AstExpression *term(LinkedTlist *tokenList);                        // DONE
bool ifStatement(AstCodeBlock *block, LinkedTlist *tokenList);      // DONE
bool whileStatement(AstCodeBlock *block, LinkedTlist *tokenList);   // DONE

static int lineOf(const Token *token) {
  return token ? getLine(token) : 999;
}

bool prolog(LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 4);
  if (token == NULL) {
    die(ERR_SYNTAX, 0, "Missing prolog");
  }
  const Token *importT = popToken(tokenList);
  const Token *identT = popToken(tokenList);
  const Token *dotT = popToken(tokenList);
  const Token *multiplyT = popToken(tokenList);
  const Token *newlineT = popToken(tokenList);
  if (getTkType(importT) == TK_KW_IMPORT &&  // import
      getTkType(identT) == TK_IDENT &&
      strcmp(getValue(identT)->s->string, "ifj26") == 0 &&  // ifj26
      getTkType(dotT) == TK_DOT &&                          //.
      getTkType(multiplyT) == TK_MULTIPLY &&                //*
      getTkType(newlineT) == TK_NEWLINE                     // newline
  ) {
    return true;  // together it matches line: import ifj26.*
  } else
    return false;
}

bool funDefs(LinkedTlist *tokenList, AstProgram *asTree) {
  if (funDef(tokenList, asTree)) {
    return funDefsTail(tokenList, asTree);
  } else
    return false;  // functions missing
}

bool funDefsTail(LinkedTlist *tokenList, AstProgram *asTree) {
  if (funDef(tokenList, asTree)) {
    return funDefsTail(tokenList, asTree);
  } else
    return true;  // epsilon
}

bool funDef(LinkedTlist *tokenList, AstProgram *asTree) {
  // skip blank lines before a function if your layout filter leaves them
  const Token *peek = lookAheadToken(tokenList, 0);
  if (peek == NULL || getTkType(peek) == TK_EOF)
    return false;

  bool isMain = mainAnnot(tokenList);

  const Token *defTK = popToken(tokenList);
  if (defTK == NULL || getTkType(defTK) != TK_KW_DEF) {
    die(ERR_SYNTAX, defTK ? getLine(defTK) : 999, "Expected 'def'");
    return false;
  }
  const Token *idTK = popToken(tokenList);
  if (idTK == NULL || getTkType(idTK) != TK_IDENT)
    die(ERR_SYNTAX, getLine(defTK), "Missing function name");

  AstFunction *fun = astFunctionNew(getString(getValue(idTK)->s), getLocation(idTK));
  fun->isMain = isMain;

  const Token *t = popToken(tokenList);
  if (t == NULL || getTkType(t) != TK_LPAREN)
    die(ERR_SYNTAX, getLine(idTK), "Missing '(' in function definition");

  if (!params(fun, tokenList))
    die(ERR_SYNTAX, getLine(idTK), "Invalid parameters");

  t = popToken(tokenList);
  if (t == NULL || getTkType(t) != TK_RPAREN)
    die(ERR_SYNTAX, getLine(idTK), "Missing ')' in function definition");

  fun->declaredReturnType = optType(tokenList);

  t = popToken(tokenList);
  if (t == NULL || getTkType(t) != TK_ASSIGN)
    die(ERR_SYNTAX, getLine(idTK), "Missing '=' in function definition");

  if (!funBody(fun, tokenList))
    die(ERR_SYNTAX, getLine(idTK), "Invalid function body");

  astProgramAppendFunction(asTree, fun);
  return true;
}

/// @brief Checks for existence of @main anotation
/// @param tokenList
/// @return result of search
bool mainAnnot(LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token != NULL && getTkType(token) == TK_ATMAIN) {  // AI [gpt-6-luna]: "Can you add token !=
                                                         // NULL && to all getTkType(token) in ifs"
    popToken(tokenList);
    mainNL(tokenList);
    return true;
  } else {
    return false;
  }
}

/// @brief If anotation @main is on another line, it takes newline, otherwise does nothing
/// @param tokenList
void mainNL(LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token != NULL &&
      getTkType(token) == TK_NEWLINE) {  // AI [gpt-6-luna]: NULL && to all getTkType(token) in ifs"
    popToken(tokenList);
    return;
  }
}

bool funBody(AstFunction *function, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token != NULL && getTkType(token) == TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token !=
                                                          // NULL && to all getTkType(token) in ifs"
    popToken(tokenList);                                  // pop newline
    token = popToken(tokenList);                          // pop new token
    if (token != NULL &&
        getTkType(token) == TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"

      function->bodyType = CODE_BLOCK;
      function->body.codeBlock = astCodeBlockNew();
      if (codeBlock(function->body.codeBlock, tokenList)) {
        token = popToken(tokenList);
        if (token != NULL &&
            getTkType(token) == TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                              // all getTkType(token) in ifs"
          return true;
        }
      }
    }
  } else {
    function->bodyType = VALUE;
    function->body.value = rValue(tokenList);
    // layoutFilter terminates every logical line with a NEWLINE; consume it
    token = lookAheadToken(tokenList, 0);
    if (token != NULL && getTkType(token) == TK_NEWLINE)
      popToken(tokenList);
    return true;
  }
  return false;
}

bool params(AstFunction *function, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token != NULL && getTkType(token) == TK_IDENT) {
    if (param(function, tokenList)) {
      return paramsTail(function, tokenList);
    } else {
      fprintf(stderr, "Incorrect function parameter");
      return false;
    }
  } else {
    return true;  // epsilon
  }
}

bool paramsTail(AstFunction *function, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected EOF");
  }
  if (getTkType(token) == TK_COMMA) {  // AI [gpt-6-luna]: "Can you add token !=
                                       // NULL && to all getTkType(token) in ifs"
    popToken((tokenList));             // pop comma
    if (param(function, tokenList)) {
      return paramsTail(function, tokenList);
    } else {
      fprintf(stderr, "Incorrect function parameter");
      return false;
    }
  } else {
    return true;  // epsilon
  }
}

bool param(AstFunction *function, LinkedTlist *tokenList) {
  const Token *token = popToken(tokenList);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected EOF");
  }
  if (getTkType(token) == TK_IDENT) {  // AI [gpt-6-luna]: "Can you add token !=
                                       // NULL && to all getTkType(token) in ifs"
    const char *name = getString(getValue(token)->s);
    token = popToken(tokenList);
    if (token != NULL && getTkType(token) == TK_COLON) {  // AI [gpt-6-luna]: "Can you add token !=
                                                          // NULL && to all getTkType(token) in ifs"
      AstDataType type = dType(tokenList);
      if (type == UNKNOWN) {
        fprintf(stderr, "Incorrect function parameter type");
        return false;
      } else {
        AstFunctionParameter *par = astFunctionParameterNew(name, type);
        if (par == NULL)
          return false;

        astFunctionAppendParameter(function, par);
        return true;
      }
    } else {
      fprintf(stderr, "Missing ':' in function parameter");
      return false;
    }
  } else {
    fprintf(stderr, "Missing identifier in function parameter");
    return false;
  }
}

AstDataType optType(LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);

  if (token != NULL && getTkType(token) == TK_COLON) {
    popToken(tokenList);
    AstDataType type = dType(tokenList);
    if (type == UNKNOWN) {
      die(ERR_SYNTAX, lineOf(token), "Incorrect type after ':'");
    }
    return type;
  }

  return UNKNOWN;  // epsilon: no type annotation (semantic analysis infers it)
}

AstDataType dType(LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    return UNKNOWN;
  }

  AstDataType type;
  switch (getTkType(token)) {
    case TK_KW_INT:
      type = INT;
      break;
    case TK_KW_DOUBLE:
      type = DOUBLE;
      break;
    case TK_KW_STRING:
      type = STRING;
      break;
    case TK_KW_UNIT:
      type = UNIT;
      break;
    default:
      return UNKNOWN;  // leave the token in the list
  }
  popToken(tokenList);
  return type;
}

// ---- Statements ----

bool codeBlock(AstCodeBlock *block, LinkedTlist *tokenList) {
  if (line(block, tokenList)) {
    return linesTail(block, tokenList);
  } else {
    return false;  // no lines in code block
  }
}

bool linesTail(AstCodeBlock *block, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);

  // The block ends when its indentation ends.
  if (token == NULL || getTkType(token) == TK_DEDENT) {
    return true;  // epsilon
  }

  if (line(block, tokenList)) {
    return linesTail(block, tokenList);
  }

  return false;
}
bool line(AstCodeBlock *block, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return false;
  }

  if (getTkType(token) == TK_KW_IF) {
    return ifStatement(block, tokenList);
  } else if (getTkType(token) == TK_KW_WHILE) {
    return whileStatement(block, tokenList);
  } else if (simpleStatement(block, tokenList)) {
    if (simpleTail(block, tokenList)) {
      token = popToken(tokenList);
      if (token != NULL &&
          getTkType(token) == TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                             // all getTkType(token) in ifs"
        return true;
      }
    }
  }
  fprintf(stderr, "Invalid statement");
  return false;
}

bool simpleTail(AstCodeBlock *block, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);

  if (token != NULL && getTkType(token) == TK_SEMICOLON) {
    popToken(tokenList);
    if (simpleStatement(block, tokenList)) {
      return simpleTail(block, tokenList);
    }
    return false;  // No statement after semicolon
  }

  return true;  // epsilon
}

bool simpleStatement(AstCodeBlock *block, LinkedTlist *tokenList) {
  AstCommand *cmd;
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    fprintf(stderr, "Unexpected end of file");
    return false;
  }
  int tokenType = getTkType(token);
  if (tokenType == TK_IDENT) {
    const Token *nextToken = lookAheadToken(tokenList, 1);
    if (nextToken != NULL && getTkType(nextToken) == TK_ASSIGN) {
      cmd = astCodeCommandNew(ASSIGNMENT, getLocation(token));
      if (!assignment(cmd, tokenList)) {
        return false;
      }
      astBlockAppendCommand(block, cmd);
      return true;
    }
    if (nextToken != NULL && getTkType(nextToken) == TK_LPAREN) {
      cmd = astCodeCommandNew(FUNCTION_CALL, getLocation(token));
      cmd->data.call.functionCall = functionCall(tokenList);
      astBlockAppendCommand(block, cmd);
      return true;
    }
    cmd = astCodeCommandNew(EXPRESSION, getLocation(token));
    cmd->data.expressionAst.expression = expression(tokenList);
    astBlockAppendCommand(block, cmd);
    return true;
  } else if (tokenType == TK_KW_RETURN) {
    cmd = astCodeCommandNew(RETURN, getLocation(token));
    if (!returnStatement(cmd, tokenList)) {
      return false;
    }
    astBlockAppendCommand(block, cmd);
    return true;
  } else if (tokenType == TK_KW_VAL || tokenType == TK_KW_VAR) {
    cmd = astCodeCommandNew(VARIABLE_DECLARATION, getLocation(token));
    if (!variableDeclaration(cmd, tokenList)) {
      return false;
    }
    astBlockAppendCommand(block, cmd);
    return true;

  } else {
    cmd = astCodeCommandNew(EXPRESSION, getLocation(token));
    cmd->data.expressionAst.expression = expression(tokenList);
    astBlockAppendCommand(block, cmd);
    return true;
  }
}

bool assignment(AstCommand *cmd, LinkedTlist *tokenList) {
  const Token *prevToken, *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected EOF");
  }
  if (getTkType(token) == TK_IDENT) {
    prevToken = popToken(tokenList);  // ident
    token = lookAheadToken(tokenList, 0);
    if (token != NULL &&
        getTkType(token) == TK_ASSIGN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      cmd->data.assignment.variableName = getString(getValue(prevToken)->s);
      popToken(tokenList);
      cmd->data.assignment.value = rValue(tokenList);
      return true;
    } else {
      die(ERR_SYNTAX, getLine(prevToken), "Missing '=' in assignment");
      return false;
    }
  }
  die(ERR_SYNTAX, getLine(token), "Incorect input for assignment");
  return false;
}

AstFunctionCall *functionCall(LinkedTlist *tokenList) {
  const Token *prevToken, *token = lookAheadToken(tokenList, 0);  // ident
  if (token != NULL && getTkType(token) == TK_IDENT) {  // AI [gpt-6-luna]: "Can you add token !=
                                                        // NULL && to all getTkType(token) in ifs"
    prevToken = popToken(tokenList);
    token = lookAheadToken(tokenList, 0);  // lparen
    if (token != NULL &&
        getTkType(token) == TK_LPAREN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      popToken(tokenList);
      AstFunctionCall *funCall =
          astFunctionCallNew(getString(getValue(prevToken)->s), getLocation(prevToken));
      const Token *nextToken = lookAheadToken(tokenList, 0);
      if ((nextToken != NULL && getTkType(nextToken) == TK_RPAREN) ||
          args(funCall, tokenList)) {  // It skips args if there are none
        token = popToken(tokenList);   // rparen
        if (token != NULL &&
            getTkType(token) == TK_RPAREN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                              // all getTkType(token) in ifs"
          return funCall;
        } else {
          die(ERR_SYNTAX, getLine(prevToken), "Missing ')' in function call");
          return NULL;
        }
      }
    } else {
      die(ERR_SYNTAX, getLine(prevToken), "Missing '(' in function call");
      return NULL;
    }
  }
  die(ERR_SYNTAX, lineOf(token), "Unexpected EOF");
  return NULL;
}

bool variableDeclaration(AstCommand *cmd, LinkedTlist *tokenList) {
  bool isMutable = false;
  if (varKW(tokenList, &isMutable)) {
    const Token *token = popToken(tokenList);
    if (token != NULL && getTkType(token) == TK_IDENT) {
      cmd->data.variableDeclaration.variableName = getString(getValue(token)->s);

      AstDataType dtype = optType(tokenList);

      cmd->data.variableDeclaration.isMutable = isMutable;
      cmd->data.variableDeclaration.declaredType = dtype;
      cmd->data.variableDeclaration.resolvedType = UNKNOWN;

      token = popToken(tokenList);
      if (token != NULL && getTkType(token) == TK_ASSIGN) {
        cmd->data.variableDeclaration.value = rValue(tokenList);
        if (cmd->data.variableDeclaration.value != NULL) {
          return true;
        }
        return false;
      } else {
        die(ERR_SYNTAX, token != NULL ? getLine(token) : 999,
            "Missing '=' in variable declaration");
      }
    } else {
      die(ERR_SYNTAX, token != NULL ? getLine(token) : 999,
          "Missing identifier in variable declaration");
    }
  }
  return false;  // it didn't start with var/val
}

bool varKW(LinkedTlist *tokenList, bool *isMutable) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected EOF");
  }
  int tkType = getTkType(token);
  if (tkType == TK_KW_VAR)
    *isMutable = true;
  else if (tkType == TK_KW_VAL)
    *isMutable = false;
  else
    return false;
  popToken(tokenList);  // pop found token
  return true;
}

bool returnStatement(AstCommand *cmd, LinkedTlist *tokenList) {
  const Token *token = popToken(tokenList);
  if (token != NULL &&
      getTkType(token) == TK_KW_RETURN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
    return returnTail(cmd, tokenList);
  }
  return false;
}

bool returnTail(AstCommand *cmd, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);

  if (token == NULL || getTkType(token) == TK_NEWLINE || getTkType(token) == TK_SEMICOLON ||
      getTkType(token) == TK_DEDENT || getTkType(token) == TK_EOF) {
    AstExpression *returnsNothing = astExpressionNew(UNIT_LITERAL, cmd->location);
    cmd->data.returnAst.value = astValueFromExpr(returnsNothing);
    return true;
  }

  cmd->data.returnAst.value = rValue(tokenList);
  return true;
}

// DONE
AstValue *rValue(LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return NULL;
  }
  int tokenType = getTkType(token);
  if (tokenType == TK_IDENT) {
    const Token *nextToken = lookAheadToken(tokenList, 1);
    if (nextToken != NULL && getTkType(nextToken) == TK_LPAREN) {
      return astValueFromFunctionCall(functionCall(tokenList));
    }
  }
  return astValueFromExpr(expression(tokenList));
}

// DONE
bool args(AstFunctionCall *funCall, LinkedTlist *tokenList) {  // there must be some args
  AstExpression *arg = term(tokenList);
  if (arg != NULL) {
    astCallAppendArgument(funCall, astFunctionCallArgumentNew(arg));
    return argsTail(funCall, tokenList);
  } else {
    return false;
  }
}

bool argsTail(AstFunctionCall *funCall, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token != NULL && getTkType(token) == TK_COMMA) {  // AI [gpt-6-luna]: "Can you add token !=
                                                        // NULL && to all getTkType(token) in ifs"
    popToken(tokenList);
    AstExpression *arg = term(tokenList);
    if (arg != NULL) {
      astCallAppendArgument(funCall, astFunctionCallArgumentNew(arg));
      return argsTail(funCall, tokenList);
    }
  } else {
    return true;  // epsilon
  }
  return false;  // term failed after comma
}

AstExpression *term(LinkedTlist *tokenList) {
  AstExpression *term;
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
  }
  int tkType = getTkType(token);
  if (tkType == TK_IDENT) {  // it is variable
    popToken(tokenList);
    term = astExpressionNew(VARIABLE, getLocation(token));
    term->data.variable = getString(getValue(token)->s);
    return term;

    // it is INT_LITERAL
  } else if (tkType == TK_NUMBER) {
    popToken(tokenList);
    term = astExpressionNew(INT_LITERAL, getLocation(token));
    term->data.intValue = getValue(token)->i;
    return term;

    // it is DOUBLE_LITERAL
  } else if (tkType == TK_DECIMAL) {
    popToken(tokenList);
    term = astExpressionNew(DOUBLE_LITERAL, getLocation(token));
    term->data.doubleValue = getValue(token)->d;
    return term;

    // it is STRING_LITERAL
  } else if (tkType == TK_STRING) {
    popToken(tokenList);
    term = astExpressionNew(STRING_LITERAL, getLocation(token));
    term->data.stringValue = getString(getValue(token)->s);
    return term;

  } else if (tkType == TK_LPAREN) {
    popToken(tokenList);
    token = lookAheadToken(tokenList, 0);
    if (token != NULL &&
        getTkType(token) == TK_RPAREN) {  // Confirmed: UNIT_LITERAL // AI [gpt-6-luna]: "Can you
                                          // add token != NULL && to all getTkType(token) in ifs"
      popToken(tokenList);
      term = astExpressionNew(UNIT_LITERAL, getLocation(token));
      return term;
    } else if (token != NULL)  // missing )
      die(ERR_SYNTAX, getLine(token), "Missing ')' for UNIT_LITERAL");
    else
      die(ERR_SYNTAX, 0, "Missing ')' for UNIT_LITERAL");
  }
  // it is not variable nor term -> Error
  else {
    die(ERR_SYNTAX, getLine(token), "Incorrect input");
  }
  return NULL;
}

bool ifStatement(AstCodeBlock *block, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return NULL;
  }

  int tkType = getTkType(token);
  if (tkType != TK_KW_IF) {
    die(ERR_SYNTAX, getLine(token), "Missing 'if' in if statement");
  }
  AstCommand *cmd = astCodeCommandNew(IF, getLocation(token));
  popToken(tokenList);
  AstExpression *condition = expression(tokenList);
  if (condition != NULL) {
    cmd->data.ifAst.condition = condition;
    token = popToken(tokenList);
    if (token == NULL)
      die(ERR_SYNTAX, lineOf(token), "Missing 'then' in if statement");
    if (getTkType(token) != TK_KW_THEN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
      die(ERR_SYNTAX, lineOf(token), "Missing 'then' in if statement");
    }
    token = popToken(tokenList);
    if (token == NULL)
      die(ERR_SYNTAX, 0, "Missing '\\n' in if statement");
    if (getTkType(token) != TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing '\\n' in if statement");
    }
    token = popToken(tokenList);
    if (token == NULL)
      die(ERR_SYNTAX, 0, "Missing indentation in if statement");
    if (getTkType(token) != TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing indentation in if statement");
    }
    cmd->data.ifAst.thenBlock = astCodeBlockNew();
    if (codeBlock(cmd->data.ifAst.thenBlock, tokenList)) {
      token = popToken(tokenList);
      if (token == NULL)
        die(ERR_SYNTAX, 0, "Missing dedentation in if statement");
      if (getTkType(token) != TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token),
            "Missing dedentation (inversion of indentation) in if statement");
      }
      token = popToken(tokenList);
      if (token == NULL)
        die(ERR_SYNTAX, 0, "Missing 'else' in if statement");
      if (getTkType(token) != TK_KW_ELSE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                             // all getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token), "Missing 'else' in if statement");
      }
      token = popToken(tokenList);
      if (token == NULL)
        die(ERR_SYNTAX, 0, "Missing '\\n' in else branch");
      if (getTkType(token) != TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                             // all getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token), "Missing '\\n' in else branch");
      }
      token = popToken(tokenList);
      if (token == NULL)
        die(ERR_SYNTAX, 0, "Missing indentation in else branch");
      if (getTkType(token) != TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token), "Missing indentation in else branch");
      }
      cmd->data.ifAst.elseBlock = astCodeBlockNew();
      if (codeBlock(cmd->data.ifAst.elseBlock, tokenList)) {
        token = popToken(tokenList);
        if (token == NULL)
          die(ERR_SYNTAX, 0, "Missing dedentation in else branch");
        if (getTkType(token) != TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                              // all getTkType(token) in ifs"
          die(ERR_SYNTAX, lineOf(token), "Missing dedentation in else branch");
        }
        astBlockAppendCommand(block, cmd);
        return true;
      }
    }
  }
  return false;
}

bool whileStatement(AstCodeBlock *block, LinkedTlist *tokenList) {
  const Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return NULL;
  }
  int tkType = getTkType(token);
  if (tkType != TK_KW_WHILE) {
    die(ERR_SYNTAX, getLine(token), "Missing 'while' in while statement");
  }
  AstCommand *cmd = astCodeCommandNew(WHILE, getLocation(token));
  popToken(tokenList);  // pop 'while'
  AstExpression *condition = expression(tokenList);
  if (condition != NULL) {
    cmd->data.whileAst.condition = condition;
    token = popToken(tokenList);
    if (token == NULL)
      die(ERR_SYNTAX, 0, "Missing 'do' in while statement");
    if (getTkType(token) != TK_KW_DO) {  // AI [gpt-6-luna]: "Can you add token !=
                                         // NULL && to all getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing 'do' in while statement");
    }
    token = popToken(tokenList);
    if (token == NULL)
      die(ERR_SYNTAX, 0, "Missing '\\n' in while statement");
    if (getTkType(token) != TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing '\\n' in while statement");
    }
    token = popToken(tokenList);
    if (token == NULL)
      die(ERR_SYNTAX, 0, "Missing indentation in while statement");
    if (getTkType(token) != TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing indentation in while statement");
    }
    cmd->data.whileAst.codeBlock = astCodeBlockNew();
    if (codeBlock(cmd->data.whileAst.codeBlock, tokenList)) {
      token = popToken(tokenList);
      if (token == NULL)
        die(ERR_SYNTAX, 0, "Missing dedentation in while statement");
      if (getTkType(token) != TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        die(ERR_SYNTAX, lineOf(token),
            "Missing dedentation (inversion of indentation) in while statement");
      }

      astBlockAppendCommand(block, cmd);
      return true;
    }
  }
  return false;
}

AstProgram *topDown(struct LinkedTlist *tokenList) {
  tokenList = layoutFilter(tokenList);

  AstProgram *asTree = astProgramNew();
  bool result = prolog(tokenList) && funDefs(tokenList, asTree);
  const Token *lastToken = popToken(tokenList);
  result = result && (lastToken != NULL && getTkType(lastToken) == TK_EOF);
  if (!result) {
    die(ERR_SYNTAX, 0, "Invalid code");
    return NULL;
  }
  return asTree;
}
