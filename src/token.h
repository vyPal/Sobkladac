/**
 * @file token.h
 * Project: IFJ26 compiler
 * @brief Lexical token structure and functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file defines the structure of a lexical token and provides functions to create and
 * manipulate tokens.
 */

#ifndef _TOKEN_H
#define _TOKEN_H

/**
 * @brief Enumeration of token types.
 */
enum TkType {
  Ident,     // *char
  Number,    // int
  Decimal,   // double
  StringTk,  // *char
  // operators
  Plus,      // +
  Minus,     // -
  Multiply,  // *
  Divide,    // /
  Bigger,    // >
  Smaller,   // <
  Equal,     // ==
  Sequal,    // <=
  Bequal,    // >=
  NotEqual,  // !=
  // others
  Colon,      // :
  Semicolon,  // ;
  Comma,      // ,
  Assign,     // =
  Pipe,       // |
  Lbracket,   // (
  Rbracket,   // )
  NewLine,    // \n
  Indent,     // tab
  AtMain      // @main
} TkType;

/**
 * @brief Structure for representing a string.
 */
typedef struct String {
  char *string;
  int length;
} String;

/**
 * @brief Union for storing values of different types.
 */
union Value {
  int i;
  double d;
  struct String s;
} Value;

/**
 * @brief Structure for representing a lexical token.
 */
typedef struct Token Token;

/**
 * @brief Structure for representing a linked list of tokens.
 */
typedef struct LinkedTlist LinkedTlist;

// TOKEN FUNCTIONS
/**
 * @brief Creates a new token with the specified type.
 * @param type Type of the token to create.
 * @return Pointer to the new token.
 */
struct Token *newToken(enum TkType type);

/**
 * @brief Sets the value of a token.
 * @param token Token to set the value for.
 * @param varType Type of the variable.
 * @param value Value to set.
 */
void setValue(struct Token *token, union Value *value);

/**
 * @brief Gets the type of a token.
 * @param token Token to get the type for.
 * @return Type of the token.
 */
enum TkType getTkType(struct Token *token);

/**
 * @brief Gets the index of a token.
 * @param token Token to get the index for.
 * @return Index of the token.
 */
union Value *getValue(struct Token *token);

// STRING FUNCTIONS

struct String *newString(char *string, int length);

char *getString(struct String *string);

int getStringLenght(struct String *string);

// LINKED LIST FUNCTIONS

/**
 * @brief Creates a new token list.
 * @return Pointer to the new token list.
 */
struct LinkedTlist *newTlist();

/**
 * @brief Adds a token to the end of a token list.
 * @param token Token to add.
 * @param last Last node in the token list.
 * @return Pointer to the last node in the token list.
 */
struct LinkedTlist *pushToken(struct Token *token, struct LinkedTlist *last);

/**
 * @brief Removes and returns the first token from a token list.
 * @param list Token list to remove the token from.
 * @return Pointer to the removed token.
 */
struct Token *popToken(struct LinkedTlist *list);

/**
 * @brief Gets the next token from a token list.
 * @param list Token list to get the token from.
 * @return Pointer to the next token.
 */
struct Token *getNextToken(struct LinkedTlist *list);

#endif  // _TOKEN_H
