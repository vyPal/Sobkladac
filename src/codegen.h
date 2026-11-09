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

/// Kind of operand
typedef enum {
  OK_VAR,
  OK_CONST,
  OK_LABEL,
  OK_TYPE,
} OperandKind;

/// Type of frame
typedef enum {
  FT_GF,
  FT_LF,
  FT_TF,
} FrameType;

/// Definition on the variable operand kind
typedef struct {
  FrameType ft;
  char *name;
} VariableOperand;

/// Data types of const operands
typedef enum {
  CT_INT,
  CT_BOOL,
  CT_FLOAT,
  CT_STRING,
  CT_NIL,
} ConstType;

/// Definition of the constant operand kind
typedef struct {
  ConstType type;
  union {
    int64_t i;
    bool b;
    double f;
    char *s;
  };
} ConstOperand;

/// Definition of operand data
typedef struct {
  OperandKind type;

  union {
    VariableOperand var;
    ConstOperand cnst;
    char *label;
    ConstType type;
  } value;
} Operand;

/// Bitfield of all possible "acceptance criteria"
/// I don't have any better name for this ig
enum {
  A_NONE = 0U,
  A_VAR = 1U << 0U,
  A_CONST = 1U << 1U,
  A_LABEL = 1U << 2U,
  A_TYPE = 1U << 3U,
  A_STR = 1U << 4U,
  A_SYMB = (unsigned)A_VAR | (unsigned)A_CONST,
  A_JUMP = (unsigned)A_LABEL | (unsigned)A_VAR | (unsigned)A_STR,
};

/// X-Macro definitions of all instructions possible in IFJcode26 including which operand types they
/// accept for automated checking
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

/// Autogenerated enum of all instructions with `OP_` prefix
typedef enum {
#define X(name, a1, a2, a3) OP_##name,
  IFJ_INSTRUCTIONS(X)
#undef X
      OP_COUNT
} Opcode;

/// Struct to hold all info about specific instructions
typedef struct {
  const char *name;
  uint8_t accepts[3];
  uint8_t arity;
} InstrInfo;

/// List of all instruction info, will be generated in .c
extern const InstrInfo instrInfo[OP_COUNT];

/// Function to validate that the instruction will accept the specified list of operand arguments
bool instrValidate(Opcode op, const Operand *args, size_t argc);

/// Emits a operand into the specified string builder
void emitOperand(StringBuilder *out, Operand op);
/// Emits an instruction into the specified string builder
void emitInstr(StringBuilder *out, Opcode op, const Operand *args, size_t argc);

/// Shorthand macro for emitting an instruction while supplying operands as vaargs
#define EMIT(out, op, ...)                                  \
  emitInstr((out), OP_##op, (const Operand[]){__VA_ARGS__}, \
            sizeof((const Operand[]){__VA_ARGS__}) / sizeof(Operand))
/// Shorthand macro for emitting an instruction that doesn't take any operand arguments
#define EMIT0(out, op) emitInstr((out), OP_##op, NULL, 0)

/// Name mangling for labels
/// @warning Calling this method multiple times with the same input provides different outputs
char *newLabel(const char *func, const char *desc);

/// Name mangling for variables
/// @warning Calling this method multiple times with the same input provides different outputs
char *newVar(const char *name);

/// Entrypoint of codegen
char *generateCode(AstProgram program);

#endif  // !CODEGEN_H
