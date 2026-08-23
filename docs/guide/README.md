# Cco Interactive Programming Guide & Architectural Reference

Welcome to the **Cco Interactive Guide**! This comprehensive documentation breaks down core C and C++ programming concepts into atomic, verified, and memory-safe Cco programs.

Every program in this guide is genuinely compiled with the Cco compiler, tested natively, verified under Valgrind (0 memory leaks, 0 errors), and cross-referenced with exact output.

---

## 📚 Guide Chapters

1. [**01. C Basics & Fundamentals**](01-c-basics.md) (Sections 1–7 | Topics 01–55)
   - Quickstart, Variables, Data Types & Constants, Arithmetic, Assignment, Comparison, Logical Operators, Booleans.
2. [**02. Control Flow & Logic**](02-c-control-flow.md) (Sections 8–16 | Topics 56–102)
   - Conditional Statements (If...Else), Real-World Problem Solving, Multi-Way Branching & Enums (Switch-Equivalent), While, Do...While, For Loops, Nested Loops, Loop Control (Break & Continue).
3. [**03. Data Collections & Standard I/O**](03-c-arrays-strings-io.md) (Sections 17–24 | Topics 103–126)
   - Array Fundamentals, Array Iteration, Multi-Dimensional Matrices, Strings, Escape Sequences, Built-In String Functions, Standard Input Parsing.
4. [**04. Memory References & Functions**](04-c-pointers-functions-math.md) (Sections 25–37 | Topics 127–162)
   - Memory Addresses & Safe References (`&Type`), Safe References vs Raw Pointers, Array Storage, Cursor Navigation, Multi-Level Indirection, Function Declarations & Signatures, Math Functions, Inlining, Recursion, Interface Dispatch, Callback Comparators.
5. [**05. Data Modeling & Dynamic Memory**](05-c-files-structs-memory.md) (Sections 38–54 | Topics 163–195)
   - File I/O (Create, Read, Write, Append, Multiline), Structs & Mutation, Nested Structs, Tagged Unions (`enum`), Type Aliases, Hardware Padding & Alignment, Memory Management & RAII, Dynamic Allocation (`alloc()`), Dynamic Growth, Automatic Zero-Leak Freeing, Auto-Scaling Dynamic Lists.
6. [**06. Safety, Modules & Systems Programming**](06-c-safety-modules-lowlevel.md) (Sections 55–65 | Topics 196–217)
   - Runtime Guardrails, Tracepoint Diagnostics, NULL Safety (`Option` Enums), Structured Error Handling (`Result` Enums), Input Validation, PRNG Simulations, Preprocessor Alternatives, Native Module Imports (`import`), Storage Classes & Stateful Objects, Bitwise Permissions, Bounded Domain Types.
7. [**07. C++ to Cco Comparisons**](07-cpp-comparisons.md) (Sections 66–70 | Topics 218–225)
   - C++ Streams (`std::cout`) vs `print()`, `auto` Type Deduction vs `let`, Procedural vs OOP Paradigms, First C++ Program Anatomy, C++ Namespaces vs Cco Global & Module Scope, Sequential Statement Execution.

---

## 🎯 Verification Guarantee

All 225 guide programs are continuously verified against the standard Cco compiler:
- **100% Compilation Pass Rate** (0 compiler errors, 0 warnings under `-Wall -Wextra -Werror -pedantic-errors -std=c11`).
- **100% Valgrind Clean** (0 memory leaks, 0 memory corruption errors).
- **100% Output Fidelity** (every shown output matches real runtime execution).

For full details, see the [C to Cco Findings & Verification Report](../C_TO_CCO_FINDINGS.md).
