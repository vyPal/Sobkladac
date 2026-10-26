/**
 * @file test_token.c
 * Project: IFJ26 compiler
 * @brief Token unit tests
 * @author Vojtech Vitek <xvitekv00>
 *
 * Tests for the functions in token.c
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "token.h"
#include "tst.h"

tstsuite("Token", ) {
  tstcase("newToken", ) {
    struct Token *token = newToken(TK_IDENT, 42);
    tstcheck(token != NULL, "newToken should return a non-NULL pointer");
    tstcheck(getTkType(token) == TK_IDENT, "Token type should be TK_IDENT");
    tstcheck(getLine(token) == 42, "Token line should be 42");
    tstcheck(getValue(token) == NULL, "Token value should be NULL");

    arenaCleanup();
  }

  tstcase("setValue and getValue", ) {
    struct Token *token = newToken(TK_STRING, 1);
    struct String *string = newString("Hello", 5);

    union Value *value = arenaAlloc(sizeof(union Value));
    value->s = string;
    setValue(token, value);
    tstcheck(getValue(token) == value, "getValue should return the set value");

    arenaCleanup();
  }
}
