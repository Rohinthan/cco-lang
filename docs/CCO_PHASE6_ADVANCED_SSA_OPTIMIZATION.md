# Cco Compiler — Phase 6 Completion Report
## Advanced SSA Optimization: SCCP, GVN, CSE, Loop Analysis & SSA DCE

---

## Executive Summary

Phase 6 introduces a production-grade advanced optimization pipeline operating directly on Static Single Assignment (SSA) form in the Cco compiler. Building directly upon the Phase 5 SSA foundation, Phase 6 incorporates:

1. **Natural Loop Analysis & Preheader Construction (`ir_loop.c`)**: Back-edge detection via dominator trees, loop nesting hierarchy, loop depth calculation, and automated dedicated preheader block synthesis.
2. **Sparse Conditional Constant Propagation (SCCP) (`ir_ssa_opt.c`)**: Wegman & Zadeck's dual-worklist algorithm combining SSA value lattice propagation (`TOP` $\to$ `CONST` $\to$ `BOT`) with CFG edge executability, constant branch folding, unreachable block pruning, and dynamic phi cleanup.
3. **Global Value Numbering & Common Subexpression Elimination (GVN/CSE) (`ir_ssa_opt.c`)**: Scoped dominator-tree expression hashing, commutative operand canonicalization, and inter-block redundancy elimination for pure instructions.
4. **Conservative Loop Invariant Code Motion (LICM) (`ir_ssa_opt.c`)**: Identification of pure computations with loop-invariant operands and safe hoisting into dedicated loop preheaders.
5. **Improved SSA Dead Code Elimination (SSA DCE) (`ir_ssa_opt.c`)**: Fixed-point elimination of unused pure instructions and dead phi nodes with strict preservation of side effects.
6. **Safety & Determinism Guarantees**: Absolute preservation of memory operations, calls, allocas, and strict IEEE-754 double-precision floating-point semantics (no unsafe reassociations).

All 10 unit test suites run under Valgrind with **0 memory leaks and 0 errors**. End-to-end differential testing verifies bit-exact execution across `-O0`, `-O1`, `--ssa`, and the internal ELF64 static linker.

---

## File Manifest

### New Files

