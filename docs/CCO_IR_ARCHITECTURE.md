# Cco Intermediate Representation (Cco IR) — Technical Specification & Architecture

## 1. Overview and Rationale

The Cco programming language compiler is establishing its native compiler foundation. Previously, compilation proceeded directly from an annotated Abstract Syntax Tree (AST) to ISO C11 source text:

```
Cco Source → Lexer → Parser → AST → Semantic/Scope Analysis → C11 Codegen → GCC/Clang → Executable
```

While direct C11 emission provides a reliable reference backend, direct AST-to-C translation couples frontend language constructs directly to C syntax and obscures low-level control flow, stack layouts, and future optimization opportunities.

**Phase 1** introduces **Cco IR**, a typed, deterministic, verified linear/basic-block intermediate representation situated between semantic analysis and code generation:

```
Cco Source
   ↓
Lexer & Parser
   ↓
AST Construction
   ↓
Module & Trait Resolution
   ↓
Scope & Single-Ownership Analysis (Affinity & Cleanup Planning)
   ↓
Cco IR Lowering (ir_lower.c)
   ↓
Cco IR Verification (ir_verify.c)
   ↓
Cco IR to C11 Codegen (ir_codegen_c.c)  [or direct textual dump via --emit-ir]
   ↓
Host C Compiler (GCC / Clang)
   ↓
Native Executable
```

The existing direct C11 backend (`src/codegen.c`) remains completely intact and operational as the primary reference path. The IR infrastructure introduces an alternative, verified compilation path (`--use-ir`) and a textual IR inspection option (`--emit-ir`).

---

## 2. Memory Architecture & Lifetime Management

All IR data structures (types, values, instructions, basic blocks, functions, and module metadata) are allocated within a dedicated **`IrArena`** memory arena (`src/ir.c`).

### Arena Characteristics
* **Block Chunks**: Memory is allocated in chunks (starting at 64 KB) and doubled exponentially as needed.
* **Alignment**: All allocations are strictly 8-byte aligned.
* **Bulk Reclamation**: An entire module's memory is released in a single `ir_module_free(IrModule *mod)` call with zero individual node traversal overhead.
* **Zero Leak Guarantee**: Tested under Valgrind Memcheck with 0 bytes lost across all allocations.

---

## 3. IR Type System

The IR defines an explicit type system independent of host C typedefs (`IrTypeKind` in `src/ir.h`):

| IR Type | Kind Enum | Description |
|---|---|---|
| `void` | `IR_TYPE_VOID` | Unit / return type for functions with no result |
| `i32` | `IR_TYPE_I32` | 32-bit signed integer (default Cco `int`) |
| `i64` | `IR_TYPE_I64` | 64-bit signed integer |
| `f64` | `IR_TYPE_F64` | 64-bit IEEE 754 floating-point (Cco `float`) |
| `bool` | `IR_TYPE_BOOL` | Boolean value (`true` / `false`) |
| `char` | `IR_TYPE_CHAR` | 8-bit character |
| `ptr` | `IR_TYPE_PTR` | Pointer with explicit target `elem_type` (e.g. `ptr<char>` for strings) |
| `struct` | `IR_TYPE_STRUCT` | Named compound struct or class |
| `array` | `IR_TYPE_ARRAY` | Array type with element type and fixed or dynamic bounds |

### Type Equality and Constructors
* `ir_type_equals(a, b)`: Structural equality comparison for primitive, pointer, array, and struct types.
* Constructors (`ir_type_i32(arena)`, `ir_type_ptr(arena, elem)`, etc.) produce standardized, arena-backed representations.

---

## 4. Value and Instruction Model

### Values (`IrValue`)
Every operand in Cco IR is explicitly typed and belongs to one of the following categories:
* `IR_VAL_CONST_INT`, `IR_VAL_CONST_FLOAT`, `IR_VAL_CONST_BOOL`, `IR_VAL_CONST_CHAR`, `IR_VAL_CONST_STRING`: Literals.
* `IR_VAL_REG`: Virtual temporary register (`%0`, `%1`, `%2`, ...), assigned deterministically per function.
* `IR_VAL_VAR`: Stack slot pointer created by `alloca` (e.g., `%x.addr`).
* `IR_VAL_PARAM`: Formal function parameter (e.g., `%arg.x`).
* `IR_VAL_GLOBAL`: Global symbol reference.

### Instruction Set (`IrOpcode`)

1. **Stack & Memory**:
   * `alloca <type>`: Allocates stack space for a local variable, returning a typed pointer.
   * `load <type>, <ptr>`: Reads value of `<type>` from `<ptr>`.
   * `store <type> <val>, <ptr>`: Writes `<val>` to memory address `<ptr>`.
2. **Constants**:
   * `const <type> <literal>`: Loads literal value into a temporary register.
3. **Arithmetic**:
   * `add`, `sub`, `mul`, `div`, `mod`: Binary operations on matching numeric types (`i32`, `i64`, `f64`).
   * `neg`: Unary arithmetic negation.
4. **Comparisons**:
   * `eq`, `ne`, `lt`, `le`, `gt`, `ge`: Binary comparison of compatible types, producing `bool`.
