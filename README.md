# Cco (C--) Compiler: A C-like Language with Compile-Time Single Ownership, Lightweight Structs, Minimal Module/Import System & Standard Library (v7.0)

**Cco (C--)** is a lightweight, systems programming language with explicit C-like syntax, **scope-exit auto-free for raw allocations**, **compile-time single ownership with move semantics**, **Lightweight Structs (Value Types)** (`struct Point2D { x: int; y: int; }`), **Arrays of Objects with for-each iteration** (`alloc(Point, n)`, `for p in pts`), **Minimal Multi-file Module/Import System** (`import "file.cco";`), and a **built-in Standard Library** (Strings, Math, File I/O). It transpiles Cco source code (`.cco`) into portable, standard C11 source code (`.c`), which is compiled to native machine binaries using `gcc` or `clang`.

> **Write it like Python's class/struct syntax reads. Compile it and it runs like C with zero runtime reference counting overhead, zero GC pauses, stack-allocated value structs with zero heap allocation, Rust-like compile-time ownership safety across multi-file programs, and GCC/Rust-style diagnostic error messages.**

---

## 🏛️ The Three Pillars of Cco

1. **RUNS LIKE C**  
   Cco is a source-to-source transpiler, not an interpreter and not a VM. Every `.cco` file becomes real, flat C11 source, compiled by `gcc` to a native binary. There is no runtime interpreter loop, no bytecode dispatch, no runtime reference counter, no VM overhead. A Cco program's speed ceiling is C's speed ceiling, full stop.

2. **HAS OBJECTS, VALUE STRUCTS & COLLECTIONS LIKE C++**  
   Cco provides heap `class` instances, stack-allocated `struct` value types, fields, methods, `obj.method(args)` call syntax, and **Arrays of Objects** (`Point[] = alloc(Point, 3)`). Under the hood it translates to C pointers and C structs—no vtables, no multiple inheritance, no operator overloading, no templates, no name-mangling maze.

3. **WRITTEN LIKE IT'S EASY**  
   No manual `malloc`/`free`. No header files to keep in sync with `.c` files. Memory is managed automatically via scope-exit auto-free, **single ownership release cascades**, **zero-allocation stack structs**, and a **Resolve-Then-Merge multi-file import system** (`import "file.cco";`)—both deterministic, zero runtime overhead, zero GC pause.

---

## 🏗️ Lightweight Structs (Value Types) (v7.0)

Cco v7.0 introduces lightweight value-type structs (`struct Point2D { x: int; y: int; }`):

- **Value Type Semantics**: Structs live on the stack and get copied by value on assignment (`let q: Point2D = p;`).
- **Primitive-Only Fields**: Struct fields must be primitive types (`int`, `float`, `char`, `bool`). Structs cannot contain heap objects or strings, ensuring zero ownership tracking or `free()` calls are needed.
- **No Methods or Heap Allocations**: Structs are plain data containers without methods or heap constructors (`Point2D { x: 1, y: 2 }` desugars to C compound literals `(Point2D){ .x = 1, .y = 2 }`).
- **In-Place Borrowed Mutation**: Structs can be passed by reference using `&Struct` (`fn scale(v: &Vec2, factor: float)`) for zero-copy in-place mutation.

---

## 📁 Minimal Module/Import System (v6.0)

Cco v6.0 introduces multi-file program support via a Resolve-Then-Merge AST architecture:

- **Import Syntax**: `import "shapes.cco";` statements must appear at the top of a file before any function or class declarations.
- **Resolve-Then-Merge AST**: Files are parsed independently into separate ASTs, then recursively merged into a single combined program AST before scope analysis or code generation runs.
- **Automatic De-duplication**: Diamond imports (`A -> B, C; B -> D; C -> D`) parse and merge `D` exactly once based on canonical absolute file path resolution.
- **Flat Global Namespace**: Functions and classes across imported files live in one global namespace without mandatory visibility modifiers or qualified names.
- **Diagnostic Error Reporting**: Compile-time detection of circular imports (`A <-> B`) and duplicate symbol definitions with clear two-location error messages.

---

## 📦 Arrays of Objects & For-Each (v5.0)

Cco v5.0 extends single-ownership to collections of class instances:

- **Allocation**: `let pts: Point[] = alloc(Point, n);` zero-initializes `n` element slots.
- **Ownership**: An array of objects owns every element inside it.
- **Borrow-Only Indexing**: Elements can be accessed, mutated, or passed as borrowed references (`pts[i].x`, `pts[i].sum()`, `&pts[i]`). Moving an element out of an array (`let p: Point = pts[0];`) is rejected at compile time with a clear diagnostic message.
- **For-Each Loops**: `for p in pts { p.sum(); }` iterates over elements, yielding borrowed element references while skipping `NULL` slots.
- **Automated Free Cascade**: At scope exit, any non-NULL elements in the array are automatically freed via their class destructor, followed by freeing the array buffer.

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

