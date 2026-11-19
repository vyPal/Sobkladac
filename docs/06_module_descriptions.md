# Brief Module Descriptions

## `ast`

Describes the main structure of the AST and defines several helper methods used when constructing the tree

## `codegen`

Full codegen logic, performs a single depth-first pass over the completed AST from the semantic analysis step, directly emitting IR in the IFJcode26 language. Only the AST and in some cases the symbol table are used as data sources

## `common`

Defines the arena allocator's methods as well as all compile-time error codes and a helper method that frees the arena, logs an error to `stderr` and exits with the supplied exit code

## `layout`

Helper methods for syntactical analyzer, transforms lexer's `TK_INDENT(num)` tokens to `TK_INDENT`/`TK_DEDENT` pairs, as well as swapping `TK_IDENT`s belonging to keywords for `TK_KW_*` tokens

## `hashtable`

Implementation of generic hashtable ADT

## `lexer`

Full lexical analyzer, reads input directly from `stdin` and returns an arena-allocated linked list of tokens

## `semantic_analysis`

Full semantic analyzer implementation, accepts a partially built AST (some fields are left uninitialized before this step) from the syntactical analyzer, checks and resolves data types, variables, etc. and fills out symbol table and all uninitialized fields

## `strbuf`

Simple string builder implementation based on the arena allocator, used by the lexer and codegen

## `symtable`

Symbol table implemented as a wrapper around the hashtable implementation, built by the semantic analyzer and added to each block of the AST for both the semantic analyzer and codegen to use

## `syn-analyzator-bottom-up`

Bottom-up syntactical analyzer used for parsing expressions

## `syn-analyzator-top-down`

Top-down syntactical analyzer used for parsing rest of IFJ26 source code

## `token`

Defines all tokens returned by lexer

\newpage
