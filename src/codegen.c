#include "codegen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "strbuf.h"

static unsigned nameCounter;

char *newVar(const char *name) {
  StringBuilder sb = sbNew(0);
  sbAppendf(&sb, "%s$%u", name, nameCounter++);
  return sb.str;
}

char *newLabel(const char *func, const char *desc) {
  StringBuilder sb = sbNew(0);
  sbAppendf(&sb, "%s$%s$%u", func, desc, nameCounter++);
  return sb.str;
}

char *newTempVar(void) {
  StringBuilder sb = sbNew(8);
  sbAppendf(&sb, "$t$%u", nameCounter++);
  return sb.str;
}

char *funcParam(unsigned int argc) {
  StringBuilder sb = sbNew(8);
  sbAppendf(&sb, "$p$%u", argc);
  return sb.str;
}

#define COUNT_OP(X) ((X) != A_NONE)

const InstrInfo instrInfo[OP_COUNT] = {
#define X(name, a1, a2, a3) \
  [OP_##name] = {#name, {a1, a2, a3}, COUNT_OP(a1) + COUNT_OP(a2) + COUNT_OP(a3)},
    IFJ_INSTRUCTIONS(X)
#undef X
};

static bool acceptsOperand(uint8_t accepts, const Operand *arg) {
  switch (arg->type) {
    case OK_VAR:
      return accepts & A_VAR;
    case OK_CONST:
      if (accepts & A_CONST)
        return true;
      return (accepts & A_STR) && arg->value.cnst.type == CT_STRING;
    case OK_LABEL:
      return accepts & A_LABEL;
    case OK_TYPE:
      return (accepts & A_TYPE) && arg->value.type != CT_NIL;
    default:
      return false;
  }
}

bool instrValidate(Opcode op, const Operand *args, size_t argc) {
  if (instrInfo[op].arity != argc)
    return false;
  for (size_t i = 0; i < argc; i++) {
    if (!acceptsOperand(instrInfo[op].accepts[i], &args[i]))
      return false;
  }
  return true;
}

const char *frameNames[3] = {"GF", "LF", "TF"};
const char *typeNames[5] = {"int", "bool", "float", "string", ""};

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

typedef struct {
  char **names;
  size_t count;
  size_t capacity;
} VariableRegistry;

static VariableRegistry registry = {0};

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

typedef struct {
  AstDataType ty;
  Operand val;
} ValueInfo;

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

static bool sameVariable(Operand a, Operand b) {
  return a.type == OK_VAR && b.type == OK_VAR && a.value.var.ft == b.value.var.ft &&
         strcmp(a.value.var.name, b.value.var.name) == 0;
}

static void storeValue(StringBuilder *code, Operand dst, ValueInfo value) {
  if (dst.type != OK_VAR)
    die(ERR_INTERNAL, 0, "Destination must be a variable");

  if (sameVariable(dst, value.val))
    return;

  EMIT(code, MOVE, dst, value.val);
}

#define INT_WRAP_LABEL "$wrap32"
#define INT_WRAP_VALUE "$wrap$value"
#define INT_WRAP_TEMP "$wrap$temp"

static void emitIntWrap(StringBuilder *code, Operand dst) {
  EMIT(code, MOVE, varOp(FT_GF, INT_WRAP_VALUE), dst);
  EMIT(code, CALL, labelOp(INT_WRAP_LABEL));
  EMIT(code, MOVE, dst, varOp(FT_GF, INT_WRAP_VALUE));
}

static void emitIntWrapRoutine(StringBuilder *code) {
  Operand value = varOp(FT_GF, INT_WRAP_VALUE);
  Operand temp = varOp(FT_GF, INT_WRAP_TEMP);
  Operand wrap = labelOp("$wrap32$wrap");
  Operand positive = labelOp("$wrap32$positive");

  EMIT(code, LABEL, labelOp(INT_WRAP_LABEL));
  EMIT(code, LT, temp, value, intOp(INT32_MIN));
  EMIT(code, JUMPIFEQ, wrap, temp, boolOp(true));
  EMIT(code, GT, temp, value, intOp(INT32_MAX));
  EMIT(code, JUMPIFEQ, wrap, temp, boolOp(true));
  EMIT0(code, RETURN);

  EMIT(code, LABEL, wrap);
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

/// Lower expression into IFJcode26
/// @param code Reference to string builder
/// @param expr Pointer to expression to lower
/// @param val Optional desired operand to store in (uses temporary if NULL)
/// @note Expects all used variables to be present in LF
/// @note Resulting value info contains destination of value
ValueInfo lowerExpression(StringBuilder *code, AstExpression *expr, const Operand *preferredDst) {
  Operand dst;
  switch (expr->expressionType) {
    case INT_LITERAL:
      return (ValueInfo){.ty = INT, .val = intOp(expr->data.intValue)};
    case DOUBLE_LITERAL:
      return (ValueInfo){.ty = DOUBLE, .val = floatOp(expr->data.doubleValue)};
    case STRING_LITERAL:
      return (ValueInfo){.ty = STRING, .val = stringOp(expr->data.stringValue)};
    case UNIT_LITERAL:
      return (ValueInfo){.ty = UNIT, .val = nilOp()};
    case VARIABLE:
      if (expr->deducedDataType == UNKNOWN) {
        die(ERR_INTERNAL, (int)expr->location.line, "Variable expression has no resolved type");
      }
      return (ValueInfo){.ty = expr->deducedDataType, .val = varOp(FT_LF, expr->data.variable)};
    case UNARY:
      if (expr->data.unary.operatorAst != NEGATION)
        die(ERR_INTERNAL, (int)expr->location.line,
            "Binary operation cannot be performed on single operand");

      ValueInfo operand = lowerExpression(code, expr->data.unary.expression, NULL);

      dst = getResultOperand(preferredDst);

      if (operand.ty == INT) {
        EMIT(code, SUB, dst, intOp(0), operand.val);
        emitIntWrap(code, dst);
      } else
        EMIT(code, SUB, dst, floatOp(0.0), operand.val);

      return (ValueInfo){.ty = operand.ty, .val = dst};
    case BINARY:;
      ValueInfo left = lowerExpression(code, expr->data.binary.leftExpression, NULL);
      ValueInfo right = lowerExpression(code, expr->data.binary.rightExpression, NULL);
      dst = getResultOperand(preferredDst);
      bool isIntOperation = left.ty == INT && right.ty == INT;

      switch (expr->data.binary.operatorAst) {
        case MULTIPLICATION:
          EMIT(code, MUL, dst, left.val, right.val);
          if (isIntOperation)
            emitIntWrap(code, dst);
          return (ValueInfo){.ty = left.ty, .val = dst};

        case DIVISION:
          if (left.ty == INT) {
            EMIT(code, IDIV, dst, left.val, right.val);
            if (isIntOperation)
              emitIntWrap(code, dst);
          } else
            EMIT(code, DIV, dst, left.val, right.val);

          return (ValueInfo){.ty = left.ty, .val = dst};

        case ADDITION:
          if (left.ty == STRING)
            EMIT(code, CONCAT, dst, left.val, right.val);
          else {
            EMIT(code, ADD, dst, left.val, right.val);
            if (isIntOperation)
              emitIntWrap(code, dst);
          }
          return (ValueInfo){.ty = left.ty, .val = dst};

        case SUBTRACTION:
          EMIT(code, SUB, dst, left.val, right.val);
          if (isIntOperation)
            emitIntWrap(code, dst);
          return (ValueInfo){.ty = left.ty, .val = dst};

        case LESSER:
          EMIT(code, LT, dst, left.val, right.val);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case LESSER_EQUAL:
          EMIT(code, GT, dst, left.val, right.val);
          EMIT(code, NOT, dst, dst);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case GREATER:
          EMIT(code, GT, dst, left.val, right.val);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case GREATER_EQUAL:
          EMIT(code, LT, dst, left.val, right.val);
          EMIT(code, NOT, dst, dst);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case EQUAL:
          EMIT(code, EQ, dst, left.val, right.val);
          return (ValueInfo){.ty = BOOL, .val = dst};

        case NOT_EQUAL:
          EMIT(code, EQ, dst, left.val, right.val);
          EMIT(code, NOT, dst, dst);
          return (ValueInfo){.ty = BOOL, .val = dst};

        default:
          die(ERR_INTERNAL, (int)expr->location.line, "Unknown binary operator");
      }
    default:
      die(ERR_INTERNAL, 0, "Unknown expression type");
  }
}

ValueInfo lowerFunctionCall(StringBuilder *code, AstFunctionCall *call,
                            const Operand *preferredDst) {
  if (strcmp(call->functionName, "print") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL);
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
        die(ERR_INTERNAL, 0, "Unable to print unknown value");
    }
    return (ValueInfo){.ty = UNIT, .val = nilOp()};
  } else if (strcmp(call->functionName, "toStr") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL);
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
        Operand dst = getResultOperand(preferredDst);
        Operand trueLabel = labelOp(newLabel("everything_is_one_function", "bool_to_str"));
        Operand endLabel = labelOp(newLabel("everything_is_one_function", "bool_to_str"));

        EMIT(code, JUMPIFEQ, trueLabel, arg.val, boolOp(true));
        EMIT(code, MOVE, dst, stringOp("false"));
        EMIT(code, JUMP, endLabel);
        EMIT(code, LABEL, trueLabel);
        EMIT(code, MOVE, dst, stringOp("true"));
        EMIT(code, LABEL, endLabel);

        return (ValueInfo){.ty = STRING, .val = dst};
      }
      case UNKNOWN:
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
    Operand invalid = labelOp(newLabel("everything_is_one_function", "readInt_invalid"));
    Operand ok = labelOp(newLabel("everything_is_one_function", "readInt"));
    EMIT(code, READ, dst, typeOp(CT_INT));
    EMIT(code, JUMPIFEQ, invalid, dst, nilOp());
    EMIT(code, LT, temp, dst, intOp(INT32_MIN));
    EMIT(code, JUMPIFEQ, invalid, temp, boolOp(true));
    EMIT(code, GT, temp, dst, intOp(INT32_MAX));
    EMIT(code, JUMPIFEQ, ok, temp, boolOp(false));
    EMIT(code, LABEL, invalid);
    EMIT(code, MOVE, dst, intOp(0));
    EMIT(code, LABEL, ok);
    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "readDouble") == 0) {
    Operand dst = getResultOperand(preferredDst);
    Operand ok = labelOp(newLabel("everything_is_one_function", "readFloat"));
    EMIT(code, READ, dst, typeOp(CT_FLOAT));
    EMIT(code, JUMPIFNEQ, ok, dst, nilOp());
    EMIT(code, MOVE, dst, floatOp(0.0));
    EMIT(code, LABEL, ok);
    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "toInt") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL);
    Operand dst = getResultOperand(preferredDst);
    EMIT(code, FLOAT2INT, dst, arg.val);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "strLength") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL);
    Operand dst = getResultOperand(preferredDst);
    EMIT(code, STRLEN, dst, arg.val);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "subStr") == 0) {
    Operand strlen = varOp(FT_LF, newTempVar());
    Operand tempOp = varOp(FT_LF, newTempVar());
    Operand index = varOp(FT_LF, newTempVar());
    Operand result = varOp(FT_LF, newTempVar());
    Operand exit40 = labelOp(newLabel("everything_is_one_function", "sub_str"));
    Operand loop = labelOp(newLabel("everything_is_one_function", "sub_str_loop"));
    Operand ok = labelOp(newLabel("everything_is_one_function", "sub_str_ok"));
    Operand dst = getResultOperand(preferredDst);

    registerVariable(strlen);
    registerVariable(tempOp);
    registerVariable(index);
    registerVariable(result);

    AstFunctionCallArgument *arg = call->firstArgument;
    ValueInfo str = lowerExpression(code, arg->expression, NULL);
    arg = arg->nextArgument;
    EMIT(code, STRLEN, strlen, str.val);
    ValueInfo start = lowerExpression(code, arg->expression, NULL);
    arg = arg->nextArgument;
    ValueInfo end = lowerExpression(code, arg->expression, NULL);

    EMIT(code, LT, tempOp, start.val, intOp(0));
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));
    EMIT(code, GT, tempOp, start.val, end.val);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));
    EMIT(code, LT, tempOp, start.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(false));
    EMIT(code, GT, tempOp, end.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));

    EMIT(code, MOVE, result, stringOp(""));
    EMIT(code, SUB, tempOp, end.val, start.val);
    EMIT(code, JUMPIFEQ, ok, tempOp, intOp(0));
    EMIT(code, MOVE, index, start.val);  // start doesn't have to be a variable
    EMIT(code, JUMPIFNEQ, loop, tempOp, strlen);
    storeValue(code, result, str);
    EMIT(code, JUMP, ok);

    EMIT(code, LABEL, loop);
    EMIT(code, GETCHAR, tempOp, str.val, index);
    EMIT(code, CONCAT, result, result, tempOp);
    EMIT(code, ADD, index, index, intOp(1));
    EMIT(code, JUMPIFNEQ, loop, index, end.val);
    EMIT(code, JUMP, ok);

    EMIT(code, LABEL, exit40);
    EMIT(code, EXIT, intOp(40));
    EMIT(code, LABEL, ok);
    EMIT(code, MOVE, dst, result);

    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "ord") == 0) {
    Operand tmp = varOp(FT_LF, newTempVar());
    Operand err = labelOp(newLabel("everything_is_one_function", "ord_err"));
    Operand ok = labelOp(newLabel("everything_is_one_function", "ord_ok"));
    Operand dst = getResultOperand(preferredDst);

    registerVariable(tmp);

    AstFunctionCallArgument *arg = call->firstArgument;
    ValueInfo str = lowerExpression(code, arg->expression, NULL);
    arg = arg->nextArgument;
    ValueInfo index = lowerExpression(code, arg->expression, NULL);

    EMIT(code, LT, tmp, index.val, intOp(0));
    EMIT(code, JUMPIFEQ, err, tmp, boolOp(true));
    EMIT(code, STRLEN, tmp, str.val);
    EMIT(code, JUMPIFEQ, err, tmp, intOp(0));
    EMIT(code, LT, tmp, index.val, tmp);
    EMIT(code, JUMPIFEQ, err, tmp, boolOp(false));

    EMIT(code, STRI2INT, dst, str.val, index.val);
    EMIT(code, JUMP, ok);

    EMIT(code, LABEL, err);
    EMIT(code, MOVE, dst, intOp(-1));
    EMIT(code, LABEL, ok);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "chr") == 0) {
    Operand dst = getResultOperand(preferredDst);
    ValueInfo ch = lowerExpression(code, call->firstArgument->expression, NULL);
    EMIT(code, INT2CHAR, dst, ch.val);
    return (ValueInfo){.ty = STRING, .val = dst};
  } else {
    Operand dst = getResultOperand(preferredDst);
    EMIT0(code, CREATEFRAME);
    AstFunctionCallArgument *arg = call->firstArgument;
    unsigned argc = 0;
    while (arg != NULL) {
      Operand op = varOp(FT_TF, funcParam(argc++));
      EMIT(code, DEFVAR, op);
      ValueInfo argValue = lowerExpression(code, arg->expression, &op);
      storeValue(code, op, argValue);
      arg = arg->nextArgument;
    }
    EMIT0(code, PUSHFRAME);
    EMIT(code, DEFVAR, varOp(FT_LF, "$ret"));
    EMIT(code, CALL, labelOp(call->functionName));
    EMIT0(code, POPFRAME);
    EMIT(code, MOVE, dst, varOp(FT_TF, "$ret"));
    return (ValueInfo){.ty = UNKNOWN, .val = dst};  // BUG: Needs symbol table to resolve data type
  }
}

