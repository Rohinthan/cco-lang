# Cco Language Reference Manual (Version 11)

Welcome to the canonical reference manual for **Cco** (pronounced *cee-co*), a statically typed, compiled systems programming language combining Python-like surface readability with native execution, explicit compiler pipelines, SSA optimizations, deterministic profile-guided optimization (PGO), and single-ownership memory management without a tracing garbage collector.

---

## 1. Lexical Structure

### 1.1 Comments
Cco supports three comment forms:
- **Python-Style Line Comments:** `# text...` ignores all characters until the end of the line.
- **C-Style Line Comments:** `// text...` ignores all characters until the end of the line.
- **C-Style Block Comments:** `/* text... */` spans multiple lines and is terminated by `*/`.

```cco
# This is a Python-inspired comment
// This is a C-style comment
/* Multi-line
   comment */
```

### 1.2 Identifiers & Keywords
Identifiers start with an ASCII letter (`a-z`, `A-Z`) or underscore `_`, followed by letters, digits, or underscores.

Reserved Keywords:
```
alloc       and         bool        break       char        class
continue    else        enum        false       float       fn
for         if          impl        import      in          int
interface   let         list_new    map         map_new     match
not         operator    or          print       return      self
Self        string      struct      true        void        while
_
```

### 1.3 Literals
- **Integer:** Decimal numbers, e.g., `0`, `42`, `-100`.
- **Float:** Floating-point decimal, e.g., `3.14159`, `0.5`, `123.0`.
- **Character:** Single character enclosed in single quotes, e.g., `'a'`, `'\n'`, `'\''`.
- **String:** Sequence of characters in double quotes, e.g., `"Hello, world!\n"`.
- **Format String (f-string):** String literal prefixed with `f"..."`, allowing embedded expressions inside `{...}`, e.g., `f"Count: {x + 1}, name: {name}"`. Double braces `{{` and `}}` escape the braces.
- **Boolean:** `true`, `false`.

### 1.4 Operators and Delimiters
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `++`, `--`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `and`, `or`, `not` (alongside `&&`, `||`, `!`)
- Range: `..`
- Delimiters: `(`, `)`, `{`, `}`, `[`, `]`, `,`, `:`, `;`, `->`, `=>`, `.`

---

## 2. Program Structure & Entry Point

### 2.1 Top-Level Script Mode
Cco programs can be written as top-level scripts without an explicit `fn main() -> int` wrapper:

```cco
let x = 10;
let y = 20;

print(x + y);
```

The compiler automatically synthesizes `main()` returning `0`.

### 2.2 Explicit Main Function
Alternatively, an explicit entry function can be defined:

```cco
fn main() -> int {
    print("Application started");
    return 0;
}
```

*Note:* Mixing top-level statements with an explicit `fn main()` produces a compile-time error.

### 2.3 Imports
Imports must precede declarations and statements:
```cco
import "std/net.cco";
import std::math;
```

---

## 3. Types & Variables

### 3.1 Primitive Types
- `int`: Signed 64-bit integer.
- `float`: 64-bit IEEE-754 double precision float.
- `bool`: Boolean (`true` or `false`).
- `char`: 8-bit ASCII character.
- `string`: Immutable string value.
- `void`: Empty unit return type.

### 3.2 Canonical Variable Declarations (Assignment-Based)
In canonical Cco (Version 11), variables are declared directly via assignment without requiring the `let` keyword. The first assignment to an identifier binds and infers its static type:

```cco
x = 10;             # Declares x, statically inferred as int
pi = 3.14159;       # Declares pi, statically inferred as float
name = "Cco";       # Declares name, statically inferred as string
is_valid = true;    # Declares is_valid, statically inferred as bool
```

### 3.3 Python-Style Explicit Type Annotations
Explicit type annotations are supported using Python-style colon syntax `ident: Type = expr;`:

```cco
x: int = 10;
value: float = 3.14;
title: string = "Cco Language";
enabled: bool = true;
```

If an explicit type annotation is incompatible with the assigned value (e.g., `x: int = "text";`), a compile-time type error is reported with location highlights.

### 3.4 Reassignment & Static Type Checking
Subsequent assignments to an already declared identifier reassign its value. The language remains fully statically typed—reassignments must match the variable's established type:

```cco
x = 10;             # First assignment: binds x as int
x = 20;             # Valid: reassignment with compatible int type
x += 5;             # Valid: compound assignment

x = "hello";        # Compile-time ERROR: cannot assign 'string' to variable of type 'int'
x = 3.14;           # Compile-time ERROR: incompatible floating-point reassignment
```

Reading a variable before it is declared or assigned produces a compile-time error:
```cco
print(z);           # Compile-time ERROR: undefined variable 'z'
z = 50;
```

### 3.5 Lexical Scoping
Variable declarations are lexically scoped. Variables introduced inside a block (such as an `if`, `while`, or `for` block) do not leak into the enclosing scope:
```cco
x = 10;
if true {
    y = 20;         # Inner scope variable
    x = 30;         # Modifies outer variable x
}
# y is out of scope here; reading y produces an undefined variable error
```

