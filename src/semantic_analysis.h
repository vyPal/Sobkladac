/**
 * @file semantic_analysis.h
 * @brief semantic analysis header
 * @author David Sobek <xsobekd00>
 *
 * Memory is managed by arena allocator.
 */

#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include <stdbool.h>

#include "ast.h"

/**
 * Performs semantic analysis on the AST.
 * Checks declarations and calls, validates types and return statements.
 *
 * @param ast Program AST to analyze.
 * @return true if semantic analysis succeeds, false otherwise.
 */
bool semanticAnalysis(AstProgram *ast);

#endif  // SEMANTIC_ANALYSIS_H
