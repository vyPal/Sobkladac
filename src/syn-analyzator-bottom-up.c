/**
 * @file syn-analyzator-bottom-up.h
 * Project: IFJ26 compiler
 * @brief Implementation of precedence SA
 * @author Martin Stovicek <xstovim00>
 *
 * Brief explanation:
 *  Will be added later
 */

#include "syn-analyzator-bottom-up.h"

#define PREC_TYPES 9

bool isUnaryContext(Token *prevToken) {
  enum TkType type = getTkType(prevToken);
  switch (type) {
    case TK_IDENT:
    case TK_NUMBER:
    case TK_DECIMAL:
    case TK_STRING:
    case TK_RPAREN:
      return false;
    default:
      return true;
  }
}

PrecSymbol tokenToPrecSymbol(struct Token *token, struct Token *prevToken) {
  if (token == NULL)
    return PREC_DOLLAR;
  enum TkType type = getTkType(token);
  switch (type) {
    case TK_IDENT:
    case TK_NUMBER:
    case TK_DECIMAL:
    case TK_STRING:
      return PREC_OPERAND;
    case TK_PLUS:
      return PREC_PLUS_MINUS;
    case TK_MINUS:
      // Unární mínus poznáme podle toho, co stálo před ním
      if (isUnaryContext(prevToken)) {
        return PREC_UNARY_MINUS;
      }
      return PREC_PLUS_MINUS;
    case TK_MULTIPLY:
    case TK_DIVIDE:
      return PREC_MUL_DIV;
    case TK_LPAREN:
      return PREC_LPAREN;
    case TK_RPAREN:
      return PREC_RPAREN;
    case TK_SMALLER:
    case TK_SEQUAL:
    case TK_BIGGER:
    case TK_BEQUAL:
      return PREC_RELATIONAL;
    case TK_EQUAL:
    case TK_NOT_EQUAL:
      return PREC_EQUAL;

    // Tokeny, které ukončují výraz
    case TK_NEWLINE:
    case TK_EOF:
    case TK_COMMA:
    case TK_COLON:
      return PREC_DOLLAR;
    default:
      return PREC_UNKNOWN;
  }
}

/// function will be implemented in different file, because it is more complex and will be
/// implemented as a precedence parser
bool expression(LinkedTlist *tokenList) {
  static const char precTable[PREC_TYPES][PREC_TYPES] = {
      /*                     0     1      2       3    4    5     6    7   8*/
      /* stack \     in  | plus | mul | U(-x) | REL | EQ | '(' | ')' | i | $ */
      /* + -             */ {'>', '<', '<', '>', '>', '<', '>', '<', '>'},  // ok
      /* * /             */ {'>', '>', '<', '>', '>', '<', '>', '<', '>'},  // ok
      /* Unary(-x)       */ {'>', '>', '>', '>', '>', '<', '>', '<', '>'},  // ok
      /* REL(<,>,<=, >=) */ {'<', '<', '<', 'E', '>', '<', '>', '<', '>'},  // ok
      /* EQ (==, !=)     */ {'<', '<', '<', '<', 'E', '<', '>', '<', '>'},  // ok
      /* (               */ {'<', '<', '<', '<', '<', '<', '=', '<', 'E'},  // ok
      /* )               */ {'>', '>', 'E', '>', '>', 'E', '>', 'E', '>'},  // ok
      /* i               */ {'>', '>', 'E', '>', '>', 'E', '>', 'E', '>'},  // ok
      /* $               */ {'<', '<', '<', '<', '<', '<', 'E', '<', 'E'}   // ok

  };
  Token *token = popToken(tokenList);
  pushToken(token, tokenList);
  return true;  // Only temporary
}
