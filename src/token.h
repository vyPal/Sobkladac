/**
 * @file token.h
 * Project: IFJ26 compiler
 * @brief Lexical token structure and functions
 * @author Vojtěch Vítek <xvitekv00>
 * 
 * This file defines the structure of a lexical token and provides functions to create and manipulate tokens.
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

enum VarType {
    None,
    Int,
    Double,
    String
} VarType;

union Value {
    int i;
    double d;
    char *string;
} Value;

struct Token {
    enum TkType type;
    enum VarType varType;
    int varIndex;
    int strLenght;
} Token;

/*
 * Creates a new token with the specified type.
 * @param type Type of the token to create.
 * @return Pointer to the new token.
 */
struct Token *newToken(enum TkType type);

/*
 * Sets the value of a token.
 * @param token Token to set the value for.
 * @param varType Type of the variable.
 * @param value Value to set.
 */
void setValue(struct Token *token, enum VarType varType, union Value value);

/*
 * Sets the string value of a token.
 * @param token Token to set the string value for.
 * @param varType Type of the variable.
 * @param value String value to set.
 * @param strLenght Length of the string.
 */
void setValueString(struct Token *token, enum VarType varType, union Value value, int strLenght);

/*
 * Gets the type of a token.
 * @param token Token to get the type for.
 * @return Type of the token.
 */
enum TkType getTkType(struct Token *token);

/*
 * Gets the variable type of a token.
 * @param token Token to get the variable type for.
 * @return Variable type of the token.
 */
enum VarType getVarType(struct Token *token);

/*
 * Gets the index of a token.
 * @param token Token to get the index for.
 * @return Index of the token.
 */
int getIndex(struct Token *token);

/*
 * Gets the string length of a token.
 * @param token Token to get the string length for.
 * @return String length of the token.
 */
int getStrLenght(struct Token *token);
