# Cco v14.0 Implementation Report: Input, Number Parsing, and Random Numbers

**Author**: Antigravity  
**Version**: v14.0  
**Status**: Completed & Verified (69/69 Integration Tests Passing, 92/92 Lexer Comparison Files Passing, 0 Leaks)

---

## 1. Executive Summary

Cco v14.0 adds seven built-in standard library functions supporting standard input reading, robust number parsing with safe guard predicates, and deterministic pseudo-random number generation:
- `read_line() -> string`: Reads a line from `stdin` with the trailing newline stripped, returning an empty string on EOF.
- `to_int(s: string) -> int`: Parses a string to an integer; exits with a formatted runtime error if invalid.
- `to_float(s: string) -> float`: Parses a string to a float; exits with a formatted runtime error if invalid.
- `is_int(s: string) -> bool`: Returns `true` if `s` is a valid integer without risk of runtime fatal errors.
- `is_float(s: string) -> bool`: Returns `true` if `s` is a valid float without risk of runtime fatal errors.
- `random_int(min: int, max: int) -> int`: Returns an unbiased random integer in `[min, max]` inclusive.
- `random_seed(seed: int) -> void`: Deterministically seeds the random number generator.

---

## 2. Architecture & Design

### A. Shared Numeric Parsing Pipeline
To prevent semantic divergence between safe validators (`is_int`/`is_float`) and conversion parsers (`to_int`/`to_float`), both pairs share common internal static helpers:
- `__cco_try_parse_int(const char *s, long *out_val) -> bool`
- `__cco_try_parse_float(const char *s, double *out_val) -> bool`

These functions trim leading and trailing whitespace, parse the numeric literal via `strtol`/`strtod`, and verify that the entire string was consumed (`*endptr == '\0'`) without errno overflow.
- `is_int(s)` and `is_float(s)` invoke the try-parse routine with `out_val = NULL` and return the boolean result.
- `to_int(s)` and `to_float(s)` invoke the try-parse routine, writing to the output variable on success and printing `"Runtime Error: '%s' is not a valid integer\n"` (or float) to `stderr` with `exit(1)` on failure.

### B. Standard Input Line Reading (`read_line`)
`read_line()` is implemented using standard ISO C11 `fgetc()` with dynamic heap buffer growth:
- Buffers are initially allocated at 128 bytes and doubled as needed (`realloc`).
- Trailing `\n` and `\r\n` line endings are stripped.
- EOF with no characters read returns an allocated empty string (`""`), allowing callers to detect end-of-input naturally.
- The returned string is registered in `scope_analysis.c` as a heap-allocated string and automatically freed at scope exit via `free()`.

### C. Deterministic Random Numbers (`random_int`, `random_seed`)
- `random_seed(seed)` sets the seed via `srand((unsigned int)seed)` and sets `__cco_rand_seeded = 1`.
- `random_int(min, max)` lazily auto-seeds with `srand((unsigned int)time(NULL))` if unseeded, and eliminates modulo bias using rejection sampling against `RAND_MAX - (RAND_MAX % range)`.

---

## 3. Strict ISO C11 Portability

Generated C code maintains **100% ISO C11 conformance** (`-std=c11 -pedantic-errors`):
- `read_line()` deliberately avoids POSIX-only `getline()`, relying solely on ISO C11 `fgetc`, `malloc`, and `realloc`.
- Generated code compiles and runs across Windows/MSVC, Linux, macOS, and BSD without POSIX header dependencies.

---

## 4. Test Harness Enhancements (`tests/run_tests.sh`)

- **`.stdin` Sidecar Convention**: Added support for optional `.stdin` fixture files (e.g. `tests/programs/65_read_line_basic.stdin`).
- Piped to test binary stdout execution and Valgrind memory leak checking.
- Seamlessly handles simultaneous `.args` and `.stdin` configurations.

---

## 5. Verification Matrix

- **Unit Test Suites**: 4/4 passing (0 memory leaks).
- **Self-Hosted Lexer Comparison Harness**: 92/92 `.cco` files passing with 100% byte-identical token streams.
- **Integration Test Suite**: 69/69 integration tests passing under Valgrind (Diff clean + 0 Leaks).
- **Example Programs**: `examples/13_number_guess.cco` verified clean with 0 leaks under Valgrind.
