/**
 * @file codegen.c
 * Project: IFJ26 compiler
 * @brief Codegen implementation
 * @author Jakub Palacky <xpalacj00>
 *
 * Codegen implementation
 */

#include "codegen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "hashtable.h"
#include "strbuf.h"
#include "symtable.h"

/// Global counter for name mangling
static unsigned nameCounter;

/// Mangle variable by adding counter
char *newVar(const char *name) {
  StringBuilder sb = sbNew(0);
  sbAppendf(&sb, "%s$%u", name, nameCounter++);
  return sb.str;
}

/// Mangle lable name by adding counter and function name
char *newLabel(const char *func, const char *desc) {
  StringBuilder sb = sbNew(0);
  sbAppendf(&sb, "%s$%s$%u", func, desc, nameCounter++);
  return sb.str;
}

/// Create new globally unique temporary variable name
char *newTempVar(void) {
  StringBuilder sb = sbNew(8);
  sbAppendf(&sb, "$t$%u", nameCounter++);
  return sb.str;
}

/// Generate parameter name for function parameter by index
char *funcParam(unsigned int argc) {
  StringBuilder sb = sbNew(8);
  sbAppendf(&sb, "$p$%u", argc);
  return sb.str;
}

/// Check whether operand should be counted
#define COUNT_OP(X) ((X) != A_NONE)

