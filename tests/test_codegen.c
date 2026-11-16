/**
 * @file test_codegen.c
 * Project: IFJ26 compiler
 * @brief Code generator tests
 * @author Jakub Palacky <xpalacj00>
 *
 * Every test builds a small AST, generates IFJcode26 from it and runs the
 * result through the reference interpreter in tests/bin. Only the observable
 * behavior (stdout and exit code) is checked
 *
 * Paths are relative to the root of the repository, which is where `make test`
 * runs from.
 */

// fork, mkstemp and friends are not part of plain C11
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ast.h"
#include "codegen.h"
#include "common.h"
#include "symtable.h"
#include "tst.h"

// AI-BEGIN [claude-opus-5-5, Claude Code]
// Prompt: "take the current state of the main.c file (just one larger test
// program AST) and inspired by it write a series of unit tests that follow how
// the other unit tests in this repo work [...] actually run the generated code
// through the interpreter [...] split it into several smaller AST programs,
// generate the code for each and run it through the interpreter"
// Context: "src/main.c, src/codegen.c, tests/, specification of IFJ26 and IFJcode26"

#define INTERPRETER "tests/bin/ic26int"
#define TIMEOUT_SECONDS 5

// ============================================================
// Running programs
// ============================================================

typedef struct {
  bool generated;  ///< generateCode() finished without dying
  int exitCode;    ///< Exit code of the interpreter, -1 if it never ran or was
                   ///< killed
  char *out;       ///< Standard output of the interpreter
  char *err;       ///< Standard error of the interpreter
} RunResult;

static int waitForExit(pid_t pid) {
  int status;

  if (pid < 0 || waitpid(pid, &status, 0) < 0 || !WIFEXITED(status))
    return -1;

  return WEXITSTATUS(status);
}

static char *readWholeFile(FILE *file) {
  fseek(file, 0, SEEK_END);
  size_t size = (size_t)ftell(file);
  fseek(file, 0, SEEK_SET);

  char *content = arenaZalloc(size + 1);
  if (fread(content, 1, size, file) != size)
    die(ERR_INTERNAL, 0, "failed to read test output");

  return content;
}

// ============================================================
// Symbol tables
// ============================================================

static void attachToBlock(AstCodeBlock *block, SymTable *table);

/// Registers a variable, a name declared twice in one function keeps its first
/// type, which is fine as the tests never redeclare a name with another type
static void insertVariable(SymTable *table, char *name, AstDataType type) {
  StSymbol *symbol = arenaZalloc(sizeof(*symbol));
  symbol->type = ST_VARIABLE;
  symbol->data.variable.dataType = type;
  symbol->data.variable.isMutable = true;
  symtableInsertSymbol(table, name, symbol);
}

static void attachToCommands(AstCommand *cmd, SymTable *table) {
  for (; cmd != NULL; cmd = cmd->nextCommand) {
    if (cmd->type == VARIABLE_DECLARATION) {
      insertVariable(table, cmd->data.variableDeclaration.variableName,
                     cmd->data.variableDeclaration.resolvedType);
    } else if (cmd->type == IF) {
      attachToBlock(cmd->data.ifAst.thenBlock, table);
      attachToBlock(cmd->data.ifAst.elseBlock, table);
    } else if (cmd->type == WHILE) {
      attachToBlock(cmd->data.whileAst.codeBlock, table);
    }
  }
}

static void attachToBlock(AstCodeBlock *block, SymTable *table) {
  if (block == NULL)
    return;

  block->symTable = table;
  attachToCommands(block->firstCommand, table);
}

/// The tests build their ASTs by hand and skip semantic analysis, so this
/// stands in for it: every user function is registered with its signature in
/// a global table, and each function gets one child table holding its
/// parameters and all of its variables, which is attached to all of its code
/// blocks. Codegen only looks symbols up, so one scope per function is enough
static void attachSymbolTables(AstProgram *program) {
  SymTable *global = symtableCreate(NULL);

  for (AstFunction *func = program->firstFunction; func != NULL; func = func->nextFunction) {
    size_t paramCount = 0;
    for (AstFunctionParameter *p = func->firstParameter; p != NULL; p = p->nextParameter)
      paramCount++;

    AstDataType *paramTypes = arenaZalloc(sizeof(AstDataType) * (paramCount + 1));
    size_t i = 0;
    for (AstFunctionParameter *p = func->firstParameter; p != NULL; p = p->nextParameter)
      paramTypes[i++] = p->dataType;

    StSymbol *symbol = arenaZalloc(sizeof(*symbol));
    symbol->type = ST_FUNCTION;
    symbol->declarationLocation = func->location;
    symbol->data.function.returnType = func->resolvedReturnType;
    symbol->data.function.parameterTypes = paramTypes;
    symbol->data.function.parameterCount = paramCount;
    symbol->data.function.isMain = func->isMain;

    if (!symtableInsertSymbol(global, func->functionName, symbol))
      die(ERR_INTERNAL, 0, "test defines function %s twice", func->functionName);
  }

  for (AstFunction *func = program->firstFunction; func != NULL; func = func->nextFunction) {
    SymTable *local = symtableCreate(global);
    for (AstFunctionParameter *p = func->firstParameter; p != NULL; p = p->nextParameter)
      insertVariable(local, p->parameterName, p->dataType);

    func->symTable = local;
    if (func->bodyType == CODE_BLOCK)
      attachToBlock(func->body.codeBlock, local);
  }
}

