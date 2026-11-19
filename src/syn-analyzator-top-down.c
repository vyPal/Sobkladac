/**
 * @file syn-analyzator-top-down.c
 * @brief Implementation of top-down syntax analyzer.
 * @author Martin Stovicek <xstovim00>
 */
#include "syn-analyzator-top-down.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "layout.h"
#include "syn-analyzator-bottom-up.h"

bool prolog(LinkedTlist *tokenList);                           // DONE
bool funDefs(LinkedTlist *tokenList, AstProgram *asTree);      // DONE
bool funDefsTail(LinkedTlist *tokenList, AstProgram *asTree);  // DONE
bool funDef(LinkedTlist *tokenList, AstProgram *asTree);       // DONE
bool mainAnnot(LinkedTlist *tokenList);                        // DONE
void mainNL(LinkedTlist *tokenList);                           // DONE
bool funBody(LinkedTlist *tokenList);
bool params(LinkedTlist *tokenList);
bool paramsTail(LinkedTlist *tokenList);
bool param(LinkedTlist *tokenList);
AstDataType optType(LinkedTlist *tokenList);  // DONE
AstDataType dType(LinkedTlist *tokenList);    // DONE

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
bool ifStatement(AstCodeBlock *block, LinkedTlist *tokenList);
bool whileStatement(AstCodeBlock *block, LinkedTlist *tokenList);

bool prolog(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 4);
  if (token == NULL) {
    die(ERR_SYNTAX, 0, "Missing prolog");
  }
  Token *importT = popToken(tokenList);
  Token *identT = popToken(tokenList);
  Token *dotT = popToken(tokenList);
  Token *multiplyT = popToken(tokenList);
  Token *newlineT = popToken(tokenList);
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
  bool isMain = mainAnnot(tokenList);
  if (lookAheadToken(tokenList, 1) == NULL) {
    return false;
  }
  Token *defTK = popToken(tokenList);
  Token *idTK = popToken(tokenList);
  if (getTkType(defTK) == TK_KW_DEF) {
    if (getTkType(idTK) == TK_IDENT) {
      // AstLocation location <- use getLocation(token);
      AstFunction *fun = astFunctionNew(getString(getValue(idTK)->s), getLocation(idTK));

      fun->isMain = isMain;

      Token *token = popToken(tokenList);
      if (token != NULL &&
          getTkType(token) == TK_LPAREN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        if (params(tokenList)) {
          token = popToken(tokenList);
          if (token != NULL &&
              getTkType(token) == TK_RPAREN) {  // Confirmed: (Params) // AI [gpt-6-luna]: "Can you
                                                // add token != NULL
                                                // && to all getTkType(token) in ifs"
            AstDataType returnType = optType(tokenList);
            if (returnType == UNKNOWN) {
              return false;  // incorrect return type
            }
            fun->declaredReturnType = returnType;
            token = popToken(tokenList);
            if (token != NULL &&
                getTkType(token) == TK_ASSIGN) {  // AI [gpt-6-luna]: "Can you add token != NULL &&
                                                  // to all getTkType(token) in ifs"
              return funBody(tokenList);
            } else {
              fprintf(stderr, "Missing '=' in function definition");
            }
          } else {
            fprintf(stderr, "Missing ')' in function definition");
          }
        }
      }
    }
    die(ERR_SYNTAX, getLine(defTK), "Ivalid function definition");
  }
  return false;
}

/// @brief Checks for existence of @main anotation
/// @param tokenList
/// @return result of search
bool mainAnnot(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
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
  Token *token = lookAheadToken(tokenList, 0);
  if (token != NULL &&
      getTkType(token) == TK_NEWLINE) {  // AI [gpt-6-luna]: NULL && to all getTkType(token) in ifs"
    popToken(tokenList);
    return;
  }
}

bool funBody(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL && getTkType(token) == TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token !=
                                                          // NULL && to all getTkType(token) in ifs"
    token = popToken(tokenList);
    if (token != NULL &&
        getTkType(token) == TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      if (codeBlock(tokenList)) {
        token = popToken(tokenList);
        if (token != NULL &&
            getTkType(token) == TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                              // all getTkType(token) in ifs"
          return true;
        }
      }
    }
  } else {
    pushToken(token, tokenList);
    return rValue(tokenList);
  }
  return false;
}

bool params(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 1);
  if (getTkType(token) == TK_IDENT) {
    if (param(tokenList)) {
      return paramsTail(tokenList);
    } else {
      fprintf(stderr, "Incorrect function parameter");
      return false;
    }
  } else {
    return true;  // epsilon
  }
}

