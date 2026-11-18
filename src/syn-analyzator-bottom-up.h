/**
 * @file syn-analyzator-bottom-up.h
 * Project: IFJ26 compiler
 * @brief precedence tokens structure and functions
 * @author Martin Stovicek <xstovim00>
 *
 * Brief explanation:
 *  Will be added later
 */

#ifndef SA_BOTTOM_UP_H
#define SA_BOTTOM_UP_H

#include <stdbool.h>

#include "token.h"

typedef enum {
  PREC_OPERAND,      // i (TK_IDENT, TK_NUMBER, TK_DECIMAL, TK_STRING)
  PREC_PLUS_MINUS,   // + a binární -
  PREC_MUL_DIV,      // * a /
  PREC_UNARY_MINUS,  // unární -
  PREC_RELATIONAL,   // <, <=, >, >=
  PREC_EQUAL,        // ==, !=
  PREC_LPAREN,       // (
  PREC_RPAREN,       // )
  PREC_DOLLAR,       // $ (konec výrazu - newline, then, do, čárka, EOF...)
  PREC_UNKNOWN       // neplatný symbol ve výrazu } PrecSymbol;
} PrecSymbol;

PrecSymbol tokenToPrecSymbol(struct Token *token, struct Token *prevToken);

/// function will be implemented in different file, because it is more complex and will be
/// implemented as a precedence parser
bool expression(LinkedTlist *tokenList);

#endif  // SA_BOTTOM_UP_H
