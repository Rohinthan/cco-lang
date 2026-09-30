# Cco x86-64 Native Backend — Technical Specification & Architecture

## 1. Overview and Evolution

The Cco programming language compiler implements an independent, direct native code generation architecture translating verified Cco Intermediate Representation (IR) to GNU/AT&T x86-64 assembly:

```
Phase 1:
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → C11 Codegen → GCC/Clang → Executable

Phase 2A (Stack-Centric Native Foundation):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → Frame Lowering → GNU/AT&T Assembly (.s) → Host Linker → Linux ELF64

Phase 2B (Register Allocation & Code Quality):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → CFG Liveness Analysis → Linear-Scan Register Allocation → Frame Spilling & Lowering → Peephole Optimization → GNU/AT&T Assembly (.s) → Host Linker → Linux ELF64
```

In **Phase 2B**, the backend replaces the naive stack-centric virtual-register approach with:
1. **Backwards Dataflow Liveness Analysis**: Computes live ranges, basic-block entry/exit sets (`live_in`, `live_out`), and function-call intersection points.
2. **Linear-Scan Register Allocation**: Implements Poletto & Sarkar (1999) linear-scan allocation over physical general-purpose registers, differentiating caller-saved scratch from callee-saved preserved registers.
3. **Selective Spilling**: Stack spill slots are allocated *only* when physical registers are exhausted or when values cross function calls in caller-saved registers.
4. **Deterministic Peephole Optimizer**: Eliminates redundant register-to-register moves (`movl %reg, %reg`), redundant store-then-load sequences, and converts constant zero initialization (`movl $0, %reg` $\rightarrow$ `xorl %reg, %reg`).

The reference C11 backend (`cco prog.cco -o prog`) and Phase 1 IR-to-C11 backend (`cco prog.cco --use-ir -o prog`) remain fully functional. The native backend is selectable via `--use-native`, assembly can be printed with `--emit-asm`, and register allocation intervals can be inspected via `--dump-regalloc`.

---

## 2. Target Environment & ABI Compliance

The backend implements the standard **System V AMD64 ABI** required on x86-64 Unix/Linux platforms.

### 2.1 Integer and Pointer Parameter Registers
The first 6 integer or pointer arguments are passed in dedicated 64-bit hardware registers:
1. `%rdi` (`%edi` for 32-bit values)
2. `%rsi` (`%esi` for 32-bit values)
3. `%rdx` (`%edx` for 32-bit values)
4. `%rcx` (`%ecx` for 32-bit values)
5. `%r8`  (`%r8d` for 32-bit values)
6. `%r9`  (`%r9d` for 32-bit values)

Arguments beyond the 6th are passed on the stack in right-to-left order, accessible at positive offsets from the frame pointer (`16(%rbp)`, `24(%rbp)`, etc.).

### 2.2 Return Values
* 32-bit integers, booleans, and characters: Returned in `%eax`.
* 64-bit integers and pointers: Returned in `%rax`.

### 2.3 Register Classes & Allocation Strategy
To avoid argument-register clobbering cycles and minimize call-preservation overhead:
* **Allocatable Caller-Saved Scratch**: `%r10`, `%r11`.
  These registers are never used as argument registers in the System V AMD64 ABI. They are allocated to short-lived expressions and temporaries. If a value in `%r10` or `%r11` must cross a function call, it is spilled to a stack slot prior to the call and reloaded immediately afterwards.
* **Allocatable Callee-Saved Preserved**: `%rbx`, `%r12`, `%r13`, `%r14`, `%r15`.
  These registers are preserved across function calls by the callee. Values that span one or more `call` instructions are prioritized for allocation into these callee-saved registers, eliminating save/reload traffic around calls.
* **Reserved Registers**:
  - `%rsp`, `%rbp`: Stack and frame pointers.
  - `%rax`: Dedicated for function return values, multiplication/division, and printf vector count (`%al = 0`).
  - `%rdx`: Dedicated for division remainder and secondary scratch.

---

## 3. Liveness Analysis & Linear-Scan Allocator (`src/x86_64_regalloc.c`)

### 3.1 Instruction Numbering
Instructions are indexed sequentially and deterministically from $0$ to $N - 1$ across all basic blocks in layout order. Each call, branch, arithmetic operation, or memory access receives a unique integer position.

