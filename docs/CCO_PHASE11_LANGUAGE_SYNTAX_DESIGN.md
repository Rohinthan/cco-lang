# Cco Compiler — Phase 11 Language Syntax Design & Ergonomics
## Python-Like Declaration Model & Surface Syntax Completion

**Author:** Cco Compiler Team  
**Date:** September 2026  
**Status:** Approved for Implementation  
**Target:** Phase 11 Completion  

---

## 1. Executive Summary & Design Vision

In Phase 10, Cco introduced high-level ergonomic syntax borrowed from modern readable languages:
- Python-style `#` comments
- Logical keywords `and`, `or`, `not`
- Optional parentheses in `if` and `while`
- Counted range loops (`for i in 0..10`)
- Expression-bodied functions (`fn square(x: int) -> int = x * x;`)
- Top-level program scripts without mandatory `fn main() -> int` boilerplate

Despite these improvements, the most prominent syntactic vestige of early Rust-inspired Cco remained: **mandatory `let`** for variable declarations:
```cco
let x = 10;
let name = "Cco";
```

Phase 11 removes `let` from canonical Cco syntax, replacing it with Python-style direct assignment:
```cco
x = 10;
name = "Cco";
```

Crucially, **Cco is not becoming a dynamically typed or interpreted language**. Cco remains:
1. **A statically compiled native language**: Every variable has a single concrete type known at compile time.
2. **Deterministic & Low-Overhead**: No dynamic boxing, no runtime tag checks, no hidden GC.
3. **Lexically Scoped**: Variables adhere to deterministic block scopes with ownership tracking and RAII-style heap destruction.
4. **Compatible**: Existing codebases and self-hosted compiler modules using `let` continue to parse and compile without changes.

---

## 2. Grammar & Surface Syntax Specification

### 2.1 Canonical Variable Declaration by Assignment
In canonical Cco, variable creation occurs via standard assignment:
```cco
identifier = expression;
```

#### Disambiguation Rule:
- When the compiler encounters an assignment to an identifier `ident`:
  - **First Occurrence in Visible Scope:** If `ident` is not declared in the current lexical scope or any enclosing parent scope, this assignment is recognized as a **Variable Declaration**.
    - The variable is allocated in the current lexical block.
    - Its static type is inferred from the right-hand side expression.
    - It is registered in the scope's symbol table.
  - **Subsequent Occurrence:** If `ident` is already declared in the current or an enclosing scope, this assignment is recognized as a **Variable Reassignment**.
    - The right-hand side expression type is verified against the variable's existing static type.
    - If the types are incompatible, a compile-time error is raised.

### 2.2 Explicit Python-Style Type Annotations
For cases where explicit typing is desired (for documentation, interface boundaries, or numeric precision such as initializing a float from an integer literal), Cco introduces Python-style type annotations:
```cco
identifier: type = expression;
```
Examples:
```cco
x: int = 10;
pi: float = 3.14159;
msg: string = "Hello Cco";
buffer: [int] = alloc(int, 64);
```

#### Grammar Production:
```ebnf
VarDeclOrAssignStmt ::= Ident ( ":" Type )? "=" Expr ";"
                      | Target ( "+=" | "-=" | "*=" | "/=" | "%=" ) Expr ";"
                      | Target ( "++" | "--" ) ";"
                      | ExprStmt
```
Notice that:
- `ident: type = expr;` is unambiguous because labels are not supported in Cco, and `:` does not appear as a binary statement prefix.
- The initializer expression is mandatory; uninitialized variable declarations are prohibited, preserving Cco's memory safety guarantee.

### 2.3 Backward Compatibility with `let`
Legacy `let` declarations remain supported for backward compatibility:
```cco
let x = 10;
let y: float = 3.14;
```
All existing tests, benchmarks, and self-hosted compiler stages continue to compile unchanged.

---

## 3. Static Semantics, Scope, & Lifetime Rules

### 3.1 Lexical Block Scoping
Cco enforces predictable, compile-time lexical block scoping:
1. **Scope Boundaries:** Every `{ ... }` block (function body, `if`, `else`, `while`, `for`, `match`) forms a discrete lexical scope.
2. **Inner Declarations:** If an assignment `z = 30;` occurs inside a block where `z` has not been declared in any outer scope, `z` belongs strictly to that block.
   - Its stack frame slot is allocated within that scope.
   - Its heap resources (strings, dynamic arrays) are automatically freed at the end of the block.
   - It cannot be accessed outside the block.
3. **Outer Reassignments:** If `x` was already declared in an outer scope, assigning `x = 20;` inside a nested block updates the existing outer variable:
```cco
x = 10;
if condition {
    x = 20;     # Reassigns outer x
    y = 30;     # Declares local y in 'then' block
    print(y);   # Valid: prints 30
}
print(x);       # Valid: prints 20
# print(y);     # Compile-time error: 'y' is undefined in outer scope
```