ValueInfo lowerValue(StringBuilder *code, AstValue *val, const Operand *preferredDst) {
  if (val->type == VALUE_EXPRESSION) {
    return lowerExpression(code, val->data.expression, preferredDst);
  }
  return lowerFunctionCall(code, val->data.functionCall, preferredDst);
}

void lowerCodeBlock(StringBuilder *code, AstCodeBlock *block);

void lowerCommand(StringBuilder *code, AstCommand *cmd) {
  switch (cmd->type) {
    case VARIABLE_DECLARATION: {
      Operand var = varOp(FT_LF, cmd->data.variableDeclaration.variableName);
      registerVariable(var);
      ValueInfo value = lowerValue(code, cmd->data.variableDeclaration.value, &var);
      storeValue(code, var, value);
      break;
    }
    case ASSIGNMENT: {
      Operand dst = varOp(FT_LF, cmd->data.assignment.variableName);
      ValueInfo value = lowerValue(code, cmd->data.assignment.value, &dst);
      storeValue(code, dst, value);
      break;
    }
    case FUNCTION_CALL:
      lowerFunctionCall(code, cmd->data.call.functionCall, NULL);
      break;
    case RETURN: {
      Operand ret = varOp(FT_LF, "$ret");
      ValueInfo value = lowerValue(code, cmd->data.returnAst.value, &ret);
      storeValue(code, ret, value);
      EMIT0(code, RETURN);
      break;
    }
    case IF:;
      Operand f = labelOp(newLabel("everything_is_one_function", "if_false"));
      Operand end = labelOp(newLabel("everything_is_one_function", "if_end"));
      ValueInfo ifcond = lowerExpression(code, cmd->data.ifAst.condition, NULL);
      EMIT(code, JUMPIFEQ, f, ifcond.val, boolOp(false));
      lowerCodeBlock(code, cmd->data.ifAst.thenBlock);
      EMIT(code, JUMP, end);
      EMIT(code, LABEL, f);
      lowerCodeBlock(code, cmd->data.ifAst.elseBlock);
      EMIT(code, LABEL, end);
      break;
    case WHILE:;
      Operand loop = labelOp(newLabel("everything_is_one_function", "when"));
      Operand loopEnd = labelOp(newLabel("everything_is_one_function", "when_end"));
      EMIT(code, LABEL, loop);
      ValueInfo whilecond = lowerExpression(code, cmd->data.whileAst.condition, NULL);
      EMIT(code, JUMPIFEQ, loopEnd, whilecond.val, boolOp(false));
      lowerCodeBlock(code, cmd->data.whileAst.codeBlock);
      EMIT(code, JUMP, loop);
      EMIT(code, LABEL, loopEnd);
      break;
    case EXPRESSION:
      lowerExpression(code, cmd->data.expressionAst.expression, NULL);
      break;
  }
}