/// Generates code in a child process, because generateCode() can die() or
/// crash, and because its global state would otherwise keep pointing into the
/// arena after arenaCleanup()
static bool generateInto(AstProgram program, FILE *file) {
  pid_t pid = fork();

  if (pid == 0) {
    alarm(TIMEOUT_SECONDS);
    attachSymbolTables(&program);

    bool written = fputs(generateCode(program), file) >= 0 && fflush(file) == 0;
    _exit(written ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  return waitForExit(pid) == EXIT_SUCCESS;
}

/// Generates code for the program and runs it in the interpreter
static RunResult runProgram(AstProgram program, const char *input) {
  RunResult result = {.exitCode = -1, .out = "", .err = ""};

  char codePath[] = "/tmp/ifj26-test-codegen-XXXXXX";
  int codeFd = mkstemp(codePath);
  FILE *code = codeFd < 0 ? NULL : fdopen(codeFd, "w");
  FILE *in = tmpfile();
  FILE *out = tmpfile();
  FILE *err = tmpfile();

  if (code == NULL || in == NULL || out == NULL || err == NULL)
    die(ERR_INTERNAL, 0, "failed to create temporary files");

  result.generated = generateInto(program, code);
  fclose(code);

  if (result.generated) {
    fputs(input, in);
    fseek(in, 0, SEEK_SET);

    pid_t pid = fork();

    if (pid == 0) {
      dup2(fileno(in), STDIN_FILENO);
      dup2(fileno(out), STDOUT_FILENO);
      dup2(fileno(err), STDERR_FILENO);
      alarm(TIMEOUT_SECONDS);  // Survives exec, so a generated infinite loop
                               // can't hang the tests

      execl(INTERPRETER, INTERPRETER, codePath, (char *)NULL);

      perror("failed to run " INTERPRETER);
      _exit(127);
    }

    result.exitCode = waitForExit(pid);
    result.out = readWholeFile(out);
    result.err = readWholeFile(err);
  }

  unlink(codePath);
  fclose(in);
  fclose(out);
  fclose(err);

  return result;
}

/// Runs the program with the given stdin and checks its exit code and stdout
#define EXPECT_RUN(program, input, code, output)                                           \
  do {                                                                                     \
    RunResult run = runProgram((program), (input));                                        \
    const char *expected = (output);                                                       \
    tstcheck(run.generated, "code generation should succeed");                             \
    tstcheck(run.exitCode == (code), "exit code should be %d, got %d, stderr: %s", (code), \
             run.exitCode, run.err);                                                       \
    tstcheck(strcmp(run.out, expected) == 0, "expected output:\n%s\nactual output:\n%s\n", \
             expected, run.out);                                                           \
  } while (0)

/// Program without input that should finish successfully and print `output`
#define EXPECT_OUTPUT(program, output) EXPECT_RUN((program), "", 0, (output))

// ============================================================
// Expressions
// ============================================================

static AstExpression *newExpr(AstExpressionType type, AstDataType dataType) {
  AstExpression *e = arenaZalloc(sizeof(*e));

  e->expressionType = type;
  e->deducedDataType = dataType;
  e->location = (AstLocation){.line = 1, .column = 1};

  return e;
}

static AstExpression *intExpr(int32_t value) {
  AstExpression *e = newExpr(INT_LITERAL, INT);
  e->data.intValue = value;
  return e;
}

static AstExpression *doubleExpr(double value) {
  AstExpression *e = newExpr(DOUBLE_LITERAL, DOUBLE);
  e->data.doubleValue = value;
  return e;
}

static AstExpression *stringExpr(char *value) {
  AstExpression *e = newExpr(STRING_LITERAL, STRING);
  e->data.stringValue = value;
  return e;
}

static AstExpression *unitExpr(void) {
  return newExpr(UNIT_LITERAL, UNIT);
}

static AstExpression *varExpr(char *name, AstDataType type) {
  AstExpression *e = newExpr(VARIABLE, type);
  e->data.variable = name;
  return e;
}

static AstExpression *intVar(char *name) {
  return varExpr(name, INT);
}

static AstExpression *negate(AstExpression *operand, AstDataType type) {
  AstExpression *e = newExpr(UNARY, type);

  e->data.unary.operatorAst = NEGATION;
  e->data.unary.expression = operand;

  return e;
}

static AstExpression *binaryExpr(AstOperator op, AstExpression *left, AstExpression *right,
                                 AstDataType type) {
  AstExpression *e = newExpr(BINARY, type);

  e->data.binary.operatorAst = op;
  e->data.binary.leftExpression = left;
  e->data.binary.rightExpression = right;

  return e;
}

static AstExpression *intOperation(AstOperator op, AstExpression *left, AstExpression *right) {
  return binaryExpr(op, left, right, INT);
}

static AstExpression *condition(AstOperator op, AstExpression *left, AstExpression *right) {
  return binaryExpr(op, left, right, BOOL);
}

// ============================================================
// Function calls and values
// ============================================================

static AstFunctionCall *makeCall(char *name, AstExpression **args, size_t argc) {
  AstFunctionCall *call = arenaZalloc(sizeof(*call));

  call->functionName = name;
  call->location = (AstLocation){.line = 1, .column = 1};

  for (size_t i = 0; i < argc; i++) {
    AstFunctionCallArgument *arg = arenaZalloc(sizeof(*arg));
    arg->expression = args[i];

    if (call->lastArgument == NULL)
      call->firstArgument = arg;
    else
      call->lastArgument->nextArgument = arg;

    call->lastArgument = arg;
  }

  return call;
}

/// Call with at least one argument
#define CALL_OF(name, ...)                           \
  makeCall((name), (AstExpression *[]){__VA_ARGS__}, \
           sizeof((AstExpression *[]){__VA_ARGS__}) / sizeof(AstExpression *))

/// Call without arguments
static AstFunctionCall *callNoArgs(char *name) {
  return makeCall(name, NULL, 0);
}

static AstValue *exprValue(AstExpression *expression) {
  AstValue *value = arenaZalloc(sizeof(*value));

  value->type = VALUE_EXPRESSION;
  value->data.expression = expression;

  return value;
}

static AstValue *callValue(AstFunctionCall *call) {
  AstValue *value = arenaZalloc(sizeof(*value));

  value->type = VALUE_FUNCTION_CALL;
  value->data.functionCall = call;

  return value;
}

// ============================================================
// Code blocks and commands
// ============================================================

static AstCodeBlock *newBlock(void) {
  return arenaZalloc(sizeof(AstCodeBlock));
}

static AstCommand *newCommand(AstCommandType type) {
  AstCommand *cmd = arenaZalloc(sizeof(*cmd));

  cmd->type = type;
  cmd->location = (AstLocation){.line = 1, .column = 1};

  return cmd;
}

static void appendCommand(AstCodeBlock *block, AstCommand *cmd) {
  if (block->lastCommand == NULL)
    block->firstCommand = cmd;
  else
    block->lastCommand->nextCommand = cmd;

  block->lastCommand = cmd;
}

static void declareVar(AstCodeBlock *block, char *name, AstDataType type, bool mutable,
                       AstValue *value) {
  AstCommand *cmd = newCommand(VARIABLE_DECLARATION);

  cmd->data.variableDeclaration.variableName = name;
  cmd->data.variableDeclaration.isMutable = mutable;
  cmd->data.variableDeclaration.declaredType = type;
  cmd->data.variableDeclaration.resolvedType = type;
  cmd->data.variableDeclaration.value = value;

  appendCommand(block, cmd);
}

/// val name = expression
static void declareVal(AstCodeBlock *block, char *name, AstDataType type, AstExpression *value) {
  declareVar(block, name, type, false, exprValue(value));
}

/// val name = call
static void declareCall(AstCodeBlock *block, char *name, AstDataType type, AstFunctionCall *call) {
  declareVar(block, name, type, false, callValue(call));
}

static void assignVar(AstCodeBlock *block, char *name, AstValue *value) {
  AstCommand *cmd = newCommand(ASSIGNMENT);

  cmd->data.assignment.variableName = name;
  cmd->data.assignment.value = value;

  appendCommand(block, cmd);
}

static void callCommand(AstCodeBlock *block, AstFunctionCall *call) {
  AstCommand *cmd = newCommand(FUNCTION_CALL);

  cmd->data.call.functionCall = call;

  appendCommand(block, cmd);
}

static void expressionCommand(AstCodeBlock *block, AstExpression *expression) {
  AstCommand *cmd = newCommand(EXPRESSION);

  cmd->data.expressionAst.expression = expression;

  appendCommand(block, cmd);
}

static void returnValue(AstCodeBlock *block, AstValue *value) {
  AstCommand *cmd = newCommand(RETURN);

  cmd->data.returnAst.value = value;

  appendCommand(block, cmd);
}

static void returnExpr(AstCodeBlock *block, AstExpression *expression) {
  returnValue(block, exprValue(expression));
}

static void ifCommand(AstCodeBlock *block, AstExpression *cond, AstCodeBlock *thenBlock,
                      AstCodeBlock *elseBlock) {
  AstCommand *cmd = newCommand(IF);

  cmd->data.ifAst.condition = cond;
  cmd->data.ifAst.thenBlock = thenBlock;
  cmd->data.ifAst.elseBlock = elseBlock;

  appendCommand(block, cmd);
}

static void whileCommand(AstCodeBlock *block, AstExpression *cond, AstCodeBlock *body) {
  AstCommand *cmd = newCommand(WHILE);

  cmd->data.whileAst.condition = cond;
  cmd->data.whileAst.codeBlock = body;

  appendCommand(block, cmd);
}

// ============================================================
// Functions
// ============================================================

static AstFunction *newFunction(AstProgram *program, char *name, AstDataType returnType,
                                AstFunctionBodyType bodyType) {
  AstFunction *func = arenaZalloc(sizeof(*func));

  func->functionName = name;
  func->location = (AstLocation){.line = 1, .column = 1};
  func->declaredReturnType = returnType;
  func->resolvedReturnType = returnType;
  func->bodyType = bodyType;

  if (program->lastFunction == NULL)
    program->firstFunction = func;
  else
    program->lastFunction->nextFunction = func;

  program->lastFunction = func;

  return func;
}

/// def name(...): returnType = value
static AstFunction *valueFunction(AstProgram *program, char *name, AstDataType returnType,
                                  AstValue *value) {
  AstFunction *func = newFunction(program, name, returnType, VALUE);
  func->body.value = value;
  return func;
}

/// def name(...): returnType =
///   block
static AstFunction *blockFunction(AstProgram *program, char *name, AstDataType returnType) {
  AstFunction *func = newFunction(program, name, returnType, CODE_BLOCK);
  func->body.codeBlock = newBlock();
  return func;
}

static void addParameter(AstFunction *func, char *name, AstDataType type) {
  AstFunctionParameter *param = arenaZalloc(sizeof(*param));

  param->parameterName = name;
  param->dataType = type;

  if (func->lastParameter == NULL)
    func->firstParameter = param;
  else
    func->lastParameter->nextParameter = param;

  func->lastParameter = param;
}

/// @main def main(): Unit =
///   block
/// Most tests add this last, as the generated program ends right after it
static AstCodeBlock *addMain(AstProgram *program) {
  AstFunction *func = blockFunction(program, "main", UNIT);
  func->isMain = true;
  return func->body.codeBlock;
}

// ============================================================
// Printing helpers
// ============================================================

/// print(expression)
static void printExpr(AstCodeBlock *block, AstExpression *expression) {
  callCommand(block, CALL_OF("print", expression));
}

/// print("text")
static void printText(AstCodeBlock *block, char *text) {
  printExpr(block, stringExpr(text));
}

/// print(expression); print("\n")
static void printLine(AstCodeBlock *block, AstExpression *expression) {
  printExpr(block, expression);
  printText(block, "\n");
}

/// val name = toStr(expression); print(name); print("\n")
static void printAsString(AstCodeBlock *block, char *name, AstExpression *expression) {
  declareCall(block, name, STRING, CALL_OF("toStr", expression));
  printLine(block, varExpr(name, STRING));
}

/// if cond then print("T") else print("F")
static void printCondition(AstCodeBlock *block, AstExpression *cond) {
  AstCodeBlock *thenBlock = newBlock();
  AstCodeBlock *elseBlock = newBlock();

  printText(thenBlock, "T");
  printText(elseBlock, "F");

  ifCommand(block, cond, thenBlock, elseBlock);
}

/// Prints the outcome of <, <=, >, >=, ==, != in this order, followed by a
/// newline
static void printComparisons(AstCodeBlock *block, AstExpression *left, AstExpression *right) {
  const AstOperator operators[] = {LESSER, LESSER_EQUAL, GREATER, GREATER_EQUAL, EQUAL, NOT_EQUAL};

  for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); i++)
    printCondition(block, condition(operators[i], left, right));

  printText(block, "\n");
}

