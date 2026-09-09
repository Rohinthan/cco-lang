# Self-Hosted Verification & Parity Report

**Official Release:** `v1.0.0-bootstrap`  
**Date:** September 2026  
**Language:** Cco (C--)  
**Repository:** [cco-lang](https://github.com/Rohinthan/cco-lang)  
**Status:** **100% Verified, 0 Leaks, Zero Errors Across All Test Corpi**

---

## 1. Executive Summary

This report documents the verification, bootstrap parity, and performance evaluation of the **Cco programming language** at milestone `v1.0.0-bootstrap`.

Cco is a memory-safe, statically typed systems programming language with compile-time affine single ownership that transpiles directly to portable standard C11. This milestone delivers:
1. **Self-Hosted Compiler Pipeline**: Complete self-hosted compiler implemented entirely in Cco source code (`selfhost/lexer_core.cco`, `selfhost/parser_core.cco`, `selfhost/typechecker_core.cco`, `selfhost/codegen_core.cco`).
2. **Unified Self-Hosted Binary Driver**: A standalone executable driver (`selfhost/cco.cco`) providing full CLI interface (`-o`, `--emit-c`, `--dump-tokens`).
3. **Standard Library Modularization**: Built-in primitives organized into clean importable modules (`std::net`, `std::math`, `std::string`, `std::io`) with canonical search path resolution.
4. **Zero-Leak Memory Safety Verification**: 100% clean Valgrind execution (0 bytes leaked in 0 blocks, 0 FD leaks) across all 303 verification targets.

---

## 2. Hardware & Toolchain Reproducibility Record

All tests, benchmarks, and bootstrap runs were executed directly on the following target environment:

| Component | Specification | Verification Command |
| :--- | :--- | :--- |
| **Commit Hash** | Official tag `v1.0.0-bootstrap` | `git rev-parse HEAD` |
| **C Compiler (GCC)** | `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0` | `gcc --version` |
| **C Compiler (Clang)** | `Ubuntu clang version 21.1.8 (6ubuntu1)` | `clang --version` |
| **Memory Analyzer** | `valgrind-3.26.0` | `valgrind --version` |
| **Operating System** | `Ubuntu Linux (kernel 7.0.0-30-generic #30-Ubuntu SMP PREEMPT_DYNAMIC x86_64)` | `uname -a` |
| **Processor (CPU)** | `AMD Ryzen 3 7320U with Radeon Graphics` (4 cores / 8 threads @ 2.40 GHz base) | `lscpu` |
| **Host Memory** | 15 GiB LPDDR5 | `free -h` |

---

## 3. Comprehensive Test Suite & Parity Metrics

The project undergoes rigorous multi-layer testing:

| Test Suite | Targets | Description | Pass Rate | Valgrind Leak Status |
| :--- | :---: | :--- | :---: | :---: |
| **Unit Tests** (`tests/unit/`) | **4 suites** | Lexer, parser, scope analysis, and hash map runtime under Valgrind | **4 / 4 (100%)** | **0 bytes in 0 blocks** |
| **Integration Corpus** (`tests/programs/`) | **101 tests** | 97 standalone programs + 4 multi-file package test suites | **101 / 101 (100%)** | **0 bytes in 0 blocks** |
| **Differential Lexer Corpus** (`tests/compare_lexers.sh`) | **127 files** | Differential test comparing C-native lexer vs self-hosted lexer byte-for-byte | **127 / 127 (100%)** | **0 bytes in 0 blocks** |
| **Network End-to-End Suite** (`tests/network/`) | **6 suites** | Real TCP socket suites testing concurrency, malformed data, buffer scaling | **6 / 6 (100%)** | **0 leaks, 0 FD leaks** |
| **Bootstrap Parity Target** (`make test_bootstrap`) | **1 target** | Target compiled by self-hosted pipeline compared to native compiler output | **1 / 1 (100%)** | **0 bytes in 0 blocks** |
| **ML Algorithms Corpus** (`cco-examples/algorithms/`) | **65 algorithms** | 65 machine learning algorithms across 12 mathematical domains | **65 / 65 (100%)** | **0 bytes in 0 blocks** |
| **Total Verification Targets** | **303** | **Grand total across compiler, runtime, stdlib, and algorithmic suites** | **303 / 303 (100%)** | **0 leaks across all 303** |

---

## 4. Compiler Diagnostic Corpus (Negative Testing)

The compiler is verified against **30 explicit negative error test cases**:
- **28 Compile-Time Rejection Tests (`_ERROR.cco`)**: All 28 rejected at compile-time with exact line, column, and diagnostic note match against gold snapshots (`tests/expected_output/*_stderr.txt`).
  - *Affine Ownership*: Use-after-move (`16`), double move (`17`), conditional branch move without join (`18`), return of borrowed parameter (`20`), move out of array index (`27`).
  - *Module System*: Circular module import (`31`), duplicate exported symbol collision (`32`).
  - *Type System & Interfaces*: Struct mutation without `mut` (`71`), interface method mismatch (`74`), class operator overloading rejection (`79`), type inference conflict (`94`), duplicate match enum arms (`97`).
- **2 Intentional Runtime Error Tests (`_RUNTIME_ERROR.cco`)**:
  - `45_pop_empty_RUNTIME_ERROR.cco`: Clean panic on popping from empty dynamic array.
  - `67_to_int_invalid_RUNTIME_ERROR.cco`: Clean panic on parsing malformed integer string.

---

## 5. Performance Evaluation & Benchmarks

Benchmarked on `AMD Ryzen 3 7320U` with 10 repetitions per workload comparing hand-written standard C11 (`gcc -O3 -Wall -Wextra -std=c11 -lm`) against Cco transpiled output:

| Workload | Metric / Scale | Hand-Written C Baseline | Cco Transpiled | Ratio ($T_{\\text{Cco}} / T_{\\text{C}}$) | Overhead |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Recursive Fibonacci** | $F(40)$ (zero heap, stack frames) | 208.69 ms $\\pm$ 2.51 ms | 225.17 ms $\\pm$ 9.25 ms | **1.0790x** | +7.90% |
| **Sieve of Eratosthenes** | $N = 10,000,000$ (heap arrays, strides) | 148.85 ms $\\pm$ 3.68 ms | 151.21 ms $\\pm$ 6.93 ms | **1.0158x** | +1.58% |
| **Sort (Bubble Sort)** | $N = 10,000$ integers (array indexing) | 75.36 ms $\\pm$ 0.53 ms | 75.72 ms $\\pm$ 1.53 ms | **1.0049x** | +0.49% |

Transpilation imposes **less than 2% overhead** for heap and array workloads, and ~7.9% for deep recursive call stacks.

---

## 6. Self-Hosted Compiler Architecture (`selfhost/`)

The self-hosted compiler pipeline is written 100% in Cco source code:

1. **`selfhost/lexer_core.cco`**: Handcrafted DFA lexer matching all Cco token kinds, strings, escapes, numeric literals, and coordinates.
2. **`selfhost/parser_core.cco` + `selfhost/ast.cco`**: Pratt operator-precedence expression parser and recursive descent statement parser.
3. **`selfhost/typechecker_core.cco`**: Scope resolution, symbol tables, and static type checking.
4. **`selfhost/codegen_core.cco`**: Emission of portable, clean standard C11 source code with automatic prelude insertion.
5. **`selfhost/cco.cco`**: Unified self-hosted CLI driver executable providing:
   ```bash
   cco <source.cco> [-o output.c] [--emit-c] [--dump-tokens]
   ```

---

## 7. Standard Library Architecture (`std/`)

Built-in capabilities are modularized into standard library packages:

- **`std::net`** (`std/net.cco`): High-level POSIX networking abstractions (`TcpServer`, `TcpClient`, `tcp_listen`, `tcp_accept`, `tcp_recv`, `tcp_send`, `tcp_close`, `sleep_millis`).
- **`std::math`** (`std/math.cco`): Numerical functions (`abs`, `min`, `max`, `clamp`, `min_float_val`, `max_float_val`, `clamp_float`, `rand_seed`, `rand_int`).
- **`std::string`** (`std/string.cco`): String manipulation (`string_concat`, `string_sub`, `string_char_at`, `to_str_int`, `to_str_float`, `to_str_bool`, `to_str_char`, `parse_int`, `parse_float`, `is_valid_int`, `is_valid_float`).
- **`std::io`** (`std/io.cco`): File system and console I/O (`file_read`, `file_write`, `io_read_line`).

The module resolver seamlessly supports both `import std::net;` and `import "std/net.cco";` from any working directory.
