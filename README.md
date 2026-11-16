# Cco — C Class and Objects

Cco (C Class and Objects) is a statically typed, compiled programming language and compiler project designed to combine readable, approachable syntax with systems-level programming capabilities and deterministic compile-time memory safety.

Cco provides a dual-target compilation model featuring a portable C11 source-to-source backend and an evolving direct native x86-64 machine-code backend.

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86--64-brightgreen.svg)]()
[![Build](https://img.shields.io/badge/Build-ISO%20C11%20%7C%20Native%20x86--64-blue.svg)]()
[![Memory Safety](https://img.shields.io/badge/Memory%20Verification-Valgrind%200%20Leaks-success.svg)]()

---

## Table of Contents

1. [Project Introduction](#1-project-introduction)
2. [What is Cco?](#2-what-is-cco)
3. [Design Philosophy](#3-design-philosophy)
4. [Key Language Features](#4-key-language-features)
5. [Compiler Architecture](#5-compiler-architecture)
6. [Backend Support](#6-backend-support)
7. [C Interoperability](#7-c-interoperability)
8. [Example Program](#8-example-program)
9. [Build and Usage Instructions](#9-build-and-usage-instructions)
10. [Current Development Status](#10-current-development-status)
11. [Roadmap](#11-roadmap)
12. [Contributing and License Information](#12-contributing-and-license-information)

---

## 1. Project Introduction

Cco (pronounced *C-co*, historically designated *C--*) explores a distinctive middle ground in programming language design: combining the visual clarity and lightweight syntax of high-level scripting languages with the raw performance, predictable resource usage, and static guarantees of compiled systems languages.

The project encompasses both the language specification and a complete reference compiler written in modular C11, featuring intermediate representation lowering, an SSA-based optimization pipeline, a direct binary x86-64 machine-code emitter, a freestanding ELF64 static linker, and comprehensive Foreign Function Interface (FFI) support for seamless C library integration.

---

## 2. What is Cco?

Cco is a statically typed, compiled systems programming language. It is not an interpreted scripting language, nor does it rely on a virtual machine or a runtime garbage collector.

### Core Attributes
- **Dual-Target Compilation**:
  1. **Standard C11 Code Generation**: Transpiles Cco source into clean, standard ISO C11 (`-std=c11 -pedantic-errors`) for auditability, cross-platform portability, or compilation with host C compilers (`gcc`, `clang`, `tcc`).
  2. **Direct Native x86-64 Backend**: Compiles directly to binary machine instructions, constructs ELF64 relocatable object files (`.o`), and links standalone Linux executables via an embedded static linker and freestanding syscall runtime (`_start`)—without requiring external toolchains.
- **Deterministic Lifetime Management**: Memory and operating system resources are managed at compile time through affine single-ownership and reverse scope-exit destructor cascades.
- **Bare-Metal Execution**: Programs execute directly on the hardware with zero runtime garbage-collection pauses.

---

## 3. Design Philosophy

Systems programming has traditionally forced a compromise between extremes:
- **Low-level languages (C, C++)** provide bare-metal execution speed and fine-grained hardware control, but lack compile-time safety against spatial and temporal memory defects (buffer overflows, use-after-free, double-free).
- **Managed languages (Python, Go, Java)** provide high developer ergonomics or automated safety through garbage collection or dynamic interpreters, but introduce runtime pauses, elevated memory footprints, and execution overhead unsuitable for latency-sensitive environments.
- **Modern safe systems languages (Rust)** prove that compile-time memory safety without garbage collection is achievable, but carry high cognitive overhead (complex lifetime annotations, strict syntactic ceremony) and longer compilation cycles.

Cco explores an alternative balance within this space:

- **Python-Inspired Ergonomics**: Declaration-by-assignment, range loops, unparenthesized control flow, expression-bodied functions, readable boolean keywords (`and`, `or`, `not`), and top-level script execution reduce visual noise and boilerplate.
- **C-Like Native Performance**: Stack-allocated value structs, zero-overhead function calls, direct machine-code generation, and low-level pointer capabilities enable performance on par with native C.
- **Rust-Inspired Memory & Resource Safety**: Affine single-ownership, compile-time move tracking, borrowed references (`&T`), and deterministic scope-exit cleanups prevent resource leaks and use-after-free errors without a garbage collector.
- **Automatic Ownership & Deterministic Cleanup**: Resources are reclaimed immediately and deterministically when their owning binding leaves scope.
- **Practical Systems Interoperability**: First-class FFI allows direct access to the vast ecosystem of existing C libraries without translation overhead.

> [!NOTE] Non-Replacement Stance
> Cco does not claim to replace C, C++, or Rust. Rather, it serves as a practical systems language and compiler exploration: offering the ergonomic speed of scripting while producing self-contained, statically checked native binaries.

---

## 4. Key Language Features

### Feature Matrix

| Area | Status | Description |
| :--- | :---: | :--- |
| **Static Typing** | Implemented | Strongly typed at compile time; primitive scalar types (`int`, `float`, `bool`, `char`, `string`, `void`). |
| **Type Inference** | Implemented | Local types inferred from initializers; validated against explicit annotations when provided. |
| **Declaration by Assignment** | Implemented | Assignment to an unresolved identifier creates a scoped binding (`x = 10;`); reassignment preserves type invariance. |
| **Optional Semicolons** | Implemented | Statements may be terminated by newlines or semicolons; multiline expressions continue across lines cleanly. |
| **Range Loops** | Implemented | Half-open interval iteration syntax (`for i in 0..10 { ... }`) desugared into deterministic counting loops. |
| **Boolean Keywords** | Implemented | Readable logical keywords (`and`, `or`, `not`) supported alongside traditional C operators (`&&`, `\|\|`, `!`). |
| **Expression-Bodied Functions** | Implemented | Concise single-expression function definitions (`fn f(x: int) -> int = expr;`). |
| **Top-Level Scripts** | Implemented | Entry files execute top-level statements directly; desugared into a synthesized native `main()` with exit code `0`. |
| **Single Ownership** | Implemented | Affine linear ownership for heap allocations (`class`, dynamic arrays, maps); compile-time use-after-move prevention. |
| **Borrowed References** | Implemented | Read-only borrow references (`&T`) pass data without transferring ownership; scope-escape restrictions enforced. |
| **Structs (Value Types)** | Implemented | Stack-allocated lightweight records with primitive fields; copied by value on assignment. |
| **Operator Overloading** | Implemented | Compile-time operator overloading for struct types across 10 operators (`+`, `-`, `*`, `/`, `==`, `!=`, `<`, `>`, `<=`, `>=`). |
| **Classes (Reference Types)** | Implemented | Heap-allocated objects with single ownership, encapsulated methods, and automatic scope-exit deallocation cascades. |
| **Tagged Unions (Enums)** | Implemented | Sum types with unit and payload variants; evaluated via compile-time exhaustiveness-checked `match` statements. |
| **Dynamic Collections** | Implemented | Typed arrays (`T[]` via `alloc`), growable lists (`list_new`, `push`, `pop`), and open-addressing hash maps (`map[K]V`). |
| **Recursive Type Descriptors** | Implemented | Arbitrary nested container descriptors (`int[][]`, `map[string]int[]`) with synthesized recursive cleanups. |
| **Interfaces (Traits)** | Implemented | Monomorphized at compile time; generic calls generate specialized functions with zero vtable dispatch overhead. |
| **F-String Interpolation** | Implemented | String interpolation with expression evaluation, automatic type conversion, and brace escaping (`{{`, `}}`). |
| **POSIX Networking** | Implemented | Core socket primitives (`net_listen`, `net_accept`, `net_recv`, `net_send`, `net_close`, `sleep_ms`) for TCP services. |
| **Native x86-64 Backend** | Implemented | Direct machine code generation adhering to the implemented subset of the System V AMD64 ABI. |
| **SSA Optimization Pipeline** | Implemented | Lengauer-Tarjan dominance, SSA $\phi$-construction, SCCP, GVN/CSE, LICM, inlining, and profile-guided block layout. |
| **Internal ELF64 Linker** | Implemented | Standalone static linker producing executable ELF64 binaries with embedded `_start` syscall runtime (no `ld` or `libc`). |
| **C11 Transpiler** | Implemented | Source-to-source transpilation targeting ISO C11 (`-std=c11 -pedantic-errors`) for GCC, Clang, TCC, and MSVC. |
| **Comprehensive C FFI** | Implemented | In-source declarations, library linking (`-L`/`-l`), raw pointers, const qualifiers, out-parameters, `unsafe` blocks, pointer difference, and aggregate pass-by-value ABI (FFI-1 through FFI-6). |
| **Structured Concurrency** | Planned | Native multithreading and thread-safe channels integrated with affine single ownership. |

---

## 5. Compiler Architecture

The Cco reference compiler operates as an integrated multi-pass pipeline supporting two distinct compilation paths:

```
                      Cco Source Code (.cco)
                                │
                                ▼
                       [ Lexer & Parser ]
                         AST Generation
                                │
                                ▼
                   [ Scope & Ownership Pass ]
            Type Inference, Move Tracking, Scope Cascades
                                │
         ┌──────────────────────┴──────────────────────┐
         ▼                                             ▼
  [ Cco-IR Lowering ]                           [ C11 Transpiler ]
  Typed Quadruples IR                            ISO C11 (.c) Generation
         │                                             │
         ▼                                             ▼
  [ SSA Construction ]                           Host C Compiler
  Dominance Tree, phi-nodes                      (gcc / clang / tcc)
         │                                             │
         ▼                                             ▼
  [ Optimization Pipeline ]                      Native Binary
  SCCP, GVN/CSE, LICM, Inlining, PGO
         │
         ▼
  [ x86-64 Machine Codegen ]
  Linear Scan Register Allocator, Binary Encoding
         │
         ├──────────────────────────────┐
         ▼                              ▼
  [ ELF64 Object Emitter ]      [ Internal Static Linker ]
  Relocatable .o Emission       Embedded _start Syscall Runtime
         │                              │
         ▼                              ▼
    Host Linker              Standalone Linux Executable
   (gcc / GNU ld)             (No external dependencies)
```

### Intermediate Representation & Optimization Pipeline

Cco lowers AST nodes into a linear quadruples Intermediate Representation (Cco-IR) before constructing Static Single Assignment (SSA) form.

| Optimization Level | CLI Flag | Passes Included |
| :--- | :---: | :--- |
| **None** | `-O0` | Direct IR lowering to target code without transformation (default). |
| **Local Optimizations** | `-O1`, `-O`, `--opt` | Local constant folding, algebraic simplification, and local dead-code elimination. |
| **SSA & Interprocedural** | `-O2` | Dominance analysis, SSA $\phi$-placement, SCCP, GVN/CSE, natural loop analysis, LICM, call graph analysis, function purity classification (`PURE`, `READONLY`), interprocedural inlining, dead function elimination (DFE), and critical edge splitting. |
| **Profile-Guided (PGO)** | `--profile-generate`<br>`--profile-use=<file>` | Runtime edge profiling, profile serialization, basic block layout reordering for biased branches, and hot-path inlining budget expansion. |

### Diagnostic & Inspection Flags

```bash
cco source.cco --emit-ir              # Dump textual Cco-IR representation
cco source.cco --dump-ssa             # Dump SSA form with phi-nodes
cco source.cco --dump-ssa-opt         # Dump SSA form after optimizations
cco source.cco --dump-loops           # Dump natural loop nesting tree
cco source.cco --dump-callgraph       # Dump call graph and function purity
cco source.cco --dump-regalloc        # Dump register allocation decisions
cco source.cco --dump-code-stats      # Dump instruction count statistics
```

---

## 6. Backend Support

Cco provides two production-grade compilation backends:

### 1. Portable ISO C11 Transpiler (Default)
- Emits clean, standard ISO C11 source code (`-std=c11 -pedantic-errors`).
- Serves as a dependable portability route for cross-compiling or deploying to targets where host C compilers (GCC, Clang, TCC, MSVC) exist.
- Emits zero external runtime dependencies beyond standard libc.

### 2. Direct Native x86-64 Backend (`--use-native` / `--use-internal-linker`)
- **Direct Machine Codegen**: Emits binary x86-64 machine instructions directly without invoking an external assembler (`as`).
- **System V AMD64 Calling Convention**: Standard parameter registers (`%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`), vector registers (`%xmm0`–`%xmm7`), 16-byte stack frame alignment, and callee-saved register preservation.
- **Register Allocation**: Fast linear scan register allocator with automatic stack frame spill-slot assignment.
- **Relocatable Object Writer**: Generates valid ELF64 relocatable object files (`.o`) containing `.text`, `.rodata`, `.symtab`, `.strtab`, and `.rela.text` sections.
- **Internal Static Linker (`--use-internal-linker`)**: Directly resolves relocations (`R_X86_64_64`, `R_X86_64_32S`, `R_X86_64_PC32`) and produces standalone executable ELF64 binaries using an embedded freestanding assembly runtime (`src/runtime/runtime_start.s`) with direct Linux syscalls (`sys_write`, `sys_mmap`, `sys_munmap`, `sys_exit`), requiring neither `gcc`, GNU `ld`, nor `libc.so`.

---

## 7. C Interoperability

Cco features a comprehensive Foreign Function Interface (FFI) for direct, type-checked communication with C libraries:

- **In-Source Declarations**: Declare external functions using `extern "C" fn name(...) -> ret;` or block syntax `extern "C" { ... }`.
- **Library Linking**: Link system or third-party libraries using `-L<dir>` and `-l<lib>` flags.
- **Raw Pointers**: First-class raw pointer types (`*const T`, `*mut T`, `*const void`, `*mut void`, `null`) with strict const-correctness and non-owning invariants.
- **Safe C String Interop**: Explicit built-in conversion functions `str_to_cstr(s)` (borrows Cco string buffer) and `str_from_cstr(p)` (defensively copies foreign C string into Cco managed memory).
- **Out Parameters**: Pass mutable addresses of lvalues via `&var` to satisfy C out-parameter patterns.
- **Quarantined Unsafe Blocks**: Operations that cannot be proven memory-safe at compile time (pointer dereferencing `*p`, `*p = val`, pointer arithmetic `p + n`, pointer casts `p as *mut T`) are strictly quarantined inside `unsafe { ... }` blocks.
- **Pointer Difference**: Signed element-scaled pointer subtraction `p1 - p2` inside `unsafe` blocks.
- **Aggregate Pass-by-Value ABI (System V AMD64)**: Pass and return Plain-Old-Data (POD) structs by value in foreign function calls. Supports recursive eightbyte classification, GPR/XMM register allocation, atomic stack rollback upon register exhaustion, and hidden return pointers (`sret`) for structures exceeding 16 bytes.

> [!TIP] Complete FFI Specification
> For complete documentation, detailed ABI classification rules, and code examples, see the [Cco FFI Specification & Guide](docs/FFI.md).

---

## 8. Example Program

The following program demonstrates Cco's syntax ergonomics, value structs, range loops, and C interoperability:

```cco
# example.cco
extern "C" {
    fn puts(s: *const char) -> int;
    fn abs(x: int) -> int;
}

struct Point {
    x: int;
    y: int;
}

fn square(n: int) -> int = n * n;

fn manhattan_distance(p1: Point, p2: Point) -> int {
    dx = p1.x - p2.x;
    dy = p1.y - p2.y;
    return abs(dx) + abs(dy);
}

fn main() -> int {
    # Call foreign C function
    puts(str_to_cstr("Starting Cco demonstration..."));

    # Value structs and functions
    p1 = Point { x: 10, y: 20 };
    p2 = Point { x: 4,  y: 12 };
    dist = manhattan_distance(p1, p2);
    print("Manhattan distance:", dist);

    # Range loop with boolean keywords
    total = 0;
    for i in 0..10 {
        if i > 2 and i < 8 {
            total += square(i);
        }
    }
    print("Computed total:", total);

    return 0;
}
```

### Compiling and Running

```bash
# 1. Compile using default C11 pipeline
cco example.cco -o example
./example

# 2. Compile using direct native x86-64 backend
cco example.cco --use-native -o example
./example

# 3. Compile and execute immediately in one step
cco example.cco --run
```

Output:
```text
Starting Cco demonstration...
Manhattan distance: 14
Computed total: 135
```

---

## 9. Build and Usage Instructions

### System Requirements
- **Operating System**: Linux x86-64 (kernel 3.2+)
- **Host C Compiler**: GCC 9+ or Clang 10+ (supporting ISO C11)
- **Build System**: GNU Make
- **Optional**: Valgrind 3.15+ (for running memory verification suites)

### 1. Building from Source

```bash
git clone https://github.com/Rohinthan/cco-lang.git
cd cco-lang
make cco
```

This compiles the `cco` compiler executable.

### 2. Installing for the Current User (Recommended)

To install Cco for your local user account without requiring `sudo` privileges:

```bash
make install
```

By default, this installs:
- Executables (`cco`, `gcco`, `cco-link`) into `~/.local/bin/`
- Standard library modules (`io.cco`, `net.cco`, `math.cco`, `string.cco`, `buffer.cco`, `deque.cco`, `result.cco`, `option.cco`) into `~/.local/lib/cco/std/`

Ensure `~/.local/bin` is in your shell `PATH`:
```bash
export PATH="$HOME/.local/bin:$PATH"
```

To uninstall at any time:
```bash
make uninstall
```

### 3. Installing System-Wide

To install system-wide across all users:

```bash
sudo make install PREFIX=/usr/local
```

To uninstall:
```bash
sudo make uninstall PREFIX=/usr/local
```

### 4. Running Verification Test Suites

```bash
# Run unit tests under Valgrind Memcheck
make unit_tests

# Run full integration test suite, bootstrap checks, and network tests
make test
```

---

## 10. Current Development Status

The Cco compiler has achieved the following verified milestones:

- **Core Language & Type System**: Fully verified across 121 integration test suites with 0 memory leaks under Valgrind Memcheck.
- **Surface Syntax Ergonomics**: Declaration by assignment, range loops, boolean keywords, expression-bodied functions, and optional semicolons fully implemented.
- **Affine Single-Ownership**: Compile-time move analysis and automated reverse-order scope-exit deallocation cascades fully verified.
- **Dual-Backend Code Generation**: Portable C11 transpilation and native direct x86-64 machine-code generation passing differential parity suites.
- **Foreign Function Interface (FFI-1 through FFI-6)**:
  - In-source `extern "C"` declarations and custom library linking (`-L`/`-l`).
  - Raw pointers (`*const T`, `*mut T`, `*void`, `null`) with const-subtyping.
  - Safe C string conversions (`str_to_cstr`, `str_from_cstr`).
  - Out parameters via address-taking (`&var`).
  - Quarantined `unsafe { ... }` blocks with dereferencing, scaled pointer arithmetic, and casts.
  - Pointer difference semantics (`p1 - p2`).
  - System V AMD64 aggregate pass-by-value ABI (POD structs passed in GPR/XMM registers, atomic stack rollback, struct returns, and hidden `sret` pointers).
- **Optimization Pipeline**: SSA construction, Dominance analysis, SCCP, GVN/CSE, LICM, inlining, and Profile-Guided Optimization (PGO) passing differential checks.
- **Self-Hosting Parity**: Self-hosted bootstrap pipeline verified with bitwise-identical Stage 2 $\equiv$ Stage 3 fixpoint verification (`make test_selfhost_fixpoint`).

---

## 11. Roadmap

Future development directions include:

- **Self-Hosted FFI Bootstrap Parity**: Updating the self-hosted compiler frontend (`selfhost/`) to parse and lower `extern "C"`, raw pointers, and aggregate ABI constructs.
- **Advanced Global Register Allocation**: Iterated register coalescing via graph coloring to minimize spill slots in tight numerical loops.
- **Structured Concurrency**: Native multithreading and message-passing channels integrated with affine single-ownership semantics.
- **Cross-Platform Architectures**: Expanding the direct machine-code backend and calling convention implementation to support ARM64 (AArch64).
- **Extended FFI Capabilities**: Exploring variadic foreign functions (`...`) and direct slice interop across foreign boundaries.

---

## 12. Contributing and License Information

### Repository Structure
- `src/`: Reference compiler implementation in ISO C11:
  - `src/common/`: Shared diagnostics, source error formatting, and timing instrumentation.
  - `src/frontend/`: Lexer, recursive-descent parser, AST structures, module resolver, trait resolver, and ownership scope analysis.
  - `src/ir/`: Quadruples IR, SSA dominance frontiers, optimization passes (SCCP, GVN/CSE, LICM, inlining, PGO), and IR verifier.
  - `src/backend/c11/`: ISO C11 code generator, runtime headers, and standard library preludes.
  - `src/backend/x86_64/`: Native machine code generator, linear scan register allocator, instruction encoder, and ELF64 object emitter.
  - `src/runtime/`: Freestanding assembly runtime (`runtime_start.s`) for standalone Linux executables.
  - `src/main.c`: Top-level CLI driver and compiler entry point.
- `selfhost/`: Self-hosted compiler sources written in Cco (`lexer.cco`, `parser.cco`, `typechecker.cco`, `codegen.cco`, `elf_core.cco`, `x86_64_core.cco`).
- `std/`: Cco standard library modules (`net.cco`, `math.cco`, `io.cco`, `string.cco`, `buffer.cco`, `deque.cco`, `result.cco`, `option.cco`).
- `tests/`: Comprehensive regression, unit, and adversarial test suites:
  - `tests/programs/`: Core integration test programs executed under Valgrind Memcheck.
  - `tests/unit/`: Unit tests for compiler subsystems (lexer, parser, scope analysis, map runtime).
  - `tests/diagnostics/`: Compiler error diagnostic fixtures and runner.
  - `tests/ffi_3c1/`, `tests/ffi_3c2/`, `tests/ffi_4/`, `tests/ffi_5/`, `tests/ffi_6/`: FFI acceptance test suites.
- `docs/`: Technical specifications, architectural decision records, and language reference documents:
  - [`docs/FFI.md`](docs/FFI.md) — Comprehensive Foreign Function Interface specification.
  - [`docs/CCO_LANGUAGE_REFERENCE.md`](docs/CCO_LANGUAGE_REFERENCE.md) — Language grammar and semantic reference.
  - [`docs/CCO_IR_ARCHITECTURE.md`](docs/CCO_IR_ARCHITECTURE.md) — Intermediate representation design.
  - [`docs/CCO_X86_64_BACKEND.md`](docs/CCO_X86_64_BACKEND.md) — Native machine code generation and instruction encoding.

### Companion Repository: `cco-examples`
Practical examples, benchmarks, and algorithms are curated in the companion repository [**Rohinthan/cco-examples**](https://github.com/Rohinthan/cco-examples).

### Development Workflow
1. Ensure all code compiles cleanly under `-Wall -Wextra -Werror -pedantic-errors -std=c11`:
   ```bash
   make clean && make cco
   ```
2. Verify all test suites pass with zero memory leaks:
   ```bash
   make unit_tests
   make test
   ```
3. Submit pull requests or open issues describing enhancements or defect remediation.

---

## License

Cco is open-source software distributed under the terms of the **Apache License 2.0** (`Apache-2.0`).
See the [LICENSE](LICENSE) file for complete details.
