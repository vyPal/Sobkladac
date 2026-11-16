/**
 * @file ast.h
 * Project: IFJ26 compiler
 * @brief Implementation of AST header
 * @author David Sobek <xsobekd00>
 * @author Jakub Palacky <xpalacj00>
 *
 * Brief explanation:
 *  PROGRAM is split into FUNCTIONS
 *  there is one MAIN FUNCTION
 *  FUNCTION body can be either a VALUE or a CODE BLOCK
 *  a VALUE can be either an EXPRESSION or a FUNCTION CALL
 *  an EXPRESSION can be a number, string, variable or an operator
 *  a CODE BLOCK refers to multiple COMMANDS with the same indentation
 *  one line of IFJ26 code contains up to one COMMAND
 */

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct AstProgram AstProgram;
typedef struct AstFunction AstFunction;
typedef struct AstFunctionParameter AstFunctionParameter;
typedef struct AstCodeBlock AstCodeBlock;
typedef struct AstCommand AstCommand;
typedef struct AstExpression AstExpression;
typedef struct AstFunctionCall AstFunctionCall;
typedef struct AstFunctionCallArgument AstFunctionCallArgument;
typedef struct AstValue AstValue;

/// @brief source code position
typedef struct {
  unsigned line;
  unsigned column;
} AstLocation;

/// @brief function body can be a value or a code block
typedef enum {
  VALUE,      // one line
  CODE_BLOCK  // multiple lines
} AstFunctionBodyType;

/// @brief IFJ26 command types
typedef enum {
  VARIABLE_DECLARATION,
  ASSIGNMENT,
  FUNCTION_CALL,
  RETURN,
  IF,
  WHILE,
  EXPRESSION
} AstCommandType;

/// @brief IFJ26 values can be expressions or function calls
typedef enum { VALUE_EXPRESSION, VALUE_FUNCTION_CALL } AstValueType;

/// @brief IFJ26 data types
typedef enum {
  UNKNOWN,
  INT,     // 32bit signed integer
  DOUBLE,  // 64bit float IEEE 754
  STRING,  // unmodifiable character list
  BOOL,    // true/false
  UNIT,    // nothing
  ANY      // for built in functions only
} AstDataType;

/// @brief IFJ26 operators
typedef enum {
  NEGATION,        // -
  MULTIPLICATION,  // *
  DIVISION,        // "/"
  ADDITION,        // +
  SUBTRACTION,     // -
  LESSER,          // <
  LESSER_EQUAL,    // <=
  GREATER,         // >
  GREATER_EQUAL,   // >=
  EQUAL,           // ==
  NOT_EQUAL        // !=
} AstOperator;

typedef enum {
  INT_LITERAL,
  DOUBLE_LITERAL,
  STRING_LITERAL,
  UNIT_LITERAL,
  VARIABLE,
  UNARY,
  BINARY
} AstExpressionType;

/// @brief IFJ26 expression
struct AstExpression {
  AstExpressionType expressionType;  // determines union
  AstLocation location;
  AstDataType deducedDataType;  // for semantic analysis

  union {
    int32_t intValue;    // INT_LITERAL
    double doubleValue;  // DOUBLE_LITERAL
    char *stringValue;   // STRING_LITERAL
    bool boolValue;      // BOOL

    // UNIT_LITERAL (no data)

    char *variable;  // VARIABLE

    struct {  // UNARY
      AstOperator operatorAst;
      AstExpression *expression;
    } unary;

    struct {  // BINARY
      AstOperator operatorAst;
      AstExpression *leftExpression;
      AstExpression *rightExpression;
    } binary;
  } data;
};

/// @brief IFJ26 function call argument
struct AstFunctionCallArgument {
  AstExpression *expression;
  AstFunctionCallArgument *nextArgument;
};

/// @brief IFJ26 function call
struct AstFunctionCall {
  char *functionName;
  AstLocation location;

  // arguments stored as linked-list
  AstFunctionCallArgument *firstArgument;
  AstFunctionCallArgument *lastArgument;
};

/// @brief IFJ26 values can be expressions or function calls
struct AstValue {
  AstValueType type;  // determines union
  union {
    AstExpression *expression;
    AstFunctionCall *functionCall;
  } data;
};

/// @brief one line of IFJ26 code has up to one command
struct AstCommand {
  AstCommandType type;  // determines union
  AstLocation location;
  AstCommand *nextCommand;

