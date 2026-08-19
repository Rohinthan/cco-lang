# Cco v13.0 Implementation Report: Command-Line Arguments (`args` / `arg_count` / `program_name`)

**Author**: Antigravity  
**Version**: v13.0  
**Status**: Completed & Verified (64/64 Integration Tests Passing, 87/87 Lexer Comparison Files Passing, 0 Leaks)

---

## 1. Executive Summary

Cco v13.0 implements first-class command-line argument processing capabilities into the language standard library via three prelude functions:
- `args() -> string[]`: Returns all arguments passed to the program after the invocation name as a fresh, caller-owned array of heap strings.
- `arg_count() -> int`: Returns the total number of arguments (equivalent to `len(args())`), optimized to avoid array and string heap allocations when only the count is needed.
- `program_name() -> string`: Returns the program's invocation path (`argv[0]`) as a fresh, caller-owned string.

This standalone usability feature allows Cco programs to be written as CLI tools, scripts, and utilities that take user arguments directly from the shell without requiring external files or workarounds.

---

## 2. Architecture & Runtime Ownership Design

### A. Capturing Program Entry Arguments
Every compiled Cco program emits `main` with the standard C signature:
```c
int main(int __cco_main_argc, char **__cco_main_argv) {
    __cco_argc = __cco_main_argc;
    __cco_argv = __cco_main_argv;
    ...
}
```
Two module-static globals (`static int __cco_argc; static char **__cco_argv;`) capture the runtime arguments as the very first statements executed in `main()`, making them globally accessible to the runtime functions while avoiding symbol collisions with any local user variables named `argc` or `argv`.

### B. Ownership & Memory Safety Contract
The standard library functions strictly maintain Cco's single-ownership RAII rules:
1. **`args()`**: Allocates a dynamic growable array (`__cco_list_new(sizeof(char *))`) and copies each argument string using `strdup()`. The caller owns the returned `string[]` array and every contained string. When the array variable exits scope, Cco's automatic cascade deallocator iterates through all array elements, calls `free()` on each non-NULL string, and then frees the array header and buffer via `__cco_free_arr()`.
2. **`program_name()`**: Duplicates `argv[0]` via `strdup()`. The caller owns the returned string, and it is automatically freed at scope exit by the standard string ownership handler.
3. **`arg_count()`**: Evaluates `(__cco_argc > 1) ? (__cco_argc - 1) : 0` directly with zero memory allocation.

---

## 3. Implementation Details

### A. Compiler Code Generation & Selective Prelude
- **`src/codegen.c`**:
  - Emitted `static int __cco_argc;` and `static char **__cco_argv;` in the file prelude.
  - Emitted `int main(int __cco_main_argc, char **__cco_main_argv)` for `main`.
  - Registered prelude chunks `get_args`, `get_arg_count`, and `get_program_name` in `scan_node_usage`.
  - Added expression type inference for `args` (`TY_STRING`, `is_array = true`), `program_name` (`TY_STRING`), and `arg_count` (`TY_INT`).
  - Added recursive ident lookup through `NODE_PROGRAM`, `NODE_FUNCTION`, and `NODE_METHOD` blocks in `find_ident_type_in_node` and `find_ident_class_in_node`.
  - Implemented string array element free cascade in `emit_releases` for `string[]` variables.

- **`src/scope_analysis.c`**:
  - Added `args` and `program_name` to `is_stdlib_heap_fn`.
  - Added `args` and `keys` to `is_expr_array_scope`.
  - Handled `NODE_FOR_EACH` in pass 1 block analysis to ensure loop-scoped variables are registered and freed every iteration.

- **`src/stdlib_prelude.h`**:
  - Added prelude chunks `get_args`, `get_arg_count`, and `get_program_name` with precise dependency tracking (`deps_get_args: list_new, arr_maybe_grow, arr_len_raw, arr_incr_len`).

### B. Test Harness Support for CLI Arguments
- **`tests/run_tests.sh`**:
  - Added support for optional `.args` sidecar files (e.g. `62_argv_basic.args`).
  - When present, the harness reads the arguments and passes them to both the execution run and the Valgrind leak-checking run. When absent, the test binary is invoked with no arguments, ensuring 100% backward compatibility.

---

## 4. Test Suite & Verification Results

### A. New Integration Tests
1. **`62_argv_basic.cco` + `62_argv_basic.args`**:
   - Invocation with arguments `"hello world 42"`.
   - Verified `arg_count() == 3`, `args() == ["hello", "world", "42"]`, for-each iteration, and leak-free scope-exit cleanup.
2. **`63_argv_empty.cco`**:
   - Invocation with 0 CLI arguments.
   - Verified `arg_count() == 0`, `len(args()) == 0`, empty loop execution, and 0 memory leaks.
3. **`64_program_name.cco`**:
   - Verified `program_name()` retrieves `"build/64_program_name"` and cleans up the returned string cleanly.

### B. Example Program
- **`examples/12_cli_args.cco`**: A full CLI utility demonstrating `program_name()`, `arg_count()`, `args()`, and loop processing.

### C. Test Results Summary
- **Unit Test Suites**: 4/4 passing (100% clean, 0 memory leaks).
- **Self-Hosted Lexer Diff Harness**: 87/87 corpus `.cco` files passing (byte-identical token streams).
- **Integration Test Suite**: 64/64 integration tests passing under Valgrind (`-pedantic-errors -std=c11`, 0 memory leaks).
