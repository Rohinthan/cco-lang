# Cco v12.0 Implementation Report: Self-Hosted Lexer Proof-of-Concept

**Version**: v12.0  
**Status**: Completed & Verified (83/83 Corpus Files Passing, 61/61 Test Programs Passing, 0 Leaks)

---

## 1. Executive Summary

Cco v12.0 delivers the final milestone of the **4-step Self-Hosting Roadmap** (v9 growable dynamic arrays $\rightarrow$ v10 hash maps $\rightarrow$ v11 tagged unions $\rightarrow$ v12 self-hosted lexer proof-of-concept).

The primary objective of v12 was to prove that the Cco language is expressive enough to implement a complete, production-grade frontend component (the lexer) in its own syntax, and to verify its correctness by diffing its token stream byte-for-byte against the hand-written C lexer (`src/lexer.c`) across every `.cco` file in the repository (83 files total across all 61 tests and 22 examples).

---

## 2. Architecture & Design

### Two Lexers, One Unified Ground-Truth Contract
```
     src/lexer.c (C Reference Lexer)        selfhost/lexer.cco (Self-Hosted Lexer)
                   │                                         │
                   ▼                                         ▼
            ./cco --dump-tokens                   compiled via ./cco into
                   │                              selfhost/lexer_selfhosted
                   ▼                                         │
        Token Stream (stdout)                                ▼
       "<line>:<col> <KIND> <val>"                 Token Stream (stdout)
                   │                              "<line>:<col> <KIND> <val>"
                   │                                         │
                   └───────────────────┬─────────────────────┘
                                       │
                                       ▼
                             diff (Byte-Identical)
```

### Standardized Token Stream Format
Each token is emitted on a single line matching the exact specification:
```
<line>:<col> <KIND> <value>
```
Supported kinds: `KEYWORD`, `IDENT`, `SYMBOL`, `INT`, `FLOAT`, `STRING`, `CHAR`, `EOF`.

### Deliberate I/O Simplification
To keep v12 cleanly focused on lexical analysis without introducing out-of-scope CLI argument parsing machinery into Cco, `selfhost/lexer.cco` reads from a fixed input file `target.cco` via `read_file("target.cco")`. The automated comparison harness copies each corpus file to `target.cco` before invoking the binary.

---

## 3. What Was Added & Changed

### A. Existing C Compiler Enhancements (`src/`)
1. **`--dump-tokens` CLI Flag (`src/main.c`)**:
   - Added `--dump-tokens <file.cco>` support to run only the lexing stage and dump the standardized token stream to stdout.
2. **`dump_tokens()` Function (`src/lexer.c`, `src/lexer.h`)**:
   - Implemented formatted token dumping for all 29 keywords, identifiers, symbols (single & multi-character), string literals, char literals, numeric literals, and EOF.
3. **`test_dump_tokens_format` (`tests/unit/test_lexer.c`)**:
   - Added a unit test validating that `--dump-tokens` output matches the specification.

### B. Transpiler & Codegen Improvements (`src/ast.h`, `src/parser.c`, `src/scope_analysis.c`, `src/codegen.c`)
To compile the self-hosted lexer without workarounds, several compiler features were refined:
1. **Map and Array Return Types from Functions**:
   - Extended `parse_function` and `parse_class_decl` (methods) to parse and preserve `return_is_map`, `return_key_type`, and `return_is_array`.
   - Updated `gen_function` and `gen_method` to emit proper C return types (`Token **` for `Token[]`, `__cco_map *` for `map[string]bool`).
   - Extended type inference in `get_expr_elem_class_name`, `infer_expr_is_map`, `get_map_key_type`, and `is_expr_array_scope` to check user function signatures.
2. **Safe Order of Evaluation on Reassignment (`NODE_ASSIGN`)**:
   - Updated `NODE_ASSIGN` in `src/codegen.c` to evaluate RHS expressions into a temporary pointer `void *__assign_tmp` before executing `emit_frees` on the LHS variable. This prevents `use-after-free` when an owned variable is read on the RHS while being reassigned (e.g. `res = concat(c, res)`).

### C. Self-Hosted Lexer (`selfhost/lexer.cco`)
Implemented entirely in Cco, combining:
- **`TokenKind` Tagged Union (v11)**:
  ```cco
  enum TokenKind {
      Ident { name: string },
      Keyword { word: string },
      IntLit { value: int },
      FloatLit { value: float },
      StringLit { value: string },
      CharLit { value: char },
      Symbol { text: string },
      Eof,
  }
  ```
- **`Token` Class (v2/v3)**:
  ```cco
  class Token {
      kind: TokenKind;
      line: int;
      col: int;
  }
  ```