### 3.2 Undefined Variable Detection
Reading any variable before its declaration/initialization in the current execution flow is a compile-time error:
```cco
print(a); # COMPILE ERROR: undefined variable 'a'
a = 10;
```
The compiler traverses statements sequentially within each block. A variable is only visible to statements that follow its declaration.

### 3.3 Reassignment Type Invariance
Variables are statically typed. Once inferred or annotated, a variable's type cannot change:
```cco
x = 10;
x = 20;      # OK: int -> int

x = 3.14;    # COMPILE ERROR: cannot assign float to variable 'x' of type int
```
Similarly, string and heap variables enforce strict type checking:
```cco
name = "Cco";
name = 42;    # COMPILE ERROR: cannot assign int to variable 'name' of type string
```

---

## 4. Compiler Architecture & Transformation Strategy

### 4.1 Parser Layer (`src/parser.c`)
1. **Explicit Type Annotations:**
   In `parse_assign_or_expr_stmt`:
   - Detect `TOKEN_IDENT` followed immediately by `TOKEN_COLON`.
   - Parse type via `parse_type_with_class`.
   - Consume `TOKEN_ASSIGN` (`=`) and parse the initializer expression.
   - Emit a `NODE_LET` AST node with `has_explicit_type = true`.
2. **Assignment Parsing:**
   - Standard `ident = expr;` parses as `NODE_ASSIGN`.

### 4.2 Desugaring & Semantic Pass (`src/parser.c` / `desugar_and_infer_program`)
To cleanly bridge assignment-based declarations with Cco's existing backend (IR, SSA, C-codegen, and scope ownership analysis), a scoped preprocessing walk is integrated:
1. Maintain a scoped symbol environment during `desugar_stmt_node`:
   - Function parameters are pre-populated as defined variables.
   - When entering a `{ ... }` block, push a new lexical scope.
   - When exiting a `{ ... }` block, pop the scope.
2. For each `NODE_ASSIGN` where target is an identifier `name`:
   - Check if `name` is already bound in the current scope or any parent scope.
   - **If not bound:**
     - Transform `NODE_ASSIGN` into `NODE_LET`!
     - Infer the variable's type from the initializer expression.
     - Register `name` (with its inferred type) in the current scope.
   - **If already bound:**
     - Keep as `NODE_ASSIGN`.
     - Verify that the assigned expression type matches the bound variable's type.
     - Emit a clear compile diagnostic if types conflict.
3. For expressions containing `NODE_IDENT`:
   - Verify that the identifier exists in the current scope chain or function parameters.
   - If not found, emit a formatted compile-time error:
     `error: undefined variable '<name>'`.

### 4.3 IR Lowering & Backend Invariance
Because `NODE_ASSIGN` declarations are transformed into `NODE_LET` during the desugaring and type inference pass:
- `src/ir_lower.c` continues to allocate stack slots via `ir_emit_alloca` for all new declarations.
- SSA construction (`src/ir_ssa.c`), memory-to-register promotion (mem2reg), optimization passes (SCCP, LICM, GVN/CSE, DCE), register allocation, and native x86-64 code generation require zero modifications.
- The ELF64 object generator and internal linker remain 100% intact.

---

## 5. Semicolon Analysis & Future Roadmap

In Cco Phase 11:
- Semicolons remain required to delimit statements (`x = 10;`, `name = "Cco";`).
- **Rationale:**
  - Unrestricted optional semicolons in a multi-token statement language require newline-sensitive tokenization (like Python's `NEWLINE`/`INDENT`/`DEDENT` tokens or Go's lexer-injected semicolons).
  - Injecting semicolons automatically before operators (e.g. multi-line binary expressions, method chaining) requires careful lookahead rules.
  - Phase 11 focuses on the Python-like declaration model (`x = 10;`, `x: int = 10;`).
  - Optional semicolons are planned for an upcoming syntax ergonomics phase (Phase 12), ensuring complete grammar stability.

---

## 6. Verification & Parity Matrix

The Phase 11 changes will be verified across:
1. **Unit Tests:** Direct testing of declaration inference, explicit type annotations, reassignment type validation, and undefined variable errors in `tests/unit/test_syntax.c`.
2. **Integration Tests:** New test cases verifying:
   - Top-level and function-level `x = 10;` syntax.
   - Explicit annotations `x: int = 10;`, `f: float = 3.14;`.
   - Compile-time error on undefined variable read.
   - Compile-time error on type mismatch during reassignment.
   - Nested block scoping and automatic cleanup.
3. **Self-Hosting & Bootstrap:** Full compilation of `selfhost/*.cco` and 4-stage bootstrap compiler parity.
4. **Valgrind Sanitization:** Zero memory leaks, zero file descriptor leaks across all 116+ existing integration tests and new Phase 11 tests.