### 3.2 Basic-Block Dataflow Equations
For each basic block $B$:
$$\text{live\_out}[B] = \bigcup_{S \in \text{succ}(B)} \text{live\_in}[S]$$
$$\text{live\_in}[B] = \text{use}[B] \cup (\text{live\_out}[B] \setminus \text{def}[B])$$

The analysis iterates backwards over the control-flow graph until the live sets reach a fixed point. This ensures that loops, nested conditionals, and loop-carried variables (accumulators and counters) have correct, unbroken live ranges.

### 3.3 Live Interval Construction
For each virtual register $v$:
* `start`: Index of the instruction that defines $v$.
* `end`: Maximum instruction index across all uses of $v$, extended across basic blocks if $v \in \text{live\_out}[B]$.
* `crosses_call`: Set to `true` if any `IR_OP_CALL`, `IR_OP_PRINT`, or `IR_OP_FREE` instruction index $C$ falls strictly within $(start, end)$.

### 3.4 Linear-Scan Algorithm
1. Intervals are sorted in ascending order of `start` position (with deterministic tie-breaking by virtual register ID).
2. An active list of allocated intervals is maintained, sorted by `end` position.
3. For each incoming interval $i$:
   * Expire all intervals in `active` whose `end < i.start`, returning their physical registers to the free pool.
   * If a free register is available:
     - If $i.\text{crosses\_call}$ is true, prefer callee-saved registers (`%rbx`, `%r12`..`%r15`).
     - If $i.\text{crosses\_call}$ is false, prefer caller-saved registers (`%r10`, `%r11`).
     - Assign the chosen register and add $i$ to `active`.
   * If no physical register is available:
     - Compare $i$ against the active interval with the latest end point (`active.last`).
     - If `active.last.end > i.end`, spill `active.last`, assign its register to $i$, and record a spill slot for `active.last`.
     - Otherwise, spill $i$ directly to a stack slot.

---

## 4. Stack Frame Architecture (`src/x86_64_target.c`)

The Phase 2B stack frame layout dynamically differentiates local variable allocas from actual spills:

```
Higher Addresses
+------------------------------------+
| Incoming Stack Args (args 7+)      |  [16(%rbp), 24(%rbp), ...]
+------------------------------------+
| Return Address (pushed by call)    |  [8(%rbp)]
+------------------------------------+
| Old Base Pointer (pushq %rbp)      |  [0(%rbp)]  <-- %rbp points here
+------------------------------------+
| Saved Callee-Saved Registers       |  [-8(%rbp), -16(%rbp), ...]
| (e.g. %rbx, %r12, %r13, ...)       |  (only those actually used by the function)
+------------------------------------+
| Parameter Home Slots               |  [-(K*8 + 8)(%rbp), ...]
+------------------------------------+
| Local Variables (allocas)          |
+------------------------------------+
| Spill Slots (spilled regs only)    |  (0 slots if all registers fit!)
+------------------------------------+
| 16-Byte Alignment Padding          |
+------------------------------------+  <-- %rsp points here
Lower Addresses
```

### System V 16-Byte Alignment with Pushed Registers
Let $K$ be the number of callee-saved registers pushed during the prologue:
$$\text{TotalNeeded} = K \times 8 + \text{RawLocalsAndSpills}$$
$$\text{TotalAligned} = (\text{TotalNeeded} + 15) \ \& \ \sim 15$$
$$\text{FrameSize} = \text{TotalAligned} - K \times 8$$

Since caller return address is 8 bytes and `pushq %rbp` is 8 bytes, total adjustment before any subsequent `call` is:
$$8 + 8 + K \times 8 + \text{FrameSize} = 16 + \text{TotalAligned} \equiv 0 \pmod{16}$$
This mathematically guarantees 16-byte stack alignment across all call instructions regardless of the number of callee-saved registers used.

---

## 5. Code Generation & Peephole Optimization (`src/x86_64_codegen.c`)

### 5.1 Register-to-Register Execution
When operands are allocated to physical registers, arithmetic instructions operate directly between hardware registers:
```gas
# Phase 2A (Stack-Centric):
movl    -24(%rbp), %eax
movl    -32(%rbp), %edx
addl    %edx, %eax
movl    %eax, -40(%rbp)

# Phase 2B (Register-Allocated):
movl    %r10d, %ecx
addl    %r11d, %ecx
```
Memory traffic is completely eliminated for non-spilled expressions.