- **`map[string]bool` Keyword Table (v10)**: O(1) hash map lookup for all 29 language keywords (`import`, `class`, `struct`, `self`, `fn`, `let`, `if`, `else`, `while`, `for`, `in`, `return`, `break`, `continue`, `print`, `alloc`, `list_new`, `map`, `map_new`, `enum`, `match`, `int`, `float`, `char`, `bool`, `string`, `void`, `true`, `false`).
- **Growable `Token[]` Array (v9)**: `list_new(Token)` and `push(tokens, tok)`.
- **Scanning Logic**:
  - Whitespace & comment skipping (`//`).
  - String literal scanning with escape sequence support (`\"`, `\\`, `\n`, `\t`).
  - Character literal scanning (`' '`, `'\n'`, `'\t'`, `'\r'`, `'\''`, etc.).
  - Number scanning (integer and floating point literals).
  - Multi-character symbols (`->`, `=>`, `==`, `!=`, `<=`, `>=`, `&&`, `||`) matched before single-character symbols.
  - Single-character symbols (`(`, `)`, `{`, `}`, `[`, `]`, `,`, `:`, `;`, `=`, `<`, `>`, `+`, `-`, `*`, `/`, `%`, `!`, `.`, `&`, `_`).
- **Formatting**:
  - `int_to_str` and `float_to_str` with `strip_trailing_zeros` and epsilon rounding.
  - Exhaustive pattern matching over `tok.kind` to emit standardized token stream.

### D. Comparison Test Harness (`tests/compare_lexers.sh`)
- Automated harness that compiles the self-hosted lexer, iterates over all 83 `.cco` files in `tests/programs` and `examples`, dumps tokens from both lexers, and diffs them.
- Integrated into `Makefile` as `make test_selfhost` and chained into `make test`.

### E. Documentation
- `selfhost/README.md`: Explains how to build, run, and verify the self-hosted lexer.
- Main `README.md`: Updated to v12.0 with the new Self-Hosted Lexer section and updated roadmap.

---

## 4. Verification Results

### A. Comparison Harness Run (`tests/compare_lexers.sh`)
```
==================================================
  Cco v12 SELF-HOSTED LEXER COMPARISON HARNESS   
==================================================
Comparing lexers on examples/01_hello_world.cco... PASS
Comparing lexers on examples/02_fibonacci.cco... PASS
Comparing lexers on examples/03_point_distance.cco... PASS
Comparing lexers on examples/04_string_builder.cco... PASS
Comparing lexers on examples/05_array_sum.cco... PASS
Comparing lexers on examples/06_word_count.cco... PASS
Comparing lexers on examples/07_ownership_demo.cco... PASS
Comparing lexers on examples/08_stack_data_structure.cco... PASS
Comparing lexers on examples/09_object_array_todo.cco... PASS
Comparing lexers on examples/10_import_demo/main.cco... PASS
Comparing lexers on examples/10_import_demo/shapes.cco... PASS
Comparing lexers on examples/11_struct_vec2.cco... PASS
Comparing lexers on examples/demo.cco... PASS
Comparing lexers on examples/expr_eval.cco... PASS
Comparing lexers on examples/word_frequency.cco... PASS
Comparing lexers on tests/programs/01_hello.cco... PASS
...
Comparing lexers on tests/programs/61_enum_field_rename_ERROR.cco... PASS
==================================================
Summary: 83/83 Passed, 0 Failed
==================================================
```

### B. Unit & Integration Test Suite (`make test`)
- Unit tests under Valgrind: `test_lexer`, `test_parser`, `test_scope`, `test_map_runtime` — **ALL PASS (0 Leaks)**
- Lexer Comparison Harness: 83/83 files — **ALL PASS**
- Integration test suite: 61/61 integration tests (01–61) — **ALL PASS (0 Leaks)**

### C. Self-Hosted Lexer Memory Safety
- Validated `selfhost/lexer_selfhosted` under `valgrind --leak-check=full --error-exitcode=1`:
  - `HEAP SUMMARY: in use at exit: 0 bytes in 0 blocks`
  - `ERROR SUMMARY: 0 errors from 0 contexts`

---

## 5. Scope Boundaries & Roadmap Conclusion

| Component | Status in v12.0 | Language Implemented In |
| :--- | :--- | :--- |
| **Lexer (Proof-of-Concept)** | **Self-Hosted** | Cco (`selfhost/lexer.cco`) |
| **Reference Lexer** | Production | C (`src/lexer.c`) |
| **Parser & AST Construction** | Production | C (`src/parser.c`, `src/ast.c`) |
| **Scope & Ownership Analysis** | Production | C (`src/scope_analysis.c`) |
| **C11 Code Generator** | Production | C (`src/codegen.c`) |

With v12.0, the 4-step self-hosting roadmap is complete: Cco has proven its expressive capability by hosting its own lexical analyzer with byte-identical accuracy.
