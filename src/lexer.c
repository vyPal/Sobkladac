/**
 * @file lexer.c
 * Project: IFJ26 compiler
 * @brief Implementation of lexical analyzer functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file implements the functions for creating and manipulating lexical tokens.
 */

#include "lexer.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

void addChar(char **str, char c) {
  size_t len = strlen(*str);
  *str = arenaRealloc(*str, len + 1, len + 2);
  (*str)[len] = c;
  (*str)[len + 1] = '\0';
}

void strEscape(char **str, int *position, int *line) {
  char nextChar;
  int nextCharInt;

  nextCharInt = fgetc(stdin);
  nextChar = (char)nextCharInt;
  (*position)++;

  if (nextCharInt == EOF)
    die(1, *line, "Unexpected end of file while parsing string at line %d colum %d", *line,
        *position);

  switch (nextChar) {
    case '\\':
      addChar(str, '\\');
      break;
    case '"':
      addChar(str, '"');
      break;
    case 'n':
      addChar(str, '\n');
      break;
    case 't':
      addChar(str, '\t');
      break;
    case 'r':
      addChar(str, '\r');
      break;
    default:
      die(1, *line, "Unexpected character after \"\\\" at line %d colum %d", *line, *position);
      break;
  }
}

void numUnderscore(char *nextChar, int *position, int *line, bool isZero) {
  int nextCharInt;

  nextCharInt = fgetc(stdin);
  *nextChar = (char)nextCharInt;
  (*position)++;

  while (*nextChar == '_') {
    nextCharInt = fgetc(stdin);
    *nextChar = (char)nextCharInt;
    (*position)++;
  }

  if ((*nextChar < '0' || *nextChar > '9') && !isZero) {
    die(1, *line, "Unexpected character after \"_\" at line %d colum %d, expected digit", *line,
        *position);
  } else if (*nextChar != '0' && isZero) {
    die(1, *line, "Unexpected character after \"_\" at line %d colum %d, expected \"0\"", *line,
        *position);
  }
}

void numDecimal(LinkedTlist **lastToken, char *nextChar, int integer, int *line, int *position,
                bool *preloadedChar) {
  int nextCharInt;
  double decimal = (double)integer;

  if (*nextChar == '.') {
    nextCharInt = fgetc(stdin);
    *nextChar = (char)nextCharInt;
    (*position)++;
    int decExp = -1;
    while ((*nextChar >= '0' && *nextChar <= '9') || *nextChar == '_') {
      if (*nextChar == '_') {
        numUnderscore(nextChar, position, line, false);
      }
      decimal = decimal + ((*nextChar - '0') * pow(10, decExp));

      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
      decExp--;
    }
  }
  int expSign = 1;
  int exponent = 0;
  if (*nextChar == 'e' || *nextChar == 'E') {
    nextCharInt = fgetc(stdin);
    *nextChar = (char)nextCharInt;
    (*position)++;
    if (*nextChar == '+' || *nextChar == '-') {
      if (*nextChar == '-')
        expSign = -1;

      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
    }

    while ((*nextChar >= '0' && *nextChar <= '9') || *nextChar == '_') {
      if (*nextChar == '_') {
        numUnderscore(nextChar, position, line, false);
      }
      exponent = exponent * 10 + (*nextChar - '0');

      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
    }
  }
  if (*nextChar != 'd' && *nextChar != 'D') {
    *preloadedChar = true;
  }

  decimal *= pow(10, expSign * exponent);

  Token *tk = newToken(TK_DECIMAL, *line);
  union Value *value = arenaAlloc(sizeof(union Value));
  value->d = decimal;
  setValue(tk, value);
  *lastToken = pushToken(tk, *lastToken);
}