/// Build table with instruction info
const InstrInfo instrInfo[OP_COUNT] = {
#define X(name, a1, a2, a3) \
  [OP_##name] = {#name, {a1, a2, a3}, COUNT_OP(a1) + COUNT_OP(a2) + COUNT_OP(a3)},
    IFJ_INSTRUCTIONS(X)
#undef X
};

/// Checks whether an operand will be accepted by the `accepts` bitfield
static bool acceptsOperand(uint8_t accepts, const Operand *arg) {
  switch (arg->type) {
    case OK_VAR:
      return accepts & (unsigned)A_VAR;
    case OK_CONST:
      if (accepts & (unsigned)A_CONST)
        return true;
      return (accepts & (unsigned)A_STR) && arg->value.cnst.type == CT_STRING;
    case OK_LABEL:
      return accepts & (unsigned)A_LABEL;
    case OK_TYPE:
      return (accepts & (unsigned)A_TYPE) && arg->value.type != CT_NIL;
    default:
      return false;
  }
}

/// Validates whether the arity and operand types match for the opcode
bool instrValidate(Opcode op, const Operand *args, size_t argc) {
  if (instrInfo[op].arity != argc)
    return false;
  for (size_t i = 0; i < argc; i++) {
    if (!acceptsOperand(instrInfo[op].accepts[i], &args[i]))
      return false;
  }
  return true;
}

/// Names of frames and data types for lowering
const char *frameNames[3] = {"GF", "LF", "TF"};
const char *typeNames[5] = {"int", "bool", "float", "string", ""};

/// Emits and operand into the string builder
void emitOperand(StringBuilder *out, Operand op) {
  switch (op.type) {
    case OK_VAR:
      sbAppendf(out, "%s@%s", frameNames[op.value.var.ft], op.value.var.name);
      break;
    case OK_CONST:;
      ConstOperand c = op.value.cnst;
      switch (c.type) {
        case CT_INT:
          sbAppendf(out, "int@%ld", c.i);
          break;
        case CT_BOOL:
          sbAppendf(out, "bool@%s", c.b ? "true" : "false");
          break;
        case CT_FLOAT:
          sbAppendf(out, "float@%a", c.f);
          break;
        case CT_STRING:
          sbAppend(out, "string@");
          size_t len = strlen(c.s);
          for (size_t i = 0; i < len; i++) {
            unsigned char ch = c.s[i];
            if (ch > 32 && ch != '#' && ch != '\\')
              sbAppendChar(out, (char)ch);
            else
              sbAppendf(out, "\\%03u", (unsigned)ch);
          }
          break;
        case CT_NIL:
          sbAppend(out, "nil@nil");
          break;
      }
      break;
    case OK_LABEL:
      sbAppend(out, op.value.label);
      break;
    case OK_TYPE:
      sbAppend(out, typeNames[op.value.type]);
  }
}

/// Emits an instruction into the string builder
void emitInstr(StringBuilder *out, Opcode op, const Operand *args, size_t argc) {
  if (!instrValidate(op, args, argc))
    die(ERR_INTERNAL, 0, "Instruction validation failed");

  sbAppend(out, instrInfo[op].name);
  for (size_t i = 0; i < argc; i++) {
    sbAppendChar(out, ' ');
    emitOperand(out, args[i]);
  }
  sbAppendChar(out, '\n');
}

/// Shorthand generators for all operand types
static inline Operand varOp(FrameType f, char *name) {
  return (Operand){.type = OK_VAR, .value.var = {.ft = f, .name = name}};
}
static inline Operand intOp(int64_t i) {
  return (Operand){.type = OK_CONST, .value.cnst = {.type = CT_INT, .i = i}};
}
static inline Operand boolOp(bool b) {
  return (Operand){.type = OK_CONST, .value.cnst = {.type = CT_BOOL, .b = b}};
}
static inline Operand floatOp(double f) {
  return (Operand){.type = OK_CONST, .value.cnst = {.type = CT_FLOAT, .f = f}};
}
static inline Operand stringOp(char *s) {
  return (Operand){.type = OK_CONST, .value.cnst = {.type = CT_STRING, .s = s}};
}
static inline Operand nilOp(void) {
  return (Operand){.type = OK_CONST, .value.cnst = {.type = CT_NIL}};
}
static inline Operand labelOp(char *l) {
  return (Operand){.type = OK_LABEL, .value.label = l};
}
static inline Operand typeOp(ConstType t) {
  return (Operand){.type = OK_TYPE, .value.type = t};
}

/// Variable definition registry
typedef struct {
  char **names;
  size_t count;
  size_t capacity;
} VariableRegistry;

/// Global registry
static VariableRegistry registry = {0};

/// Registers a variable in the variable registry growing it if necessary
static void registerVariable(Operand op) {
  if (op.type != OK_VAR)
    return;
  if (registry.names == NULL) {
    registry.names = (char **)arenaAlloc(sizeof(char *) * 64);
    registry.capacity = 64;
    registry.count = 0;
  }
  if (registry.count + 1 > registry.capacity) {
    registry.names =
        (char **)arenaRealloc((void *)registry.names, sizeof(char *) * registry.capacity,
                              sizeof(char *) * registry.capacity * 2);
    registry.capacity *= 2;
  }
  registry.names[registry.count++] = op.value.var.name;
}

/// Structure for emitters that can return values to return info about type and location
typedef struct {
  AstDataType ty;
  Operand val;
  bool negate;  // Flag for use in conditions to use JUMPIFNEQ instead of JUMPIFEQ
} ValueInfo;

/// Returns an operand to store the result in, uses `preferred` if non-NULL otherwise generates a
/// new temporary
static Operand getResultOperand(const Operand *preferred) {
  if (preferred != NULL) {
    if (preferred->type != OK_VAR)
      die(ERR_INTERNAL, 0, "Result destination must be a variable");

    return *preferred;
  }

  Operand tmp = varOp(FT_LF, newTempVar());
  registerVariable(tmp);
  return tmp;
}

/// Checks whether two operands are the same variable
static bool sameVariable(Operand a, Operand b) {
  return a.type == OK_VAR && b.type == OK_VAR && a.value.var.ft == b.value.var.ft &&
         strcmp(a.value.var.name, b.value.var.name) == 0;
}

/// Copies value to destination unless they're the same
static void storeValue(StringBuilder *code, Operand dst, ValueInfo value) {
  if (dst.type != OK_VAR)
    die(ERR_INTERNAL, 0, "Destination must be a variable");

  if (sameVariable(dst, value.val))
    return;

  EMIT(code, MOVE, dst, value.val);
}

/// Converts an Int value to Double, any other value is returned unchanged
/// Int constants are converted at compile time, anything else is converted into `dst` (or a new
/// temporary if `dst` is NULL), so the source variable is never overwritten
static ValueInfo intToDouble(StringBuilder *code, ValueInfo value, const Operand *dst) {
  if (value.ty != INT)
    return value;

  if (value.val.type == OK_CONST)
    return (ValueInfo){.ty = DOUBLE, .val = floatOp((double)value.val.value.cnst.i)};

  Operand out = getResultOperand(dst);
  EMIT(code, INT2FLOAT, out, value.val);
  return (ValueInfo){.ty = DOUBLE, .val = out};
}

/// Stores value to a destination of type `dstType`, converting Int to Double when needed
static void storeTyped(StringBuilder *code, Operand dst, AstDataType dstType, ValueInfo value) {
  if (dstType == DOUBLE)
    value = intToDouble(code, value, &dst);
  storeValue(code, dst, value);
}

/// Global labels for integer wrapping helper/routine
#define INT_WRAP_LABEL "$wrap32"
#define INT_WRAP_VALUE "$wrap$value"
#define INT_WRAP_TEMP "$wrap$temp"

/// Emits a call to the int wrapping helper
static void emitIntWrap(StringBuilder *code, Operand dst) {
  Operand skipWrap = labelOp(newLabel("int_wrap_routine", "skip"));
  Operand doWrap = labelOp(newLabel("int_wrap_routine", "do"));
  Operand temp = varOp(FT_GF, INT_WRAP_TEMP);

  // Check whether value needs to be wrapped before calling wrap routine
  EMIT(code, LT, temp, dst, intOp(INT32_MIN));
  EMIT(code, JUMPIFEQ, doWrap, temp, boolOp(true));
  EMIT(code, GT, temp, dst, intOp(INT32_MAX));
  EMIT(code, JUMPIFEQ, doWrap, temp, boolOp(true));
  EMIT(code, JUMP, skipWrap);

  EMIT(code, LABEL, doWrap);
  EMIT(code, MOVE, varOp(FT_GF, INT_WRAP_VALUE), dst);
  EMIT(code, CALL, labelOp(INT_WRAP_LABEL));
  EMIT(code, MOVE, dst, varOp(FT_GF, INT_WRAP_VALUE));

  EMIT(code, LABEL, skipWrap);
}

/// Emits the int wrapping routing itself
/// If the value in `INT_WRAP_VALUE` is above `INT32_MAX` or below `INT32_MIN` then it gets wrapped
/// around, otherwise it is returned unmodified
/// This method uses the global frame to avoid frame manipulation
static void emitIntWrapRoutine(StringBuilder *code) {
  Operand value = varOp(FT_GF, INT_WRAP_VALUE);
  Operand temp = varOp(FT_GF, INT_WRAP_TEMP);
  Operand positive = labelOp("$wrap32$positive");

  // Wrap value
  // value = ((value + 2^31) mod 2^32) - 2^31
  EMIT(code, LABEL, labelOp(INT_WRAP_LABEL));
  EMIT(code, ADD, value, value, intOp(INT64_C(1) << 31));
  EMIT(code, IDIV, temp, value, intOp(INT64_C(1) << 32));
  EMIT(code, MUL, temp, temp, intOp(INT64_C(1) << 32));
  EMIT(code, SUB, value, value, temp);
  EMIT(code, LT, temp, value, intOp(0));
  EMIT(code, JUMPIFEQ, positive, temp, boolOp(false));
  EMIT(code, ADD, value, value, intOp(INT64_C(1) << 32));
  EMIT(code, LABEL, positive);
  EMIT(code, SUB, value, value, intOp(INT64_C(1) << 31));
  EMIT0(code, RETURN);
}

/// Add a and b wrapping them as if they were 32bit numbers
int64_t wrap32AddSext(int64_t a, int64_t b) {
  return (int64_t)(int32_t)((uint32_t)a + (uint32_t)b);
}

/// Subtract a and b wrapping them as if they were 32bit numbers
int64_t wrap32SubSext(int64_t a, int64_t b) {
  return (int64_t)(int32_t)((uint32_t)a - (uint32_t)b);
}

/// Multiply a and b wrapping them as if they were 32bit numbers
int64_t wrap32MulSext(int64_t a, int64_t b) {
  return (int64_t)(int32_t)((uint32_t)a * (uint32_t)b);
}

/// Divide a and b wrapping them as if they were 32bit numbers
int64_t wrap32DivSext(int64_t a, int64_t b) {
  int32_t lhs = (int32_t)(uint32_t)a;
  int32_t rhs = (int32_t)(uint32_t)b;

  if (rhs == 0) {
    return 0;
  }
  if (lhs == INT32_MIN && rhs == -1) {
    return (int64_t)INT32_MIN;
  }

  return (int64_t)(lhs / rhs);
}

/// Check if operand is a constant and is zero (int only)
bool isConstZero(Operand op) {
  return op.type == OK_CONST && (op.value.cnst.type == CT_INT && op.value.cnst.i == 0);
}

/// Check if operand is a constant and is one (int or float)
bool isConstOne(Operand op) {
  return op.type == OK_CONST && ((op.value.cnst.type == CT_INT && op.value.cnst.i == 1) ||
                                 (op.value.cnst.type == CT_FLOAT && op.value.cnst.f == 1.0));
}

/// Check if operand is a constant and is negative one (int only)
bool isConstNegOne(Operand op) {
  return op.type == OK_CONST && (op.value.cnst.type == CT_INT && op.value.cnst.i == -1);
}

static HashTable *knownConstants;

/// Lower expression into IFJcode26
/// @param code Reference to string builder
/// @param expr Pointer to expression to lower
/// @param preferredDst Operand to return value in. Only a recommendation, uses temp if NULL
/// @note Expects all used variables to be present in LF
/// @note Resulting value info contains actual destination of value
ValueInfo lowerExpression(StringBuilder *code, AstExpression *expr, const Operand *preferredDst,
                          bool isCond) {
  Operand dst;
  switch (expr->expressionType) {
    // Return constant operand for literals
    case INT_LITERAL:
      return (ValueInfo){.ty = INT, .val = intOp(expr->data.intValue)};
    case DOUBLE_LITERAL:
      return (ValueInfo){.ty = DOUBLE, .val = floatOp(expr->data.doubleValue)};
    case STRING_LITERAL:
      return (ValueInfo){.ty = STRING, .val = stringOp(expr->data.stringValue)};
    case BOOL_LITERAL:
      return (ValueInfo){.ty = BOOL, .val = boolOp(expr->data.boolValue)};
    case UNIT_LITERAL:
      return (ValueInfo){.ty = UNIT, .val = nilOp()};

    // Assume variable is available in local frame, use deducedDataType from AST
    case VARIABLE:
      if (expr->deducedDataType == UNKNOWN) {
        die(ERR_INTERNAL, (int)expr->location.line, "Variable expression has no resolved type");
      }
      const ValueInfo *info = htGet(knownConstants, expr->data.variable);
      if (info != NULL)
        return *info;
      return (ValueInfo){.ty = expr->deducedDataType, .val = varOp(FT_LF, expr->data.variable)};

    // Only negation is allowed as unary op, subtract value from 0 or 0.0
    case UNARY:;
      AstOperator op = expr->data.unary.operatorAst;
      if (op != NEGATION && op != LOGICAL_NEGATION)
        die(ERR_INTERNAL, (int)expr->location.line,
            "Binary operation cannot be performed on single operand");

      ValueInfo operand = lowerExpression(code, expr->data.unary.expression, NULL, isCond);

      if (operand.val.type == OK_CONST) {
        if (op == NEGATION) {
          if (operand.ty == INT) {
            return (ValueInfo){.ty = INT, .val = intOp(wrap32SubSext(0, operand.val.value.cnst.i))};
          } else {
            return (ValueInfo){.ty = DOUBLE, .val = floatOp(0.0 - operand.val.value.cnst.f)};
          }
        } else {
          if (operand.negate)
            return (ValueInfo){.ty = BOOL, .val = boolOp(operand.val.value.cnst.b)};
          return (ValueInfo){.ty = BOOL, .val = boolOp(!operand.val.value.cnst.b)};
        }
      }

      dst = getResultOperand(preferredDst);

      if (op == NEGATION) {
        if (operand.ty == INT) {
          EMIT(code, SUB, dst, intOp(0), operand.val);
          emitIntWrap(code, dst);
        } else
          EMIT(code, SUB, dst, floatOp(0.0), operand.val);
      } else {
        if (!operand.negate)
          EMIT(code, NOT, dst, operand.val);
        else
          storeValue(code, dst, operand);
      }

      return (ValueInfo){.ty = operand.ty, .val = dst};

    // Perform binary op, uses data type of left operand to determine resulting type and instruction
    // to use Assumes both operands are of the same data type Numerical operations return the same
    // type as the left operand, logic operations return bool
    case BINARY:;
      ValueInfo left = lowerExpression(code, expr->data.binary.leftExpression, NULL, isCond);
      ValueInfo right = lowerExpression(code, expr->data.binary.rightExpression, NULL, isCond);
      bool isIntOperation = left.ty == INT && right.ty == INT;

      if (left.ty == DOUBLE || right.ty == DOUBLE) {
        left = intToDouble(code, left, NULL);
        right = intToDouble(code, right, NULL);
      }

      if (left.val.type == OK_CONST && right.val.type == OK_CONST) {
        switch (expr->data.binary.operatorAst) {
          case MULTIPLICATION:
            if (isIntOperation)
              return (ValueInfo){
                  .ty = INT,
                  .val = intOp(wrap32MulSext(left.val.value.cnst.i, right.val.value.cnst.i))};
            else
              return (ValueInfo){.ty = DOUBLE,
                                 .val = floatOp(left.val.value.cnst.f * right.val.value.cnst.f)};

          case DIVISION:
            if (isIntOperation) {
              if (right.val.value.cnst.i == 0)
                break;  // Fall through to runtime division to fail
              return (ValueInfo){
                  .ty = INT,
                  .val = intOp(wrap32DivSext(left.val.value.cnst.i, right.val.value.cnst.i))};
            } else {
              if (right.val.value.cnst.f == 0.0)
                break;  // Fall through to runtime division to fail
              return (ValueInfo){.ty = DOUBLE,
                                 .val = floatOp(left.val.value.cnst.f / right.val.value.cnst.f)};
            }

          case ADDITION:
            if (left.ty == STRING) {
              size_t len1 = strlen(left.val.value.cnst.s);
              size_t len2 = strlen(right.val.value.cnst.s);
              char *new = arenaAlloc(len1 + len2 + 1);
              strncpy(new, left.val.value.cnst.s, len1);
              strncpy(new + len1, right.val.value.cnst.s, len2);
              new[len1 + len2] = 0;
              return (ValueInfo){.ty = STRING, .val = stringOp(new)};
            } else if (isIntOperation)
              return (ValueInfo){
                  .ty = INT,
                  .val = intOp(wrap32AddSext(left.val.value.cnst.i, right.val.value.cnst.i))};
            else
              return (ValueInfo){.ty = DOUBLE,
                                 .val = floatOp(left.val.value.cnst.f + right.val.value.cnst.f)};

          case SUBTRACTION:
            if (isIntOperation)
              return (ValueInfo){
                  .ty = INT,
                  .val = intOp(wrap32SubSext(left.val.value.cnst.i, right.val.value.cnst.i))};
            else
              return (ValueInfo){.ty = DOUBLE,
                                 .val = floatOp(left.val.value.cnst.f - right.val.value.cnst.f)};

          case LESSER:
            if (left.ty == STRING)
              return (ValueInfo){
                  .ty = BOOL,
                  .val = boolOp(strcmp(left.val.value.cnst.s, right.val.value.cnst.s) < 0)};
            if (left.ty == DOUBLE)
              return (ValueInfo){.ty = BOOL,
                                 .val = boolOp(left.val.value.cnst.f < right.val.value.cnst.f)};
            return (ValueInfo){.ty = BOOL,
                               .val = boolOp(left.val.value.cnst.i < right.val.value.cnst.i)};

          case LESSER_EQUAL:
            if (left.ty == STRING)
              return (ValueInfo){
                  .ty = BOOL,
                  .val = boolOp(strcmp(left.val.value.cnst.s, right.val.value.cnst.s) <= 0)};
            if (left.ty == DOUBLE)
              return (ValueInfo){.ty = BOOL,
                                 .val = boolOp(left.val.value.cnst.f <= right.val.value.cnst.f)};
            return (ValueInfo){.ty = BOOL,
                               .val = boolOp(left.val.value.cnst.i <= right.val.value.cnst.i)};

          case GREATER:
            if (left.ty == STRING)
              return (ValueInfo){
                  .ty = BOOL,
                  .val = boolOp(strcmp(left.val.value.cnst.s, right.val.value.cnst.s) > 0)};
            if (left.ty == DOUBLE)
              return (ValueInfo){.ty = BOOL,
                                 .val = boolOp(left.val.value.cnst.f > right.val.value.cnst.f)};
            return (ValueInfo){.ty = BOOL,
                               .val = boolOp(left.val.value.cnst.i > right.val.value.cnst.i)};

          case GREATER_EQUAL:
            if (left.ty == STRING)
              return (ValueInfo){
                  .ty = BOOL,
                  .val = boolOp(strcmp(left.val.value.cnst.s, right.val.value.cnst.s) >= 0)};
            if (left.ty == DOUBLE)
              return (ValueInfo){.ty = BOOL,
                                 .val = boolOp(left.val.value.cnst.f >= right.val.value.cnst.f)};
            return (ValueInfo){.ty = BOOL,
                               .val = boolOp(left.val.value.cnst.i >= right.val.value.cnst.i)};

          case EQUAL:
          case NOT_EQUAL: {
            bool equal;
            switch (left.ty) {
              case INT:
                equal = left.val.value.cnst.i == right.val.value.cnst.i;
                break;
              case DOUBLE:
                equal = left.val.value.cnst.f == right.val.value.cnst.f;
                break;
              case STRING:
                equal = strcmp(left.val.value.cnst.s, right.val.value.cnst.s) == 0;
                break;
              case BOOL:
                equal = left.val.value.cnst.b == right.val.value.cnst.b;
                if (left.negate != right.negate)
                  equal = !equal;
                break;
              case UNIT:
                equal = true;
                break;
              default:
                die(ERR_INTERNAL, (int)expr->location.line,
                    "Invalid operands for constant equality");
            }
            return (ValueInfo){
                .ty = BOOL, .val = boolOp(expr->data.binary.operatorAst == EQUAL ? equal : !equal)};
          }

          case LOGICAL_AND:
            return (ValueInfo){.ty = BOOL,
                               .val = boolOp((left.val.value.cnst.b != left.negate) &&
                                             (right.val.value.cnst.b != right.negate))};

          case LOGICAL_OR:
            return (ValueInfo){.ty = BOOL,
                               .val = boolOp((left.val.value.cnst.b != left.negate) ||
                                             (right.val.value.cnst.b != right.negate))};

          default:
            break;
        }
      }

      dst = getResultOperand(preferredDst);
      Operand temp = varOp(FT_GF, INT_WRAP_TEMP);

      switch (expr->data.binary.operatorAst) {
        case MULTIPLICATION:
          if (isConstOne(left.val))
            storeValue(code, dst, right);
          else if (isConstOne(right.val))
            storeValue(code, dst, left);
          else if (isConstNegOne(left.val)) {
            EMIT(code, SUB, dst, intOp(0), right.val);
            emitIntWrap(code, dst);
          } else if (isConstNegOne(right.val)) {
            EMIT(code, SUB, dst, intOp(0), left.val);
            emitIntWrap(code, dst);
          } else if (isConstZero(left.val) || isConstZero(right.val))
            return (ValueInfo){.ty = left.ty, .val = intOp(0)};
          else {
            EMIT(code, MUL, dst, left.val, right.val);
            if (isIntOperation)
              emitIntWrap(code, dst);
          }
          return (ValueInfo){.ty = left.ty, .val = dst};

        case DIVISION:
          if (isConstOne(right.val))
            storeValue(code, dst, left);
          else if (isConstNegOne(right.val)) {
            EMIT(code, SUB, dst, intOp(0), left.val);
            emitIntWrap(code, dst);
          } else if (isIntOperation) {
            EMIT(code, IDIV, dst, left.val, right.val);
            emitIntWrap(code, dst);
          } else
            EMIT(code, DIV, dst, left.val, right.val);

          return (ValueInfo){.ty = left.ty, .val = dst};

        case ADDITION:
          // Concatenation for strings
          if (left.ty == STRING)
            EMIT(code, CONCAT, dst, left.val, right.val);
          else if (isConstZero(left.val))
            storeValue(code, dst, right);
          else if (isConstZero(right.val))
            storeValue(code, dst, left);
          else {
            EMIT(code, ADD, dst, left.val, right.val);
            if (isIntOperation)
              emitIntWrap(code, dst);
          }
          return (ValueInfo){.ty = left.ty, .val = dst};

        case SUBTRACTION:
          if (isConstZero(right.val))
            storeValue(code, dst, left);
          else {
            EMIT(code, SUB, dst, left.val, right.val);
            if (isIntOperation)
              emitIntWrap(code, dst);
          }
          return (ValueInfo){.ty = left.ty, .val = dst};

        case LESSER:
          EMIT(code, LT, dst, left.val, right.val);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case LESSER_EQUAL:
          EMIT(code, GT, dst, left.val, right.val);
          if (!isCond)
            EMIT(code, NOT, dst, dst);
          return (ValueInfo){.ty = BOOL, .val = dst, .negate = isCond};

        case GREATER:
          EMIT(code, GT, dst, left.val, right.val);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case GREATER_EQUAL:
          EMIT(code, LT, dst, left.val, right.val);
          if (!isCond)
            EMIT(code, NOT, dst, dst);
          return (ValueInfo){.ty = BOOL, .val = dst, .negate = isCond};

        case EQUAL:
          EMIT(code, EQ, dst, left.val, right.val);
          if (left.negate != right.negate)
            EMIT(code, NOT, dst, dst);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case NOT_EQUAL:
          EMIT(code, EQ, dst, left.val, right.val);
          if (!isCond != (left.negate != right.negate))
            EMIT(code, NOT, dst, dst);
          return (ValueInfo){.ty = BOOL, .val = dst, .negate = isCond};

        case LOGICAL_AND:
          if (left.val.type == OK_CONST && !left.val.value.cnst.b)
            return (ValueInfo){.ty = BOOL, .val = boolOp(false)};
          if (right.val.type == OK_CONST && !right.val.value.cnst.b)
            return (ValueInfo){.ty = BOOL, .val = boolOp(false)};
          if (!(left.negate && right.negate)) {
            if (left.negate) {
              EMIT(code, NOT, temp, left.val);
              EMIT(code, AND, dst, temp, right.val);
            } else if (right.negate) {
              EMIT(code, NOT, temp, right.val);
              EMIT(code, AND, dst, temp, left.val);
            } else
              EMIT(code, AND, dst, left.val, right.val);
          } else {
            if (isCond) {
              EMIT(code, OR, dst, left.val, right.val);
              return (ValueInfo){.ty = BOOL, .val = dst, .negate = true};
            }
            EMIT(code, OR, dst, left.val, right.val);
            EMIT(code, NOT, dst, dst);
          }
          return (ValueInfo){.ty = BOOL, .val = dst};

        case LOGICAL_OR:
          if (left.val.type == OK_CONST && left.val.value.cnst.b)
            return (ValueInfo){.ty = BOOL, .val = boolOp(true)};
          if (right.val.type == OK_CONST && right.val.value.cnst.b)
            return (ValueInfo){.ty = BOOL, .val = boolOp(true)};
          if (!(left.negate && right.negate)) {
            if (left.negate) {
              EMIT(code, NOT, temp, left.val);
              EMIT(code, OR, dst, temp, right.val);
            } else if (right.negate) {
              EMIT(code, NOT, temp, right.val);
              EMIT(code, OR, dst, temp, left.val);
            } else
              EMIT(code, OR, dst, left.val, right.val);
          } else {
            if (isCond) {
              EMIT(code, AND, dst, left.val, right.val);
              return (ValueInfo){.ty = BOOL, .val = dst, .negate = true};
            }
            EMIT(code, AND, dst, left.val, right.val);
            EMIT(code, NOT, dst, dst);
          }
          return (ValueInfo){.ty = BOOL, .val = dst};

        default:
          die(ERR_INTERNAL, (int)expr->location.line, "Unknown binary operator");
      }
    default:
      die(ERR_INTERNAL, 0, "Unknown expression type");
  }
}

/// Function currently being lowered, needed to know the type returned values are converted to
static AstFunction *currentFunction;

/// Lowers a function call, preferredDst specifies the preferred destination operand, note that this
/// operand does not have to be used, consult the returned info for the actual value location
ValueInfo lowerFunctionCall(StringBuilder *code, AstFunctionCall *call, const Operand *preferredDst,
                            SymTable *table) {
  if (strcmp(call->functionName, "print") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL, false);
    switch (arg.ty) {
      case INT:
      case DOUBLE:
      case STRING:
      case BOOL:
        EMIT(code, WRITE, arg.val);
        break;

      case UNIT:
        EMIT(code, WRITE, stringOp("()"));
        break;

      case UNKNOWN:
      case ANY:
        die(ERR_INTERNAL, 0, "Unable to print unknown value");
    }
    return (ValueInfo){.ty = UNIT, .val = nilOp()};
  } else if (strcmp(call->functionName, "toStr") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL, false);
    switch (arg.ty) {
      case STRING:
        return arg;
      case UNIT:
        return (ValueInfo){.ty = STRING, .val = stringOp("()")};
      case INT:
      case DOUBLE: {
        Operand dst = getResultOperand(preferredDst);
        if (arg.ty == INT)
          EMIT(code, INT2STR, dst, arg.val);
        else
          EMIT(code, FLOAT2STR, dst, arg.val);
        return (ValueInfo){.ty = STRING, .val = dst};
      }
      case BOOL: {
        // Needs to compare value at runtime and return constant based on value
        Operand dst = getResultOperand(preferredDst);
        Operand trueLabel = labelOp(newLabel(currentFunction->functionName, "bool_to_str"));
        Operand endLabel = labelOp(newLabel(currentFunction->functionName, "bool_to_str"));

        EMIT(code, JUMPIFEQ, trueLabel, arg.val, boolOp(true));
        EMIT(code, MOVE, dst, stringOp("false"));
        EMIT(code, JUMP, endLabel);
        EMIT(code, LABEL, trueLabel);
        EMIT(code, MOVE, dst, stringOp("true"));
        EMIT(code, LABEL, endLabel);

        return (ValueInfo){.ty = STRING, .val = dst};
      }
      case UNKNOWN:
      case ANY:
        die(ERR_INTERNAL, 0, "Unable to convert unknown value");
    }
    die(ERR_INTERNAL, 0, "Unreachable");
  } else if (strcmp(call->functionName, "readLine") == 0) {
    Operand dst = getResultOperand(preferredDst);
    EMIT(code, READ, dst, typeOp(CT_STRING));
    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "readInt") == 0) {
    Operand dst = getResultOperand(preferredDst);
    Operand temp = varOp(FT_GF, INT_WRAP_TEMP);
    Operand invalid = labelOp(newLabel(currentFunction->functionName, "readInt_invalid"));
    Operand ok = labelOp(newLabel(currentFunction->functionName, "readInt"));
    EMIT(code, READ, dst, typeOp(CT_INT));
    EMIT(code, JUMPIFEQ, invalid, dst, nilOp());
    EMIT(code, LT, temp, dst, intOp(INT32_MIN));
    EMIT(code, JUMPIFEQ, invalid, temp, boolOp(true));
    EMIT(code, GT, temp, dst, intOp(INT32_MAX));
    EMIT(code, JUMPIFEQ, ok, temp, boolOp(false));
    EMIT(code, LABEL, invalid);
    EMIT(code, MOVE, dst, intOp(0));
    EMIT(code, LABEL, ok);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "readDouble") == 0) {
    Operand dst = getResultOperand(preferredDst);
    Operand ok = labelOp(newLabel(currentFunction->functionName, "readFloat"));
    EMIT(code, READ, dst, typeOp(CT_FLOAT));
    EMIT(code, JUMPIFNEQ, ok, dst, nilOp());
    EMIT(code, MOVE, dst, floatOp(0.0));
    EMIT(code, LABEL, ok);
    return (ValueInfo){.ty = DOUBLE, .val = dst};
  } else if (strcmp(call->functionName, "readBoolean") == 0) {
    Operand dst = getResultOperand(preferredDst);
    Operand ok = labelOp(newLabel(currentFunction->functionName, "readBoolean"));
    EMIT(code, READ, dst, typeOp(CT_BOOL));
    EMIT(code, JUMPIFNEQ, ok, dst, nilOp());
    EMIT(code, MOVE, dst, boolOp(false));
    EMIT(code, LABEL, ok);
    return (ValueInfo){.ty = DOUBLE, .val = dst};
  } else if (strcmp(call->functionName, "toInt") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL, false);
    Operand dst = getResultOperand(preferredDst);
    if (arg.ty == INT)
      storeValue(code, dst, arg);
    else
      EMIT(code, FLOAT2INT, dst, arg.val);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "strLength") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL, false);
    Operand dst = getResultOperand(preferredDst);
    EMIT(code, STRLEN, dst, arg.val);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "subStr") == 0) {
    // Most complex builtin method, algorithm:
    //  1. Check bounds
    //  2. Return early with empty string if `start == end` and `start < strlen`
    //  3. Get char at `index`
    //  4. Concat char with result string
    //  5. Increment `index`
    //  6. If `index != end` jump back to 3.
    //  7. Copy result to destination
    //  8. Return

    // Define all temporaries and labels
    Operand strlen = varOp(FT_LF, newTempVar());
    Operand tempOp = varOp(FT_LF, newTempVar());
    Operand index = varOp(FT_LF, newTempVar());
    Operand result = varOp(FT_LF, newTempVar());
    Operand exit40 = labelOp(newLabel(currentFunction->functionName, "sub_str"));
    Operand loop = labelOp(newLabel(currentFunction->functionName, "sub_str_loop"));
    Operand ok = labelOp(newLabel(currentFunction->functionName, "sub_str_ok"));
    Operand dst = getResultOperand(preferredDst);

    // Register temporaries
    registerVariable(strlen);
    registerVariable(tempOp);
    registerVariable(index);
    registerVariable(result);

    // Compute source string, start and end
    AstFunctionCallArgument *arg = call->firstArgument;
    ValueInfo str = lowerExpression(code, arg->expression, NULL, false);
    arg = arg->nextArgument;
    EMIT(code, STRLEN, strlen, str.val);
    ValueInfo start = lowerExpression(code, arg->expression, NULL, false);
    arg = arg->nextArgument;
    ValueInfo end = lowerExpression(code, arg->expression, NULL, false);

    // Check bounds, if rules are not met, jump to label that exits with code 40
    EMIT(code, LT, tempOp, start.val, intOp(0));
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));
    EMIT(code, GT, tempOp, start.val, end.val);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));
    EMIT(code, LT, tempOp, start.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(false));
    EMIT(code, GT, tempOp, end.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));

    // Init result with empty string
    EMIT(code, MOVE, result, stringOp(""));
    // Jump to return if start and end are equal
    EMIT(code, SUB, tempOp, end.val, start.val);
    EMIT(code, JUMPIFEQ, ok, tempOp, intOp(0));
    // Copy start to new variable since start could be a constant
    EMIT(code, MOVE, index, start.val);
    // Jump to loop if number of characters to copy is less than the string length
    EMIT(code, JUMPIFNEQ, loop, tempOp, strlen);
    // If `end - start` is equal to the string length (start = 0 && end = strlen)
    // copy source string to result and jump to return
    storeValue(code, result, str);
    EMIT(code, JUMP, ok);

    // Start of loop
    EMIT(code, LABEL, loop);
    EMIT(code, GETCHAR, tempOp, str.val, index);  // Get character at `index`
    EMIT(code, CONCAT, result, result, tempOp);   // Concat it with the result string
    EMIT(code, ADD, index, index, intOp(1));      // Increment `index`
    EMIT(code, JUMPIFNEQ, loop, index, end.val);  // Loop if `index` is not equal to `end`
    EMIT(code, JUMP, ok);                         // Jump to return

    // Exit with error code 40
    EMIT(code, LABEL, exit40);
    EMIT(code, EXIT, intOp(40));
    // Copy result to destination operand
    EMIT(code, LABEL, ok);
    EMIT(code, MOVE, dst, result);

    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "ord") == 0) {
    // Define temp vars and labels
    Operand tmp = varOp(FT_LF, newTempVar());
    Operand err = labelOp(newLabel(currentFunction->functionName, "ord_err"));
    Operand ok = labelOp(newLabel(currentFunction->functionName, "ord_ok"));
    Operand dst = getResultOperand(preferredDst);

    // Register temp
    registerVariable(tmp);

    // Compute arguments
    AstFunctionCallArgument *arg = call->firstArgument;
    ValueInfo str = lowerExpression(code, arg->expression, NULL, false);
    arg = arg->nextArgument;
    ValueInfo index = lowerExpression(code, arg->expression, NULL, false);

    // Check bounds
    EMIT(code, LT, tmp, index.val, intOp(0));
    EMIT(code, JUMPIFEQ, err, tmp, boolOp(true));
    EMIT(code, STRLEN, tmp, str.val);
    EMIT(code, JUMPIFEQ, err, tmp, intOp(0));
    EMIT(code, LT, tmp, index.val, tmp);
    EMIT(code, JUMPIFEQ, err, tmp, boolOp(false));

    // Convert character to int
    EMIT(code, STRI2INT, dst, str.val, index.val);
    EMIT(code, JUMP, ok);

    // Return possibly writing -1 to dst if error
    EMIT(code, LABEL, err);
    EMIT(code, MOVE, dst, intOp(-1));
    EMIT(code, LABEL, ok);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "chr") == 0) {
    Operand dst = getResultOperand(preferredDst);
    ValueInfo ch = lowerExpression(code, call->firstArgument->expression, NULL, false);
    EMIT(code, INT2CHAR, dst, ch.val);
    return (ValueInfo){.ty = STRING, .val = dst};
  } else {
    Operand dst = getResultOperand(preferredDst);
    EMIT0(code, CREATEFRAME);  // Create new frame

    // Compute values for arguments
    //
    // BUG: Currently works because function arguments canot be function calls, if function calls
    // were a valid type of argument, the TF currently being built would be cleared by the argument
    // function call. If we implement the extension for this, the argument building will have to
    // happen on LF beofre CREATEFRAME and will need to be copied to new TF only after all arguments
    // are evaluated
    AstFunctionCallArgument *arg = call->firstArgument;
    const StSymbol *sym = symtableLookup(table, call->functionName);
    if (sym == NULL)
      die(ERR_INTERNAL, 0, "Could not find function %s in symbol table", call->functionName);
    if (sym->type != ST_FUNCTION)
      die(ERR_INTERNAL, 0, "Cannot call a variable");
    unsigned argc = 0;
    while (arg != NULL) {
      if (argc >= sym->data.function.parameterCount)
        die(ERR_INTERNAL, 0, "Too many arguments for function %s", call->functionName);
      Operand op = varOp(FT_TF, sym->data.function.parameterNames[argc]);
      EMIT(code, DEFVAR, op);
      ValueInfo argValue = lowerExpression(code, arg->expression, &op, false);
      storeTyped(code, op, sym->data.function.parameterTypes[argc++], argValue);
      arg = arg->nextArgument;
    }

    EMIT0(code, PUSHFRAME);                         // Push frame
    EMIT(code, DEFVAR, varOp(FT_LF, "$ret"));       // Define return var
    EMIT(code, CALL, labelOp(call->functionName));  // Call function
    EMIT0(code, POPFRAME);                          // Pop frame so that we can access old LF
    EMIT(code, MOVE, dst, varOp(FT_TF, "$ret"));    // Move return value from TF to LF
    return (ValueInfo){.ty = sym->data.function.returnType, .val = dst};
  }
}

