# Cco Compiler — Phase 5 Completion Report
## SSA-Based IR, Dominance Analysis & Advanced Optimization Infrastructure

---

## What Was Built

Phase 5 delivers a complete SSA (Static Single Assignment) infrastructure layer on top of the existing Cco IR, without breaking any existing pipeline.

### New Source Files

| File | Purpose |
|------|---------|
| [`src/ir_dominance.h`](file:///home/raccoon/cco-lang/src/ir_dominance.h) | Dominance analysis API |
| [`src/ir_dominance.c`](file:///home/raccoon/cco-lang/src/ir_dominance.c) | Iterative dominator computation, idom, DF |
| [`src/ir_ssa.h`](file:///home/raccoon/cco-lang/src/ir_ssa.h) | SSA pipeline API |
| [`src/ir_ssa.c`](file:///home/raccoon/cco-lang/src/ir_ssa.c) | SSA construction, verification, optimization, destruction |
| [`tests/unit/test_ssa.c`](file:///home/raccoon/cco-lang/tests/unit/test_ssa.c) | 13-test SSA unit suite |
| [`docs/CCO_PHASE5_SSA_AND_ANALYSIS.md`](file:///home/raccoon/cco-lang/docs/CCO_PHASE5_SSA_AND_ANALYSIS.md) | This document |

### Modified Files

| File | Change |
|------|--------|
| [`src/ir.h`](file:///home/raccoon/cco-lang/src/ir.h) | Added `IR_OP_PHI`, phi fields to `IrInstruction`, `ir_emit_phi`, `ir_phi_add_incoming` |
| [`src/ir.c`](file:///home/raccoon/cco-lang/src/ir.c) | Implemented phi instruction emission |
| [`src/ir_verify.c`](file:///home/raccoon/cco-lang/src/ir_verify.c) | Phi placement check (phi must precede non-phi in a block) |
| [`src/ir_print.c`](file:///home/raccoon/cco-lang/src/ir_print.c) | Added `ir_dump_function`, phi printing |
| [`src/main.c`](file:///home/raccoon/cco-lang/src/main.c) | `--ssa`, `--dump-ssa`, `--help`/`-h` flags; updated all `build_ir_module` call sites |
| [`Makefile`](file:///home/raccoon/cco-lang/Makefile) | Added `ir_dominance.c`, `ir_ssa.c` to `SRC`; added `test_ssa` target |

---

## Architecture

```
Cco Source
    ↓
Frontend (Lexer / Parser / AST)
    ↓
Semantic / Scope Analysis
    ↓
Cco IR
    ↓
IR Verification
    ↓
[--ssa] SSA Construction (mem2reg)
    ├── CFG Dominance Analysis
    ├── Dominance Frontier Computation
    ├── φ-node Placement (iterated DF)
    └── SSA Renaming (dominator tree walk)
    ↓
SSA Verification
    ↓
[--ssa -O1] SSA Optimizations
    ├── Trivial φ Folding
    ├── SSA Constant Propagation
    └── SSA Dead Code Elimination
    ↓
SSA Destruction
    ├── Critical Edge Splitting
    └── Parallel Copy Sequentialization (cycle-breaking)
    ↓
[-O1] IR Optimization Pipeline (constant folding, DCE, …)
    ↓
Liveness Analysis
    ↓
Dual-Class Linear-Scan Register Allocation
    ↓
x86-64 Backend
    ↓
ELF64 Object
    ↓
Internal Linker
    ↓
Standalone Executable
```

---

## Dominance Analysis

### Algorithm
Iterative fixed-point algorithm (Cooper et al.) over a BFS reverse-postorder traversal.

### Queries Supported

```c
IrDomInfo *dom = ir_dominance_compute(fn);

// Does 'a' dominate 'b'?
ir_dominates(dom, a, b);

// Does 'a' strictly dominate 'b'?
ir_strictly_dominates(dom, a, b);

// Immediate dominator of 'b'
IrBasicBlock *idom = ir_get_idom(dom, b);

// Dominance frontier of 'b'
int count;
IrBasicBlock *const *df = ir_get_dominance_frontier(dom, b, &count);

// Verify correctness
ir_dominance_verify(dom, &err);

// Debug dump
ir_dominance_dump(stdout, dom);

ir_dominance_free(dom);
```

---

## SSA Construction

### What Gets Promoted
Local scalar variables (`alloca` instructions) whose pointer only appears in `store val -> var` and `load %r = var` patterns.

### Algorithm
1. Identify promotable allocas
2. Compute dominance and dominance frontiers
3. Place φ-nodes at iterated DF boundaries
4. Rename via dominator tree DFS walk with scoped version stacks

---

## SSA Verification

`ir_ssa_verify_function` checks:
- **Single-definition**: every virtual register defined exactly once
- **Dominance of uses**: the definition of every used register dominates the use site
- **φ-node placement**: φ-nodes appear only at the top of basic blocks, before all non-φ instructions
- **φ-node predecessors**: every incoming block in a φ-node is an actual predecessor of the block

---

## SSA Destruction

`ir_ssa_deconstruct_function` lowers SSA back to standard non-SSA IR:
1. **Critical edge splitting**: inserts intermediate `split.<P>.<S>` blocks so copies can be safely placed
2. **Parallel copy sequentialization**: emits sequential copies before the predecessor's terminator, breaking cycles via temporaries

---

## New CLI Flags

| Flag | Effect |
|------|--------|
| `--ssa` | Enable SSA construction → optimization → destruction before codegen |
| `--dump-ssa` | Build SSA form, dump it to stdout, then exit (no codegen) |
| `--help`, `-h` | Print full usage guide |

### Examples

```bash
# Compile with SSA-level optimization
cco hello.cco --ssa -O1 -o hello --run

# Inspect SSA form of a program
cco hello.cco --dump-ssa

# Native binary with SSA and internal linker
cco hello.cco --ssa --use-internal-linker -o hello --run

# Fully native, no SSA, internal linker
cco hello.cco --use-internal-linker -o hello --run
```

### Example --dump-ssa Output (loop)

```
fn @main() -> i32 {
entry:
    %0 = const f64 0
    br label %while.cond.1
while.cond.1:
    %22 = phi f64 [entry: %0], [while.body.2: %14]
    %10 = const f64 3
    %11 = lt f64 %22, %10
    condbr %11, label %while.body.2, label %while.end.3
while.body.2:
    %13 = const f64 1
    %14 = add f64 %22, %13
    br label %while.cond.1
while.end.3:
    print f64 %22
    …
}
```

The loop counter `%22` is promoted to a φ-node at the loop header — no stack load/store needed.

---

## Test Results

### Phase 5 SSA Unit Tests (13 tests)

| # | Test | Result |
|---|------|--------|
| 1 | CFG Dominance: Straight-line | ✅ PASS |
| 2 | CFG Dominance: Diamond (if-else) | ✅ PASS |
| 3 | CFG Dominance: Natural Loop | ✅ PASS |
| 4 | CFG Dominance: Unreachable Block | ✅ PASS |
| 5 | SSA Construction: If-Else scalar promotion | ✅ PASS |
| 6 | SSA Construction: While Loop loop-carried | ✅ PASS |
| 7 | SSA Verifier Negative: Non-dominating use | ✅ PASS |
| 8 | SSA Verifier Negative: Duplicate definition | ✅ PASS |
| 9 | SSA Verifier Negative: Phi after non-phi | ✅ PASS |
| 10 | SSA Optimizations: Const Prop + DCE | ✅ PASS |
| 11 | SSA Destruction: Critical Edge Splitting | ✅ PASS |
| 12 | SSA Destruction: Parallel Copy Cycle (x↔y) | ✅ PASS |
| 13 | E2E Differential: Non-SSA vs SSA vs Native Internal | ✅ PASS |

**Valgrind: 0 errors / 0 leaks (591 allocs, 591 frees)**

### Full Test Suite (after Phase 5)

| Suite | Result |
|-------|--------|
| Unit tests (9 suites) | ✅ All passed, 0 leaks |
| Self-hosted lexer | ✅ 135/135 |
| Bootstrap parity | ✅ 4/4 |
| Integration / network | ✅ 111/111 PASS, 0 failed |

---

## Scope Boundaries Respected

> [!NOTE]
> The following were **intentionally excluded** from Phase 5 (reserved for future phases):
> - Global Value Numbering (GVN)
> - LICM / loop optimizations
> - Vectorization / autovectorization
> - Memory SSA / alias analysis
> - Interprocedural / LTO
> - JIT compilation
> - Profile-guided optimization