### Example: Duplicate Symbol Definition Error Across Files (v6.0)
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

### Example: Circular Import Error (v6.0)
```
error: circular import detected
  tests/programs/31_import_circular_ERROR/a.cco imports b.cco (line 1)
  b.cco imports a.cco (line 1)
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
- [`examples/09_object_array_todo.cco`](examples/09_object_array_todo.cco): Arrays of Objects (`Task[]`) and `for-each` loop iteration
- [`examples/10_import_demo/`](examples/10_import_demo/): Multi-file import system (`import "shapes.cco";`)
- [`examples/11_struct_vec2.cco`](examples/11_struct_vec2.cco): Lightweight Structs (`struct`), value copies, and in-place borrowed mutation (`&Struct`)

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

## 🧪 Test Suite Matrix (v7.0)

| Test Case | Description | Result | Valgrind Leak Status |
| :--- | :--- | :---: | :---: |
| `test_lexer` | Unit tests for tokenizer, keywords (`class`, `struct`, `self`, `in`, `import`) & `&` borrow token | **PASS** | 0 Bytes Leaked |
| `test_parser` | Unit tests for AST node construction, `NODE_STRUCT` parsing & enforcement | **PASS** | 0 Bytes Leaked |
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
| `16_use_after_move_ERROR` | Formatted Rust-style rejection of use-after-move | **PASS** | Compile Error (As Expected) |
| `17_double_move_ERROR` | Formatted Rust-style rejection of double-move | **PASS** | Compile Error (As Expected) |
| `18_conditional_move_ERROR` | Formatted Rust-style rejection of conditional move | **PASS** | Compile Error (As Expected) |
| `19_move_via_return` | Returning owned objects and moving between scopes | **PASS** | 0 Bytes Leaked |
| `20_return_borrowed_ERROR` | Formatted Rust-style rejection of returning borrowed parameter | **PASS** | Compile Error (As Expected) |
| `21_stdlib_string` | Standard Library String operations (`concat`, `len`, `equals`, `substring`) | **PASS** | 0 Bytes Leaked |
| `22_stdlib_math` | Standard Library Math operations (`sqrt`, `pow`, `abs`, `min`, `max`) | **PASS** | 0 Bytes Leaked |
| `23_stdlib_file_io` | Standard Library File I/O operations (`read_file`, `write_file`) | **PASS** | 0 Bytes Leaked |
| `24_object_array_basic` | Object array allocation (`Point[]`), indexing, and scope-exit free cascade | **PASS** | 0 Bytes Leaked |
| `25_object_array_foreach` | For-each loop iteration over object arrays (`for p in pts`) | **PASS** | 0 Bytes Leaked |
| `26_object_array_free_cascade` | Null-checked release cascade skipping empty array slots | **PASS** | 0 Bytes Leaked |
| `27_object_array_move_out_ERROR` | Rejecting move-out of array element (`let p = pts[0]`) | **PASS** | Compile Error (As Expected) |
| `28_object_array_bounds_ERROR` | Rejecting constant index out-of-bounds (`pts[10]`) | **PASS** | Compile Error (As Expected) |
| `29_import_basic` | Basic multi-file import (`import "shapes.cco";`) | **PASS** | 0 Bytes Leaked |
| `30_import_diamond` | Diamond import resolution & AST de-duplication | **PASS** | 0 Bytes Leaked |
| `31_import_circular_ERROR` | Circular import cycle detection & full chain error report | **PASS** | Compile Error (As Expected) |
| `32_import_duplicate_symbol_ERROR` | Duplicate class/function definition rejection across files | **PASS** | Compile Error (As Expected) |
| `33_struct_basic` | Basic struct declaration, construction, and field printing | **PASS** | 0 Bytes Leaked |
| `34_struct_copy_semantics` | Struct value copy on assignment (`let q = p; q.x = 99; print(p.x)`) | **PASS** | 0 Bytes Leaked |
| `35_struct_borrowed_param` | In-place struct mutation via borrowed reference (`&Vec2`) | **PASS** | 0 Bytes Leaked |
| `36_struct_nonprimitive_field_ERROR` | Rejecting non-primitive field in struct with clear note | **PASS** | Compile Error (As Expected) |
| `37_struct_class_name_collision_ERROR` | Rejecting class and struct name collision with two-location error | **PASS** | Compile Error (As Expected) |

---

## 📄 License
MIT License. Developed for the Cco Source-to-Source Transpiler Sprint.


