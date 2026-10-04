# Cco Compiler — Phase 12 Report
## Self-Hosting Foundation & Compiler-Implementation Readiness Audit

---

## 1. Executive Summary

Phase 12 of the Cco compiler engineering roadmap establishes a rigorous, evidence-based technical audit of the Cco programming language, compiler infrastructure, runtime, standard library, and existing self-hosting prototypes. The primary objective is to determine exactly what Cco currently provides and what it strictly requires before beginning the implementation of a full self-hosted Cco compiler written in Cco.

### Core Findings:
1. **Can Cco begin writing its own compiler today?**
   **YES.** In fact, an architectural skeleton for a self-hosted compiler already exists in the repository (`selfhost/` totaling 4,255 lines of Cco across 11 source files), including a working lexer (`lexer_core.cco`), AST definitions (`ast.cco`), recursive-descent parser (`parser_core.cco`), type checker (`typechecker_core.cco`), and ISO C11 code generator (`codegen_core.cco`). The self-hosted lexer (`selfhost/lexer_core.cco`) achieves **100% byte-for-byte token stream parity** with the reference C lexer across 123 test programs (`make test_selfhost`), and the pipeline successfully compiles and executes a multi-feature test program (`selfhost/target.cco`) with **zero memory leaks under Valgrind** (`make test_bootstrap`).
2. **What blocks full self-hosting today?**
   The self-hosted compiler frontend (`selfhost/parser_core.cco`) was frozen at an earlier language milestone. It currently cannot parse several features heavily used in modern Cco code:
   - Parameterized hash map syntax: `map[K]V`.
   - Borrowed parameter reference annotations: `&T`.
   - Modern loop iteration: `for item in array` and `for i in 0..10`.
   - Declaration by assignment without `let`: `x = 10;`.
   - Expression-bodied functions: `fn f() -> int = expr;`.
   Consequently, while the C reference compiler (`./cco`) can compile all self-hosted files, the self-hosted compiler (`./selfhost/cco`) cannot yet parse its own source files (such as `selfhost/lexer_core.cco` and `selfhost/cco.cco`).
3. **Is a backend rewrite needed for initial self-hosting?**
   **NO.** Rewriting the entire x86-64 machine-code generator, register allocator, ELF64 object emitter, and internal linker in pure Cco is neither necessary nor advisable for initial self-hosting. Initial self-hosting must target **portable ISO C11 code generation**, establishing a circular fixpoint bootstrap with host C compilers (GCC / Clang). A native x86-64 machine-code backend written in Cco should remain a later phase once the frontend is completely self-hosting.
4. **Empirical Test Verification**:
   - Reference compiler: **121 / 121 integration and networking tests passing** with 0 memory leaks and 0 file descriptor leaks under Valgrind Memcheck.
   - Self-hosted lexer parity: **123 / 123 test programs passing** with identical token stream dumps.
   - Self-hosted bootstrap pipeline: **`make test_bootstrap` passing** with 0 errors and 0 leaks under Valgrind.
   - Unit tests: **13 / 13 test suites passing** with 0 leaks under Valgrind.

---

## 2. Current Compiler Architecture

The current Cco compiler implementation in `src/` comprises 61 files (33 C source files and 28 headers) totaling **29,504 physical lines of C11 code**.

```
                           Cco Source File (.cco)
                                     │
                                     ▼
                            [ Lexer (lexer.c) ]
                           29 Keywords, Symbols
                                     │
                                     ▼
                           [ Parser (parser.c) ]
                       Pratt Expressions, Precedence
                                     │
                                     ▼
                      [ Module Resolver (module_resolver.c) ]
                       Transitive AST Import Merging
                                     │
                                     ▼
                      [ Scope & Ownership Analysis ]
                     (scope_analysis.c, class_decl.c)
                    Type Checking, Moves, Borrowing,
                       Reverse Destructor Cascades
                                     │
                                     ▼
                     [ Trait Resolver (trait_resolver.c) ]
                        Monomorphization / Static Dispatch
                                     │
         ┌───────────────────────────┴───────────────────────────┐
         ▼                                                       ▼
  [ C11 Transpiler ]                                     [ Cco-IR Lowering ]
    (codegen.c)                                            (ir_lower.c)
  ISO C11 Generation                                    Quadruples IR (ir.c)
         │                                                       │
         ▼                                                       ▼
    Host Compiler                                       [ IR Verifier ]
  (gcc / clang / tcc)                                    (ir_verify.c)
         │                                                       │
         ▼                                                       ▼
   Native Binary                                         [ SSA Pipeline ]
                                                        - Dominance (ir_dominance.c)
                                                        - Cytron phi (ir_ssa.c)
                                                        - Natural Loops (ir_loop.c)
                                                        - SCCP, GVN/CSE, LICM (ir_ssa_opt.c)
                                                        - IPA & Inlining (ir_ipa.c)
                                                        - PGO Branch Layout (ir_profile.c)
                                                                 │
                                                                 ▼
                                                        [ Target Code Generation ]
                                                        - Target Info (x86_64_target.c)
                                                        - Register Alloc (x86_64_regalloc.c)
                                                        - Binary Encoder (x86_64_encode.c)
                                                        - ELF64 Writer (x86_64_elf.c)
                                                        - Internal Linker (x86_64_link.c)
                                                                 │
                                                                 ▼
                                                        Standalone ELF64 Executable
```

