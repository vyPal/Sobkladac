/**
 * @file test_ast.c
 * Project: IFJ26 compiler
 * @brief AST unit tests
 * @author Jakub Palacky <xpalacj00>
 *
 * Tests for the AST constructors and list helpers in ast.c
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ast.h"
#include "common.h"
#include "tst.h"

static const AstLocation loc = {.line = 12, .column = 34};

/// True if all `n` bytes at `p` are zero.
static int allZero(const void *p, size_t n) {
  const unsigned char *b = p;
  for (size_t i = 0; i < n; i++)
    if (b[i] != 0)
      return 0;
  return 1;
}

static int sameLoc(AstLocation a, AstLocation b) {
  return a.line == b.line && a.column == b.column;
}

/**
 * Runs `fn` in a child process and returns how it ended:
 *   0      fn returned normally
 *   1..127 fn called exit()/die() with that code
 *   >= 128 the child crashed (128 + signal number)
 */
static int runStatus(void (*fn)(void)) {
  fflush(NULL);
  pid_t pid = fork();
  if (pid < 0) {
    perror("test setup: fork");
    exit(2);
  }
  if (pid == 0) {
    // hide the error message that die() prints
    if (freopen("/dev/null", "w", stderr) == NULL || freopen("/dev/null", "w", stdout) == NULL)
      _exit(3);
    fn();
    _exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}

static void appendFunctionToNull(void) {
  astProgramAppendFunction(NULL, astFunctionNew("f", loc));
}
static void appendParameterToNull(void) {
  astFunctionAppendParameter(NULL, astFunctionParameterNew("p", INT));
}
static void appendCommandToNull(void) {
  astBlockAppendCommand(NULL, astCodeCommandNew(RETURN, loc));
}
static void appendArgumentToNull(void) {
  astCallAppendArgument(NULL, astFunctionCallArgumentNew(NULL));
}

tstsuite("AST", ) {
  tstcase("astProgramNew", ) {
    AstProgram *p = astProgramNew();
    tstassert(p != NULL, "astProgramNew must not return NULL");
    tstcheck(p->firstFunction == NULL && p->lastFunction == NULL, "New program must be empty");

    arenaCleanup();
  }

  tstcase("astFunctionNew", ) {
    char name[] = "foo";
    AstFunction *f = astFunctionNew(name, loc);
    tstassert(f != NULL, "astFunctionNew must not return NULL");
    tstcheck(strcmp(f->functionName, "foo") == 0, "Name must be stored");
    tstcheck(f->functionName != name, "Name must be duplicated, not aliased");
    name[0] = 'X';
    tstcheck(strcmp(f->functionName, "foo") == 0,
             "Changing the source buffer must not affect the copy");
    tstcheck(sameLoc(f->location, loc), "Location must be stored");
    tstcheck(f->isMain == false, "isMain must default to false");
    tstcheck(f->firstParameter == NULL && f->lastParameter == NULL, "Parameter list must be empty");
    tstcheck(f->declaredReturnType == UNKNOWN && f->resolvedReturnType == UNKNOWN,
             "Return types must default to UNKNOWN");
    tstcheck(f->bodyType == VALUE && f->body.value == NULL, "Body must be zeroed");
    tstcheck(f->nextFunction == NULL, "nextFunction must be NULL");

    AstFunction *empty = astFunctionNew("", loc);
    tstcheck(empty->functionName != NULL && empty->functionName[0] == '\0',
             "Empty name must be duplicated as \"\"");

    arenaCleanup();
  }

  tstcase("astFunctionParameterNew", ) {
    char name[] = "param";
    AstFunctionParameter *p = astFunctionParameterNew(name, DOUBLE);
    tstassert(p != NULL, "astFunctionParameterNew must not return NULL");
    tstcheck(strcmp(p->parameterName, "param") == 0, "Name must be stored");
    tstcheck(p->parameterName != name, "Name must be duplicated, not aliased");
    name[0] = 'X';
    tstcheck(strcmp(p->parameterName, "param") == 0,
             "Changing the source buffer must not affect the copy");
    tstcheck(p->dataType == DOUBLE, "Data type must be stored");
    tstcheck(p->nextParameter == NULL, "nextParameter must be NULL");

    arenaCleanup();
  }

  tstcase("astCodeBlockNew", ) {
    AstCodeBlock *b = astCodeBlockNew();
    tstassert(b != NULL, "astCodeBlockNew must not return NULL");
    tstcheck(b->firstCommand == NULL && b->lastCommand == NULL, "New block must be empty");

    arenaCleanup();
  }

  tstcase("astCodeCommandNew", ) {
    const AstCommandType types[] = {
        VARIABLE_DECLARATION, ASSIGNMENT, FUNCTION_CALL, RETURN, IF, WHILE, EXPRESSION};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
      AstCommand *c = astCodeCommandNew(types[i], loc);
      tstassert(c != NULL, "astCodeCommandNew must not return NULL");
      tstcheck(c->type == types[i], "Type %d must be stored", (int)types[i]);
      tstcheck(sameLoc(c->location, loc), "Location must be stored");
      tstcheck(c->nextCommand == NULL, "nextCommand must be NULL");
      tstcheck(allZero(&c->data, sizeof(c->data)), "Command data must be zeroed");
    }

    arenaCleanup();
  }

  tstcase("astExpressionNew", ) {
    const AstExpressionType types[] = {INT_LITERAL, DOUBLE_LITERAL, STRING_LITERAL, UNIT_LITERAL,
                                       VARIABLE,    UNARY,          BINARY};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
      AstExpression *e = astExpressionNew(types[i], loc);
      tstassert(e != NULL, "astExpressionNew must not return NULL");
      tstcheck(e->expressionType == types[i], "Type %d must be stored", (int)types[i]);
      tstcheck(sameLoc(e->location, loc), "Location must be stored");
      tstcheck(e->deducedDataType == UNKNOWN, "deducedDataType must default to UNKNOWN");
      tstcheck(allZero(&e->data, sizeof(e->data)), "Expression data must be zeroed");
    }

    arenaCleanup();
  }

  tstcase("astFunctionCallNew", ) {
    char name[] = "Ifj.write";
    AstFunctionCall *c = astFunctionCallNew(name, loc);
    tstassert(c != NULL, "astFunctionCallNew must not return NULL");
    tstcheck(strcmp(c->functionName, "Ifj.write") == 0, "Name must be stored");
    tstcheck(c->functionName != name, "Name must be duplicated, not aliased");
    name[0] = 'X';
    tstcheck(strcmp(c->functionName, "Ifj.write") == 0,
             "Changing the source buffer must not affect the copy");
    tstcheck(sameLoc(c->location, loc), "Location must be stored");
    tstcheck(c->firstArgument == NULL && c->lastArgument == NULL, "Argument list must be empty");

    arenaCleanup();
  }

  tstcase("astFunctionCallArgumentNew", ) {
    AstExpression *e = astExpressionNew(INT_LITERAL, loc);
    AstFunctionCallArgument *a = astFunctionCallArgumentNew(e);
    tstassert(a != NULL, "astFunctionCallArgumentNew must not return NULL");
    tstcheck(a->expression == e, "Expression must be stored");
    tstcheck(a->nextArgument == NULL, "nextArgument must be NULL");

    AstFunctionCallArgument *n = astFunctionCallArgumentNew(NULL);
    tstcheck(n != NULL && n->expression == NULL, "NULL expression must be accepted");

    arenaCleanup();
  }

  tstcase("astValueFromExpr / astValueFromFunctionCall", ) {
    AstExpression *e = astExpressionNew(VARIABLE, loc);
    AstValue *ve = astValueFromExpr(e);
    tstassert(ve != NULL, "astValueFromExpr must not return NULL");
    tstcheck(ve->type == VALUE_EXPRESSION, "Type must be VALUE_EXPRESSION");
    tstcheck(ve->data.expression == e, "Expression must be stored");

    AstFunctionCall *c = astFunctionCallNew("f", loc);
    AstValue *vc = astValueFromFunctionCall(c);
    tstassert(vc != NULL, "astValueFromFunctionCall must not return NULL");
    tstcheck(vc->type == VALUE_FUNCTION_CALL, "Type must be VALUE_FUNCTION_CALL");
    tstcheck(vc->data.functionCall == c, "Function call must be stored");

    tstcheck(ve != vc, "Each call must allocate a new value");

    arenaCleanup();
  }

  tstcase("Constructors return distinct allocations", ) {
    AstExpression *a = astExpressionNew(INT_LITERAL, loc);
    AstExpression *b = astExpressionNew(INT_LITERAL, loc);
    tstcheck(a != b, "Two calls must not return the same node");
    a->data.intValue = 42;
    tstcheck(b->data.intValue == 0, "Writing one node must not affect another");

    arenaCleanup();
  }

  tstcase("astProgramAppendFunction", ) {
    AstProgram *p = astProgramNew();
    AstFunction *f1 = astFunctionNew("a", loc);
    AstFunction *f2 = astFunctionNew("b", loc);
    AstFunction *f3 = astFunctionNew("c", loc);

    astProgramAppendFunction(p, f1);
    tstcheck(p->firstFunction == f1 && p->lastFunction == f1, "Single function is first and last");
    tstcheck(f1->nextFunction == NULL, "Single function has no successor");

    astProgramAppendFunction(p, f2);
    astProgramAppendFunction(p, f3);
    tstcheck(p->firstFunction == f1, "First function must stay first");
    tstcheck(p->lastFunction == f3, "Last appended function must be last");
    tstcheck(f1->nextFunction == f2 && f2->nextFunction == f3 && f3->nextFunction == NULL,
             "Functions must be linked in append order");

    arenaCleanup();
  }

  tstcase("astFunctionAppendParameter", ) {
    AstFunction *f = astFunctionNew("f", loc);
    AstFunctionParameter *p1 = astFunctionParameterNew("a", INT);
    AstFunctionParameter *p2 = astFunctionParameterNew("b", STRING);
    AstFunctionParameter *p3 = astFunctionParameterNew("c", BOOL);

    astFunctionAppendParameter(f, p1);
    tstcheck(f->firstParameter == p1 && f->lastParameter == p1,
             "Single parameter is first and last");
    tstcheck(p1->nextParameter == NULL, "Single parameter has no successor");

    astFunctionAppendParameter(f, p2);
    astFunctionAppendParameter(f, p3);
    tstcheck(f->firstParameter == p1, "First parameter must stay first");
    tstcheck(f->lastParameter == p3, "Last appended parameter must be last");
    tstcheck(p1->nextParameter == p2 && p2->nextParameter == p3 && p3->nextParameter == NULL,
             "Parameters must be linked in append order");

    arenaCleanup();
  }

  tstcase("astBlockAppendCommand", ) {
    AstCodeBlock *b = astCodeBlockNew();
    AstCommand *c1 = astCodeCommandNew(VARIABLE_DECLARATION, loc);
    AstCommand *c2 = astCodeCommandNew(ASSIGNMENT, loc);
    AstCommand *c3 = astCodeCommandNew(RETURN, loc);

    astBlockAppendCommand(b, c1);
    tstcheck(b->firstCommand == c1 && b->lastCommand == c1, "Single command is first and last");
    tstcheck(c1->nextCommand == NULL, "Single command has no successor");

    astBlockAppendCommand(b, c2);
    astBlockAppendCommand(b, c3);
    tstcheck(b->firstCommand == c1, "First command must stay first");
    tstcheck(b->lastCommand == c3, "Last appended command must be last");
    tstcheck(c1->nextCommand == c2 && c2->nextCommand == c3 && c3->nextCommand == NULL,
             "Commands must be linked in append order");

    arenaCleanup();
  }

  tstcase("astCallAppendArgument", ) {
    AstFunctionCall *c = astFunctionCallNew("f", loc);
    AstFunctionCallArgument *a1 = astFunctionCallArgumentNew(astExpressionNew(INT_LITERAL, loc));
    AstFunctionCallArgument *a2 = astFunctionCallArgumentNew(astExpressionNew(VARIABLE, loc));
    AstFunctionCallArgument *a3 = astFunctionCallArgumentNew(astExpressionNew(STRING_LITERAL, loc));

    astCallAppendArgument(c, a1);
    tstcheck(c->firstArgument == a1 && c->lastArgument == a1, "Single argument is first and last");
    tstcheck(a1->nextArgument == NULL, "Single argument has no successor");

    astCallAppendArgument(c, a2);
    astCallAppendArgument(c, a3);
    tstcheck(c->firstArgument == a1, "First argument must stay first");
    tstcheck(c->lastArgument == a3, "Last appended argument must be last");
    tstcheck(a1->nextArgument == a2 && a2->nextArgument == a3 && a3->nextArgument == NULL,
             "Arguments must be linked in append order");

    arenaCleanup();
  }

  tstcase("Appending to NULL container dies with ERR_INTERNAL", ) {
    tstcheck(runStatus(appendFunctionToNull) == ERR_INTERNAL,
             "astProgramAppendFunction(NULL, ...) must exit with 99");
    tstcheck(runStatus(appendParameterToNull) == ERR_INTERNAL,
             "astFunctionAppendParameter(NULL, ...) must exit with 99");
    tstcheck(runStatus(appendCommandToNull) == ERR_INTERNAL,
             "astBlockAppendCommand(NULL, ...) must exit with 99");
    tstcheck(runStatus(appendArgumentToNull) == ERR_INTERNAL,
             "astCallAppendArgument(NULL, ...) must exit with 99");

    arenaCleanup();
  }

  tstcase("Build a small program tree", ) {
    // fun main() { var x = add(1, 2); return x }
    AstProgram *prog = astProgramNew();
    AstFunction *mainFn = astFunctionNew("main", loc);
    mainFn->isMain = true;
    mainFn->bodyType = CODE_BLOCK;
    mainFn->body.codeBlock = astCodeBlockNew();
    astProgramAppendFunction(prog, mainFn);

    AstFunctionCall *call = astFunctionCallNew("add", loc);
    AstExpression *one = astExpressionNew(INT_LITERAL, loc);
    one->data.intValue = 1;
    AstExpression *two = astExpressionNew(INT_LITERAL, loc);
    two->data.intValue = 2;
    astCallAppendArgument(call, astFunctionCallArgumentNew(one));
    astCallAppendArgument(call, astFunctionCallArgumentNew(two));

    AstCommand *decl = astCodeCommandNew(VARIABLE_DECLARATION, loc);
    decl->data.variableDeclaration.variableName = "x";
    decl->data.variableDeclaration.isMutable = true;
    decl->data.variableDeclaration.value = astValueFromFunctionCall(call);
    astBlockAppendCommand(mainFn->body.codeBlock, decl);

    AstExpression *x = astExpressionNew(VARIABLE, loc);
    x->data.variable = "x";
    AstCommand *ret = astCodeCommandNew(RETURN, loc);
    ret->data.returnAst.value = astValueFromExpr(x);
    astBlockAppendCommand(mainFn->body.codeBlock, ret);

    // walk it back
    AstFunction *f = prog->firstFunction;
    tstassert(f != NULL && f->isMain && strcmp(f->functionName, "main") == 0,
              "main must be reachable");
    AstCommand *c = f->body.codeBlock->firstCommand;
    tstassert(c != NULL && c->type == VARIABLE_DECLARATION,
              "First command must be the declaration");
    AstValue *v = c->data.variableDeclaration.value;
    tstassert(v->type == VALUE_FUNCTION_CALL, "Declaration value must be a call");
    AstFunctionCallArgument *arg = v->data.functionCall->firstArgument;
    tstcheck(
        arg->expression->data.intValue == 1 && arg->nextArgument->expression->data.intValue == 2,
        "Call arguments must be 1, 2 in order");
    c = c->nextCommand;
    tstassert(c != NULL && c->type == RETURN, "Second command must be the return");
    tstcheck(c->data.returnAst.value->data.expression == x, "Return must yield x");
    tstcheck(c->nextCommand == NULL, "Return must be the last command");

    arenaCleanup();
  }
}
