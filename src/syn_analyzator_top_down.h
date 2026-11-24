/**
 * @file syn_analyzator_top_down.h
 * Project: IFJ26 compiler
 * @brief Declarations for the top-down syntax analyzer
 * @author Martin Stovicek <xstovim00>
 *
 * This file contains the declarations of the top-down syntax analyzer functions.
 */

#ifndef TOP_DOWN_H
#define TOP_DOWN_H

#include "ast.h"
#include "syn_analyzator_bottom_up.h"
#include "token.h"

// AI-BEGIN [gpt-6-luna]
// Prompt: "Create fun declarations in the top down.h inspired by ast.h"
// Context: ast.h
/**
 * @brief Parses a program and builds its AST.
 * @param tokenList Token list to parse.
 * @return The parsed program.
 */
AstProgram *topDown(struct LinkedTlist *tokenList);

/**
 * @brief Parses the required import prolog.
 * @param tokenList Token list to parse.
 * @return true if the prolog is valid; otherwise false.
 */
bool prolog(LinkedTlist *tokenList);

/**
 * @brief Parses all function definitions in a program.
 * @param tokenList Token list to parse.
 * @param asTree Program AST to append parsed functions to.
 * @return true if function definitions were parsed; otherwise false.
 */
bool funDefs(LinkedTlist *tokenList, AstProgram *asTree);

/**
 * @brief Parses the remaining function definitions.
 * @param tokenList Token list to parse.
 * @param asTree Program AST to append parsed functions to.
 * @return true if the remaining input is valid; otherwise false.
 */
bool funDefsTail(LinkedTlist *tokenList, AstProgram *asTree);

/**
 * @brief Parses one function definition.
 * @param tokenList Token list to parse.
 * @param asTree Program AST to append the function to.
 * @return true if a function definition was parsed; otherwise false.
 */
bool funDef(LinkedTlist *tokenList, AstProgram *asTree);

/**
 * @brief Parses the main-function annotation.
 * @param tokenList Token list to parse.
 * @return true if the annotation is present; otherwise false.
 */
bool mainAnnot(LinkedTlist *tokenList);

/**
 * @brief Consumes the newline after the main-function annotation.
 * @param tokenList Token list to parse.
 */
void mainNL(LinkedTlist *tokenList);

/**
 * @brief Parses a function body.
 * @param function Function whose body is being parsed.
 * @param tokenList Token list to parse.
 * @return true if the function body was parsed; otherwise false.
 */
bool funBody(AstFunction *function, LinkedTlist *tokenList);

/**
 * @brief Parses a function's parameter list.
 * @param function Function to append parameters to.
 * @param tokenList Token list to parse.
 * @return true if the parameter list was parsed; otherwise false.
 */
bool params(AstFunction *function, LinkedTlist *tokenList);

/**
 * @brief Parses the remaining parameters in a function's parameter list.
 * @param function Function to append parameters to.
 * @param tokenList Token list to parse.
 * @return true if the remaining parameters were parsed; otherwise false.
 */
bool paramsTail(AstFunction *function, LinkedTlist *tokenList);

/**
 * @brief Parses one function parameter.
 * @param function Function to append the parameter to.
 * @param tokenList Token list to parse.
 * @return true if a parameter was parsed; otherwise false.
 */
bool param(AstFunction *function, LinkedTlist *tokenList);

/**
 * @brief Parses an optional data type annotation.
 * @param tokenList Token list to parse.
 * @return The parsed data type, or UNKNOWN if no annotation is present.
 */
AstDataType optType(LinkedTlist *tokenList);

/**
 * @brief Parses a required data type.
 * @param tokenList Token list to parse.
 * @return The parsed data type, or UNKNOWN if it is invalid.
 */
AstDataType dType(LinkedTlist *tokenList);

/**
 * @brief Parses a code block.
 * @param block Code block to append parsed commands to.
 * @param tokenList Token list to parse.
 * @return true if the code block was parsed; otherwise false.
 */
bool codeBlock(AstCodeBlock *block, LinkedTlist *tokenList);