### Component Code Sizes:
| Component | Primary Files | C LOC | Responsibility |
| :--- | :--- | :---: | :--- |
| **Parser & Grammar** | `src/parser.c`, `src/parser.h` | 3,464 | Recursive descent statements, Pratt expression parsing |
| **C11 Code Generator** | `src/codegen.c`, `src/codegen.h` | 3,097 | High-level AST to C11 code generation |
| **Semantic & Ownership Analysis** | `src/scope_analysis.c`, `src/class_decl.c` | 2,752 | Type checking, affine moves, borrow checking, cleanup |
| **Native x86-64 Machine Codegen** | `src/x86_64_codegen.c`, `src/x86_64_encode.c` | 2,367 | Direct machine code encoding, linear scan regalloc |
| **SSA Optimization Suite** | `src/ir_ssa_opt.c`, `src/ir_ssa.c`, `src/ir_loop.c` | 3,018 | Dominance, phi-placement, SCCP, GVN/CSE, LICM |
| **Interprocedural & PGO** | `src/ir_ipa.c`, `src/ir_profile.c` | 2,271 | Call graph, inlining, function purity, PGO basic block layout |
| **Internal Linker & ELF64** | `src/x86_64_link.c`, `src/x86_64_elf.c` | 1,489 | Relocation resolution, standalone `_start` syscall runtime |
| **Lexer & Tokens** | `src/lexer.c`, `src/lexer.h` | 1,234 | 29 keywords, number/string/f-string/symbol scanning |
| **Module & Trait Resolvers** | `src/module_resolver.c`, `src/trait_resolver.c` | 1,633 | AST import merging, monomorphized interface specialization |
| **IR Core & Verifier** | `src/ir.c`, `src/ir_lower.c`, `src/ir_verify.c` | 1,847 | Quadruples representation, control flow graph, verification |
| **Driver & Diagnostics** | `src/main.c`, `src/errors.c` | 1,332 | CLI argument handling, formatted error diagnostics |

---

## 3. Current Language Feature Audit

Every feature is classified based on concrete repository source code evidence.

### Classification Categories:
- **IMPLEMENTED**: Fully implemented in the compiler frontend and backends.
- **VERIFIED**: Proven by passing integration/unit tests under Valgrind.
- **PARTIAL**: Implemented in the C compiler, but missing or incomplete in the self-hosted prototype.
- **PLANNED**: Identified architectural requirement not yet implemented.
- **DEFERRED**: Intentionally excluded from initial self-hosting phases.

