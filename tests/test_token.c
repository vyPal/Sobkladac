/**
 * @file test_token.c
 * Project: IFJ26 compiler
 * @brief Unit tests for lexical token functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file tests the functions for creating and manipulating lexical tokens (token.c) using the
 * tst framework.
 */

// AI-BEGIN [Claude Sonnet 5.5, claude.ai]
// Prompt: "create me one file with tests for lexer.c and another one with tests for token.c
// tests based on tst.h"
// Context: Uploaded tst.h (test framework), lexer.c, token.c, token.h, lexer.h.
// The tests were verified by compiling them
// against the uploaded sources and running them under ASan/UBSan.
#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <unistd.h>

#include "token.h"
#include "tst.h"

// tstcase() and tstsuite() take an optional, variadic list of tags. The tag list is always passed,
// empty if there are no tags, because omitting a variadic argument is not ISO C99 (-Wpedantic).

/// Number of token types; TK_EOF is the last enumerator.
#define TK_COUNT ((int)TK_EOF + 1)

// ---------------------------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------------------------

static int savedStdout = -1;
static FILE *captureFile = NULL;

/// Starts redirecting stdout into a temporary file.
static void captureBegin(void) {
  fflush(stdout);
  savedStdout = dup(STDOUT_FILENO);
  captureFile = tmpfile();
  if (savedStdout < 0 || captureFile == NULL || dup2(fileno(captureFile), STDOUT_FILENO) < 0) {
    perror("test setup: cannot capture stdout");
    exit(2);
  }
}

/// Stops the redirection and copies what was printed to buf (NUL-terminated).
static void captureEnd(char *buf, size_t size) {
  fflush(stdout);
  dup2(savedStdout, STDOUT_FILENO);
  close(savedStdout);
  fseek(captureFile, 0, SEEK_SET);
  size_t n = fread(buf, 1, size - 1, captureFile);
  buf[n] = '\0';
  fclose(captureFile);
  captureFile = NULL;
}

/// Returns what printToken() prints for the token.
static const char *printed(const struct Token *token) {
  static char buf[256];
  captureBegin();
  printToken(token);
  captureEnd(buf, sizeof buf);
  return buf;
}

/// Builds the expected beginning of the printToken() output.
static const char *header(enum TkType type, int line) {
  static char buf[64];
  snprintf(buf, sizeof buf, "Token type: %d, line: %d , ", (int)type, line);
  return buf;
}

/// Builds a list head->...->tail from the given tokens and returns the head.
static struct LinkedTlist *listOf(struct Token **tokens, int count) {
  struct LinkedTlist *head = newTlist();
  struct LinkedTlist *tail = head;
  for (int i = 0; i < count; i++) tail = pushToken(tokens[i], tail);
  return head;
}

// ---------------------------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------------------------

