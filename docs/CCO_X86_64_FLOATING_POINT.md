# Cco x86-64 Floating-Point and XMM Backend — Technical Specification & Architecture

## 1. Overview and Evolution

The Cco programming language compiler implements an independent, direct native code generation architecture translating verified Cco Intermediate Representation (IR) to GNU/AT&T x86-64 assembly.

```
Phase 1:
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → C11 Codegen → GCC/Clang → Executable

Phase 2A (Stack-Centric Native Foundation):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → Frame Lowering → GNU/AT&T Assembly (.s) → Host Linker → Linux ELF64

Phase 2B (Register Allocation & Integer Code Quality):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → CFG Liveness Analysis → Linear-Scan Register Allocation → Frame Spilling & Lowering → Peephole Optimization → GNU/AT&T Assembly (.s) → Host Linker → Linux ELF64

Phase 2C (Floating-Point and XMM Register Backend):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → Type-Aware CFG Liveness Analysis → Dual-Class Linear-Scan Register Allocation (GPR + XMM) → Frame Spilling & Lowering → IEEE 754 SSE2 Code Generation → Peephole Optimization → GNU/AT&T Assembly (.s) → Host Linker → Linux ELF64
```

In **Phase 2C**, the native x86-64 backend is extended from an integer/pointer-only code generator to fully support 64-bit IEEE 754 floating-point operations (`f64`, Cco type `float`) using the SSE2 instruction set and XMM registers (`%xmm0`–`%xmm15`).

Key additions in Phase 2C include:
1. **Type-Aware Dual-Class Register Allocation**: Extends linear scan to handle disjoint physical register classes (`REG_CLASS_INT` and `REG_CLASS_FLOAT`).
2. **System V AMD64 Floating-Point ABI**: Implements argument passing in `%xmm0`–`%xmm7`, return values in `%xmm0`, independent consumption of integer and vector registers, and vector register count in `%al` for variadic calls (`printf`).
3. **Aliasing-Safe Call Argument Staging**: A 128-byte stack staging area prevents register clobbering when expression values reside in ABI argument registers.
4. **IEEE 754 SSE2 Lowering**: Scalar double-precision arithmetic (`addsd`, `subsd`, `mulsd`, `divsd`), exact sign-bit negation via 16-byte aligned bitmask in `.rodata`, and unordered comparisons via `ucomisd` with full NaN and signed-zero handling.
5. **Exact Constant Pool**: Floating-point literals are deposited in `.rodata` as exact 64-bit hexadecimal integer representations (`.quad 0x%016lx`), preventing compiler-host formatting drifts.

---

## 2. System V AMD64 ABI Floating-Point Specification

The native backend complies strictly with the System V AMD64 ABI for floating-point operations on Linux x86-64.

### 2.1 Floating-Point Parameter Passing
* Up to 8 floating-point arguments are passed in vector registers:
  1. `%xmm0`
  2. `%xmm1`
  3. `%xmm2`
  4. `%xmm3`
  5. `%xmm4`
  6. `%xmm5`
  7. `%xmm6`
  8. `%xmm7`
* Floating-point arguments consume XMM registers **independently** of integer and pointer arguments. For example, in `fn mix(a: int, x: float, b: int, y: float)`:
  - `a` is passed in `%rdi` (Integer Argument 0)
  - `x` is passed in `%xmm0` (Float Argument 0)
  - `b` is passed in `%rsi` (Integer Argument 1)
  - `y` is passed in `%xmm1` (Float Argument 1)

### 2.2 Floating-Point Return Values
* Functions returning `float` (`f64`) deliver their return value in `%xmm0`.

### 2.3 Variadic Call Convention (`%al`)
* The System V ABI requires that for variable-argument functions (such as `printf`), the `%al` register must be set to the number of XMM registers used to pass arguments (0 to 8).
* When invoking `printf` for floating-point values:
  ```gas
  movsd  %xmm_val, %xmm0
  leaq   .LC_fmt_float(%rip), %rdi
  movb   $1, %al
  call   printf
  ```
* For non-variadic function calls, `%al` is also set to the number of vector registers passed, ensuring total compatibility.

### 2.4 Caller-Saved Nature of XMM Registers
* Under the System V AMD64 ABI, **all XMM registers (`%xmm0` through `%xmm15`) are caller-saved** (volatile).
* No XMM registers are preserved by callees across function calls. Any live floating-point value spanning a function call must be preserved by the caller in a designated stack spill slot.

---

