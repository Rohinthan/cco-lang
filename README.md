# Cco Programming Language

A statically typed, natively compiled programming language designed to combine Python-inspired programming ergonomics with systems-level execution and compile-time memory safety.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86--64-brightgreen.svg)]()
[![Build](https://img.shields.io/badge/Build-ISO%20C11%20%7C%20Native%20x86--64-blue.svg)]()
[![Memory Safety](https://img.shields.io/badge/Memory%20Verification-Valgrind%200%20Leaks-success.svg)]()

Cco (pronounced *C-co*, historically designated *C--*) explores a distinctive middle ground in programming language design: the visual clarity and lightweight syntax of high-level scripting languages coupled with the performance, predictable resource usage, and static guarantees of compiled systems languages. 

Cco features a dual-target compilation model:
1. **Direct Native Compilation**: A native x86-64 backend that emits binary machine instructions, constructs standard ELF64 relocatable object files (`.o`), and links standalone Linux executables via an embedded static linker and freestanding syscall runtime (`_start`)—without requiring external toolchains (`gcc`, `as`, `ld`, or `libc.so`).
2. **Standard C11 Transpilation**: A portable source-to-source backend generating clean, standard ISO C11 (`-std=c11 -pedantic-errors`) for auditing, cross-compilation, or deployment with host C compilers (`gcc`, `clang`, `tcc`).

---

## Quick Start: Download, Install & Run

Get up and running with Cco in under a minute:

### 1. Download & Install

```bash
# Clone the repository
git clone https://github.com/Rohinthan/cco-lang.git
cd cco-lang

# Build the compiler executable
make cco

# Install into ~/.local/bin and standard library into ~/.local/lib/cco
make install
```

> [!TIP]
> Ensure `~/.local/bin` is in your `$PATH` (typically default on modern Linux distros, or add `export PATH="$HOME/.local/bin:$PATH"` to your `~/.bashrc`).  
> For system-wide installation across all users, run: `sudo make install PREFIX=/usr/local`

### 2. Write Your First Program (`example.cco`)

```cco
# example.cco
fn square(x: int) -> int = x * x;

x = 10;
y = square(x);
print(y);

total = 0;
for i in 0..5 {
    if i > 0 and i < 4 {
        total += i;
    }
}
print(total);
```

### 3. Compile and Run

```bash
# Compile to a native executable
cco example.cco -o example

# Run the binary
./example
```

Output:
```text
100
6
```

You can also execute programs directly in one step without producing a persistent binary:
```bash
cco example.cco --run
```

---

## Why Cco?

Systems programming has traditionally required developers to choose between two poles:

- **Low-level languages (C, C++)** provide bare-metal execution speed and fine-grained hardware control, but lack compile-time memory safety. Unchecked pointer arithmetic and manual lifecycle management routinely introduce spatial and temporal memory vulnerabilities (buffer overflows, use-after-free, double-free).
- **Managed languages (Python, Go, Java)** provide high developer ergonomics or automated safety through garbage collection or dynamic interpreters. However, they introduce non-deterministic runtime pauses, elevated memory footprints, and execution overhead that can make them unsuitable for latency-sensitive or resource-constrained environments.
- **Modern safe systems languages (Rust)** prove that compile-time memory safety without garbage collection is achievable, but often carry high cognitive overhead (complex lifetime annotations, strict syntactic ceremony) and long compilation times.

Cco explores an alternative balance within this design space:

- **Concise, readable syntax**: First-class declaration-by-assignment, range loops, unparenthesized control flow, expression-bodied functions, and top-level scripts reduce cognitive overhead.
- **Static type checking**: Every variable, parameter, and expression has a statically resolved type. Omitting variable keywords does not introduce dynamic typing or runtime type tags.
- **Compile-time single ownership**: Memory safety is managed deterministically through move semantics, borrowed references (`&T`), and reverse scope-exit destructor cascades. No runtime garbage collector is used.
- **Fast, self-contained compilation**: Direct machine-code encoding and an internal ELF64 static linker compile and link standalone executables in under 15 milliseconds.

The objective is not to replace established languages, but to provide a practical systems language where programs are as concise to write as scripts while executing as bare-metal native binaries.

---

## Language Features Overview

| Area | Status | Description |
| :--- | :---: | :--- |
| **Static Typing** | Implemented | Strongly typed at compile time; primitive scalar types (`int`, `float`, `bool`, `char`, `string`, `void`). |
| **Type Inference** | Implemented | Local types inferred from initializers; validated against explicit annotations when provided. |
| **Declaration by Assignment** | Implemented | Assignment to an unresolved identifier creates a scoped binding (`x = 10;`); reassignment preserves type invariance. |
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
| **Interfaces (Traits)** | Implemented | Monomorphized at compile time; generic calls generate specialized functions with zero vtable dispatch overhead. |
| **F-String Interpolation** | Implemented | String interpolation with expression evaluation, automatic type conversion, and brace escaping (`{{`, `}}`). |
| **POSIX Networking** | Implemented | Core socket primitives (`net_listen`, `net_accept`, `net_recv`, `net_send`, `net_close`, `sleep_ms`) for TCP services. |
| **Native x86-64 Backend** | Implemented | Direct machine code generation adhering to the implemented subset of the System V AMD64 ABI. |
| **SSA Optimization Pipeline** | Implemented | Lengauer-Tarjan dominance, SSA $\phi$-construction, SCCP, GVN/CSE, LICM, inlining, and profile-guided block layout. |
| **Internal ELF64 Linker** | Implemented | Standalone static linker producing executable ELF64 binaries with embedded `_start` syscall runtime (no `ld` or `libc`). |
| **C11 Transpiler** | Implemented | Source-to-source transpilation targeting ISO C11 (`-std=c11 -pedantic-errors`) for GCC, Clang, TCC, and MSVC. |
| **In-Source C FFI** | Planned | Explicit `extern "C"` declaration syntax in Cco source code (currently handled via `.o` object-level linkage). |
| **Multithreading / Concurrency** | Planned | Native threading and thread-safe channels (currently network sockets execute sequentially). |

---

## Language Syntax & Ergonomics

### 1. Variables and Declaration by Assignment

Cco allows variables to be declared upon first assignment without mandatory keywords:

```cco
# Variable creation via assignment
count = 42;               # Inferred as int
pi = 3.14159;             # Inferred as float
name = "Cco";             # Inferred as string
active = true;            # Inferred as bool
```

Subsequent assignments mutate the variable in place:

```cco
count = 50;
count += 5;
```

Explicit type annotations are available whenever desired without requiring `let`:

```cco
limit: int = 100;
ratio: float = 0.5;
```

Classic `let` syntax (`let x = 10;`, `let x: int = 10;`) remains fully supported for backward compatibility.

### 2. Compile-Time Static Type Preservation

Omitting declaration keywords does **not** introduce dynamic typing. The compiler strictly verifies type invariance across reassignments:

```cco
x = 10;
x = "text";               # Compile error: cannot reassign int variable to string
```

Initial values are also checked against explicit annotations:

```cco
val: int = 3.14;          # Compile error: cannot initialize 'int' with 'float'
```

### 3. Functions

Cco supports both concise expression-bodied functions and standard multi-statement blocks:

```cco
# Expression-bodied functions (single expression return)
fn square(x: int) -> int = x * x;
fn add(a: int, b: int) -> int = a + b;
fn is_positive(x: int) -> bool = x > 0;

# Standard block-bodied function
fn clamp(val: int, low: int, high: int) -> int {
    if val < low {
        return low;
    }
    if val > high {
        return high;
    }
    return val;
}
```

### 4. Control Flow and Range Iteration

Parentheses around conditions are optional, and readable boolean keywords are supported:

```cco
running = true;
paused = false;

if running and not paused {
    print("System active");
} else if paused {
    print("System suspended");
} else {
    print("System stopped");
}
```

Range loops iterate cleanly over half-open integer intervals `[start, end)`:

```cco
total = 0;
for i in 0..10 {
    total += i;
}
print(total);             # 45
```

Traditional C-style loops and while loops remain supported:

```cco
while total > 0 {
    total -= 10;
}
```

### 5. Structs and Operator Overloading

Structs are stack-allocated value types with zero heap allocation overhead. They support compile-time operator overloading:

```cco
struct Vec2 {
    x: float;
    y: float;
}

fn operator+(a: Vec2, b: Vec2) -> Vec2 {
    return Vec2 { x: a.x + b.x, y: a.y + b.y };
}

fn operator==(a: Vec2, b: Vec2) -> bool {
    return a.x == b.x and a.y == b.y;
}

p = Vec2 { x: 1.0, y: 2.0 };
q = Vec2 { x: 3.0, y: 4.0 };
sum = p + q;
print(sum.x);             # 4.0
print(p == q);            # false
```

### 6. Tagged Unions (Enums) and Pattern Matching

Enums support unit and payload variants, verified with compile-time exhaustiveness checking:

```cco
enum Shape {
    Circle { radius: float },
    Rect { width: float, height: float },
    Point,
}

fn area(s: &Shape) -> float {
    match s {
        Shape.Circle { radius } => {
            return 3.14159 * radius * radius;
        }
        Shape.Rect { width, height } => {
            return width * height;
        }
        Shape.Point => {
            return 0.0;
        }
    }
}

s = Shape.Rect { width: 4.0, height: 5.0 };
print(area(&s));          # 20.0
```

---

## Memory Safety & Ownership Model

Cco achieves memory safety through **compile-time affine single-ownership and scope-based deallocation**, avoiding runtime garbage collection:

### What the Compiler Enforces:

1. **Unique Ownership**: Every heap-allocated resource (class instances, dynamic arrays, hash tables) has a single owning binding at any point in execution.
2. **Move Semantics**: Transferring an owned variable via assignment or function parameter transfer moves ownership. The compiler's scope analyzer marks the source variable as moved and invalidates subsequent reads:
   ```cco
   class Buffer { size: int; }

   fn consume(b: Buffer) -> void {
       # b is owned and will be deallocated at the end of consume()
   }

   buf = Buffer { size: 1024 };
   consume(buf);          # Ownership transferred
   # print(buf.size);     <- Compile error: use of moved variable 'buf'
   ```
3. **Borrowed References (`&T`)**: Functions can inspect owned instances without taking ownership via borrowed references (`&T`). Borrowed references cannot outlive the owning scope and cannot be returned from functions.
4. **Automated Scope-Exit Cascades**: When an owned resource leaves its defining lexical block without having been moved, the compiler automatically injects destructor and `free()` calls in reverse declaration order.
5. **Collection Ownership Integrity**: Moving elements out of arrays or hash maps without replacement is rejected at compile time to prevent dangling slots. When an array or map leaves scope, all contained objects are deallocated recursively.

### Scope and Limitations:

Cco's current ownership model enforces single ownership, moves, and non-escaping borrowed references. It does not currently implement arbitrary lifetime parameters or non-lexical lifetimes for nested pointers across complex reference graphs.

---

## Compiler Architecture

The Cco compiler operates as an integrated multi-pass pipeline supporting two distinct compilation paths:

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

---

## Native Compilation & Linker

Cco includes a fully self-contained native code generation backend:

- **Target Architecture**: x86-64 (AMD64).
- **Calling Convention**: System V AMD64 ABI subset:
  - Integer/pointer arguments passed in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
  - Floating-point arguments passed in `%xmm0`–`%xmm7`.
  - Return values passed in `%rax` and `%xmm0`.
  - Standard 16-byte stack frame alignment.
  - Callee-saved registers (`%rbx`, `%rsp`, `%rbp`, `%r12`–`%r15`) preserved across calls.
- **Register Allocation**: Linear scan register allocator with automatic stack frame spill-slot assignment.
- **Machine Code Encoder**: Direct binary instruction encoding (REX prefixes, ModR/M, SIB) without invoking an external assembler.
- **ELF64 Object Writer**: Generates valid relocatable ELF64 object files (`.o`) containing `.text`, `.rodata`, `.symtab`, `.strtab`, and `.rela.text` sections.
- **Internal Static Linker (`--use-internal-linker`)**: Directly resolves relocations (`R_X86_64_64`, `R_X86_64_32S`, `R_X86_64_PC32`) and synthesizes an executable ELF header and `PT_LOAD` program headers. It embeds a minimal assembly runtime (`src/runtime_start.s`) utilizing direct Linux kernel syscalls (`sys_write`, `sys_mmap`, `sys_munmap`, `sys_exit`), generating standalone binaries without glibc or GNU `ld`.

---

## Optimization Pipeline

Cco provides a staged optimization pipeline operating on its Static Single Assignment (SSA) intermediate representation:

| Optimization Level | CLI Flag | Passes Included |
| :--- | :---: | :--- |
| **None** | `-O0` | Direct IR lowering to target code without transformation (default). |
| **Local Optimizations** | `-O1`, `-O`, `--opt` | Local constant folding, algebraic simplification, and local dead-code elimination. |
| **SSA & Interprocedural** | `-O2` | Dominance analysis, SSA $\phi$-placement, SCCP, GVN/CSE, natural loop analysis, LICM, call graph analysis, function purity classification (`PURE`, `READONLY`), interprocedural inlining, dead function elimination (DFE), and critical edge splitting. |
| **Profile-Guided (PGO)** | `--profile-generate`<br>`--profile-use=<file>` | Runtime edge profiling, profile serialization, basic block layout reordering for biased branches, and hot-path inlining budget expansion. |

### Diagnostic & Inspection Flags:

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

## Building & Installing from Source

### System Requirements:
- **Operating System**: Linux x86-64 (kernel 3.2+)
- **C Compiler**: GCC 9+ or Clang 10+ (supporting ISO C11)
- **Build System**: GNU Make
- **Optional**: Valgrind 3.15+ (for running memory leak test suites)

### 1. Build the Compiler

```bash
# Clone the repository
git clone https://github.com/Rohinthan/cco-lang.git
cd cco-lang

# Build the compiler executable (cco)
make cco

# Verify local binary
./cco --help
```

### 2. Install to System

#### Option A: User Installation (Recommended)
Installs `cco`, `gcco`, and `cco-link` to `$HOME/.local/bin` and the standard library to `$HOME/.local/lib/cco/std`:

```bash
make install
```

Ensure `~/.local/bin` is in your shell `PATH` (typically default on modern Linux, or append to `~/.bashrc`):
```bash
export PATH="$HOME/.local/bin:$PATH"
```

#### Option B: System-Wide Installation
Installs binaries to `/usr/local/bin` and standard libraries to `/usr/local/lib/cco/std` across all users:

```bash
sudo make install PREFIX=/usr/local
```

### 3. Uninstall

To remove Cco binaries and the standard library from your system:

```bash
# For user-level installation:
make uninstall

# For system-wide installation:
sudo make uninstall PREFIX=/usr/local
```

---

## Basic CLI Usage

Once installed, use `cco` directly from any directory *(or `./cco` if running locally inside the cloned repository)*:

```bash
# 1. Compile to a native binary using the default C11 pipeline
cco program.cco -o program
./program

# 2. Compile directly to an executable and run immediately
cco program.cco --run

# 3. Compile via native x86-64 backend and external linker
cco program.cco --use-native -o program

# 4. Compile and link completely standalone without gcc or ld
cco program.cco --use-internal-linker -o program

# 5. Compile with SSA optimizations (-O2)
cco program.cco --ssa -O2 -o program

# 6. Profile-Guided Optimization (two-phase compilation)
cco program.cco --profile-generate -o program_instr && ./program_instr
cco program.cco --profile-use cco.profile -O2 -o program_opt

# 7. Output pure ISO C11 source code
cco program.cco -o output.c
```

---

## Standard Library API Overview

Cco includes a built-in standard library supporting essential systems tasks:

- **Strings (`std/string.cco`)**: `len`, `concat`, `equals`, `char_at`, `substring`.
- **Math (`std/math.cco`)**: `sqrt`, `pow`, `abs_int`, `abs_float`, `floor`, `ceil`, `min_int`, `max_int`, `min_float`, `max_float`.
- **File I/O (`std/io.cco`)**: `read_file`, `write_file`, `read_line`.
- **System**: `args`, `arg_count`, `program_name`, `to_int`, `to_float`, `random_int`, `random_seed`.
- **POSIX Networking (`std/net.cco`)**:
  - `net_listen(port: int) -> int`: Binds TCP socket with `SO_REUSEADDR` and listens with 128 backlog.
  - `net_accept(server_fd: int) -> int`: Accepts incoming client connection.
  - `net_recv(client_fd: int, max_bytes: int) -> string`: Reads data into managed string with HTTP framing detection.
  - `net_send(client_fd: int, data: string) -> int`: Transmits response bytes.
  - `net_close(fd: int) -> void`: Closes socket descriptor.
  - `sleep_ms(ms: int) -> void`: High-resolution sleep via `nanosleep`.

---

## Testing & Validation

The Cco compiler maintains strict verification standards across all components:

- **100% Memory Verification under Valgrind**: Every integration test program is executed under `valgrind --leak-check=full --error-exitcode=1`. Any memory leak or file descriptor leak immediately fails the test runner.
- **Integration Test Matrix**: **121 / 121 tests passing** across scalar arithmetic, branching, loops, memory moves, borrowed references, structs, classes, maps, enums, POSIX networking, and modern syntax ergonomics.
- **Self-Hosted Lexer Comparison Harness**: **123 / 123 tests passing** via `tests/compare_lexers.sh`. A lexer written in Cco (`selfhost/lexer.cco`) produces token streams 100% byte-identical to the C reference lexer across all test programs.
- **Compiler Bootstrap Validation**: Self-hosted parser, typechecker, and code generator verify pipeline bootstrap parity.
- **Differential Optimization Verification**: Tests confirm that `-O0`, `-O1`, `-O2`, and PGO builds produce bitwise identical stdout results across differential program suites.

Run the test suites locally:

```bash
# Run unit tests (optimizer, SSA, linker, syntax) under Valgrind
make unit_tests

# Run full integration test suite, bootstrap checks, and network tests
make test
```

---

## Performance & Benchmarking

Cco includes a reproducible benchmarking harness located in the companion repository [`cco-examples/bench`](https://github.com/Rohinthan/cco-examples/tree/main/bench). The harness compares execution runtimes, compilation latencies, and binary sizes across equivalent implementations in C (`gcc -O3`), C++, Rust (`--release`), Python 3.12, and Cco.

### Empirical Characteristics:

- **Compilation Speed**: The native x86-64 backend processes, optimizes, and links standalone executables in **under 15 milliseconds** on modern x86-64 hardware.
- **Linker Latency**: The internal static linker resolves relocations and writes ELF binaries in **0.09 to 0.15 ms**, compared to 15.0 to 25.0 ms when invoking external linkers.
- **Runtime Execution**: In numerical loops and arithmetic kernels, Cco native code executes with throughput comparable to GCC `-O1` out-of-the-box, with SSA optimizations (LICM, GVN/CSE) and inlining providing significant instruction count reductions.

For raw benchmark data and methodology, see [`cco-examples/bench/results`](https://github.com/Rohinthan/cco-examples/tree/main/bench/results).

---

## Project Status & Roadmap

### Current Status:
- **Core Language & Type System**: Stable and verified across 121 integration test suites.
- **Python-Inspired Surface Syntax**: Implemented and verified (declaration by assignment, range loops, boolean keywords, expression bodies).
- **Affine Ownership & Memory Safety**: Implemented and verified (0 leaks under Valgrind Memcheck).
- **Native x86-64 Backend & Internal Linker**: Implemented for the supported System V AMD64 ABI subset.
- **Optimization Pipeline (SSA, LICM, PGO)**: Implemented and passing differential verification.

### Roadmap & Planned Work:
- **In-Source C FFI**: Native `extern "C" fn name(...) -> type;` declarations directly in `.cco` source code.
- **Cross-Platform Architectures**: Expanding the direct machine-code backend to support ARM64 (AArch64).
- **Advanced Global Register Allocation**: Iterated register coalescing via graph coloring to reduce spill code in high-pressure loops.
- **Structured Concurrency**: Native multithreading and message-passing channels integrated with affine ownership semantics.

---

## Technical Documentation Index

Detailed architectural specifications and engineering reports are available in [`docs/`](docs/):

- [`docs/CCO_LANGUAGE_REFERENCE.md`](docs/CCO_LANGUAGE_REFERENCE.md) — Comprehensive language grammar and semantic reference.
- [`docs/CCO_IR_ARCHITECTURE.md`](docs/CCO_IR_ARCHITECTURE.md) — Design of the quadruples intermediate representation.
- [`docs/CCO_X86_64_BACKEND.md`](docs/CCO_X86_64_BACKEND.md) — Native machine code generation and instruction encoding.
- [`docs/CCO_PHASE3B_INTERNAL_ELF64_LINKER.md`](docs/CCO_PHASE3B_INTERNAL_ELF64_LINKER.md) — Internal ELF64 static linker architecture and `_start` syscall runtime.
- [`docs/CCO_PHASE6_ADVANCED_SSA_OPTIMIZATION.md`](docs/CCO_PHASE6_ADVANCED_SSA_OPTIMIZATION.md) — SSA construction, dominance analysis, SCCP, GVN/CSE, and LICM.
- [`docs/CCO_PHASE8_PROFILE_GUIDED_OPTIMIZATION.md`](docs/CCO_PHASE8_PROFILE_GUIDED_OPTIMIZATION.md) — Profile-guided optimization and branch layout reordering.
- [`docs/CCO_PHASE10_LANGUAGE_SYNTAX_DESIGN.md`](docs/CCO_PHASE10_LANGUAGE_SYNTAX_DESIGN.md) & [`docs/CCO_PHASE11_LANGUAGE_SYNTAX_DESIGN.md`](docs/CCO_PHASE11_LANGUAGE_SYNTAX_DESIGN.md) — Python-inspired surface syntax design rationale.
- [`docs/NETWORKING_AUDIT.md`](docs/NETWORKING_AUDIT.md) — POSIX socket runtime architecture and leak-free verification.

---

## Companion Repository: `cco-examples`

Application examples, algorithm implementations, and benchmarks are curated in the companion repository [**Rohinthan/cco-examples**](https://github.com/Rohinthan/cco-examples):

- **`codebase/`**: 251+ comprehensive example programs demonstrating language features from basics to advanced server architectures.
- **`algorithms/`**: 65 machine learning and scientific computing implementations (linear regression, k-means, MLP, DBSCAN, decision trees, A* search, etc.) verified against NumPy and scikit-learn.
- **`bench/`**: Multi-language performance benchmark suite and automated statistical measurement harness.
- **`examples/`**: Tutorial programs and multi-file module imports.

---

## Contributing

Contributions to the Cco compiler are welcome.

### Repository Organization:
- `src/`: Core compiler implementation (lexer, parser, AST, semantic analyzer, IR, SSA optimizer, x86-64 backend, ELF generator, internal linker).
- `selfhost/`: Self-hosted compiler sources written in Cco (`lexer.cco`, `parser.cco`, `typechecker.cco`, `codegen.cco`).
- `std/`: Cco standard library modules (`net.cco`, `math.cco`, `io.cco`, `string.cco`).
- `tests/`: Official unit test suites (`tests/unit/`) and integration test programs (`tests/programs/`).
- `docs/`: Technical specifications and design documentation.

### Development Workflow:
1. Ensure all code compiles cleanly under `-Wall -Wextra -Werror -pedantic-errors -std=c11`:
   ```bash
   make clean && make cco
   ```
2. Verify all test suites pass with zero memory leaks:
   ```bash
   make unit_tests
   make test
   ```
3. Open an issue or pull request describing the proposed enhancement or bug fix.

---

## License

Cco is open source software licensed under the **GNU General Public License v3.0 or later** (GPL-3.0-or-later).  
See the [`LICENSE`](LICENSE) file for the full license text.
