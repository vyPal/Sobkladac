/**
 * @file syn-analyzator-top-down.c
 * @brief Implementation of top-down syntax analyzer.
 * @author Martin Stovicek <xstovim00>
 */
#include "syn-analyzator-top-down.h"

#include <stdbool.h>

bool Prolog(LinkedTlist *tokenList) {
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

bool FunDefs(LinkedTlist *tokenList) {
  if (Fundef(tokenList)) {
    return FundefsTail(tokenList);
  }
}

bool FundefsTail(LinkedTlist *tokenList) {
  return true;
}

bool Fundef(LinkedTlist *tokenList) {
  return true;
}

AstProgram *topDown(struct LinkedTlist *tokenList) {
  tokenList = layoutFilter(tokenList);

  bool result = Prolog(tokenList) && Fundefs(tokenList);
  Token *lastToken = popToken(tokenList);
  result = result && (getTkType(lastToken) == TK_EOF);

  // // Check for prologue
  // bool prologCorrect = false;
  // Token *newToken = popToken(tokenList);
  // if (getTkType(newToken) == TK_IDENT && strcmp(getValue(newToken), "import") == 0) {
  //   newToken = popToken(tokenList);
  //   if (getTkType(newToken) == TK_STRING) {
  //     if (strcmp(getValue(newToken), "ifj26.*") ==
  //         0)  // TODO: check if ifj26.* will exist as string or something special
  //       prologCorrect = true;
  //   }
  // }
  // if (!prologCorrect) {
  //   fprintf(stderr, "Error: Prologue is not correct.\n");
  //   return NULL;
  // }
  return NULL;
}