tstsuite("token", ) {
  // ----- tokens -----------------------------------------------------------------------------

  tstcase("newToken stores type and line, value starts as NULL", ) {
    struct Token *t = newToken(TK_PLUS, 7);
    tstassert(t != NULL, "newToken returns a token");
    tstcheck(getTkType(t) == TK_PLUS, "type is stored");
    tstcheck(getLine(t) == 7, "line is stored");
    tstcheck(getValue(t) == NULL, "value is NULL until set");
  }

  tstcase("newToken works for every token type", ) {
    for (int type = 0; type < TK_COUNT; type++) {
      struct Token *t = newToken((enum TkType)type, type + 1);
      tstexpect(t != NULL && getTkType(t) == (enum TkType)type && getLine(t) == type + 1,
                "token type %d keeps its type and line", type);
    }
  }

  tstcase("newToken accepts boundary line numbers", ) {
    tstcheck(getLine(newToken(TK_EOF, 0)) == 0, "line 0");
    tstcheck(getLine(newToken(TK_EOF, 1)) == 1, "line 1");
    tstcheck(getLine(newToken(TK_EOF, 2147483647)) == 2147483647, "line INT_MAX");
  }

  tstcase("tokens are independent objects", ) {
    struct Token *a = newToken(TK_PLUS, 1);
    struct Token *b = newToken(TK_MINUS, 2);
    tstcheck(a != b, "two calls return two different tokens");
    tstcheck(getTkType(a) == TK_PLUS && getLine(a) == 1, "first token is untouched");
    tstcheck(getTkType(b) == TK_MINUS && getLine(b) == 2, "second token is untouched");

    union Value v = {.i = 5};
    setValue(a, &v);
    tstcheck(getValue(b) == NULL, "setting a value on one token does not affect another");
  }

  tstcase("setValue / getValue with an int", ) {
    struct Token *t = newToken(TK_NUMBER, 1);
    union Value v = {.i = -42};
    setValue(t, &v);
    tstcheck(getValue(t) == &v, "getValue returns the very same pointer");
    tstcheck(getValue(t)->i == -42, "int value is readable");
  }

  tstcase("setValue / getValue with a double", ) {
    struct Token *t = newToken(TK_DECIMAL, 1);
    union Value v = {.d = 3.5};
    setValue(t, &v);
    tstcheck(getValue(t) == &v, "getValue returns the very same pointer");
    tstcheck(getValue(t)->d == 3.5, "double value is readable");
  }

  tstcase("setValue / getValue with a string", ) {
    struct Token *t = newToken(TK_STRING, 1);
    char text[] = "abc";
    struct String *s = newString(text, 3);
    union Value v = {.s = s};
    setValue(t, &v);
    tstcheck(getValue(t) == &v, "getValue returns the very same pointer");
    tstcheck(getValue(t)->s == s, "string pointer is readable");
  }

  tstcase("setValue replaces the previous value and can clear it", ) {
    struct Token *t = newToken(TK_NUMBER, 1);
    union Value first = {.i = 1};
    union Value second = {.i = 2};
    setValue(t, &first);
    setValue(t, &second);
    tstcheck(getValue(t) == &second, "second value replaces the first");
    setValue(t, NULL);
    tstcheck(getValue(t) == NULL, "setValue(NULL) clears the value");
  }

  tstcase("getters work on a const token", ) {
    const struct Token *t = newToken(TK_COMMA, 9);
    tstcheck(getTkType(t) == TK_COMMA, "getTkType");
    tstcheck(getLine(t) == 9, "getLine");
    tstcheck(getValue(t) == NULL, "getValue");
  }

  // ----- strings ----------------------------------------------------------------------------

  tstcase("newString keeps the text pointer and the length", ) {
    char text[] = "hello";
    struct String *s = newString(text, 5);
    tstassert(s != NULL, "newString returns a string");
    tstcheck(getString(s) == text, "the text is not copied, the pointer is kept");
    tstcheck(getStringLength(s) == 5, "length is stored");
    tstcheck(strcmp(getString(s), "hello") == 0, "content is readable");
  }

  tstcase("newString does not copy: later changes of the buffer are visible", ) {
    char text[] = "hello";
    struct String *s = newString(text, 5);
    text[0] = 'j';
    tstcheck(strcmp(getString(s), "jello") == 0, "the string sees the change");
  }

  tstcase("newString with an empty string", ) {
    char text[] = "";
    struct String *s = newString(text, 0);
    tstcheck(getStringLength(s) == 0, "length is 0");
    tstcheck(getString(s)[0] == '\0', "content is empty");
  }

  tstcase("newString length is the stored length, not strlen", ) {
    char text[] = {'a', '\0', 'b', '\0'};
    struct String *s = newString(text, 3);
    tstcheck(getStringLength(s) == 3, "length counts the embedded NUL");
    tstcheck(memcmp(getString(s), "a\0b", 3) == 0, "all three bytes are reachable");
  }

  tstcase("newString strings are independent objects", ) {
    char a[] = "a";
    char b[] = "bb";
    struct String *sa = newString(a, 1);
    struct String *sb = newString(b, 2);
    tstcheck(sa != sb, "different objects");
    tstcheck(getString(sa) == a && getStringLength(sa) == 1, "first is untouched");
    tstcheck(getString(sb) == b && getStringLength(sb) == 2, "second is untouched");
  }

  // ----- token list: creation and push ------------------------------------------------------

  tstcase("newTlist creates an empty list", ) {
    struct LinkedTlist *l = newTlist();
    tstassert(l != NULL, "newTlist returns a list");
    tstcheck(lookAheadToken(l, 0) == NULL, "there is no first token");
    tstcheck(popToken(l) == NULL, "popping an empty list returns NULL");
  }

  tstcase("newTlist returns a new list every time", ) {
    tstcheck(newTlist() != newTlist(), "two lists are two objects");
  }

  tstcase("pushToken onto NULL creates a new list", ) {
    struct Token *a = newToken(TK_PLUS, 1);
    struct LinkedTlist *l = pushToken(a, NULL);
    tstassert(l != NULL, "a list is returned");
    tstcheck(lookAheadToken(l, 0) == a, "the token is the first one");
    tstcheck(lookAheadToken(l, 1) == NULL, "and the only one");
  }

  tstcase("pushToken fills an empty head node in place", ) {
    struct LinkedTlist *head = newTlist();
    struct Token *a = newToken(TK_PLUS, 1);
    struct LinkedTlist *tail = pushToken(a, head);
    tstcheck(tail == head, "the head node is reused, no extra node");
    tstcheck(lookAheadToken(head, 0) == a, "token is stored in the head");
    tstcheck(lookAheadToken(head, 1) == NULL, "list has one token");
  }

  tstcase("pushToken appends and returns the new tail", ) {
    struct LinkedTlist *head = newTlist();
    struct Token *a = newToken(TK_PLUS, 1);
    struct Token *b = newToken(TK_MINUS, 2);
    struct LinkedTlist *tail = pushToken(a, head);
    struct LinkedTlist *tail2 = pushToken(b, tail);
    tstcheck(tail2 != tail, "a second token gets a new node");
    tstcheck(lookAheadToken(head, 0) == a, "first token");
    tstcheck(lookAheadToken(head, 1) == b, "second token");
    tstcheck(lookAheadToken(head, 2) == NULL, "nothing after the second token");
    tstcheck(lookAheadToken(tail2, 0) == b, "the returned tail starts at the last token");
  }

  tstcase("pushToken keeps the order of many tokens", ) {
    enum { N = 100 };
    struct Token *tokens[N];
    struct LinkedTlist *head = newTlist();
    struct LinkedTlist *tail = head;
    for (int i = 0; i < N; i++) {
      tokens[i] = newToken(TK_NUMBER, i);
      tail = pushToken(tokens[i], tail);
    }
    int inOrder = 1;
    for (int i = 0; i < N; i++) inOrder &= (lookAheadToken(head, i) == tokens[i]);
    tstcheck(inOrder, "all tokens come out in the order they were pushed");
    tstcheck(lookAheadToken(head, N) == NULL, "no extra token at the end");
  }

  // ----- token list: popToken ---------------------------------------------------------------

  tstcase("popToken on NULL returns NULL", ) {
    tstcheck(popToken(NULL) == NULL, "NULL list is tolerated");
  }

  tstcase("popToken on a one-token list empties it", ) {
    struct Token *a = newToken(TK_PLUS, 1);
    struct LinkedTlist *l = pushToken(a, NULL);
    tstcheck(popToken(l) == a, "returns the token");
    tstcheck(lookAheadToken(l, 0) == NULL, "list is empty now");
    tstcheck(popToken(l) == NULL, "popping again returns NULL");
  }

  tstcase("popToken returns the tokens in FIFO order", ) {
    struct Token *tokens[4] = {newToken(TK_PLUS, 1), newToken(TK_MINUS, 2),
                               newToken(TK_MULTIPLY, 3), newToken(TK_DIVIDE, 4)};
    struct LinkedTlist *l = listOf(tokens, 4);
    for (int i = 0; i < 4; i++) {
      tstexpect(popToken(l) == tokens[i], "pop number %d returns token number %d", i, i);
    }
    tstcheck(popToken(l) == NULL, "the list is exhausted");
    tstcheck(lookAheadToken(l, 0) == NULL, "and empty");
  }

  tstcase("popToken moves the rest of the list forward", ) {
    struct Token *tokens[3] = {newToken(TK_PLUS, 1), newToken(TK_MINUS, 2),
                               newToken(TK_MULTIPLY, 3)};
    struct LinkedTlist *l = listOf(tokens, 3);
    (void)popToken(l);
    tstcheck(lookAheadToken(l, 0) == tokens[1], "the second token is now the first");
    tstcheck(lookAheadToken(l, 1) == tokens[2], "the third token is now the second");
    tstcheck(lookAheadToken(l, 2) == NULL, "list got shorter");
  }

  // ----- token list: lookAheadToken ---------------------------------------------------------

  tstcase("lookAheadToken on NULL returns NULL", ) {
    tstcheck(lookAheadToken(NULL, 0) == NULL, "n = 0");
    tstcheck(lookAheadToken(NULL, 3) == NULL, "n = 3");
  }

  tstcase("lookAheadToken with a negative index returns NULL", ) {
    struct Token *tokens[2] = {newToken(TK_PLUS, 1), newToken(TK_MINUS, 2)};
    struct LinkedTlist *l = listOf(tokens, 2);
    tstcheck(lookAheadToken(l, -1) == NULL, "n = -1");
    tstcheck(lookAheadToken(l, -100) == NULL, "n = -100");
  }

  tstcase("lookAheadToken returns the n-th token, head is 0", ) {
    struct Token *tokens[3] = {newToken(TK_PLUS, 1), newToken(TK_MINUS, 2),
                               newToken(TK_MULTIPLY, 3)};
    struct LinkedTlist *l = listOf(tokens, 3);
    tstcheck(lookAheadToken(l, 0) == tokens[0], "n = 0");
    tstcheck(lookAheadToken(l, 1) == tokens[1], "n = 1");
    tstcheck(lookAheadToken(l, 2) == tokens[2], "n = 2");
  }

  tstcase("lookAheadToken past the end returns NULL", ) {
    struct Token *tokens[3] = {newToken(TK_PLUS, 1), newToken(TK_MINUS, 2),
                               newToken(TK_MULTIPLY, 3)};
    struct LinkedTlist *l = listOf(tokens, 3);
    tstcheck(lookAheadToken(l, 3) == NULL, "n = length");
    tstcheck(lookAheadToken(l, 4) == NULL, "n = length + 1");
    tstcheck(lookAheadToken(l, 100000) == NULL, "n much larger than the list");
  }

  tstcase("lookAheadToken does not remove anything", ) {
    struct Token *tokens[2] = {newToken(TK_PLUS, 1), newToken(TK_MINUS, 2)};
    struct LinkedTlist *l = listOf(tokens, 2);
    tstcheck(lookAheadToken(l, 1) == tokens[1], "first look");
    tstcheck(lookAheadToken(l, 1) == tokens[1], "second look gives the same result");
    tstcheck(lookAheadToken(l, 0) == tokens[0], "head is unchanged");
    tstcheck(popToken(l) == tokens[0], "and can still be popped");
  }

  tstcase("lookAheadToken works from a node in the middle of the list", ) {
    struct Token *tokens[3] = {newToken(TK_PLUS, 1), newToken(TK_MINUS, 2),
                               newToken(TK_MULTIPLY, 3)};
    struct LinkedTlist *head = newTlist();
    struct LinkedTlist *n0 = pushToken(tokens[0], head);
    struct LinkedTlist *n1 = pushToken(tokens[1], n0);
    struct LinkedTlist *n2 = pushToken(tokens[2], n1);
    tstcheck(lookAheadToken(n1, 0) == tokens[1], "n1 starts at the second token");
    tstcheck(lookAheadToken(n1, 1) == tokens[2], "and sees the third one");
    tstcheck(lookAheadToken(n2, 1) == NULL, "the tail sees nothing after itself");
  }

  // ----- printToken -------------------------------------------------------------------------

  tstcase("printToken(NULL) prints nothing", ) {
    tstcheck(strcmp(printed(NULL), "") == 0, "no output for NULL");
  }

  tstcase("printToken without a value prints (null)", ) {
    char expected[128];
    snprintf(expected, sizeof expected, "%svalue: (null)\n", header(TK_PLUS, 3));
    tstcheck(strcmp(printed(newToken(TK_PLUS, 3)), expected) == 0, "operator token");
    snprintf(expected, sizeof expected, "%svalue: (null)\n", header(TK_IDENT, 3));
    tstcheck(strcmp(printed(newToken(TK_IDENT, 3)), expected) == 0, "identifier token");
  }

  tstcase("printToken prints the string of TK_IDENT and TK_STRING", ) {
    char text[] = "name";
    union Value v = {.s = newString(text, 4)};
    char expected[128];

    struct Token *ident = newToken(TK_IDENT, 2);
    setValue(ident, &v);
    snprintf(expected, sizeof expected, "%svalue: name\n", header(TK_IDENT, 2));
    tstcheck(strcmp(printed(ident), expected) == 0, "identifier");

    struct Token *str = newToken(TK_STRING, 5);
    setValue(str, &v);
    snprintf(expected, sizeof expected, "%svalue: name\n", header(TK_STRING, 5));
    tstcheck(strcmp(printed(str), expected) == 0, "string");
  }

  tstcase("printToken prints the int of TK_NUMBER and TK_INDENT", ) {
    union Value v = {.i = 42};
    char expected[128];

    struct Token *num = newToken(TK_NUMBER, 1);
    setValue(num, &v);
    snprintf(expected, sizeof expected, "%svalue: 42\n", header(TK_NUMBER, 1));
    tstcheck(strcmp(printed(num), expected) == 0, "number");

    struct Token *indent = newToken(TK_INDENT, 4);
    setValue(indent, &v);
    snprintf(expected, sizeof expected, "%svalue: 42\n", header(TK_INDENT, 4));
    tstcheck(strcmp(printed(indent), expected) == 0, "indent");

    union Value neg = {.i = -7};
    struct Token *n2 = newToken(TK_NUMBER, 1);
    setValue(n2, &neg);
    snprintf(expected, sizeof expected, "%svalue: -7\n", header(TK_NUMBER, 1));
    tstcheck(strcmp(printed(n2), expected) == 0, "negative number");
  }

  tstcase("printToken prints the double of TK_DECIMAL", ) {
    union Value v = {.d = 3.5};
    struct Token *t = newToken(TK_DECIMAL, 6);
    setValue(t, &v);
    char expected[128];
    snprintf(expected, sizeof expected, "%svalue: 3.500000\n", header(TK_DECIMAL, 6));
    tstcheck(strcmp(printed(t), expected) == 0, "decimal is printed with six decimal places");
  }

  tstcase("printToken says (not applicable) for types without a printable value", ) {
    union Value v = {.i = 1};
    struct Token *t = newToken(TK_PLUS, 8);
    setValue(t, &v);
    char expected[128];
    snprintf(expected, sizeof expected, "%svalue: (not applicable)\n", header(TK_PLUS, 8));
    tstcheck(strcmp(printed(t), expected) == 0, "operator with a value attached");
  }
}
// AI-END
