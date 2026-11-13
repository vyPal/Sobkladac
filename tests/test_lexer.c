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
// Context: Uploaded tst.h (test framework), lexer.c, token.c, token.h, lexer.h.
// The tests were verified by compiling them
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

// tstcase() and tstsuite() take an optional, variadic list of tags. The tag list is always passed,
// empty if there are no tags, because omitting a variadic argument is not ISO C99 (-Wpedantic).

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
 * Lexes `text` in a child process (without the error message if `quiet`) and returns how it ended:
 *   0          lexing succeeded
 *   LEX_ERROR  the lexer called die(1, ...), i.e. reported a lexical error
 *   >= 128     the child crashed (128 + signal number)
 */
static int lexRun(const char *text, int quiet) {
  feed(text);
  fflush(NULL);
  pid_t pid = fork();
  if (pid < 0) {
    perror("test setup: fork");
    exit(2);
  }
  if (pid == 0) {
    // hide the error message that die() prints, unless the caller wants to see it
    if (freopen("/dev/null", "w", stdout) == NULL ||
        (quiet && freopen("/dev/null", "w", stderr) == NULL))
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

/// Same as lexRun(), without the error message of the lexer.
static int lexStatus(const char *text) {
  return lexRun(text, 1);
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
    fprintf(stderr, "      lexer says: ");
    fflush(stderr);
    lexRun(text, 0);  // run it once more to show the message that die() prints
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

/// Type of token n, or -1 if there is no such token.
static int tokType(LinkedTlist *l, int n) {
  Token *k = lookAheadToken(l, n);
  return k == NULL ? -1 : (int)getTkType(k);
}

/// Token n has the given type.
static int tokIs(LinkedTlist *l, int n, enum TkType type) {
  return tokType(l, n) == (int)type;
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

/// Token n is a TK_NUMBER with a non-negative value.
static int tokNonNegative(LinkedTlist *l, int n) {
  Token *k = lookAheadToken(l, n);
  return k != NULL && getTkType(k) == TK_NUMBER && getValue(k) != NULL && getValue(k)->i >= 0;
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

tstsuite("lexer", ) {
  // ----- general ----------------------------------------------------------------------------

  tstcase("empty input and spaces yield only EOF", ) {
    LinkedTlist *l = lex("");
    tstcheck(tokSeq(l, TK_EOF), "empty input");
    tstcheck(tokLine(l, 0, 1), "EOF is on line 1");
    tstcheck(tokSeq(lex("     "), TK_EOF), "spaces only");
  }

  // ----- operators and punctuation ----------------------------------------------------------

  tstcase("every operator and punctuation token", ) {
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

  tstcase("operators next to other tokens", ) {
    tstcheck(tokSeq(lex("a+b"), TK_IDENT, TK_PLUS, TK_IDENT, TK_EOF), "a+b");
    tstcheck(tokSeq(lex("a / b"), TK_IDENT, TK_DIVIDE, TK_IDENT, TK_EOF), "a / b");
    tstcheck(tokSeq(lex("f(a,b);"), TK_IDENT, TK_LPAREN, TK_IDENT, TK_COMMA, TK_IDENT, TK_RPAREN,
                    TK_SEMICOLON, TK_EOF),
             "f(a,b);");
    tstcheck(tokSeq(lex("a.b"), TK_IDENT, TK_DOT, TK_IDENT, TK_EOF), "a.b");
    tstcheck(tokSeq(lex("()"), TK_LPAREN, TK_RPAREN, TK_EOF), "the Unit literal () is two tokens");
    tstcheck(tokSeq(lex("==="), TK_EQUAL, TK_ASSIGN, TK_EOF), "two-character operators are greedy");
    tstcheck(tokSeq(lex("= ="), TK_ASSIGN, TK_ASSIGN, TK_EOF), "a space splits an operator");

    // a negative number is the unary minus and a positive literal
    LinkedTlist *l = lex("-5");
    tstcheck(tokSeq(l, TK_MINUS, TK_NUMBER, TK_EOF) && tokInt(l, 1, TK_NUMBER, 5), "-5");
  }

  // ----- @main ------------------------------------------------------------------------------

  tstcase("the @main annotation", ) {
    tstcheck(tokSeq(lex("@main def f"), TK_ATMAIN, TK_IDENT, TK_IDENT, TK_EOF),
             "on the same line as def");
    // specification, section 2: the annotation may also be on a line of its own
    tstcheck(
        tokSeq(lex("@main\ndef f"), TK_ATMAIN, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_IDENT, TK_EOF),
        "on a line of its own");
  }

  tstcase("a malformed @main annotation is a lexical error", ) {
    // @ is used only in @main, and nothing may be between @ and main
    static const char *const bad[] = {"@", "@x", "@mai", "@mainx", "@Main ", "@ main"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is a lexical error", bad[i]);
    }
  }

  // ----- identifiers and keywords -----------------------------------------------------------

  tstcase("identifiers keep their exact text", ) {
    static const char *const ids[] = {"a",  "Hello", "snake_case", "camelCase1", "x1",
                                      "_x", "__",    "_1",         "A_B_C",      "x_"};
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
      LinkedTlist *l = lex(ids[i]);
      tstexpect(tokSeq(l, TK_IDENT, TK_EOF) && tokStr(l, 0, TK_IDENT, ids[i]), "identifier '%s'",
                ids[i]);
    }
  }

  tstcase("keywords are single words with their exact text", ) {
    // token.h has no keyword tokens, so a keyword is a word (TK_IDENT) with the keyword as text.
    // The language is case-sensitive: If is not the keyword if.
    static const char *const words[] = {"def", "do",     "Double",  "else", "if",    "import",
                                        "Int", "return", "String",  "then", "Unit",  "val",
                                        "var", "while",  "Boolean", "case", "class", "extension",
                                        "for", "List",   "match",   "type", "If"};
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) {
      LinkedTlist *l = lex(words[i]);
      tstexpect(tokSeq(l, TK_IDENT, TK_EOF) && tokStr(l, 0, TK_IDENT, words[i]), "keyword '%s'",
                words[i]);
    }
  }

  tstcase("a lone underscore is not an identifier", ) {
    // the specification does not say whether this is a lexical error or a separate token
    static const char *const src[] = {"_", "_ ", "a = _"};
    for (size_t i = 0; i < sizeof src / sizeof src[0]; i++) {
      int ok = lexStatus(src[i]) == LEX_ERROR;
      if (!ok && lexStatus(src[i]) == 0) {
        LinkedTlist *l = lex(src[i]);
        ok = 1;
        for (int n = 0; lookAheadToken(l, n) != NULL; n++)
          if (tokStr(l, n, TK_IDENT, "_"))
            ok = 0;
      }
      tstexpect(ok, "'%s' does not produce the identifier _", src[i]);
    }
  }

  // ----- integers ---------------------------------------------------------------------------

  tstcase("integer literals", ) {
    static const struct {
      const char *src;
      int value;
    } nums[] = {{"0", 0},
                {"7", 7},
                {"42", 42},
                {"1_000", 1000},
                {"1_2_3", 123},
                {"1_000_000", 1000000},
                {"2147483647", INT_MAX}};
    for (size_t i = 0; i < sizeof nums / sizeof nums[0]; i++) {
      LinkedTlist *l = lex(nums[i].src);
      tstexpect(tokSeq(l, TK_NUMBER, TK_EOF) && tokInt(l, 0, TK_NUMBER, nums[i].value),
                "integer '%s' is %d", nums[i].src, nums[i].value);
    }
  }

  tstcase("malformed integer literals are a lexical error", ) {
    // a non-zero literal must not start with 0; _ is allowed only between two digits
    static const char *const bad[] = {"01", "007", "0_1", "0_0_1", "1_", "1__", "1_a"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is a lexical error", bad[i]);
    }
  }

  tstcase("an integer literal out of range is never lexed as a negative number", ) {
    // the specification does not test the behaviour on overflow, but a literal is never negative
    // (2147483648 used to wrap around to -2147483648)
    static const char *const big[] = {"2147483648", "3000000000", "99999999999999999999"};
    for (size_t i = 0; i < sizeof big / sizeof big[0]; i++) {
      int status = lexStatus(big[i]);
      tstexpect(status == LEX_ERROR || (status == 0 && tokNonNegative(lex(big[i]), 0)),
                "'%s' is an error or a non-negative number", big[i]);
    }
  }

  // ----- decimals ---------------------------------------------------------------------------

  tstcase("decimal literals", ) {
    // forms: [int].{frac}[exp] and {int}{exp}; the optional suffix d or D is allowed everywhere
    static const struct {
      const char *src;
      double value;
    } decs[] = {{"0.3", 0.3},     {"3.14", 3.14},   {".5", 0.5},
                {"1e3", 1000.0},  {"1E+3", 1000.0}, {"1e-2", 0.01},
                {"2.5e2", 250.0}, {"0e0", 0.0},     {"1_0.2_5", 10.25},
                {"1e1_0", 1e10},  {"1.5d", 1.5},    {"1.5D", 1.5},
                {"1e2d", 100.0},  {".5d", 0.5},     {"3000000000.5", 3000000000.5}};
    for (size_t i = 0; i < sizeof decs / sizeof decs[0]; i++) {
      LinkedTlist *l = lex(decs[i].src);
      tstexpect(tokSeq(l, TK_DECIMAL, TK_EOF) && tokDec(l, 0, decs[i].value), "decimal '%s' is %g",
                decs[i].src, decs[i].value);
    }
  }

  tstcase("decimals are exact and cover the whole range of double", ) {
    // 0.3 used to come out as 0.30000000000000004, DBL_MAX as inf and 4.9e-324 as 0
    tstcheck(tokDec(lex("0.1"), 0, 0.1), "0.1");
    tstcheck(tokDec(lex("0.7"), 0, 0.7), "0.7");
    tstcheck(tokDec(lex("123.456"), 0, 123.456), "123.456");
    tstcheck(tokDec(lex("1.7976931348623157e308"), 0, DBL_MAX), "largest double");
    tstcheck(tokDec(lex("2.2250738585072014e-308"), 0, DBL_MIN), "smallest normal double");
    tstcheck(tokDec(lex("4.9e-324"), 0, 4.9e-324), "smallest subnormal double");
  }

  tstcase("malformed decimal literals are a lexical error", ) {
    // the exponent needs at least one digit; _ is allowed only between two digits
    static const char *const bad[] = {"1e",     "1e+",  "1e-",  "1.5e",
                                      "1.5e+x", "1.5_", "1e_5", "1__.5"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is a lexical error", bad[i]);
    }
  }

  tstcase("decimal literals out of range are a lexical error", ) {
    // used to silently become inf (or 0 for an overflowing exponent)
    static const char *const bad[] = {"1e309", "1e999", "1e2147483648"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "'%s' is out of range", bad[i]);
    }
  }

  // ----- strings ----------------------------------------------------------------------------

  tstcase("single-line strings", ) {
    // escape sequences: \\ \" \n \r \t
    static const struct {
      const char *src;
      const char *text;
    } strs[] = {{"\"\"", ""},
                {"\"abc\"", "abc"},
                {"\"a b\"", "a b"},
                {"\"a\\nb\"", "a\nb"},
                {"\"\\t\"", "\t"},
                {"\"\\r\"", "\r"},
                {"\"\\\\\"", "\\"},
                {"\"\\\"\"", "\""},
                {"\"say \\\"hi\\\"\"", "say \"hi\""},
                {"\"// not a comment\"", "// not a comment"},
                {"\"/* nor this */\"", "/* nor this */"}};
    for (size_t i = 0; i < sizeof strs / sizeof strs[0]; i++) {
      LinkedTlist *l = lex(strs[i].src);
      tstexpect(tokSeq(l, TK_STRING, TK_EOF) && tokStr(l, 0, TK_STRING, strs[i].text), "string %s",
                esc(strs[i].src));
    }
    tstcheck(tokSeq(lex("\"a\" + \"b\""), TK_STRING, TK_PLUS, TK_STRING, TK_EOF), "\"a\" + \"b\"");
    tstcheck(tokSeq(lex("f(\"x\")"), TK_IDENT, TK_LPAREN, TK_STRING, TK_RPAREN, TK_EOF),
             "f(\"x\")");
  }

  tstcase("invalid single-line strings are a lexical error", ) {
    // unterminated, a real newline inside, an unsupported escape sequence
    static const char *const bad[] = {"\"",      "\"abc",     "\"abc\n", "\"abc\ndef\"",
                                      "\"\\q\"", "\"\\x41\"", "\"\\0\"", "\"a\\"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  tstcase("multi-line strings", ) {
    // everything between the triple quotes is kept literally, escape sequences are not evaluated
    static const struct {
      const char *src;
      const char *text;
    } strs[] = {{"\"\"\"abc\"\"\"", "abc"},
                {"\"\"\"\"\"\"", ""},
                {"\"\"\"a\nb\"\"\"", "a\nb"},
                {"\"\"\"\nabc\n\"\"\"", "\nabc\n"},
                {"\"\"\"a\"b\"\"\"", "a\"b"},
                {"\"\"\"a\"\"b\"\"\"", "a\"\"b"},
                {"\"\"\"x\"\n\"\"\"", "x\"\n"},
                {"\"\"\"a\\nb\"\"\"", "a\\nb"},
                // the example from the specification, section 1.1
                {"\"\"\"A\nhoj s\\nvete\n\"\"\"", "A\nhoj s\\nvete\n"}};
    for (size_t i = 0; i < sizeof strs / sizeof strs[0]; i++) {
      LinkedTlist *l = lex(strs[i].src);
      tstexpect(tokSeq(l, TK_STRING, TK_EOF) && tokStr(l, 0, TK_STRING, strs[i].text), "string %s",
                esc(strs[i].src));
    }
  }

  tstcase("unterminated multi-line strings are a lexical error", ) {
    static const char *const bad[] = {"\"\"\"abc", "\"\"\"abc\"", "\"\"\"abc\"\"", "\"\"\"",
                                      "\"\"\"\""};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  tstcase("line numbers after multi-line strings", ) {
    // the string token has the line where it starts, x is on line 3
    LinkedTlist *l = lex("\"\"\"a\nb\"\"\"\nx");
    tstcheck(tokSeq(l, TK_STRING, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokLine(l, 0, 1) && tokLine(l, 3, 3), "the string is on line 1, x on line 3");

    // regression: a newline right after one quote character was not counted
    l = lex("\"\"\"x\"\ny\"\"\"\nfoo");
    tstcheck(tokStr(l, 0, TK_STRING, "x\"\ny") && tokLine(l, 3, 3), "newline after one quote");

    // regression: a newline right after two quote characters was not counted
    l = lex("\"\"\"x\"\"\ny\nz\"\"\"\nfoo");
    tstcheck(tokStr(l, 0, TK_STRING, "x\"\"\ny\nz") && tokLine(l, 3, 4),
             "newline after two quotes");
  }

  // ----- newlines and indentation -----------------------------------------------------------

  tstcase("a newline is followed by an indent token with the number of spaces", ) {
    LinkedTlist *l = lex("a\nb");
    tstcheck(tokSeq(l, TK_IDENT, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "a NEWLINE INDENT b");
    tstcheck(tokInt(l, 2, TK_INDENT, 0), "no indentation is INDENT 0");
    tstcheck(tokInt(lex("a\n  b"), 2, TK_INDENT, 2), "2 spaces");
    tstcheck(tokInt(lex("a\n    b"), 2, TK_INDENT, 4), "4 spaces");

    l = lex("a\n b\n   c\nd");
    tstcheck(tokInt(l, 2, TK_INDENT, 1) && tokInt(l, 5, TK_INDENT, 3) && tokInt(l, 8, TK_INDENT, 0),
             "indents 1, 3 and 0 on following lines");

    tstcheck(tokSeq(lex("   a"), TK_IDENT, TK_EOF), "spaces at the start of the file: no indent");
    tstcheck(tokSeq(lex("a\n"), TK_IDENT, TK_NEWLINE, TK_INDENT, TK_EOF), "newline at the end");
  }

  tstcase("tokens know their line numbers", ) {
    LinkedTlist *l = lex("a\nb\nc");
    tstcheck(tokLine(l, 0, 1) && tokLine(l, 1, 1), "a and its newline are on line 1");
    tstcheck(tokLine(l, 2, 2) && tokLine(l, 3, 2) && tokLine(l, 4, 2), "indent, b and newline: 2");
    tstcheck(tokLine(l, 5, 3) && tokLine(l, 6, 3) && tokLine(l, 7, 3), "indent, c and EOF: 3");
  }

  tstcase("a tab at the start of a non-empty line is a lexical error", ) {
    static const char *const bad[] = {"\tx", "a\n\tx", "a\n  \tx"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  // ----- comments ---------------------------------------------------------------------------

  tstcase("line comments", ) {
    tstcheck(tokSeq(lex("// comment"), TK_EOF), "alone, at the end of the file");
    tstcheck(tokSeq(lex("x // comment"), TK_IDENT, TK_EOF), "after a token");
    tstcheck(tokSeq(lex("// \"x /* y"), TK_EOF), "quotes and block openers inside are ignored");

    // a comment ends at the end of the line; the newline is still a token
    LinkedTlist *l = lex("x//c\ny");
    tstcheck(tokSeq(l, TK_IDENT, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "tokens");
    tstcheck(tokLine(l, 3, 2), "y is on line 2");
  }

  tstcase("block comments on one line separate tokens and may be nested", ) {
    tstcheck(tokSeq(lex("/* c */"), TK_EOF), "alone");
    tstcheck(tokSeq(lex("a/* c */b"), TK_IDENT, TK_IDENT, TK_EOF),
             "a comment separates identifiers");
    tstcheck(tokSeq(lex("1/**/2"), TK_NUMBER, TK_NUMBER, TK_EOF), "a comment separates numbers");
    tstcheck(tokSeq(lex("/* // */ x"), TK_IDENT, TK_EOF), "a line comment marker inside");
    tstcheck(tokSeq(lex("/* a /* b */ c */ x"), TK_IDENT, TK_EOF), "nested comment");
  }

  tstcase("a multi-line block comment acts as an end of line", ) {
    LinkedTlist *l = lex("/* one\ntwo */\nx");
    tstcheck(tokSeq(l, TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF), "one end of line, not two");
    tstcheck(tokLine(l, 0, 2) && tokLine(l, 2, 3), "lines inside the comment are counted");

    // regression: these were rejected because only \n was accepted after the comment
    l = lex("/*\n*/");
    tstcheck(tokSeq(l, TK_EOF) && tokLine(l, 0, 2), "end of file right after */");
    tstcheck(tokSeq(lex("/*\n*/   "), TK_EOF), "spaces after */, then the end of the file");
    tstcheck(tokSeq(lex("x /*\n*/"), TK_IDENT, TK_EOF), "after a token");
    tstcheck(tokSeq(lex("/*\n*/   \nx"), TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF),
             "spaces, newline");
    tstcheck(tokSeq(lex("/*\n*/ /* y */\nx"), TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF),
             "another block comment may follow");
    tstcheck(tokSeq(lex("/*\n*/ // y\nx"), TK_NEWLINE, TK_INDENT, TK_IDENT, TK_EOF),
             "a line comment may follow");
  }

  tstcase("no token may follow a multi-line block comment on the same line", ) {
    static const char *const bad[] = {"/*\n*/x",  "/*\n*/ foo", "/*\n*/ / 2",
                                      "/*\n*/ +", "/*\n*/ 1",   "/*\n*/ \"s\""};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  tstcase("unterminated block comments are a lexical error", ) {
    static const char *const bad[] = {"/*", "/* abc", "/* a /* b */", "/*\n\n"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == LEX_ERROR, "%s is a lexical error", esc(bad[i]));
    }
  }

  // ----- unknown characters -----------------------------------------------------------------

  tstcase("unknown characters are a lexical error with exit status 1", ) {
    // braces are not part of IFJ26, nor are backticks (quoted identifiers)
    static const char *const bad[] = {"#", "$", "?", "&", "~", "`", "\\", "^", "'", "{", "}"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      tstexpect(lexStatus(bad[i]) == 1, "'%s' ends with exit status 1", bad[i]);
    }
    tstcheck(lexStatus("a + b") == 0, "valid input ends normally");
  }

  // ----- extension BOOLTHEN ----------------------------------------------------------------

  tstcase("BOOLTHEN: !, && and || are single tokens", ) {
    // Specification, section 5.2. The names of the token types of the extension are not used
    // here: the tests only check that each of them is one token that differs from the others
    // and from the tokens of the base language.
    static const char *const one[] = {"!", "&&", "||", "true", "false"};
    for (size_t i = 0; i < sizeof one / sizeof one[0]; i++) {
      LinkedTlist *l = lex(one[i]);
      tstexpect(tokCount(l) == 2 && tokIs(l, 1, TK_EOF), "'%s' is one token", one[i]);
    }

    tstcheck(TK_NOT >= 0 && TK_AND >= 0 && TK_OR >= 0 && TK_NOT != TK_AND && TK_NOT != TK_OR &&
                 TK_AND != TK_OR,
             "!, && and || are three different tokens");
    tstcheck(TK_OR != (int)TK_PIPE && TK_NOT != (int)TK_NOT_EQUAL, "|| is not a pipe, ! is not !=");

    // the tokens of the base language are not affected
    tstcheck(tokSeq(lex("!="), TK_NOT_EQUAL, TK_EOF), "!= is still one token");
    tstcheck(tokSeq(lex("|"), TK_PIPE, TK_EOF), "a single | is still a pipe");
    tstcheck(lexStatus("a & b") == LEX_ERROR, "a single & is still an unknown character");

    // in expressions, with and without spaces
    tstcheck(
        tokSeq(lex("!a && b || c"), TK_NOT, TK_IDENT, TK_AND, TK_IDENT, TK_OR, TK_IDENT, TK_EOF),
        "!a && b || c");
    tstcheck(tokSeq(lex("x&&y"), TK_IDENT, TK_AND, TK_IDENT, TK_EOF), "x&&y");
    tstcheck(
        tokSeq(lex("!(a<b)"), TK_NOT, TK_LPAREN, TK_IDENT, TK_SMALLER, TK_IDENT, TK_RPAREN, TK_EOF),
        "!(a<b)");
  }

  // ----- larger inputs and a whole program --------------------------------------------------

  tstcase("long identifiers and strings", ) {
    // the buffer grows while the text is read: very long texts and all lengths around its steps
    char *id = repeat("", 'a', 100000, "");
    char *str = repeat("\"", 'b', 100000, "\"");
    char *ml = repeat("\"\"\"", 'c', 100000, "\"\"\"");
    tstcheck(tokRun(lex(id), 0, TK_IDENT, 'a', 100000), "identifier of 100000 characters");
    tstcheck(tokRun(lex(str), 0, TK_STRING, 'b', 100000), "string of 100000 characters");
    tstcheck(tokRun(lex(ml), 0, TK_STRING, 'c', 100000), "multi-line string of 100000 characters");
    free(id);
    free(str);
    free(ml);

    for (size_t n = 1; n <= 200; n++) {
      id = repeat("", 'q', n, "");
      str = repeat("\"", 'q', n, "\"");
      ml = repeat("\"\"\"", 'q', n, "\"\"\"");
      tstexpect(tokRun(lex(id), 0, TK_IDENT, 'q', n), "identifier of length %zu", n);
      tstexpect(tokRun(lex(str), 0, TK_STRING, 'q', n), "string of length %zu", n);
      tstexpect(tokRun(lex(ml), 0, TK_STRING, 'q', n), "multi-line string of length %zu", n);
      free(id);
      free(str);
      free(ml);
    }
  }

  tstcase("the example program from the specification", ) {
    // section 2 of the specification (the comments are shortened to ASCII)
    LinkedTlist *l =
        lex("import ifj26.*\n"
            "def factorial(n: Int): Int =\n"
            "  var result = 1\n"
            "  var i: Int = n // the type is optional\n"
            "  while i > 1 do\n"
            "    result = result * i\n"
            "    i = i - 1\n"
            "  result // the last expression is the result\n"
            "@main def runFoo(): Unit =\n"
            "  val n = readInt()\n"
            "  val result = factorial(n)\n"
            "  print(result)\n"
            "  print(\"\\n\")\n");
    tstcheck(
        tokSeq(l,
               // import ifj26.*
               TK_IDENT, TK_IDENT, TK_DOT, TK_MULTIPLY, TK_NEWLINE,
               // def factorial(n: Int): Int =
               TK_INDENT, TK_IDENT, TK_IDENT, TK_LPAREN, TK_IDENT, TK_COLON, TK_IDENT, TK_RPAREN,
               TK_COLON, TK_IDENT, TK_ASSIGN, TK_NEWLINE,
               // var result = 1
               TK_INDENT, TK_IDENT, TK_IDENT, TK_ASSIGN, TK_NUMBER, TK_NEWLINE,
               // var i: Int = n
               TK_INDENT, TK_IDENT, TK_IDENT, TK_COLON, TK_IDENT, TK_ASSIGN, TK_IDENT, TK_NEWLINE,
               // while i > 1 do
               TK_INDENT, TK_IDENT, TK_IDENT, TK_BIGGER, TK_NUMBER, TK_IDENT, TK_NEWLINE,
               // result = result * i
               TK_INDENT, TK_IDENT, TK_ASSIGN, TK_IDENT, TK_MULTIPLY, TK_IDENT, TK_NEWLINE,
               // i = i - 1
               TK_INDENT, TK_IDENT, TK_ASSIGN, TK_IDENT, TK_MINUS, TK_NUMBER, TK_NEWLINE,
               // result
               TK_INDENT, TK_IDENT, TK_NEWLINE,
               // @main def runFoo(): Unit =
               TK_INDENT, TK_ATMAIN, TK_IDENT, TK_IDENT, TK_LPAREN, TK_RPAREN, TK_COLON, TK_IDENT,
               TK_ASSIGN, TK_NEWLINE,
               // val n = readInt()
               TK_INDENT, TK_IDENT, TK_IDENT, TK_ASSIGN, TK_IDENT, TK_LPAREN, TK_RPAREN, TK_NEWLINE,
               // val result = factorial(n)
               TK_INDENT, TK_IDENT, TK_IDENT, TK_ASSIGN, TK_IDENT, TK_LPAREN, TK_IDENT, TK_RPAREN,
               TK_NEWLINE,
               // print(result)
               TK_INDENT, TK_IDENT, TK_LPAREN, TK_IDENT, TK_RPAREN, TK_NEWLINE,
               // print("\n")
               TK_INDENT, TK_IDENT, TK_LPAREN, TK_STRING, TK_RPAREN, TK_NEWLINE,
               // the empty last line
               TK_INDENT, TK_EOF),
        "token types");
    tstcheck(tokStr(l, 0, TK_IDENT, "import") && tokStr(l, 1, TK_IDENT, "ifj26"), "prolog words");
    tstcheck(tokInt(l, 38, TK_INDENT, 4), "the body of while is indented by 4");
    tstcheck(tokStr(l, 91, TK_STRING, "\n"), "the string \"\\n\" is one newline character");
    tstcheck(tokLine(l, 55, 9) && tokLine(l, 95, 14), "@main is on line 9, EOF on line 14");
  }
}
// AI-END