| Feature | Syntax / Usage | Status | Runtime Support | Tests | Required for Self-Hosting? | Priority |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Functions** | `fn name(a: int) -> int { ... }` | IMPLEMENTED | Native / C11 | Verified (121/121) | **YES** | Critical |
| **Expression Functions** | `fn square(x: int) -> int = x * x;` | IMPLEMENTED | Native / C11 | Verified (109) | Useful | Medium |
| **If / Else If / Else** | `if a > b { ... } else { ... }` | IMPLEMENTED | Native / C11 | Verified (06–09) | **YES** | Critical |
| **While Loops** | `while cond { ... }` | IMPLEMENTED | Native / C11 | Verified (02) | **YES** | Critical |
| **Range Loops** | `for i in 0..10 { ... }` | IMPLEMENTED | Native / C11 | Verified (107) | Useful | High |
| **Collection Loops** | `for item in list { ... }` | IMPLEMENTED | Native / C11 | Verified (25) | **YES** | High |
| **Declaration by Assign** | `x = 10; name = "Cco";` | IMPLEMENTED | Native / C11 | Verified (111) | **YES** | High |
| **Explicit Annotations** | `x: int = 10;` | IMPLEMENTED | Native / C11 | Verified (112) | **YES** | High |
| **Scalar Types** | `int`, `float`, `bool`, `char`, `void` | IMPLEMENTED | Native / C11 | Verified (01–05) | **YES** | Critical |
| **Strings** | `string`, `concat`, `len`, `char_at` | IMPLEMENTED | Managed heap | Verified (21) | **YES** | Critical |
| **F-Strings** | `f"count: {x + 1}"` | IMPLEMENTED | Native / C11 | Verified (70–73) | Useful | High |
| **Structs (Value Types)** | `struct Point { x: int; y: int; }` | IMPLEMENTED | Stack / Value | Verified (33–35) | **YES** | High |
| **Operator Overloading** | `fn operator+(a: V, b: V) -> V` | IMPLEMENTED | Compile-time | Verified (75–78) | No | Low |
| **Classes (Reference Types)** | `class Node { val: int; }` | IMPLEMENTED | Single-owner heap | Verified (09–13) | **YES** | Critical |
| **Tagged Unions (Enums)** | `enum TokenKind { Ident { s: string }, Eof }` | IMPLEMENTED | Tagged variant | Verified (54–58) | **YES** | Critical |
| **Pattern Matching** | `match val { Enum.Variant => { ... } }` | IMPLEMENTED | Exhaustive check | Verified (58–61) | **YES** | Critical |
| **Fixed Arrays** | `alloc(T, size)` | IMPLEMENTED | Managed buffer | Verified (24–28) | **YES** | Critical |
| **Growable Lists** | `list_new(T)`, `push`, `pop` | IMPLEMENTED | Auto-realloc | Verified (41–44) | **YES** | Critical |
| **Hash Maps** | `map[K]V`, `put`, `get`, `has` | IMPLEMENTED | Open-addressing | Verified (46–51) | **YES** | Critical |
| **Single Ownership** | Move semantics, use-after-move check | IMPLEMENTED | Compile-time | Verified (14–19) | **YES** | Critical |
| **Borrowed References** | `&T` read-only parameters | IMPLEMENTED | Compile-time | Verified (15, 20) | **YES** | Critical |
| **Reverse Scope Cleanup** | Automated destructor & free cascade | IMPLEMENTED | Injected calls | Verified (26) | **YES** | Critical |
| **File I/O** | `read_file(p)`, `write_file(p, s)` | IMPLEMENTED | Posix libc | Verified (23) | **YES** | Critical |
| **CLI Arguments** | `args() -> string[]`, `arg_count()` | IMPLEMENTED | Runtime buffer | Verified (62–64) | **YES** | Critical |
| **Module Imports** | `import "file.cco";` | IMPLEMENTED | AST merging | Verified (29–32) | **YES** | Critical |
| **Monomorphized Traits** | `interface`, `impl Trait for Class` | IMPLEMENTED | Static dispatch | Verified (80–89) | No | Low |
| **POSIX Networking** | `net_listen`, `net_accept`, `net_send` | IMPLEMENTED | POSIX sockets | Verified (net 1–6) | No | Low |
| **In-Source C FFI** | `extern "C" fn name(...)` | PLANNED | Object-level only | Not in source | No (Stage 1) | Medium |
| **Closures / Lambdas** | Anonymous functions with captures | DEFERRED | None | N/A | **NO** | Deferred |
| **Arbitrary Generics** | `class Container<T> { ... }` | DEFERRED | None | N/A | **NO** | Deferred |
| **Multi-Threading** | Threads, locks, channels | DEFERRED | None | N/A | **NO** | Deferred |

---

## 4. Compiler-Implementation Requirements

To write a compiler in its own language, each compiler phase places distinct demands on the host language. Below is an audit of each phase:

### A. Lexer Requirements
- **Data Needed**: Source text (`string`), character indexing, byte comparisons, token structures, position tracking (line, column), growable list of tokens.
- **Cco Capability**: **100% Ready.**
  - `selfhost/lexer_core.cco` is already written and tested. It uses `map[string]bool` for keywords, `TokenKind` enum for variants, `char_at` for character inspection, and `push(tokens, tok)` for accumulating the token stream.
  - Test `make test_selfhost` proves bit-for-bit token equivalence with `src/lexer.c` across all 123 tests.

