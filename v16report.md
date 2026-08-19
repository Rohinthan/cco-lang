# Cco v16.0 Implementation Report: Operator Overloading for Structs

**Author**: Antigravity  
**Version**: v16.0  
**Status**: Completed & Verified (79/79 Integration Tests Passing, 104/104 Lexer Comparison Files Passing, 0 Memory Leaks under Valgrind)

---

## 1. Executive Summary

Cco v16.0 introduces **Operator Overloading for Structs** at the top-level grammar and code generation layers. Structs in Cco are pure value types (copied on assignment and parameter passing, stack-allocated, zero heap ownership tracking). This makes operator overloading on structs completely safe and zero-overhead: binary operators (`+`, `-`, `*`, `/`, `==`, `!=`) and unary negation (`-`) take struct copies by value and return values, desugaring to standard C functions without any ownership or lifetime complexity.

---

## 2. Architecture & Design

### A. Grammar & Lexer Extensions (`src/lexer.h`, `src/lexer.c`, `src/parser.c`)
- **`operator` Keyword**: Added `TOKEN_OPERATOR` keyword recognized by the C lexer and the self-hosted lexer (`selfhost/lexer.cco`).
- **Grammar Addition**: Extended `function_decl` to support `fn operator<symbol>(params) -> type block`:
  - `operator+`, `operator-`, `operator*`, `operator/`
  - `operator==`, `operator!=`
- **Top-Level Non-Method Functions**: Operator functions are declared at top level, keeping the rule that structs do not define methods intact.

### B. Validation & Semantic Checking (`src/parser.c`, `src/module_resolver.c`)
- **Struct-Only Restriction**: Operator overloading is strictly restricted to `struct` types. Declarations for `class` types are rejected with a clear diagnostic message explaining that class operator overloading requires move/borrow semantics.
- **Arity Checks**:
  - `+`, `*`, `/`, `==`, `!=`: exactly 2 parameters.
  - `-`: 1 parameter (unary negation) or 2 parameters (binary subtraction).
- **Matching Operand Types**: Both parameters of a binary operator function must be the exact same struct type.
- **Comparison Return Type**: `operator==` and `operator!=` must return `bool`.
- **Duplicate Operator Detection**: Two operator functions with the same symbol, arity, and struct type are rejected with a two-location error note.

### C. Call-Site Desugaring & Code Generation (`src/codegen.c`)
- **Function Name Mangling**:
  - `operator+(a: Vec2, b: Vec2)` -> `__cco_operator_add_Vec2(Vec2 a, Vec2 b)`
  - `operator-(a: Vec2, b: Vec2)` -> `__cco_operator_sub_Vec2(Vec2 a, Vec2 b)`
  - `operator-(a: Vec2)` -> `__cco_operator_neg_Vec2(Vec2 a)`
  - `operator*(a: Vec2, b: Vec2)` -> `__cco_operator_mul_Vec2(Vec2 a, Vec2 b)`
  - `operator/(a: Vec2, b: Vec2)` -> `__cco_operator_div_Vec2(Vec2 a, Vec2 b)`
  - `operator==(a: Vec2, b: Vec2)` -> `__cco_operator_eq_Vec2(Vec2 a, Vec2 b)`
  - `operator!=(a: Vec2, b: Vec2)` -> `__cco_operator_ne_Vec2(Vec2 a, Vec2 b)`
- **Resolution**: In `gen_expr` for `NODE_BINARY` and `NODE_UNARY`, when operands are of struct type, the compiler looks up the matching operator definition and emits a call to `__cco_operator_<op>_<Struct>()`. If not defined, a diagnostic compile error is raised suggesting the fix.
- **Forward Prototypes**: Function and operator prototypes are emitted at the top of the generated C file to permit mutually recursive or forward-referenced operator calls.

### D. Scope & Ownership Analysis Pass (`src/scope_analysis.c`)
- **Confirmed Zero Changes**: Because structs are value types copied byte-for-byte with no heap pointer ownership, `scope_analysis.c` required zero changes.

### E. Self-Hosted Lexer Parity (`selfhost/lexer.cco`)
- Updated `selfhost/lexer.cco` with the `operator` keyword in `init_keywords()`, verifying 100% token parity across all 104 `.cco` files in the repository.

---

## 3. Strict ISO C11 Portability

All generated C11 code is compiled with `-Wall -Wextra -Werror -pedantic-errors -std=c11`:
- No compiler-specific extensions or runtime overhead.
- Clean C function calls passing structs by value.

---

## 4. Test Suite Matrix

| Test Name | Description | Status | Leaks |
| :--- | :--- | :--- | :--- |
| `75_operator_overload_add` | Basic binary struct operator overloading (`operator+`) | **PASS** | 0 Bytes Leaked |
| `76_operator_overload_multiple` | Multiple struct operators (`+`, `-`, `*`, `==`, `!=`) | **PASS** | 0 Bytes Leaked |
| `77_operator_overload_missing_ERROR` | Rejecting missing struct operator definition at compile time | **PASS** | Compile Error (As Expected) |
| `78_operator_overload_unary_neg` | Unary negation operator overloading (`operator-(a)`) by arity | **PASS** | 0 Bytes Leaked |
| `79_operator_overload_class_ERROR` | Rejecting operator overloading for class types (struct-only in v16) | **PASS** | Compile Error (As Expected) |
| `compare_lexers` | Self-hosted lexer comparison across 104 `.cco` files (100% parity) | **PASS** | 0 Bytes Leaked |
