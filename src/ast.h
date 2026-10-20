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
typedef struct AstProgram AstProgram;
typedef struct AstFunction AstFunction;
typedef struct AstParam AstParam;
typedef struct Ast_block Ast_block;
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
      Ast_block *then_block;
      Ast_block *else_block;
    } if_ast;

    struct {  // WHILE
      Ast_expression *condition;
      Ast_block *block;
    } while_ast;

    struct {  // EXPRESSION
      Ast_expression *expr;
    } expr_ast;
  } data;
};

/* Blok uchovává příkazy v pořadí provádění.
 * Ukazatel na poslední příkaz umožňuje přidávat příkazy v O(1) čase.
 */
struct Ast_block {
  Ast_code_line *first;
  Ast_code_line *last;
};

/* ---------- Parametry a funkce ---------- */

struct AstParam {
  char *name;
  Ast_data_type type; /* Parametr má v základním IFJ26 explicitní typ. */
  bool symbol_resolved;
  AstParam *next;
};

typedef enum {
  AST_FUNCTION_BODY_VALUE, /* Jednořádkové tělo: výraz nebo volání. */
  AST_FUNCTION_BODY_BLOCK  /* Víceřádkové odsazené tělo. */
} AstFunctionBodyKind;

struct AstFunction {
  char *name;
  bool is_main; /* Funkce označená anotací @main. */
  Ast_location location;

  AstParam *params;
  AstParam *params_last;

  bool has_explicit_return_type;      /* Byl napsán ': Int', ': Unit' atd.? */
  Ast_data_type declared_return_type; /* Platí, pokud je typ explicitní. */
  Ast_data_type resolved_return_type; /* Výsledný typ po sémantické analýze. */

  AstFunctionBodyKind body_kind;  // determines union
  union {
    Ast_value *value; /* AST_FUNCTION_BODY_VALUE */
    Ast_block *block; /* AST_FUNCTION_BODY_BLOCK */
  } body;

  AstFunction *next;
};

/* ---------- Celý program ---------- */

struct AstProgram {
  AstFunction *functions;      /* První funkce v programu. */
  AstFunction *functions_last; /* Poslední funkce, pro přidávání O(1). */
};

/* ---------- Základní API modulu ast.c ----------
 * Následující funkce jsou pouze deklarace. Jejich implementace patří
 * do ast.c. Pokud si tým zvolí jiné rozhraní, můžete je upravit.
 */

AstProgram *ast_program_new(void);
AstFunction *ast_function_new(const char *name, Ast_location location);
AstParam *ast_param_new(const char *name, Ast_data_type type);
Ast_block *ast_block_new(void);
Ast_code_line *ast_stmt_new(Ast_code_line_type kind, Ast_location location);
Ast_expression *ast_expr_new(Ast_expression_type kind, Ast_location location);
Ast_function_call *ast_call_new(const char *name, Ast_location location);
Ast_function_call_argument *ast_call_arg_new(Ast_expression *term);
Ast_value *ast_value_from_expr(Ast_expression *expr);
Ast_value *ast_value_from_call(Ast_function_call *call);

void ast_program_append_function(AstProgram *program, AstFunction *function);
void ast_function_append_param(AstFunction *function, AstParam *param);
void ast_block_append_stmt(Ast_block *block, Ast_code_line *stmt);
void ast_call_append_arg(Ast_function_call *call, Ast_function_call_argument *arg);

/* Uvolní celý AST včetně všech jeho uzlů a vlastněných řetězců. */
void ast_free_program(AstProgram *program);

/* Pomocné funkce pro ladění; výpis směřuje do předaného streamu,
 * při ladění překladače se typicky použije stderr, nikdy stdout.
 */
const char *ast_type_name(Ast_data_type type);
void ast_dump_program(const AstProgram *program, FILE *out);

#endif  // AST_H
