/**
 * @file test_lexer.c
 * Project: IFJ26 compiler
 * @brief Unit tests for lexical analyzer functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file tests the lexical analyzer (lexer.c) using the tst framework.
 */

// AI-BEGIN [Claude Sonnet 5.5, claude.ai]
// Prompt: "create me one file with tests for lexer.c and another one with tests for token.c
// tests based on tst.h"
// Context: Uploaded tst.h (test framework), lexer_1.c (lexer), token.c, token.h, lexer.h, strbuf.c
// and strbuf.h. Earlier in the same conversation the AI fixed the lexer (integer overflow, exact
// decimals via strtod, line counting in multi-line strings, text after block comments) and switched
// it to StringBuilder; these tests cover those fixes. The tests were verified by compiling them
// against the uploaded sources and running them under ASan/UBSan.
#define _POSIX_C_SOURCE 200809L

#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lexer.h"
#include "token.h"
#include "tst.h"

/// Exit status of a lexical error: the lexer calls die(1, ...).
#define LEX_ERROR 1

// ---------------------------------------------------------------------------------------------
// running the lexer on a string
// ---------------------------------------------------------------------------------------------

/// Writes the whole buffer to fd.
static int writeAll(int fd, const char *data, size_t len) {
  while (len > 0) {
    ssize_t n = write(fd, data, len);
    if (n <= 0)
      return -1;
    data += n;
    len -= (size_t)n;
  }
  return 0;
}

/// Makes `text` the content of stdin, which is what lexerMain() reads.
static void feed(const char *text) {
  char path[] = "/tmp/test_lexer_XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0 || writeAll(fd, text, strlen(text)) != 0 || close(fd) != 0 ||
      freopen(path, "r", stdin) == NULL) {
    perror("test setup: cannot prepare stdin");
    exit(2);
  }
  unlink(path);
}

/**
 * Lexes `text` in a child process and returns how it ended:
 *   0          lexing succeeded
 *   LEX_ERROR  the lexer called die(1, ...), i.e. reported a lexical error
 *   >= 128     the child crashed (128 + signal number)
 */
static int lexStatus(const char *text) {
  feed(text);
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
    lexerMain();
    _exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}

/// Returns a printable version of `s` (newline -> \n, tab -> \t, ...) for test messages.
static const char *esc(const char *s) {
  static char buf[160];
  size_t o = 0;
  for (; *s != '\0' && o + 4 < sizeof buf; s++) {
    switch (*s) {
      case '\n':
        buf[o++] = '\\';
        buf[o++] = 'n';
        break;
      case '\t':
        buf[o++] = '\\';
        buf[o++] = 't';
        break;
      case '\r':
        buf[o++] = '\\';
        buf[o++] = 'r';
        break;
      case '\\':
        buf[o++] = '\\';
        buf[o++] = '\\';
        break;
      default:
        buf[o++] = *s;
        break;
    }
  }
  buf[o] = '\0';
  return buf;
}

/**
 * Lexes `text` and returns the token list. Only for input that is expected to be valid.
 *
 * The input is first lexed in a child process. If the lexer rejects it (or crashes), NULL is
 * returned and a note is printed, so a wrongly rejected input fails only its own test instead of
 * terminating the whole test run. All token helpers below accept NULL and report a mismatch.
 */
static LinkedTlist *lex(const char *text) {
  int status = lexStatus(text);
  if (status != 0) {
    fprintf(stderr, "      lexer failed (exit status %d) on: %s\n", status, esc(text));
    return NULL;
  }
  feed(text);
  return lexerMain();
}

// ---------------------------------------------------------------------------------------------
// checking tokens
// ---------------------------------------------------------------------------------------------

static const char *const TK_NAMES[] = {
    "IDENT",  "NUMBER",    "DECIMAL", "STRING", "PLUS",   "MINUS",  "MULTIPLY",
    "DIVIDE", "BIGGER",    "SMALLER", "EQUAL",  "SEQUAL", "BEQUAL", "NOT_EQUAL",
    "COLON",  "SEMICOLON", "COMMA",   "ASSIGN", "PIPE",   "LPAREN", "RPAREN",
    "DOT",    "NEWLINE",   "INDENT",  "ATMAIN", "EOF"};

static const char *tokName(int type) {
  if (type < 0 || type >= (int)(sizeof TK_NAMES / sizeof TK_NAMES[0]))
    return "?";
  return TK_NAMES[type];
}

/// Number of tokens in the list.
static int tokCount(LinkedTlist *l) {
  int n = 0;
  while (lookAheadToken(l, n) != NULL) n++;
  return n;
}

/// Token n has the given type.
static int tokIs(LinkedTlist *l, int n, enum TkType type) {
  Token *k = lookAheadToken(l, n);
  return k != NULL && getTkType(k) == type;
}

/// Token n is on the given line.
static int tokLine(LinkedTlist *l, int n, int line) {
  Token *k = lookAheadToken(l, n);
  return k != NULL && getLine(k) == line;
}

/// Token n has the given type and int value (TK_NUMBER, TK_INDENT).
static int tokInt(LinkedTlist *l, int n, enum TkType type, int value) {
  Token *k = lookAheadToken(l, n);
  return k != NULL && getTkType(k) == type && getValue(k) != NULL && getValue(k)->i == value;
}

/// Token n is a TK_DECIMAL with exactly this value.
static int tokDec(LinkedTlist *l, int n, double value) {
  Token *k = lookAheadToken(l, n);
  return k != NULL && getTkType(k) == TK_DECIMAL && getValue(k) != NULL && getValue(k)->d == value;
}

