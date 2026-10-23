/**
 * @file lexer.c
 * Project: IFJ26 compiler
 * @brief Implementation of lexical analyzer functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file implements the functions for creating and manipulating lexical tokens.
 */

#include "lexer.h"

#include <stdbool.h>
#include <stdio.h>

LinkedTlist *lexerMain() {
  int line = 1;
  int position = 0;

  bool preloadedChar;
  char nextChar;
  int nextCharInt;

  LinkedTlist *tokenList = newTlist();
  LinkedTlist *listLastToken = tokenList;

  while (nextCharInt != EOF) {
    if (preloadedChar) {
      preloadedChar = false;
    } else {
      nextCharInt = fgetc(stdin);
      nextChar = (char)nextCharInt;
      position++;
    }

    switch (nextChar) {
      case '+':
        listLastToken = pushToken(newToken(TK_PLUS, line), listLastToken);
        break;
      case '-':
        listLastToken = pushToken(newToken(TK_MINUS, line), listLastToken);
        break;
      case '*':
        listLastToken = pushToken(newToken(TK_MULTIPLY, line), listLastToken);
        break;
      case '/':
        listLastToken = pushToken(newToken(TK_DIVIDE, line), listLastToken);
        break;
      case '(':
        listLastToken = pushToken(newToken(TK_LPAREN, line), listLastToken);
        break;
      case ')':
        listLastToken = pushToken(newToken(TK_RPAREN, line), listLastToken);
        break;
      case ':':
        listLastToken = pushToken(newToken(TK_COLON, line), listLastToken);
        break;
      case ';':
        listLastToken = pushToken(newToken(TK_SEMICOLON, line), listLastToken);
        break;
      case ',':
        listLastToken = pushToken(newToken(TK_COMMA, line), listLastToken);
        break;
      default:
        break;
    }
  }
  listLastToken = pushToken(newToken(TK_EOF, line), listLastToken);

  return NULL;
}