void lowerCodeBlock(StringBuilder *code, AstCodeBlock *block) {
  AstCommand *cmd = block->firstCommand;
  while (cmd != NULL) {
    lowerCommand(code, cmd);
    cmd = cmd->nextCommand;
  }
}

void lowerFunction(StringBuilder *code, AstFunction *func) {
  EMIT(code, LABEL, labelOp(func->functionName));

  AstFunctionParameter *param = func->firstParameter;
  unsigned argc = 0;
  while (param != NULL) {
    Operand np = varOp(FT_LF, param->parameterName);
    EMIT(code, DEFVAR, np);
    Operand arg = varOp(FT_LF, funcParam(argc++));
    EMIT(code, MOVE, np, arg);
    param = param->nextParameter;
  }

  StringBuilder internal = sbNew(0);
  registry.count = 0;
  if (func->bodyType == VALUE) {
    Operand ret = varOp(FT_LF, "$ret");
    ValueInfo value = lowerValue(&internal, func->body.value, &ret);
    storeValue(&internal, ret, value);
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

char *generateCode(AstProgram program) {
  // Allocate the registry before the string builder so that the string builder can grow
  registerVariable(nilOp());
  StringBuilder code = sbNew(0);
  sbAppend(&code, ".IFJcode26\n");
  EMIT0(&code, CREATEFRAME);
  EMIT0(&code, PUSHFRAME);
  EMIT(&code, DEFVAR, varOp(FT_LF, "$ret"));
  EMIT(&code, DEFVAR, varOp(FT_GF, INT_WRAP_VALUE));
  EMIT(&code, DEFVAR, varOp(FT_GF, INT_WRAP_TEMP));
  for (AstFunction *func = program.firstFunction; func != NULL; func = func->nextFunction)
    if (func->isMain) {
      EMIT(&code, CALL, labelOp(func->functionName));
      break;
    }
  EMIT(&code, EXIT, intOp(0));
  emitIntWrapRoutine(&code);
  AstFunction *func = program.firstFunction;
  while (func != NULL) {
    lowerFunction(&code, func);
    func = func->nextFunction;
  }
  return sbBuild(&code);
}
