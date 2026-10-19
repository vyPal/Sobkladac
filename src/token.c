/**
 * @file token.c
 * Project: IFJ26 compiler
 * @brief implementation of lexical token functions
 * @author Vojtěch Vítek <xvitekv00>
 * 
 * This file implements the functions for creating and manipulating lexical tokens.
 */

#include "token.h"

struct Token *newToken(enum TkType type) {
    struct Token token;
    token.type = type;
    token.varType = None;
    token.varIndex = -1;
    token.strLenght = -1;
    return &token;
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