/**
 * @file syn-analyzator-top-down.h
 * Project: IFJ26 compiler
 * @brief Implementation of top-down syntax analyzer
 * @author Martin Stovicek <xstovim00>
 *
 * Brief explanation:
 *  This file contains the implementation of the top-down syntax analyzer for the IFJ26 compiler.
 */

#ifndef TOP_DOWN_H
#define TOP_DOWN_H

#include "ast.h"
#include "token.h"

AstProgram *topDown(struct LinkedTlist *tokenList);

#endif  // TOP_DOWN_H