5. **Logic**:
   * `and`, `or`: Binary short-circuitable boolean operators.
   * `not`: Unary boolean inversion.
6. **Subroutines & Calls**:
   * `call <ret_type> @name(<arg0>, ...)`: Explicit function invocation.
7. **Runtime & Memory Cleanups**:
   * `print <val>`: Formatted runtime printing based on operand type.
   * `free <ptr>`: Explicit heap deallocation of single allocations or arrays.
   * `release <ptr>, <class_name>`: Explicit reference-counted destructor execution.
8. **Terminators**:
   * `br label %target`: Unconditional branch.
   * `condbr %cond, label %true_target, label %false_target`: Conditional branch on boolean register.
   * `ret <type> %val` / `ret void`: Function return.

---

## 5. Basic Blocks and Control Flow Graph (CFG)

A basic block (`IrBasicBlock`) represents a straight-line sequence of instructions with:
* A deterministic alphanumeric label (`entry`, `while.cond.1`, `if.then.2`, etc.).
* A doubly linked list of instructions (`first_inst`, `last_inst`).
* **Strict Termination**: The block must conclude with exactly one terminator instruction (`br`, `condbr`, or `ret`). No instructions are permitted after the terminator.
* **CFG Predecessors & Successors**: Dynamically tracked and rebuilt via `ir_recompute_cfg(IrFunction *fn)`.

---

## 6. AST to IR Lowering (`ir_lower.c`)

Lowering consumes the semantically analyzed and scope-annotated AST:
1. **Entry Allocation**: Local variables declared via `NODE_LET` are assigned a dedicated `alloca` slot in the function's `entry` block.
2. **Single-Assignment Temporaries**: Expressions evaluate into unique virtual registers (`%0`, `%1`, ...).
3. **Control-Flow Translation**:
   * `if / else`: Condition evaluated $\rightarrow$ `condbr` to `if.then` or `if.else` $\rightarrow$ both branch to `if.merge`.
   * `while`: Inflow branch to `while.cond` $\rightarrow$ `condbr` to `while.body` or `while.end` $\rightarrow$ body loopback branch to `while.cond`.
   * `for`: Lower initializer $\rightarrow$ branch to `for.cond` $\rightarrow$ `condbr` to `for.body` or `for.end` $\rightarrow$ body branches to `for.step` $\rightarrow$ step branches to `for.cond`.
4. **Affine Ownership & Destructor Lowering**: Scope-analyzed deallocations (`frees_to_emit`, `releases_to_emit`) attached to statement exits and returns are lowered directly into explicit `free` and `release` IR instructions.

---

## 7. IR Verification Subsystem (`ir_verify.c`)

The verifier strictly audits modules and functions prior to code emission, rejecting malformed IR:
* **Missing or Misplaced Terminators**: Blocks must terminate, and only the final instruction may be a terminator.
* **Type Incompatibilities**: Arithmetic operations must have identical numeric operands; comparisons must produce `bool`; branch conditions must be `bool`.
* **Signature Conformance**: Function calls must supply exact argument counts and compatible types matching callee definitions.
* **Branch Validity**: Branch targets must reside within the enclosing function.

### Diagnostics Format
When verification fails, formatted error output identifies the exact context:
```text
Cco IR verification failed:
  function: main
  block: entry
  instruction: add
  error: operand types do not match (i32 vs bool)
```

---

## 8. Cco IR to C11 Backend (`ir_codegen_c.c`)

To validate that Cco IR preserves language semantics without requiring native machine-code emission in Phase 1, `ir_codegen_c.c` lowers verified IR back into standard ISO C11:
* Basic blocks map to C labels (`__bb_entry:;`, `__bb_while_cond_1:;`).
* Virtual registers are declared as local typed variables at function headers (`_r0`, `_r1`, ...).
* Branches map to structured `goto` jumps.
* Allocas map to address-of stack variables.
* The emitted C conforms strictly to ISO C11 and compiles under `-Wall -Wextra -Werror -pedantic-errors`.

---

## 9. CLI Driver Integration

The `cco` command-line interface provides seamless access to the IR subsystem:
* `cco program.cco -o binary`: Standard reference AST $\rightarrow$ C11 $\rightarrow$ Native pipeline.
* `cco program.cco --emit-ir`: Lower AST $\rightarrow$ IR, verify IR, and dump deterministic textual IR to `stdout`.
* `cco program.cco --use-ir -o binary`: Full AST $\rightarrow$ IR $\rightarrow$ IR Verification $\rightarrow$ IR-C11 $\rightarrow$ Native pipeline.
* `cco program.cco --use-ir --run`: Lower to IR, verify, compile, and execute in a single command.

---

## 10. Phase 2 Scope Demarcation

Phase 1 strictly establishes the typed IR, verifier, lowering engine, and IR-to-C validation bridge.
The following items are strictly deferred to **Phase 2 (Native Backend)**:
* Target machine descriptions (registers, stack layouts).
* Target instruction selection (x86-64 / ARM64 / RISC-V).
* Register allocation (e.g. Linear Scan or Graph Coloring).
* Calling convention lowering (System V AMD64 ABI).
* ELF / Mach-O / COFF object emission.
