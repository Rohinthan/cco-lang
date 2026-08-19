# Cco v17.0 Implementation Report: Interfaces via Compile-Time Monomorphization

**Version**: v17.0  
**Status**: Completed & Verified (89/89 Integration Tests Passing, 115/115 Lexer Comparison Files Passing, 0 Memory Leaks under Valgrind)

---

## 1. Executive Summary

Cco v17.0 introduces **Interfaces via Compile-Time Monomorphization** — a zero-runtime-cost abstraction mechanism that avoids runtime vtables, fat pointers, and garbage collection overhead. Interfaces in Cco allow writing reusable generic functions with explicit trait constraints (`impl Trait` and `&impl Trait`). During compilation, the compiler checks explicit conformance (`impl Trait for Class;`), validates that generic template function bodies only access methods declared in the interface, monomorphizes each generic function into concrete specialized functions for every concrete type used at call sites (`fn__ConcreteClass`), and drops unused templates so zero dead code is emitted.

---

## 2. Architecture & Design

### A. Grammar & Lexer Extensions (`src/lexer.h`, `src/lexer.c`, `selfhost/lexer.cco`)
- **New Keywords**:
  - `interface`: Declares an interface containing method signatures.
  - `impl`: Explicit assertion that a class implements an interface, or parameter type annotation (`impl Trait` / `&impl Trait`).
  - `Self`: Refers to the implementing concrete class within interface method signatures.
- **Self-Hosted Lexer Parity**: Updated `selfhost/lexer.cco` with `interface`, `impl`, and `Self` in `init_keywords()`. Verified 100% token-by-token parity across 115 test files.

### B. AST & Parser (`src/ast.h`, `src/parser.c`, `src/class_decl.c`)
- **New AST Nodes**:
  - `NODE_INTERFACE`: Contains interface name and array of `NODE_INTERFACE_METHOD` nodes.
  - `NODE_INTERFACE_METHOD`: Method signature declaring name, parameters, return type, and optional `Self` usage.
  - `NODE_IMPL`: Explicit conformance assertion linking an interface name to a class name.
- **Top-Level Program Parsing**: Parser extended to parse top-level `interface` definitions and `impl Interface for Class;` declarations.
- **Generic Parameter Annotations**: Function parameters support `impl InterfaceName` (owned) and `&impl InterfaceName` (borrowed).

### C. Trait Resolver & Monomorphization Pass (`src/trait_resolver.h`, `src/trait_resolver.c`)
- **Pipeline Location**: Runs directly after multi-file module resolution and before scope/ownership analysis (`resolve_program()` -> `resolve_and_monomorphize_traits()` -> `analyze_scopes()`).
- **Section 6 Impl Conformance Checking**:
  - Validates that every referenced interface and class exists in the merged symbol table.
  - Strictly rejects `impl` declarations on `struct` types (`error: interfaces can only be implemented by classes — '<struct>' is a struct, which cannot have methods`).
  - Validates that the implementing class defines every required method with exact signature matching (parameter count, types, borrow qualifiers, and return types, substituting `Self` with the concrete class).
- **Section 7 Template Body Validation**:
  - Statically analyzes generic function bodies to guarantee that only declared interface methods are called on `impl Trait` parameters before monomorphization.
- **Call-Site Traversal & Specialization**:
  - Discovers all call sites targeting generic functions with `impl Trait` parameters.
  - Validates that the argument passed satisfies the required interface via confirmed `impl` assertions.
  - Generates specialized function clones named `<fn>__<ConcreteClass>` with concrete parameter types.
  - Rewrites call sites to target the specialized functions.
  - Silently drops uncalled generic templates, ensuring zero unspecialized AST nodes reach scope analysis or code generation.

### D. Zero-Cost Scope & Ownership Integration (`src/scope_analysis.c`, `src/codegen.c`)
- **Seamless Pipeline Reuse**: Monomorphized functions are ordinary Cco AST functions with concrete class parameter types. They seamlessly pass through Cco v3 single-ownership analysis, borrow checking, and scope-exit auto-free without requiring downstream pipeline modifications.
- **C11 Code Generation**: Monomorphized calls emit standard direct C function calls (`Point_describe(p)`), yielding raw C performance with zero virtual dispatch overhead and -O3 compiler optimization.

---

## 3. Strict ISO C11 Portability & Zero Leaks

All generated C11 code is verified with `-Wall -Wextra -Werror -pedantic-errors -std=c11`:
- No runtime vtables, no pointer indirection, no dynamic type tags.
- Full Valgrind memory safety verification (0 leaks, 0 errors across all tests).

---

## 4. Test Suite Matrix (Tests 80–89)

| Test Name | Description | Status | Leaks |
| :--- | :--- | :--- | :--- |
| `80_interface_basic` | Single interface, single implementing class, single monomorphized call | **PASS** | 0 Bytes Leaked |
| `81_interface_multiple_impls` | Multiple classes (`Point`, `Circle`) implementing the same interface (`Describable`) | **PASS** | 0 Bytes Leaked |
| `82_interface_missing_method_ERROR` | Rejecting `impl` declaration when a class lacks a required method | **PASS** | Compile Error (As Expected) |
| `83_interface_signature_mismatch_ERROR` | Rejecting `impl` declaration on method return type / signature mismatch | **PASS** | Compile Error (As Expected) |
| `84_interface_struct_impl_ERROR` | Rejecting `impl` declaration for `struct` value types | **PASS** | Compile Error (As Expected) |
| `85_interface_unimplementing_call_ERROR` | Rejecting generic function calls with classes that lack an explicit `impl` | **PASS** | Compile Error (As Expected) |
| `86_interface_non_interface_method_ERROR` | Rejecting calls to non-interface methods inside generic function body | **PASS** | Compile Error (As Expected) |
| `87_interface_borrowed_param` | Generic function taking borrowed `&impl Trait` parameter without move | **PASS** | 0 Bytes Leaked |
| `88_interface_self_param` | Interface methods using `Self` in parameter signatures (`Comparable`) | **PASS** | 0 Bytes Leaked |
| `89_interface_unused_template_dropped` | Unused generic template functions pruned without dead code or linker errors | **PASS** | 0 Bytes Leaked |
| `compare_lexers` | Self-hosted lexer comparison across 115 `.cco` files (100% parity) | **PASS** | 0 Bytes Leaked |