bool paramsTail(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL && getTkType(token) == TK_COMMA) {  // AI [gpt-6-luna]: "Can you add token !=
                                                        // NULL && to all getTkType(token) in ifs"
    if (param(tokenList)) {
      return paramsTail(tokenList);
    } else {
      fprintf(stderr, "Incorrect function parameter");
      return false;
    }
  } else {
    pushToken(token, tokenList);
    return true;  // epsilon
  }
}

bool param(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL && getTkType(token) == TK_IDENT) {  // AI [gpt-6-luna]: "Can you add token !=
                                                        // NULL && to all getTkType(token) in ifs"
    token = popToken(tokenList);
    if (token != NULL && getTkType(token) == TK_COLON) {  // AI [gpt-6-luna]: "Can you add token !=
                                                          // NULL && to all getTkType(token) in ifs"
      AstDataType type = dType(tokenList);
      if (type == UNKNOWN) {
        fprintf(stderr, "Incorrect function parameter type");
        return false;
      } else {
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
  Token *token = popToken(tokenList);

  if (token != NULL && getTkType(token) == TK_COLON) {
    AstDataType type = dType(tokenList);
    if (type == UNKNOWN) {
      fprintf(stderr, "Incorrect function type");
    }
    return type;
  }

  if (token != NULL) {
    pushToken(token, tokenList);
  }

  return ANY;  // epsilon: no type annotation
}

AstDataType dType(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token == NULL) {
    return UNKNOWN;
  }

  switch (getTkType(token)) {
    case TK_KW_INT:
      return INT;
    case TK_KW_DOUBLE:
      return DOUBLE;
    case TK_KW_STRING:
      return STRING;
    case TK_KW_UNIT:
      return UNIT;
    default:
      pushToken(token, tokenList);
      return UNKNOWN;
  }
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
  Token *token = lookAheadToken(tokenList, 0);

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
  Token *token = lookAheadToken(tokenList, 0);
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
  Token *token = lookAheadToken(tokenList, 0);

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
  Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    fprintf(stderr, "Unexpected end of file");
    return false;
  }
  int tokenType = getTkType(token);
  if (tokenType == TK_IDENT) {
    Token *nextToken = lookAheadToken(tokenList, 1);
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
    return true;
  }
}