/// Lowers a value (either expression or function call)
ValueInfo lowerValue(StringBuilder *code, AstValue *val, const Operand *preferredDst,
                     SymTable *table) {
  if (val->type == VALUE_EXPRESSION) {
    return lowerExpression(code, val->data.expression, preferredDst, false);
  }
  return lowerFunctionCall(code, val->data.functionCall, preferredDst, table);
}

void lowerCodeBlock(StringBuilder *code, AstCodeBlock *block);

/// Lowers a command
void lowerCommand(StringBuilder *code, AstCommand *cmd, SymTable *table) {
  switch (cmd->type) {
    case VARIABLE_DECLARATION: {
      // Declare and register variable operand for this variable, creates on LF
      Operand var = varOp(FT_LF, cmd->data.variableDeclaration.variableName);
      registerVariable(var);
      ValueInfo value = lowerValue(code, cmd->data.variableDeclaration.value, &var, table);
      if (cmd->data.variableDeclaration.resolvedType == DOUBLE)
        value = intToDouble(code, value, &var);
      storeValue(code, var, value);
      if (!cmd->data.variableDeclaration.isMutable && value.val.type == OK_CONST) {
        ValueInfo *copy = arenaAlloc(sizeof(ValueInfo));
        *copy = value;
        if (!htInsert(knownConstants, cmd->data.variableDeclaration.variableName, copy))
          die(ERR_INTERNAL, 0, "Unable to insert immutable variable into constant table");
      }
      break;
    }
    case ASSIGNMENT: {
      Operand dst = varOp(FT_LF, cmd->data.assignment.variableName);
      ValueInfo value = lowerValue(code, cmd->data.assignment.value, &dst, table);
      storeTyped(code, dst, cmd->data.assignment.resolvedType, value);
      break;
    }
    case FUNCTION_CALL:
      // Could return value, but this value is ignored
      lowerFunctionCall(code, cmd->data.call.functionCall, NULL, table);
      break;
    case RETURN: {
      // Lower value and store or copy it to the return variable
      Operand ret = varOp(FT_LF, "$ret");
      ValueInfo value = lowerValue(code, cmd->data.returnAst.value, &ret, table);
      storeTyped(code, ret, currentFunction->resolvedReturnType, value);
      EMIT0(code, RETURN);
      break;
    }
    case IF:;
      Operand end = labelOp(newLabel(currentFunction->functionName, "if_end"));
      ValueInfo ifcond = lowerExpression(code, cmd->data.ifAst.condition, NULL, true);
      if (ifcond.val.type == OK_CONST) {
        if (ifcond.val.value.cnst.b != ifcond.negate)
          lowerCodeBlock(code, cmd->data.ifAst.thenBlock);
        else if (cmd->data.ifAst.elseBlock != NULL)
          lowerCodeBlock(code, cmd->data.ifAst.elseBlock);
        break;
      }
      if (cmd->data.ifAst.elseBlock == NULL) {
        if (ifcond.negate)
          EMIT(code, JUMPIFEQ, end, ifcond.val, boolOp(true));
        else
          EMIT(code, JUMPIFEQ, end, ifcond.val, boolOp(false));
        lowerCodeBlock(code, cmd->data.ifAst.thenBlock);
        EMIT(code, LABEL, end);
      } else {
        Operand f = labelOp(newLabel(currentFunction->functionName, "if_false"));
        if (ifcond.negate)
          EMIT(code, JUMPIFEQ, f, ifcond.val, boolOp(true));
        else
          EMIT(code, JUMPIFEQ, f, ifcond.val, boolOp(false));
        lowerCodeBlock(code, cmd->data.ifAst.thenBlock);
        EMIT(code, JUMP, end);
        EMIT(code, LABEL, f);
        lowerCodeBlock(code, cmd->data.ifAst.elseBlock);
        EMIT(code, LABEL, end);
      }
      break;
    case WHILE:;
      Operand loop = labelOp(newLabel(currentFunction->functionName, "when"));
      Operand loopEnd = labelOp(newLabel(currentFunction->functionName, "when_end"));
      EMIT(code, LABEL, loop);
      ValueInfo whilecond = lowerExpression(code, cmd->data.whileAst.condition, NULL, true);
      if (whilecond.val.type == OK_CONST) {
        if (whilecond.val.value.cnst.b != whilecond.negate) {
          Operand loopInner = labelOp(newLabel(currentFunction->functionName, "when_inner"));
          EMIT(code, LABEL, loopInner);
          lowerCodeBlock(code, cmd->data.whileAst.codeBlock);
          EMIT(code, JUMP, loopInner);
        }
        break;
      }
      if (whilecond.negate)
        EMIT(code, JUMPIFEQ, loopEnd, whilecond.val, boolOp(true));
      else
        EMIT(code, JUMPIFEQ, loopEnd, whilecond.val, boolOp(false));
      lowerCodeBlock(code, cmd->data.whileAst.codeBlock);
      EMIT(code, JUMP, loop);
      EMIT(code, LABEL, loopEnd);
      break;
    case EXPRESSION:
      lowerExpression(code, cmd->data.expressionAst.expression, NULL, false);
      break;
  }
}

