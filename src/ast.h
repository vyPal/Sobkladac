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
typedef struct AstBlock AstBlock;
typedef struct AstStmt AstStmt;
typedef struct Ast_expression Ast_expression;
typedef struct AstCall AstCall;
typedef struct AstCallArg AstCallArg;
typedef struct AstValue AstValue;

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

typedef struct Ast_expression {
  Ast_expression_type expression_type;
  Ast_location location;
  Ast_data_type deduced_data_type;  // for sematic analysis

  union {
    int32_t int_value;    // INT_LITERAL
    double double_value;  // DOUBLE_LITERAL
    char *string_value;   // STRING_LITERAL
    // UNIT_LITERAL (no data)

    char *variable;  // VARIABLE

    struct {  // UNARY
      Ast_operator ast_operator;
      Ast_expression *expression;
    } unary;

    struct {  // BINARY
      Ast_operator ast_operator;
      Ast_expression *left_expression;
      Ast_expression *right_expression;
    } binary;
  } data;
} Ast_expression;

/* ---------- Volání funkcí a argumenty ---------- */

/* Argumenty jsou v základním IFJ26 pouze termy (literály nebo proměnné),
 * nikoli libovolné výrazy. AstExpr * se zde používá pro opětovné využití
 * reprezentace literálu/proměnné; převod z parseru musí dodržet toto pravidlo.
 */
struct AstCallArg {
  Ast_expression *term;
  AstCallArg *next;
};

struct AstCall {
  char *name; /* Název volané uživatelské či vestavěné funkce. */
  Ast_location loc;

  AstCallArg *arguments; /* První argument, seznam je spojový. */
  AstCallArg *arguments_last;
};

/* Některá místa IFJ26 přijímají buď výraz, nebo volání funkce
 * (např. inicializace proměnné, pravá strana přiřazení nebo návratová
 * hodnota). Tento obal zachovává rozdíl mezi těmito dvěma případy.
 */
typedef enum { AST_VALUE_EXPR, AST_VALUE_CALL } AstValueKind;

struct AstValue {
  AstValueKind kind;
  union {
    Ast_expression *expr;
    AstCall *call;
  } as;
};

/* ---------- Příkazy a bloky ---------- */

typedef enum {
  AST_STMT_VAR_DECL, /* val x = ... / var x = ... */
  AST_STMT_ASSIGN,   /* x = ... */
  AST_STMT_CALL,     /* samostatný příkaz volání funkce */
  AST_STMT_RETURN,   /* return ... nebo samostatné return */
  AST_STMT_IF,       /* if ... then ... else ... */
  AST_STMT_WHILE,    /* while ... do ... */
  AST_STMT_EXPR      /* samostatné vyhodnocení výrazu */
} AstStmtKind;

struct AstStmt {
  AstStmtKind kind; /* Určuje platnou část union 'as'. */
  Ast_location loc;
  AstStmt *next; /* Další příkaz ve stejném bloku, nebo NULL. */

  union {
    struct { /* AST_STMT_VAR_DECL */
      char *name;
      bool is_mutable;             /* false pro val, true pro var */
      bool has_explicit_type;      /* Bylo uvedeno ': Int' apod.? */
      Ast_data_type declared_type; /* Platí, pokud has_explicit_type */
      Ast_data_type resolved_type; /* Typ doplněný/ověřený analýzou */
      AstValue *initializer;       /* Inicializace je povinná. */
    } var_decl;

    struct { /* AST_STMT_ASSIGN */
      char *target_name;
      AstValue *value;
    } assign;

    struct { /* AST_STMT_CALL */
      AstCall *call;
    } call_stmt;

    struct { /* AST_STMT_RETURN */
      AstValue *value;
    } return_stmt;

    struct { /* AST_STMT_IF */
      Ast_expression *condition;
      AstBlock *then_block;
      AstBlock *else_block; /* V základním IFJ26 je else povinné. */
    } if_stmt;

    struct { /* AST_STMT_WHILE */
      Ast_expression *condition;
      AstBlock *body;
    } while_stmt;

    struct { /* AST_STMT_EXPR */
      Ast_expression *expr;
    } expr_stmt;
  } as;
};

/* Blok uchovává příkazy v pořadí provádění.
 * Ukazatel na poslední příkaz umožňuje přidávat příkazy v O(1) čase.
 */
struct AstBlock {
  AstStmt *first;
  AstStmt *last;
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
  Ast_location loc;

  AstParam *params;
  AstParam *params_last;

  bool has_explicit_return_type;      /* Byl napsán ': Int', ': Unit' atd.? */
  Ast_data_type declared_return_type; /* Platí, pokud je typ explicitní. */
  Ast_data_type resolved_return_type; /* Výsledný typ po sémantické analýze. */

  AstFunctionBodyKind body_kind;
  union {
    AstValue *value; /* AST_FUNCTION_BODY_VALUE */
    AstBlock *block; /* AST_FUNCTION_BODY_BLOCK */
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
AstFunction *ast_function_new(const char *name, Ast_location loc);
AstParam *ast_param_new(const char *name, Ast_data_type type);
AstBlock *ast_block_new(void);
AstStmt *ast_stmt_new(AstStmtKind kind, Ast_location loc);
Ast_expression *ast_expr_new(Ast_expression_type kind, Ast_location loc);
AstCall *ast_call_new(const char *name, Ast_location loc);
AstCallArg *ast_call_arg_new(Ast_expression *term);
AstValue *ast_value_from_expr(Ast_expression *expr);
AstValue *ast_value_from_call(AstCall *call);

void ast_program_append_function(AstProgram *program, AstFunction *function);
void ast_function_append_param(AstFunction *function, AstParam *param);
void ast_block_append_stmt(AstBlock *block, AstStmt *stmt);
void ast_call_append_arg(AstCall *call, AstCallArg *arg);

/* Uvolní celý AST včetně všech jeho uzlů a vlastněných řetězců. */
void ast_free_program(AstProgram *program);

/* Pomocné funkce pro ladění; výpis směřuje do předaného streamu,
 * při ladění překladače se typicky použije stderr, nikdy stdout.
 */
const char *ast_type_name(Ast_data_type type);
void ast_dump_program(const AstProgram *program, FILE *out);

#endif  // AST_H