// ============================================================
// Shared functions
// ============================================================

/// def add(a: Int, b: Int): Int = a + b
static void addAddFunction(AstProgram *program) {
  AstFunction *func = valueFunction(program, "add", INT,
                                    exprValue(intOperation(ADDITION, intVar("a"), intVar("b"))));

  addParameter(func, "a", INT);
  addParameter(func, "b", INT);
}

/// def factorial(n: Int): Int =
///   if n <= 1 then
///     return 1
///   else
///     val previous = factorial(n - 1)
///     return n * previous
static void addFactorialFunction(AstProgram *program) {
  AstFunction *func = blockFunction(program, "factorial", INT);
  addParameter(func, "n", INT);

  AstCodeBlock *thenBlock = newBlock();
  AstCodeBlock *elseBlock = newBlock();

  returnExpr(thenBlock, intExpr(1));

  declareCall(elseBlock, "previous", INT,
              CALL_OF("factorial", intOperation(SUBTRACTION, intVar("n"), intExpr(1))));
  returnExpr(elseBlock, intOperation(MULTIPLICATION, intVar("n"), intVar("previous")));

  ifCommand(func->body.codeBlock, condition(LESSER_EQUAL, intVar("n"), intExpr(1)), thenBlock,
            elseBlock);
}

/// Runs a program which prints "before", calls subStr(word, start, end) and
/// prints "after"
static RunResult runInvalidSubStr(char *word, int32_t start, int32_t end) {
  AstProgram program = {0};
  AstCodeBlock *body = addMain(&program);

  printText(body, "before");
  declareCall(body, "piece", STRING,
              CALL_OF("subStr", stringExpr(word), intExpr(start), intExpr(end)));
  printText(body, "after");

  return runProgram(program, "");
}

