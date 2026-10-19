/**
 * @file token.h
 * Project: IFJ26 compiler
 * @brief Lexical token structure and functions
 * @author Vojtech Vitek <xvitekv00>
 * 
 * This file defines the structure of a lexical token and provides functions to create and manipulate tokens.
 */

 /**
  * @brief Enumeration of token types.
  */
enum TkType {
    Ident,      // *char
    Number,     // int
    Decimal,    // double
    String,     // *char
    // operators
    Plus,       // +
    Minus,      // -
    Multiply,   // *
    Divide,     // /
    Bigger,     // >
    Smaller,    // <
    Equal,      // ==
    Sequal,     // <=
    Bequal,     // >=
    NotEqual,   // !=
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
 * @brief Enumeration of variable types. 
*/
enum VarType {
    None,
    Int,
    Double,
    String
} VarType;

/** 
 * @brief Union for storing values of different types.
 */
union Value {
    int i;
    double d;
    char *string;
} Value;

/** 
 * @brief Structure for representing a lexical token.
 */
struct Token {
    enum TkType type;
    enum VarType varType;
    int varIndex;
    int strLenght;
} Token;

/** 
 * @brief Structure for representing a linked list of tokens.
 */
struct LinkedTlist {
    struct Token *token;
    struct LinkedTlist *next;
} LinkedTlist;

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
void setValue(struct Token *token, enum VarType varType, union Value value);

/**
 * @brief Sets the string value of a token.
 * @param token Token to set the string value for.
 * @param varType Type of the variable.
 * @param value String value to set.
 * @param strLenght Length of the string.
 */
void setValueString(struct Token *token, enum VarType varType, union Value value, int strLenght);

/**
 * @brief Gets the type of a token.
 * @param token Token to get the type for.
 * @return Type of the token.
 */
enum TkType getTkType(struct Token *token);

/**
 * @brief Gets the variable type of a token.
 * @param token Token to get the variable type for.
 * @return Variable type of the token.
 */
enum VarType getVarType(struct Token *token);

/**
 * @brief Gets the index of a token.
 * @param token Token to get the index for.
 * @return Index of the token.
 */
int getIndex(struct Token *token);

/**
 * @brief Gets the string length of a token.
 * @param token Token to get the string length for.
 * @return String length of the token.
 */
int getStrLenght(struct Token *token);

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
struct LinkedTlist *addTokenEnd(struct Token *token, struct LinkedTlist *last);

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