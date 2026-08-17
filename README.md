# CMM (C--) Compiler: A C-like Language with Compile-Time Single Ownership & Move Semantics (v3.0)

**CMM (C--)** is a lightweight, systems programming language with explicit C-like syntax, **scope-exit auto-free for raw allocations**, and **compile-time single ownership with move semantics** for classes and objects. It transpiles CMM source code (`.cmm`) into portable, standard C11 source code (`.c`), which is compiled to native machine binaries using `gcc` or `clang`.

> **Write it like Python's class syntax reads. Compile it and it runs like C with zero runtime reference counting overhead, zero GC pauses, no manual free(), and compile-time ownership safety.**

---

## 🏛️ The Three Pillars of CMM

1. **RUNS LIKE C**  
   CMM is a source-to-source transpiler, not an interpreter and not a VM. Every `.cmm` file becomes real, flat C11 source, compiled by `gcc` to a native binary. There is no runtime interpreter loop, no bytecode dispatch, no runtime reference counter, no VM overhead. A CMM program's speed ceiling is C's speed ceiling, full stop.

2. **HAS OBJECTS LIKE C++**  
   CMM provides `class`, fields, methods, and `obj.method(args)` call syntax—the actual ergonomic win of C++ over plain C, placing verbs next to their nouns instead of `distance(&a, &b)` scattered functions. Under the hood it translates to C structs and functions taking a `self` pointer—no vtables, no multiple inheritance, no operator overloading, no templates, no name-mangling maze.

3. **WRITTEN LIKE IT'S EASY**  
   No manual `malloc`/`free`. No header files to keep in sync with `.c` files. No `->` vs `.` decision (member access is always `.`). Memory is managed automatically via scope-exit auto-free (for raw allocations) and **compile-time single ownership with move semantics** (for objects)—both deterministic, zero runtime overhead, zero GC pause.

---

## 🌟 Key Language Features (v3.0)

- **Classes & Methods**: Declare classes with typed fields and methods taking explicit `self`.
- **Compile-Time Single Ownership & Move Semantics**:
  - Every object has exactly **one owning variable**.
  - Assigning or passing an object without `&` is a **MOVE**.
  - Passing an object with `&` (`p: &Point`) is a **BORROW** without transferring ownership.
  - Returning an owned object transfers ownership to the caller.
  - Compile-time static analysis detects and rejects at compile-time:
    1. **Use-after-move**
    2. **Double-move**
    3. **Conditional move**
    4. **Returning a borrowed parameter**
- **Zero Runtime Overhead**: No `__rc` header field on structs, no runtime retain/release calls. Destructors (`ClassName_free`) are deterministically injected by the compiler at scope exit or move points.
- **Scope-Exit Auto-Free (`alloc`)**: Memory allocated via `alloc(type, count)` returns a managed heap pointer that is automatically freed by the compiler when its enclosing scope exits—including early `return`, `break`, `continue`, or normal block fallthrough.
- **Valgrind Validated**: All valid programs pass full leak checking (`--leak-check=full --error-exitcode=1`) with **0 memory leaks and 0 errors**.

---

## 💡 Transpilation Example: Move Semantics & Borrowing

### CMM Source (`examples/points_demo.cmm`)
```cmm
class Point {
    x: int;
    y: int;

    fn sum(self) -> int {
        return self.x + self.y;
    }
}

fn describe(p: &Point) -> void {
    print(p.sum());
}

fn main() -> int {
    let a: Point = Point { x: 3, y: 4 };
    describe(a); // Borrowed: a remains valid
    print(a.sum());

    let b: Point = a; // Moved: ownership transferred to b
    print(b.sum());
    return 0;
}
```

### Transpiled Standard C
```c
typedef struct Point Point;

struct Point {
    int x;
    int y;
};

static inline void Point_free(Point *p) {
    if (p) {
        free(p);
    }
}

static inline Point *Point_new(int x, int y) {
    Point *__obj = (Point *)malloc(sizeof(Point));
    __obj->x = x;
    __obj->y = y;
    return __obj;
}

int Point_sum(Point * self) {
    return (self->x + self->y);
}

void describe(Point * p) {
    printf("%d\n", (int)(Point_sum(p)));
}

int main(void) {
    Point * a = Point_new(3, 4);
    describe(a);
    printf("%d\n", (int)(Point_sum(a)));
    Point * b = a;
    printf("%d\n", (int)(Point_sum(b)));
    Point_free(b);
    return 0;
}
```

---

## 🛠️ Build and Testing Instructions

### Requirements
- Linux (Ubuntu 20.04+)
- `gcc` (v9+) or `clang`
- `make`
- `valgrind`

### Quick Start
```bash
# Clone and build the CMM compiler executable
make cmm

# Run full Unit and Integration Test Suite under Valgrind
make test
```

---

## 🧪 Test Suite Matrix

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
| `16_use_after_move_ERROR` | Compile-time rejection of use-after-move | **PASS** | Compile Error 1 (As Expected) |
| `17_double_move_ERROR` | Compile-time rejection of double-move | **PASS** | Compile Error 1 (As Expected) |
| `18_conditional_move_ERROR` | Compile-time rejection of conditional move | **PASS** | Compile Error 1 (As Expected) |
| `19_move_via_return` | Returning owned objects and moving between scopes | **PASS** | 0 Bytes Leaked |
| `20_return_borrowed_ERROR` | Compile-time rejection of returning borrowed parameter | **PASS** | Compile Error 1 (As Expected) |

---

## 📄 License
MIT License. Developed for the CMM Source-to-Source Transpiler Sprint.
