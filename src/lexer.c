/**
 * @file lexer.c
 * Project: IFJ26 compiler
 * @brief Implementation of lexical analyzer functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file implements the functions for creating and manipulating lexical tokens.
 */

#include "lexer.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "strbuf.h"

void strEscape(StringBuilder *sb, int *position, int *line) {
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
      sbAppendChar(sb, '\\');
      break;
    case '"':
      sbAppendChar(sb, '"');
      break;
    case 'n':
      sbAppendChar(sb, '\n');
      break;
    case 't':
      sbAppendChar(sb, '\t');
      break;
    case 'r':
      sbAppendChar(sb, '\r');
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

void numDecimal(LinkedTlist **lastToken, char prevChar, char *nextChar, StringBuilder *lexeme,
                int *line, int *position, bool *preloadedChar) {
  int nextCharInt;

  if (prevChar == '.') {
    if (prevChar == *nextChar) {
      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
    }
    sbAppendChar(lexeme, '.');
    if (*nextChar < '0' || *nextChar > '9')
      die(1, *line, "Unexpected character at line %d colum %d, expected digit", *line, *position);
    while ((*nextChar >= '0' && *nextChar <= '9') || *nextChar == '_') {
      if (*nextChar == '_') {
        numUnderscore(nextChar, position, line, false);
      }
      sbAppendChar(lexeme, *nextChar);

      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
    }
    prevChar = *nextChar;
  }
  if (prevChar == 'e' || prevChar == 'E') {
    if (prevChar == *nextChar) {
      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
    }
    sbAppendChar(lexeme, 'e');
    if (*nextChar == '+' || *nextChar == '-') {
      sbAppendChar(lexeme, *nextChar);

      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
    }

    if (*nextChar < '0' || *nextChar > '9')
      die(1, *line, "Unexpected character at line %d colum %d, after E/e there must be a digit",
          *line, *position);

    while ((*nextChar >= '0' && *nextChar <= '9') || *nextChar == '_') {
      if (*nextChar == '_') {
        numUnderscore(nextChar, position, line, false);
      }
      sbAppendChar(lexeme, *nextChar);

      nextCharInt = fgetc(stdin);
      *nextChar = (char)nextCharInt;
      (*position)++;
    }
  }
  if (*nextChar != 'd' && *nextChar != 'D') {
    *preloadedChar = true;
    if ((*nextChar >= 'a' && *nextChar <= 'z') || (*nextChar >= 'A' && *nextChar <= 'Z'))
      die(1, *line, "Unexpected character at line %d colum %d", *line, *position);
  }

  double decimal = strtod(sbBuild(lexeme), NULL);
  if (isinf(decimal))
    die(1, *line, "Decimal literal out of range at line %d colum %d", *line, *position);

  Token *tk = newToken(TK_DECIMAL, *line);
  union Value *value = arenaAlloc(sizeof(union Value));
  (*value).d = decimal;
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

            StringBuilder sb = sbNew(0);
            int startLine = line;
            bool endString = false;
            while (!endString) {
              if (nextCharInt == EOF)
                die(1, line, "Unexpected end of file while parsing string at line %d colum %d",
                    line, position);
              else if (nextChar == '\n') {
                line++;
                position = 0;
                sbAppendChar(&sb, nextChar);
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
                    sbAppendChar(&sb, '"');
                    sbAppendChar(&sb, '"');
                    sbAppendChar(&sb, nextChar);
                    if (nextChar == '\n') {
                      line++;
                      position = 0;
                    }
                  }
                } else {
                  sbAppendChar(&sb, '"');
                  sbAppendChar(&sb, nextChar);
                  if (nextChar == '\n') {
                    line++;
                    position = 0;
                  }
                }
              } else
                sbAppendChar(&sb, nextChar);

              nextCharInt = fgetc(stdin);
              nextChar = (char)nextCharInt;
              position++;
            }
            if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z') ||
                (nextChar >= '0' && nextChar <= '9'))
              die(1, line, "Unexpected character at line %d colum %d", line, position);
            preloadedChar = true;

            // write token
            Token *tk = newToken(TK_STRING, startLine);
            char *built = sbBuild(&sb);
            String *string = newString(built, sb.len);
            union Value *value = arenaAlloc(sizeof(union Value));
            value->s = string;
            setValue(tk, value);
            lastToken = pushToken(tk, lastToken);
          }
        }
        // single line string
        else {
          StringBuilder sb = sbNew(0);
          while (nextChar != '"') {
            if (nextCharInt == EOF)
              die(1, line, "Unexpected end of file while parsing string at line %d colum %d", line,
                  position);
            else if (nextChar == '\n') {
              die(1, line, "Unexpected newline while parsing string at line %d colum %d", line,
                  position);
            } else if (nextChar == '\\') {
              strEscape(&sb, &position, &line);
            } else
              sbAppendChar(&sb, nextChar);

            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
          }

          nextCharInt = fgetc(stdin);
          nextChar = (char)nextCharInt;
          position++;
          if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z') ||
              (nextChar >= '0' && nextChar <= '9'))
            die(1, line, "Unexpected character at line %d colum %d", line, position);
          preloadedChar = true;

          // write token
          Token *tk = newToken(TK_STRING, line);
          char *built = sbBuild(&sb);
          String *string = newString(built, sb.len);
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
        }
        if (nextChar == '.' || nextChar == 'e' || nextChar == 'E') {
          StringBuilder lex = sbNew(0);
          sbAppendChar(&lex, '0');
          numDecimal(&lastToken, nextChar, &nextChar, &lex, &line, &position, &preloadedChar);
        } else if (nextChar >= '0' && nextChar <= '9') {
          die(1, line, "Unexpected character at line %d colum %d, number cant start with 0", line,
              position);
        } else if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z')) {
          die(1, line, "Unexpected character at line %d colum %d", line, position);
        } else {
          preloadedChar = true;
          Token *tk = newToken(TK_NUMBER, line);
          int zero = 0;
          union Value *value = arenaAlloc(sizeof(union Value));
          (*value).i = zero;
          setValue(tk, value);
          lastToken = pushToken(tk, lastToken);
        }
        break;
      // decimal
      case '.':
        nextCharInt = fgetc(stdin);
        char prev = nextChar;
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar >= '0' && nextChar <= '9') {
          StringBuilder lex = sbNew(0);
          numDecimal(&lastToken, prev, &nextChar, &lex, &line, &position, &preloadedChar);
        } else {
          preloadedChar = true;
          lastToken = pushToken(newToken(TK_DOT, line), lastToken);
        }
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
            continue;
          }

          if (nextChar == '\t') {
            // tabulátor je chyba jen na neprázdném řádku
            int c = nextCharInt;
            while (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
              c = fgetc(stdin);
              position++;
            }
            if (c != '\n' && c != EOF)
              die(1, line, "Tab in indentation at line %d column %d", line, position);
            // prázdný řádek: pokračuje se od \n / EOF
            nextCharInt = c;
            nextChar = (char)c;
            indentCount = 0;
          }

          preloadedChar = true;
          Token *tk = newToken(TK_INDENT, line);
          union Value *value = arenaAlloc(sizeof(union Value));
          (*value).i = indentCount;
          setValue(tk, value);
          lastToken = pushToken(tk, lastToken);
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
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar != ' ' && nextChar != '\n' && nextCharInt != EOF)
          die(1, line, "Unexpected character at line %d colum %d, expected \" \" after \"@main\"",
              line, position);
        if (nextChar == '\n' || nextCharInt == EOF)
          preloadedChar = true;
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
        if (nextChar == '=') {
          lastToken = pushToken(newToken(TK_NOT_EQUAL, line), lastToken);
        } else {
          lastToken = pushToken(newToken(TK_NOT, line), lastToken);
          preloadedChar = true;
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
          int depth = 1;
          bool newLine = false;
          char prevChar = 0;
          while (depth > 0) {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
            if (prevChar == '*' && nextChar == '/') {
              depth--;
            } else if (prevChar == '/' && nextChar == '*') {
              depth++;
            } else if (nextChar == '\n') {
              position = 0;
              line++;
              newLine = true;
            } else if (nextCharInt == EOF)
              die(1, line, "Unexpected end of file while parsing comment at line %d colum %d", line,
                  position);
            prevChar = nextChar;
          }
          nextCharInt = fgetc(stdin);
          nextChar = (char)nextCharInt;
          position++;

          if (newLine) {
            while (nextChar == ' ') {
              nextCharInt = fgetc(stdin);
              nextChar = (char)nextCharInt;
              position++;
            }
            // only newline, EOF or another comment may follow a multiline comment
            bool ok = (nextCharInt == EOF || nextChar == '\n');
            if (nextChar == '/') {
              int next2 = fgetc(stdin);
              ok = (next2 == '/' || next2 == '*');
              ungetc(next2, stdin);  // let the main loop see it again
            }
            if (!ok)
              die(1, line,
                  "Unexpected character at line %d colum %d, no token is allowed after multiline "
                  "comment",
                  line, position);
          }
          preloadedChar = true;
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
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '|') {
          lastToken = pushToken(newToken(TK_OR, line), lastToken);
        } else {
          preloadedChar = true;
          lastToken = pushToken(newToken(TK_PIPE, line), lastToken);
        }
        break;
      case '&':
        nextCharInt = fgetc(stdin);
        nextChar = (char)nextCharInt;
        position++;
        if (nextChar == '&')
          lastToken = pushToken(newToken(TK_AND, line), lastToken);
        else
          die(1, line, "Unexpected character at line %d colum %d, expected \"&&\"", line, position);
        break;
      case '\t':
        if (line == 1 && lastToken->token == NULL) {
          int c = nextCharInt;
          while (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            c = fgetc(stdin);
            position++;
          }

          if (c != '\n' && c != EOF)
            die(1, line, "Tab in indentation at line %d column %d", line, position);

          nextCharInt = c;
          nextChar = (char)c;
          preloadedChar = true;
        }
        break;
      case ' ':
      case '\r':
      case '\f':
      case '\v':
        break;
      default:
        // identifier
        if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z') ||
            nextChar == '_') {
          StringBuilder sb = sbNew(0);
          sbAppendChar(&sb, nextChar);
          bool isUnderscore = (nextChar == '_');
          // identifier
          while (true) {
            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
            if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z') ||
                (nextChar >= '0' && nextChar <= '9') || nextChar == '_') {
              sbAppendChar(&sb, nextChar);
              isUnderscore = false;
            } else if (isUnderscore) {
              die(1, line, "Unexpected character at line %d colum %d", line, position);
            } else {
              preloadedChar = true;
              break;
            }
          }
          Token *tk = newToken(TK_IDENT, line);
          char *built = sbBuild(&sb);
          String *string = newString(built, sb.len);
          union Value *value = arenaAlloc(sizeof(union Value));
          value->s = string;
          setValue(tk, value);
          lastToken = pushToken(tk, lastToken);
        }
        // number
        else if (nextChar >= '1' && nextChar <= '9') {
          StringBuilder lex = sbNew(0);
          sbAppendChar(&lex, nextChar);
          nextCharInt = fgetc(stdin);
          nextChar = (char)nextCharInt;
          position++;
          while ((nextChar >= '0' && nextChar <= '9') || nextChar == '_') {
            if (nextChar == '_')
              numUnderscore(&nextChar, &position, &line, false);
            sbAppendChar(&lex, nextChar);

            nextCharInt = fgetc(stdin);
            nextChar = (char)nextCharInt;
            position++;
          }
          if (nextChar == '.' || nextChar == 'e' || nextChar == 'E') {
            numDecimal(&lastToken, nextChar, &nextChar, &lex, &line, &position, &preloadedChar);
          } else if ((nextChar >= 'a' && nextChar <= 'z') || (nextChar >= 'A' && nextChar <= 'Z')) {
            die(1, line, "Unexpected character at line %d colum %d", line, position);
          } else {
            preloadedChar = true;
            double integer = strtod(sbBuild(&lex), NULL);
            if (integer > INT_MAX)
              die(1, line, "Integer literal out of range at line %d colum %d", line, position);
            Token *tk = newToken(TK_NUMBER, line);
            union Value *value = arenaAlloc(sizeof(union Value));
            (*value).i = (int)integer;
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
