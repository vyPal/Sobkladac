/**
 * @file semantic_analysis.c
 * @brief semantic analysis
 * @author David Sobek <xsobekd00>
 *
 * Memory is managed by arena allocator.
 */

/*
Semantic analysis checklist:
1. Symbol table initialization
- Add built-in functions to the symbol table.
- Add all user-defined functions to the symbol table.
- Check duplicate function names.
- Check that exactly one valid main exists.
2. Declaration and reference checking
- Check variable declarations.
- Check duplicate declarations in the same scope.
- Check that variables are declared before use.
- check function calls.
- Check that immutable variables (val) aren't modified.
3. Type checking
- Infer types of expressions.
- Check operator compatibility.
- Check assignment compatibility.
- Check function argument types.
- Check function return types.
- Check conditions in if and while.
4. Return checking
- Check that return statements match the declared return type.
- Check that functions return on all required execution paths.
- Check that main follows its required return rules.
5. AST resolution for code generation
- Assign unique symbol IDs to variables.
- Assign unique function IDs to functions.
- Store resolved IDs in AST references.
- Ensure the code generator can distinguish variables with identical names in different scopes.
*/

#include <stdbool.h>

#include "ast.h"
#include "common.h"
#include "symtable.h"

bool semanticAnalysis(AstProgram *ast) {
  return false;
}
