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

bool prolog(LinkedTlist *tokenList);
bool funDefs(LinkedTlist *tokenList, AstProgram *asTree);
bool funDefsTail(LinkedTlist *tokenList, AstProgram *asTree);
bool funDef(LinkedTlist *tokenList, AstProgram *asTree);
bool mainAnnot(LinkedTlist *tokenList);
void mainNL(LinkedTlist *tokenList);
bool funBody(LinkedTlist *tokenList);
bool params(LinkedTlist *tokenList);
bool paramsTail(LinkedTlist *tokenList);
bool param(LinkedTlist *tokenList);
AstDataType optType(LinkedTlist *tokenList);
AstDataType dType(LinkedTlist *tokenList);

bool codeBlock(LinkedTlist *tokenList);
bool linesTail(LinkedTlist *tokenList);
bool line(LinkedTlist *tokenList);

bool simpleTail(LinkedTlist *tokenList);
bool simpleStatement(LinkedTlist *tokenList);
bool assignment(AstCommand *cmd, LinkedTlist *tokenList);  // DONE
AstFunctionCall *functionCall(LinkedTlist *tokenList);     // DONE
bool variableDeclaration(LinkedTlist *tokenList);
bool varKW(LinkedTlist *tokenList, bool *isMutable);  // DONE
bool returnStatement(LinkedTlist *tokenList);
bool returnTail(LinkedTlist *tokenList);
AstValue *rValue(LinkedTlist *tokenList);                         // DONE
bool args(AstFunctionCall *funCall, LinkedTlist *tokenList);      // DONE
bool argsTail(AstFunctionCall *funCall, LinkedTlist *tokenList);  // DONE
AstExpression *term(LinkedTlist *tokenList);                      // DONE
bool ifStatement(LinkedTlist *tokenList);
bool whileStatement(LinkedTlist *tokenList);

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
  Token *defTK = popToken(tokenList);
  Token *idTK = popToken(tokenList);
  if (getTkType(defTK) == TK_KW_DEF && getTkType(idTK) == TK_IDENT) {
    // AstLocation location = {.line = getLine(idTK), .column = 1}; // I don't know percise location
    AstFunction *fun = malloc(sizeof(AstFunction));
    if (fun == NULL) {
      die(ERR_INTERNAL, 0, "Memory allocation failed");
    }
    if (asTree->firstFunction == NULL) {
      asTree->firstFunction = fun;
    } else {
      asTree->lastFunction = fun;
    }

    fun->isMain = isMain;

    Token *token = popToken(tokenList);
    if (token != NULL &&
        getTkType(token) == TK_LPAREN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                          // getTkType(token) in ifs"
      if (params(tokenList)) {
        token = popToken(tokenList);
        if (token != NULL &&
            getTkType(token) ==
                TK_RPAREN) {  // Confirmed: (Params) // AI [gpt-6-luna]: "Can you add token != NULL
                              // && to all getTkType(token) in ifs"
          AstDataType returnType = optType(tokenList);
          if (returnType == UNKNOWN) {
            return false;  // incorrect return type
          }
          fun->declaredReturnType = returnType;
          token = popToken(tokenList);
          if (token != NULL &&
              getTkType(token) == TK_ASSIGN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to
                                                // all getTkType(token) in ifs"
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
  return false;
}

/// @brief Checks for existence of @main anotation
/// @param tokenList
/// @return result of search
bool mainAnnot(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL && getTkType(token) == TK_ATMAIN) {  // AI [gpt-6-luna]: "Can you add token !=
                                                         // NULL && to all getTkType(token) in ifs"
    mainNL(tokenList);
    return true;
  } else {
    pushToken(token, tokenList);
    return false;
  }
}

/// @brief If anotation @main is on another line, it takes newline, otherwise does nothing
/// @param tokenList
void mainNL(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL && getTkType(token) == TK_NEWLINE) {  // AI [gpt-6-luna]: "Can you add token !=
                                                          // NULL && to all getTkType(token) in ifs"
    return;
  } else {
    pushToken(token, tokenList);
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
  if (lookAheadToken(tokenList, 1) == TK_IDENT) {
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
  if (token != NULL && getTkType(token) == TK_COLON) {  // AI [gpt-6-luna]: "Can you add token !=
                                                        // NULL && to all getTkType(token) in ifs"
    if (dType(tokenList) == UNKNOWN) {
      fprintf(stderr, "Incorrect Function type");
    }
    return dType(tokenList);
  } else
    pushToken(token, tokenList);
  return ANY;  // epsilon
}

AstDataType dType(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  switch (getTkType(token)) {
    case TK_KW_INT:
      return INT;
      break;
    case TK_KW_DOUBLE:
      return DOUBLE;
      break;
    case TK_KW_STRING:
      return STRING;
      break;
    case TK_KW_UNIT:
      return UNIT;
      break;
    default:
      return UNKNOWN;
      break;
  }
}

// ---- Statements ----

bool codeBlock(LinkedTlist *tokenList) {
  if (line(tokenList)) {
    return linesTail(tokenList);
  } else {
    return false;  // no lines in code block
  }
}

bool linesTail(LinkedTlist *tokenList) {
  if (line(tokenList)) {
    return linesTail(tokenList);
  } else {
    return true;  // epsilon
  }
}

bool line(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return false;
  }
  if (getTkType(token) == TK_KW_IF) {
    return ifStatement(tokenList);
  } else if (getTkType(token) == TK_KW_WHILE) {
    return whileStatement(tokenList);
  } else if (simpleStatement(tokenList)) {
    if (simpleTail(tokenList)) {
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

bool simpleTail(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL &&
      getTkType(token) == TK_SEMICOLON) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
    if (simpleStatement(tokenList)) {
      return simpleTail(tokenList);
    }
  } else {
    pushToken(token, tokenList);
    return true;  // epsilon
  }
  return false;  // No statement after semicolon
}

bool simpleStatement(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    fprintf(stderr, "Unexpected end of file");
    return false;
  }
  int tokenType = getTkType(token);
  if (tokenType == TK_IDENT) {
    Token *nextToken = lookAheadToken(tokenList, 1);
    if (nextToken != NULL && getTkType(nextToken) == TK_ASSIGN) {
      return assignment(tokenList);
    }
    if (nextToken != NULL && getTkType(nextToken) == TK_LPAREN) {
      return functionCall(tokenList);
    }
    return expression(tokenList);
  } else if (tokenType == TK_KW_RETURN) {
    return returnStatement(tokenList);
  } else if (tokenType == TK_KW_VAL || tokenType == TK_KW_VAR) {
    return variableDeclaration(tokenList);
  } else {
    return expression(tokenList);
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

bool variableDeclaration(LinkedTlist *tokenList) {
  bool isMutable = false;
  if (varKW(tokenList, &isMutable)) {
    popToken(tokenList);  // pop the found var/val
    Token *token = popToken(tokenList);
    if (token != NULL && getTkType(token) == TK_IDENT) {  // AI [gpt-6-luna]: "Can you add token !=
                                                          // NULL && to all getTkType(token) in ifs"
      AstDataType dtype = optType(tokenList);
      if (dtype == UNKNOWN) {
        fprintf(stderr, "Incorrect variable type");
        return false;
      }
      token = popToken(tokenList);
      if (token != NULL &&
          getTkType(token) == TK_ASSIGN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                            // getTkType(token) in ifs"
        return rValue(tokenList);
      } else {
        die(ERR_SYNTAX, getLine(token), "Missing '=' in function call");
      }
    } else {
      die(ERR_SYNTAX, getLine(token), "Missing identifier in function call");
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
  return false;
}

bool returnStatement(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (token != NULL &&
      getTkType(token) == TK_KW_RETURN) {  // AI [gpt-6-luna]: "Can you add token != NULL && to all
                                           // getTkType(token) in ifs"
    return returnTail(tokenList);
  }
  return false;
}

bool returnTail(LinkedTlist *tokenList) {
  return rValue(tokenList) || true;  // epsilon
}

// DONE
AstValue *rValue(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return false;
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

bool ifStatement(LinkedTlist *tokenList) {
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

bool whileStatement(LinkedTlist *tokenList) {
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

  AstProgram *asTree = malloc(sizeof(AstProgram));
  if (asTree == NULL) {
    die(ERR_INTERNAL, 0, "Memory allocation failed");
  }
  bool result = prolog(tokenList) && funDefs(tokenList, asTree);
  Token *lastToken = popToken(tokenList);
  result = result && (getTkType(lastToken) == TK_EOF);
  if (!result) {
    free(asTree);
    return NULL;
  }
  return asTree;
}
