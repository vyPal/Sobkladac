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

#include "ast.h"
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

typedef enum { TERMINAL, NON_TERMINAL } ItemType;

typedef struct {
  ItemType type;
  PrecSymbol symbol;       // Used when type is TERMINAL.
  Token *token;            // Original token for terminals.
  bool startBrkP;          // Marks the start of a handle to reduce.
  AstExpression *astNode;  // Used when type is NON_TERMINAL.
} PrecStackItem;

typedef struct {
  PrecStackItem *items;
  int top;
  int capacity;
} PrecStack;

/// Initializes an empty stack with the requested positive capacity.
void stackInit(PrecStack *s, int capacity);

/// Pushes an item, growing the arena-backed array when it is full.
void pushItem(PrecStackItem item, PrecStack *stack);

/// Removes and returns the top item. Fails if the stack is empty.
PrecStackItem popItem(PrecStack *stack);

/// Returns the nearest terminal from the top of the stack, or NULL.
PrecStackItem *getTopItem(PrecStack *stack);

/// Creates a terminal stack item; the AST node is initialized to NULL.
PrecStackItem makeTerminalItem(PrecSymbol symbol, Token *token, bool isBreakpoint);

PrecSymbol tokenToPrecSymbol(struct Token *token, struct Token *prevToken);

/// Parses an expression from the token list.
bool expression(LinkedTlist *tokenList);

#endif  // SA_BOTTOM_UP_H