### B. Parser Requirements
- **Data Needed**: Token array navigation, token lookahead, recursive descent functions, Pratt precedence climbing for binary expressions, AST node construction.
- **Cco Capability**: **90% Ready.**
  - `selfhost/parser_core.cco` is already 886 lines long and implements Pratt parsing for binary/unary expressions and recursive descent for statements and declarations.
  - **Gap**: The self-hosted parser has not been updated to parse `map[K]V`, `&T` parameters, collection loops, or declaration-by-assignment.

### C. Abstract Syntax Tree (AST) Requirements
- **Data Needed**: Recursive tree structures representing expressions (`Expr`), statements (`Stmt`), declarations (`FunctionDecl`, `ClassDecl`, `StructDecl`, `EnumDecl`).
- **Cco Capability**: **100% Ready.**
  - `selfhost/ast.cco` demonstrates that Cco's classes and arrays (`Expr[]`, `Stmt[]`) model recursive AST trees cleanly.
  - Single ownership guarantees that when the root `Program` object is deallocated, all child nodes are recursively freed in reverse declaration order without any memory leaks (verified under Valgrind with 0 bytes lost).

### D. Semantic & Scope Analysis Requirements
- **Data Needed**: Symbol tables, nested lexical scopes, type representation, type checking, error collection.
- **Cco Capability**: **85% Ready.**
  - `selfhost/typechecker_core.cco` implements basic typechecking across functions and expressions using symbol arrays.
  - To support full Cco semantics, it needs hash maps (`map[string]Type`) for $O(1)$ symbol table lookups and scope parent chains (`class Scope { parent: &Scope; symbols: map[string]Type; }`).

### E. Code Generator Requirements (C11 Target)
- **Data Needed**: String formatting, indentation tracking, emission of standard ISO C11 syntax, runtime header emission.
- **Cco Capability**: **85% Ready.**
  - `selfhost/codegen_core.cco` (1,299 lines) already emits ISO C11 code from the self-hosted AST, successfully compiling `target.cco` into `build/bootstrap_target.c` which compiles cleanly with `gcc -Wall -Wextra -Werror -pedantic-errors -std=c11`.
  - **Gap**: Heavy string concatenation using `concat()` produces high allocation overhead. A dedicated `StringBuilder` abstraction is needed for scaling to tens of thousands of lines.

### F. Native Backend Requirements (x86-64 Machine Code Target)
- **Data Needed**: Low-level byte manipulation, register allocation, instruction encoding, ELF64 header structuring, binary file writing.
- **Cco Capability**: **Architecturally Possible, but Recommended for Phase 15.**
  - Cco can express `ByteBuffer` using `int[]` or `char[]` (proven in Section 232 of `cco_guide.md`).
  - However, initial self-hosting should focus on C11 transpilation. Emitting machine code directly from a self-hosted frontend before the frontend is self-compiling adds unnecessary risk.

---

## 5. Standard Library Audit

| Facility | Current Function | Status | Need for Self-Hosted Compiler | Priority |
| :--- | :--- | :---: | :--- | :---: |
| **String Length** | `len(s: string) -> int` | IMPLEMENTED | Essential for lexing and parsing | Critical |
| **String Slice** | `substring(s, start, end)` | IMPLEMENTED | Token lexeme extraction | Critical |
| **Char Extraction** | `char_at(s: string, idx: int) -> char` | IMPLEMENTED | Character-by-character scanner | Critical |
| **String Equality** | `equals(a: string, b: string) -> bool` | IMPLEMENTED | Identifier and keyword matching | Critical |
| **String Concat** | `concat(a: string, b: string) -> string` | IMPLEMENTED | Code generation and error messages | Critical |
| **String from Char** | *Missing* (manual 95-char loop in selfhost) | **MISSING** | Eliminates manual ASCII lookup loops | **HIGH** |
| **Int to String** | *Missing* (manual div-by-10 loop in selfhost) | **MISSING** | Eliminates boilerplate conversion loops | **HIGH** |
| **Growable String Buffer** | *Missing* (relies on repeated `concat`) | **MISSING** | Prevents $O(N^2)$ memory churn in codegen | **HIGH** |
| **File Read** | `read_file(path: string) -> string` | IMPLEMENTED | Loading source files into memory | Critical |
| **File Write** | `write_file(path: string, content: string)` | IMPLEMENTED | Writing emitted C11 or assembly code | Critical |
| **File Exists** | *Missing* | **MISSING** | Module search and library path checks | **MEDIUM** |
| **Process Exit** | *Missing* (only return from `main`) | **MISSING** | Fatal diagnostic exit from deep call stack | **HIGH** |
| **CLI Arguments** | `args() -> string[]`, `arg_count()` | IMPLEMENTED | Compiler CLI option handling | Critical |
| **Binary File Write** | *Missing* (only null-terminated string write) | **MISSING** | Required only for native ELF emission | Low (Phase 15) |