### 3.6 Legacy `let` Compatibility
For backwards compatibility with existing Cco codebases, the `let` keyword remains fully supported:
```cco
let legacy_a = 10;
let legacy_b: int = 20;
```

---

## 4. Functions & Methods

### 4.1 Standard Function Syntax
Functions are defined with the `fn` keyword, typed parameters, a return type arrow `->`, and a brace-enclosed body:

```cco
fn add(a: int, b: int) -> int {
    return a + b;
}
```

### 4.2 Expression-Bodied Functions
For concise, single-expression functions, Cco provides expression-body syntax using `=`:

```cco
fn square(x: int) -> int = x * x;
fn max(a: int, b: int) -> int = if a > b { a } else { b };
```

This desugars directly into a return statement inside a block, compiling with identical semantics and zero overhead.

### 4.3 Parameter Borrowing (`&`)
Parameters can be borrowed by prefixing the type with `&`:
```cco
fn print_user(u: &User) -> void {
    print(u.name);
}
```

---

## 5. Control Flow

### 5.1 Conditionals (`if` / `else`)
Parentheses around conditions are optional. Braces `{ ... }` around the body are mandatory:

```cco
if x > 10 {
    print("Large");
} else if x > 0 {
    print("Positive");
} else {
    print("Non-positive");
}
```

Legacy C-style parentheses `if (x > 10)` remain fully supported.

### 5.2 While Loops
Parentheses around the loop condition are optional:

```cco
let count = 0;
while count < 5 {
    print(count);
    count += 1;
}
```

Legacy C-style parentheses `while (count < 5)` remain fully supported.

### 5.3 Counted Range Loops (`for .. in`)
Cco supports counted iteration over half-open integer intervals $[start, end)$:

```cco
let total = 0;
for i in 0..10 {
    total += i;
}
print(total);  # 45
```

Semantics:
- Interval is half-open: `0..10` iterates from `0` through `9`.
- If `start >= end`, the loop body executes zero times.
- Negative ranges are supported: `for i in -5..0`.
- Desugars to an induction variable loop with full SSA optimization.

### 5.4 Collection Iteration (`for .. in`)
Iterates over elements of an array:
```cco
for item in my_array {
    print(item);
}
```

### 5.5 C-Style For Loops
Legacy 3-clause for loops remain supported:
```cco
for (let i = 0; i < 10; i++) {
    print(i);
}
```

### 5.6 Loop Control (`break`, `continue`)
- `break;`: Immediately terminates the innermost loop.
- `continue;`: Advances directly to the next iteration.

---

## 6. Operators & Expressions

### 6.1 Boolean Logic (`and`, `or`, `not`)
Cco provides readable keyword operators alongside traditional C-style operators:

```cco
if age >= 18 and active {
    print("Eligible");
}

if not ready or cancelled {
    return 0;
}
```

### 6.2 Operator Precedence Table
Operators are listed in decreasing order of precedence:

| Precedence | Operator | Description | Associativity |
| :--- | :--- | :--- | :--- |
| 1 (Highest) | `()`, `[]`, `.`, call `()` | Primary, indexing, member access | Left-to-right |
| 2 | `not`, `!`, `-` (unary), `&` | Logical NOT, negation, borrow | Right-to-left |
| 3 | `*`, `/`, `%` | Multiplication, division, remainder | Left-to-right |
| 4 | `+`, `-` | Addition, subtraction | Left-to-right |
| 5 | `<`, `<=`, `>`, `>=` | Relational comparisons | Left-to-right |
| 6 | `==`, `!=` | Equality comparisons | Left-to-right |
| 7 | `and`, `&&` | Logical AND (short-circuiting) | Left-to-right |
| 8 (Lowest) | `or`, `\|\|` | Logical OR (short-circuiting) | Left-to-right |

---

## 7. Data Structures & Object Model

### 7.1 Classes (Heap-Allocated Reference Types)
Classes represent reference types with single-ownership semantics:
```cco
class Point {
    x: int;
    y: int;

    fn get_x(self) -> int = self.x;
}
```

### 7.2 Structs (Value-Copied Types)
Structs represent flat value types copied bitwise:
```cco
struct Vector2 {
    x: float;
    y: float;
}
```

### 7.3 Enums & Pattern Matching
Algebraic data types with payload variants and exhaustive `match`:
```cco
enum Result {
    Ok { val: int },
    Err { msg: string },
}

match r {
    Result.Ok { val } => print(val),
    Result.Err { msg } => print(msg),
    _ => print("unknown"),
}
```

### 7.4 Interfaces & Implementation
Trait-style interfaces:
```cco
interface Printable {
    fn print_me(self) -> void;
}

impl Printable for Point;
```

---

## 8. Compiler CLI Reference

Compile and run a Cco program:
```bash
cco program.cco --run
```

Compile directly to native x86-64 binary:
```bash
cco program.cco -o program
```

Compile with optimization and SSA:
```bash
cco program.cco -O2 -o program
```

Profile-guided optimization:
```bash
cco program.cco --profile-generate -o program
./program
cco program.cco --profile-use cco.profile -O2 -o program_pgo
```
