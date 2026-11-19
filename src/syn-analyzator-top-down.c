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

#include "common.h"
#include "layout.h"

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
bool assignment(LinkedTlist *tokenList);
bool functionCall(LinkedTlist *tokenList);
bool variableDeclaration(LinkedTlist *tokenList);
bool varKW(LinkedTlist *tokenList, bool *isMutable);
bool returnStatement(LinkedTlist *tokenList);
bool returnTail(LinkedTlist *tokenList);
bool rValue(LinkedTlist *tokenList);
bool args(LinkedTlist *tokenList);
bool argsTail(LinkedTlist *tokenList);
bool term(LinkedTlist *tokenList);
bool ifStatement(LinkedTlist *tokenList);
bool whileStatement(LinkedTlist *tokenList);

AstExpressionType lit(int tkType);

bool prolog(LinkedTlist *tokenList) {
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
    if (getTkType(token) == TK_LPAREN) {
      if (params(tokenList)) {
        token = popToken(tokenList);
        if (getTkType(token) == TK_RPAREN) {  // Confirmed: (Params)
          AstDataType returnType = optType(tokenList);
          if (returnType == UNKNOWN) {
            return false;  // incorrect return type
          }
          fun->declaredReturnType = returnType;
          token = popToken(tokenList);
          if (getTkType(token) == TK_ASSIGN) {
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
  if (getTkType(token) == TK_ATMAIN) {
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
  if (getTkType(token) == TK_NEWLINE) {
    return;
  } else {
    pushToken(token, tokenList);
  }
}

bool funBody(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (getTkType(token) == TK_NEWLINE) {
    token = popToken(tokenList);
    if (getTkType(token) == TK_INDENT) {
      if (codeBlock(tokenList)) {
        token = popToken(tokenList);
        if (getTkType(token) == TK_DEDENT) {
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
  if (getTkType(token) == TK_COMMA) {
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
  if (getTkType(token) == TK_IDENT) {
    token = popToken(tokenList);
    if (getTkType(token) == TK_COLON) {
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
  if (getTkType(token) == TK_COLON) {
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
      if (getTkType(token) == TK_NEWLINE) {
        return true;
      }
    }
  }
  fprintf(stderr, "Invalid statement");
  return false;
}

bool simpleTail(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (getTkType(token) == TK_SEMICOLON) {
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

bool assignment(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (getTkType(token) == TK_IDENT) {
    token = popToken(tokenList);
    if (getTkType(token) == TK_ASSIGN) {
      return rValue(tokenList);
    } else {
      fprintf(stderr, "Missing '=' in assignment");
      return false;
    }
  }
  return false;
}

bool functionCall(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (getTkType(token) == TK_IDENT) {
    token = popToken(tokenList);
    if (getTkType(token) == TK_LPAREN) {
      if (args(tokenList)) {
        token = popToken(tokenList);
        if (getTkType(token) == TK_RPAREN) {
          return true;
        } else {
          fprintf(stderr, "Missing ')' in function call");
          return false;
        }
      }
    } else {
      fprintf(stderr, "Missing '(' in function call");
      return false;
    }
  }
  return false;
}

bool variableDeclaration(LinkedTlist *tokenList) {
  bool isMutable = false;
  if (varKW(tokenList, &isMutable)) {
    popToken(tokenList);  // pop the found var/val
    Token *token = popToken(tokenList);
    if (getTkType(token) == TK_IDENT) {
      AstDataType dtype = optType(tokenList);
      if (dtype == UNKNOWN) {
        fprintf(stderr, "Incorrect variable type");
        return false;
      }
      token = popToken(tokenList);
      if (getTkType(token) == TK_ASSIGN) {
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
  int tkType = getTkType(token);
  if (tkType == TK_KW_VAR)
    *isMutable = true;
  else if (tkType == TK_KW_VAL)
    *isMutable = false;
  else
    return false;
  return true;
}

bool returnStatement(LinkedTlist *tokenList) {
  Token *token = popToken(tokenList);
  if (getTkType(token) == TK_KW_RETURN) {
    return returnTail(tokenList);
  }
  return false;
}

bool returnTail(LinkedTlist *tokenList) {
  return rValue(tokenList) || true;  // epsilon
}

bool rValue(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  if (token == NULL) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
    return false;
  }
  int tokenType = getTkType(token);
  if (tokenType == TK_IDENT) {
    Token *nextToken = lookAheadToken(tokenList, 1);
    if (nextToken != NULL && getTkType(nextToken) == TK_LPAREN) {
      return functionCall(tokenList);
    }
  }
  return expression(tokenList);
}

bool args(LinkedTlist *tokenList) {
  if (term(tokenList)) {
    return argsTail(tokenList);
  } else {
    return true;  // epsilon
  }
}

bool argsTail(LinkedTlist *tokenList) {
  Token *token = lookAheadToken(tokenList, 0);
  if (getTkType(token) == TK_COMMA) {
    popToken(tokenList);
    if (term(tokenList)) {
      return argsTail(tokenList);
    }
  } else {
    return true;  // epsilon
  }
  return false;  // term failed after comma
}

bool term(LinkedTlist *tokenList) {
  AstExpressionType literal = BINARY;  // default wrong option, because can't use macro
  Token *token = lookAheadToken(tokenList, 0);
  int tkType = getTkType(token);
  literal = lit(tkType);
  if (tkType == TK_IDENT) {
    popToken(tokenList);
    return true;
  } else if (literal != BINARY) {
    popToken(tokenList);
    return true;
  } else if (tkType == TK_LPAREN) {
    popToken(tokenList);
    token = popToken(tokenList);
    if (getTkType(token) == TK_RPAREN) {  // Confirmed: UNIT_LITERAL
      literal = UNIT_LITERAL;
      return true;
    } else {
      die(ERR_SYNTAX, getLine(token), "Missing ')' for UNIT_LITERAL");
      return false;
    }
  }
  return false;
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
    if (getTkType(token) != TK_KW_THEN) {
      die(ERR_SYNTAX, getLine(token), "Missing 'then' in if statement");
    }
    token = popToken(tokenList);
    if (getTkType(token) != TK_NEWLINE) {
      die(ERR_SYNTAX, getLine(token), "Missing '\\n' in if statement");
    }
    token = popToken(tokenList);
    if (getTkType(token) != TK_INDENT) {
      die(ERR_SYNTAX, getLine(token), "Missing indentation in if statement");
    }
    if (codeBlock(tokenList)) {
      token = popToken(tokenList);
      if (getTkType(token) != TK_DEDENT) {
        die(ERR_SYNTAX, getLine(token),
            "Missing dedentation (inversion of indentation) in if statement");
      }
      token = popToken(tokenList);
      if (getTkType(token) != TK_KW_ELSE) {
        die(ERR_SYNTAX, getLine(token), "Missing 'else' in if statement");
      }
      token = popToken(tokenList);
      if (getTkType(token) != TK_NEWLINE) {
        die(ERR_SYNTAX, getLine(token), "Missing '\\n' in else branch");
      }
      token = popToken(tokenList);
      if (getTkType(token) != TK_INDENT) {
        die(ERR_SYNTAX, getLine(token), "Missing indentation in else branch");
      }
      if (codeBlock(tokenList)) {
        token = popToken(tokenList);
        if (getTkType(token) != TK_DEDENT) {
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
    if (getTkType(token) != TK_KW_DO) {
      die(ERR_SYNTAX, getLine(token), "Missing 'do' in while statement");
    }
    token = popToken(tokenList);
    if (getTkType(token) != TK_NEWLINE) {
      die(ERR_SYNTAX, getLine(token), "Missing '\\n' in while statement");
    }
    token = popToken(tokenList);
    if (getTkType(token) != TK_INDENT) {
      die(ERR_SYNTAX, getLine(token), "Missing indentation in while statement");
    }
    if (codeBlock(tokenList)) {
      token = popToken(tokenList);
      if (getTkType(token) != TK_DEDENT) {
        die(ERR_SYNTAX, getLine(token),
            "Missing dedentation (inversion of indentation) in while statement");
      }
      return true;
    }
  }
  return false;
}

AstExpressionType lit(int tkType) {
  switch (tkType) {
    case TK_DECIMAL:
      return DOUBLE_LITERAL;
    case TK_NUMBER:
      return INT_LITERAL;
    case TK_STRING:
      return STRING_LITERAL;
    default:
      return BINARY;  // default wrong option
  }
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
