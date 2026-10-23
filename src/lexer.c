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

#include "common.h"

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
      case '\n':
        listLastToken = pushToken(newToken(TK_NEWLINE, line), listLastToken);
        int indentCount = 0;
        while (preloadedChar == false) {
          nextCharInt = fgetc(stdin);
          nextChar = (char)nextCharInt;
          position++;

          if (nextChar == ' ') {
            indentCount++;
          } else {
            preloadedChar = true;
            Token *tk = newToken(TK_INDENT, line);
            setValue(tk, indentCount);
            listLastToken = pushToken(tk, listLastToken);
          }
        }
        break;
      case '@':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar != 'm')
          die(1, line, "Unexpected character at line %d colum %d, expected \"@main\"", line,
              position);
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar != 'a')
          die(1, line, "Unexpected character at line %d colum %d, expected \"@main\"", line,
              position);
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar != 'i')
          die(1, line, "Unexpected character at line %d colum %d, expected \"@main\"", line,
              position);
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar != 'n')
          die(1, line, "Unexpected character at line %d colum %d, expected \"@main\"", line,
              position);
        break;

        // signs
      case '>':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '=') {
          listLastToken = pushToken(newToken(TK_BEQUAL, line), listLastToken);
        } else {
          listLastToken = pushToken(newToken(TK_BIGGER, line), listLastToken);
          preloadedChar = true;
        }
        break;
      case '<':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '=') {
          listLastToken = pushToken(newToken(TK_SMALLER, line), listLastToken);
        } else {
          listLastToken = pushToken(newToken(TK_SEQUAL, line), listLastToken);
          preloadedChar = true;
        }
        break;
      case '=':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '=') {
          listLastToken = pushToken(newToken(TK_EQUAL, line), listLastToken);
        } else {
          listLastToken = pushToken(newToken(TK_ASSIGN, line), listLastToken);
          preloadedChar = true;
        }
        break;
      case '!':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar != '=') {
          die(1, line, "Unexpected character at line %d colum %d, expected \"!=\"", line, position);
        } else {
          listLastToken = pushToken(newToken(TK_NOT_EQUAL, line), listLastToken);
        }
        break;

        // one line tokens
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
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '/') {
          while (nextChar != '\n') {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
          }
          preloadedChar = true;
        } else if (nextChar == '*') {
          bool endComment = false;
          while (!endComment) {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
            if (nextChar == '*') {
              nextCharInt = fgetc(stdin);
              nextChar = (char)nextCharInt;
              position++;
              if (nextChar == '/')
                endComment = true;
            }
          }
        } else {
          listLastToken = pushToken(newToken(TK_DIVIDE, line), listLastToken);
          preloadedChar = true;
        }
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
      case ' ':
        break;
      default:
        die(1, line, "Unexpected character at line %d colum %d", line, position);
        break;
    }
  }
  listLastToken = pushToken(newToken(TK_EOF, line), listLastToken);

  return tokenList;
}
