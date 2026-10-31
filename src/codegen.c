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
  AstDataType ty;
  Operand val;
} ValueInfo;

static Operand getResultOperand(StringBuilder *code, const Operand *preferred) {
  if (preferred != NULL) {
    if (preferred->type != OK_VAR)
      die(ERR_INTERNAL, 0, "Result destination must be a variable");

    return *preferred;
  }

  Operand tmp = varOp(FT_LF, newTempVar());
  EMIT(code, DEFVAR, tmp);
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
      return (ValueInfo){
          .ty = UNKNOWN,
          .val =
              varOp(FT_LF, expr->data.variable)};  // BUG: Needs symbol table to resolve data type
    case UNARY:
      if (expr->data.unary.operatorAst != NEGATION)
        die(ERR_INTERNAL, (int)expr->location.line,
            "Binary operation cannot be performed on single operand");

      ValueInfo operand = lowerExpression(code, expr->data.unary.expression, NULL);

      dst = getResultOperand(code, preferredDst);

      EMIT(code, SUB, dst, intOp(0), operand.val);

      return (ValueInfo){.ty = operand.ty, .val = dst};
    case BINARY:;
      ValueInfo left = lowerExpression(code, expr->data.binary.leftExpression, NULL);
      ValueInfo right = lowerExpression(code, expr->data.binary.rightExpression, NULL);
      dst = getResultOperand(code, preferredDst);

      switch (expr->data.binary.operatorAst) {
        case MULTIPLICATION:
          EMIT(code, MUL, dst, left.val, right.val);
          return (ValueInfo){.ty = left.ty, .val = dst};

        case DIVISION:
          if (left.ty == INT)
            EMIT(code, IDIV, dst, left.val, right.val);
          else
            EMIT(code, DIV, dst, left.val, right.val);

          return (ValueInfo){.ty = left.ty, .val = dst};

        case ADDITION:
          if (left.ty == STRING)
            EMIT(code, CONCAT, dst, left.val, right.val);
          else
            EMIT(code, ADD, dst, left.val, right.val);
          return (ValueInfo){.ty = left.ty, .val = dst};

        case SUBTRACTION:
          EMIT(code, SUB, dst, left.val, right.val);
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
        Operand dst = getResultOperand(code, preferredDst);
        if (arg.ty == INT)
          EMIT(code, INT2STR, dst, arg.val);
        else
          EMIT(code, FLOAT2STR, dst, arg.val);
        return (ValueInfo){.ty = STRING, .val = dst};
      }
      case BOOL: {
        Operand dst = getResultOperand(code, preferredDst);
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
    Operand dst = getResultOperand(code, preferredDst);
    EMIT(code, READ, dst, typeOp(CT_STRING));
    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "readInt") == 0) {
    Operand dst = getResultOperand(code, preferredDst);
    EMIT(code, READ, dst, typeOp(CT_INT));
    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "readDouble") == 0) {
    Operand dst = getResultOperand(code, preferredDst);
    EMIT(code, READ, dst, typeOp(CT_FLOAT));
    return (ValueInfo){.ty = STRING, .val = dst};
  } else if (strcmp(call->functionName, "toInt") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL);
    Operand dst = getResultOperand(code, preferredDst);
    EMIT(code, FLOAT2INT, dst, arg.val);
    return (ValueInfo){.ty = INT, .val = dst};
  } else if (strcmp(call->functionName, "strLength") == 0) {
    ValueInfo arg = lowerExpression(code, call->firstArgument->expression, NULL);
    Operand dst = getResultOperand(code, preferredDst);
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
    Operand dst = getResultOperand(code, preferredDst);

    EMIT(code, DEFVAR, strlen);
    EMIT(code, DEFVAR, tempOp);
    EMIT(code, DEFVAR, index);
    EMIT(code, DEFVAR, result);

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
    EMIT(code, GT, tempOp, start.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));
    EMIT(code, GT, tempOp, end.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, boolOp(true));

    EMIT(code, MOVE, result, stringOp(""));
    EMIT(code, SUB, tempOp, end.val, start.val);
    EMIT(code, JUMPIFEQ, ok, tempOp, intOp(0));
    EMIT(code, MOVE, index, start.val);  // start doesn't have to be a variable
    EMIT(code, JUMPIFNEQ, loop, tempOp, strlen);
    storeValue(code, dst, str);
    EMIT(code, JUMP, ok);

    EMIT(code, LABEL, loop);
    EMIT(code, GETCHAR, tempOp, str.val, index);
    EMIT(code, CONCAT, result, result, tempOp);
    EMIT(code, ADD, index, index, intOp(1));
    EMIT(code, JUMPIFNEQ, loop, index, end.val);
    EMIT(code, MOVE, dst, result);
    EMIT(code, JUMP, ok);

    EMIT(code, LABEL, exit40);
    EMIT(code, EXIT, intOp(40));
    EMIT(code, LABEL, ok);

    return (ValueInfo){.ty = STRING, .val = dst};
  } else {
    Operand dst = getResultOperand(code, preferredDst);
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
      EMIT(code, DEFVAR, var);
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

  if (func->bodyType == VALUE) {
    Operand ret = varOp(FT_LF, "$ret");
    ValueInfo value = lowerValue(code, func->body.value, &ret);
    storeValue(code, ret, value);
    EMIT0(code, RETURN);
  } else {
    lowerCodeBlock(code, func->body.codeBlock);
  }
}

char *generateCode(AstProgram program) {
  StringBuilder code = sbNew(0);
  sbAppend(&code, ".IFJcode26\n");
  EMIT0(&code, CREATEFRAME);
  EMIT0(&code, PUSHFRAME);
  EMIT(&code, DEFVAR, varOp(FT_LF, "$ret"));
  for (AstFunction *func = program.firstFunction; func != NULL; func = func->nextFunction)
    if (func->isMain) {
      EMIT(&code, CALL, labelOp(func->functionName));
      break;
    }
  EMIT(&code, EXIT, intOp(0));
  AstFunction *func = program.firstFunction;
  while (func != NULL) {
    lowerFunction(&code, func);
    func = func->nextFunction;
  }
  return sbBuild(&code);
}