/**
 * @brief Parses the remaining lines in a code block.
 * @param block Code block to append parsed commands to.
 * @param tokenList Token list to parse.
 * @return true if the remaining lines were parsed; otherwise false.
 */
bool linesTail(AstCodeBlock *block, LinkedTlist *tokenList);

/**
 * @brief Parses one line in a code block.
 * @param block Code block to append the parsed command to.
 * @param tokenList Token list to parse.
 * @return true if a line was parsed; otherwise false.
 */
bool line(AstCodeBlock *block, LinkedTlist *tokenList);

/**
 * @brief Parses a semicolon-separated sequence of simple statements.
 * @param block Code block to append statements to.
 * @param tokenList Token list to parse.
 * @return true if the statement sequence was parsed; otherwise false.
 */
bool simpleTail(AstCodeBlock *block, LinkedTlist *tokenList);

/**
 * @brief Parses one simple statement.
 * @param block Code block to append the parsed statement to.
 * @param tokenList Token list to parse.
 * @return true if a statement was parsed; otherwise false.
 */
bool simpleStatement(AstCodeBlock *block, LinkedTlist *tokenList);

/**
 * @brief Parses an assignment.
 * @param cmd Assignment command to populate.
 * @param tokenList Token list to parse.
 * @return true if the assignment was parsed; otherwise false.
 */
bool assignment(AstCommand *cmd, LinkedTlist *tokenList);

/**
 * @brief Parses a function call.
 * @param tokenList Token list to parse.
 * @return The parsed function call.
 */
AstFunctionCall *functionCall(LinkedTlist *tokenList);

/**
 * @brief Parses a variable declaration.
 * @param cmd Variable declaration command to populate.
 * @param tokenList Token list to parse.
 * @return true if the declaration was parsed; otherwise false.
 */
bool variableDeclaration(AstCommand *cmd, LinkedTlist *tokenList);

/**
 * @brief Parses a val or var keyword.
 * @param tokenList Token list to parse.
 * @param isMutable Set to true for var and false for val.
 * @return true if a val or var keyword was found; otherwise false.
 */
bool varKW(LinkedTlist *tokenList, bool *isMutable);

/**
 * @brief Parses a return statement.
 * @param cmd Return command to populate.
 * @param tokenList Token list to parse.
 * @return true if the return statement was parsed; otherwise false.
 */
bool returnStatement(AstCommand *cmd, LinkedTlist *tokenList);

/**
 * @brief Parses the optional value following a return keyword.
 * @param cmd Return command to populate.
 * @param tokenList Token list to parse.
 * @return true if the return value was parsed; otherwise false.
 */
bool returnTail(AstCommand *cmd, LinkedTlist *tokenList);

/**
 * @brief Parses a value.
 * @param tokenList Token list to parse.
 * @return The parsed value.
 */
AstValue *rValue(LinkedTlist *tokenList);

/**
 * @brief Parses a function call's argument list.
 * @param funCall Function call to append arguments to.
 * @param tokenList Token list to parse.
 * @return true if the argument list was parsed; otherwise false.
 */
bool args(AstFunctionCall *funCall, LinkedTlist *tokenList);

/**
 * @brief Parses the remaining arguments in a function call.
 * @param funCall Function call to append arguments to.
 * @param tokenList Token list to parse.
 * @return true if the remaining arguments were parsed; otherwise false.
 */
bool argsTail(AstFunctionCall *funCall, LinkedTlist *tokenList);

/**
 * @brief Parses an expression term.
 * @param tokenList Token list to parse.
 * @return The parsed expression term.
 */
AstExpression *term(LinkedTlist *tokenList);

/**
 * @brief Parses an if statement.
 * @param block Code block to append the parsed statement to.
 * @param tokenList Token list to parse.
 * @return true if the statement was parsed; otherwise false.
 */
bool ifStatement(AstCodeBlock *block, LinkedTlist *tokenList);

/**
 * @brief Parses a while statement.
 * @param block Code block to append the parsed statement to.
 * @param tokenList Token list to parse.
 * @return true if the statement was parsed; otherwise false.
 */
bool whileStatement(AstCodeBlock *block, LinkedTlist *tokenList);
// AI-END

#endif  // TOP_DOWN_H
