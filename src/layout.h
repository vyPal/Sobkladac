/**
 * @file layout.h
 * Project: IFJ26 compiler
 * @brief Layout filtering functions.
 * @author Jakub Palacky <xpalacj00>
 * @author Martin Stovicek <xstovim00>
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#include "token.h"

/**
 * @brief Applies indentation and line-break rules to a token list.
 * @param in Token list produced by the lexer; consumed during filtering.
 * @return A new token list with indentation and dedentation tokens.
 */
LinkedTlist *layoutFilter(LinkedTlist *in);

#endif  // LAYOUT_H