tstsuite("Code generator", ) {
  // ============================================================
  // Values and variables
  // ============================================================

  tstcase("Interpreter is available", ) {
    tstcheck(access(INTERPRETER, X_OK) == 0, "run the tests from the root of the repository");
  }

  tstcase("Printing literals", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printLine(body, intExpr(42));
    printLine(body, intExpr(-42));
    printLine(body, doubleExpr(4.5));
    printLine(body, stringExpr("hello"));
    printLine(body, unitExpr());

    EXPECT_OUTPUT(program, "42\n-42\n0x1.2p+2\nhello\n()\n");
    arenaCleanup();
  }

  tstcase("Printing does not add separators", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printExpr(body, intExpr(1));
    printExpr(body, stringExpr("a"));
    printExpr(body, intExpr(2));

    EXPECT_OUTPUT(program, "1a2");
    arenaCleanup();
  }

  tstcase("String literals are escaped", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);
    char *text = "space tab\t hash# backslash\\ newline\n 'quote\" @at 035 \\035";

    printExpr(body, stringExpr(text));

    EXPECT_OUTPUT(program, text);
    arenaCleanup();
  }

  tstcase("Empty string literal", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printText(body, "[");
    printExpr(body, stringExpr(""));
    printText(body, "]");

    EXPECT_OUTPUT(program, "[]");
    arenaCleanup();
  }

  tstcase("Variables of every type", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "i", INT, intExpr(7));
    declareVal(body, "d", DOUBLE, doubleExpr(0.5));
    declareVal(body, "s", STRING, stringExpr("text"));
    declareVal(body, "u", UNIT, unitExpr());

    printLine(body, intVar("i"));
    printLine(body, varExpr("d", DOUBLE));
    printLine(body, varExpr("s", STRING));
    printLine(body, varExpr("u", UNIT));

    EXPECT_OUTPUT(program, "7\n0x1p-1\ntext\n()\n");
    arenaCleanup();
  }

  tstcase("Variables copy their value", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVar(body, "a", INT, true, exprValue(intExpr(1)));
    declareVal(body, "b", INT, intVar("a"));
    assignVar(body, "a", exprValue(intExpr(2)));

    printLine(body, intVar("a"));
    printLine(body, intVar("b"));

    EXPECT_OUTPUT(program, "2\n1\n");
    arenaCleanup();
  }

  tstcase("Assignment using the old value", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVar(body, "x", INT, true, exprValue(intExpr(3)));
    assignVar(
        body, "x",
        exprValue(intOperation(MULTIPLICATION, intOperation(ADDITION, intVar("x"), intExpr(1)),
                               intOperation(ADDITION, intVar("x"), intExpr(2)))));
    printLine(body, intVar("x"));

    EXPECT_OUTPUT(program, "20\n");
    arenaCleanup();
  }

  tstcase("Function with many local variables", ) {
    enum { COUNT = 100 };
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVar(body, "total", INT, true, exprValue(intExpr(0)));

    for (int i = 0; i < COUNT; i++) {
      char *name = arenaAlloc(16);
      snprintf(name, 16, "v%d", i);

      declareVal(body, name, INT, intExpr(i));
      assignVar(body, "total", exprValue(intOperation(ADDITION, intVar("total"), intVar(name))));
    }

    printLine(body, intVar("total"));

    EXPECT_OUTPUT(program, "4950\n");
    arenaCleanup();
  }

  tstcase("Function with many temporaries", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    AstExpression *sum = intExpr(0);
    for (int i = 1; i <= 100; i++) sum = intOperation(ADDITION, sum, intExpr(i));

    printLine(body, sum);

    EXPECT_OUTPUT(program, "5050\n");
    arenaCleanup();
  }

  // ============================================================
  // Arithmetic
  // ============================================================

  tstcase("Integer arithmetic", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "a", INT, intExpr(7));
    declareVal(body, "b", INT, intExpr(5));

    printLine(body, intOperation(ADDITION, intVar("a"), intVar("b")));
    printLine(body, intOperation(SUBTRACTION, intVar("b"), intVar("a")));
    printLine(body, intOperation(MULTIPLICATION, intVar("a"), intVar("b")));

    EXPECT_OUTPUT(program, "12\n-2\n35\n");
    arenaCleanup();
  }

  tstcase("Nested arithmetic", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    // ((a + b) * 2) - (a - (b - 1)) = 21
    declareVal(body, "a", INT, intExpr(7));
    declareVal(body, "b", INT, intExpr(5));
    declareVal(
        body, "result", INT,
        intOperation(SUBTRACTION,
                     intOperation(MULTIPLICATION, intOperation(ADDITION, intVar("a"), intVar("b")),
                                  intExpr(2)),
                     intOperation(SUBTRACTION, intVar("a"),
                                  intOperation(SUBTRACTION, intVar("b"), intExpr(1)))));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "21\n");
    arenaCleanup();
  }

  tstcase("Integer division rounds towards zero", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printLine(body, intOperation(DIVISION, intExpr(17), intExpr(5)));
    printLine(body, intOperation(DIVISION, intExpr(-7), intExpr(2)));
    printLine(body, intOperation(DIVISION, intExpr(7), intExpr(-2)));

    EXPECT_OUTPUT(program, "3\n-3\n-3\n");
    arenaCleanup();
  }

  tstcase("Integer division by zero", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "zero", INT, intExpr(0));
    printText(body, "before");
    printLine(body, intOperation(DIVISION, intExpr(1), intVar("zero")));
    printText(body, "after");

    EXPECT_RUN(program, "", 57, "before");
    arenaCleanup();
  }

  tstcase("Integer negation", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "a", INT, intExpr(7));
    declareVal(body, "negative", INT, negate(intVar("a"), INT));

    printLine(body, intVar("negative"));
    printLine(body, negate(intVar("negative"), INT));

    EXPECT_OUTPUT(program, "-7\n7\n");
    arenaCleanup();
  }

  tstcase("Int overflow wraps around", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "max", INT, intExpr(INT32_MAX));
    printLine(body, intOperation(ADDITION, intVar("max"), intExpr(1)));
    printLine(body, intOperation(MULTIPLICATION, intVar("max"), intExpr(2)));

    EXPECT_OUTPUT(program, "-2147483648\n-2\n");
    arenaCleanup();
  }

  tstcase("Int overflow at every operator", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "min", INT, intExpr(INT32_MIN));
    declareVal(body, "max", INT, intExpr(INT32_MAX));

    printLine(body, intOperation(SUBTRACTION, intVar("min"), intExpr(1)));
    printLine(body, intOperation(ADDITION, intVar("max"), intVar("max")));
    printLine(body, intOperation(MULTIPLICATION, intVar("min"), intVar("min")));
    printLine(body, intOperation(MULTIPLICATION, intVar("min"), intVar("max")));
    printLine(body, intOperation(MULTIPLICATION, intExpr(65536), intExpr(65536)));
    printLine(body, intOperation(MULTIPLICATION, intExpr(1000000), intExpr(1000000)));
    printLine(body, intOperation(DIVISION, intVar("min"), intExpr(-1)));
    printLine(body, negate(intVar("min"), INT));

    EXPECT_OUTPUT(program,
                  "2147483647\n-2\n0\n-2147483648\n0\n-727379968\n-"
                  "2147483648\n-2147483648\n");
    arenaCleanup();
  }

  tstcase("Int overflow inside calls and nested expressions", ) {
    AstProgram program = {0};
    addAddFunction(&program);

    AstCodeBlock *body = addMain(&program);
    declareVal(body, "max", INT, intExpr(INT32_MAX));
    declareCall(body, "called", INT, CALL_OF("add", intVar("max"), intExpr(1)));
    declareVal(
        body, "nested", INT,
        intOperation(DIVISION, intOperation(ADDITION, intVar("max"), intExpr(1)), intExpr(2)));
    printLine(body, intVar("called"));
    printLine(body, intVar("nested"));
    printAsString(body, "text", intOperation(ADDITION, intVar("max"), intExpr(2)));

    EXPECT_OUTPUT(program, "-2147483648\n-1073741824\n-2147483647\n");
    arenaCleanup();
  }

  tstcase("readInt gives 0 outside of Int range", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "a", INT, callNoArgs("readInt"));
    declareCall(body, "b", INT, callNoArgs("readInt"));
    declareCall(body, "c", INT, callNoArgs("readInt"));
    declareCall(body, "d", INT, callNoArgs("readInt"));
    printLine(body, intVar("a"));
    printLine(body, intVar("b"));
    printLine(body, intVar("c"));
    printLine(body, intVar("d"));

    EXPECT_RUN(program, "2147483647\n-2147483648\n2147483648\n-2147483649\n", 0,
               "2147483647\n-2147483648\n0\n0\n");
    arenaCleanup();
  }

  tstcase("Double arithmetic", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "ratio", DOUBLE,
               binaryExpr(DIVISION, doubleExpr(9.0), doubleExpr(2.0), DOUBLE));
    printLine(body, varExpr("ratio", DOUBLE));

    printAsString(body, "sum", binaryExpr(ADDITION, doubleExpr(1.5), doubleExpr(2.25), DOUBLE));
    printAsString(body, "difference",
                  binaryExpr(SUBTRACTION, doubleExpr(1.5), doubleExpr(2.25), DOUBLE));
    printAsString(body, "product",
                  binaryExpr(MULTIPLICATION, doubleExpr(1.5), doubleExpr(2.0), DOUBLE));

    EXPECT_OUTPUT(program, "0x1.2p+2\n3.75\n-0.75\n3.00\n");
    arenaCleanup();
  }

  tstcase("Double negation", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "d", DOUBLE, doubleExpr(1.5));
    printAsString(body, "negative", negate(varExpr("d", DOUBLE), DOUBLE));

    EXPECT_OUTPUT(program, "-1.50\n");
    arenaCleanup();
  }

  tstcase("String concatenation", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVar(body, "s", STRING, true, exprValue(stringExpr("ab")));
    assignVar(body, "s",
              exprValue(binaryExpr(ADDITION, varExpr("s", STRING), stringExpr(" cd"), STRING)));
    assignVar(body, "s",
              exprValue(binaryExpr(ADDITION, varExpr("s", STRING), varExpr("s", STRING), STRING)));
    printLine(body, varExpr("s", STRING));
    printLine(body, binaryExpr(ADDITION, stringExpr(""), stringExpr(""), STRING));

    EXPECT_OUTPUT(program, "ab cdab cd\n\n");
    arenaCleanup();
  }

  // ============================================================
  // Comparisons
  // ============================================================

  tstcase("Comparing integers", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printComparisons(body, intExpr(5), intExpr(7));
    printComparisons(body, intExpr(7), intExpr(5));
    printComparisons(body, intExpr(5), intExpr(5));

    EXPECT_OUTPUT(program, "TTFFFT\nFFTTFT\nFTFTTF\n");
    arenaCleanup();
  }

  tstcase("Comparing doubles", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printComparisons(body, doubleExpr(0.5), doubleExpr(1.5));
    printComparisons(body, doubleExpr(1.5), doubleExpr(0.5));
    printComparisons(body, doubleExpr(0.5), doubleExpr(0.5));

    EXPECT_OUTPUT(program, "TTFFFT\nFFTTFT\nFTFTTF\n");
    arenaCleanup();
  }

  tstcase("Comparing strings", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printComparisons(body, stringExpr("abc"), stringExpr("abd"));
    printComparisons(body, stringExpr("abc"), stringExpr("ab"));
    printComparisons(body, stringExpr("abc"), stringExpr("abc"));

    EXPECT_OUTPUT(program, "TTFFFT\nFFTTFT\nFTFTTF\n");
    arenaCleanup();
  }

  tstcase("Comparing Unit values", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printCondition(body, condition(EQUAL, unitExpr(), unitExpr()));
    printCondition(body, condition(NOT_EQUAL, unitExpr(), unitExpr()));

    EXPECT_OUTPUT(program, "TF");
    arenaCleanup();
  }

  tstcase("Storing comparison results", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "less", BOOL, condition(LESSER, intExpr(5), intExpr(7)));
    declareVal(body, "lessEqual", BOOL, condition(LESSER_EQUAL, intExpr(7), intExpr(5)));
    declareVal(body, "notEqual", BOOL, condition(NOT_EQUAL, intExpr(5), intExpr(7)));

    printLine(body, varExpr("less", BOOL));
    printLine(body, varExpr("lessEqual", BOOL));
    printAsString(body, "text", varExpr("notEqual", BOOL));
    printCondition(body, varExpr("less", BOOL));

    EXPECT_OUTPUT(program, "true\nfalse\ntrue\nT");
    arenaCleanup();
  }

  // ============================================================
  // Control flow
  // ============================================================

  tstcase("If and else", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printCondition(body, condition(GREATER, intExpr(7), intExpr(5)));
    printCondition(body, condition(GREATER, intExpr(5), intExpr(7)));

    EXPECT_OUTPUT(program, "TF");
    arenaCleanup();
  }

  tstcase("If with an empty branch", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    AstCodeBlock *thenBlock = newBlock();
    AstCodeBlock *elseBlock = newBlock();
    printText(thenBlock, "then");
    expressionCommand(elseBlock, unitExpr());

    ifCommand(body, condition(EQUAL, intExpr(1), intExpr(1)), thenBlock, elseBlock);
    ifCommand(body, condition(EQUAL, intExpr(1), intExpr(2)), thenBlock, elseBlock);
    printText(body, " end");

    EXPECT_OUTPUT(program, "then end");
    arenaCleanup();
  }

  tstcase("Nested if", ) {
    AstProgram program = {0};
    AstFunction *classify = blockFunction(&program, "classify", UNIT);
    addParameter(classify, "x", INT);

    // if x < 0 then "negative" else (if x == 0 then "zero" else "positive")
    AstCodeBlock *negative = newBlock();
    AstCodeBlock *notNegative = newBlock();
    AstCodeBlock *zero = newBlock();
    AstCodeBlock *positive = newBlock();

    printText(negative, "negative\n");
    printText(zero, "zero\n");
    printText(positive, "positive\n");
    ifCommand(notNegative, condition(EQUAL, intVar("x"), intExpr(0)), zero, positive);

    ifCommand(classify->body.codeBlock, condition(LESSER, intVar("x"), intExpr(0)), negative,
              notNegative);
    returnExpr(classify->body.codeBlock, unitExpr());

    AstCodeBlock *body = addMain(&program);
    callCommand(body, CALL_OF("classify", intExpr(-1)));
    callCommand(body, CALL_OF("classify", intExpr(0)));
    callCommand(body, CALL_OF("classify", intExpr(1)));

    EXPECT_OUTPUT(program, "negative\nzero\npositive\n");
    arenaCleanup();
  }

  tstcase("While loop", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVar(body, "counter", INT, true, exprValue(intExpr(0)));

    AstCodeBlock *loop = newBlock();
    printLine(loop, intVar("counter"));
    assignVar(loop, "counter", exprValue(intOperation(ADDITION, intVar("counter"), intExpr(1))));

    whileCommand(body, condition(LESSER, intVar("counter"), intExpr(3)), loop);

    EXPECT_OUTPUT(program, "0\n1\n2\n");
    arenaCleanup();
  }

  tstcase("While loop that never runs", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    AstCodeBlock *loop = newBlock();
    printText(loop, "unreachable");
    whileCommand(body, condition(LESSER, intExpr(1), intExpr(0)), loop);
    printText(body, "done");

    EXPECT_OUTPUT(program, "done");
    arenaCleanup();
  }

  tstcase("Nested while loops with declarations in their bodies", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    // var i = 0
    // while i < 2 do
    //   var j = 0
    //   while j < 2 do
    //     val product = (i + 1) * (j + 1)
    //     print(product)
    //     j = j + 1
    //   i = i + 1
    declareVar(body, "i", INT, true, exprValue(intExpr(0)));

    AstCodeBlock *outer = newBlock();
    AstCodeBlock *inner = newBlock();

    declareVal(inner, "product", INT,
               intOperation(MULTIPLICATION, intOperation(ADDITION, intVar("i"), intExpr(1)),
                            intOperation(ADDITION, intVar("j"), intExpr(1))));
    printExpr(inner, intVar("product"));
    assignVar(inner, "j", exprValue(intOperation(ADDITION, intVar("j"), intExpr(1))));

    declareVar(outer, "j", INT, true, exprValue(intExpr(0)));
    whileCommand(outer, condition(LESSER, intVar("j"), intExpr(2)), inner);
    assignVar(outer, "i", exprValue(intOperation(ADDITION, intVar("i"), intExpr(1))));

    whileCommand(body, condition(LESSER, intVar("i"), intExpr(2)), outer);

    EXPECT_OUTPUT(program, "1224");
    arenaCleanup();
  }

  // ============================================================
  // Functions
  // ============================================================

  tstcase("Function with a value body", ) {
    AstProgram program = {0};
    addAddFunction(&program);

    AstCodeBlock *body = addMain(&program);
    declareVal(body, "a", INT, intExpr(7));
    declareCall(body, "result", INT, CALL_OF("add", intVar("a"), intExpr(5)));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "12\n");
    arenaCleanup();
  }

  tstcase("Function with a nested expression as its value body", ) {
    AstProgram program = {0};

    // def twicePlusOne(x: Int): Int = (x * 2) + 1
    AstFunction *func = valueFunction(
        &program, "twicePlusOne", INT,
        exprValue(intOperation(ADDITION, intOperation(MULTIPLICATION, intVar("x"), intExpr(2)),
                               intExpr(1))));
    addParameter(func, "x", INT);

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "result", INT, CALL_OF("twicePlusOne", intExpr(5)));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "11\n");
    arenaCleanup();
  }

  tstcase("Function with a call as its value body", ) {
    AstProgram program = {0};
    addAddFunction(&program);

    // def increment(x: Int): Int = add(x, 1)
    AstFunction *increment = valueFunction(&program, "increment", INT,
                                           callValue(CALL_OF("add", intVar("x"), intExpr(1))));
    addParameter(increment, "x", INT);

    // def length(s: String): Int = strLength(s)
    AstFunction *length = valueFunction(&program, "length", INT,
                                        callValue(CALL_OF("strLength", varExpr("s", STRING))));
    addParameter(length, "s", STRING);

    // def greet(): Unit = print("hi\n")
    valueFunction(&program, "greet", UNIT, callValue(CALL_OF("print", stringExpr("hi\n"))));

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "next", INT, CALL_OF("increment", intExpr(41)));
    declareCall(body, "size", INT, CALL_OF("length", stringExpr("abc")));
    callCommand(body, callNoArgs("greet"));
    printLine(body, intVar("next"));
    printLine(body, intVar("size"));

    EXPECT_OUTPUT(program, "hi\n42\n3\n");
    arenaCleanup();
  }

  tstcase("Function with subStr as its value body", ) {
    AstProgram program = {0};

    // def first(s: String): String = subStr(s, 0, 1)
    AstFunction *first =
        valueFunction(&program, "first", STRING,
                      callValue(CALL_OF("subStr", varExpr("s", STRING), intExpr(0), intExpr(1))));
    addParameter(first, "s", STRING);

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "letter", STRING, CALL_OF("first", stringExpr("abc")));
    printLine(body, varExpr("letter", STRING));

    EXPECT_OUTPUT(program, "a\n");
    arenaCleanup();
  }

  tstcase("Arguments keep their order and type", ) {
    AstProgram program = {0};

    // def describe(label: String, a: Int, b: Double, c: Unit): Int =
    //   print(label); print(a); print(b); print(c)
    //   return a - 1
    AstFunction *func = blockFunction(&program, "describe", INT);
    addParameter(func, "label", STRING);
    addParameter(func, "a", INT);
    addParameter(func, "b", DOUBLE);
    addParameter(func, "c", UNIT);

    AstCodeBlock *block = func->body.codeBlock;
    printLine(block, varExpr("label", STRING));
    printLine(block, intVar("a"));
    printLine(block, varExpr("b", DOUBLE));
    printLine(block, varExpr("c", UNIT));
    returnExpr(block, intOperation(SUBTRACTION, intVar("a"), intExpr(1)));

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "result", INT,
                CALL_OF("describe", stringExpr("x"), intExpr(5), doubleExpr(0.5), unitExpr()));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "x\n5\n0x1p-1\n()\n4\n");
    arenaCleanup();
  }

  tstcase("Expressions as arguments", ) {
    AstProgram program = {0};
    addAddFunction(&program);

    AstCodeBlock *body = addMain(&program);
    declareVal(body, "a", INT, intExpr(2));
    declareCall(body, "result", INT,
                CALL_OF("add", intOperation(MULTIPLICATION, intVar("a"), intExpr(3)),
                        intOperation(ADDITION, intOperation(ADDITION, intVar("a"), intVar("a")),
                                     intExpr(1))));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "11\n");
    arenaCleanup();
  }

  tstcase("Assigning the result of a call", ) {
    AstProgram program = {0};
    addAddFunction(&program);

    AstCodeBlock *body = addMain(&program);
    declareVar(body, "x", INT, true, exprValue(intExpr(1)));

    AstCodeBlock *loop = newBlock();
    assignVar(loop, "x", callValue(CALL_OF("add", intVar("x"), intVar("x"))));

    whileCommand(body, condition(LESSER, intVar("x"), intExpr(100)), loop);
    printLine(body, intVar("x"));

    EXPECT_OUTPUT(program, "128\n");
    arenaCleanup();
  }

  tstcase("Discarding the result of a call", ) {
    AstProgram program = {0};
    addAddFunction(&program);

    AstCodeBlock *body = addMain(&program);
    callCommand(body, CALL_OF("add", intExpr(1), intExpr(2)));
    printText(body, "done");

    EXPECT_OUTPUT(program, "done");
    arenaCleanup();
  }

  tstcase("Function with a loop", ) {
    AstProgram program = {0};

    // def sumTo(n: Int): Int =
    //   var i = 0
    //   var total = 0
    //   while i < n do
    //     total = total + i
    //     i = i + 1
    //   return total
    AstFunction *func = blockFunction(&program, "sumTo", INT);
    addParameter(func, "n", INT);

    AstCodeBlock *block = func->body.codeBlock;
    declareVar(block, "i", INT, true, exprValue(intExpr(0)));
    declareVar(block, "total", INT, true, exprValue(intExpr(0)));

    AstCodeBlock *loop = newBlock();
    assignVar(loop, "total", exprValue(intOperation(ADDITION, intVar("total"), intVar("i"))));
    assignVar(loop, "i", exprValue(intOperation(ADDITION, intVar("i"), intExpr(1))));

    whileCommand(block, condition(LESSER, intVar("i"), intVar("n")), loop);
    returnExpr(block, intVar("total"));

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "sum", INT, CALL_OF("sumTo", intExpr(5)));
    printLine(body, intVar("sum"));

    EXPECT_OUTPUT(program, "10\n");
    arenaCleanup();
  }

  tstcase("Recursion", ) {
    AstProgram program = {0};
    addFactorialFunction(&program);

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "fact", INT, CALL_OF("factorial", intExpr(5)));
    printLine(body, intVar("fact"));

    EXPECT_OUTPUT(program, "120\n");
    arenaCleanup();
  }

  tstcase("Recursion keeps local variables of each call", ) {
    AstProgram program = {0};

    // def fib(n: Int): Int =
    //   if n < 2 then
    //     return n
    //   else
    //     val a = fib(n - 1)
    //     val b = fib(n - 2)
    //     return a + b
    AstFunction *func = blockFunction(&program, "fib", INT);
    addParameter(func, "n", INT);

    AstCodeBlock *thenBlock = newBlock();
    AstCodeBlock *elseBlock = newBlock();

    returnExpr(thenBlock, intVar("n"));

    declareCall(elseBlock, "a", INT,
                CALL_OF("fib", intOperation(SUBTRACTION, intVar("n"), intExpr(1))));
    declareCall(elseBlock, "b", INT,
                CALL_OF("fib", intOperation(SUBTRACTION, intVar("n"), intExpr(2))));
    returnExpr(elseBlock, intOperation(ADDITION, intVar("a"), intVar("b")));

    ifCommand(func->body.codeBlock, condition(LESSER, intVar("n"), intExpr(2)), thenBlock,
              elseBlock);

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "result", INT, CALL_OF("fib", intExpr(10)));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "55\n");
    arenaCleanup();
  }

  tstcase("Mutual recursion and calling functions before their definition", ) {
    AstProgram program = {0};

    // main comes first, the explicit return keeps it from running into isEven
    AstCodeBlock *body = addMain(&program);
    declareCall(body, "even", INT, CALL_OF("isEven", intExpr(7)));
    printLine(body, intVar("even"));
    returnExpr(body, unitExpr());

    AstFunction *functions[2] = {blockFunction(&program, "isEven", INT),
                                 blockFunction(&program, "isOdd", INT)};
    char *other[2] = {"isOdd", "isEven"};

    // def isEven(n: Int): Int =          def isOdd(n: Int): Int =
    //   if n == 0 then                     if n == 0 then
    //     return 1                           return 0
    //   else                               else
    //     return isOdd(n - 1)                return isEven(n - 1)
    for (int i = 0; i < 2; i++) {
      addParameter(functions[i], "n", INT);

      AstCodeBlock *thenBlock = newBlock();
      AstCodeBlock *elseBlock = newBlock();

      returnExpr(thenBlock, intExpr(i == 0 ? 1 : 0));
      returnValue(elseBlock,
                  callValue(CALL_OF(other[i], intOperation(SUBTRACTION, intVar("n"), intExpr(1)))));

      ifCommand(functions[i]->body.codeBlock, condition(EQUAL, intVar("n"), intExpr(0)), thenBlock,
                elseBlock);
    }

    EXPECT_OUTPUT(program, "0\n");
    arenaCleanup();
  }

  tstcase("Callee has its own variables", ) {
    AstProgram program = {0};

    // def clobber(): Int =
    //   var x = 100
    //   x = x + 1
    //   return x
    AstFunction *func = blockFunction(&program, "clobber", INT);
    declareVar(func->body.codeBlock, "x", INT, true, exprValue(intExpr(100)));
    assignVar(func->body.codeBlock, "x",
              exprValue(intOperation(ADDITION, intVar("x"), intExpr(1))));
    returnExpr(func->body.codeBlock, intVar("x"));

    AstCodeBlock *body = addMain(&program);
    declareVal(body, "x", INT, intExpr(1));
    declareCall(body, "y", INT, callNoArgs("clobber"));
    printLine(body, intVar("x"));
    printLine(body, intVar("y"));

    EXPECT_OUTPUT(program, "1\n101\n");
    arenaCleanup();
  }

  tstcase("Return leaves loops and branches", ) {
    AstProgram program = {0};

    // def firstOver(limit: Int): Int =
    //   var i = 0
    //   while i < 100 do
    //     if i * i > limit then
    //       return i
    //     else
    //       print("x")
    //     i = i + 1
    //   return 0 - 1
    AstFunction *func = blockFunction(&program, "firstOver", INT);
    addParameter(func, "limit", INT);

    AstCodeBlock *block = func->body.codeBlock;
    declareVar(block, "i", INT, true, exprValue(intExpr(0)));

    AstCodeBlock *loop = newBlock();
    AstCodeBlock *thenBlock = newBlock();
    AstCodeBlock *elseBlock = newBlock();

    returnExpr(thenBlock, intVar("i"));
    printText(elseBlock, "x");

    ifCommand(
        loop,
        condition(GREATER, intOperation(MULTIPLICATION, intVar("i"), intVar("i")), intVar("limit")),
        thenBlock, elseBlock);
    assignVar(loop, "i", exprValue(intOperation(ADDITION, intVar("i"), intExpr(1))));

    whileCommand(block, condition(LESSER, intVar("i"), intExpr(100)), loop);
    returnExpr(block, intOperation(SUBTRACTION, intExpr(0), intExpr(1)));

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "result", INT, CALL_OF("firstOver", intExpr(10)));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "xxxx4\n");
    arenaCleanup();
  }

  tstcase("Returning a call", ) {
    AstProgram program = {0};
    addFactorialFunction(&program);

    // def factorialOfThree(): Int =
    //   return factorial(3)
    AstFunction *func = blockFunction(&program, "factorialOfThree", INT);
    returnValue(func->body.codeBlock, callValue(CALL_OF("factorial", intExpr(3))));

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "result", INT, callNoArgs("factorialOfThree"));
    printLine(body, intVar("result"));

    EXPECT_OUTPUT(program, "6\n");
    arenaCleanup();
  }

  tstcase("Main can have any name", ) {
    AstProgram program = {0};

    AstFunction *helper = blockFunction(&program, "helper", UNIT);
    printText(helper->body.codeBlock, "wrong function");
    returnExpr(helper->body.codeBlock, unitExpr());

    AstFunction *entry = blockFunction(&program, "runFoo", UNIT);
    entry->isMain = true;
    printText(entry->body.codeBlock, "entry");
    returnExpr(entry->body.codeBlock, unitExpr());

    EXPECT_OUTPUT(program, "entry");
    arenaCleanup();
  }

  // ============================================================
  // Builtin functions
  // ============================================================

  tstcase("toStr", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printAsString(body, "fromInt", intExpr(-12));
    printAsString(body, "fromDouble", doubleExpr(4.5));
    printAsString(body, "fromString", stringExpr("text"));
    printAsString(body, "fromUnit", unitExpr());

    EXPECT_OUTPUT(program, "-12\n4.50\ntext\n()\n");
    arenaCleanup();
  }

  tstcase("toStr of variables", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "i", INT, intExpr(1));
    declareVal(body, "d", DOUBLE, doubleExpr(0.25));
    declareVal(body, "s", STRING, stringExpr("text"));
    declareVal(body, "u", UNIT, unitExpr());

    printAsString(body, "fromInt", intVar("i"));
    printAsString(body, "fromDouble", varExpr("d", DOUBLE));
    printAsString(body, "fromString", varExpr("s", STRING));
    printAsString(body, "fromUnit", varExpr("u", UNIT));

    EXPECT_OUTPUT(program, "1\n0.25\ntext\n()\n");
    arenaCleanup();
  }

  tstcase("toInt truncates", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "positive", INT, CALL_OF("toInt", doubleExpr(4.9)));
    declareCall(body, "negative", INT, CALL_OF("toInt", doubleExpr(-4.9)));
    printLine(body, intVar("positive"));
    printLine(body, intVar("negative"));

    EXPECT_OUTPUT(program, "4\n-4\n");
    arenaCleanup();
  }

  tstcase("strLength", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "empty", INT, CALL_OF("strLength", stringExpr("")));
    declareCall(body, "escaped", INT, CALL_OF("strLength", stringExpr("a b\n#")));
    printLine(body, intVar("empty"));
    printLine(body, intVar("escaped"));

    EXPECT_OUTPUT(program, "0\n5\n");
    arenaCleanup();
  }

  tstcase("subStr", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "word", STRING, stringExpr("compiler"));
    AstExpression *word = varExpr("word", STRING);

    declareCall(body, "middle", STRING, CALL_OF("subStr", word, intExpr(1), intExpr(4)));
    declareCall(body, "start", STRING, CALL_OF("subStr", word, intExpr(0), intExpr(2)));
    declareCall(body, "end", STRING, CALL_OF("subStr", word, intExpr(6), intExpr(8)));
    declareCall(body, "whole", STRING, CALL_OF("subStr", word, intExpr(0), intExpr(8)));
    declareCall(body, "empty", STRING, CALL_OF("subStr", word, intExpr(3), intExpr(3)));

    printLine(body, varExpr("middle", STRING));
    printLine(body, varExpr("start", STRING));
    printLine(body, varExpr("end", STRING));
    printLine(body, varExpr("whole", STRING));
    printText(body, "[");
    printExpr(body, varExpr("empty", STRING));
    printText(body, "]");

    EXPECT_OUTPUT(program, "omp\nco\ner\ncompiler\n[]");
    arenaCleanup();
  }

  tstcase("subStr with variable bounds", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "from", INT, intExpr(1));
    declareVal(body, "to", INT, intExpr(3));
    declareCall(body, "piece", STRING,
                CALL_OF("subStr", stringExpr("abcd"), intVar("from"), intVar("to")));
    printLine(body, varExpr("piece", STRING));

    EXPECT_OUTPUT(program, "bc\n");
    arenaCleanup();
  }

  tstcase("subStr assigned back to its argument", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVar(body, "alias", STRING, true, exprValue(stringExpr("abcdef")));
    assignVar(body, "alias",
              callValue(CALL_OF("subStr", varExpr("alias", STRING), intExpr(1), intExpr(4))));
    printLine(body, varExpr("alias", STRING));

    assignVar(body, "alias",
              callValue(CALL_OF("subStr", varExpr("alias", STRING), intExpr(0), intExpr(3))));
    printLine(body, varExpr("alias", STRING));

    EXPECT_OUTPUT(program, "bcd\nbcd\n");
    arenaCleanup();
  }

  tstcase("subStr in a loop", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    // Reverses the string one character at a time
    declareVal(body, "word", STRING, stringExpr("abc"));
    declareVar(body, "i", INT, true, exprValue(intExpr(3)));
    declareVar(body, "reversed", STRING, true, exprValue(stringExpr("")));

    AstCodeBlock *loop = newBlock();
    assignVar(loop, "i", exprValue(intOperation(SUBTRACTION, intVar("i"), intExpr(1))));
    declareCall(loop, "letter", STRING,
                CALL_OF("subStr", varExpr("word", STRING), intVar("i"),
                        intOperation(ADDITION, intVar("i"), intExpr(1))));
    assignVar(loop, "reversed",
              exprValue(binaryExpr(ADDITION, varExpr("reversed", STRING), varExpr("letter", STRING),
                                   STRING)));

    whileCommand(body, condition(GREATER, intVar("i"), intExpr(0)), loop);
    printLine(body, varExpr("reversed", STRING));

    EXPECT_OUTPUT(program, "cba\n");
    arenaCleanup();
  }

  tstcase("subStr with invalid bounds exits with 40", ) {
    const struct {
      char *word;
      int32_t start;
      int32_t end;
    } invalid[] = {
        {"abc", -1, 2},  // start < 0
        {"abc", 0, -1},  // end < 0
        {"abc", 2, 1},   // start > end
        {"abc", 3, 3},   // start >= strLength(value)
        {"abc", 4, 5},   // start >= strLength(value)
        {"abc", 1, 4},   // end > strLength(value)
        {"", 0, 0},      // start >= strLength(value)
    };

    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
      RunResult run = runInvalidSubStr(invalid[i].word, invalid[i].start, invalid[i].end);

      tstcheck(run.exitCode == 40, "subStr(\"%s\", %d, %d) should exit with 40, got %d",
               invalid[i].word, invalid[i].start, invalid[i].end, run.exitCode);
      tstcheck(strcmp(run.out, "before") == 0, "program should stop at subStr, printed: %s",
               run.out);
    }

    arenaCleanup();
  }

  tstcase("readLine", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "first", STRING, callNoArgs("readLine"));
    declareCall(body, "second", STRING, callNoArgs("readLine"));
    declareCall(body, "third", STRING, callNoArgs("readLine"));
    declareCall(body, "firstLength", INT, CALL_OF("strLength", varExpr("first", STRING)));

    printLine(body, intVar("firstLength"));
    printText(body, "[");
    printExpr(body, varExpr("second", STRING));
    printExpr(body, varExpr("third", STRING));
    printText(body, "]");

    // Includes the line ending, EOF gives an empty string
    EXPECT_RUN(program, "abc\nlast", 0, "4\n[last]");
    arenaCleanup();
  }

  tstcase("readInt", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "x", INT, callNoArgs("readInt"));
    declareCall(body, "y", INT, callNoArgs("readInt"));
    printLine(body, intOperation(ADDITION, intVar("x"), intVar("y")));

    EXPECT_RUN(program, "40\n2\n", 0, "42\n");
    arenaCleanup();
  }

  tstcase("readDouble", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "x", DOUBLE, callNoArgs("readDouble"));
    printAsString(body, "text", varExpr("x", DOUBLE));

    EXPECT_RUN(program, "1.25\n", 0, "1.25\n");
    arenaCleanup();
  }

  tstcase("readInt and readDouble give 0 on invalid input and EOF", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "invalidInt", INT, callNoArgs("readInt"));
    declareCall(body, "invalidDouble", DOUBLE, callNoArgs("readDouble"));
    declareCall(body, "eofInt", INT, callNoArgs("readInt"));

    printLine(body, intOperation(ADDITION, intVar("invalidInt"), intExpr(1)));
    printAsString(body, "text", varExpr("invalidDouble", DOUBLE));
    printLine(body, intVar("eofInt"));

    EXPECT_RUN(program, "abc\nxyz\n", 0, "1\n0.00\n0\n");
    arenaCleanup();
  }

  tstcase("ord", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "code", INT, CALL_OF("ord", stringExpr("AB"), intExpr(1)));
    declareCall(body, "outOfRange", INT, CALL_OF("ord", stringExpr("AB"), intExpr(2)));
    declareCall(body, "negative", INT, CALL_OF("ord", stringExpr("AB"), intExpr(-1)));
    declareCall(body, "empty", INT, CALL_OF("ord", stringExpr(""), intExpr(0)));

    printLine(body, intVar("code"));
    printLine(body, intVar("outOfRange"));
    printLine(body, intVar("negative"));
    printLine(body, intVar("empty"));

    EXPECT_OUTPUT(program, "66\n-1\n-1\n-1\n");
    arenaCleanup();
  }

  tstcase("chr", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareCall(body, "letter", STRING, CALL_OF("chr", intExpr(65)));
    printLine(body, varExpr("letter", STRING));

    EXPECT_OUTPUT(program, "A\n");
    arenaCleanup();
  }

  tstcase("chr with an invalid code", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printText(body, "before");
    declareCall(body, "letter", STRING, CALL_OF("chr", intExpr(300)));

    EXPECT_RUN(program, "", 58, "before");
    arenaCleanup();
  }

  // ============================================================
  // Int to Double conversion
  // ============================================================

  tstcase("Int literal mixed with a Double in arithmetic", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    printAsString(body, "leftInt", binaryExpr(ADDITION, intExpr(1), doubleExpr(2.5), DOUBLE));
    printAsString(body, "rightInt",
                  binaryExpr(MULTIPLICATION, doubleExpr(2.5), intExpr(2), DOUBLE));
    printAsString(body, "subtract", binaryExpr(SUBTRACTION, intExpr(10), doubleExpr(0.5), DOUBLE));

    // Float division, IDIV would give 3
    printAsString(body, "divide", binaryExpr(DIVISION, intExpr(7), doubleExpr(2.0), DOUBLE));
    printAsString(body, "divideRight", binaryExpr(DIVISION, doubleExpr(7.0), intExpr(2), DOUBLE));

    EXPECT_OUTPUT(program, "3.50\n5.00\n9.50\n3.50\n3.50\n");
    arenaCleanup();
  }

  tstcase("Int variable mixed with a Double keeps its own type", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    // val i = 3; val d = 0.5
    // toStr(i + d); toStr(d * i); print(i); print(i + 1)
    declareVal(body, "i", INT, intExpr(3));
    declareVal(body, "d", DOUBLE, doubleExpr(0.5));
    printAsString(body, "sum", binaryExpr(ADDITION, intVar("i"), varExpr("d", DOUBLE), DOUBLE));
    printAsString(body, "product",
                  binaryExpr(MULTIPLICATION, varExpr("d", DOUBLE), intVar("i"), DOUBLE));

    // The conversion must not overwrite i
    printLine(body, intVar("i"));
    printLine(body, intOperation(ADDITION, intVar("i"), intExpr(1)));

    EXPECT_OUTPUT(program, "3.50\n1.50\n3\n4\n");
    arenaCleanup();
  }

  tstcase("Int subexpression mixed with a Double", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    // toStr((i * 2) + 0.25), toStr(0.5 - (i + i))
    declareVal(body, "i", INT, intExpr(3));
    printAsString(body, "a",
                  binaryExpr(ADDITION, intOperation(MULTIPLICATION, intVar("i"), intExpr(2)),
                             doubleExpr(0.25), DOUBLE));
    printAsString(body, "b",
                  binaryExpr(SUBTRACTION, doubleExpr(0.5),
                             intOperation(ADDITION, intVar("i"), intVar("i")), DOUBLE));

    EXPECT_OUTPUT(program, "6.25\n-5.50\n");
    arenaCleanup();
  }

  tstcase("Comparing an Int with a Double", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "i", INT, intExpr(2));
    declareVal(body, "d", DOUBLE, doubleExpr(2.0));

    printComparisons(body, intExpr(2), doubleExpr(2.5));
    printComparisons(body, doubleExpr(2.0), intExpr(2));
    printComparisons(body, intVar("i"), varExpr("d", DOUBLE));
    printComparisons(body, varExpr("d", DOUBLE), intExpr(3));

    EXPECT_OUTPUT(program, "TTFFFT\nFTFTTF\nFTFTTF\nTTFFFT\n");
    arenaCleanup();
  }

  tstcase("Int argument for a Double parameter", ) {
    AstProgram program = {0};

    // def half(x: Double): Double = x / 2.0
    AstFunction *func = valueFunction(
        &program, "half", DOUBLE,
        exprValue(binaryExpr(DIVISION, varExpr("x", DOUBLE), doubleExpr(2.0), DOUBLE)));
    addParameter(func, "x", DOUBLE);

    // def mix(a: Int, b: Double, c: Double): Double = a + b + c
    AstFunction *mix =
        valueFunction(&program, "mix", DOUBLE,
                      exprValue(binaryExpr(
                          ADDITION, binaryExpr(ADDITION, intVar("a"), varExpr("b", DOUBLE), DOUBLE),
                          varExpr("c", DOUBLE), DOUBLE)));
    addParameter(mix, "a", INT);
    addParameter(mix, "b", DOUBLE);
    addParameter(mix, "c", DOUBLE);

    AstCodeBlock *body = addMain(&program);
    declareVal(body, "i", INT, intExpr(5));
    declareCall(body, "fromLiteral", DOUBLE, CALL_OF("half", intExpr(3)));
    declareCall(body, "fromVariable", DOUBLE, CALL_OF("half", intVar("i")));
    declareCall(body, "fromExpression", DOUBLE,
                CALL_OF("half", intOperation(ADDITION, intVar("i"), intExpr(2))));
    declareCall(body, "mixed", DOUBLE, CALL_OF("mix", intExpr(1), intVar("i"), doubleExpr(0.5)));

    printAsString(body, "a", varExpr("fromLiteral", DOUBLE));
    printAsString(body, "b", varExpr("fromVariable", DOUBLE));
    printAsString(body, "c", varExpr("fromExpression", DOUBLE));
    printAsString(body, "d", varExpr("mixed", DOUBLE));
    printLine(body, intVar("i"));

    EXPECT_OUTPUT(program, "1.50\n2.50\n3.50\n6.50\n5\n");
    arenaCleanup();
  }

  tstcase("Int argument inside a Double parameter is a Double", ) {
    AstProgram program = {0};

    // def show(x: Double): Unit = print(x)
    AstFunction *func = blockFunction(&program, "show", UNIT);
    addParameter(func, "x", DOUBLE);
    printLine(func->body.codeBlock, varExpr("x", DOUBLE));
    returnExpr(func->body.codeBlock, unitExpr());

    AstCodeBlock *body = addMain(&program);
    callCommand(body, CALL_OF("show", intExpr(2)));

    EXPECT_OUTPUT(program, "0x1p+1\n");
    arenaCleanup();
  }

  tstcase("Int value declared as a Double", ) {
    AstProgram program = {0};
    addAddFunction(&program);
    AstCodeBlock *body = addMain(&program);

    // val i = 4; val a: Double = 5; val b: Double = i; val c: Double = i * 2
    // val d: Double = add(1, 2)
    declareVal(body, "i", INT, intExpr(4));
    declareVal(body, "a", DOUBLE, intExpr(5));
    declareVal(body, "b", DOUBLE, intVar("i"));
    declareVal(body, "c", DOUBLE, intOperation(MULTIPLICATION, intVar("i"), intExpr(2)));
    declareCall(body, "d", DOUBLE, CALL_OF("add", intExpr(1), intExpr(2)));

    printLine(body, varExpr("a", DOUBLE));
    printAsString(body, "sa", varExpr("a", DOUBLE));
    printAsString(body, "sb", varExpr("b", DOUBLE));
    printAsString(body, "sc", varExpr("c", DOUBLE));
    printAsString(body, "sd", varExpr("d", DOUBLE));
    printAsString(body, "sum", binaryExpr(ADDITION, varExpr("a", DOUBLE), doubleExpr(0.5), DOUBLE));
    printLine(body, intVar("i"));

    EXPECT_OUTPUT(program, "0x1.4p+2\n5.00\n4.00\n8.00\n3.00\n5.50\n4\n");
    arenaCleanup();
  }

  tstcase("Int value assigned to a Double variable", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    // var d: Double = 1.5; val i = 7
    // d = 2; toStr(d); d = i; toStr(d); d = d + i; toStr(d)
    declareVal(body, "i", INT, intExpr(7));
    declareVar(body, "d", DOUBLE, true, exprValue(doubleExpr(1.5)));
    assignVar(body, "d", exprValue(intExpr(2)));
    printAsString(body, "a", varExpr("d", DOUBLE));
    assignVar(body, "d", exprValue(intVar("i")));
    printAsString(body, "b", varExpr("d", DOUBLE));
    assignVar(body, "d",
              exprValue(binaryExpr(ADDITION, varExpr("d", DOUBLE), intVar("i"), DOUBLE)));
    printAsString(body, "c", varExpr("d", DOUBLE));

    EXPECT_OUTPUT(program, "2.00\n7.00\n14.00\n");
    arenaCleanup();
  }

  tstcase("Int returned from a Double function", ) {
    AstProgram program = {0};

    // def literal(): Double = 4
    valueFunction(&program, "literal", DOUBLE, exprValue(intExpr(4)));

    // def explicit(n: Int): Double { return n + 1 }
    AstFunction *explicitFunc = blockFunction(&program, "explicit", DOUBLE);
    addParameter(explicitFunc, "n", INT);
    returnExpr(explicitFunc->body.codeBlock, intOperation(ADDITION, intVar("n"), intExpr(1)));

    // def viaCall(): Double = add(2, 3)
    addAddFunction(&program);
    valueFunction(&program, "viaCall", DOUBLE, callValue(CALL_OF("add", intExpr(2), intExpr(3))));

    AstCodeBlock *body = addMain(&program);
    declareCall(body, "a", DOUBLE, callNoArgs("literal"));
    declareCall(body, "b", DOUBLE, CALL_OF("explicit", intExpr(9)));
    declareCall(body, "c", DOUBLE, callNoArgs("viaCall"));
    printAsString(body, "sa", varExpr("a", DOUBLE));
    printAsString(body, "sb", varExpr("b", DOUBLE));
    printAsString(body, "sc", varExpr("c", DOUBLE));

    EXPECT_OUTPUT(program, "4.00\n10.00\n5.00\n");
    arenaCleanup();
  }

  tstcase("toInt accepts an Int", ) {
    AstProgram program = {0};
    AstCodeBlock *body = addMain(&program);

    declareVal(body, "i", INT, intExpr(-3));
    declareCall(body, "a", INT, CALL_OF("toInt", intExpr(5)));
    declareCall(body, "b", INT, CALL_OF("toInt", intVar("i")));
    printLine(body, intVar("a"));
    printLine(body, intVar("b"));

    EXPECT_OUTPUT(program, "5\n-3\n");
    arenaCleanup();
  }
}
// AI-END