/// Token n has the given type (TK_IDENT, TK_STRING) and exactly these len bytes of text. The text
/// must also be NUL-terminated.
static int tokText(LinkedTlist *l, int n, enum TkType type, const char *text, size_t len) {
  Token *k = lookAheadToken(l, n);
  if (k == NULL || getTkType(k) != type || getValue(k) == NULL)
    return 0;
  struct String *s = getValue(k)->s;
  return s != NULL && getString(s) != NULL && getStringLength(s) == len &&
         memcmp(getString(s), text, len) == 0 && getString(s)[len] == '\0';
}

/// Same as tokText for a text without embedded NULs.
static int tokStr(LinkedTlist *l, int n, enum TkType type, const char *text) {
  return tokText(l, n, type, text, strlen(text));
}

/// Token n is a TK_IDENT or TK_STRING made of exactly `len` times the character `c`.
static int tokRun(LinkedTlist *l, int n, enum TkType type, char c, size_t len) {
  Token *k = lookAheadToken(l, n);
  if (k == NULL || getTkType(k) != type || getValue(k) == NULL)
    return 0;
  struct String *s = getValue(k)->s;
  if (s == NULL || getStringLength(s) != len || getString(s)[len] != '\0')
    return 0;
  for (size_t i = 0; i < len; i++)
    if (getString(s)[i] != c)
      return 0;
  return 1;
}

/// The list has exactly this sequence of token types (the list is terminated by -1).
static int tokSeqImpl(LinkedTlist *l, ...) {
  va_list ap;
  va_start(ap, l);
  int i = 0;
  int ok = 1;
  for (;;) {
    int want = va_arg(ap, int);
    if (want < 0)
      break;
    Token *k = lookAheadToken(l, i);
    if (k == NULL || (int)getTkType(k) != want) {
      ok = 0;
      break;
    }
    i++;
  }
  va_end(ap);
  if (ok && lookAheadToken(l, i) != NULL)
    ok = 0;  // more tokens than expected
  if (!ok) {
    fprintf(stderr, "      got:");
    for (int j = 0; j < 40 && lookAheadToken(l, j) != NULL; j++)
      fprintf(stderr, " %s", tokName((int)getTkType(lookAheadToken(l, j))));
    fprintf(stderr, "\n");
  }
  return ok;
}
#define tokSeq(l, ...) tokSeqImpl((l), __VA_ARGS__, -1)

/// Returns a malloc'd string: prefix + n times c + suffix.
static char *repeat(const char *prefix, char c, size_t n, const char *suffix) {
  size_t a = strlen(prefix);
  size_t b = strlen(suffix);
  char *s = malloc(a + n + b + 1);
  if (s == NULL)
    exit(2);
  memcpy(s, prefix, a);
  memset(s + a, c, n);
  memcpy(s + a + n, suffix, b);
  s[a + n + b] = '\0';
  return s;
}

// ---------------------------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------------------------

