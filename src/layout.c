/**
 * @file layout.c
 * Project: IFJ26 compiler
 * @brief Function for layout filtering of tokens
 * @author Jakub Palacky <xpalacj00>
 *
 * File contains the implementation of the layout filtering function,
 *  which processes a list of tokens and applies layout rules to generate
 *  a new list of tokens with proper indentation and dedentation.
 */

#include "common.h"
#include "syn-analyzator-top-down.h"

LinkedTlist *layoutFilter(LinkedTlist *in) {
  LinkedTlist *out = newTlist();
  LinkedTlist *last = out;

  size_t cap = 16, top = 0;
  int *stack = arenaAlloc(cap * sizeof(int));
  stack[0] = 0;

  int depth = 0;          // parenthesis nesting
  int lineIndent = 0;     // indentation of the current logical line
  int pendingIndent = 0;  // value of the last raw TK_INDENT
  bool atLineStart = true;
  bool checkContinuation = false;
  enum TkType prevType = TK_NEWLINE;

  Token *tk;
  while ((tk = popToken(in)) != NULL) {
    enum TkType type = getTkType(tk);
    int line = getLine(tk);

    if (type == TK_EOF) {  // close the last line and all open blocks
      if (!atLineStart)
        last = pushToken(newToken(TK_NEWLINE, line), last);
      for (; top > 0; top--) last = pushToken(newToken(TK_DEDENT, line), last);
      pushToken(tk, last);
      break;
    }

    if (depth > 0) {  // inside parentheses: line breaks are whitespace
      if (type == TK_NEWLINE)
        continue;
      if (type == TK_INDENT) {
        pendingIndent = getValue(tk)->i;
        checkContinuation = true;
        continue;
      }
      if (checkContinuation) {
        checkContinuation = false;
        if (pendingIndent < lineIndent || (pendingIndent == lineIndent && type != TK_RPAREN))
          die(ERR_SYNTAX, line, "Continuation line must be indented more than its logical line");
      }
      if (type == TK_COMMA && prevType != TK_LPAREN && prevType != TK_COMMA && isTrailingComma(in))
        continue;
    } else {
      if (type == TK_NEWLINE) {  // blank/comment lines: atLineStart is already true
        if (!atLineStart)
          last = pushToken(tk, last);
        atLineStart = true;
        continue;
      }
      if (type == TK_INDENT) {
        pendingIndent = getValue(tk)->i;
        continue;
      }
    }

    if (atLineStart) {  // first real token of a logical line
      if (pendingIndent > stack[top]) {
        if (++top == cap) {
          stack = arenaRealloc(stack, cap * sizeof(int), 2 * cap * sizeof(int));
          cap *= 2;
        }
        stack[top] = pendingIndent;
        last = pushToken(newToken(TK_INDENT, line), last);
      } else {
        while (pendingIndent < stack[top]) {
          top--;
          last = pushToken(newToken(TK_DEDENT, line), last);
        }
        if (pendingIndent != stack[top])
          die(ERR_SYNTAX, line, "Indentation does not match any enclosing block");
      }
      lineIndent = pendingIndent;
      atLineStart = false;
    }

    if (type == TK_LPAREN)
      depth++;
    else if (type == TK_RPAREN && depth > 0)
      depth--;
    else if (type == TK_IDENT)
      tk = toKeyword(tk);

    last = pushToken(tk, last);
    prevType = type;
  }
  return out;
}
