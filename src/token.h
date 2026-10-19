enum TkType {
    Ident,      // *char
    Number,     // int
    Decimal,    // double
    String,     // *char
    // brackets
    Lbracket,   // (
    Rbracket,   // )
    // operators
    Plus,       // +
    Minus,      // -
    Multiply,   // *
    Divide,     // /
    Bigger,     // >
    Smaller,    // <
    Equal,      // ==
    Sequal,     // <=
    Bequal,     // >=
    NotEqual,   // !=
    // others
    Colon,      // :
    Semicolon,  // ;
    Comma,      // ,
    Assign,     // =
    Pipe,       // |
    NewLine,    // \n
    Indent,     // tab
    AtMain      // @main
} TkType;

enum VarType {
    None,
    Int,
    Double,
    String
} VarType;

union Value {
    int i;
    double d;
    char *string;
} Value;

struct Token {
    enum TkType type;
    enum VarType varType;
    int varIndex;
} Token;

struct Token *newToken(enum TkType type);

void setValue(struct Token *token, enum VarType varType, union Value value);

enum TkType getTkType(struct Token *token);

enum VarType getVarType(struct Token *token);

int getIndex(struct Token *token);