## 3. Type-Aware Register Allocation (`src/x86_64_regalloc.c`)

### 3.1 Register Classes
The register allocator explicitly distinguishes between general-purpose and floating-point registers:
```c
typedef enum RegClass {
    REG_CLASS_INT,
    REG_CLASS_FLOAT
} RegClass;
```
Each `LiveInterval` records its `RegClass` and type. The linear-scan allocator enforces complete class separation: an integer virtual register is never assigned an XMM register, and a float virtual register is never assigned a GPR.

### 3.2 Physical Register Pools
* **Integer Caller-Saved Pool**: `%r10`, `%r11` (disjoint from ABI argument registers).
* **Integer Callee-Saved Pool**: `%rbx`, `%r12`, `%r13`, `%r14`, `%r15`.
* **Floating-Point Allocatable Pool** (14 registers):
  - Non-argument registers: `%xmm8`, `%xmm9`, `%xmm10`, `%xmm11`, `%xmm12`, `%xmm13` (allocated first).
  - Argument registers: `%xmm0`, `%xmm1`, `%xmm2`, `%xmm3`, `%xmm4`, `%xmm5`, `%xmm6`, `%xmm7` (allocated second).
* **Reserved Floating-Point Scratch**:
  - `%xmm14`: Primary comparison scratch operand.
  - `%xmm15`: Secondary comparison scratch, arithmetic load scratch, and negation mask scratch.

### 3.3 Linear-Scan Allocation with Class-Specific Spilling
When the allocator processes an interval `curr`:
1. It queries the appropriate register pool (`FLOAT_POOL` for `REG_CLASS_FLOAT`, GPR pools for `REG_CLASS_INT`).
2. If all physical registers in the pool are busy, it searches the `active` list for the victim interval *of the same register class* whose end point is furthest in the future.
3. If `victim->end > curr->end`, the victim is evicted, its physical register is assigned to `curr`, and `victim` is marked as spilled. Otherwise, `curr` is spilled.
4. Float spills are assigned dedicated 8-byte stack slots in the function frame.

### 3.4 Call-Crossing Preservation
Because all XMM registers are caller-saved:
* Any float virtual register whose live interval crosses a function call has its spill slot allocated in the frame.
* Upon definition (`emit_store_result`), its value is written through to its spill slot `spill_offset(%rbp)`.
* Immediately following the `call` instruction, `reload_caller_saved_after_call` reloads the value from the stack back into its assigned physical register.

---

## 4. Floating-Point Instruction Lowering (`src/x86_64_codegen.c`)

### 4.1 IEEE 754 Constant Pool
Float literals are stored in the `.rodata` section using exact 64-bit IEEE 754 bit representations:
```gas
    .section .rodata
.LC_float_0:
    .quad 0x400921fb54442d18  # 3.141592653589793
.LC_float_1:
    .quad 0x4000000000000000  # 2.0
```
This guarantees bit-for-bit exactness regardless of host compiler locale or floating-point printing subtleties.

### 4.2 Arithmetic Lowering
* **Addition**: `addsd %src, %dst`
* **Subtraction**: `subsd %src, %dst`
* **Multiplication**: `mulsd %src, %dst`
* **Division**: `divsd %src, %dst`
* To prevent register clobbering when `inst->rhs` shares `target`, the right-hand operand is evaluated and loaded into scratch `%xmm15` before `target` is overwritten with `inst->lhs`.

### 4.3 Sign-Bit Negation
Floating-point negation in IEEE 754 flips the sign bit (bit 63) while leaving all other bits intact. This is implemented via bitwise XOR with a 16-byte aligned mask:
```gas
    .section .rodata
    .align 16
.LC_neg_float_mask:
    .quad 0x8000000000000000
    .quad 0

    .text
    # In function body:
    movsd .LC_neg_float_mask(%rip), %xmm15
    xorpd %xmm15, %target
```
Using `movsd` into scratch `%xmm15` followed by register-to-register `xorpd` completely avoids SSE unaligned memory faults while ensuring exact negation for all values, including `+0.0` $\rightarrow$ `-0.0` and `-0.0` $\rightarrow$ `+0.0`.

### 4.4 Unordered Comparisons & NaN Handling
Floating-point comparisons use `ucomisd`, which sets x86 EFLAGS:
* **Unordered** (NaN operand): `ZF=1`, `PF=1`, `CF=1`
* **Greater Than**: `ZF=0`, `PF=0`, `CF=0`
* **Less Than**: `ZF=0`, `PF=0`, `CF=1`
* **Equal**: `ZF=1`, `PF=0`, `CF=0`

