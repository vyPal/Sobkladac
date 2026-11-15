/**
 * @file syn-analyzator-top-down.c
 * @brief Implementation of top-down syntax analyzer.
 * @author Martin Stovicek <xstovim00>
 */
#include "syn-analyzator-top-down.h"

#include <stdbool.h>

#include "common.h"

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
bool LinesTail(LinkedTlist *tokenList);
bool Line(LinkedTlist *tokenList);

bool prolog(LinkedTlist *tokenList) {
  Token *importT = popToken(tokenList);
  Token *identT = popToken(tokenList);
  Token *dotT = popToken(tokenList);
  Token *multiplyT = popToken(tokenList);
  Token *newlineT = popToken(tokenList);
  if (getTkType(importT) == TK_KW_IMPORT &&                                       // import
      getTkType(identT) == TK_IDENT && strcmp(getValue(identT), "ifj26") == 0 &&  // ifj26
      getTkType(dotT) == TK_DOT &&                                                //.
      getTkType(multiplyT) == TK_MULTIPLY &&                                      //*
      getTkType(newlineT) == TK_NEWLINE                                           // newline
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

bool funDef(LinkedTlist *tokenList, AstProgram *asTree) {  // TODO
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
  astFreeProgram(asTree);
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
    return RValue(tokenList);
  }
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
  if (Line(tokenList)) {
    return LinesTail(tokenList);
  } else {
    return false;  // no lines in code block
  }
}

bool LinesTail(LinkedTlist *tokenList) {
  if (Line(tokenList)) {
    return LinesTail(tokenList);
  } else {
    return true;  // epsilon
  }
}

bool Line(LinkedTlist *tokenList) {
  return true;  // TODO
}

bool RValue(LinkedTlist *tokenList) {
  return true;  // TODO
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
    astFreeProgram(asTree);
    return NULL;
  }
  return asTree;
}