tstsuite("lexer") {
  // ----- basics -----------------------------------------------------------------------------

  tstcase("empty input yields only EOF") {
    LinkedTlist *l = lex("");
    tstcheck(tokSeq(l, TK_EOF), "just the EOF token");
    tstcheck(tokLine(l, 0, 1), "EOF is on line 1");
  }

  tstcase("only spaces yield only EOF") {
    tstcheck(tokSeq(lex("     "), TK_EOF), "spaces are skipped");
  }

  tstcase("the token list always ends with EOF") {
    LinkedTlist *l = lex("a + 1");
    tstcheck(tokIs(l, tokCount(l) - 1, TK_EOF), "last token is EOF");
  }

  // ----- operators and punctuation ----------------------------------------------------------

  tstcase("every operator and punctuation token") {
    static const struct {
      const char *src;
      enum TkType type;
    } ops[] = {{"+", TK_PLUS},      {"-", TK_MINUS},   {"*", TK_MULTIPLY},   {"/", TK_DIVIDE},
               {">", TK_BIGGER},    {"<", TK_SMALLER}, {"=", TK_ASSIGN},     {"==", TK_EQUAL},
               {"<=", TK_SEQUAL},   {">=", TK_BEQUAL}, {"!=", TK_NOT_EQUAL}, {":", TK_COLON},
               {";", TK_SEMICOLON}, {",", TK_COMMA},   {"|", TK_PIPE},       {"(", TK_LPAREN},
               {")", TK_RPAREN},    {".", TK_DOT}};
    for (size_t i = 0; i < sizeof ops / sizeof ops[0]; i++) {
      tstexpect(tokSeq(lex(ops[i].src), ops[i].type, TK_EOF), "'%s' is %s", ops[i].src,
                tokName(ops[i].type));
    }
  }

  tstcase("operators work without spaces around them") {
    tstcheck(tokSeq(lex("a+b"), TK_IDENT, TK_PLUS, TK_IDENT, TK_EOF), "a+b");
    tstcheck(tokSeq(lex("a>=b"), TK_IDENT, TK_BEQUAL, TK_IDENT, TK_EOF), "a>=b");
    tstcheck(tokSeq(lex("a<=b"), TK_IDENT, TK_SEQUAL, TK_IDENT, TK_EOF), "a<=b");
    tstcheck(tokSeq(lex("a==b"), TK_IDENT, TK_EQUAL, TK_IDENT, TK_EOF), "a==b");
    tstcheck(tokSeq(lex("a!=b"), TK_IDENT, TK_NOT_EQUAL, TK_IDENT, TK_EOF), "a!=b");
    tstcheck(tokSeq(lex("(a)"), TK_LPAREN, TK_IDENT, TK_RPAREN, TK_EOF), "(a)");
    tstcheck(tokSeq(lex("a.b"), TK_IDENT, TK_DOT, TK_IDENT, TK_EOF), "a.b");
    tstcheck(tokSeq(lex("f(a,b);"), TK_IDENT, TK_LPAREN, TK_IDENT, TK_COMMA, TK_IDENT, TK_RPAREN,
                    TK_SEMICOLON, TK_EOF),
             "f(a,b);");
  }

  tstcase("two-character operators are matched greedily") {
    tstcheck(tokSeq(lex("==="), TK_EQUAL, TK_ASSIGN, TK_EOF), "=== is == then =");
    tstcheck(tokSeq(lex("!=="), TK_NOT_EQUAL, TK_ASSIGN, TK_EOF), "!== is != then =");
    tstcheck(tokSeq(lex("<>"), TK_SMALLER, TK_BIGGER, TK_EOF), "<> is < then >");
    tstcheck(tokSeq(lex("=>"), TK_ASSIGN, TK_BIGGER, TK_EOF), "=> is = then >");
    tstcheck(tokSeq(lex(">=="), TK_BEQUAL, TK_ASSIGN, TK_EOF), ">== is >= then =");
  }

  tstcase("a space splits a two-character operator") {
    tstcheck(tokSeq(lex("= ="), TK_ASSIGN, TK_ASSIGN, TK_EOF), "= =");
    tstcheck(tokSeq(lex("< ="), TK_SMALLER, TK_ASSIGN, TK_EOF), "< =");
  }

  tstcase("a minus sign is never part of a number") {
    LinkedTlist *l = lex("-5");
    tstcheck(tokSeq(l, TK_MINUS, TK_NUMBER, TK_EOF), "-5 is MINUS NUMBER");
    tstcheck(tokInt(l, 1, TK_NUMBER, 5), "the number is 5");
  }

  tstcase("whitespace between tokens is ignored") {
    tstcheck(tokSeq(lex("a      b"), TK_IDENT, TK_IDENT, TK_EOF), "many spaces");
    tstcheck(tokSeq(lex("   a b   "), TK_IDENT, TK_IDENT, TK_EOF), "leading and trailing spaces");
  }

  // ----- @main ------------------------------------------------------------------------------

  tstcase("@main is a single token") {
    tstcheck(tokSeq(lex("@main "), TK_ATMAIN, TK_EOF), "@main followed by a space");
    tstcheck(tokSeq(lex("@main x"), TK_ATMAIN, TK_IDENT, TK_EOF), "@main followed by a token");
  }

  tstcase("malformed @main is a lexical error") {
    static const char *const bad[] = {"@",     "@x",      "@m",     "@mai",  "@mainx",
                                      "@main", "@main\n", "@Main ", "@ main"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is a lexical error", esc(bad[i]));
    }
  }

  // ----- identifiers ------------------------------------------------------------------------

  tstcase("identifiers keep their exact text") {
    static const char *const ids[] = {"a",  "abc", "Hello", "snake_case", "camelCase1", "x1",
                                      "_x", "__",  "_1",    "A_B_C",      "a__b",       "x_"};
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
      LinkedTlist *l = lex(ids[i]);
      tstexpect(tokSeq(l, TK_IDENT, TK_EOF) && tokStr(l, 0, TK_IDENT, ids[i]), "identifier '%s'",
                ids[i]);
    }
  }

  tstcase("identifiers are separated by spaces and operators") {
    LinkedTlist *l = lex("foo bar+baz");
    tstcheck(tokSeq(l, TK_IDENT, TK_IDENT, TK_PLUS, TK_IDENT, TK_EOF), "token types");
    tstcheck(tokStr(l, 0, TK_IDENT, "foo"), "first");
    tstcheck(tokStr(l, 1, TK_IDENT, "bar"), "second");
    tstcheck(tokStr(l, 3, TK_IDENT, "baz"), "third");
  }

  tstcase("a letter is not split from the digits that follow it") {
    LinkedTlist *l = lex("a1 b22");
    tstcheck(tokSeq(l, TK_IDENT, TK_IDENT, TK_EOF), "two identifiers");
    tstcheck(tokStr(l, 0, TK_IDENT, "a1"), "a1");
    tstcheck(tokStr(l, 1, TK_IDENT, "b22"), "b22");
  }

  tstcase("the lexer does not treat keywords specially") {
    LinkedTlist *l = lex("if else while return");
    tstcheck(tokSeq(l, TK_IDENT, TK_IDENT, TK_IDENT, TK_IDENT, TK_EOF), "all are identifiers");
    tstcheck(tokStr(l, 0, TK_IDENT, "if"), "if");
  }

  tstcase("a lone underscore is not an identifier") {
    tstcheck(lexStatus("_") == LEX_ERROR, "_");
    tstcheck(lexStatus("_ ") == LEX_ERROR, "_ followed by a space");
    tstcheck(lexStatus("a = _") == LEX_ERROR, "_ at the end of a line");
    tstcheck(lexStatus("_+1") == LEX_ERROR, "_ followed by an operator");
  }

  // ----- integers ---------------------------------------------------------------------------

  tstcase("integer literals") {
    static const struct {
      const char *src;
      int value;
    } nums[] = {{"0", 0},
                {"00", 0},
                {"0_0", 0},
                {"0__0", 0},
                {"7", 7},
                {"42", 42},
                {"100", 100},
                {"1_000", 1000},
                {"1__0", 10},
                {"1_2_3", 123},
                {"2147483647", INT_MAX}};
    for (size_t i = 0; i < sizeof nums / sizeof nums[0]; i++) {
      LinkedTlist *l = lex(nums[i].src);
      tstexpect(tokSeq(l, TK_NUMBER, TK_EOF) && tokInt(l, 0, TK_NUMBER, nums[i].value),
                "integer '%s' is %d", nums[i].src, nums[i].value);
    }
  }

  tstcase("integers next to other tokens") {
    LinkedTlist *l = lex("12+3");
    tstcheck(tokSeq(l, TK_NUMBER, TK_PLUS, TK_NUMBER, TK_EOF), "12+3");
    tstcheck(tokInt(l, 0, TK_NUMBER, 12) && tokInt(l, 2, TK_NUMBER, 3), "values");
    tstcheck(tokSeq(lex("(1)"), TK_LPAREN, TK_NUMBER, TK_RPAREN, TK_EOF), "(1)");
    tstcheck(tokSeq(lex("0 1"), TK_NUMBER, TK_NUMBER, TK_EOF), "0 1");
    tstcheck(tokSeq(lex("0+0"), TK_NUMBER, TK_PLUS, TK_NUMBER, TK_EOF), "0+0");
  }

  tstcase("integers too large for int are a lexical error") {
    static const char *const big[] = {"2147483648",           "2147483649",       "3000000000",
                                      "4294967295",           "4294967296",       "9999999999",
                                      "99999999999999999999", "1_000_000_000_000"};
    for (size_t i = 0; i < sizeof big / sizeof big[0]; i++) {
      tstexpect(lexStatus(big[i]) == LEX_ERROR, "'%s' does not fit into int", big[i]);
    }
  }

  tstcase("malformed integers are a lexical error") {
    static const char *const bad[] = {"01", "007", "0_1",   "0_",  "1_",   "1__",
                                      "1a", "0a",  "12abc", "1_a", "0_0_1"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is a lexical error", bad[i]);
    }
  }

  // ----- decimals ---------------------------------------------------------------------------

  tstcase("decimal literals") {
    static const struct {
      const char *src;
      double value;
    } decs[] = {
        {"0.0", 0.0},    {"0.5", 0.5},     {"0.3", 0.3},       {"1.5", 1.5},
        {"3.14", 3.14},  {".5", 0.5},      {".25", 0.25},      {"1e3", 1000.0},
        {"1E3", 1000.0}, {"1e+3", 1000.0}, {"1e-2", 0.01},     {"2.5e2", 250.0},
        {"0e0", 0.0},    {"0e5", 0.0},     {"1_0.2_5", 10.25}, {"1e1_0", 1e10},
        {"1.5d", 1.5},   {"1.5D", 1.5},    {"1e2d", 100.0},    {"3000000000.5", 3000000000.5}};
    for (size_t i = 0; i < sizeof decs / sizeof decs[0]; i++) {
      LinkedTlist *l = lex(decs[i].src);
      tstexpect(tokSeq(l, TK_DECIMAL, TK_EOF) && tokDec(l, 0, decs[i].value), "decimal '%s' is %g",
                decs[i].src, decs[i].value);
    }
  }

  tstcase("decimals are exact: the nearest double, not a sum of powers of ten") {
    // 0.3 used to come out as 0.30000000000000004
    tstcheck(tokDec(lex("0.3"), 0, 0.3), "0.3");
    tstcheck(tokDec(lex("0.1"), 0, 0.1), "0.1");
    tstcheck(tokDec(lex("0.7"), 0, 0.7), "0.7");
    tstcheck(tokDec(lex("123.456"), 0, 123.456), "123.456");
    tstcheck(tokDec(lex("2.675"), 0, 2.675), "2.675");
  }

  tstcase("decimals at the limits of double") {
    // 1.7976931348623157e308 used to become inf and 4.9e-324 used to become 0
    tstcheck(tokDec(lex("1.7976931348623157e308"), 0, DBL_MAX), "largest double");
    tstcheck(tokDec(lex("2.2250738585072014e-308"), 0, DBL_MIN), "smallest normal double");
    tstcheck(tokDec(lex("4.9e-324"), 0, 4.9e-324), "smallest subnormal double");
    tstcheck(tokDec(lex("1e308"), 0, 1e308), "1e308");
    tstcheck(tokDec(lex("1e-300"), 0, 1e-300), "1e-300");
  }

  tstcase("the integer part of a decimal may exceed int") {
    // used to wrap around: 3000000000.5 came out as -1294967295.5
    tstcheck(tokDec(lex("3000000000.5"), 0, 3000000000.5), "3000000000.5");
    tstcheck(tokDec(lex("2147483648.0"), 0, 2147483648.0), "2147483648.0");
    tstcheck(tokDec(lex("99999999999.0"), 0, 99999999999.0), "99999999999.0");
    tstcheck(tokDec(lex("4294967296e2"), 0, 429496729600.0), "4294967296e2");
  }

  tstcase("a very long decimal is read completely") {
    char *src = repeat("0.", '3', 5000, "");
    tstcheck(tokDec(lex(src), 0, strtod(src, NULL)), "0.333... with 5000 digits");
    free(src);
  }

  tstcase("decimals next to other tokens") {
    LinkedTlist *l = lex("1.5+2");
    tstcheck(tokSeq(l, TK_DECIMAL, TK_PLUS, TK_NUMBER, TK_EOF), "1.5+2");
    tstcheck(tokDec(l, 0, 1.5) && tokInt(l, 2, TK_NUMBER, 2), "values");
    tstcheck(tokSeq(lex("(.5)"), TK_LPAREN, TK_DECIMAL, TK_RPAREN, TK_EOF), "(.5)");
    tstcheck(tokSeq(lex("1.5 2.5"), TK_DECIMAL, TK_DECIMAL, TK_EOF), "1.5 2.5");
    tstcheck(tokSeq(lex("a.5"), TK_IDENT, TK_DECIMAL, TK_EOF), "a.5 is an identifier and .5");
  }

  tstcase("a decimal and an integer are different tokens") {
    tstcheck(tokIs(lex("5"), 0, TK_NUMBER), "5 is a NUMBER");
    tstcheck(tokIs(lex("5.0"), 0, TK_DECIMAL), "5.0 is a DECIMAL");
    tstcheck(tokIs(lex("5e0"), 0, TK_DECIMAL), "5e0 is a DECIMAL");
    tstcheck(tokIs(lex("0e0"), 0, TK_DECIMAL), "0e0 is a DECIMAL");
  }

  tstcase("decimals out of range are a lexical error") {
    // used to silently become inf (or 0 for an overflowing exponent)
    static const char *const bad[] = {"1e309", "1e999", "1e2147483648", "9e99999999999", "1.8e308"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is out of range", bad[i]);
    }
  }

  tstcase("malformed decimals are a lexical error") {
    static const char *const bad[] = {"1.",   "0.",     "1.e5",   "1e",    "1e+",  "1e-",
                                      "1._5", "1e_5",   "1.5_",   "1.5x",  "1e5x", "1.5e",
                                      "0e",   "1.5e+x", "1.5ee3", "1__.5", "1.5__"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is a lexical error", bad[i]);
    }
  }

  tstcase("a d suffix after a leading dot does not produce an extra identifier", +bug) {
    // BUG: .5d lexes as DECIMAL IDENT(d). case '.' sets preloadedChar before numDecimal(), and
    // numDecimal() does not clear it when it consumes the d suffix, so the main loop reads the
    // stale 'd' again. 0.5d and 1.5d are fine.
    tstcheck(tokSeq(lex(".5d"), TK_DECIMAL, TK_EOF), ".5d");
    tstcheck(tokSeq(lex(".5D x"), TK_DECIMAL, TK_IDENT, TK_EOF), ".5D x");
  }

  // ----- single-line strings ----------------------------------------------------------------

  tstcase("single-line strings") {
    static const struct {
      const char *src;
      const char *text;
    } strs[] = {{"\"\"", ""},
                {"\"abc\"", "abc"},
                {"\"a b\"", "a b"},
                {"\"a'b\"", "a'b"},
                {"\"a\\nb\"", "a\nb"},
                {"\"\\t\"", "\t"},
                {"\"\\r\"", "\r"},
                {"\"\\\\\"", "\\"},
                {"\"\\\"\"", "\""},
                {"\"say \\\"hi\\\"\"", "say \"hi\""},
                {"\"// not a comment\"", "// not a comment"},
                {"\"/* nor this */\"", "/* nor this */"},
                {"\"x = 1 + 2\"", "x = 1 + 2"},
                {"\"h\xc3\xa9llo\"", "h\xc3\xa9llo"}};
    for (size_t i = 0; i < sizeof strs / sizeof strs[0]; i++) {
      LinkedTlist *l = lex(strs[i].src);
      tstexpect(tokSeq(l, TK_STRING, TK_EOF) && tokStr(l, 0, TK_STRING, strs[i].text), "string %s",
                esc(strs[i].src));
    }
  }

  tstcase("strings next to other tokens") {
    LinkedTlist *l = lex("\"a\" + \"b\"");
    tstcheck(tokSeq(l, TK_STRING, TK_PLUS, TK_STRING, TK_EOF), "\"a\" + \"b\"");
    tstcheck(tokStr(l, 0, TK_STRING, "a") && tokStr(l, 2, TK_STRING, "b"), "values");
    tstcheck(tokSeq(lex("\"a\"+\"b\""), TK_STRING, TK_PLUS, TK_STRING, TK_EOF), "no spaces");
    tstcheck(tokSeq(lex("f(\"x\")"), TK_IDENT, TK_LPAREN, TK_STRING, TK_RPAREN, TK_EOF),
             "f(\"x\")");
    tstcheck(tokSeq(lex("\"\" \"a\""), TK_STRING, TK_STRING, TK_EOF),
             "empty string, then a string");
    tstcheck(tokSeq(lex("\"a\"\n"), TK_STRING, TK_NEWLINE, TK_INDENT, TK_EOF),
             "string at end of line");
  }

  tstcase("a very long single-line string") {
    char *src = repeat("\"", 'b', 100000, "\"");
    LinkedTlist *l = lex(src);
    tstcheck(tokSeq(l, TK_STRING, TK_EOF), "one string token");
    tstcheck(tokRun(l, 0, TK_STRING, 'b', 100000), "all 100000 characters are there");
    free(src);
  }

  tstcase("invalid single-line strings are a lexical error") {
    static const char *const bad[] = {"\"",      "\"abc",  "\"abc\ndef\"", "\"abc\n",
                                      "\"\\q\"", "\"\\",   "\"a\\",        "\"\\x41\"",
                                      "\"a\"b",  "\"a\"1", "\"a\"Z",       "\"\\0\""};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  // ----- multi-line strings -----------------------------------------------------------------

  tstcase("multi-line strings") {
    static const struct {
      const char *src;
      const char *text;
    } strs[] = {{"\"\"\"abc\"\"\"", "abc"},
                {"\"\"\"a b\"\"\"", "a b"},
                {"\"\"\"\"\"\"", ""},
                {"\"\"\"a\nb\"\"\"", "a\nb"},
                {"\"\"\"\nabc\n\"\"\"", "\nabc\n"},
                {"\"\"\"a\"b\"\"\"", "a\"b"},
                {"\"\"\"a\"\"b\"\"\"", "a\"\"b"},
                {"\"\"\"a\"b\"\"c\"\"\"", "a\"b\"\"c"},
                {"\"\"\"x\"\n\"\"\"", "x\"\n"},
                {"\"\"\"// not a comment\"\"\"", "// not a comment"}};
    for (size_t i = 0; i < sizeof strs / sizeof strs[0]; i++) {
      LinkedTlist *l = lex(strs[i].src);
      tstexpect(tokSeq(l, TK_STRING, TK_EOF) && tokStr(l, 0, TK_STRING, strs[i].text), "string %s",
                esc(strs[i].src));
    }
  }

  tstcase("escape sequences are not processed in multi-line strings") {
    // the source is: """a\nb"""   (backslash and the letter n)
    LinkedTlist *l = lex("\"\"\"a\\nb\"\"\"");
    tstcheck(tokStr(l, 0, TK_STRING, "a\\nb"), "the backslash is kept as it is");
    tstcheck(tokText(l, 0, TK_STRING, "a\\nb", 4), "string is 4 characters long");
  }

  tstcase("a multi-line string can be followed by other tokens") {
    LinkedTlist *l = lex("\"\"\"a\"\"\" + 1");
    tstcheck(tokSeq(l, TK_STRING, TK_PLUS, TK_NUMBER, TK_EOF), "\"\"\"a\"\"\" + 1");
  }

  tstcase("a very long multi-line string") {
    char *src = repeat("\"\"\"", 'c', 100000, "\"\"\"");
    LinkedTlist *l = lex(src);
    tstcheck(tokSeq(l, TK_STRING, TK_EOF), "one string token");
    tstcheck(tokRun(l, 0, TK_STRING, 'c', 100000), "all 100000 characters are there");
    free(src);
  }

  tstcase("invalid multi-line strings are a lexical error") {
    static const char *const bad[] = {"\"\"\"abc",      "\"\"\"abc\"",    "\"\"\"abc\"\"",
                                      "\"\"\"",         "\"\"\"\"",       "\"\"\"\"\"",
                                      "\"\"\"a\"\"\"b", "\"\"\"a\"\"\"1", "\"\"\"a\nb\n"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  tstcase("line numbers after a multi-line string") {
    // """a<nl>b"""<nl>x : the string starts on line 1, x is on line 3
    LinkedTlist *l = lex("\"\"\"a\nb\"\"\"\nx");
    tstcheck(tokSeq(l, TK_STRING, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokLine(l, 0, 1), "the string token has the line where it starts");
    tstcheck(tokLine(l, 1, 2), "the newline after the string is on line 2");
    tstcheck(tokLine(l, 3, 3), "x is on line 3");
  }

  tstcase("line numbers when a newline follows a single quote inside a multi-line string") {
    // regression: a newline right after one quote character was not counted
    // line 1: """x"    line 2: y"""    line 3: foo
    LinkedTlist *l = lex("\"\"\"x\"\ny\"\"\"\nfoo");
    tstcheck(tokSeq(l, TK_STRING, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokStr(l, 0, TK_STRING, "x\"\ny"), "string content");
    tstcheck(tokLine(l, 3, 3), "foo is on line 3");
  }

  tstcase("line numbers when a newline follows two quotes inside a multi-line string") {
    // regression: a newline right after two quote characters was not counted
    // line 1: """x""   line 2: y   line 3: z"""   line 4: foo
    LinkedTlist *l = lex("\"\"\"x\"\"\ny\nz\"\"\"\nfoo");
    tstcheck(tokSeq(l, TK_STRING, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokStr(l, 0, TK_STRING, "x\"\"\ny\nz"), "string content");
    tstcheck(tokLine(l, 3, 4), "foo is on line 4");
  }

  tstcase("line numbers with several quote-newline combinations in one string") {
    // line 1: """a"   line 2: b""   line 3: c"   line 4: """   line 5: end
    LinkedTlist *l = lex("\"\"\"a\"\nb\"\"\nc\"\n\"\"\"\nend");
    tstcheck(tokSeq(l, TK_STRING, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokLine(l, 3, 5), "end is on line 5");
  }

  // ----- newlines, indentation, lines -------------------------------------------------------

  tstcase("a newline is followed by an indent token") {
    LinkedTlist *l = lex("a\nb");
    tstcheck(tokSeq(l, TK_IDENT, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "a NEWLINE INDENT b");
    tstcheck(tokInt(l, 2, TK_INDENT, 0), "no indentation is INDENT 0");
  }

  tstcase("the indent token holds the number of leading spaces") {
    tstcheck(tokInt(lex("a\n  b"), 2, TK_INDENT, 2), "2 spaces");
    tstcheck(tokInt(lex("a\n    b"), 2, TK_INDENT, 4), "4 spaces");
    tstcheck(tokInt(lex("a\n b"), 2, TK_INDENT, 1), "1 space");
    char *wide = repeat("a\n", ' ', 16, "b");
    tstcheck(tokInt(lex(wide), 2, TK_INDENT, 16), "16 spaces");
    free(wide);

    // tokens: a NEWLINE INDENT(1) b NEWLINE INDENT(3) c NEWLINE INDENT(0) d EOF
    LinkedTlist *l = lex("a\n b\n   c\nd");
    tstcheck(tokInt(l, 2, TK_INDENT, 1), "first indent is 1");
    tstcheck(tokInt(l, 5, TK_INDENT, 3), "second indent is 3");
    tstcheck(tokInt(l, 8, TK_INDENT, 0), "third indent is 0");
  }

  tstcase("spaces at the start of the file produce no indent token") {
    tstcheck(tokSeq(lex("   a"), TK_IDENT, TK_EOF), "only the identifier");
  }

  tstcase("a newline at the end of the file") {
    LinkedTlist *l = lex("a\n");
    tstcheck(tokSeq(l, TK_IDENT, TK_NEWLINE, TK_INDENT, TK_EOF), "NEWLINE and INDENT 0 before EOF");
    tstcheck(tokInt(l, 2, TK_INDENT, 0), "indent is 0");
  }

  tstcase("blank lines") {
    LinkedTlist *l = lex("a\n\nb");
    tstcheck(tokSeq(l, TK_IDENT, TK_NEWLINE, TK_INDENT, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF),
             "every newline gets its own NEWLINE and INDENT");
    tstcheck(tokSeq(lex("\n"), TK_NEWLINE, TK_INDENT, TK_EOF), "a lone newline");
    tstcheck(tokSeq(lex("\n\n"), TK_NEWLINE, TK_INDENT, TK_NEWLINE, TK_INDENT, TK_EOF), "two");
  }

  tstcase("line numbers of tokens") {
    LinkedTlist *l = lex("a\nb\nc");
    tstcheck(tokSeq(l, TK_IDENT, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_NEWLINE, TK_INDENT, TK_IDENT,
                    TK_EOF),
             "tokens");
    tstcheck(tokLine(l, 0, 1), "a is on line 1");
    tstcheck(tokLine(l, 1, 1), "the first newline belongs to line 1");
    tstcheck(tokLine(l, 2, 2), "the indent is on line 2");
    tstcheck(tokLine(l, 3, 2), "b is on line 2");
    tstcheck(tokLine(l, 4, 2), "the second newline belongs to line 2");
    tstcheck(tokLine(l, 5, 3), "the indent is on line 3");
    tstcheck(tokLine(l, 6, 3), "c is on line 3");
    tstcheck(tokLine(l, 7, 3), "EOF is on the last line");
  }

  // ----- comments ---------------------------------------------------------------------------

  tstcase("line comments") {
    tstcheck(tokSeq(lex("// comment"), TK_EOF), "comment at the end of the file");
    tstcheck(tokSeq(lex("x // comment"), TK_IDENT, TK_EOF), "after a token");
    tstcheck(tokSeq(lex("//"), TK_EOF), "empty comment");
    tstcheck(tokSeq(lex("x//c\ny"), TK_IDENT, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF),
             "a comment ends at the newline, which is still a token");
    tstcheck(tokSeq(lex("// a // b"), TK_EOF), "comment markers inside a comment");
    tstcheck(tokSeq(lex("// \"not a string"), TK_EOF), "a quote inside a comment");
    tstcheck(tokSeq(lex("// /* not a block"), TK_EOF), "a block opener inside a comment");
  }

  tstcase("a line comment does not swallow the next line") {
    LinkedTlist *l = lex("// one\nx");
    tstcheck(tokSeq(l, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokLine(l, 2, 2), "x is on line 2");
  }

  tstcase("block comments on one line") {
    tstcheck(tokSeq(lex("/* c */"), TK_EOF), "alone");
    tstcheck(tokSeq(lex("/**/"), TK_EOF), "empty");
    tstcheck(tokSeq(lex("a /* c */ b"), TK_IDENT, TK_IDENT, TK_EOF), "between tokens");
    tstcheck(tokSeq(lex("a/* c */b"), TK_IDENT, TK_IDENT, TK_EOF), "without spaces");
    tstcheck(tokSeq(lex("/* c */ x"), TK_IDENT, TK_EOF), "token after a one-line comment");
    tstcheck(tokSeq(lex("/* // */ x"), TK_IDENT, TK_EOF), "a line comment marker inside");
  }

  tstcase("nested block comments") {
    tstcheck(tokSeq(lex("/* a /* b */ c */ x"), TK_IDENT, TK_EOF), "two levels");
    tstcheck(tokSeq(lex("/* a /* b /* c */ d */ e */ x"), TK_IDENT, TK_EOF), "three levels");
  }

  tstcase("multi-line block comments count lines") {
    LinkedTlist *l = lex("/* one\ntwo */\nx");
    tstcheck(tokSeq(l, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokLine(l, 0, 2), "the newline after the comment is on line 2");
    tstcheck(tokLine(l, 2, 3), "x is on line 3");
  }

  tstcase("a multi-line block comment may be the last thing in the file") {
    // regression: these were rejected because only \n was accepted after the comment
    LinkedTlist *l = lex("/*\n*/");
    tstcheck(tokSeq(l, TK_EOF), "end of file right after */");
    tstcheck(tokLine(l, 0, 2), "EOF is on line 2");
    tstcheck(tokSeq(lex("/*\n*/   "), TK_EOF), "spaces after */, then the end of the file");
    tstcheck(tokSeq(lex("x /*\n*/"), TK_IDENT, TK_EOF), "after a token");
  }

  tstcase("a multi-line block comment may be followed by another comment") {
    // regression
    LinkedTlist *l = lex("/*\n*/ /* y */\nx");
    tstcheck(tokSeq(l, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "a block comment follows");
    tstcheck(tokLine(l, 2, 3), "x is on line 3");

    l = lex("/*\n*/ // y\nx");
    tstcheck(tokSeq(l, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "a line comment follows");
    tstcheck(tokLine(l, 2, 3), "x is on line 3");

    tstcheck(tokSeq(lex("/*\n*/ /*\n*/"), TK_EOF), "two multi-line comments");
    tstcheck(tokSeq(lex("/*\n*/    // y"), TK_EOF), "spaces, then a line comment at the end");
  }

  tstcase("a multi-line block comment followed by a newline") {
    tstcheck(tokSeq(lex("/*\n*/\n"), TK_NEWLINE, TK_INDENT, TK_EOF), "newline, then the end");
    tstcheck(tokSeq(lex("/*\n*/   \nx"), TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF),
             "spaces, then a newline");
  }

  tstcase("no token may follow a multi-line block comment on the same line") {
    static const char *const bad[] = {"/*\n*/x",  "/*\n*/ foo", "/*\n*/ / 2",
                                      "/*\n*/ +", "/*\n*/ 1",   "/*\n*/ \"s\"",
                                      "/*\n*/ (", "/*\n*/ /",   "/*\n*/ /x"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  tstcase("unterminated block comments are a lexical error") {
    static const char *const bad[] = {"/*", "/* abc", "/* abc\n", "/* a /* b */", "/*\n\n", "/* *"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  tstcase("a slash on its own is the division operator") {
    tstcheck(tokSeq(lex("a / b"), TK_IDENT, TK_DIVIDE, TK_IDENT, TK_EOF), "a / b");
    tstcheck(tokSeq(lex("a/b"), TK_IDENT, TK_DIVIDE, TK_IDENT, TK_EOF), "a/b");
    tstcheck(tokSeq(lex("6/2"), TK_NUMBER, TK_DIVIDE, TK_NUMBER, TK_EOF), "6/2");
    tstcheck(tokSeq(lex("a /"), TK_IDENT, TK_DIVIDE, TK_EOF), "slash at the end of the file");
  }

  // ----- invalid characters -----------------------------------------------------------------

  tstcase("characters that are not part of the language are a lexical error") {
    static const char *const bad[] = {"#",  "$",    "?",     "&", "~",  "`",     "\\",      "\t",
                                      "\r", "a\tb", "a\r\n", "!", "!a", "a ! b", "\xc3\xa9"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  tstcase("a lexical error is reported with exit status 1, not a crash") {
    tstcheck(lexStatus("#") == 1, "status is exactly 1");
    tstcheck(lexStatus("a + b") == 0, "valid input ends normally");
  }

  // ----- larger inputs ----------------------------------------------------------------------

  tstcase("a very long identifier") {
    char *src = repeat("", 'a', 100000, "");
    LinkedTlist *l = lex(src);
    tstcheck(tokSeq(l, TK_IDENT, TK_EOF), "one identifier");
    tstcheck(tokRun(l, 0, TK_IDENT, 'a', 100000), "all 100000 characters are there");
    free(src);
  }

  tstcase("strings and identifiers of every small length") {
    // the internal buffer grows while the text is read: check the sizes around its growth steps
    for (size_t n = 1; n <= 300; n++) {
      char *id = repeat("", 'q', n, "");
      char *str = repeat("\"", 'q', n, "\"");
      char *ml = repeat("\"\"\"", 'q', n, "\"\"\"");
      tstexpect(tokRun(lex(id), 0, TK_IDENT, 'q', n), "identifier of length %zu", n);
      tstexpect(tokRun(lex(str), 0, TK_STRING, 'q', n), "string of length %zu", n);
      tstexpect(tokRun(lex(ml), 0, TK_STRING, 'q', n), "multi-line string of length %zu", n);
      free(id);
      free(str);
      free(ml);
    }
  }

  tstcase("many tokens") {
    enum { N = 5000 };
    char *big = malloc(2 * N + 1);
    for (int i = 0; i < N; i++) {
      big[2 * i] = 'a';
      big[2 * i + 1] = ' ';
    }
    big[2 * N] = '\0';
    LinkedTlist *l = lex(big);
    tstcheck(tokCount(l) == N + 1, "N identifiers and EOF");
    tstcheck(tokIs(l, N, TK_EOF), "EOF is last");
    free(big);
  }

  // ----- putting it together ----------------------------------------------------------------

  tstcase("a small program") {
    LinkedTlist *l =
        lex("@main a = 1\n"
            "  b = a + 2.5 // comment\n"
            "  c = \"hi\" | (a != b)\n");
    tstcheck(tokSeq(l,
                    // line 1
                    TK_ATMAIN, TK_IDENT, TK_ASSIGN, TK_NUMBER, TK_NEWLINE,
                    // line 2
                    TK_INDENT, TK_IDENT, TK_ASSIGN, TK_IDENT, TK_PLUS, TK_DECIMAL, TK_NEWLINE,
                    // line 3
                    TK_INDENT, TK_IDENT, TK_ASSIGN, TK_STRING, TK_PIPE, TK_LPAREN, TK_IDENT,
                    TK_NOT_EQUAL, TK_IDENT, TK_RPAREN, TK_NEWLINE,
                    // the empty last line
                    TK_INDENT, TK_EOF),
             "token types");
    tstcheck(tokStr(l, 1, TK_IDENT, "a") && tokInt(l, 3, TK_NUMBER, 1), "first line values");
    tstcheck(tokInt(l, 5, TK_INDENT, 2) && tokDec(l, 10, 2.5), "second line values");
    tstcheck(tokStr(l, 15, TK_STRING, "hi") && tokInt(l, 12, TK_INDENT, 2), "third line values");
    tstcheck(tokLine(l, 0, 1) && tokLine(l, 4, 1), "line 1");
    tstcheck(tokLine(l, 5, 2) && tokLine(l, 11, 2), "line 2");
    tstcheck(tokLine(l, 12, 3) && tokLine(l, 18, 3), "line 3");
    tstcheck(tokLine(l, 23, 4) && tokLine(l, 24, 4), "line 4 (after the last newline)");
  }

  tstcase("one lexer run does not leak state into the next") {
    // the second run must not see anything of the first
    LinkedTlist *first = lex("a b c");
    LinkedTlist *second = lex("+");
    tstcheck(tokCount(first) == 4, "first list is intact");
    tstcheck(tokSeq(second, TK_PLUS, TK_EOF), "second list contains only its own tokens");
    tstcheck(tokLine(lex("x"), 0, 1), "line numbers start at 1 again");
  }
}
// AI-END