/// Lowers command block
void lowerCodeBlock(StringBuilder *code, AstCodeBlock *block) {
  AstCommand *cmd = block->firstCommand;
  while (cmd != NULL) {
    lowerCommand(code, cmd, block->symTable);
    cmd = cmd->nextCommand;
  }
}

/// Lowers a function declaration
void lowerFunction(StringBuilder *code, AstFunction *func) {
  currentFunction = func;
  knownConstants = htCreate(32);

  // Emit un-mangled function name as entry label
  EMIT(code, LABEL, labelOp(func->functionName));

  // Repeated variable declarations are not allowed, so the function body is lowered into a
  // separate string builder while writing all declarations to the global registry. These
  // declarations are appended to the main string builder before the function body
  StringBuilder internal = sbNew(0);
  registry.count = 0;
  if (func->bodyType == VALUE) {
    Operand ret = varOp(FT_LF, "$ret");
    ValueInfo value = lowerValue(&internal, func->body.value, &ret, func->symTable);
    storeTyped(&internal, ret, func->resolvedReturnType, value);
    EMIT0(&internal, RETURN);
    for (size_t i = 0; i < registry.count; i++) {
      EMIT(code, DEFVAR, varOp(FT_LF, registry.names[i]));
    }
    sbAppend(code, sbBuild(&internal));
  } else {
    lowerCodeBlock(&internal, func->body.codeBlock);
    for (size_t i = 0; i < registry.count; i++) {
      EMIT(code, DEFVAR, varOp(FT_LF, registry.names[i]));
    }
    sbAppend(code, sbBuild(&internal));
  }
}

