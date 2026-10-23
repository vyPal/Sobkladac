/**
 * @file codegen.h
 * Project: IFJ26 compiler
 * @brief Header file for codegen
 * @author Jakub Palacky <xpalacj00>
 *
 * Type and function definitions to be used by codegen
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ast.h"
#include "strbuf.h"

typedef enum {
  OK_VAR,
  OK_CONST,
  OK_LABEL,
  OK_TYPE,
} OperandKind;

typedef enum {
  FT_GF,
  FT_LF,
  FT_TF,
} FrameType;

typedef struct {
  FrameType ft;
  char *name;
} VariableOperand;

typedef enum {
  CT_INT,
  CT_BOOL,
  CT_FLOAT,
  CT_STRING,
  CT_NIL,
} ConstType;

typedef struct {
  ConstType type;
  union {
    int64_t i;
    bool b;
    double f;
    char *s;
  };
} ConstOperand;

typedef struct {
  OperandKind type;

  union {
    VariableOperand var;
    ConstOperand cnst;
    char *label;
    ConstType type;
  } value;
} Operand;

enum {
  A_NONE = 0,
  A_VAR = 1u << 0,
  A_CONST = 1u << 1,
  A_LABEL = 1u << 2,
  A_TYPE = 1u << 3,
  A_STR = 1u << 4,
  A_SYMB = A_VAR | A_CONST,
  A_JUMP = A_LABEL | A_VAR | A_STR,
};

// clang-format off
#define IFJ_INSTRUCTIONS(X)                              \
  /* 7.4.1 frames, function calls */                     \
  X(MOVE,        A_VAR,   A_SYMB,  A_NONE)               \
  X(CREATEFRAME, A_NONE,  A_NONE,  A_NONE)               \
  X(PUSHFRAME,   A_NONE,  A_NONE,  A_NONE)               \
  X(POPFRAME,    A_NONE,  A_NONE,  A_NONE)               \
  X(DEFVAR,      A_VAR,   A_NONE,  A_NONE)               \
  X(CALL,        A_JUMP,  A_NONE,  A_NONE)               \
  X(RETURN,      A_NONE,  A_NONE,  A_NONE)               \
  /* 7.4.2 data stack */                                 \
  X(PUSHS,       A_SYMB,  A_NONE,  A_NONE)               \
  X(POPS,        A_VAR,   A_NONE,  A_NONE)               \
  X(CLEARS,      A_NONE,  A_NONE,  A_NONE)               \
  /* 7.4.3 arithmetic, relational, bool, conversion */   \
  X(ADD,         A_VAR,   A_SYMB,  A_SYMB)               \
  X(SUB,         A_VAR,   A_SYMB,  A_SYMB)               \
  X(MUL,         A_VAR,   A_SYMB,  A_SYMB)               \
  X(DIV,         A_VAR,   A_SYMB,  A_SYMB)               \
  X(IDIV,        A_VAR,   A_SYMB,  A_SYMB)               \
  X(ADDS,        A_NONE,  A_NONE,  A_NONE)               \
  X(SUBS,        A_NONE,  A_NONE,  A_NONE)               \
  X(MULS,        A_NONE,  A_NONE,  A_NONE)               \
  X(DIVS,        A_NONE,  A_NONE,  A_NONE)               \
  X(IDIVS,       A_NONE,  A_NONE,  A_NONE)               \
  X(LT,          A_VAR,   A_SYMB,  A_SYMB)               \
  X(GT,          A_VAR,   A_SYMB,  A_SYMB)               \
  X(EQ,          A_VAR,   A_SYMB,  A_SYMB)               \
  X(LTS,         A_NONE,  A_NONE,  A_NONE)               \
  X(GTS,         A_NONE,  A_NONE,  A_NONE)               \
  X(EQS,         A_NONE,  A_NONE,  A_NONE)               \
  X(AND,         A_VAR,   A_SYMB,  A_SYMB)               \
  X(OR,          A_VAR,   A_SYMB,  A_SYMB)               \
  X(NOT,         A_VAR,   A_SYMB,  A_NONE)               \
  X(ANDS,        A_NONE,  A_NONE,  A_NONE)               \
  X(ORS,         A_NONE,  A_NONE,  A_NONE)               \
  X(NOTS,        A_NONE,  A_NONE,  A_NONE)               \
  X(INT2FLOAT,   A_VAR,   A_SYMB,  A_NONE)               \
  X(FLOAT2INT,   A_VAR,   A_SYMB,  A_NONE)               \
  X(INT2CHAR,    A_VAR,   A_SYMB,  A_NONE)               \
  X(STRI2INT,    A_VAR,   A_SYMB,  A_SYMB)               \
  X(FLOAT2STR,   A_VAR,   A_SYMB,  A_NONE)               \
  X(INT2STR,     A_VAR,   A_SYMB,  A_NONE)               \
  X(INT2FLOATS,  A_NONE,  A_NONE,  A_NONE)               \
  X(FLOAT2INTS,  A_NONE,  A_NONE,  A_NONE)               \
  X(INT2CHARS,   A_NONE,  A_NONE,  A_NONE)               \
  X(STRI2INTS,   A_NONE,  A_NONE,  A_NONE)               \
  X(FLOAT2STRS,  A_NONE,  A_NONE,  A_NONE)               \
  X(INT2STRS,    A_NONE,  A_NONE,  A_NONE)               \
  /* 7.4.4 I/O */                                        \
  X(READ,        A_VAR,   A_TYPE,  A_NONE)               \
  X(WRITE,       A_SYMB,  A_NONE,  A_NONE)               \
  /* 7.4.5 strings */                                    \
  X(CONCAT,      A_VAR,   A_SYMB,  A_SYMB)               \
  X(STRLEN,      A_VAR,   A_SYMB,  A_NONE)               \
  X(GETCHAR,     A_VAR,   A_SYMB,  A_SYMB)               \
  X(SETCHAR,     A_VAR,   A_SYMB,  A_SYMB)               \
  /* 7.4.6 arrays */                                     \
  X(NEWARRAY,    A_VAR,   A_SYMB,  A_NONE)               \
  X(ARRAYGET,    A_VAR,   A_VAR,   A_SYMB)               \
  X(ARRAYSET,    A_VAR,   A_SYMB,  A_SYMB)               \
  /* 7.4.7 types */                                      \
  X(TYPE,        A_VAR,   A_SYMB,  A_NONE)               \
  X(ISINT,       A_VAR,   A_SYMB,  A_NONE)               \
  X(TYPES,       A_NONE,  A_NONE,  A_NONE)               \
  X(ISINTS,      A_NONE,  A_NONE,  A_NONE)               \
  /* 7.4.8 control flow */                               \
  X(LABEL,       A_LABEL, A_NONE,  A_NONE)               \
  X(JUMP,        A_JUMP,  A_NONE,  A_NONE)               \
  X(JUMPIFEQ,    A_JUMP,  A_SYMB,  A_SYMB)               \
  X(JUMPIFNEQ,   A_JUMP,  A_SYMB,  A_SYMB)               \
  X(JUMPIFEQS,   A_JUMP,  A_NONE,  A_NONE)               \
  X(JUMPIFNEQS,  A_JUMP,  A_NONE,  A_NONE)               \
  X(EXIT,        A_SYMB,  A_NONE,  A_NONE)               \
  /* 7.4.9 debugging */                                  \
  X(BREAK,       A_NONE,  A_NONE,  A_NONE)               \
  X(DPRINT,      A_SYMB,  A_NONE,  A_NONE)
// clang-format on

typedef enum {
#define X(name, a1, a2, a3) OP_##name,
  IFJ_INSTRUCTIONS(X)
#undef X
      OP_COUNT
} Opcode;

typedef struct {
  const char *name;
  uint8_t accepts[3];
  uint8_t arity;
} InstrInfo;

extern const InstrInfo instrInfo[OP_COUNT];

bool instrValidate(Opcode op, const Operand *args, size_t argc);

void emitOperand(StringBuilder *out, Operand op);
void emitInstr(StringBuilder *out, Opcode op, const Operand *args, size_t argc);

#define EMIT(out, op, ...)                                  \
  emitInstr((out), OP_##op, (const Operand[]){__VA_ARGS__}, \
            sizeof((const Operand[]){__VA_ARGS__}) / sizeof(Operand))
#define EMIT0(out, op) emitInstr((out), OP_##op, NULL, 0)

char *newLabel(const char *func, const char *desc);
char *newVar(const char *name);

char *generateCode(AstProgram program);

#endif  // !CODEGEN_H
