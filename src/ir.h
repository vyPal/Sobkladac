#ifndef IR_H
#define IR_H

#include <stdbool.h>
#include <stddef.h>

#include "codegen.h"
#include "strbuf.h"

typedef struct Instr {
  Opcode op;
  uint8_t argc;
  Operand args[3];
  struct Instr *prev, *next;
} Instr;

typedef struct {
  Instr *head, *tail;
  size_t len;
} InstrList;

Instr *irAppend(InstrList *l, Opcode op, const Operand *args, size_t argc);
Instr *irInsertBefore(InstrList *l, Instr *at, Opcode op, const Operand *args, size_t argc);
Instr *irRemove(InstrList *l, Instr *i);
void irSplice(InstrList *dst, InstrList *src);

bool operandEq(const Operand *a, const Operand *b);
void irOptimize(InstrList *l);
void irEmit(StringBuilder *out, const InstrList *l);

#define IR_EMIT(list, op, ...)                              \
  irAppend((list), OP_##op, (const Operand[]){__VA_ARGS__}, \
           sizeof((const Operand[]){__VA_ARGS__}) / sizeof(Operand))
#define IR_EMIT0(list, op) irAppend((list), OP_##op, NULL, 0)

#endif  // IR_H
