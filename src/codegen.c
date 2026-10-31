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
          for (size_t i = 0; i < strlen(c.s); i++) {
            char ch = c.s[i];
            if (ch > 32 && ch != 35 && ch != 92)
              sbAppendChar(out, ch);
            else
              sbAppendf(out, "%3d", (int)ch);
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

/// Lower expression into IFJcode26
/// @param code Reference to string builder
/// @param expr Pointer to expression to lower
/// @param val Optional desired operand to store in (uses temporary if NULL)
/// @note Expects all used variables to be present in LF
/// @note Resulting value info contains destination of value
ValueInfo lowerExpression(StringBuilder *code, AstExpression *expr, Operand *val) {
  Operand dst;
  if (val != NULL)
    dst = *val;
  else if (expr->expressionType == VARIABLE) {
    return (ValueInfo){
        .ty = UNIT,
        .val = varOp(FT_LF, expr->data.variable)};  // BUG: Needs symbol table to resolve data type
  } else {
    dst = varOp(FT_LF, newTempVar());
    EMIT(code, DEFVAR, dst);
  }

  ValueInfo inf;
  switch (expr->expressionType) {
    case INT_LITERAL:
      EMIT(code, MOVE, dst, intOp(expr->data.intValue));
      return (ValueInfo){.ty = INT, .val = dst};
    case DOUBLE_LITERAL:
      EMIT(code, MOVE, dst, floatOp(expr->data.doubleValue));
      return (ValueInfo){.ty = DOUBLE, .val = dst};
    case STRING_LITERAL:
      EMIT(code, MOVE, dst, stringOp(expr->data.stringValue));
      return (ValueInfo){.ty = STRING, .val = dst};
    case UNIT_LITERAL:
      EMIT(code, MOVE, dst, nilOp());
      return (ValueInfo){.ty = UNIT, .val = dst};
    case VARIABLE:
      EMIT(code, MOVE, dst, varOp(FT_LF, expr->data.variable));
      return (ValueInfo){.ty = UNIT, .val = dst};  // BUG: Needs symbol table to resolve data type
    case UNARY:
      if (expr->data.unary.operatorAst != NEGATION)
        die(ERR_INTERNAL, (int)expr->location.line,
            "Binary operation cannot be performed on single operand");
      inf = lowerExpression(code, expr->data.unary.expression, val);
      EMIT(code, SUB, dst, intOp(0), dst);
      return (ValueInfo){.ty = inf.ty, .val = dst};
    case BINARY:;
      inf = lowerExpression(code, expr->data.binary.leftExpression, val);
      ValueInfo inf2 = lowerExpression(code, expr->data.binary.rightExpression, NULL);
      switch (expr->data.binary.operatorAst) {
        case NEGATION:
          die(ERR_INTERNAL, (int)expr->location.line,
              "Unary operation cannot be performed on two operands");
        case MULTIPLICATION:
          EMIT(code, MUL, dst, inf.val, inf2.val);
          break;
        case DIVISION:
          EMIT(code, IDIV, dst, inf.val, inf2.val);
          break;
        case ADDITION:
          EMIT(code, ADD, dst, inf.val, inf2.val);
          break;
        case SUBTRACTION:
          EMIT(code, SUB, dst, inf.val, inf2.val);
          break;
        case LESSER:
          EMIT(code, LT, dst, inf.val, inf2.val);
          break;
        case LESSER_EQUAL:;
          EMIT(code, GT, dst, inf.val, inf2.val);
          EMIT(code, NOT, dst, dst);
          break;
        case GREATER:
          EMIT(code, GT, dst, inf.val, inf2.val);
          break;
        case GREATER_EQUAL:;
          EMIT(code, LT, dst, inf.val, inf2.val);
          EMIT(code, NOT, dst, dst);
          break;
        case EQUAL:
          EMIT(code, EQ, dst, inf.val, inf2.val);
          break;
        case NOT_EQUAL:
          EMIT(code, EQ, dst, inf.val, inf2.val);
          EMIT(code, NOT, dst, dst);
          break;
      }
      return (ValueInfo){.ty = inf.ty, .val = dst};
    default:
      die(ERR_INTERNAL, 0, "Unknown expression type");
  }
}

void lowerFunctionCall(StringBuilder *code, AstFunctionCall *call, Operand *dst) {
  Operand ret;
  if (dst != NULL)
    ret = *dst;
  else
    ret = varOp(FT_LF, "$ret");

  ValueInfo exprRet;
  if (strcmp(call->functionName, "print") == 0) {
    exprRet = lowerExpression(code, call->firstArgument->expression, NULL);
    switch (exprRet.ty) {
      case INT:
      case DOUBLE:
      case STRING:
      case BOOL:;
        EMIT(code, WRITE, exprRet.val);
        break;
      case UNIT:
        EMIT(code, WRITE, stringOp("()"));
        break;
      case UNKNOWN:
        die(ERR_INTERNAL, 0, "Unable to print unknown value");
    }
    return;
  } else if (strcmp(call->functionName, "toStr") == 0) {
    exprRet = lowerExpression(code, call->firstArgument->expression, &ret);
    switch (exprRet.ty) {
      case INT:
        EMIT(code, INT2STR, ret, exprRet.val);
        break;
      case DOUBLE:
        EMIT(code, FLOAT2STR, ret, exprRet.val);
        break;
      case STRING:
        // WARN: Could need move if lowerExpression ever doesn't use val even if supplied
        break;
      case BOOL:;
        Operand trueLabel = labelOp(newLabel("everything_is_one_function", "bool_to_str"));
        Operand endLabel = labelOp(newLabel("everything_is_one_function", "bool_to_str"));
        EMIT(code, JUMPIFEQ, trueLabel, exprRet.val, boolOp(true));
        EMIT(code, MOVE, ret, stringOp("false"));
        EMIT(code, JUMP, endLabel);
        EMIT(code, LABEL, trueLabel);
        EMIT(code, MOVE, ret, stringOp("true"));
        EMIT(code, LABEL, endLabel);
        break;
      case UNIT:
        EMIT(code, MOVE, ret, stringOp("()"));
        break;
      case UNKNOWN:
        die(ERR_INTERNAL, 0, "Unable to print unknown value");
    }
    return;
  } else if (strcmp(call->functionName, "readLine") == 0) {
    EMIT(code, READ, ret, typeOp(CT_STRING));
  } else if (strcmp(call->functionName, "readInt") == 0) {
    EMIT(code, READ, ret, typeOp(CT_INT));
  } else if (strcmp(call->functionName, "readDouble") == 0) {
    EMIT(code, READ, ret, typeOp(CT_FLOAT));
  } else if (strcmp(call->functionName, "toInt") == 0) {
    exprRet = lowerExpression(code, call->firstArgument->expression, &ret);
    EMIT(code, FLOAT2INT, ret, ret);
  } else if (strcmp(call->functionName, "strLength") == 0) {
    exprRet = lowerExpression(code, call->firstArgument->expression, &ret);
    EMIT(code, STRLEN, ret, ret);
  } else if (strcmp(call->functionName, "subStr") == 0) {
    Operand strlen = varOp(FT_LF, newTempVar());
    Operand tempOp = varOp(FT_LF, newTempVar());
    Operand index = varOp(FT_LF, newTempVar());
    Operand exit40 = labelOp(newLabel("everything_is_one_function", "sub_str"));
    Operand loop = labelOp(newLabel("everything_is_one_function", "sub_str_loop"));
    Operand ok = labelOp(newLabel("everything_is_one_function", "sub_str_ok"));

    EMIT(code, DEFVAR, strlen);
    EMIT(code, DEFVAR, tempOp);
    EMIT(code, DEFVAR, index);

    AstFunctionCallArgument *arg = call->firstArgument;
    ValueInfo str = lowerExpression(code, arg->expression, NULL);
    arg = arg->nextArgument;
    EMIT(code, STRLEN, strlen, str.val);
    ValueInfo start = lowerExpression(code, arg->expression, NULL);
    arg = arg->nextArgument;
    ValueInfo end = lowerExpression(code, arg->expression, NULL);

    EMIT(code, LT, tempOp, start.val, intOp(0));
    EMIT(code, JUMPIFEQ, exit40, tempOp, intOp(1));
    EMIT(code, GT, tempOp, start.val, end.val);
    EMIT(code, JUMPIFEQ, exit40, tempOp, intOp(1));
    EMIT(code, LT, tempOp, start.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, intOp(0));
    EMIT(code, GT, tempOp, end.val, strlen);
    EMIT(code, JUMPIFEQ, exit40, tempOp, intOp(1));

    EMIT(code, MOVE, ret, stringOp(""));
    EMIT(code, SUB, tempOp, end.val, start.val);
    EMIT(code, JUMPIFEQ, ok, tempOp, intOp(0));
    EMIT(code, MOVE, index, start.val);  // start doesn't have to be a variable
    EMIT(code, JUMPIFNEQ, loop, tempOp, strlen);
    EMIT(code, MOVE, ret, str.val);
    EMIT(code, JUMP, ok);

    EMIT(code, LABEL, loop);
    EMIT(code, GETCHAR, tempOp, str.val, index);
    EMIT(code, CONCAT, ret, ret, tempOp);
    EMIT(code, ADD, index, intOp(1));
    EMIT(code, JUMPIFNEQ, loop, index, end.val);
    EMIT(code, JUMP, ok);

    EMIT(code, LABEL, exit40);
    EMIT(code, EXIT, intOp(40));
    EMIT(code, LABEL, ok);
  }

  EMIT0(code, CREATEFRAME);
  AstFunctionCallArgument *arg = call->firstArgument;
  unsigned argc = 0;
  while (arg != NULL) {
    Operand op = varOp(FT_TF, funcParam(argc++));
    lowerExpression(code, arg->expression, &op);
    arg = arg->nextArgument;
  }
  EMIT0(code, PUSHFRAME);
  EMIT(code, DEFVAR, ret);
  EMIT(code, CALL, labelOp(call->functionName));
  EMIT0(code, POPFRAME);
  EMIT(code, MOVE, ret, varOp(FT_TF, "$ret"));
}

void lowerValue(StringBuilder *code, AstValue *val, Operand *dst) {
  Operand ret;
  if (dst != NULL)
    ret = *dst;
  else
    ret = varOp(FT_LF, newTempVar());
  if (val->type == VALUE_EXPRESSION)
    lowerExpression(code, val->data.expression, &ret);
  else
    lowerFunctionCall(code, val->data.functionCall, &ret);
}

void lowerCodeBlock(StringBuilder *code, AstCodeBlock *block);

void lowerCommand(StringBuilder *code, AstCommand *cmd) {
  switch (cmd->type) {
    case VARIABLE_DECLARATION:;
      Operand var = varOp(FT_LF, cmd->data.variableDeclaration.variableName);
      lowerValue(code, cmd->data.variableDeclaration.value, &var);
      break;
    case ASSIGNMENT:;
      Operand dst = varOp(FT_LF, cmd->data.assignment.variableName);
      lowerValue(code, cmd->data.assignment.value, &dst);
      break;
    case FUNCTION_CALL:
      lowerFunctionCall(code, cmd->data.call.functionCall, NULL);
      break;
    case RETURN:;
      Operand ret = varOp(FT_LF, "$ret");
      lowerValue(code, cmd->data.returnAst.value, &ret);
      EMIT0(code, RETURN);
      break;
    case IF:;
      Operand f = labelOp(newLabel("everything_is_one_function", "if_false"));
      ValueInfo ifcond = lowerExpression(code, cmd->data.ifAst.condition, NULL);
      EMIT(code, JUMPIFEQ, f, ifcond.val, intOp(0));
      lowerCodeBlock(code, cmd->data.ifAst.thenBlock);
      EMIT(code, LABEL, f);
      lowerCodeBlock(code, cmd->data.ifAst.elseBlock);
      break;
    case WHILE:;
      Operand loop = labelOp(newLabel("everything_is_one_function", "when"));
      Operand loopEnd = labelOp(newLabel("everything_is_one_function", "when_end"));
      EMIT(code, LABEL, loop);
      ValueInfo whilecond = lowerExpression(code, cmd->data.whileAst.condition, NULL);
      EMIT(code, JUMPIFEQ, loopEnd, whilecond.val, intOp(0));
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
  if (func->isMain)
    EMIT(code, LABEL, labelOp("$main"));

  AstFunctionParameter *param = func->firstParameter;
  unsigned argc = 0;
  while (param != NULL) {
    Operand np = varOp(FT_LF, param->parameterName);
    Operand arg = varOp(FT_LF, funcParam(argc++));
    EMIT(code, MOVE, np, arg);
    param = param->nextParameter;
  }

  if (func->bodyType == VALUE) {
    Operand ret = varOp(FT_LF, "$ret");
    lowerValue(code, func->body.value, &ret);
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
  EMIT(&code, CALL, labelOp("$main"));
  EMIT0(&code, POPFRAME);
  EMIT(&code, EXIT, varOp(FT_TF, "$ret"));
  AstFunction *func = program.firstFunction;
  while (func != NULL) {
    lowerFunction(&code, func);
    func = func->nextFunction;
  }
  return sbBuild(&code);
}
