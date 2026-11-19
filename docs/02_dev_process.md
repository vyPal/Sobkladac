# Teamwork and Development Process

## Work distribution

- **Jakub Palacký (`xpalacj00`):** Common utilities, AST helper methods, String Builder, Codegen

## Version Control and Code Review

- All code was tracked with Git and hosted on GitHub
- Branch names were selected according to <https://conventionalbranch.org/>
- PRs were required to have at least 1 approval from another team member (not the PR author)
- AI was used as a co-reviewer at the beggining

## Code Quality and Testing

- **Formatting:** `clang-format` was used with a custom mixed configuration
- **Static Analysis & Memory:** `cppcheck` and `-fsanitize=address,undefined` were used
- **Testing:**
  - Unit tests were written for each component by the component's author
  - Tests use the tst.h bundled testing library
  - Codegen tests use a bundled ifjcode26 interpreter to test code generation
