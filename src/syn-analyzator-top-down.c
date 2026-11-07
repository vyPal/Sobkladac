/**
 * @file syn-analyzator-top-down.c
 * @brief Implementation of top-down syntax analyzer.
 * @author Martin Stovicek <xstovim00>
 */
#include "syn-analyzator-top-down.h"

#include <stdbool.h>

AstProgram *topDown(struct LinkedTlist *tokenList) {
  // Check for prologue
  bool prologCorrect = false;
  Token *newToken = popToken(tokenList);
  if (getTkType(newToken) == TK_IDENT && strcmp(getValue(newToken), "import") == 0) {
    newToken = popToken(tokenList);
    if (getTkType(newToken) == TK_STRING) {
      if (strcmp(getValue(newToken), "ifj26.*") ==
          0)  // TODO: check if ifj26.* will exist as string or something special
        prologCorrect = true;
    }
  }
  if (!prologCorrect) {
    fprintf(stderr, "Error: Prologue is not correct.\n");
    return NULL;
  }
  return NULL;
}
