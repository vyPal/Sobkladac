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
    struct Token *token = arena_alloc(sizeof(Token));
    token->type = type;
    token->varType = None;
    token->varIndex = -1;
    token->strLenght = -1;
    return token;
};

void setValue(struct Token *token, enum VarType varType, union Value value) {
    token->varType = varType;
    // hash value and set the index
};

void setValueString(struct Token *token, enum VarType varType, union Value value, int strLenght) {
    token->varType = varType;
    token->strLenght = strLenght;
    // hash value and set the index
};

enum TkType getTkType(struct Token *token) {
    return token->type;
};

enum VarType getVarType(struct Token *token) {
    return token->varType;
};

int getIndex(struct Token *token) {
    return token->varIndex;
};

int getStrLenght(struct Token *token) {
    return token->strLenght;
};

struct LinkedTlist *newTlist() {
    struct LinkedTlist *list = arena_alloc(sizeof(LinkedTlist));
    list->next = NULL;
    list->token = NULL;
    return list;
};

struct LinkedTlist *addTokenEnd(struct Token *token, struct LinkedTlist *last) {
    if (last == NULL) {
        last = newTlist();
        last->token = token;
        return last;
    }

    last->next = newTlist();
    last->next->token = token;
    return last->next;
};

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
};

struct Token *getNextToken(struct LinkedTlist *list) {
    if (list->next == NULL || list == NULL)
        return NULL;
    return list->next->token;
};