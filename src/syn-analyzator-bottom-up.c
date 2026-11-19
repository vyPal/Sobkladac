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

#include <limits.h>

#include "ast.h"
#include "common.h"

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

AstLocation getLocation(Token *token) {
  AstLocation location = {
      .line = (unsigned)getLine(token),
      .column = 100,  // (unsigned)getColumn(token), // TODO after implementation this function in
                      // lexer, uncomment and replace hard-coded 100
  };
  return location;
}

AstExpressionType tokenToExpressionType(Token *token) {
  if (token == NULL) {
    die(ERR_SYNTAX, 999, "Unexpected end of file");
  }
  enum TkType type = getTkType(token);
  switch (type) {
    case TK_IDENT:
      return VARIABLE;
    case TK_NUMBER:
      return INT_LITERAL;
    case TK_DECIMAL:
      return DOUBLE_LITERAL;
    case TK_STRING:
      return STRING_LITERAL;
    case TK_PLUS:
    case TK_MINUS:  // unary minus is already checked
    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_BIGGER:
    case TK_SMALLER:
    case TK_BEQUAL:
    case TK_SEQUAL:
    case TK_EQUAL:
    case TK_NOT_EQUAL:
      return BINARY;
    default:
      break;
  }
  return UNIT_LITERAL;  // incorrect one
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

void stackInit(PrecStack *s, int capacity) {
  if (s == NULL || capacity <= 0)
    die(ERR_INTERNAL, 0, "Invalid precedence stack initialization");

  s->capacity = capacity;
  s->items = arenaAlloc((size_t)capacity * sizeof *s->items);
  s->top = -1;
}

void pushItem(PrecStackItem item, PrecStack *stack) {
  if (stack == NULL || stack->items == NULL || stack->capacity <= 0)
    die(ERR_INTERNAL, 0, "Invalid precedence stack");

  if (stack->top >= stack->capacity - 1) {
    size_t oldCapacity = (size_t)stack->capacity;
    size_t newCapacity = oldCapacity * 2;
    stack->items = arenaRealloc(stack->items, oldCapacity * sizeof(PrecStackItem),
                                newCapacity * sizeof(PrecStackItem));
    stack->capacity = (int)newCapacity;
  }
  stack->top++;
  stack->items[stack->top] = item;
}

PrecStackItem popItem(PrecStack *stack) {
  if (stack == NULL || stack->top < 0)
    die(ERR_SYNTAX, 999, "Missing item on stack to pop");

  return stack->items[stack->top--];
}

PrecStackItem *getTopItem(PrecStack *stack) {
  if (stack == NULL)
    return NULL;

  for (int i = stack->top; i >= 0; i--) {
    if (stack->items[i].type == TERMINAL)
      return &stack->items[i];
  }
  return NULL;
}

PrecStackItem makeTerminalItem(PrecSymbol sym, Token *token, bool isBreakpoint) {
  PrecStackItem item;
  item.symbol = sym;
  item.token = token;
  item.type = TERMINAL;
  item.startBrkP = isBreakpoint;
  item.astNode = NULL;
  return item;
}

bool isBreakPoint(PrecStack *stack) {
  PrecStackItem *top = getTopItem(stack);
  return top->startBrkP || top->symbol == PREC_DOLLAR;
}

AstExpression *executeReduce(PrecStack *stack) {
  // we pop items from stack until we hit on Breakpoint item
  int count = 0;
  PrecStackItem popped[3];

  while (!isBreakPoint(stack)) {
    popped[count] = popItem(stack);
    count++;
  }
  getTopItem(stack)->startBrkP = false;  // Getting rid of imaginary breakpoint '<'

  // RULE 1: E-> i (1 symbol - literal/variable)
  if (count == 1 && popped[0].type == TERMINAL) {
    PrecStackItem onePop = popped[0];
    if (onePop.symbol != PREC_OPERAND)
      die(ERR_SYNTAX, getLine(onePop.token), "Wrong operand");
    AstExpressionType ttET = tokenToExpressionType(onePop.token);
    if (ttET == UNIT_LITERAL || ttET == UNARY || ttET == BINARY)
      return NULL;
    return astExpressionNew(ttET, getLocation(onePop.token));
  }

  // RULE 2: E-> (E) (3 symbols)
  // if TODO

  // RULE 3: E-> -E (Unary - 2 symbols)
  // if TODO

  // RULE 4: E-> E OPERATOR E (Binary 3 symbols)
  // if TODO
  return NULL;
}

bool stackCorrect(PrecStack stack) {
  stack.capacity = stack.capacity;  // useless, just to lose warnings
  return true;                      // TODO
}

/// function will be implemented in different file, because it is more complex and will be
/// implemented as a precedence parser
bool expression(LinkedTlist *tokenList) {  // TODO: Add parametr: specific ending token
  if (tokenList == NULL)
    die(ERR_SYNTAX, 999, "Missing tokens for expression");
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
  PrecStack stack;
  stackInit(&stack, 3);
  PrecStackItem item = makeTerminalItem(PREC_DOLLAR, NULL, false);
  pushItem(item, &stack);              // default settings - I pushed $ on stack
  Token *token = popToken(tokenList);  // guaranteed token
  Token *prevToken = token;
  PrecSymbol sym = tokenToPrecSymbol(token, prevToken);
  PrecStackItem b, *a;
  do {
    a = getTopItem(&stack);
    b = makeTerminalItem(sym, token, false);
    switch (precTable[a->symbol][b.symbol]) {
      case '<':
        a->startBrkP = true;
      case '=':
        /*push b - 1st line, then read another symbol b on input - other lines*/
        pushItem(b, &stack);
        prevToken = token;
        token = popToken(tokenList);
        if (token == NULL) {
          die(ERR_SYNTAX, getLine(prevToken), "Missing other tokens");
        }
        sym = tokenToPrecSymbol(token, prevToken);
        b = makeTerminalItem(sym, token, false);
        break;
      case '>':
        executeReduce(&stack);
        break;
      default:
        die(ERR_SYNTAX, getLine(token), "Invalid expression");
        break;
    }
  } while (b.symbol == PREC_DOLLAR && stackCorrect(stack));
  return true;  // Only temporary
}
