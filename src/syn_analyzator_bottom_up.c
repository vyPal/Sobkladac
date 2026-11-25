/**
 * @file syn_analyzator_bottom_up.c
 * Project: IFJ26 compiler
 * @brief Implementation of precedence SA
 * @author Martin Stovicek <xstovim00>
 *
 * Brief explanation:
 *  This file implements precendent table, some converters to make table smaller,
 *  stack for reduction terminals into Expression tree;
 *  Rest of it are some helper functions
 */

#include "syn_analyzator_bottom_up.h"

#include <limits.h>
#include <stddef.h>

#include "ast.h"
#include "common.h"

#define PREC_TYPES 11

bool isUnaryContext(const Token *prevToken) {
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

AstLocation getLocation(const Token *token) {
  AstLocation location = {
      .line = (unsigned)lineOf(token),
      .column = 0,  // (unsigned)getColumn(token), // TODO after implementation this function in
                    // lexer, uncomment and replace hard-coded 100
  };
  return location;
}

AstOperator tokenToOperator(const Token *token) {
  if (token == NULL) {
    die(ERR_SYNTAX, 0, "Unexpected end of file");
  }
  enum TkType type = getTkType(token);
  switch (type) {
    case TK_PLUS:
      return ADDITION;
    case TK_MINUS:
      return SUBTRACTION;
    case TK_MULTIPLY:
      return MULTIPLICATION;
    case TK_DIVIDE:
      return DIVISION;
    case TK_BIGGER:
      return GREATER;
    case TK_SMALLER:
      return LESSER;
    case TK_BEQUAL:
      return GREATER_EQUAL;
    case TK_SEQUAL:
      return LESSER_EQUAL;
    case TK_EQUAL:
      return EQUAL;
    case TK_NOT_EQUAL:
      return NOT_EQUAL;
    // Boolthen ext.
    case TK_AND:
      return LOGICAL_AND;
    case TK_OR:
      return LOGICAL_OR;
    default:
      break;
  }
  return NEGATION;  // wrong input
}

AstExpressionType tokenToExpressionType(const Token *token) {
  if (token == NULL) {
    die(ERR_SYNTAX, 0, "Unexpected end of file");
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
    case TK_KW_TRUE:
    case TK_KW_FALSE:
      return BOOL_LITERAL;
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
    // boolthen ext.
    case TK_AND:
    case TK_OR:
      return BINARY;
    default:
      break;
  }
  return UNIT_LITERAL;  // incorrect one
}

PrecSymbol tokenToPrecSymbol(const struct Token *token, const struct Token *prevToken) {
  if (token == NULL)
    die(ERR_SYNTAX, lineOf(prevToken), "Missing tokens");
  enum TkType type = getTkType(token);
  switch (type) {
    case TK_IDENT:
    case TK_NUMBER:
    case TK_DECIMAL:
    case TK_STRING:
    case TK_KW_TRUE:   // ext. boolthen
    case TK_KW_FALSE:  // ext. boolthen
      return PREC_OPERAND;
    case TK_PLUS:
      return PREC_PLUS_MINUS;
    case TK_MINUS:
      // We can identify unary minus by looking at what preceded it
      if (isUnaryContext(prevToken)) {
        return PREC_UNARY;
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

    // Boolthen
    case TK_NOT:
      return PREC_UNARY;
    case TK_AND:
      return PREC_AND;
    case TK_OR:
      return PREC_OR;

    // Tokens that terminate an expression
    case TK_NEWLINE:
    case TK_SEMICOLON:
    case TK_EOF:
    case TK_COMMA:
    case TK_COLON:
    case TK_KW_THEN:
    case TK_KW_DO:
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
    die(ERR_SYNTAX, 0, "Missing item on stack to pop");

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
  const PrecStackItem *top = getTopItem(stack);
  return top->startBrkP || top->symbol == PREC_DOLLAR;
}

AstExpression *executeReduce(PrecStack *stack) {
  // we pop items from stack until we hit on Breakpoint item
  int count = 0;
  PrecStackItem popped[3];

  // The handle starts right above the breakpoint terminal and may begin with a non-terminal
  // (e.g. E + E), so stop only when the item on top of the stack is the breakpoint itself.
  while (stack->top >= 0 && !(stack->items[stack->top].type == TERMINAL &&
                              (stack->items[stack->top].startBrkP ||
                               stack->items[stack->top].symbol == PREC_DOLLAR))) {
    if (count >= 3) {
      return NULL;
    }
    popped[count] = popItem(stack);
    count++;
  }
  getTopItem(stack)->startBrkP = false;  // Getting rid of imaginary breakpoint '<'

  // RULE 1: E-> i (1 symbol - literal/variable)
  if (count == 1 && popped[0].type == TERMINAL) {
    PrecStackItem onePop = popped[0];
    if (onePop.symbol != PREC_OPERAND)
      die(ERR_SYNTAX, lineOf(onePop.token), "Wrong operand");
    AstExpressionType ttET = tokenToExpressionType(onePop.token);
    if (ttET == UNIT_LITERAL || ttET == UNARY || ttET == BINARY)
      return NULL;
    AstExpression *ex = astExpressionNew(ttET, getLocation(onePop.token));
    switch (getTkType(onePop.token)) {
      case TK_NUMBER:
        ex->data.intValue = getValue(onePop.token)->i;
        break;
      case TK_DECIMAL:
        ex->data.doubleValue = getValue(onePop.token)->d;
        break;
      case TK_STRING:
        ex->data.stringValue = getString(getValue(onePop.token)->s);
        break;
      case TK_IDENT:
        ex->data.variable = getString(getValue(onePop.token)->s);
        break;
        // boolthen ext.
      case TK_KW_TRUE:
        ex->data.boolValue = true;
        break;
      case TK_KW_FALSE:
        ex->data.boolValue = false;
        break;
      default:
        break;
    }
    return ex;
  }

  // RULE 2: E-> (E) (3 symbols)
  if (count == 3 && popped[2].symbol == PREC_LPAREN && popped[1].type == NON_TERMINAL &&
      popped[0].symbol == PREC_RPAREN) {
    return popped[1].astNode;
  }

  // RULE 3: E-> -E | !E (Unary - 2 symbols)
  if (count == 2 && popped[1].type == TERMINAL && popped[1].symbol == PREC_UNARY &&
      popped[0].type == NON_TERMINAL) {
    AstExpression *ex = astExpressionNew(UNARY, getLocation(popped[1].token));
    const Token *token = popped[1].token;  // looks at unary operator
    if (token == NULL) {
      die(ERR_SYNTAX, lineOf(token), "Missing token");
    }
    if (getTkType(token) == TK_NOT) {
      ex->data.unary.operatorAst = LOGICAL_NEGATION;
    } else if (getTkType(token) == TK_MINUS) {
      ex->data.unary.operatorAst = NEGATION;
    } else
      die(ERR_SYNTAX, lineOf(token), "Invalid unary token (Valid only -x or !x)");

    ex->data.unary.expression = popped[0].astNode;
    return ex;
  }

  // RULE 4: E-> E OPERATOR E (Binary 3 symbols)
  if (count == 3 && popped[2].type == NON_TERMINAL &&
      tokenToExpressionType(popped[1].token) == BINARY && popped[0].type == NON_TERMINAL) {
    AstExpression *ex = astExpressionNew(BINARY, getLocation(popped[1].token));
    AstOperator op = tokenToOperator(popped[1].token);
    if (op == NEGATION) {
      die(ERR_SYNTAX, lineOf(popped[1].token), "Wrong operator for expression");
    }
    ex->data.binary.leftExpression = popped[2].astNode;
    ex->data.binary.operatorAst = op;
    ex->data.binary.rightExpression = popped[0].astNode;
    return ex;
  }
  return NULL;
}

bool stackCorrect(PrecStack stack) {
  const PrecStackItem *dolar = getTopItem(&stack);
  // Checks that first terminal is dollar, without any breakpoint '<' and on top of that is only one
  // non-terminal
  return dolar->symbol == PREC_DOLLAR && !dolar->startBrkP && stack.top == 1 &&
         stack.items[stack.top].type == NON_TERMINAL;
}

/// function will be implemented in different file, because it is more complex and will be
/// implemented as a precedence parser
AstExpression *expression(LinkedTlist *tokenList) {  // TODO: Add parametr: specific ending token
  if (tokenList == NULL)
    die(ERR_SYNTAX, 0, "Missing tokens for expression");
  static const char precTable[PREC_TYPES][PREC_TYPES] = {
      /*                     0     1    2    3    4    5    6    7   8     9   10 */
      /* stack \     in  | plus | mul | U | REL| EQ | '('| ')' | i |  $ | AND| OR |*/
      /* + -             */ {'>', '<', '<', '>', '>', '<', '>', '<', '>', '>', '>'},  // ok
      /* * /             */ {'>', '>', '<', '>', '>', '<', '>', '<', '>', '>', '>'},  // ok
      /* Unary(-x)       */ {'>', '>', '<', '>', '>', '<', '>', '<', '>', '>', '>'},  // ok
      /* REL(<,>,<=, >=) */ {'<', '<', '<', 'E', '>', '<', '>', '<', '>', '>', '>'},  // ok
      /* EQ (==, !=)     */ {'<', '<', '<', '<', 'E', '<', '>', '<', '>', '>', '>'},  // ok
      /* (               */ {'<', '<', '<', '<', '<', '<', '=', '<', 'E', '<', '<'},  // ok
      /* )               */ {'>', '>', 'E', '>', '>', 'E', '>', 'E', '>', '>', '>'},  // ok
      /* i               */ {'>', '>', 'E', '>', '>', 'E', '>', 'E', '>', '>', '>'},  // ok
      /* $               */ {'<', '<', '<', '<', '<', '<', 'E', '<', 'E', '<', '<'},  // ok
      /* AND             */ {'<', '<', '<', '<', '<', '<', '>', '<', '>', '>', '>'},  // ok
      /* OR              */ {'<', '<', '<', '<', '<', '<', '>', '<', '>', '<', '>'}   // ok

  };
  // `()` is the Unit literal; the precedence grammar has no rule for an empty pair of parentheses
  const Token *first = lookAheadToken(tokenList, 0);
  const Token *second = lookAheadToken(tokenList, 1);
  if (first != NULL && second != NULL && getTkType(first) == TK_LPAREN &&
      getTkType(second) == TK_RPAREN) {
    popToken(tokenList);
    popToken(tokenList);
    return astExpressionNew(UNIT_LITERAL, getLocation(first));
  }

  PrecStack stack;
  stackInit(&stack, 3);
  PrecStackItem termItem = makeTerminalItem(PREC_DOLLAR, NULL, false);
  pushItem(termItem, &stack);                   // default settings - I pushed $ on stack
  Token *token = lookAheadToken(tokenList, 0);  // guaranteed token
  Token *prevToken = token;
  PrecSymbol sym = tokenToPrecSymbol(token, prevToken);
  if (sym == PREC_UNKNOWN) {
    die(ERR_SYNTAX, lineOf(token), "Unexpected token on input");
  }

  PrecStackItem b;
  AstExpression *expr;

  do {
    PrecStackItem *a = getTopItem(&stack);
    b = makeTerminalItem(sym, token, false);
    switch (precTable[a->symbol][b.symbol]) {
      case '<':
        a->startBrkP = true;
        // fallthrough
      case '=':
        /*push b - 1st line, then read another symbol b on input - other lines*/
        pushItem(b, &stack);
        prevToken = popToken(tokenList);
        token = lookAheadToken(tokenList, 0);
        if (token == NULL) {
          die(ERR_SYNTAX, lineOf(prevToken), "Missing other tokens");
        }
        sym = tokenToPrecSymbol(token, prevToken);
        if (sym == PREC_UNKNOWN) {
          die(ERR_SYNTAX, lineOf(token), "Unexpected token on input");
        }
        b = makeTerminalItem(sym, token, false);
        break;

      case '>':
        expr = executeReduce(&stack);
        if (expr == NULL) {
          die(ERR_SYNTAX, lineOf(prevToken), "No suitable grammar rule!");
        }
        PrecStackItem item;
        item.symbol = PREC_UNKNOWN;
        item.token = token;
        item.type = NON_TERMINAL;
        item.startBrkP = false;
        item.astNode = expr;
        pushItem(item, &stack);
        break;
      default:
        die(ERR_SYNTAX, lineOf(token), "Invalid expression");
        break;
    }
  } while (!(b.symbol == PREC_DOLLAR && stackCorrect(stack)));
  return stack.items[stack.top].astNode;  // All went through correctly
}