The native backend generates exact IEEE 754 boolean results using conditional set instructions:
| Operation | Assembly Sequence | Rationale |
|---|---|---|
| `==` | `setnp %al`<br>`sete %dl`<br>`andb %dl, %al` | True only if `ZF=1` and `PF=0` (equal and not unordered/NaN). |
| `!=` | `setp %al`<br>`setne %dl`<br>`orb %dl, %al` | True if `PF=1` (unordered/NaN) or `ZF=0` (not equal). |
| `<` | `setb %al`<br>`setnp %dl`<br>`andb %dl, %al` | True only if `CF=1` and `PF=0` (strictly less and not NaN). |
| `<=` | `setbe %al`<br>`setnp %dl`<br>`andb %dl, %al` | True only if (`CF=1` or `ZF=1`) and `PF=0`. |
| `>` | `seta %al` | `seta` checks `CF=0` and `ZF=0`. If NaN, `CF=1`, so `seta` is automatically 0. |
| `>=` | `setae %al`<br>`setnp %dl`<br>`andb %dl, %al` | True only if `CF=0` and `PF=0`. |

Signed zeros (`+0.0` and `-0.0`) compare equal under `ucomisd`, satisfying the IEEE 754 requirement that `+0.0 == -0.0` evaluates to true.

### 4.5 Aliasing-Safe Call Argument Staging
When setting up arguments for an `IR_OP_CALL`:
Expression values evaluated before the call may already reside in argument registers (e.g. `%xmm0` or `%rdi`). To eliminate parallel-copy cycles and argument clobbering:
1. `subq $128, %rsp` reserves a 16-byte aligned temporary staging area.
2. All argument expressions are evaluated and written to the staging slots (`%xmm15` $\rightarrow$ `0..56(%rsp)` for floats; `%rax` $\rightarrow$ `64..120(%rsp)` for integers).
3. Once all arguments are safely captured on the stack, they are loaded into their respective ABI registers (`%xmm0`–`%xmm7` and `%rdi`–`%r9`).
4. `addq $128, %rsp` restores the stack pointer to exact 16-byte alignment prior to `call`.
5. The return value is harvested from `%xmm0` (float) or `%rax` (integer).

---

## 5. Stack Frame Architecture (`src/x86_64_target.c`)

The stack frame manages 8-byte aligned slots for:
1. Spilled `f64` virtual registers.
2. Call-crossing caller-saved XMM registers.
3. Spilled integer virtual registers and callee-saved registers.
4. Function call parameters spilled from homing registers.

The total frame size is rounded up to a multiple of 16 bytes, preserving the System V AMD64 ABI 16-byte stack alignment invariant across all calls.

---

## 6. Verification and Parity

Phase 2C verification includes exhaustive testing across the entire compiler stack:

1. **Unit and Differential Test Suite (`tests/unit/test_x86_64.c`)**:
   - 26 tests covering integer regalloc, control flow, recursion, deep stack frames, and 8 dedicated Phase 2C tests:
     * `test_float_arithmetic`: `+`, `-`, `*`, `/`, and unary `neg`.
     * `test_float_comparisons`: All 6 relational operators, signed zeros (`+0.0 == -0.0`), and NaN unordered behavior.
     * `test_float_constants_and_variables`: Exact `.rodata` pool loading and variable reassignments.
     * `test_float_abi_parameters`: Full 8-register float argument passing and mixed integer/float parameter passing.
     * `test_float_recursion_and_nesting`: Recursive float functions and nested calls.
     * `test_float_loop_carried`: Float accumulators and convergence loops.
     * `test_float_register_pressure_spilling`: 16 concurrent live floats exceeding the 14-register XMM pool, verifying spill slot allocation and reloading.
     * `test_float_differential_parity`: Bit-for-bit differential comparison against the reference C11 backend on Newton-Raphson square root approximation.
2. **Valgrind Memcheck**:
   - 0 errors, 0 memory leaks across all 6 unit test suites (`make unit_tests`).
3. **Integration Suite (`tests/run_tests.sh`)**:
   - 111 integration and POSIX networking tests passing with 0 leaks and 0 FD leaks.
4. **Self-Hosted Comparison & Bootstrap (`make test_selfhost`, `make test_bootstrap`)**:
   - 134/134 self-hosted lexer comparison tests passing with exact token matches.
   - Self-hosted parser, typechecker, and codegen pipelines fully validated with 100% bootstrap parity under Valgrind.
