/**
 * @file syn-analyzator-bottom-up.h
 * Project: IFJ26 compiler
 * @brief Data structures and functions for precedence-based expression parsing.
 * @author Martin Stovicek <xstovim00>
 */

#ifndef SA_BOTTOM_UP_H
#define SA_BOTTOM_UP_H

#include <stdbool.h>

#include "ast.h"
#include "token.h"

/**
 * @brief Precedence classes used to index the parser's precedence table.
 */
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

/**
 * @brief Distinguishes input tokens from reduced expression nodes on the stack.
 */
typedef enum { TERMINAL, NON_TERMINAL } ItemType;

/**
 * @brief One terminal or reduced expression stored on the precedence stack.
 */
typedef struct {
  ItemType type;
  PrecSymbol symbol;       // Used when type is TERMINAL.
  Token *token;            // Original token for terminals.
  bool startBrkP;          // Marks the start of a handle to reduce.
  AstExpression *astNode;  // Used when type is NON_TERMINAL.
} PrecStackItem;

/**
 * @brief Array-backed precedence stack.
 *
 * The stack is empty when @c top is -1.
 */
typedef struct {
  PrecStackItem *items;
  int top;
  int capacity;
} PrecStack;

/**
 * @brief Initializes an empty precedence stack.
 * @param s Stack to initialize.
 * @param capacity Initial stack capacity; must be positive.
 */
void stackInit(PrecStack *s, int capacity);

/**
 * @brief Pushes an item onto the precedence stack.
 * @param item Item to push.
 * @param stack Stack to modify.
 *
 * The arena-backed storage grows automatically when the stack is full.
 */
void pushItem(PrecStackItem item, PrecStack *stack);

/**
 * @brief Removes and returns the top stack item.
 * @param stack Stack to pop from.
 * @return The item that was at the top of the stack.
 *
 * An attempt to pop an empty stack is reported as a syntax error.
 */
PrecStackItem popItem(PrecStack *stack);

/**
 * @brief Finds the nearest terminal on the stack.
 * @param stack Stack to search, starting at its top.
 * @return Pointer to the nearest terminal, or NULL if none exists.
 *
 * The returned pointer may be invalidated by a push that grows the stack.
 */
PrecStackItem *getTopItem(PrecStack *stack);

/**
 * @brief Creates a terminal stack item.
 * @param symbol Precedence class of the terminal.
 * @param token Original token represented by the item.
 * @param isBreakpoint Whether this item marks the start of a handle.
 * @return Initialized terminal item with no AST node.
 */
PrecStackItem makeTerminalItem(PrecSymbol symbol, Token *token, bool isBreakpoint);

/**
 * @brief Gets the source location for an AST node from a token.
 * @param token Token to retrieve the location from.
 * @return The token's AST source location.
 */
AstLocation getLocation(Token *token);

/**
 * @brief Maps a token to its precedence class.
 * @param token Token to classify; NULL represents the end marker.
 * @param prevToken Previous token, used to distinguish unary and binary minus.
 * @return The token's precedence class.
 */
PrecSymbol tokenToPrecSymbol(struct Token *token, struct Token *prevToken);

/**
 * @brief Checks whether the stack contains one complete reduced expression.
 * @param stack Precedence stack to check.
 * @return true if the stack contains the end marker followed by one
 *         non-terminal and no pending breakpoint; otherwise false.
 */
bool stackCorrect(PrecStack stack);

/**
 * @brief Parses an expression from a token list.
 * @param tokenList List whose leading tokens make up the expression.
 * @return Expression if parsing succeeds; otherwise it won't return.
 */
AstExpression *expression(LinkedTlist *tokenList);

/**
 * @brief Extra function for getting correct binary operator.
 * @param token Token to get Binary operator from
 * @return correct operator on success, NEGATION Operator when token is not operator
 */
AstOperator tokenToOperator(Token *token);

/**
 * @brief Decides if expression type is literal or binary from token
 * @param token Token to get Expression type from
 * @return type - literal/binary on success, UNIT_LITERAL ExpType when token is different
 */
AstExpressionType tokenToExpressionType(Token *token);

#endif  // SA_BOTTOM_UP_H