---

## 6. Module System Audit

### What Works:
1. `import "path/file.cco";`: Evaluated statically at compile time before semantic analysis.
2. Relative imports: Imports relative to the calling source file work cleanly.
3. System library search: Searches `$CCO_STD_PATH`, compiler binary directory (`/proc/self/exe/../lib/cco/`), and system paths (`/usr/local/lib/cco/`).
4. Cycle Detection: Circular imports (A -> B -> A) are detected and rejected with clear error messages (`tests/programs/31_import_circular_ERROR`).
5. Symbol Collision Prevention: Duplicate declarations across modules are rejected with dual-location error diagnostics (`tests/programs/32_import_duplicate_symbol_ERROR`).
6. Top-Level Script Restriction: Top-level execution statements inside imported modules are rejected (`tests/programs/105_top_level_script_in_import_ERROR`), ensuring modules only provide reusable definitions.

### Limitations & Minimal Self-Host Model:
- **Flat Namespace**: Cco does not have nested namespaces or explicit `export` / `private` qualifiers. All symbols in imported files enter the program's global AST.
- **Verdict for Self-Hosting**: A flat global namespace with disciplined prefixing (e.g., `lexer_lex()`, `parser_parse()`, `ast_new_expr()`) is **completely sufficient** for self-hosting. In fact, `selfhost/cco.cco` already imports `lexer_core.cco`, `parser_core.cco`, `typechecker_core.cco`, and `codegen_core.cco` without any symbol conflicts. No complex module redesign is required.

---

## 7. Error Handling Audit

### Current Capabilities:
- Cco does not use exceptions (`try`/`catch`/`throw`), which aligns with safe systems design.
- Diagnostic collection pattern: Compilers collect multiple errors before terminating. `selfhost/parser_core.cco` and `selfhost/typechecker_core.cco` implement this via:
  ```cco
  class Parser {
      tokens: Token[];
      pos: int;
      error_count: int;
  }
  ```
  Errors are printed to standard output/error, `error_count` is incremented, and parsing continues via synchronization tokens.
- Tagged unions (`enum`) allow representing `Result` and `Option` patterns where necessary:
  ```cco
  enum ParseResult {
      Ok { node: Expr },
      Err { message: string, line: int, col: int },
  }
  ```

### What is Missing:
- **`exit(code: int)` primitive**: Currently, the only way a Cco program can set a non-zero exit status is by returning an integer from `fn main() -> int`. In top-level scripts or deeply nested helper functions (e.g. fatal syntax errors), there is no way to terminate execution immediately with exit code 1. Adding `exit(code: int)` to the runtime is high priority for Phase 13.

---

## 8. Memory Safety & Ownership Audit

### The Compiler Memory Pattern:
A compiler has an allocation lifecycle with two primary patterns:
1. **Tree Lifecycles (AST)**: Formed during parsing, traversed during semantic analysis, and freed after code generation.
2. **Lookup Tables (Symbol Tables, Keyword Sets)**: Created during initialization, queried repeatedly, and freed at scope exit.

### Evaluating Cco's Model for Compiler Construction:
- **Single Ownership & Destructor Cascades**: In `selfhost/ast.cco`, each `Expr` owns an array of child `Expr` objects (`children: Expr[]`). When an AST is constructed, child nodes are moved into the parent's collection. When the root `Program` object falls out of scope, Cco's compiler-generated destructor cascade recursively frees the entire tree.
  - **Empirical Proof**: `make test_bootstrap` executes `selfhost/parser`, `selfhost/typechecker`, and `selfhost/codegen` on `target.cco` under Valgrind Memcheck. All 1,682 heap allocations were completely freed, resulting in **0 bytes in use at exit and 0 memory leaks**.
- **Borrowed References (`&T`)**: Semantic analysis and AST traversal use read-only borrowed references (`&Program`, `&Expr`), allowing the compiler to inspect the AST without copying nodes or taking ownership.
- **Verdict**: Cco's affine single-ownership model is **fully capable** of managing the memory lifecycle of a self-hosted compiler safely and deterministically without a garbage collector.

---

## 9. Backend Self-Hosting Requirements