bool assignment(AstCommand *cmd, LinkedTlist *tokenList) {
  Token *prevToken, *token = lookAheadToken(tokenList, 0);
  if (token == NULL || getTkType(token) == TK_EOF) {
    die(ERR_SYNTAX, 999, "Unexpected EOF");
  }
  if (getTkType(token) == TK_IDENT) {
    prevToken = popToken(tokenList);  // ident
    token = lookAheadToken(tokenList, 0);
    if (token != NULL &&
        getTkType(token) == TK_ASSIGN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      cmd->data.assignment.variableName = getString(getValue(token)->s),
      cmd->data.assignment.value = rValue(tokenList);
      prevToken = popToken(tokenList);
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
  AstFunctionCall *funCall;
  Token *prevToken, *token = lookAheadToken(tokenList, 0);  // ident
  if (token != NULL && getTkType(token) == TK_IDENT) {  // AI [gpt-6-luna]: "Can you add token !=
                                                        // NULL && to all getTkType(token) in ifs"
    prevToken = popToken(tokenList);
    token = lookAheadToken(tokenList, 0);  // lparen
    if (token != NULL &&
        getTkType(token) == TK_LPAREN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      funCall = astFunctionCallNew(getString(getValue(prevToken)->s), getLocation(prevToken));
      Token *nextToken = lookAheadToken(tokenList, 1);
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
  die(ERR_SYNTAX, 999, "Unexpected EOF");
  return NULL;
}

bool variableDeclaration(AstCommand *cmd, LinkedTlist *tokenList) {
  bool isMutable = false;
  if (varKW(tokenList, &isMutable)) {
    Token *token = popToken(tokenList);
    if (token != NULL && getTkType(token) == TK_IDENT) {
      cmd->data.variableDeclaration.variableName = getString(getValue(token)->s);

      AstDataType dtype = optType(tokenList);
      if (dtype == UNKNOWN) {
        die(ERR_SYNTAX, getLine(token), "Incorrect variable type");
        return false;
      }

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
  Token *token = lookAheadToken(tokenList, 0);
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
  return true;
}

bool returnStatement(AstCommand *cmd, LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL &&
      getTkType(token) == TK_KW_RETURN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
    return returnTail(cmd, tokenList);
  }
  return false;
}

bool returnTail(AstCommand *cmd, LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);

  if (token == NULL || getTkType(token) == TK_NEWLINE || getTkType(token) == TK_SEMICOLON ||
      getTkType(token) == TK_DEDENT || getTkType(token) == TK_EOF) {
    AstExpression *returnsNothing = astExpressionNew(UNIT_LITERAL, cmd->location);
    cmd->data.returnAst.value = astValueFromExpr(returnsNothing);
  }

  cmd->data.returnAst.value = rValue(tokenList);
  return true;
}

// DONE
AstValue *rValue(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return NULL;
  }
  int tokenType = getTkType(token);
  if (tokenType == TK_IDENT) {
    Token *nextToken = lookAheadToken(tokenList, 1);
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
  Token *token = lookAheadToken(tokenList, 0);
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
  Token *token = lookAheadToken(tokenList, 0);
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
    } else {  // missing )
      die(ERR_SYNTAX, getLine(token), "Missing ')' for UNIT_LITERAL");
    }
  }
  // it is not variable nor term -> Error
  else {
    die(ERR_SYNTAX, getLine(token), "Incorrect input");
  }
  return NULL;
}

bool ifStatement(AstCommand *cmd, LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  int tkType = getTkType(token);
  if (tkType != TK_KW_IF) {
    die(ERR_SYNTAX, getLine(token), "Missing 'if' in if statement");
  }
  popToken(tokenList);
  if (expression(tokenList)) {
    token = popToken(tokenList);
    if (token == NULL ||
        getTkType(token) != TK_KW_THEN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing 'then' in if statement");
    }
    token = popToken(tokenList);
    if (token == NULL ||
        getTkType(token) != TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing '\\n' in if statement");
    }
    token = popToken(tokenList);
    if (token == NULL ||
        getTkType(token) != TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing indentation in if statement");
    }
    if (codeBlock(tokenList)) {
      token = popToken(tokenList);
      if (token == NULL ||
          getTkType(token) != TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token),
            "Missing dedentation (inversion of indentation) in if statement");
      }
      token = popToken(tokenList);
      if (token == NULL ||
          getTkType(token) != TK_KW_ELSE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                             // all getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token), "Missing 'else' in if statement");
      }
      token = popToken(tokenList);
      if (token == NULL ||
          getTkType(token) != TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                             // all getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token), "Missing '\\n' in else branch");
      }
      token = popToken(tokenList);
      if (token == NULL ||
          getTkType(token) != TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token), "Missing indentation in else branch");
      }
      if (codeBlock(tokenList)) {
        token = popToken(tokenList);
        if (token == NULL ||
            getTkType(token) != TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                              // all getTkType(token) in ifs"
          die(ERR_SYNTAX, getLine(token), "Missing dedentation in else branch");
        }
        return true;
      }
    }
  }
  return false;
}

bool whileStatement(AstCommand *cmd, LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  int tkType = getTkType(token);
  if (tkType != TK_KW_WHILE) {
    die(ERR_SYNTAX, getLine(token), "Missing 'while' in while statement");
  }
  if (expression(tokenList)) {
    token = popToken(tokenList);
    if (token == NULL || getTkType(token) != TK_KW_DO) {  // AI [gpt-6-luna]: "Can you add token !=
                                                          // NULL && to all getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing 'do' in while statement");
    }
    token = popToken(tokenList);
    if (token == NULL ||
        getTkType(token) != TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing '\\n' in while statement");
    }
    token = popToken(tokenList);
    if (token == NULL ||
        getTkType(token) != TK_INDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      die(ERR_SYNTAX, getLine(token), "Missing indentation in while statement");
    }
    if (codeBlock(tokenList)) {
      token = popToken(tokenList);
      if (token == NULL ||
          getTkType(token) != TK_DEDENT) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        die(ERR_SYNTAX, getLine(token),
            "Missing dedentation (inversion of indentation) in while statement");
      }
      return true;
    }
  }
  return false;
}

AstProgram *topDown(struct LinkedTlist *tokenList) {
  tokenList = layoutFilter(tokenList);

  AstProgram *asTree = astProgramNew();
  bool result = prolog(tokenList) && funDefs(tokenList, asTree);
  Token *lastToken = popToken(tokenList);
  result = result && (lastToken != NULL && getTkType(lastToken) == TK_EOF);
  if (!result) {
    die(ERR_SYNTAX, 0, "Invalid code");
    return NULL;
  }
  return asTree;
}
