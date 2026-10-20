#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/// @brief IFJ26 data types
typedef enum {
  UNKNOWN,
  INT,     // 32bit signed integer
  DOUBLE,  // 64bit float IEEE 754
  STRING,  // unmodifiable character list
  UNIT     // nothing
} Ast_data_type;

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
} Ast_operator;

/// @brief source code position
typedef struct {
  unsigned line;
  unsigned column;
} Ast_location;

// TODO remove:
typedef struct Ast_program Ast_program;
typedef struct Ast_function Ast_function;
typedef struct Ast_function_parameter Ast_function_parameter;
typedef struct Ast_code_block Ast_code_block;
typedef struct Ast_code_line Ast_code_line;
typedef struct Ast_expression Ast_expression;
typedef struct Ast_function_call Ast_function_call;
typedef struct Ast_function_call_argument Ast_function_call_argument;
typedef struct Ast_value Ast_value;

/// @brief Ast_expression union type
typedef enum {
  INT_LITERAL,
  DOUBLE_LITERAL,
  STRING_LITERAL,
  UNIT_LITERAL,
  VARIABLE,
  UNARY,
  BINARY
} Ast_expression_type;

/// @brief IFJ26 expression
typedef struct Ast_expression {
  Ast_expression_type expression_type;  // determines union
  Ast_location location;
  Ast_data_type deduced_data_type;  // for sematic analysis

  union {
    int32_t int_value;    // INT_LITERAL
    double double_value;  // DOUBLE_LITERAL
    char *string_value;   // STRING_LITERAL
    // UNIT_LITERAL (no data)

    char *variable;  // VARIABLE

    struct {  // UNARY
      Ast_operator operator_ast;
      Ast_expression *expression;
    } unary;

    struct {  // BINARY
      Ast_operator operator_ast;
      Ast_expression *left_expression;
      Ast_expression *right_expression;
    } binary;
  } data;
} Ast_expression;

/// @brief IFJ26 function call argument
struct Ast_function_call_argument {
  Ast_expression *expression;
  Ast_function_call_argument *next_argument;
};

/// @brief IFJ26 function call
struct Ast_function_call {
  char *function_name;
  Ast_location location;

  // arguments stored as linked-list
  Ast_function_call_argument *first_argument;
  Ast_function_call_argument *last_argument;
};

/// @brief IFJ26 values can be expressions or function calls
typedef enum { EXPRESSION, FUNCTION_CALL } Ast_value_type;

/// @brief IFJ26 values can be expressions or function calls
struct Ast_value {
  Ast_value_type type;  // determines union
  union {
    Ast_expression *expression;
    Ast_function_call *function_call;
  } data;
};

/// @brief what is on a single line of IFJ26 code
typedef enum {
  VARIABLE_DECLARATION,
  ASSIGNEMENT,
  FUNCTION_CALL,
  RETURN,
  IF,
  WHILE,
  EXPRESSION
} Ast_code_line_type;

/// @brief a single line of IFJ26 code
struct Ast_code_line {
  Ast_code_line_type type;  // determines union
  Ast_location location;
  Ast_code_line *next_line;

  union {
    struct {  // VARIABLE_DECLARATION
      char *variable_name;
      bool is_mutable;  // false == val, true == var
      Ast_data_type declared_type;
      Ast_data_type resolved_type;
      Ast_value *value;
    } variable_declaration;

    struct {  // ASSIGNEMENT
      char *variable_name;
      Ast_value *value;
    } assignement;

    struct {  // FUNCTION_CALL
      Ast_function_call *function_call;
    } call;

    struct {  // RETURN
      Ast_value *value;
    } return_ast;

    struct {  // IF
      Ast_expression *condition;
      Ast_code_block *then_block;
      Ast_code_block *else_block;
    } if_ast;

    struct {  // WHILE
      Ast_expression *condition;
      Ast_code_block *code_block;
    } while_ast;

    struct {  // EXPRESSION
      Ast_expression *expression;
    } expression_ast;
  } data;
};

/// @brief a block of code in a linked list
struct Ast_code_block {
  Ast_code_line *first_line;
  Ast_code_line *last_line;
};

/// @brief IFJ26 function parameter
struct Ast_function_parameter {
  char *parameter_name;
  Ast_data_type data_type;
  Ast_function_parameter *next_parameter;
};

/// @brief function body can be a value or a code block
typedef enum {
  VALUE,      // one line
  CODE_BLOCK  // multiple lines
} Ast_function_body_type;

/// @brief IFJ26 function
struct Ast_function {
  char *function_name;
  bool is_main;  // @main
  Ast_location location;

  Ast_function_parameter *first_parameter;
  Ast_function_parameter *last_parameter;

  Ast_data_type declared_return_type;
  Ast_data_type resolved_return_type;

  Ast_function_body_type body_type;  // determines union
  union {
    Ast_value *value;            // VALUE
    Ast_code_block *code_block;  // BLOCK
  } body;

  Ast_function *next_function;
};

/// @brief whole program
struct Ast_program {
  Ast_function *firsst_function;
  Ast_function *last_function;
};

Ast_program *ast_program_new(void);
Ast_function *ast_function_new(const char *name, Ast_location location);
Ast_function_parameter *ast_function_parameter_new(const char *name, Ast_data_type type);
Ast_code_block *ast_code_block_new(void);
Ast_code_line *ast_code_line_new(Ast_code_line_type type, Ast_location location);
Ast_expression *ast_expression_new(Ast_expression_type type, Ast_location location);
Ast_function_call *ast_function_call_new(const char *name, Ast_location location);
Ast_function_call_argument *ast_function_call_argument_new(Ast_expression *expression);
Ast_value *ast_value_from_expr(Ast_expression *expression);
Ast_value *ast_value_from_function_call(Ast_function_call *function_call);

void ast_program_append_function(Ast_program *program, Ast_function *function);
void ast_function_append_parameter(Ast_function *function, Ast_function_parameter *parameter);
void ast_block_append_line(Ast_code_block *block, Ast_code_line *line);
void ast_call_append_argument(Ast_function_call *call, Ast_function_call_argument *argument);

void ast_free_program(Ast_program *program);

#endif  // AST_H
