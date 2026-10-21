/**
 * @file ast.h
 * Project: IFJ26 compiler
 * @brief Implementation of AST header
 * @author David Sobek <xsobekd00>
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
#include <stdio.h>

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
  UNIT     // nothing
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

AstProgram *astProgramNew(void);
AstFunction *astFunctionNew(const char *name, AstLocation location);
AstFunctionParameter *astFunctionParameterNew(const char *name, AstDataType type);
AstCodeBlock *astCodeBlockNew(void);
AstCommand *astCodeCommandNew(AstCommandType type, AstLocation location);
AstExpression *astExpressionNew(AstExpressionType type, AstLocation location);
AstFunctionCall *astFunctionCallNew(const char *name, AstLocation location);
AstFunctionCallArgument *astFunctionCallArgumentNew(AstExpression *expression);
AstValue *astValueFromExpr(AstExpression *expression);
AstValue *astValueFromFunctionCall(AstFunctionCall *functionCall);

void astProgramAppendFunction(AstProgram *program, AstFunction *function);
void astFunctionAppendParameter(AstFunction *function, AstFunctionParameter *parameter);
void astBlockAppendCommand(AstCodeBlock *block, AstCommand *command);
void astCallAppendArgument(AstFunctionCall *call, AstFunctionCallArgument *argument);

void astFreeProgram(AstProgram *program);

#endif  // AST_H