  union {
    struct {  // VARIABLE_DECLARATION
      char *variableName;
      bool isMutable;  // false == val, true == var
      AstDataType declaredType;
      AstDataType resolvedType;
      AstValue *value;
    } variableDeclaration;

    struct {  // ASSIGNMENT
      char *variableName;
      AstValue *value;
    } assignment;

    struct {  // FUNCTION_CALL
      AstFunctionCall *functionCall;
    } call;

    struct {  // RETURN
      AstValue *value;
    } returnAst;

    struct {  // IF
      AstExpression *condition;
      AstCodeBlock *thenBlock;
      AstCodeBlock *elseBlock;
    } ifAst;

    struct {  // WHILE
      AstExpression *condition;
      AstCodeBlock *codeBlock;
    } whileAst;

    struct {  // EXPRESSION
      AstExpression *expression;
    } expressionAst;
  } data;
};

/// @brief a block of code in a linked list
struct AstCodeBlock {
  AstCommand *firstCommand;
  AstCommand *lastCommand;
};

/// @brief IFJ26 function parameter
struct AstFunctionParameter {
  char *parameterName;
  AstDataType dataType;
  AstFunctionParameter *nextParameter;
};

/// @brief IFJ26 function
struct AstFunction {
  char *functionName;
  bool isMain;  // @main
  AstLocation location;

  AstFunctionParameter *firstParameter;
  AstFunctionParameter *lastParameter;

  AstDataType declaredReturnType;
  AstDataType resolvedReturnType;

  AstFunctionBodyType bodyType;  // determines union
  union {
    AstValue *value;          // VALUE
    AstCodeBlock *codeBlock;  // BLOCK
  } body;

  AstFunction *nextFunction;
};

/// @brief whole program
struct AstProgram {
  AstFunction *firstFunction;
  AstFunction *lastFunction;
};

/// Allocates a zero-initialized AstProgram
AstProgram *astProgramNew(void);

/// Allocates a zero-initialized AstFunction
/// @param name Name of function
/// @param location Location of function
/// @warning name must be NULL terminated
/// @note name will be duplicated into the arena to avoid use-after-free
AstFunction *astFunctionNew(const char *name, AstLocation location);

/// Allocates a zero-initialized AstFunctionParameter
/// @param name Name of function parameter
/// @param type DataType of parameter
/// @warning name must be NULL terminated
/// @note name will be duplicated into the arena to avoid use-after-free
AstFunctionParameter *astFunctionParameterNew(const char *name, AstDataType type);

/// Allocates a zero-initialized AstCodeBlock
AstCodeBlock *astCodeBlockNew(void);

/// Allocates a zero-initialized AstCommand
/// @param type Type of command/statement
/// @param location Location of statement
AstCommand *astCodeCommandNew(AstCommandType type, AstLocation location);

/// Allocates a zero-initialized AstExpression
/// @param type Type of expression
/// @param location Location of expression
AstExpression *astExpressionNew(AstExpressionType type, AstLocation location);

/// Allocates a zero-initialized AstFunctionCall
/// @param name Name of function to be called
/// @param location Location of function call
/// @warning name must be NULL terminated
/// @note name will be duplicated into the arena to avoid use-after-free
AstFunctionCall *astFunctionCallNew(const char *name, AstLocation location);

/// Allocates a zero-initialized AstFunctionCallArgument
/// @param expression Expression that will act as the call's argument
AstFunctionCallArgument *astFunctionCallArgumentNew(AstExpression *expression);

/// Allocates a zero-initialized AstValue with type VALUE_EXPRESSION
/// @param expression Expression to use as value's data
AstValue *astValueFromExpr(AstExpression *expression);

/// Allocates a zero-initialized AstValue with type VALUE_FUNCTION_CALL
/// @param functionCall Function call to use as value's data
AstValue *astValueFromFunctionCall(AstFunctionCall *functionCall);

/// Appends function to program's list of functions
/// @note Supplying a NULL program or function kills the compiler
void astProgramAppendFunction(AstProgram *program, AstFunction *function);

//// Appends parameter to function's paramter list
/// @note Supplying a NULL function or parameter kills the compiler
void astFunctionAppendParameter(AstFunction *function, AstFunctionParameter *parameter);

/// Appends commands to code block's command list
/// @note Supplying a NULL block or command kills the compiler
void astBlockAppendCommand(AstCodeBlock *block, AstCommand *command);

/// Appends argument to function call's list of arguments
/// @note Supplying a NULL call or argument kills the compiler
void astCallAppendArgument(AstFunctionCall *call, AstFunctionCallArgument *argument);

#endif  // AST_H
