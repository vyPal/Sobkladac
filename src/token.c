/**
 * @file token.c
 * Project: IFJ26 compiler
 * @brief implementation of lexical token functions
 * @author Vojtech Vitek <xvitekv00>
 *
 * This file implements the functions for creating and manipulating lexical tokens.
 */

#include "token.h"

#include <stddef.h>

#include "common.h"

typedef struct Token {
  enum TkType type;
  union Value *value;
  int line;
} Token;

typedef struct LinkedTlist {
  struct Token *token;
  struct LinkedTlist *next;
} LinkedTlist;

struct Token *newToken(enum TkType type, int line) {
  struct Token *token = arena_alloc(sizeof(struct Token));
  token->type = type;
  token->value = NULL;
  token->line = line;
  return token;
}

void setValue(struct Token *token, union Value *value) {
  token->value = value;
}

enum TkType getTkType(const struct Token *token) {
  return token->type;
}

union Value *getValue(const struct Token *token) {
  return token->value;
}

int getLine(const struct Token *token) {
  return token->line;
}

// STRING FUNCTIONS

struct String *newString(const char *text, size_t length) {
  struct String *string = arena_alloc(sizeof(struct String));
  string->string = text;
  string->length = length;

  return string;
}

char *getString(const struct String *string) {
  return string->string;
}

size_t getStringLength(const struct String *string) {
  return string->length;
}

// LIST FUNCTIONS

struct LinkedTlist *newTlist(void) {
  struct LinkedTlist *list = arena_alloc(sizeof(struct LinkedTlist));
  list->next = NULL;
  list->token = NULL;
  return list;
}

struct LinkedTlist *pushToken(struct Token *token, struct LinkedTlist *last) {
  if (last == NULL) {
    last = newTlist();
    last->token = token;
    return last;
  }

  if (last->token == NULL) {
    last->token = token;
    return last;
  }

  last->next = newTlist();
  last->next->token = token;
  return last->next;
}

struct Token *popToken(struct LinkedTlist *list) {
  if (list == NULL)
    return NULL;

  struct Token *returnTk = list->token;

  if (list->next != NULL) {
    list->token = list->next->token;
    list->next = list->next->next;
  } else {
    list->token = NULL;
  }

  return returnTk;
}

struct Token *lookAheadToken(struct LinkedTlist *list, int n) {
  if (list == NULL || n < 0)
    return NULL;

  for (int i = 0; i < n && list != NULL; i++) {
    list = list->next;
  }

  if (list == NULL)
    return NULL;

  return list->token;
}
