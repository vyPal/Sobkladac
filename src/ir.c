#include "ir.h"

#include <stdint.h>
#include <string.h>

#include "codegen.h"
#include "common.h"

static Instr *irNew(Opcode op, const Operand *args, size_t argc) {
  if (!instrValidate(op, args, argc))
    die(ERR_INTERNAL, 0, "Instruction validation failed (%s)", instrInfo[op].name);
  Instr *i = arenaZalloc(sizeof *i);
  i->op = op;
  i->argc = (uint8_t)argc;
  for (size_t k = 0; k < argc; k++) i->args[k] = args[k];
  return i;
}

Instr *irInsertBefore(InstrList *l, Instr *at, Opcode op, const Operand *args, size_t argc) {
  Instr *i = irNew(op, args, argc);
  if (at == NULL) {
    i->prev = l->tail;
    if (l->tail)
      l->tail->next = i;
    else
      l->head = i;
    l->tail = i;
  } else {
    i->next = at;
    i->prev = at->prev;
    if (at->prev)
      at->prev->next = i;
    else
      l->head = i;
    at->prev = i;
  }
  l->len++;
  return i;
}

Instr *irAppend(InstrList *l, Opcode op, const Operand *args, size_t argc) {
  return irInsertBefore(l, NULL, op, args, argc);
}

Instr *irRemove(InstrList *l, Instr *i) {
  Instr *next = i->next;
  if (i->prev)
    i->prev->next = i->next;
  else
    l->head = i->next;
  if (i->next)
    i->next->prev = i->prev;
  else
    l->tail = i->prev;
  l->len--;
  return next;
}

void irSplice(InstrList *dst, InstrList *src) {
  if (!src->head)
    return;
  if (dst->tail) {
    dst->tail->next = src->head;
    src->head->prev = dst->tail;
  } else {
    dst->head = src->head;
  }
  dst->tail = src->tail;
  dst->len += src->len;
  *src = (InstrList){0};
}

bool operandEq(const Operand *a, const Operand *b) {
  if (a->type != b->type)
    return false;
  switch (a->type) {
    case OK_VAR:
      return a->value.var.ft == b->value.var.ft &&
             strcmp(a->value.var.name, b->value.var.name) == 0;
    case OK_LABEL:
      return strcmp(a->value.label, b->value.label) == 0;
    case OK_TYPE:
      return a->value.type == b->value.type;
    case OK_CONST: {
      const ConstOperand *x = &a->value.cnst, *y = &b->value.cnst;
      if (x->type != y->type)
        return false;
      switch (x->type) {
        case CT_INT:
          return x->i == y->i;
        case CT_BOOL:
          return x->b == y->b;
        case CT_FLOAT:
          return memcmp(&x->f, &y->f, sizeof(double)) == 0;
        case CT_STRING:
          return strcmp(x->s, y->s) == 0;
        case CT_NIL:
          return true;
      }
    }
  }
  return false;
}

static bool isConst(const Instr *i, ConstType t) {
  return i && i->op == OP_PUSHS && i->args[0].type == OK_CONST && i->args[0].value.cnst.type == t;
}
static bool is(const Instr *i, Opcode op) {
  return i && i->op == op;
}
static bool endsBlock(Opcode op) {
  return op == OP_JUMP || op == OP_RETURN || op == OP_EXIT;
}

// clang-format off
static Opcode threeAddrOf(Opcode op) {
  switch (op) {
    case OP_ADDS: return OP_ADD;
    case OP_SUBS: return OP_SUB;
    case OP_MULS: return OP_MUL;
    case OP_DIVS: return OP_DIV;
    case OP_IDIVS: return OP_IDIV;
    case OP_LTS: return OP_LT;
    case OP_GTS: return OP_GT;
    case OP_EQS: return OP_EQ;
    case OP_ANDS: return OP_AND;
    case OP_ORS: return OP_OR;
    case OP_STRI2INTS: return OP_STRI2INT;
    case OP_NOTS: return OP_NOT;
    case OP_INT2FLOATS: return OP_INT2FLOAT;
    case OP_FLOAT2INTS: return OP_FLOAT2INT;
    case OP_INT2CHARS: return OP_INT2CHAR;
    case OP_FLOAT2STRS: return OP_FLOAT2STR;
    case OP_INT2STRS: return OP_INT2STR;
    case OP_TYPES: return OP_TYPE;
    case OP_ISINTS: return OP_ISINT;
    default: return OP_COUNT;
  }
}
// clang-format on

Instr *peepholeAt(InstrList *l, Instr *i, bool *changed) {
  Instr *n1 = i->next, *n2 = n1 ? n1->next : NULL, *n3 = n2 ? n2->next : NULL;
  Instr *back = i->prev ? (i->prev->prev ? i->prev->prev : i->prev) : NULL;
#define RESTART()                 \
  do {                            \
    *changed = true;              \
    return back ? back : l->head; \
  } while (0)

  // MOVE x x ->
  if (i->op == OP_MOVE && operandEq(&i->args[0], &i->args[1])) {
    irRemove(l, i);
    RESTART();
  }

  // JUMP l; LABEL l -> LABEL l
  if (i->op == OP_JUMP && is(n1, OP_LABEL) && operandEq(&i->args[0], &n1->args[0])) {
    irRemove(l, i);
    RESTART();
  }

  // unreachable after block end
  if (endsBlock(i->op) && n1 && n1->op != OP_LABEL) {
    while (i->next && i->next->op != OP_LABEL) irRemove(l, i->next);
    RESTART();
  }

  if (is(i, OP_PUSHS)) {
    if (is(n1, OP_POPS)) {
      if (!operandEq(&i->args[0], &n1->args[0]))
        irInsertBefore(l, i, OP_MOVE, (Operand[]){n1->args[0], i->args[0]}, 2);
      irRemove(l, n1);
      irRemove(l, i);
      RESTART();
    }

    if (is(n1, OP_PUSHS)) {
      if (n2 && is(n3, OP_POPS) && threeAddrOf(n2->op) != OP_COUNT &&
          instrInfo[threeAddrOf(n2->op)].arity == 3) {
        irInsertBefore(l, i, threeAddrOf(n2->op), (Operand[]){n3->args[0], i->args[0], n1->args[0]},
                       3);
        irRemove(l, n3);
        irRemove(l, n2);
        irRemove(l, n1);
        irRemove(l, i);
        RESTART();
      }
    }
  }

  return NULL;
#undef RESTART
}

void irOptimize(InstrList *l) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (Instr *i = l->head; i;) {
      Instr *restart = peepholeAt(l, i, &changed);
      i = restart ? restart : i->next;
    }
  }
}

void irEmit(StringBuilder *out, const InstrList *l) {
  for (const Instr *i = l->head; i; i = i->next) {
    sbAppend(out, instrInfo[i->op].name);
    for (size_t k = 0; k < i->argc; k++) {
      sbAppendChar(out, ' ');
      emitOperand(out, i->args[k]);
    }
    sbAppendChar(out, '\n');
  }
}
