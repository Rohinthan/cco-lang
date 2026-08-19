# Cco Self-Hosted Lexer Proof-of-Concept (v12.0)

This directory contains the **self-hosted lexer** for the Cco language, written entirely in Cco itself (`selfhost/lexer.cco`).

---

## 🎯 What This Proves (and What It Doesn't)

### What This Proves
- **Language Expressiveness**: Proves that Cco is expressive enough to implement a complete, real-world lexer in its own syntax.
- **Correctness & Ground-Truth Parity**: The self-hosted lexer produces **byte-identical** token streams to the reference hand-written C lexer (`src/lexer.c`) across all 83 `.cco` files in the repository (all 61 test programs and 22 example programs).
- **Deep Feature Integration**: `lexer.cco` exercises nearly every major feature built across Cco v1–v11 in a single non-trivial program:
  - **Tagged Unions & Pattern Matching (v11)**: `TokenKind` enum with payload variants (`Ident`, `Keyword`, `IntLit`, `FloatLit`, `StringLit`, `CharLit`, `Symbol`, `Eof`) matched exhaustively.
  - **Hash Maps (v10)**: `map[string]bool` for O(1) keyword recognition across all 29 language keywords.
  - **Growable Dynamic Arrays (v9)**: `list_new(Token)` and `push(tokens, tok)` for accumulating scanned tokens.
  - **Classes & Object Safety (v2/v3/v5)**: `Token` class wrapping enum `kind`, line, and column info.
  - **Strings & Character Operations (v1/v4)**: `len`, `char_at`, `substring`, `concat`, `equals`, and escape sequence handling.
  - **Deterministic Single Ownership**: Zero-leak memory management validated under Valgrind.

### What This Does NOT Claim
- **Not Full Self-Hosting**: This is a proof-of-concept scoped specifically to the **lexer stage**. The parser, scope analyzer, and C code generator remain hand-written in C.
- **Standalone Proof-of-Concept**: The self-hosted lexer is a standalone proof binary in `selfhost/` and is not wired into the `cco` compiler CLI pipeline as a replacement for `src/lexer.c`.

---

## 🏗️ Building and Running

### 1. Transpile and Compile the Self-Hosted Lexer
```bash
# Transpile Cco lexer to C, then compile to native executable
./cco selfhost/lexer.cco -o selfhost/lexer_selfhosted.c
gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 selfhost/lexer_selfhosted.c -o selfhost/lexer_selfhosted -lm
```

### 2. Lex a Target File
The self-hosted lexer reads from `target.cco` in the current working directory:
```bash
cp tests/programs/01_hello.cco target.cco
./selfhost/lexer_selfhosted
```

### 3. Run the Comparison Test Harness
To verify byte-identical output against the C reference compiler (`./cco --dump-tokens`) across the entire codebase:
```bash
make test_selfhost
# or directly:
bash tests/compare_lexers.sh
```

---

## 📊 Token Stream Contract

Both the C compiler's `--dump-tokens` flag and `selfhost/lexer_selfhosted` emit tokens in the standardized format:
```
<line>:<col> <KIND> <value>
```

Example on `tests/programs/01_hello.cco`:
```
1:1 KEYWORD fn
1:4 IDENT main
1:8 SYMBOL (
1:9 SYMBOL )
1:11 SYMBOL ->
1:14 KEYWORD int
1:18 SYMBOL {
2:5 KEYWORD print
2:10 SYMBOL (
2:11 STRING Hello Cco World!
2:29 SYMBOL )
2:30 SYMBOL ;
...
8:1 EOF
```