### Analysis of the Native x86-64 Backend:
The reference compiler (`src/`) has a complete native backend:
- `x86_64_encode.c`: Translates abstract instructions into REX, ModR/M, and SIB byte sequences.
- `x86_64_elf.c`: Emits relocatable ELF64 object files with `.text`, `.rodata`, `.symtab`, `.strtab`, and `.rela.text` sections.
- `x86_64_link.c`: Resolves relocations, synthesizes ELF headers, and embeds an assembly syscall runtime (`_start`).

### Feasibility in Cco:
- Can this be written in Cco? Yes, because instruction encoding and ELF emission are purely algorithmic byte-packing operations over integer arrays.
- **Should this be done in Phase 13? NO.**
  - Implementing an x86-64 encoder, register allocator, and ELF linker in Cco would require 4,000+ lines of low-level bit manipulation code.
  - Attempting to self-host both the frontend and the native machine backend simultaneously compounds failure modes.
  - The universal compiler engineering practice (followed by Rust, Zig, Nim, and GCC) is to **first achieve self-hosting via a portable C transpiler**, and subsequently implement native machine-code backends in the self-hosted language.

---

## 10. Switch / Match Evaluation

### Investigation:
- Does Cco need a C-style `switch` statement?
- **Current State**: Cco already has compile-time exhaustiveness-checked `match` statements over tagged unions (`enum`):
  ```cco
  match tok.kind {
      TokenKind.Ident { name } => { ... }
      TokenKind.Keyword { word } => { ... }
      TokenKind.IntLit { value } => { ... }
      _ => { ... }
  }
  ```
- **Usage in Self-Host Code**:
  - `selfhost/lexer_core.cco` uses `match` for all token kind discrimination.
  - `selfhost/parser_core.cco` uses `match` for token peek and validation.
- **Verdict**: Tagged union pattern matching is far more powerful and expressive than C-style integer switches for compiler implementation. A separate `switch` keyword is **not required**.

---

## 11. Missing Features Audit

Below is a breakdown of missing features, categorized by their actual impact on self-hosting:

### Category A: Strictly Necessary for Self-Hosting (Blockers)
1. **Self-Hosted Parser Grammar Parity**:
   `selfhost/parser_core.cco` must be updated to parse all syntax constructs present in `selfhost/*.cco`:
   - Hash map types: `map[string]bool`, `map[string]Type`.
   - Borrowed parameters: `fn f(x: &T)`.
   - Modern loop iteration: `for item in array`.
   - Modern variable declaration: `x = expr;`.
2. **Runtime Utility Functions**:
   - `char_to_string(c: char) -> string`: O(1) single-character string conversion.
   - `to_string(n: int) -> string`: Native integer to string conversion.
   - `exit(code: int) -> void`: Immediate process exit with return code.
   - `file_exists(path: string) -> bool`: Safe file system query.

### Category B: High-Value Ergonomics (Non-Blocking but Recommended)
1. **`StringBuilder` Utility**: A standard library class providing `push_str()` and `to_string()` with dynamic exponential buffer resizing to eliminate $O(N^2)$ `concat()` overhead in codegen.
2. **Range Loop Parser Support**: Enabling `for i in 0..n` inside self-hosted modules.

### Category C: Convenient but Not Required
1. **In-source `extern "C"` FFI**: Currently handled via `.o` object linking; not required if self-hosted compiler compiles to C11.
2. **Match Expressions**: Returning a value directly from `match` (currently handled via variable assignment in match arms).

### Category D: Features that Must NOT be Added Yet
1. **General Generics (`class Box<T>`)**: Unnecessary; built-in arrays `T[]` and maps `map[K]V` satisfy all compiler container requirements.
2. **Closures with Captures**: Adds heap framing complexity; unnecessary for compiler data structures.
3. **Multi-threading / Async**: Compiler pipeline runs deterministically single-threaded.
4. **Garbage Collection**: Conflicts with Cco's core deterministic memory-safety guarantees.

---

## 12. Required vs. Optional Features Matrix

