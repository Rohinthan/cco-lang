# Cco (C--) Compiler: A C-like Language with Compile-Time Single Ownership, Move Semantics & Standard Library (v4.0)

**Cco (C--)** is a lightweight, systems programming language with explicit C-like syntax, **scope-exit auto-free for raw allocations**, **compile-time single ownership with move semantics** for classes and objects, and a **built-in Standard Library** (Strings, Math, File I/O). It transpiles Cco source code (`.cco`) into portable, standard C11 source code (`.c`), which is compiled to native machine binaries using `gcc` or `clang`.

> **Write it like Python's class syntax reads. Compile it and it runs like C with zero runtime reference counting overhead, zero GC pauses, no manual free(), Rust-like compile-time ownership safety, and GCC/Rust-style diagnostic error messages.**

---

## 🏛️ The Three Pillars of Cco

1. **RUNS LIKE C**  
   Cco is a source-to-source transpiler, not an interpreter and not a VM. Every `.cco` file becomes real, flat C11 source, compiled by `gcc` to a native binary. There is no runtime interpreter loop, no bytecode dispatch, no runtime reference counter, no VM overhead. A Cco program's speed ceiling is C's speed ceiling, full stop.

2. **HAS OBJECTS LIKE C++**  
   Cco provides `class`, fields, methods, and `obj.method(args)` call syntax—the actual ergonomic win of C++ over plain C, placing verbs next to their nouns instead of `distance(&a, &b)` scattered functions. Under the hood it translates to C structs and functions taking a `self` pointer—no vtables, no multiple inheritance, no operator overloading, no templates, no name-mangling maze.

3. **WRITTEN LIKE IT'S EASY**  
   No manual `malloc`/`free`. No header files to keep in sync with `.c` files. No `->` vs `.` decision (member access is always `.`). Memory is managed automatically via scope-exit auto-free (for raw allocations and heap strings) and **compile-time single ownership with move semantics** (for objects)—both deterministic, zero runtime overhead, zero GC pause.

---

## 📚 Standard Library API Reference (Part A)

Cco includes a built-in Standard Library available in every file without manual imports or headers.

### 1. String Functions
| Function | Signature | Description | Heap Ownership |
| :--- | :--- | :--- | :--- |
| `len` | `len(s: string) -> int` | Returns character count of string `s` | - |
| `concat` | `concat(a: string, b: string) -> string` | Returns a fresh heap-allocated string containing `a + b` | Caller owns return string |
| `equals` | `equals(a: string, b: string) -> bool` | Returns `true` if string `a` equals string `b` | - |
| `char_at` | `char_at(s: string, i: int) -> char` | Returns character at 0-indexed position `i` (with bounds check) | - |
| `substring` | `substring(s: string, start: int, end: int) -> string` | Returns a fresh heap-allocated substring from `start` to `end` | Caller owns return string |

### 2. Math Functions
| Function | Signature | Description |
| :--- | :--- | :--- |
| `sqrt` | `sqrt(x: float) -> float` | Square root of `x` |
| `pow` | `pow(base: float, exp: float) -> float` | `base` raised to `exp` power |
| `abs_int` | `abs_int(x: int) -> int` | Absolute value of integer `x` |
| `abs_float` | `abs_float(x: float) -> float` | Absolute value of float `x` |
| `floor` | `floor(x: float) -> float` | Floor of float `x` |
| `ceil` | `ceil(x: float) -> float` | Ceiling of float `x` |
| `min_int` | `min_int(a: int, b: int) -> int` | Minimum of two integers |
| `max_int` | `max_int(a: int, b: int) -> int` | Maximum of two integers |
| `min_float` | `min_float(a: float, b: float) -> float` | Minimum of two floats |
| `max_float` | `max_float(a: float, b: float) -> float` | Maximum of two floats |

### 3. File I/O Functions
| Function | Signature | Description | Heap Ownership |
| :--- | :--- | :--- | :--- |
| `read_file` | `read_file(path: string) -> string` | Reads entire file at `path` into a fresh heap-allocated string | Caller owns return string |
| `write_file` | `write_file(path: string, content: string) -> bool` | Writes `content` to file at `path`, returning `true` on success | - |

---

## 🎯 Diagnostic Error Message Polish (Part B)

Cco features GCC/Rust-style two-location diagnostic error reporting with verbatim source line snippets, line number padding, carets pointing at exact spans, and explanatory notes.

### Example: Use-After-Move Error
```
error: use of moved value 'a'
  --> tests/programs/16_use_after_move_ERROR.cco:9:11
    |
  9 |     print(a.x);
    |           ^ value used here after being moved
    |
note: 'a' was moved into 'b' on line 8
  --> tests/programs/16_use_after_move_ERROR.cco:8:20
    |
  8 |     let b: Point = a;
    |                    ^ move occurs here
    |
```

### Example: Returning a Borrowed Value Error
```
error: cannot return borrowed value 'p'
  --> tests/programs/20_return_borrowed_ERROR.cco:7:12
    |
  7 |     return p;
    |            ^ return of borrowed value
    |
note: 'p' is a borrowed parameter (&Point) — this function does not own it and cannot transfer ownership to the caller
  --> tests/programs/20_return_borrowed_ERROR.cco:6:8
    |
  6 | fn bad(p: &Point) -> Point {
    |        ^ parameter declared as borrowed here
    |
```

