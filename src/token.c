/**
 * @file token.c
 * Project: IFJ26 compiler
 * @brief implementation of lexical token functions
 * @author Vojtech Vitek <xvitekv00>
 * 
 * This file implements the functions for creating and manipulating lexical tokens.
 */

#include "token.h"
#include "common.c"

struct Token *newToken(enum TkType type) {
    struct Token *token = arena_alloc(sizeof(struct Token));
    token->type = type;
    token->value = NULL;
    return token;
}

void setValue(struct Token *token, union Value *value) {
    token->value = arena_alloc(sizeof(union Value));
    token->value = value;
}

enum TkType getTkType(struct Token *token) {
    return token->type;
}

union Value *getValue(struct Token *token) {
    return token->value;
}

struct String *newString(char *text, int length) {
    struct String *string =  arena_alloc(sizeof(struct String));
    string->string = text;
    string->length = length;

    return string;
}

char *getString(struct String *string) {
    return string->string;
}

int getStringLenght(struct String *string) {
    return string->length;
}

struct LinkedTlist *newTlist() {
    struct LinkedTlist *list = arena_alloc(sizeof(struct LinkedTlist));
    list->next = NULL;
    list->token = NULL;
    return list;
}

struct LinkedTlist *addTokenEnd(struct Token *token, struct LinkedTlist *last) {
    if (last == NULL) {
        last = newTlist();
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
        list->token = getNextToken(list);
        list->next = list->next->next;
    }
    else {
        list->token = NULL;
    }
    
    return returnTk;
}

struct Token *getNextToken(struct LinkedTlist *list) {
    if (list->next == NULL || list == NULL)
        return NULL;
    return list->next->token;
}