### 5.2 Deterministic Peephole Transformations
The post-codegen peephole pass performs safe, deterministic linear rewrites:
1. **Self-Move Elimination**:
   `movl %reg, %reg` and `movq %reg, %reg` are detected and deleted.
2. **Immediate Zero Constant Optimization**:
   `movl $0, %reg` is transformed to `xorl %reg, %reg`.
3. **Store-Then-Load Redundancy Elimination**:
   `movl %reg, offset(%rbp)` immediately followed by `movl offset(%rbp), %reg` has the redundant load removed.

---

## 6. Verification and Test Matrix

The test suite validates both correctness and code quality under Valgrind Memcheck:

| Test Name | Feature Verified | Expected Result | Valgrind Status |
|---|---|---|---|
| `test_return_constant` | Basic function return | Exit code 42 | 0 errors, 0 leaks |
| `test_arithmetic_operations` | Arithmetic lowering (`+`, `-`, `*`, `/`, `%`, `neg`) | Exact math | 0 errors, 0 leaks |
| `test_variables_and_assignment` | Local variable mutation & scope | Accurate values | 0 errors, 0 leaks |
| `test_comparisons` | Conditional setcc (`==`, `!=`, `<`, `<=`, `>`, `>=`) | Truth values | 0 errors, 0 leaks |
| `test_control_flow_branches_and_loops` | `if`/`else`, `while`, and `for` control flow | Branch parity | 0 errors, 0 leaks |
| `test_functions_and_multiple_parameters` | Inter-function calls & ABI registers | Correct dispatch | 0 errors, 0 leaks |
| `test_recursive_function` | Stack frame isolation on recursion (`fib(7)`) | 13 | 0 errors, 0 leaks |
| `test_stack_behavior_deep_locals` | Frame padding under 8 deep locals | 36 | 0 errors, 0 leaks |
| `test_differential_parity_with_c11_backend` | Collatz sequence (27 steps to 111) vs C11 | Bit-for-bit match | 0 errors, 0 leaks |
| `test_regalloc_reuse` | Register reuse over short-lived subexpressions | 144 | 0 errors, 0 leaks |
| `test_regalloc_pressure` | 16 simultaneously live values (forced spilling) | 136 | 0 errors, 0 leaks |
| `test_regalloc_call_preservation` | Values live across function call boundaries | 65 | 0 errors, 0 leaks |
| `test_regalloc_multiple_calls` | Multiple sequential calls with persistent values | 121 | 0 errors, 0 leaks |
| `test_regalloc_loop_carried` | Loop accumulator and counter in registers | 110 | 0 errors, 0 leaks |
| `test_regalloc_nested_branches` | Merged values across nested branch blocks | 84 | 0 errors, 0 leaks |
| `test_regalloc_recursion` | Recursive Fibonacci (`fib(10)`) | 55 | 0 errors, 0 leaks |
| `test_regalloc_many_params` | 6 System V ABI parameter passing | 21 | 0 errors, 0 leaks |
| `test_regalloc_large_locals` | 20 local variables with cross-arithmetic | 42 | 0 errors, 0 leaks |

---

## 7. Limitations & Recommendations for Phase 2C

### Current Limitations:
1. **Scalar Types Only**: Floating point (`f64`) operations currently use C11 or are unlowered in native backend.
2. **Aggregates and Struct Return**: Returning large structs by value exceeding 16 bytes via hidden pointer is deferred.
3. **Assembly File Output**: Emits `.s` files and invokes system assembler `gcc -no-pie` rather than direct ELF64 binary object emission.

### Recommendations for Phase 2C:
1. **Floating-Point SSE Support**:
   - Introduce XMM register classes (`%xmm0`–`%xmm15`).
   - Implement scalar IEEE 754 operations (`movsd`, `addsd`, `subsd`, `mulsd`, `divsd`, `ucomisd`).
   - Pass floating-point parameters per System V AMD64 ABI in `%xmm0`–`%xmm7`.
2. **Direct ELF64 Object Emission (Phase 2D)**:
   - Implement direct machine-code binary emission (ELF header, section header table, text/data relocations) to remove dependency on external assembler.