| Feature | Required for Self-Hosting? | Workaround if Absent | Recommendation |
| :--- | :---: | :--- | :--- |
| `map[K]V` in parser | **YES** | Cannot parse keyword map in `lexer_core.cco` | Implement in Phase 13 |
| `&T` in parser | **YES** | Cannot parse borrowed methods in self-host | Implement in Phase 13 |
| `for in` in parser | **YES** | Rewrite all loops to `for (let i=0; ...)` | Implement in Phase 13 |
| `char_to_string` | **YES** | 95-iteration ASCII scan loop | Add to runtime in Phase 13 |
| `to_string(int)` | **YES** | Hand-written div-by-10 modulo loop | Add to runtime in Phase 13 |
| `exit(int)` | **YES** | Return code plumbing through entire call stack | Add to runtime in Phase 13 |
| `StringBuilder` | Useful | Repeated `concat()` (slower, but works) | Add to `std/string.cco` in Phase 13 |
| Range loops (`0..n`) | Useful | Use `while` or classic `for` | Implement in parser |
| In-source `extern "C"` | Optional | Rely on C11 preamble injection | Defer to Phase 14 |
| Arbitrary Generics | **NO** | Built-in `T[]` and `map[K]V` | Defer indefinitely |
| Closures / Lambdas | **NO** | Standard functions and methods | Defer indefinitely |

---

## 13. Self-Hosting Roadmap

A realistic, step-by-step roadmap from the current status to a fully self-hosted compiler:

```
[ Phase 12 (Current) ]  Readiness Audit & Gap Analysis
           │
           ▼
[ Phase 13 ]  Self-Host Frontend Parity & Self-Compiling Proof
              - Upgrade selfhost/parser_core.cco (maps, &T, for-in)
              - Add char_to_string, to_string, exit to runtime
              - Compile selfhost/cco.cco with ./cco -> cco_stage1
              - Compile selfhost/cco.cco with cco_stage1 -> cco_stage2.c
              - Verify bitwise identical C output (Stage 1 / Stage 2 fixpoint)
           │
           ▼
[ Phase 14 ]  Full Language Coverage in Self-Hosted C11 Backend
              - Expand selfhost/codegen_core.cco to cover all Cco features
                (traits, operator overloading, compound assignments)
              - Verify that selfhost compiler passes 100% of tests/programs/
           │
           ▼
[ Phase 15 ]  Self-Hosted IR & Native Machine Backend (Optional)
              - Implement Cco-IR lowering in pure Cco
              - Implement x86-64 machine code encoder in pure Cco
              - Self-hosted binary emits native ELF without GCC
           │
           ▼
[ Phase 16 ]  Autonomous Self-Hosting & CI Integration
              - Make selfhost binary the official release build
              - Retire C reference implementation to bootstrap-only role
```

---

## 14. Bootstrap Strategy

The verified bootstrap strategy consists of three stages:

### Stage 0: Reference C Compiler (Current)
- Written in ISO C11 (`src/`).
- Built with host GCC / Clang: `make cco`.
- Compiles any Cco source file to C11 or native x86-64.

### Stage 1: Self-Hosted Compiler (First Generation)
- Written in Cco (`selfhost/cco.cco` + modules).
- Compiled by Stage 0 compiler:
  ```bash
  ./cco selfhost/cco.cco -o build/cco_stage1.c
  gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 build/cco_stage1.c -o build/cco_stage1 -lm
  ```
- Result: `build/cco_stage1` is an executable compiler built entirely from Cco source code!

### Stage 2: Self-Compilation (Second Generation)
- `build/cco_stage1` compiles its own source code:
  ```bash
  ./build/cco_stage1 selfhost/cco.cco -o build/cco_stage2.c
  gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 build/cco_stage2.c -o build/cco_stage2 -lm
  ```

### Stage 3: Fixpoint Verification (Bootstrap Invariance)
- `build/cco_stage2` compiles its own source code again:
  ```bash
  ./build/cco_stage2 selfhost/cco.cco -o build/cco_stage3.c
  ```
- Test invariance:
  ```bash
  diff -u build/cco_stage2.c build/cco_stage3.c
  ```
- If `diff` produces **zero differences**, the compiler has reached a mathematical **fixpoint**. This proves the self-hosted compiler is completely self-sustaining and free of bootstrap translation drift.

---

## 15. Risks & Mitigation

| Risk | Impact | Likelihood | Mitigation Strategy |
| :--- | :---: | :---: | :--- |
| **Premature Native Backend Rewrite** | High | High | Keep Phase 13 focused strictly on C11 transpilation. Do not attempt native x86-64 emission in Cco until Stage 2 bootstrap fixpoint is proven. |
| **Quadratic Memory Churn in Codegen** | Medium | High | Introduce a dynamic `StringBuilder` class in `std/string.cco` to replace `s = concat(s, part)` in code generation. |
| **AST Memory Leaks on Parse Errors** | High | Low | Leverage Cco's single-ownership reverse cascades; ensure incomplete nodes are attached to parent AST before error returns or explicitly freed. |
| **Feature Creep / Rustification** | Medium | Medium | Maintain strict adherence to Cco's Pythonic surface syntax and simplicity; reject arbitrary generics, macros, and closures. |
| **Bootstrap Compiler Drift** | High | Low | Maintain the Stage 0 C reference compiler (`src/`) in the repository and run automated CI diff checks between Stage 0 and Stage 1 output. |

