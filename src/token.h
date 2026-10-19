/**
 * @file token.h
 * Project: IFJ26 compiler
 * @brief Lexical token structure and functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file defines the structure of a lexical token and provides functions to create and
 * manipulate tokens.
 */

#ifndef TOKEN_H
#define TOKEN_H

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
  Indent,     // int (number of spaces)
  AtMain      // @main
};

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
  struct String *s;
};

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
 * @param value Value to set.
 * @param line Number of line the token is on.
 */
void setValue(struct Token *token, union Value *value, int line);

/**
 * @brief Gets the type of a token.
 * @param token Token to get the type for.
 * @return Type of the token.
 */
enum TkType getTkType(const struct Token *token);

/**
 * @brief Gets the value stored in a token.
 * @param token Token to get the value from.
 * @return Pointer to the token's value.
 */
union Value *getValue(const struct Token *token);

/**
 * @brief Gets the line number of a token.
 * @param token Token to get the line number from.
 * @return Line number of the token.
 */
int getLine(const struct Token *token);

// STRING FUNCTIONS

/**
 * @brief Creates a string with the specified contents and length.
 * @param string String contents.
 * @param length Length of the string.
 * @return Pointer to the new string.
 */
struct String *newString(char *string, int length);

/**
 * @brief Gets the contents of a string.
 * @param string String to retrieve the contents from.
 * @return Pointer to the string contents.
 */
char *getString(const struct String *string);

/**
 * @brief Gets the length of a string.
 * @param string String to get the length of.
 * @return Length of the string.
 */
int getStringLenght(const struct String *string);

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
 * @note This function is not expected to be used after the program starts using popToken function.
 * @note User is expected to keep pointer to both head and tail of the list.
 */
struct LinkedTlist *pushToken(struct Token *token, struct LinkedTlist *last);

/**
 * @brief Removes and returns the first token from a token list.
 * @param list Token list to remove the token from.
 * @return Pointer to the removed token.
 * @note When program starts using this function it isnt expected to use pushToken function ever
 * again.
 */
struct Token *popToken(struct LinkedTlist *list);

/**
 * @brief Looks ahead in the token list and returns the nth token without removing it.
 * @param list Token list to look ahead in.
 * @param n Number of tokens to look ahead, head of the list is 0.
 * @return Pointer to the nth token in the list.
 */
struct Token *lookAheadToken(struct LinkedTlist *list, int n);

#endif  // TOKEN_H