---

## 🎨 Example Program Gallery (Part C)

A comprehensive suite of example programs is available in [`examples/`](examples/):

- [`examples/01_hello_world.cco`](examples/01_hello_world.cco): Basic syntax, primitive types, and printing
- [`examples/02_fibonacci.cco`](examples/02_fibonacci.cco): Iterative & recursive Fibonacci sequence
- [`examples/03_point_distance.cco`](examples/03_point_distance.cco): Object-oriented Point class with math stdlib (`sqrt`, `pow`)
- [`examples/04_string_builder.cco`](examples/04_string_builder.cco): String manipulation (`concat`, `len`, `substring`, `equals`)
- [`examples/05_array_sum.cco`](examples/05_array_sum.cco): Dynamic array allocation (`alloc`) and iteration
- [`examples/06_word_count.cco`](examples/06_word_count.cco): File I/O (`read_file`) and word counting
- [`examples/07_ownership_demo.cco`](examples/07_ownership_demo.cco): Ownership transfer (moves) vs borrowed references (`&Class`)
- [`examples/08_stack_data_structure.cco`](examples/08_stack_data_structure.cco): OOP Stack data structure with dynamic array buffer

See [`examples/README.md`](examples/README.md) for detailed descriptions and execution instructions.

---

## 🛠️ Build and Testing Instructions

### Requirements
- Linux (Ubuntu 20.04+)
- `gcc` (v9+) or `clang`
- `make`
- `valgrind`

### Quick Start
```bash
# Clone and build the Cco compiler executable
make cco

# Run full Unit and Integration Test Suite under Valgrind
make test
```

---

## 🧪 Test Suite Matrix (v4.0)

| Test Case | Description | Result | Valgrind Leak Status |
| :--- | :--- | :---: | :---: |
| `test_lexer` | Unit tests for tokenizer, keywords (`class`, `self`) & `&` borrow token | **PASS** | 0 Bytes Leaked |
| `test_parser` | Unit tests for AST node construction & `&` parameter type parsing | **PASS** | 0 Bytes Leaked |
| `test_scope` | Unit tests for ownership pass, move tracking, free injection | **PASS** | 0 Bytes Leaked |
| `01_hello` | Basic printing, strings, arithmetic operations | **PASS** | 0 Bytes Leaked |
| `02_alloc_basic` | Basic array allocation, indexing, & block exit free | **PASS** | 0 Bytes Leaked |
| `03_early_return` | Early return inside nested loop with heap alloc | **PASS** | 0 Bytes Leaked |
| `04_loop_alloc` | Allocation inside loop body (freed every iteration) | **PASS** | 0 Bytes Leaked |
| `05_ownership_transfer` | Function returning allocated pointer to caller scope | **PASS** | 0 Bytes Leaked |
| `06_nested_scopes` | Allocations in conditional `if`/`else` branches | **PASS** | 0 Bytes Leaked |
| `07_break_continue` | `break` and `continue` statements inside loops | **PASS** | 0 Bytes Leaked |
| `08_reassign_alloc` | Reassigning managed variable to new allocation | **PASS** | 0 Bytes Leaked |
| `09_basic_class` | Basic class creation and method call | **PASS** | 0 Bytes Leaked |
| `10_method_call` | Method calls with object parameters | **PASS** | 0 Bytes Leaked |
| `11_aliasing_refcount` | Object assignment move semantics & single cleanup free | **PASS** | 0 Bytes Leaked |
| `12_reassign_object` | Reassigning object variables with automatic cleanup of old object | **PASS** | 0 Bytes Leaked |
| `13_object_early_return` | Object lifetime inside loops with early returns | **PASS** | 0 Bytes Leaked |
| `14_basic_move` | Single ownership move and clean deallocation | **PASS** | 0 Bytes Leaked |
| `15_borrowed_param` | Borrowed parameter (`&Point`) without ownership transfer | **PASS** | 0 Bytes Leaked |
| `16_use_after_move_ERROR` | Formatted Rust-style rejection of use-after-move | **PASS** | Compile Error 1 (As Expected) |
| `17_double_move_ERROR` | Formatted Rust-style rejection of double-move | **PASS** | Compile Error 1 (As Expected) |
| `18_conditional_move_ERROR` | Formatted Rust-style rejection of conditional move | **PASS** | Compile Error 1 (As Expected) |
| `19_move_via_return` | Returning owned objects and moving between scopes | **PASS** | 0 Bytes Leaked |
| `20_return_borrowed_ERROR` | Formatted Rust-style rejection of returning borrowed parameter | **PASS** | Compile Error 1 (As Expected) |
| `21_stdlib_string` | Standard Library String operations (`concat`, `len`, `equals`, `substring`) | **PASS** | 0 Bytes Leaked |
| `22_stdlib_math` | Standard Library Math operations (`sqrt`, `pow`, `abs`, `min`, `max`) | **PASS** | 0 Bytes Leaked |
| `23_stdlib_file_io` | Standard Library File I/O operations (`read_file`, `write_file`) | **PASS** | 0 Bytes Leaked |

---

## 📄 License
MIT License. Developed for the Cco Source-to-Source Transpiler Sprint.
