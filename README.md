# Cco (C--) Programming Language & Compiler

A statically-typed systems programming language and source-to-source compiler that transpiles Cco source code (`.cco`) into portable standard ISO C11 (`.c`), compiled to native machine binaries using `gcc` or `clang`.

### Key References & Repositories

- **Companion Code & Algorithm Repository**: [**Rohinthan/cco-examples**](https://github.com/Rohinthan/cco-examples) — 240+ complete Cco programs and 65 AI/ML algorithms verified with zero Valgrind memory leaks.
- **Beginner's Guide & Tutorial**: [`docs/LEARN.md`](docs/LEARN.md) — Step-by-step introduction across 10 complete programs.
- **Mathematical Correctness Report**: [`docs/ML_ALGORITHM_CORRECTNESS_VERIFICATION.md`](docs/ML_ALGORITHM_CORRECTNESS_VERIFICATION.md) — Independent Python/NumPy/scikit-learn verification report for all 65 algorithms.
- **Networking Architecture & Audit**: [`docs/NETWORKING_AUDIT.md`](docs/NETWORKING_AUDIT.md) — POSIX socket runtime specification and connection lifecycle analysis.

---

## 1. Quick Start & Execution Guide

### Prerequisites
- Linux (Ubuntu 20.04+) or POSIX environment (macOS, FreeBSD, WSL)
- `gcc` (v9+) or `clang`
- `make`
- `valgrind` (optional, for memory verification)

### Building the Compiler

Build the Cco compiler from source:
```bash
git clone https://github.com/Rohinthan/cco-lang.git
cd cco-lang
make cco
```

The compiled binary is generated at `./cco`.

---

### Executing Programs with `./cco <file.cco> --run`

To compile and execute a Cco program in a single step, use the `--run` flag:

```bash
./cco examples/01_hello_world.cco --run
```

#### Why Use the `--run` Flag?

Cco is a source-to-source compiler targeting standard ISO C11 rather than an interpreter or bytecode virtual machine. Under standard operation, `./cco input.cco -o output.c` only emits an intermediate C source file.

The `--run` flag automates the entire compile-and-execute pipeline into a single command:
1. **Source Transpilation**: Parses `.cco` source code, performs module imports, validates trait monomorphization, tracks ownership/borrow scopes, and generates standard ISO C11 code at `build/output.c`.
2. **Background Native Compilation**: Invokes the C compiler directly with optimization and strict conformance flags:
   ```bash
   gcc -O3 -Wall -Wextra -std=c11 build/output.c -o build/cco_out -lm
   ```
3. **Immediate Execution**: Runs the resulting machine binary (`./build/cco_out`), directs program output to stdout/stderr, and forwards the exit status code.

This flag provides a rapid script-like iteration cycle during development and testing without requiring manual multi-step C compiler invocations.

---

### Standalone Native Compilation (Production Workflow)

For production deployment and standalone distribution where the Cco compiler is not present on the target host:

```bash
# Step 1: Transpile Cco source to standard ISO C11
./cco examples/01_hello_world.cco -o build/hello.c

# Step 2: Compile to a standalone native binary
gcc -O3 -Wall -Wextra -Werror -pedantic-errors -std=c11 build/hello.c -o bin/hello -lm

# Step 3: Run standalone binary directly
./bin/hello
```

---

### Running the Test Suite

Run the full unit test suite, self-hosting verification, and integration tests under Valgrind:

```bash
make test
```

---

## 2. Companion Repository: `cco-examples`

The companion repository [**Rohinthan/cco-examples**](https://github.com/Rohinthan/cco-examples) hosts the comprehensive test and application corpus for the Cco language:

```
cco-examples/
├── codebase/                    # 240+ programs covering the complete language surface
│   ├── 01_hello.cco ... 100_while_break.cco
│   ├── 101_while_continue.cco ... 180_struct_alignment_optimized.cco
│   ├── 181_enum_level_match.cco ... 220_cpp_strings_and_formatting.cco
│   ├── 221_cpp_intro_procedural_vs_oop.cco ... 233_live_socket_tcp_server.cco
│   ├── 234_scope_exit_control_flow_gauntlet.cco ... 240_stateful_map_db_http_server.cco
│   └── 241_linear_regression_variants.cco
│
├── algorithms/                  # 65 Machine Learning & Scientific Computing Implementations
│   ├── 01_linear_regression.cco ... 13_xgboost_lightgbm.cco
│   ├── 14_kmeans.cco ... 24_logistic_multinomial.cco
│   ├── 25_mlp.cco ... 38_mixture_of_experts.cco
│   ├── 39_a_star_search.cco ... 46_stochastic_gradient_descent.cco
│   └── 47_isolation_forest.cco ... 65_ivf_pq_vector_index.cco
│
└── tests/correctness/           # Mathematical correctness test suites vs Python references
```

### Algorithm Verification Summary

All 65 algorithms in `cco-examples` compile under strict ISO C11 flags (`-Wall -Wextra -Werror -pedantic-errors -std=c11 -lm`) and execute with **0 memory leaks (0 bytes leaked)** under Valgrind.

Coverage breakdown against independent Python 3.12 (NumPy, scikit-learn, SciPy) references:
- **Tier 1 (18 algorithms / 27.7%)**: Full end-to-end mathematical verification against reference outputs ($< 10^{-5}$ tolerance).
- **Tier 2 (14 algorithms / 21.5%)**: Core mathematical subroutines verified against NumPy/analytical references.
- **Tier 3 (33 algorithms / 50.8%)**: Empirical domain convergence, policy stability, and leak-free verification.

---

## 3. Core Architectural Principles

1. **Native C Speed**: Source-to-source transpilation to flat ISO C11 source. No interpreter loop, no bytecode dispatch, and no runtime garbage collector overhead.
2. **Deterministic Memory Safety**: Compile-time single ownership with move semantics, borrowed references (`&T`), and automated scope-exit deallocation cascades.
3. **Value Types and Collections**: Stack-allocated lightweight structs (`struct`), heap-allocated classes (`class`), open-addressing hash maps (`map[K]V`), and growable dynamic arrays (`Token[]`).
4. **Compile-Time Polymorphism**: Interface monomorphization generating specialized C functions with zero runtime vtable overhead.
5. **Selective Prelude Emission**: Generates only the runtime helper functions (`__cco_*`) required by the specific AST nodes present in the source file.

---

## 4. Language Features

### Type Inference for `let`
Type annotations on `let` statements are optional. The compiler infers the static type directly from the initializer:

```cco
let x = 10;                           // inferred: int
let name = "hello";                   // inferred: string
let pi = 3.14;                        // inferred: float
let done = true;                      // inferred: bool
let p = Point { x: 1, y: 2 };         // inferred: Point
let arr = alloc(int, 5);              // inferred: int[]
let m = map_new(string, int);         // inferred: map[string]int
```

Explicit type annotations (`let x: int = 10;`) remain supported.

---

### Compound Assignment and Increment / Decrement
Supports in-place modification for scalar types, strings, and structs:

```cco
total += i;      // desugars to: total = total + i;
total -= i;      // desugars to: total = total - i;
total *= 2;      // desugars to: total = total * 2;
total /= 2;      // desugars to: total = total / 2;
total %= 3;      // desugars to: total = total % 3;

msg += " world"; // for strings: msg = concat(msg, " world"); (frees previous string buffer)
p += q;          // for structs: resolves via operator+

i++;             // desugars to: i = i + 1; (statement only)
i--;             // desugars to: i = i - 1; (statement only)
```

Example comparison:
```cco
// Idiomatic Cco v19.0
fn main() -> int {
    let total = 0;
    for (let i = 0; i < 10; i++) {
        total += i;
    }
    print(total);
    return 0;
}
```

---

### Operator Overloading for Structs
Supports compile-time operator overloading on value-type structs across 10 operators:
- **Binary Arithmetic**: `+`, `-`, `*`, `/`
- **Equality Comparison**: `==`, `!=`
- **Ordering Comparison**: `<`, `>`, `<=`, `>=`
- **Unary Negation**: `-`

Operator overloads are restricted to stack-allocated `struct` value types. Classes rely on move/borrow semantics to avoid unintended heap allocations.

```cco
struct Vec2 {
    x: float;
    y: float;
}

fn operator+(a: Vec2, b: Vec2) -> Vec2 {
    return Vec2 { x: a.x + b.x, y: a.y + b.y };
}

fn operator-(a: Vec2) -> Vec2 {
    return Vec2 { x: -a.x, y: -a.y };
}

fn operator==(a: Vec2, b: Vec2) -> bool {
    return a.x == b.x && a.y == b.y;
}

fn magnitude_sq(v: Vec2) -> float {
    return v.x * v.x + v.y * v.y;
}

fn operator<(a: Vec2, b: Vec2) -> bool {
    return magnitude_sq(a) < magnitude_sq(b);
}

fn main() -> int {
    let p = Vec2 { x: 1.0, y: 2.0 };
    let q = Vec2 { x: 3.0, y: 4.0 };
    let sum = p + q;
    let neg = -p;
    print(sum.x);   // 4
    print(neg.x);   // -1
    print(p == p);  // true
    print(p < q);   // true
    return 0;
}
```

---

### Interfaces via Compile-Time Monomorphization
Interfaces define contracts implemented by classes. Generic functions accepting `&impl Interface` are monomorphized at compile time, generating specialized C functions per concrete type with zero runtime dispatch:

```cco
interface Printable {
    fn describe(self) -> void;
}

class Point {
    x: int;
    y: int;

    fn describe(self) -> void {
        print(f"Point({self.x}, {self.y})");
    }
}

class Circle {
    radius: int;

    fn describe(self) -> void {
        print(f"Circle(r={self.radius})");
    }
}

impl Printable for Point;
impl Printable for Circle;

// Monomorphized at compile-time: generates announce_Point and announce_Circle in C
fn announce(item: &impl Printable) -> void {
    item.describe();
}

fn main() -> int {
    let p = Point { x: 10, y: 25 };
    let c = Circle { radius: 50 };

    announce(p);
    announce(c);

    return 0;
}
```

---

### Python-Style F-String Interpolation
Supports string interpolation with expression evaluation and automatic type conversions:

```cco
fn main() -> int {
    let name = "Alice";
    let score = 98.5;
    let rank = 1;
    print(f"Player {name} achieved rank #{rank} with score {score}!");
    return 0;
}
```

- Expressions of type `int`, `float`, `bool`, and `char` are converted automatically via standard helpers.
- Double braces `{{` and `}}` escape interpolation and render literal braces.
- Intermediate temporary allocations are freed automatically with zero memory leaks.

---

### Tagged Unions / Enums & Pattern Matching
Native tagged unions (`enum`) supporting unit and payload variants, evaluated via compile-time checked `match` statements:

```cco
enum Expr {
    Num { value: int },
    Add { left: Expr, right: Expr },
    Mul { left: Expr, right: Expr },
    Eof,
}

fn eval(e: &Expr) -> int {
    match e {
        Expr.Num { value } => {
            return value;
        }
        Expr.Add { left, right } => {
            return eval(left) + eval(right);
        }
        Expr.Mul { left, right } => {
            return eval(left) * eval(right);
        }
        Expr.Eof => {
            return 0;
        }
    }
}

fn main() -> int {
    let tree = Expr.Add {
        left: Expr.Num { value: 10 },
        right: Expr.Num { value: 20 },
    };
    print(eval(&tree)); // 30
    return 0;
}
```

- **Compile-Time Exhaustiveness**: Matching without a wildcard (`_ => { ... }`) requires all declared variants to be covered.
- **Borrow-Only Semantics**: Scrutinee is borrowed (`&Enum`), binding borrowed references to internal fields to avoid premature frees.
- **Automatic Free Cascade**: Recursive structures (such as AST trees) are deallocated recursively at scope exit.

---

### Lightweight Structs (Value Types)
Stack-allocated value types with primitive-only fields:

```cco
struct Point2D {
    x: int;
    y: int;
}

fn offset(p: &Point2D, dx: int, dy: int) -> void {
    p.x += dx;
    p.y += dy;
}
```

- Structs are copied by value on assignment.
- Fields are restricted to primitive types (`int`, `float`, `char`, `bool`), eliminating ownership overhead.
- Passed by reference via `&Struct` for in-place mutation without heap allocation.

---

### Arrays of Objects & For-Each Iteration
Collections of heap-allocated class instances managed under single-ownership:

```cco
let pts: Point[] = alloc(Point, 3);
pts[0] = Point { x: 1, y: 2 };
pts[1] = Point { x: 3, y: 4 };

for p in pts {
    p.describe();
}
```

- Moving an element out of an array is rejected at compile time to prevent dangling slots.
- At scope exit, all non-NULL objects are deallocated via their class destructor before the array buffer is freed.

---

### Hash Maps and Growable Arrays
Standard dynamic collection types with automatic cleanup:

```cco
// Growable array
let items = list_new(int);
push(items, 10);
push(items, 20);
let val = pop(items); // 20
let count = len(items); // 1

// Open-addressing hash map
let m = map_new(string, int);
m = put(m, "alpha", 1);
m = put(m, "beta", 2);
let score = get(m, "alpha"); // 1
let has_key = has(m, "beta"); // true
```

---

### Module and Import System
Multi-file support through a Resolve-Then-Merge AST pipeline:

```cco
import "shapes.cco";

fn main() -> int {
    let c = Circle { radius: 10 };
    return 0;
}
```

- Diamond imports (`A -> B, C; B -> D; C -> D`) are deduplicated based on canonical file path resolution.
- Circular imports (`A <-> B`) are detected and rejected at compile time with diagnostic traces.

---

### Selective Prelude Emission
Only runtime helpers (`__cco_*`) required by the program are emitted into the generated `.c` file:
- Transitive dependencies are tracked (e.g., bounds check includes array length query).
- Fixed emission ordering produces deterministic and diffable compiler output.
- Programs that do not use strings, arrays, or maps contain zero runtime boilerplate.

---

### Self-Hosted Lexer Proof-of-Concept
A self-hosted lexer written entirely in Cco (`selfhost/lexer.cco`) implements lexical analysis for full Cco syntax:
- Built with language constructs including tagged unions, hash maps, growable arrays, and classes.
- Verified byte-for-byte against the C reference lexer across all test cases via `tests/compare_lexers.sh`.

---

## 5. Standard Library API Reference

### 1. String Functions
| Function | Signature | Description | Heap Ownership |
| :--- | :--- | :--- | :--- |
| `len` | `len(s: string) -> int` | Character count of string | Borrowed |
| `concat` | `concat(a: string, b: string) -> string` | Concatenates two strings | Caller owns return string |
| `equals` | `equals(a: string, b: string) -> bool` | Checks string equality | Borrowed |
| `char_at` | `char_at(s: string, i: int) -> char` | Character at index `i` with bounds check | Borrowed |
| `substring` | `substring(s: string, start: int, end: int) -> string` | Substring slice | Caller owns return string |

### 2. Math Functions
| Function | Signature | Description |
| :--- | :--- | :--- |
| `sqrt` | `sqrt(x: float) -> float` | Square root |
| `pow` | `pow(base: float, exp: float) -> float` | Power function |
| `abs_int` | `abs_int(x: int) -> int` | Absolute value of integer |
| `abs_float` | `abs_float(x: float) -> float` | Absolute value of float |
| `floor` | `floor(x: float) -> float` | Floor of float |
| `ceil` | `ceil(x: float) -> float` | Ceiling of float |
| `min_int` | `min_int(a: int, b: int) -> int` | Minimum of two integers |
| `max_int` | `max_int(a: int, b: int) -> int` | Maximum of two integers |
| `min_float` | `min_float(a: float, b: float) -> float` | Minimum of two floats |
| `max_float` | `max_float(a: float, b: float) -> float` | Maximum of two floats |

### 3. File I/O Functions
| Function | Signature | Description | Heap Ownership |
| :--- | :--- | :--- | :--- |
| `read_file` | `read_file(path: string) -> string` | Reads entire file into string | Caller owns return string |
| `write_file` | `write_file(path: string, content: string) -> bool` | Writes string to file path | Borrowed |

### 4. Command-Line Arguments
| Function | Signature | Description | Heap Ownership |
| :--- | :--- | :--- | :--- |
| `args` | `args() -> string[]` | Returns arguments after program name | Caller owns array & strings |
| `arg_count` | `arg_count() -> int` | Returns argument count | Value |
| `program_name` | `program_name() -> string` | Returns program invocation path (`argv[0]`) | Caller owns return string |

### 5. Input, Numeric Parsing, and Random Numbers
| Function | Signature | Description | Heap Ownership |
| :--- | :--- | :--- | :--- |
| `read_line` | `read_line() -> string` | Reads one line from stdin (stripping `
`) | Caller owns return string |
| `to_int` | `to_int(s: string) -> int` | Parses integer (runtime exit on failure) | Value |
| `to_float` | `to_float(s: string) -> float` | Parses float (runtime exit on failure) | Value |
| `is_int` | `is_int(s: string) -> bool` | Validates integer format | Value |
| `is_float` | `is_float(s: string) -> bool` | Validates float format | Value |
| `random_int` | `random_int(min: int, max: int) -> int` | Unbiased random integer in `[min, max]` | Value |
| `random_seed` | `random_seed(seed: int) -> void` | Seeds random generator | Value |

### 6. Native POSIX Networking
POSIX socket functions for TCP network services:
- `net_listen(port: int) -> int`: Creates an `AF_INET` TCP stream socket, sets `SO_REUSEADDR`, binds to `0.0.0.0:port`, and listens with a 128-connection backlog.
- `net_accept(server_fd: int) -> int`: Accepts an incoming TCP connection, returning a connected `client_fd` (or `-1` on error).
- `net_recv(client_fd: int, max_bytes: int) -> string`: Reads bytes into an RAII-managed string with HTTP framing detection.
- `net_send(client_fd: int, data: string) -> int`: Transmits response bytes over the wire with `MSG_NOSIGNAL`.
- `net_close(fd: int) -> void`: Closes socket file descriptors.
- `sleep_ms(ms: int) -> void`: Millisecond sleep via `nanosleep`.

#### Minimal HTTP Server Example
```cco
fn handle_client(client_fd: int) -> void {
    let req = net_recv(client_fd, 0);
    if (len(req) == 0) {
        net_close(client_fd);
        return;
    }

    let body = "{"status": "OK", "message": "Hello from Cco!"}
";
    let body_len = len(body);
    let resp = f"HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: {body_len}
Connection: close

{body}";
    
    net_send(client_fd, resp);
    net_close(client_fd);
}

fn main() -> int {
    let server_fd = net_listen(8080);
    if (server_fd < 0) return 1;

    print("Server listening on http://127.0.0.1:8080");
    while (true) {
        let client_fd = net_accept(server_fd);
        if (client_fd >= 0) {
            handle_client(client_fd);
        }
    }
    net_close(server_fd);
    return 0;
}
```

*Note: Sockets currently operate sequentially on a single thread. Connections must complete before subsequent clients are processed.*

---

## 6. Diagnostic Error Reporting

Cco provides compiler diagnostics indicating error spans, source lines, and contextual notes:

```
error: duplicate definition of 'Helper'
  --> tests/programs/32_import_duplicate_symbol_ERROR/b.cco:1:1
    |
  1 | class Helper {
    | ^ duplicate definition
    |
note: first defined here:
  --> tests/programs/32_import_duplicate_symbol_ERROR/a.cco:1:1
    |
  1 | class Helper {
    | ^ first defined here
    |
```

---

## 7. Example Programs Gallery

Available in [`examples/`](examples/):

- [`examples/01_hello_world.cco`](examples/01_hello_world.cco): Basic syntax and types
- [`examples/02_fibonacci.cco`](examples/02_fibonacci.cco): Iterative and recursive Fibonacci
- [`examples/03_point_distance.cco`](examples/03_point_distance.cco): Classes, methods, and standard math
- [`examples/04_string_builder.cco`](examples/04_string_builder.cco): String operations
- [`examples/05_array_sum.cco`](examples/05_array_sum.cco): Dynamic array allocation and loops
- [`examples/06_word_count.cco`](examples/06_word_count.cco): File I/O and token counting
- [`examples/07_ownership_demo.cco`](examples/07_ownership_demo.cco): Ownership moves vs borrowed references
- [`examples/08_stack_data_structure.cco`](examples/08_stack_data_structure.cco): Dynamic stack data structure
- [`examples/09_object_array_todo.cco`](examples/09_object_array_todo.cco): Arrays of objects and for-each iteration
- [`examples/10_import_demo/`](examples/10_import_demo/): Multi-file imports
- [`examples/11_struct_vec2.cco`](examples/11_struct_vec2.cco): Structs, value copies, and borrowed mutations
- [`examples/12_cli_args.cco`](examples/12_cli_args.cco): Command-line argument parsing
- [`examples/13_number_guess.cco`](examples/13_number_guess.cco): Interactive stdin game
- [`examples/word_frequency.cco`](examples/word_frequency.cco): Hash maps and key iteration
- [`examples/printable_interface.cco`](examples/printable_interface.cco): Interfaces and monomorphization
- [`examples/vec2_operators.cco`](examples/vec2_operators.cco): Struct operator overloading

---

## 8. Test Suite Matrix

All test cases are verified using `valgrind --leak-check=full --error-exitcode=1`:

| Test Case | Description | Result | Valgrind Leak Status |
| :--- | :--- | :---: | :---: |
| `test_lexer` | Tokenizer, keywords (`class`, `struct`, `map`, `self`, `import`) & `&` borrow | PASS | 0 Bytes Leaked |
| `test_parser` | AST node construction, `map[K]V` parsing & enforcement | PASS | 0 Bytes Leaked |
| `test_scope` | Ownership analysis, move tracking, deallocation injection | PASS | 0 Bytes Leaked |
| `test_map_runtime` | Open-addressing map runtime, tombstones, class cleanup & rehash | PASS | 0 Bytes Leaked |
| `01_hello` | Basic printing, strings, arithmetic operations | PASS | 0 Bytes Leaked |
| `02_alloc_basic` | Basic array allocation, indexing, & block exit free | PASS | 0 Bytes Leaked |
| `03_early_return` | Early return inside nested loop with heap allocation | PASS | 0 Bytes Leaked |
| `04_loop_alloc` | Allocation inside loop body (freed every iteration) | PASS | 0 Bytes Leaked |
| `05_ownership_transfer` | Function returning allocated pointer to caller scope | PASS | 0 Bytes Leaked |
| `06_nested_scopes` | Allocations in conditional `if`/`else` branches | PASS | 0 Bytes Leaked |
| `07_break_continue` | `break` and `continue` statements inside loops | PASS | 0 Bytes Leaked |
| `08_reassign_alloc` | Reassigning managed variable to new allocation | PASS | 0 Bytes Leaked |
| `09_basic_class` | Basic class creation and method call | PASS | 0 Bytes Leaked |
| `10_method_call` | Method calls with object parameters | PASS | 0 Bytes Leaked |
| `11_aliasing_refcount` | Object assignment move semantics & single cleanup free | PASS | 0 Bytes Leaked |
| `12_reassign_object` | Reassigning object variables with automatic cleanup of old object | PASS | 0 Bytes Leaked |
| `13_object_early_return` | Object lifetime inside loops with early returns | PASS | 0 Bytes Leaked |
| `14_basic_move` | Single ownership move and clean deallocation | PASS | 0 Bytes Leaked |
| `15_borrowed_param` | Borrowed parameter (`&Point`) without ownership transfer | PASS | 0 Bytes Leaked |
| `16_use_after_move_ERROR` | Diagnostic rejection of use-after-move | PASS | Compile Error (Expected) |
| `17_double_move_ERROR` | Diagnostic rejection of double-move | PASS | Compile Error (Expected) |
| `18_conditional_move_ERROR` | Diagnostic rejection of conditional move | PASS | Compile Error (Expected) |
| `19_move_via_return` | Returning owned objects and moving between scopes | PASS | 0 Bytes Leaked |
| `20_return_borrowed_ERROR` | Diagnostic rejection of returning borrowed parameter | PASS | Compile Error (Expected) |
| `21_stdlib_string` | String operations (`concat`, `len`, `equals`, `substring`) | PASS | 0 Bytes Leaked |
| `22_stdlib_math` | Math operations (`sqrt`, `pow`, `abs`, `min`, `max`) | PASS | 0 Bytes Leaked |
| `23_stdlib_file_io` | File I/O operations (`read_file`, `write_file`) | PASS | 0 Bytes Leaked |
| `24_object_array_basic` | Object array allocation (`Point[]`), indexing, and scope-exit cleanup | PASS | 0 Bytes Leaked |
| `25_object_array_foreach` | For-each loop iteration over object arrays (`for p in pts`) | PASS | 0 Bytes Leaked |
| `26_object_array_free_cascade` | Null-checked release cascade skipping empty array slots | PASS | 0 Bytes Leaked |
| `27_object_array_move_out_ERROR` | Rejecting move-out of array element (`let p = pts[0]`) | PASS | Compile Error (Expected) |
| `28_object_array_bounds_ERROR` | Rejecting constant index out-of-bounds | PASS | Compile Error (Expected) |
| `29_import_basic` | Multi-file import (`import "shapes.cco";`) | PASS | 0 Bytes Leaked |
| `30_import_diamond` | Diamond import resolution & AST deduplication | PASS | 0 Bytes Leaked |
| `31_import_circular_ERROR` | Circular import cycle detection | PASS | Compile Error (Expected) |
| `32_import_duplicate_symbol_ERROR` | Duplicate class/function definition rejection across files | PASS | Compile Error (Expected) |
| `33_struct_basic` | Struct declaration, construction, and field access | PASS | 0 Bytes Leaked |
| `34_struct_copy_semantics` | Struct value copy on assignment | PASS | 0 Bytes Leaked |
| `35_struct_borrowed_param` | In-place struct mutation via borrowed reference (`&Vec2`) | PASS | 0 Bytes Leaked |
| `36_struct_nonprimitive_field_ERROR` | Rejecting non-primitive field in struct | PASS | Compile Error (Expected) |
| `37_struct_class_name_collision_ERROR` | Rejecting class and struct name collision | PASS | Compile Error (Expected) |
| `38_prelude_minimal` | Program with zero stdlib calls emits zero `__cco_` helpers | PASS | 0 Bytes Leaked |
| `39_prelude_partial` | Program with `concat()` emits `__cco_concat` with no unused helpers | PASS | 0 Bytes Leaked |
| `40_prelude_transitive` | Array indexing transitively emits bounds check and array length | PASS | 0 Bytes Leaked |
| `41_growable_push_primitive` | `list_new(int)`, repeated `push()`, `len()` dynamic count | PASS | 0 Bytes Leaked |
| `42_growable_pop_primitive` | `pop()` from primitive array, length decrement, returned value | PASS | 0 Bytes Leaked |
| `43_growable_push_pop_class` | Growable object array, `push()` transfer, `pop()` sole ownership move | PASS | 0 Bytes Leaked |
| `44_growable_realloc_stress` | Buffer reallocations under stress with zero leaks | PASS | 0 Bytes Leaked |
| `45_pop_empty_RUNTIME_ERROR` | Runtime error when `pop()` called on empty array | PASS | Runtime Error (Expected) |
| `46_map_basic_primitive` | `map[int]int` creation, `put()`, `get()`, scope-exit cleanup | PASS | 0 Bytes Leaked |
| `47_map_string_key` | `map[string]int` creation, string key cloning, `get()` retrieval | PASS | 0 Bytes Leaked |
| `48_map_class_value_ownership` | `map[string]Point`, object moves in `put()`, overwrite cleanup | PASS | 0 Bytes Leaked |
| `49_map_remove_and_tombstones` | `remove()` with tombstone tagging, caller move-out, `has()` | PASS | 0 Bytes Leaked |
| `50_map_keys_and_len` | `keys(m)` extraction to `int[]`, `len(m)` query, and iteration | PASS | 0 Bytes Leaked |
| `51_map_rehash_stress` | Dynamic rehashing from capacity 8 to 128+ with 0 leaks | PASS | 0 Bytes Leaked |
| `52_map_invalid_key_type_ERROR` | Rejecting invalid key type (`map[float]int`) | PASS | Compile Error (Expected) |
| `53_map_put_reassignment_check_ERROR` | Rejecting unassigned `put(m, k, v)` call | PASS | Compile Error (Expected) |
| `54_enum_unit_variants` | Unit variants (`enum Color { Red, Green, Blue }`), match | PASS | 0 Bytes Leaked |
| `55_enum_payload_variants` | Payload variants (`enum Shape { Circle { radius: int } }`) | PASS | 0 Bytes Leaked |
| `56_enum_recursive_eval` | Recursive tagged union trees (`Expr.Add`, `Expr.Mul`), evaluation | PASS | 0 Bytes Leaked |
| `57_enum_ownership_move` | Ownership transfer of enum instances | PASS | 0 Bytes Leaked |
| `58_enum_borrowed_match` | Borrow-only match scrutinee (`match &token`), field binding | PASS | 0 Bytes Leaked |
| `59_enum_nonexhaustive_ERROR` | Rejecting non-exhaustive match statements | PASS | Compile Error (Expected) |
| `60_enum_duplicate_arm_ERROR` | Rejecting duplicate match arms | PASS | Compile Error (Expected) |
| `61_enum_field_rename_ERROR` | Rejecting field name renaming in match patterns | PASS | Compile Error (Expected) |
| `62_argv_basic` | CLI arguments via `args()`, `arg_count()` | PASS | 0 Bytes Leaked |
| `63_argv_empty` | Zero CLI arguments invocation | PASS | 0 Bytes Leaked |
| `64_program_name` | Program path retrieval via `program_name()` | PASS | 0 Bytes Leaked |
| `65_read_line_basic` | Standard input line reading, newline stripping, EOF handling | PASS | 0 Bytes Leaked |
| `66_to_int_valid` | Numeric parsing via `to_int()` and `to_float()` | PASS | 0 Bytes Leaked |
| `67_to_int_invalid_RUNTIME_ERROR` | Catching invalid numeric string in `to_int()` | PASS | Runtime Error (Expected) |
| `68_is_int_is_float_guard` | Safe parse-guarding pattern (`is_int()`, `is_float()`) | PASS | 0 Bytes Leaked |
| `69_random_seeded_deterministic` | Deterministic random sequence via `random_seed()` & `random_int()` | PASS | 0 Bytes Leaked |
| `70_fstring_basic` | Single integer variable interpolation (`f"x = {x}"`) | PASS | 0 Bytes Leaked |
| `71_fstring_multiple_exprs` | Multiple interpolations across primitive types | PASS | 0 Bytes Leaked |
| `72_fstring_escaped_braces` | Escaped brace literals `{{` and `}}` | PASS | 0 Bytes Leaked |
| `73_fstring_nested_expr` | Nested arithmetic and function calls inside `{...}` | PASS | 0 Bytes Leaked |
| `74_fstring_unbalanced_ERROR` | Rejecting unbalanced or unterminated f-strings | PASS | Compile Error (Expected) |
| `75_operator_overload_add` | Binary struct operator overloading (`operator+`) | PASS | 0 Bytes Leaked |
| `76_operator_overload_multiple` | Multiple struct operators (`+`, `-`, `*`, `==`, `!=`) | PASS | 0 Bytes Leaked |
| `77_operator_overload_missing_ERROR` | Rejecting missing struct operator definition | PASS | Compile Error (Expected) |
| `78_operator_overload_unary_neg` | Unary negation operator overloading (`operator-(a)`) | PASS | 0 Bytes Leaked |
| `79_operator_overload_class_ERROR` | Rejecting operator overloading for class types | PASS | Compile Error (Expected) |
| `compare_lexers` | Self-hosted lexer diff harness across codebase (100% byte-identical) | PASS | 0 Bytes Leaked |

---

## 9. Strict C11 Portability and Conformance

Cco-generated C output strictly conforms to **standard ISO C11** (`-std=c11 -pedantic-errors`):

- **Zero Non-Standard Compiler Extensions**: Compiler-specific extensions (such as GNU `__typeof__`) are not present in generated output.
- **Strict Standard C11 I/O**: `read_line()` is implemented using standard ISO C11 `fgetc()` with dynamic heap buffer reallocation (`malloc`/`realloc`) rather than POSIX-only `getline()`, ensuring generated C code compiles cleanly across Windows/MSVC, Linux, macOS, and BSD without POSIX dependencies.
- **Networking Portability**: Core Cco constructs are strictly portable ISO C11. Native networking functions (`net_listen`, `net_accept`, `net_recv`, `net_send`, `net_close`) bind to POSIX socket headers (`<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<unistd.h>`) and require a POSIX environment.
- **Multi-Compiler Conformance**: Generated C code compiles without warnings or errors under `gcc`, `clang`, `tcc`, and MSVC.
- **Compiler Platform Requirements**: While generated C code is strictly portable standard C11, the Cco compiler executable itself relies on POSIX `realpath()` for canonical module resolution and requires a POSIX environment to run.

---

## 10. License

MIT License. Developed for the Cco Source-to-Source Transpiler Project.