/// Main code generation method
char *generateCode(AstProgram program) {
  // Allocate the registry before the string builder so that the string builder can grow
  registerVariable(nilOp());
  StringBuilder code = sbNew(0);
  sbAppend(&code, ".IFJcode26\n");  // Emit header
  // Create new frame and push it
  EMIT0(&code, CREATEFRAME);
  EMIT0(&code, PUSHFRAME);
  EMIT(&code, DEFVAR, varOp(FT_LF, "$ret"));  // Define return variable
  // Define global integer wrapping variables
  EMIT(&code, DEFVAR, varOp(FT_GF, INT_WRAP_VALUE));
  EMIT(&code, DEFVAR, varOp(FT_GF, INT_WRAP_TEMP));

  // Find main functino and emit call to it and an exit right after
  for (AstFunction *func = program.firstFunction; func != NULL; func = func->nextFunction)
    if (func->isMain) {
      EMIT(&code, CALL, labelOp(func->functionName));
      break;
    }
  EMIT(&code, EXIT, intOp(0));
  emitIntWrapRoutine(&code);

  // Emit all user-defined functions
  AstFunction *func = program.firstFunction;
  while (func != NULL) {
    lowerFunction(&code, func);
    func = func->nextFunction;
  }
  return sbBuild(&code);
}
