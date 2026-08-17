# CMM (C--) Compiler: A C-like Language with Automatic Memory Management and C++-Style Objects

**CMM (C--)** is a lightweight, systems programming language with explicit C-like syntax, **scope-exit auto-free for raw allocations**, and **deterministic reference-counted memory management for classes and objects**. It compiles CMM source code (`.cmm`) into portable, standard C11 source code (`.c`), which is then compiled to native machine binaries using `gcc` or `clang`.

> **Write it like Python's class syntax reads. Compile it and it runs like C. No garbage collector pause, no manual free(), no header files.**

---

## 🏛️ The Three Pillars of CMM

1. **RUNS LIKE C**  
   CMM is a source-to-source transpiler, not an interpreter and not a VM. Every `.cmm` file becomes real, flat C11 source, compiled by `gcc` to a native binary. There is no runtime interpreter loop, no bytecode dispatch, no hidden VM overhead. A CMM program's speed ceiling is C's speed ceiling, full stop.

2. **HAS OBJECTS LIKE C++**  
   CMM provides `class`, fields, methods, and `obj.method(args)` call syntax—the actual ergonomic win of C++ over plain C, placing verbs next to their nouns instead of `distance(&a, &b)` scattered functions. Under the hood it translates to C structs and functions taking a `self` pointer—no vtables, no multiple inheritance, no operator overloading, no templates, no name-mangling maze.

3. **WRITTEN LIKE IT'S EASY**  
   No manual `malloc`/`free`. No header files to keep in sync with `.c` files. No `->` vs `.` decision (member access is always `.`). Memory is managed automatically via scope-exit auto-free (v1, for raw allocations) and reference counting (v2, for objects)—both deterministic, both zero-runtime-pause.

---

## 🌟 Key Language Features

- **Classes & Methods**: Declare classes with typed fields and methods taking explicit `self`.
- **Automatic Reference Counting (ARC)**: Objects carry an internal `__rc` count. Retains are automatically injected on assignment/aliasing, releases on scope exit, reassignment, or early return. Objects are freed immediately when `__rc == 0`.
- **Scope-Exit Auto-Free (`alloc`)**: Memory allocated via `alloc(type, count)` returns a managed heap pointer that is **automatically freed by the compiler** when its enclosing scope exits—including early `return`, `break`, `continue`, or normal block fallthrough.
- **Ownership Transfer Semantics**: Returning an object or heap-allocated pointer from a function transfers ownership seamlessly to the caller.
- **Valgrind Validated**: Passes full leak checking (`--leak-check=full --error-exitcode=1`) with **0 memory leaks and 0 errors**.

---

## 💡 Transpilation Example: Classes & Refcounting

### CMM Source (`examples/points_demo.cmm`)
```cmm
class Point {
    x: int;
    y: int;

    fn sum(self) -> int {
        return self.x + self.y;
    }
}

fn main() -> int {
    let a: Point = Point { x: 3, y: 4 };
    let b: Point = a;              // alias, __rc becomes 2
    print(a.sum());
    print(b.sum());
    let c: Point = Point { x: 10, y: 20 };
    c = Point { x: 1, y: 1 };      // old c released, freed immediately
    print(c.sum());
    return 0;
}
```

### Transpiled Standard C
```c
typedef struct Point Point;
struct Point {
    int __rc;
    int x;
    int y;
};

static inline Point *Point_retain(Point *p) {
    if (p) p->__rc++;
    return p;
}

static inline void Point_release(Point *p) {
    if (p && --p->__rc == 0) {
        free(p);
    }
}

static inline Point *Point_new(int x, int y) {
    Point *__obj = (Point *)malloc(sizeof(Point));
    __obj->__rc = 1;
    __obj->x = x;
    __obj->y = y;
    return __obj;
}

int Point_sum(Point * self) {
    __typeof__((self->x + self->y)) __cmm_ret_val = (self->x + self->y);
    return __cmm_ret_val;
}

int main(void) {
    Point * a = Point_new(3, 4);
    Point * b = Point_retain(a);
    printf("%d\n", (int)(Point_sum(a)));
    printf("%d\n", (int)(Point_sum(b)));
    Point * c = Point_new(10, 20);
    Point_release(c);
    c = Point_new(1, 1);
    printf("%d\n", (int)(Point_sum(c)));
    __typeof__(0) __cmm_ret_val = 0;
    Point_release(c);
    Point_release(b);
    Point_release(a);
    return __cmm_ret_val;
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
| `test_lexer` | Unit tests for tokenizer line/column, keywords (`class`, `self`) & tokens | **PASS** | 0 Bytes Leaked |
| `test_parser` | Unit tests for AST node construction & class/method parsing | **PASS** | 0 Bytes Leaked |
| `test_scope` | Unit tests for scope analysis pass, free & release injection | **PASS** | 0 Bytes Leaked |
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
| `11_aliasing_refcount` | Object aliasing, `__rc` retain tracking & single free | **PASS** | 0 Bytes Leaked |
| `12_reassign_object` | Reassigning object variables with automatic release | **PASS** | 0 Bytes Leaked |
| `13_object_early_return` | Object lifetime inside loops with early returns | **PASS** | 0 Bytes Leaked |

---

## 📄 License
MIT License. Developed for the CMM Source-to-Source Transpiler Sprint.
