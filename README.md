# Cco (C--) Programming Language & Compiler

A modern statically-typed systems programming language combining **Python-like source ergonomics** with **bare-metal native execution**, **compile-time single ownership** (zero garbage collection), an **advanced SSA optimization pipeline**, and a **built-in x86-64 ELF64 backend and static linker**.

Cco compiles your code in two complementary ways:
1. **Direct Native Compilation**: Native x86-64 machine code generation, ELF64 object file emission, and standalone ELF linking with an embedded Linux syscall runtime (`_start`) — zero external dependencies on `gcc`, `as`, `ld`, or `libc.so`.
2. **Standard ISO C11 Transpilation**: Highly portable, readable C11 source generation for cross-platform targets (`gcc`, `clang`, `tcc`, MSVC).

---

### Key References & Repositories

- **Companion Code, Examples & Benchmarks**: [**Rohinthan/cco-examples**](https://github.com/Rohinthan/cco-examples) — 250+ complete Cco programs, 65 AI/ML algorithms, and multi-language benchmarks (Cco vs. C vs. C++ vs. Rust vs. Python).
- **Language Syntax Design**: [`docs/CCO_PHASE10_LANGUAGE_SYNTAX_DESIGN.md`](docs/CCO_PHASE10_LANGUAGE_SYNTAX_DESIGN.md) & [`docs/CCO_PHASE11_LANGUAGE_SYNTAX_DESIGN.md`](docs/CCO_PHASE11_LANGUAGE_SYNTAX_DESIGN.md).
- **Compiler Architecture & IR**: [`docs/CCO_IR_ARCHITECTURE.md`](docs/CCO_IR_ARCHITECTURE.md) & [`docs/CCO_DESIGN_IMPLEMENTATION_EVALUATION.md`](docs/CCO_DESIGN_IMPLEMENTATION_EVALUATION.md).
- **Native Backend & Linker**: [`docs/CCO_X86_64_BACKEND.md`](docs/CCO_X86_64_BACKEND.md) & [`docs/CCO_PHASE3B_INTERNAL_ELF64_LINKER.md`](docs/CCO_PHASE3B_INTERNAL_ELF64_LINKER.md).
- **Optimization Pipeline (SSA, LICM, PGO)**: [`docs/CCO_PHASE6_ADVANCED_SSA_OPTIMIZATION.md`](docs/CCO_PHASE6_ADVANCED_SSA_OPTIMIZATION.md) & [`docs/CCO_PHASE8_PROFILE_GUIDED_OPTIMIZATION.md`](docs/CCO_PHASE8_PROFILE_GUIDED_OPTIMIZATION.md).

---

## 1. Quick Look: Modern Cco

Cco feels as clean and concise as Python, but runs at native compiled speed with complete static type safety:

```cco
# A complete, executable Cco program
name = "Cco";
count: int = 5;

fn square(x: int) -> int = x * x;

print(f"Welcome to {name}!");

for i in 0..count {
    if i % 2 == 0 and i > 0 {
        print(f"Even square: {i}^2 = {square(i)}");
    }
}
```

```bash
$ ./cco program.cco -o program
$ ./program
Welcome to Cco!
Even square: 2^2 = 4
Even square: 4^2 = 16
```

---

## 2. Quick Start & Execution Guide

### Prerequisites
- Linux (x86-64) or POSIX environment (macOS, FreeBSD, WSL)
- `gcc` (v9+) or `clang` (for building the compiler itself or using the C11 transpiler backend)
- `make`
- `valgrind` (optional, for memory verification)

### Building the Compiler

Build the Cco compiler from source:
```bash
git clone https://github.com/Rohinthan/cco-lang.git
cd cco-lang
make cco
```
The compiler binary is generated at `./cco`.

---

### Compilation Modes

#### 1. Default Native Compilation (`./cco <file.cco> -o <binary>`)
Cco compiles directly to a standalone native binary in a single command:
```bash
./cco hello.cco -o hello
./hello
```
If `-o` is omitted, the binary name is automatically derived from the input file:
```bash
./cco hello.cco
./hello
```

#### 2. Development JIT Mode (`./cco <file.cco> --run`)
Rapidly compile and execute in memory without producing a persistent binary:
```bash
./cco hello.cco --run
```

#### 3. Direct Native x86-64 Backend (`--use-native`)
Emits System V AMD64 binary object files (`.o`) directly via Cco's internal machine code encoder:
```bash
# Compile to native ELF64 object file (.o) without host assembler
./cco hello.cco --use-native -c -o hello.o

# Compile and link using external host linker
./cco hello.cco --use-native -o hello
```

#### 4. Standalone Internal ELF64 Linker (`--use-internal-linker`)
Generates standalone Linux x86-64 executables with zero host toolchain dependencies (no `gcc`, `as`, `ld`, or `libc.so`):
```bash
./cco hello.cco --use-internal-linker -o hello
```

#### 5. Advanced SSA Optimizations (`-O1`, `-O2`, `--ssa`)
Activates Cco's SSA optimization passes (SCCP, GVN/CSE, LICM, inlining):
```bash
./cco hello.cco --ssa -O2 -o hello
```

#### 6. Profile-Guided Optimization (`--profile-generate`, `--profile-use`)
Two-phase profile-guided compilation for branch probability reordering and hot-path inlining:
```bash
# Phase 1: Compile with profiling instrumentation
./cco server.cco --profile-generate --profile-file server.prof -o server_instr
./server_instr  # Run representative workload to generate profile

# Phase 2: Compile with profile feedback
./cco server.cco --profile-use server.prof -O2 -o server_optimized
```

#### 7. Pure C11 Source Transpilation (`./cco <file.cco> -o <file.c>`)
Emits clean, standard ISO C11 source for auditing or distribution:
```bash
./cco hello.cco -o output.c
```

---

## 3. Surface Syntax & Language Ergonomics

Cco blends Python-like source ergonomics with the predictability and performance of a compiled systems language.

### Declaration by Assignment (No Mandatory `let`)
Variables can be declared by simply assigning to them. The compiler statically infers the type from the right-hand side:
```cco
x = 10;                   # Inferred as int
pi = 3.14159;             # Inferred as float
name = "Cco Language";    # Inferred as string
active = true;            # Inferred as bool
```
Subsequent assignments to the same variable in the same scope reassign it:
```cco
x = 20;                   # Mutates x
x += 5;                   # In-place compound modification
```

### Static Type Safety
Omitting `let` does **not** make Cco dynamically typed. The type system remains strictly checked at compile time:
```cco
x = 42;
x = "invalid";            # Compile-time error: cannot reassign int variable to string
```

### Explicit Type Annotations
Explicit annotations are available whenever desired, without needing `let`:
```cco
x: int = 10;
rate: float = 0.05;
greeting: string = "Hello";
```
Initializers are validated against explicit annotations at compile time:
```cco
count: int = 3.14;        # Compile-time error: cannot initialize 'int' with 'float'
```

### Full Backward Compatibility (`let`)
Classic `let` syntax remains 100% valid and supported:
```cco
let a = 10;
let b: int = 20;
```

### Top-Level Script Syntax
Entry-point files require no boilerplate `fn main() -> int { ... return 0; }`. Statements at the file root execute sequentially:
```cco
x = 10;
y = 20;
print(f"Total: {x + y}");
```
- Helper functions, classes, and structs can be declared alongside top-level code in any order.
- Hand-written `fn main()` remains fully supported for multi-file architectures or custom entry points.
- The compiler automatically desugars top-level statements into a synthesized native entry point with affine ownership cleanup and exit code `0`.

### Range Loops (`for .. in`)
Iterate cleanly over half-open integer intervals `[start, end)`:
```cco
total = 0;
for i in 0..10 {
    total += i;
}
print(total);             # 45
```
Traditional C-style counting loops remain supported:
```cco
for (let i = 0; i < 10; i++) {
    total += i;
}
```

### Boolean Keywords & Unparenthesized Conditions
Conditionals do not require parentheses, and support readable boolean keywords alongside traditional operators:
```cco
active = true;
ready = false;

if active and not ready {
    print("Initializing...");
}

if active or ready {
    print("Ready to execute.");
}

while running {
    tick();
}
```

| Modern Syntax | C-Style Equivalent | Meaning |
| :--- | :--- | :--- |
| `a and b` | `a && b` | Logical AND |
| `a or b` | `a \|\| b` | Logical OR |
| `not a` | `!a` | Logical NOT |
| `for i in 0..n` | `for (i = 0; i < n; i++)` | Range iteration |
| `# comment` | `// comment` | Single-line comment |

### Expression-Bodied Functions
Functions returning a single expression can be defined concisely with `=`:
```cco
fn square(x: int) -> int = x * x;
fn add(a: int, b: int) -> int = a + b;
fn is_positive(x: int) -> bool = x > 0;
```
Standard block bodies `{ ... }` remain used for multi-statement functions.

### Python-Style F-Strings
Format strings cleanly with expression evaluation and automatic type conversion:
```cco
name = "Alice";
score = 98.5;
rank = 1;
print(f"Player {name} achieved rank #{rank} with score {score}!");
```
Literal braces can be escaped using `{{` and `}}`.

---

## 4. Systems Architecture & Memory Model

### Deterministic Single Ownership (Zero GC)
Cco guarantees memory safety and predictable resource management without a garbage collector through **compile-time affine single-ownership**:
- **Unique Ownership**: Heap-allocated objects (`class`, dynamic arrays, maps) have a single owner.
- **Move Semantics**: Assigning or passing an owned object moves ownership. The source becomes invalid.
- **Borrowed References (`&T`)**: Pass references to functions without transferring ownership.
- **Automated Deallocation**: Destructors and memory frees are injected at the precise lexical scope exit where a resource ceases to be live.
- **No Dangling Pointers**: Use-after-move, double-free, and returning borrowed references are rejected at compile time.

```cco
class Buffer {
    data: int;
}

fn inspect(buf: &Buffer) -> void {
    print(buf.data);      # Borrowed access: ownership remains with caller
}

fn consume(buf: Buffer) -> void {
    # Owned: buf will be deallocated at the end of consume()
}

b = Buffer { data: 42 };
inspect(&b);              # Borrowed reference
consume(b);               # Ownership moved
# inspect(&b);            <- Compile error: use of moved variable 'b'
```

### Tagged Unions / Enums & Pattern Matching
Native tagged unions (`enum`) supporting unit and payload variants, verified with compile-time exhaustiveness checking:
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
```

### Structs (Stack Value Types) & Operator Overloading
Lightweight stack-allocated structs with zero heap overhead, supporting 10 compile-time operator overloads (`+`, `-`, `*`, `/`, `==`, `!=`, `<`, `>`, `<=`, `>=`):
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

### Interfaces & Compile-Time Monomorphization
Interfaces define contracts implemented by classes. Generic functions accepting `&impl Interface` are monomorphized at compile time, generating specialized functions per concrete type with **zero virtual table dispatch overhead**:
```cco
interface Printable {
    fn describe(self) -> void;
}

class User {
    id: int;
    fn describe(self) -> void {
        print(f"User({self.id})");
    }
}

impl Printable for User;

fn output(item: &impl Printable) -> void {
    item.describe();      # Monomorphized call directly to User.describe
}
```

---

## 5. Compiler Architecture & Native Backend

```
Cco Source Code (.cco)
       │
       ▼
 [Lexer & Parser] ── Python-style ergonomics, declaration-by-assignment, ranges
       │
       ▼
[AST & Type Check] ── Static type checking, scope analysis, single-ownership verification
       │
       ├────────────────────────────────────────┐
       ▼                                        ▼
[Cco IR Builder]                        [C11 Transpiler]
       │                                        │
[SSA Construction] (phi-nodes, dom tree)        ▼
       │                                 Portable ISO C11 (.c)
[Optimization Pipeline]                         │
  ├── SCCP (constant propagation)               ▼
  ├── GVN / CSE (redundancy elimination)   Host GCC / Clang
  ├── LICM (loop invariant hoisting)            │
  ├── Inlining & Purity Analysis                ▼
  └── PGO (profile branch layout)          Native Binary
       │
       ▼
[x86-64 Codegen] (System V AMD64 ABI, linear scan regalloc)
       │
       ├────────────────────────────────────────┐
       ▼                                        ▼
[ELF64 Object Writer]                   [Internal Static Linker]
  Emits relocatable .o files              Embeds freestanding _start runtime
       │                                  Produces standalone Linux binary
       ▼                                        │
  Host Linker (ld/gcc)                          ▼
       │                                Standalone ELF64 Binary
       └────────────────────────────────────────┘
```

### Compiler Optimization Capabilities

| Pass / Technology | Implementation | Description |
| :--- | :--- | :--- |
| **Lengauer-Tarjan Dominance** | [`src/ir_dominance.c`](src/ir_dominance.c) | Computes immediate dominators, dominance frontiers, and post-dominance. |
| **SSA Construction** | [`src/ir_ssa.c`](src/ir_ssa.c) | Places $\phi$-functions at iterated dominance frontiers; out-of-SSA critical edge splitting. |
| **SCCP** | [`src/ir_ssa_opt.c`](src/ir_ssa_opt.c) | Sparse Conditional Constant Propagation folds constant conditionals and prunes dead edges. |
| **GVN / CSE** | [`src/ir_ssa_opt.c`](src/ir_ssa_opt.c) | Global Value Numbering eliminates redundant common subexpressions across basic blocks. |
| **Natural Loop Analysis & LICM** | [`src/ir_loop.c`](src/ir_loop.c) | Identifies loops, computes loop nest trees, and hoists invariant instructions to preheaders. |
| **Interprocedural Analysis & Inlining** | [`src/ir_ipa.c`](src/ir_ipa.c) | Computes call graph SCCs, classifies function purity (`PURE`, `READONLY`), and inlines hot calls. |
| **Profile-Guided Optimization (PGO)** | [`src/ir_profile.c`](src/ir_profile.c) | Profiles edge execution counters to reorder basic blocks and bias hot branch layouts. |
| **ELF64 Internal Linker** | [`src/x86_64_link.c`](src/x86_64_link.c) | Resolves symbols and relocations directly, emitting standalone executables without `ld`. |

---

## 6. Standard Library

### String Functions
| Function | Signature | Description |
| :--- | :--- | :--- |
| `len` | `len(s: string) -> int` | Character count of string |
| `concat` | `concat(a: string, b: string) -> string` | Concatenates two strings |
| `equals` | `equals(a: string, b: string) -> bool` | Checks string equality |
| `char_at` | `char_at(s: string, i: int) -> char` | Character at index `i` with bounds check |
| `substring` | `substring(s: string, start: int, end: int) -> string` | Substring slice |

### Math Functions
| Function | Signature | Description |
| :--- | :--- | :--- |
| `sqrt` | `sqrt(x: float) -> float` | Square root |
| `pow` | `pow(base: float, exp: float) -> float` | Power function |
| `abs_int` / `abs_float` | `abs_int(x: int) -> int` | Absolute value |
| `floor` / `ceil` | `floor(x: float) -> float` | Floor / Ceiling |
| `min_int` / `max_int` | `min_int(a: int, b: int) -> int` | Minimum / Maximum of two integers |
| `min_float` / `max_float` | `min_float(a: float, b: float) -> float` | Minimum / Maximum of two floats |

### File I/O & System
| Function | Signature | Description |
| :--- | :--- | :--- |
| `read_file` | `read_file(path: string) -> string` | Reads entire file into string |
| `write_file` | `write_file(path: string, content: string) -> bool` | Writes string content to file |
| `read_line` | `read_line() -> string` | Reads one line from standard input |
| `args` | `args() -> string[]` | Returns command-line argument array |
| `arg_count` | `arg_count() -> int` | Number of command-line arguments |
| `program_name` | `program_name() -> string` | Invocation name (`argv[0]`) |
| `to_int` / `to_float` | `to_int(s: string) -> int` | Numeric string parsing |
| `random_int` | `random_int(min: int, max: int) -> int` | Random integer in `[min, max]` |
| `random_seed` | `random_seed(seed: int) -> void` | Seeds random number generator |

### Native POSIX Networking (`std/net.cco`)
High-performance non-blocking POSIX socket primitives for TCP network services:
- `net_listen(port: int) -> int`: Binds `0.0.0.0:port` with `SO_REUSEADDR` and listens with 128-connection backlog.
- `net_accept(server_fd: int) -> int`: Accepts incoming connection returning `client_fd`.
- `net_recv(client_fd: int, max_bytes: int) -> string`: Reads bytes into managed string with HTTP framing detection.
- `net_send(client_fd: int, data: string) -> int`: Transmits response bytes over the socket.
- `net_close(fd: int) -> void`: Closes socket file descriptor.
- `sleep_ms(ms: int) -> void`: High-resolution sleep via `nanosleep`.

```cco
# Minimal TCP HTTP Server in Modern Cco
fn handle(client_fd: int) -> void {
    req = net_recv(client_fd, 0);
    if len(req) == 0 {
        net_close(client_fd);
        return;
    }
    body = "{\"status\": \"ok\", \"message\": \"Hello from Cco!\"}\n";
    resp = f"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n{body}";
    net_send(client_fd, resp);
    net_close(client_fd);
}

server_fd = net_listen(8080);
if server_fd >= 0 {
    print("Serving on http://127.0.0.1:8080");
    while true {
        client = net_accept(server_fd);
        if client >= 0 {
            handle(client);
        }
    }
    net_close(server_fd);
}
```

---

## 7. Verification & Test Suite Matrix

Every commit is verified with `make test` and `make unit_tests` under **Valgrind Memcheck** (`valgrind --leak-check=full --error-exitcode=1`). Any single byte leak fails the build:

```bash
$ make unit_tests
Running Profile-Guided Optimization (PGO) Unit Tests...
All PGO unit tests passed successfully.
Running Syntax Ergonomics Unit Tests...
All Syntax Ergonomics Unit Tests Passed!
All unit tests passed with 0 errors and 0 memory leaks.

$ make test
Testing 01_hello... PASSED (Diff Clean + 0 Leaks)
...
Testing 106_syntax_ergonomics... PASSED (Diff Clean + 0 Leaks)
Testing 107_syntax_range_loops... PASSED (Diff Clean + 0 Leaks)
Testing 108_syntax_boolean_precedence... PASSED (Diff Clean + 0 Leaks)
Testing 109_syntax_expression_bodies... PASSED (Diff Clean + 0 Leaks)
Testing 110_syntax_missing_brace_ERROR... PASSED (Compilation Failed as Expected)
Testing 111_syntax_declaration_assignment... PASSED (Diff Clean + 0 Leaks)
Testing 112_syntax_explicit_type_annotation... PASSED (Diff Clean + 0 Leaks)
Testing 113_syntax_undefined_read_ERROR... PASSED (Compilation Failed as Expected)
Testing 114_syntax_type_reassign_mismatch_ERROR... PASSED (Compilation Failed as Expected)
Testing 115_syntax_annotation_mismatch_ERROR... PASSED (Compilation Failed as Expected)
Cco Native POSIX Networking & FD Leak Suite: 6 Passed, 0 Failed
Total Suite Summary: 121 Passed, 0 Failed (0 Leaks + 0 FD Leaks)
```

| Phase / Feature | Focus | Result | Valgrind Status |
| :--- | :--- | :---: | :---: |
| **Phase 10 & 11 Ergonomics** | Declaration assignment, explicit annotations, range loops, boolean keywords | **PASS** | 0 Bytes Leaked |
| **Phase 8 PGO** | Profile instrumentation, deserialization, biased block reordering | **PASS** | 0 Bytes Leaked |
| **Phase 7 Inlining & IPA** | Callgraph SCC, function purity, multi-return inlining | **PASS** | 0 Bytes Leaked |
| **Phase 6 SSA Optimizations** | SCCP constant propagation, GVN/CSE, natural loop analysis, LICM | **PASS** | 0 Bytes Leaked |
| **Phase 5 SSA Construction** | Lengauer-Tarjan dominance, $\phi$-placement, out-of-SSA destruction | **PASS** | 0 Bytes Leaked |
| **Phase 3B Internal Linker** | Freestanding `_start` runtime, standalone ELF64 executable emission | **PASS** | 0 Bytes Leaked |
| **Phase 3A ELF64 Codegen** | Relocatable `.o` emission, System V AMD64 ABI, linear scan regalloc | **PASS** | 0 Bytes Leaked |
| **POSIX Networking** | TCP socket streaming, concurrent request isolation, leak-free teardown | **PASS** | 0 Bytes Leaked (0 FD Leaks) |
| **Single Ownership** | Move semantics, borrow checking, automated scope-exit cascades | **PASS** | 0 Bytes Leaked |

---

## 8. Companion Repository: `cco-examples`

All applications, code showcases, and scientific computing algorithms are curated in the companion repository [**Rohinthan/cco-examples**](https://github.com/Rohinthan/cco-examples):

```
cco-examples/
├── codebase/                    # 251+ comprehensive Cco language programs
│   ├── 01_hello.cco ... 100_while_break.cco
│   ├── 101_while_continue.cco ... 244_top_level_script_array_sum.cco
│   └── 245_syntax_range_loops.cco ... 251_native_speed_demo.cco
│
├── algorithms/                  # 65 Machine Learning & Scientific Computing Implementations
│   ├── 01_linear_regression.cco ... 13_xgboost_lightgbm.cco
│   ├── 14_kmeans.cco ... 38_mixture_of_experts.cco
│   └── 39_a_star_search.cco ... 65_ivf_pq_vector_index.cco
│
├── bench/                       # Multi-language performance benchmark suite (Cco vs C, C++, Rust, Python)
└── examples/                    # Introductory tutorials & multi-file import showcases
```

---

## 9. License

Cco is licensed under the **GNU General Public License v3.0 or later**.  
See [`LICENSE`](LICENSE) for the full license text.
