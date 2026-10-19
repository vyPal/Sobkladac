#include "token.h"

struct Token *newToken(enum TkType type) {
    struct Token token;
    token.type = type;
    token.varType = None;
    token.varIndex = -1;
};

void setValue(struct Token *token, enum VarType varType, union Value value) {
    token->varType = varType;
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