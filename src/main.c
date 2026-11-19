/**
 * @file main.c
 * Project: IFJ26 compiler
 * @author Jakub Palacky <xpalacj00>
 */

#include <stdio.h>

#include "ast.h"
#include "codegen.h"
#include "common.h"
#include "lexer.h"
#include "semantic_analysis.h"
#include "syn_analyzator_top_down.h"

int main(void) {
  LinkedTlist *list = lexerMain();

  AstProgram *prog = topDown(list);
  semanticAnalysis(prog);
  char *out = generateCode(*prog);

  printf("%s", out);

  arenaCleanup();
  return ERR_OK;
}
