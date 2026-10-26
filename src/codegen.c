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
  sbAppendChar(out, ' ');
  for (size_t i = 0; i < argc; i++) {
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

/// Lower expression into IFJcode26
/// @note Expects all used variables to be present in LF
/// @note Result is left on top of stack
void lowerExpression(StringBuilder *code, AstExpression *expr) {
  switch (expr->expressionType) {
    case INT_LITERAL:
      EMIT(code, PUSHS, intOp(expr->data.intValue));
      break;
    case DOUBLE_LITERAL:
      EMIT(code, PUSHS, floatOp(expr->data.doubleValue));
      break;
    case STRING_LITERAL:
      EMIT(code, PUSHS, stringOp(expr->data.stringValue));
      break;
    case UNIT_LITERAL:
      EMIT(code, PUSHS, nilOp());
      break;
    case VARIABLE:
      EMIT(code, PUSHS, varOp(FT_LF, expr->data.variable));
      break;
    case UNARY:
      EMIT(code, PUSHS, intOp(0));
      lowerExpression(code, expr->data.unary.expression);
      if (expr->data.unary.operatorAst != NEGATION)
        die(ERR_INTERNAL, (int)expr->location.line,
            "Binary operation cannot be performed on single operand");
      EMIT0(code, SUBS);
      break;
    case BINARY:
      lowerExpression(code, expr->data.binary.leftExpression);
      lowerExpression(code, expr->data.binary.rightExpression);
      switch (expr->data.binary.operatorAst) {
        case NEGATION:
          die(ERR_INTERNAL, (int)expr->location.line,
              "Unary operation cannot be performed on two operands");
        case MULTIPLICATION:
          EMIT0(code, MULS);
          break;
        case DIVISION:
          EMIT0(code, IDIVS);
          break;
        case ADDITION:
          EMIT0(code, ADDS);
          break;
        case SUBTRACTION:
          EMIT0(code, SUBS);
          break;
        case LESSER:
          EMIT0(code, LTS);
          break;
        case LESSER_EQUAL:;
          EMIT0(code, GTS);
          EMIT0(code, NOTS);
          break;
        case GREATER:
          EMIT0(code, GTS);
          break;
        case GREATER_EQUAL:;
          EMIT0(code, LTS);
          EMIT0(code, NOTS);
          break;
        case EQUAL:
          EMIT0(code, EQS);
          break;
        case NOT_EQUAL:
          EMIT0(code, EQS);
          EMIT0(code, NOTS);
          break;
      }
  }
}

void lowerFunctionCall(StringBuilder *code, AstFunctionCall *call) {
  EMIT0(code, CREATEFRAME);
  AstFunctionCallArgument *arg = call->firstArgument;
  unsigned argc = 0;
  while (arg != NULL) {
    lowerExpression(code, arg->expression);
    Operand op = varOp(FT_TF, funcParam(argc++));
    EMIT(code, DEFVAR, op);
    EMIT(code, POPS, op);
    arg = arg->nextArgument;
  }
  EMIT0(code, PUSHFRAME);
  EMIT(code, CALL, labelOp(call->functionName));
  EMIT0(code, POPFRAME);
  EMIT(code, PUSHS, varOp(FT_TF, "$ret"));
}

void lowerValue(StringBuilder *code, AstValue *val) {
  if (val->type == VALUE_EXPRESSION)
    lowerExpression(code, val->data.expression);
  else
    lowerFunctionCall(code, val->data.functionCall);
}

void lowerCodeBlock(StringBuilder *code, AstCodeBlock *block);

void lowerCommand(StringBuilder *code, AstCommand *cmd) {
  switch (cmd->type) {
    case VARIABLE_DECLARATION:;
      Operand var = varOp(FT_LF, cmd->data.variableDeclaration.variableName);
      EMIT(code, DEFVAR, var);
      lowerValue(code, cmd->data.variableDeclaration.value);
      EMIT(code, POPS, var);
      break;
    case ASSIGNMENT:;
      Operand dst = varOp(FT_LF, cmd->data.assignment.variableName);
      EMIT(code, DEFVAR, dst);
      lowerValue(code, cmd->data.assignment.value);
      EMIT(code, POPS, dst);
      break;
    case FUNCTION_CALL:
      lowerFunctionCall(code, cmd->data.call.functionCall);
      break;
    case RETURN:
      lowerValue(code, cmd->data.returnAst.value);
      EMIT(code, POPS, varOp(FT_LF, "$ret"));
      EMIT0(code, RETURN);
      break;
    case IF:;
      Operand t = labelOp(newLabel("everything_is_one_function", "if_true"));
      Operand f = labelOp(newLabel("everything_is_one_function", "if_false"));
      lowerExpression(code, cmd->data.ifAst.condition);
      EMIT(code, PUSHS, intOp(0));
      EMIT(code, JUMPIFEQS, f);
      EMIT(code, JUMP, t);
      EMIT(code, LABEL, t);
      lowerCodeBlock(code, cmd->data.ifAst.thenBlock);
      EMIT(code, LABEL, f);
      lowerCodeBlock(code, cmd->data.ifAst.elseBlock);
      break;
    case WHILE:;
      Operand loop = labelOp(newLabel("everything_is_one_function", "when"));
      Operand loopEnd = labelOp(newLabel("everything_is_one_function", "when_end"));
      EMIT(code, LABEL, loop);
      lowerExpression(code, cmd->data.whileAst.condition);
      EMIT(code, PUSHS, intOp(0));
      EMIT(code, JUMPIFEQS, loopEnd);
      EMIT(code, JUMP, loop);
      lowerCodeBlock(code, cmd->data.whileAst.codeBlock);
      EMIT(code, LABEL, loopEnd);
      break;
    case EXPRESSION:
      lowerExpression(code, cmd->data.expressionAst.expression);
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
    lowerValue(code, func->body.value);
    EMIT(code, POPS, varOp(FT_LF, "$ret"));
  } else {
    lowerCodeBlock(code, func->body.codeBlock);
  }
}

char *generateCode(AstProgram program) {
  StringBuilder code = sbNew(0);
  sbAppend(&code, ".IFJcode26\n");
  EMIT0(&code, CREATEFRAME);
  EMIT0(&code, PUSHFRAME);
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