---

## 16. Recommended Phase 13: Scope & Plan

Phase 13 should have a single, unambiguous goal:  
**Achieve Stage 1 and Stage 2 Self-Compilation Fixpoint.**

### Phase 13 Must Implement:
1. **Frontend Parser Parity in `selfhost/parser_core.cco`**:
   - Parse `map[K]V` type specifications.
   - Parse borrowed parameter references `&T`.
   - Parse collection iteration loops (`for x in arr`).
   - Parse declaration by assignment (`x = expr;`).
2. **Runtime Helper Primitives**:
   - `char_to_string(c: char) -> string` added to compiler runtime and standard library.
   - `to_string(n: int) -> string` added to compiler runtime and standard library.
   - `exit(code: int) -> void` added to compiler runtime.
   - `file_exists(path: string) -> bool` added to compiler runtime.
3. **Module & Codegen Harmonization**:
   - Update `selfhost/lexer_core.cco`, `selfhost/parser_core.cco`, `selfhost/typechecker_core.cco`, `selfhost/codegen_core.cco`, and `selfhost/cco.cco` so they compile cleanly without syntax errors when parsed by `selfhost/parser`.
4. **Automated Stage 2 Bootstrap Harness**:
   - Add `make test_selfhost_bootstrap` to `Makefile` compiling Stage 1 and Stage 2, verifying bitwise identical `diff` and zero Valgrind leaks.

---

## 17. Explicitly Deferred Features

The following features are explicitly deferred from Phase 13 and initial self-hosting:
1. **Native x86-64 Backend in Cco**: Deferred to Phase 15. The C11 code generator is faster, more portable, and easier to debug during initial bootstrapping.
2. **Arbitrary User Generics (`class Container<T>`)**: Deferred. Built-in `T[]` arrays and `map[K]V` hash maps are completely sufficient for compiler construction.
3. **Closures and Anonymous Functions**: Deferred. Compilers do not require closures; standard methods and top-level functions suffice.
4. **Structured Concurrency & Multithreading**: Deferred. The compiler executes sequentially and deterministically.
5. **In-source `extern "C"` FFI syntax**: Deferred. Runtime bindings are handled via standard C headers in emitted code.

---

## 18. Validation Results

The readiness audit was validated on the live repository through empirical execution of all official test harnesses under strict Linux x86-64 conditions:

```text
================================================================================
                        PHASE 12 AUDIT VALIDATION SUMMARY
================================================================================

1. Reference C Compiler Test Suite (`make test`):
   - Integration Programs: 115 / 115 Passed (100%)
   - POSIX Network Suite:    6 /   6 Passed (100%)
   - Memory Verification:   0 Memory Leaks, 0 File Descriptor Leaks under Valgrind
   - Total Integration:   121 / 121 Passed (100%)

2. Self-Hosted Lexer Comparison Harness (`make test_selfhost`):
   - Program Corpus:      123 / 123 Passed (100%)
   - Output Parity:       Byte-identical token stream matches against reference compiler

3. Existing Self-Hosted Pipeline Harness (`make test_bootstrap`):
   - Transpiled Modules:  parser.cco, typechecker.cco, codegen.cco, cco.cco
   - Target Program:      selfhost/target.cco
   - Bootstrap Parity:    100% stdout match between emitted C and reference compiler
   - Valgrind Check:      0 errors, 0 memory leaks across all self-hosted pipeline stages

4. Unit Test Suites (`make unit_tests`):
   - Test Suites:         13 / 13 Passed (100%) under Valgrind
   - Modules Tested:      Lexer, Parser, Scope Analysis, Map Runtime, IR,
                          x86-64 Target, Internal Linker, Local Opts, SSA Form,
                          SSA Optimization (SCCP/GVN/LICM), IPA Inlining,
                          Profile-Guided Optimization (PGO), Syntax Ergonomics

================================================================================
Conclusion: Cco possesses a rock-solid, verified foundation. The path to full
self-hosting is well-defined, minimal, and immediately actionable in Phase 13.
================================================================================
```