LinkedTlist *lexerMain(void) {
  int line = 1;
  int position = 0;

  bool preloadedChar = false;
  char nextChar;
  int nextCharInt = 0;

  LinkedTlist *tokenList = newTlist();
  LinkedTlist *lastToken = tokenList;

  while (nextCharInt != EOF) {
    if (preloadedChar) {
      preloadedChar = false;
      if (feof(stdin))
        break;
    } else {
      nextCharInt = fgetc(stdin);
      if (nextCharInt == EOF)
        break;
      nextChar = (char)nextCharInt;
      position++;
    }

    switch (nextChar) {
      // string
      case '"':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        // multi line string
        if (nextChar == '"') {
          nextCharInt = fgetc(stdin);
          nextChar = (char)nextCharInt;
          position++;
          // empty string
          if (nextChar != '"') {
            preloadedChar = true;
            Token *tk = newToken(TK_STRING, line);
            String *string = newString("", 0);
            union Value *value = arenaAlloc(sizeof(union Value));
            value->s = string;
            setValue(tk, value);
            lastToken = pushToken(tk, lastToken);
          } else {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;

            char *str = arenaAlloc(sizeof(char));
            str[0] = '\0';
            int startLine = line;
            bool endString = false;
            while (!endString) {
              if (nextCharInt == EOF)
                die(1, line, "Unexpected end of file while parsing string at line %d colum %d",
                    line, position);
              else if (nextChar == '\n') {
                line++;
                position = 0;
                addChar(&str, nextChar);
              } else if (nextChar == '"') {
                nextCharInt = fgetc(stdin);
                nextChar = (char)nextCharInt;
                position++;
                if (nextChar == '"') {
                  nextCharInt = fgetc(stdin);
                  nextChar = (char)nextCharInt;
                  position++;
                  if (nextChar == '"') {
                    endString = true;
                  } else {
                    addChar(&str, '"');
                    addChar(&str, '"');
                    addChar(&str, nextChar);
                  }
                } else {
                  addChar(&str, '"');
                  addChar(&str, nextChar);
                }
              } else
                addChar(&str, nextChar);

              nextCharInt = fgetc(stdin);
              nextChar = (char)nextCharInt;
              position++;
            }
            preloadedChar = true;

            // write token
            Token *tk = newToken(TK_STRING, startLine);
            String *string = newString(str, strlen(str));
            union Value *value = arenaAlloc(sizeof(union Value));
            value->s = string;
            setValue(tk, value);
            lastToken = pushToken(tk, lastToken);
          }
        }
        // single line string
        else {
          char *str = arenaAlloc(sizeof(char));
          str[0] = '\0';
          while (nextChar != '"') {
            if (nextCharInt == EOF)
              die(1, line, "Unexpected end of file while parsing string at line %d colum %d", line,
                  position);
            else if (nextChar == '\n') {
              die(1, line, "Unexpected newline while parsing string at line %d colum %d", line,
                  position);
            } else if (nextChar == '\\') {
              strEscape(&str, &position, &line);
            } else
              addChar(&str, nextChar);

            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
          }
          // write token
          Token *tk = newToken(TK_STRING, line);
          String *string = newString(str, strlen(str));
          union Value *value = arenaAlloc(sizeof(union Value));
          value->s = string;
          setValue(tk, value);
          lastToken = pushToken(tk, lastToken);
        }
        break;
      // number
      case '0':
        while (nextChar == '0' || nextChar == '_') {
          nextCharInt = fgetc(stdin);
          nextChar = (char)nextCharInt;
          position++;
          if (nextChar == '_')
            numUnderscore(&nextChar, &position, &line, true);
          break;
        }
        if (nextChar == '.' || nextChar == 'e' || nextChar == 'E') {
          numDecimal(&lastToken, &nextChar, 0, &line, &position, &preloadedChar);
        } else {
          preloadedChar = true;
          Token *tk = newToken(TK_NUMBER, line);
          int zero = 0;
          union Value *value = arenaAlloc(sizeof(union Value));
          value->i = zero;
          setValue(tk, value);
          lastToken = pushToken(tk, lastToken);
        }
        break;
      // decimal
      case '.':
        numDecimal(&lastToken, &nextChar, 0, &line, &position, &preloadedChar);
        break;
      // other
      case '\n':
        lastToken = pushToken(newToken(TK_NEWLINE, line), lastToken);
        line++;
        position = 0;
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
            int indent = indentCount;
            union Value *value = arenaAlloc(sizeof(union Value));
            value->i = indent;
            setValue(tk, value);
            lastToken = pushToken(tk, lastToken);
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
        lastToken = pushToken(newToken(TK_ATMAIN, line), lastToken);
        break;

        // signs
      case '>':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '=') {
          lastToken = pushToken(newToken(TK_BEQUAL, line), lastToken);
        } else {
          lastToken = pushToken(newToken(TK_BIGGER, line), lastToken);
          preloadedChar = true;
        }
        break;
      case '<':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '=') {
          lastToken = pushToken(newToken(TK_SEQUAL, line), lastToken);
        } else {
          lastToken = pushToken(newToken(TK_SMALLER, line), lastToken);
          preloadedChar = true;
        }
        break;
      case '=':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '=') {
          lastToken = pushToken(newToken(TK_EQUAL, line), lastToken);
        } else {
          lastToken = pushToken(newToken(TK_ASSIGN, line), lastToken);
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
          lastToken = pushToken(newToken(TK_NOT_EQUAL, line), lastToken);
        }
        break;

        // one line tokens
      case '+':
        lastToken = pushToken(newToken(TK_PLUS, line), lastToken);
        break;
      case '-':
        lastToken = pushToken(newToken(TK_MINUS, line), lastToken);
        break;
      case '*':
        lastToken = pushToken(newToken(TK_MULTIPLY, line), lastToken);
        break;
      case '/':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '/') {
          while (nextChar != '\n' && nextCharInt != EOF) {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
          }
          preloadedChar = true;
        } else if (nextChar == '*') {
          bool endComment = false;
          char prevChar = 0;
          while (!endComment) {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
            if (prevChar == '*' && nextChar == '/') {
              endComment = true;
            } else if (nextChar == '\n') {
              position = 0;
              line++;
            } else if (nextCharInt == EOF)
              die(1, line, "Unexpected end of file while parsing comment at line %d colum %d", line,
                  position);
            prevChar = nextChar;
          }
        } else {
          lastToken = pushToken(newToken(TK_DIVIDE, line), lastToken);
          preloadedChar = true;
        }
        break;
      case '(':
        lastToken = pushToken(newToken(TK_LPAREN, line), lastToken);
        break;
      case ')':
        lastToken = pushToken(newToken(TK_RPAREN, line), lastToken);
        break;
      case ':':
        lastToken = pushToken(newToken(TK_COLON, line), lastToken);
        break;
      case ';':
        lastToken = pushToken(newToken(TK_SEMICOLON, line), lastToken);
        break;
      case ',':
        lastToken = pushToken(newToken(TK_COMMA, line), lastToken);
        break;
      case '|':
        lastToken = pushToken(newToken(TK_PIPE, line), lastToken);
        break;
      case ' ':
        break;
      default:
        // identifier
        if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z')) {
          char *str = arenaAlloc(2 * sizeof(char));
          str[0] = nextChar;
          str[1] = '\0';
          while (true) {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
            if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z') ||
                (nextChar >= '0' && nextChar <= '9') || nextChar == '_') {
              addChar(&str, nextChar);
            } else {
              preloadedChar = true;
              break;
            }
          }
          Token *tk = newToken(TK_IDENT, line);
          String *string = newString(str, strlen(str));
          union Value *value = arenaAlloc(sizeof(union Value));
          value->s = string;
          setValue(tk, value);
          lastToken = pushToken(tk, lastToken);
        }
        // number
        else if (nextChar >= '1' && nextChar <= '9') {
          int integer = nextChar - '0';
          nextCharInt = fgetc(stdin);
          nextChar = (char)nextCharInt;
          position++;
          while ((nextChar >= '0' && nextChar <= '9') || nextChar == '_') {
            if (nextChar == '_')
              numUnderscore(&nextChar, &position, &line, false);
            integer = integer * 10 + (nextChar - '0');

            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
          }
          if (nextChar == '.' || nextChar == 'e' || nextChar == 'E') {
            numDecimal(&lastToken, &nextChar, integer, &line, &position, &preloadedChar);
          } else {
            preloadedChar = true;
            Token *tk = newToken(TK_NUMBER, line);
            union Value *value = arenaAlloc(sizeof(union Value));
            value->i = integer;
            setValue(tk, value);
            lastToken = pushToken(tk, lastToken);
          }
        } else
          die(1, line, "Unexpected character at line %d colum %d", line, position);
        break;
    }
  }
  pushToken(newToken(TK_EOF, line), lastToken);

  return tokenList;
}