| File | Description |
|------|-------------|
| [`src/ir_loop.h`](file:///home/raccoon/cco-lang/src/ir_loop.h) | Natural loop detection API, loop hierarchy, preheader generation, loop dumping |
| [`src/ir_loop.c`](file:///home/raccoon/cco-lang/src/ir_loop.c) | Loop analysis implementation, back-edge analysis, loop block collection, preheader insertion |
| [`src/ir_ssa_opt.h`](file:///home/raccoon/cco-lang/src/ir_ssa_opt.h) | Advanced SSA optimization interface: SCCP, GVN/CSE, SSA DCE, LICM, pipeline metrics |
| [`src/ir_ssa_opt.c`](file:///home/raccoon/cco-lang/src/ir_ssa_opt.c) | Advanced SSA optimizer passes and driver (`ir_ssa_advanced_optimize_function`) |
| [`tests/unit/test_opt_ssa.c`](file:///home/raccoon/cco-lang/tests/unit/test_opt_ssa.c) | Comprehensive 13-part unit and differential test suite |
| [`docs/CCO_PHASE6_ADVANCED_SSA_OPTIMIZATION.md`](file:///home/raccoon/cco-lang/docs/CCO_PHASE6_ADVANCED_SSA_OPTIMIZATION.md) | This completion report |

### Modified Files

| File | Modifications |
|------|---------------|
| [`src/ir_ssa.h`](file:///home/raccoon/cco-lang/src/ir_ssa.h) | Added `dump_ssa_opt` to `IrSsaOptions` |
| [`src/ir_ssa.c`](file:///home/raccoon/cco-lang/src/ir_ssa.c) | Hooked `ir_ssa_advanced_optimize_function` into SSA pipeline before SSA deconstruction |
| [`src/ir_opt.c`](file:///home/raccoon/cco-lang/src/ir_opt.c) | Guarded non-SSA constant propagation with single-definition check (`def_count[id] == 1`) to preserve parallel copy correctness |
| [`src/main.c`](file:///home/raccoon/cco-lang/src/main.c) | Added `--dump-ssa-opt` and `--dump-loops` CLI flags and updated documentation |
| [`Makefile`](file:///home/raccoon/cco-lang/Makefile) | Added `ir_loop.c` and `ir_ssa_opt.c` to `SRC`; integrated `test_opt_ssa` into `unit_tests` |

---

## Pipeline Architecture

The compiler pipeline now integrates Phase 6 advanced SSA optimization between SSA construction and SSA destruction:

```
Cco Source
    ↓
Frontend (Lexer, Parser, AST)
    ↓
Semantic / Scope Analysis
    ↓
Cco IR (Three-Address Code)
    ↓
IR Verification
    ↓
[--ssa] SSA Construction (mem2reg)
    ├── Dominance Analysis & Dominance Frontiers
    ├── φ-node Placement (Iterated DF)
    └── SSA Renaming (Dominator Tree Walk)
    ↓
SSA Verification
    ↓
[--ssa -O1] Advanced SSA Optimization (Phase 6)
    ├── Natural Loop Analysis (Back-edge CFG walk, loop nesting)
    ├── Sparse Conditional Constant Propagation (SCCP)
    │   ├── Value Lattice (TOP -> CONST -> BOT)
    │   ├── CFG Executability Analysis
    │   ├── Constant Branch Folding & Dead Block Pruning
    │   └── Dynamic Phi Edge Cleanup
    ├── Dominator-Tree Scoped GVN & CSE
    │   ├── Commutative Operand Canonicalization
    │   └── Cross-block Pure Expression Elimination
    ├── Conservative Loop Invariant Code Motion (LICM)
    │   ├── Dedicated Preheader Synthesis
    │   └── Invariant Pure Arithmetic Hoisting
    └── Fixed-point SSA Dead Code Elimination (SSA DCE)
    ↓
SSA Verification
    ↓
SSA Destruction
    ├── Critical Edge Splitting
    └── Parallel Copy Sequentialization (Cycle-breaking)
    ↓
[-O1] IR Optimization Pipeline (Local folding, Non-SSA CFG simplify)
    ↓
Liveness Analysis
    ↓
Dual-Class Linear-Scan Register Allocation
    ↓
x86-64 Native Code Generation
    ↓
ELF64 Object File Generation
    ↓
Internal Static Linker / System Linker
    ↓
Standalone ELF64 Executable
```

---

## Detailed Component Design

### 1. Natural Loop Analysis (`ir_loop.h`, `ir_loop.c`)

Natural loops are defined mathematically using dominance relationships:
- An edge $(B, H)$ in the CFG is a **back-edge** if and only if $H \text{ dom } B$.
- $H$ is the **loop header**, and $B$ is the **loop latch**.
- The natural loop body consists of $H$ plus all basic blocks that can reach $B$ without going through $H$.

#### Key Features:
- **Loop Discovery**: Discovers all natural loops and computes loop depth and parent/child nesting relationships.
- **Dedicated Preheader Synthesis (`ir_loop_get_or_create_preheader`)**:
  - A dedicated preheader is a single non-loop predecessor block through which all external entries into the loop header must pass.
  - If the header has only one external predecessor and that edge is not critical, that predecessor serves as the preheader.
  - If multiple external predecessors exist or the edge is shared, a dedicated preheader block `loop.preheader.<N>` is created. All external edges and phi inputs are re-routed through it.

---

### 2. Sparse Conditional Constant Propagation (SCCP)

Implemented following Wegman & Zadeck's algorithm, SCCP optimizes constant values while simultaneously discovering unreachable control flow paths.

#### Three-Level Lattice:
- $\top$ (`LATTICE_TOP` / Unknown): Unvisited; may become any value.
- $C$ (`LATTICE_CONST` / Constant): Proved to have a specific compile-time constant value.
- $\bot$ (`LATTICE_BOT` / Overdefined): Known to vary or cannot be proved constant.

#### Dual Worklists:
1. `cfg_wl`: CFG edges that have become executable.
2. `ssa_wl`: SSA virtual registers whose lattice values have transitioned.

#### Lattice Propagation Rules:
- **Constants**: `const i64 / f64 / bool` $\to$ `LATTICE_CONST`.
- **Arithmetic / Logic**: Folded if all operands are `LATTICE_CONST`. If any operand is `LATTICE_BOT`, the result transitions to `LATTICE_BOT`.
- **Conditional Branches**:
  - If the condition is `LATTICE_CONST`, only the reachable destination edge is added to `cfg_wl`.
  - If the condition transitions to `LATTICE_BOT`, both branch edges are marked executable.
- **Phi Nodes**: Evaluated over executable incoming edges only:
  - If all executable incoming edges have the same constant $C$, the phi evaluates to $C$.
  - If conflicting constants or $\bot$ are seen, the phi transitions to $\bot$.
  - Unexecutable incoming edges are ignored during convergence.

#### Dead Code & CFG Cleanup:
- Blocks never reached by any executable edge are identified and pruned from the function.
- Branches whose condition was constant are rewritten as unconditional jumps (`br`).
- Phi nodes with unreachable predecessor edges are pruned or collapsed to single-input values.

---

### 3. Global Value Numbering & CSE (GVN/CSE)

GVN identifies identical computations across basic blocks and replaces redundant calculations with earlier results.

#### Canonicalization:
Commutative operations are canonicalized by ordering operands ($v_1 \le v_2$):
- Integer addition (`IR_OP_ADD`), multiplication (`IR_OP_MUL`)
- Equality comparisons (`IR_OP_EQ`, `IR_OP_NE`)
- Bitwise operations (`IR_OP_BAND`, `IR_OP_BOR`, `IR_OP_BXOR`)
- Floating-point addition (`IR_OP_FADD`) and multiplication (`IR_OP_FMUL`)

Non-commutative operations (`SUB`, `DIV`, `REM`, `LT`, `LE`, `GT`, `GE`, `FSUB`, `FDIV`) strictly preserve operand order.

#### Dominator-Tree Scoped Table:
A scoped hash table is populated during a pre-order traversal of the dominator tree. Because definitions dominate all uses in SSA form, an expression computed at node $D$ is valid for any dominated node $N$, allowing seamless cross-block CSE. Upon exiting a dominator subtree, expressions defined within that scope are popped.

---

### 4. Conservative Loop Invariant Code Motion (LICM)

LICM identifies computations inside natural loops whose values do not change across iterations and hoists them before the loop:
- Candidate instructions must be pure, side-effect-free arithmetic or comparison operations.
- All operands of the instruction must be loop-invariant (either constant or defined in a block outside the loop).
- The instruction is moved into the loop's dedicated preheader block, ensuring it executes exactly once before loop entry.
- Calls, memory loads, stores, allocas, and pointer operations are never hoisted.

---

### 5. Improved SSA Dead Code Elimination (SSA DCE)

SSA DCE cleans up unused instructions and phi nodes using a fixed-point algorithm:
- Side-effecting operations (calls, stores, prints, returns, branches) are always preserved.
- An instruction or phi node is deemed dead if its defined virtual register has a use count of zero.
- Removing a dead instruction decreases the use counts of its operands, which may trigger further dead code removal in subsequent iterations.

---

## Safety & Semantics Guarantees

### Memory Safety
Cco Phase 6 does not implement Memory SSA or alias analysis. Consequently:
- Memory loads (`IR_OP_LOAD`), stores (`IR_OP_STORE`), and allocations (`IR_OP_ALLOCA`) are treated as opaque, side-effecting operations.
- Memory operations are never hoisted by LICM or deduplicated across blocks by GVN.

### Floating-Point Correctness (IEEE-754)
- Floating-point arithmetic strictly follows IEEE-754 semantics.
- No unsafe reassociations (e.g. $(a + b) + c \neq a + (b + c)$) are performed.
- Division by zero, signed zero ($-0.0$), and NaN propagation are fully preserved.

### Parallel Copy & Post-SSA Soundness
During SSA deconstruction, phi nodes are lowered to parallel copies across predecessor blocks. In `src/ir_opt.c`, constant propagation was refined to require `def_count[id] == 1` before performing global constant substitution, preventing cross-block register assignment collisions after SSA destruction.

---

## Command-Line Interface

| Flag | Description |
|------|-------------|
| `--ssa` | Enable SSA construction, advanced SSA optimizations, and SSA destruction |
| `-O1` | Enable optimization pipeline (works in conjunction with `--ssa`) |
| `--dump-ssa` | Print the initial SSA IR immediately after construction and exit |
| `--dump-ssa-opt` | Print the optimized SSA IR after Phase 6 optimizations and exit |
| `--dump-loops` | Print loop analysis information (headers, latches, depth, body blocks) and exit |

### Usage Examples

```bash
# Compile and run with full SSA optimization and internal ELF64 linker
cco program.cco --ssa -O1 --use-internal-linker -o program --run

# Inspect SSA form before and after Phase 6 optimization
cco program.cco --dump-ssa
cco program.cco --dump-ssa-opt

# Inspect loop hierarchy and natural loops
cco program.cco --dump-loops
```

---

## Test & Verification Results

### Unit Test Suite (`tests/unit/test_opt_ssa.c`)

| Test Name | Focus | Result |
|-----------|-------|--------|
| `test_loop_analysis_simple` | Single natural loop, latch, header, loop body identification | ✅ PASS |
| `test_loop_analysis_nested` | Nested natural loops, loop depth, parent-child loop hierarchy | ✅ PASS |
| `test_loop_preheader_creation` | Dedicated preheader block creation and phi predecessor routing | ✅ PASS |
| `test_sccp_constant_folding` | Constant arithmetic and comparison propagation in straight-line code | ✅ PASS |
| `test_sccp_dead_branch_pruning` | Constant conditional branch pruning and dead block removal | ✅ PASS |
| `test_sccp_phi_propagation` | Constant propagation through phi nodes from single executable edge | ✅ PASS |
| `test_sccp_loop_convergence` | Convergence of loop-carried phi values to overdefined ($\bot$) | ✅ PASS |
| `test_gvn_simple` | Redundant pure computation deduplication within a block | ✅ PASS |
| `test_gvn_cross_block` | Scoped dominator-tree CSE across basic blocks | ✅ PASS |
| `test_gvn_commutative` | Canonicalization of commutative operands ($a + b \equiv b + a$) | ✅ PASS |
| `test_licm_simple` | Pure invariant arithmetic hoisting into dedicated preheader | ✅ PASS |
| `test_licm_memory_preserved` | Memory loads/stores preserved; non-hoisting verification | ✅ PASS |
| `test_ssa_dce` | Fixed-point unused computation and phi removal | ✅ PASS |

**Valgrind Memory Audit**: 0 errors, 0 memory leaks across 853 allocations and frees.

### Differential End-to-End Test Suite

Five differential test programs were verified across all execution modes:
1. `-O0` (Reference C11 backend)
2. `-O1` (Non-SSA optimized backend)
3. `--ssa -O1` (Phase 6 Advanced SSA optimized backend)
4. `--ssa -O1 --use-internal-linker` (Phase 6 SSA + Internal Static Linker)

**Results**: 100% bit-exact output parity across all modes.

### Full Regression Suite

| Test Suite | Pass Count | Status |
|------------|------------|--------|
| Unit Test Suites (10 suites under Valgrind) | 10 / 10 suites | ✅ PASS (0 leaks) |
| Self-hosted Lexer Tests | 135 / 135 tests | ✅ PASS |
| Self-hosted Parser & Codegen Tests | 4 / 4 tests | ✅ PASS (0 leaks) |
| Full Compiler Regression (incl. POSIX network) | 111 / 111 tests | ✅ PASS |

---

## Explicitly Deferred Items

In accordance with project scope boundaries, the following optimizations were intentionally excluded:
- **Memory SSA & Alias Analysis**: Preserved for future memory optimization passes.
- **Auto-Vectorization & SIMD**: Excluded to maintain backend simplicity and portability.
- **Interprocedural Analysis (IPA) & Function Inlining**: Scheduled for Phase 7.
- **Profile-Guided Optimization (PGO) & JIT Compilation**: Outside of static ELF64 scope.
