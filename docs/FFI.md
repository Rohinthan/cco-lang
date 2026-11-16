# Cco Foreign Function Interface (FFI) Specification & Guide

This document describes the Foreign Function Interface (FFI) of the Cco programming language, consolidating capabilities across all completed FFI milestones (FFI-1 through FFI-6).

Cco provides zero-overhead, type-checked interoperability with ISO C libraries under the System V AMD64 Application Binary Interface (ABI). This enables Cco programs to call standard C runtime libraries (e.g., `libc`, `libm`), interact with third-party native libraries (e.g., SQLite, POSIX APIs), manipulate raw pointers in quarantined `unsafe` blocks, and pass or return plain-old-data (POD) aggregate structures by value.

---

## Table of Contents

1. [Cco-to-C Interoperability Architecture](#1-cco-to-c-interoperability-architecture)
2. [Foreign Function Declarations (`extern "C"`)](#2-foreign-function-declarations-extern-c)
3. [Library Linking & Toolchain Integration](#3-library-linking--toolchain-integration)
4. [Pointer Types, Mutability & Constness](#4-pointer-types-mutability--constness)
5. [C String Interoperability](#5-c-string-interoperability)
6. [Out Parameters & Address-Taking Restrictions](#6-out-parameters--address-taking-restrictions)
7. [Unsafe Boundaries & General Pointer Operations](#7-unsafe-boundaries--general-pointer-operations)
8. [Pointer Difference Semantics](#8-pointer-difference-semantics)
9. [Aggregate Pass-by-Value ABI (System V AMD64)](#9-aggregate-pass-by-value-abi-system-v-amd64)
10. [Dual-Backend Parity (C11 vs. Native x86-64)](#10-dual-backend-parity-c11-vs-native-x86-64)
11. [Unsupported ABI Types & Strict Rejection Boundaries](#11-unsupported-abi-types--strict-rejection-boundaries)
12. [Memory Management & Ownership Model at the Foreign Boundary](#12-memory-management--ownership-model-at-the-foreign-boundary)
13. [Current Status & Implementation Boundaries](#13-current-status--implementation-boundaries)

---

## 1. Cco-to-C Interoperability Architecture

Cco combines high-level syntax with low-level systems capabilities. While ordinary Cco code is statically type-checked and memory-safe under compile-time affine single-ownership, systems programming frequently requires direct communication with existing C libraries, operating system system-calls, and raw memory buffers.

Cco's FFI architecture is built on three core principles:

1. **Zero Runtime Overhead**: Foreign function calls compile directly to standard machine calls (`call` instructions on x86-64 or direct C function invocations in the C11 backend) adhering to the System V AMD64 calling convention. No runtime wrappers, dynamic translation layers, or marshaling thunks are generated.
2. **Strict Safety Isolation**: Unchecked operations (pointer dereferencing, pointer arithmetic, unchecked casts, and aggregate pass-by-value invocations) are quarantined within explicit `unsafe { ... }` blocks. Code outside `unsafe` remains statically checked against spatial memory errors and unmanaged mutation.
3. **Dual-Backend Parity**: Every FFI capability is implemented with identical observable semantics across both the portable **C11 source-to-source backend** and the **native x86-64 machine-code backend**.

---

## 2. Foreign Function Declarations (`extern "C"`)

External C functions are declared at module scope using `extern "C"`. Both single-line declarations and grouped block declarations are supported.

### Syntax

```cco
# Single-line foreign function declaration
extern "C" fn puts(s: *const char) -> int;
extern "C" fn cos(x: float) -> float;

# Grouped foreign function block
extern "C" {
    fn abs(x: int) -> int;
    fn getenv(name: *const char) -> *char;
    fn free(ptr: *mut void) -> void;
    fn malloc(size: int) -> *mut void;
}
```

### Parameter & Return Types

Foreign function signatures accept:
- **Primitive Scalar Types**: `int` (64-bit signed integer), `float` (64-bit IEEE-754 double precision), `char` (8-bit byte / character), `bool` (1-byte boolean), `void` (only as return type).
- **Raw Pointer Types**: `*const T`, `*mut T` (or legacy `*T`), `*const void`, `*mut void` (or `*void`), and nested pointer types (e.g., `**char`).
- **Supported POD Struct Types**: Plain-Old-Data structs passed or returned by value.

### Parameter Ownership Semantics

All parameters passed to `extern "C"` functions are **borrowed** by default:
- The caller retains ownership of arguments.
- The callee does not deallocate or move foreign arguments.
- Passing a local variable to a foreign function does not mark it as moved in Cco's ownership scope analyzer.

---

## 3. Library Linking & Toolchain Integration

To resolve external symbols at link time, Cco provides command-line flags for specifying library search paths and archive dependencies.

### Command-Line Options

- `-L<dir>` or `-L <dir>`: Add `<dir>` to the linker's library search path.
- `-l<lib>` or `-l <lib>`: Link against the shared library or archive `lib<lib>.so` / `lib<lib>.a`.

```bash
# Link against system libraries (e.g., libm, libz)
cco app.cco -lm -lz -o app

# Link against a custom library in ./lib
cco app.cco -L ./lib -lmylib -o app

# Compile via the native x86-64 backend with custom linking
cco app.cco --use-native -L/opt/custom/lib -lsqlite3 -o app

# Compile and run immediately
cco app.cco -L./lib -lmylib --run
```

### Toolchain Invocation & Safety

- **Process Execution via `execvp`**: The compiler driver invokes the system toolchain (`gcc`, `clang`, or the host linker) via argument-vector execution (`execvp`) rather than shell string concatenation (`system()`). This prevents shell-injection vulnerabilities and cleanly preserves directories containing spaces.
- **Dependency Ordering**: Command-line `-L` and `-l` options are passed to the host linker in strict user-specified order, ensuring correct symbol resolution for static archives (`.a`).
- **Freestanding Linker Boundary**: The standalone internal linker (`--use-internal-linker`) targets self-contained freestanding Linux binaries with an embedded syscall runtime and does not link dynamic C libraries. Passing `-l` or `-L` with `--use-internal-linker` generates a compile-time error advising the use of `--use-native`.

---

## 4. Pointer Types, Mutability & Constness

Cco provides first-class raw pointer types for interfacing with unmanaged memory addresses, memory-mapped I/O, and foreign library handles.

### Pointer Type Hierarchy

| Type Syntax | Semantics | Mutability | Safe Usage |
| :--- | :--- | :---: | :--- |
| `*const T` | Pointer to constant `T` | Immutable target | Storage, passing, null-checks, equality |
| `*mut T` (or `*T`) | Pointer to mutable `T` | Mutable target | Storage, passing, null-checks, out-parameters |
| `*const void` | Untyped constant memory handle | Immutable target | Opaque read-only handle |
| `*mut void` (or `*void`) | Untyped mutable memory handle | Mutable target | Opaque mutable handle, allocator returns |
| `null` | Null pointer literal (`0x0`) | N/A | Assignable to any raw pointer type |

### Const Qualification & Subtyping

Cco enforces strict const-correctness at compile time:
- **Covariant Subtyping**: A mutable pointer `*mut T` can be implicitly passed or assigned where `*const T` is expected.
  ```cco
  let p: *mut int = ...;
  let q: *const int = p;     # Allowed: safe widening to const
  ```
- **Const Invariance**: A constant pointer `*const T` cannot be implicitly assigned to `*mut T`. Mutating memory through `*const T` is rejected at compile time.
  ```cco
  let c: *const int = ...;
  # let m: *mut int = c;    <- Compile error: cannot assign *const int to *mut int
  ```

### Safe Pointer Operations

Outside `unsafe` blocks, raw pointers can be safely:
1. Declared, initialized, and assigned.
2. Initialized or assigned to `null`.
3. Compared for equality or inequality (`p == q`, `p != q`, `p == null`, `p != null`).
4. Passed as arguments or received as return values.

### Non-Owning Invariant

Raw pointers (`*const T`, `*mut T`, `*void`) are strictly **unmanaged addresses**. Cco's scope analyzer never attaches destructor cascades, drop flags, or automatic `free()` calls to raw pointer bindings.

---

## 5. C String Interoperability

Cco distinguishes sharply between its managed, length-prefixed `string` type and unmanaged, NUL-terminated C character pointers (`*const char`, `*char`).

To prevent memory leaks, dangling pointers, and ownership ambiguities, conversion between the two is explicit via built-in functions:

```cco
# Convert Cco managed string to foreign C string
str_to_cstr(s: string) -> *const char

# Ingest foreign C string into Cco managed string
str_from_cstr(p: *const char) -> string
```
*(Canonical aliases `string_to_cstr` and `string_from_cstr` are also supported.)*

### `str_to_cstr` Semantics
- Returns a borrowed, non-owning `*const char` pointing to the underlying contiguous, NUL-terminated byte buffer of `s`.
- The returned pointer is valid only as long as the source `string` remains in scope and unmodified.
- Passing an empty string `""` safely yields a pointer to a valid static empty string buffer.
- Does not allocate heap memory.

### `str_from_cstr` Semantics
- Ingests a NUL-terminated C string and allocates a new, independent Cco `string` using a defensive copy (`strdup`/`memcpy`).
- The resulting string is fully integrated into Cco's affine single-ownership model: it is automatically cleaned up with 0 leaks upon scope exit.
- Passing `null` safely produces an owned empty string `""` without crashing.
- Accepts only `*const char`, `*char`, and `null`. Passing untyped `*void` or non-character pointers is rejected at compile time.

### Example

```cco
extern "C" {
    fn puts(s: *const char) -> int;
    fn getenv(name: *const char) -> *char;
}

fn main() -> int {
    # 1. Borrow Cco string as *const char for foreign call
    msg: string = "Hello, foreign C library!";
    puts(str_to_cstr(msg));

    # 2. Ingest unowned C return pointer into an owned Cco string
    path_ptr: *char = getenv(str_to_cstr("PATH"));
    if path_ptr != null {
        path: string = str_from_cstr(path_ptr);
        print("Path length:", len(path));
        # path is automatically deallocated upon scope exit; path_ptr is untouched
    }
    return 0;
}
```

---

## 6. Out Parameters & Address-Taking Restrictions

C APIs commonly use pointer arguments as "out parameters" to write return values back to the caller. Cco supports this pattern using the address-of operator `&`.

### Valid Address-Taking Targets (Lvalues Only)

The `&` operator produces a mutable pointer (`*mut T`) and is permitted only on valid lvalues:
- Local variable bindings: `&x`
- Struct fields: `&ts.sec`
- Nested struct fields: `&outer.inner.field`
- Array elements: `&buf[0]`

```cco
extern "C" {
    fn get_dimensions(width: *mut int, height: *mut int) -> int;
}

fn main() -> int {
    w: int = 0;
    h: int = 0;
    get_dimensions(&w, &h);
    print("Dimensions:", w, h);
    return 0;
}
```

### Strict Compile-Time Restrictions

To guarantee memory safety and prevent dangling stack pointers:
1. **No Rvalue Address-Taking**: Expressions such as `&(x + 1)`, `&42`, or `&fn_call()` are rejected at compile time.
2. **No Address Escape in Safe Code**: An address taken with `&` cannot be returned from a function or stored in long-lived data structures outside an `unsafe` block.

---

## 7. Unsafe Boundaries & General Pointer Operations

Operations that cannot be proven memory-safe at compile time are quarantined inside `unsafe { ... }` blocks.

### The `unsafe` Block

```cco
unsafe {
    # Unchecked dereferencing, pointer arithmetic, and casts permitted here
}
```

`unsafe` blocks may be nested arbitrarily. Code inside `unsafe` remains statically typed, but the programmer assumes responsibility for memory validity.

### Unchecked Pointer Dereferencing

Dereferencing is permitted only inside `unsafe` blocks:
- **Read**: `*p` reads a scalar value of type `T` from the target address.
- **Write**: `*p = expr` stores a value of type `T` to the target address through a `*mut T` pointer.

```cco
val: int = 100;
unsafe {
    p: *mut int = &val;
    *p = 200;              # Dereference write
    current: int = *p;     # Dereference read
}
print(val);                # 200
```
Attempting to dereference a pointer outside `unsafe` results in a fatal compile error:
`"raw pointer dereferencing is not supported in safe code"`.

### Pointer Arithmetic

Inside `unsafe` blocks, pointers support addition and subtraction with integer offsets:
- `p + n`: Returns a pointer advanced by $n \times \text{sizeof}(T)$ bytes.
- `p - n`: Returns a pointer retreated by $n \times \text{sizeof}(T)$ bytes.
- `p[i]`: Pointer indexing, equivalent to `*(p + i)` for both reads and writes.

```cco
let arr: int[] = alloc(int, 4);
arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40;

unsafe {
    let p: *mut int = &arr[0];
    let second: int = *(p + 1);   # 20
    p[2] = 99;                    # arr[2] is now 99
}
```

### Pointer Casting (`as`)

Inside `unsafe` blocks, pointers may be cast explicitly between pointer types using the `as` operator:

```cco
unsafe {
    let raw: *mut void = malloc(64);
    let int_ptr: *mut int = raw as *mut int;
    *int_ptr = 42;
    free(raw);
}
```

---

## 8. Pointer Difference Semantics

Inside `unsafe` blocks, subtracting two pointers of compatible types evaluates to the signed distance between them:

$$\text{difference} = \frac{\text{address}(p_1) - \text{address}(p_2)}{\text{sizeof}(T)}$$

### Semantics & Types

```cco
unsafe {
    let diff: int = p1 - p2;
}
```

- **Result Type**: 64-bit signed integer (`int`), corresponding to standard `ptrdiff_t`.
- **Scaling**: The byte offset is automatically divided by the size of the pointee type $T$.
- **Sign**: Yields a positive integer when $p_1 > p_2$, zero when $p_1 = p_2$, and a negative integer when $p_1 < p_2$.
- **Compatibility**: Both pointers must point to the same type (or compatible constness). Subtracting incompatible types is rejected at compile time.
- **Precondition**: Under System V ABI rules, pointer difference is well-defined when both pointers refer to elements of the same allocation (or one element past the end).

---

## 9. Aggregate Pass-by-Value ABI (System V AMD64)

Starting in FFI-6, Cco supports passing and returning Plain-Old-Data (POD) aggregate structures by value in foreign function calls, adhering strictly to the **System V AMD64 ABI specification**.

### Supported POD Struct Types

A struct is eligible for FFI pass-by-value if it consists entirely of:
- Primitive scalar types (`int`, `float`, `char`, `bool`).
- Raw pointer types (`*const T`, `*mut T`, `*void`).
- Nested POD structs composed of the above.

```cco
struct Point2D {
    x: float;
    y: float;
}

struct Record {
    id: int;
    active: bool;
    pos: Point2D;          # Nested POD struct
}
```

### Eightbyte Classification

The System V AMD64 ABI classifies aggregates in 8-byte units ("eightbytes") based on the types and offsets of their fields:

1. **INTEGER**: Fields of integer, boolean, character, or pointer types.
2. **SSE**: 64-bit floating point fields (`float`).
3. **NO_CLASS**: Padding bytes or uninitialized alignment space.
4. **MEMORY**: Aggregates exceeding 16 bytes (2 eightbytes), containing unaligned fields, or containing types classified as MEMORY.

#### Merge Rules
If an eightbyte contains multiple fields, classes are merged according to precedence:
$$\text{MEMORY} > \text{INTEGER} > \text{SSE} > \text{NO_CLASS}$$

### Parameter Passing & Register Allocation

Parameters are allocated registers according to their classification:

| Class | Available Registers | Capacity |
| :--- | :--- | :---: |
| **INTEGER** | `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9` | Up to 6 eightbytes |
| **SSE** | `%xmm0`, `%xmm1`, `%xmm2`, `%xmm3`, `%xmm4`, `%xmm5`, `%xmm6`, `%xmm7` | Up to 8 eightbytes |
| **MEMORY** | Stack memory | Passed on outgoing stack frame |

#### Atomic Rollback Rule
If an aggregate requires multiple registers (e.g., two INTEGER registers or one INTEGER + one SSE) and sufficient registers of any required class are not available, **all register allocations for that aggregate are atomically rolled back**, and the entire aggregate is passed on the stack.

### Aggregate Return Handling

Aggregates returned by value from foreign functions are classified using the same eightbyte rules:

| Classification | Return Location |
| :--- | :--- |
| **1 Eightbyte INTEGER** | `%rax` |
| **2 Eightbytes INTEGER** | `%rax` (low eightbyte), `%rdx` (high eightbyte) |
| **1 Eightbyte SSE** | `%xmm0` |
| **2 Eightbytes SSE** | `%xmm0` (low eightbyte), `%xmm1` (high eightbyte) |
| **Mixed INTEGER / SSE** | `%rax` (INTEGER eightbyte), `%xmm0` (SSE eightbyte) |
| **Mixed SSE / INTEGER** | `%xmm0` (SSE eightbyte), `%rax` (INTEGER eightbyte) |
| **MEMORY (> 16 bytes)** | Hidden `sret` pointer passed as first parameter in `%rdi` |

#### Hidden Return Pointer (`sret`)
For structures classified as `MEMORY` (such as structs larger than 16 bytes), the caller allocates space on its stack frame and passes a hidden pointer to this space in `%rdi` as the first argument. The foreign callee writes the return value directly into this buffer and returns the address in `%rax`.

### Foreign Aggregate Call Boundary

Because aggregate pass-by-value operations interact directly with machine calling conventions and memory layout, calling foreign functions that take or return structs by value requires an `unsafe` block:

```cco
struct Vector3 {
    x: float;
    y: float;
    z: float;
}

extern "C" {
    fn compute_normal(v: Vector3) -> Vector3;
}

fn main() -> int {
    let v: Vector3 = Vector3 { x: 1.0, y: 2.0, z: 3.0 };
    let norm: Vector3 = Vector3 { x: 0.0, y: 0.0, z: 0.0 };
    unsafe {
        norm = compute_normal(v);
    }
    return 0;
}
```

---

## 10. Dual-Backend Parity (C11 vs. Native x86-64)

Cco maintains full observable parity between its two compilation backends:

```
                      Cco FFI Source Code
                               │
               ┌───────────────┴───────────────┐
               ▼                               ▼
      [ C11 Transpiler ]             [ Native x86-64 Backend ]
    Generates ISO C11 code           Direct Binary Codegen
    Host C compiler applies         Compiler performs direct
    System V AMD64 ABI rules         Eightbyte ABI classification,
               │                    GPR/XMM allocation, & sret emission
               ▼                               ▼
       Identical Observable ABI Behavior & Program Output
```

1. **C11 Backend**: Transpiles Cco POD structs to standard C `struct` definitions and foreign function calls to standard C calls. The host C compiler (`gcc` or `clang`) applies the platform ABI.
2. **Native x86-64 Backend**: The native compiler implements recursive eightbyte classification, linear-scan GPR/XMM assignment with atomic rollback, and hidden `sret` pointer management directly, emitting machine code without external dependencies.
3. **Verification**: All FFI acceptance suites verify identical standard output, return codes, and memory layouts across both backends.

---

## 11. Unsupported ABI Types & Strict Rejection Boundaries

To prevent silent data corruption and maintain memory safety guarantees, Cco strictly rejects unsupported types at foreign boundaries with fatal compile-time diagnostics:

| Unsupported Construct | Rejection Rationale | Compile-Time Behavior |
| :--- | :--- | :--- |
| **Managed Types by Value** (`string`, `T[]`, `map[K]V`) | Managed types carry Cco-specific metadata (drop flags, capacities, hash headers) incompatible with C value layout. | Fatal compile error |
| **Heap Classes by Value** (`class`) | Reference types governed by affine single-ownership; must be passed via pointer handles. | Fatal compile error |
| **Tagged Unions with Payloads** (`enum`) | Cco enum discriminant and payload layout does not match arbitrary C unions. | Fatal compile error |
| **Aggregates with Alignment > 8** (`int128`, 16-byte vector types) | Natural alignment exceeding 8 bytes requires 16-byte or 32-byte stack alignment rules not supported in the standard POD subset. | Fatal compile error |
| **Extended Precision** (`float80` / x87) | System V AMD64 `X87` / `COMPLEX_X87` classes are intentionally unsupported. | Fatal compile error |
| **Unaligned Bitfields & Packed Structs** | Non-standard memory offsets violate natural eightbyte classification boundaries. | Fatal compile error |
| **Variadic Foreign Functions** (`...`) | System V AMD64 requires `%rax` to count vector registers passed to variadic calls. | Fatal compile error |

---

## 12. Memory Management & Ownership Model at the Foreign Boundary

Cco's core memory safety guarantee is that **ordinary Cco programming is automatically managed without garbage collection**. The introduction of raw pointers and FFI does not turn Cco into a manually managed language.

### Ordinary Cco Code vs. Foreign Contracts

```
┌──────────────────────────────────────────────┐
│             Ordinary Cco Domain              │
│  - Affine single-ownership                   │
│  - Compile-time move tracking                │
│  - Automated scope-exit deallocation         │
│  - Borrowed references (&T)                  │
└──────────────────────┬───────────────────────┘
                       │
             FFI Boundary (Explicit)
   str_to_cstr() ───►  │  ───► str_from_cstr()
   &lvalue       ───►  │  ◄─── raw pointers (*mut T, *void)
                       │
┌──────────────────────┴───────────────────────┐
│               Foreign C Domain               │
│  - Unmanaged addresses                       │
│  - Library-specific lifetime contracts       │
│  - Caller must invoke foreign free()         │
└──────────────────────────────────────────────┘
```

### Key Ownership Principles:

1. **Foreign Memory Ownership**: When a foreign C function allocates memory (e.g., via `malloc()` or a library initializer) and returns a raw pointer (`*mut void`, `*char`), that memory is governed by the foreign library's contract. The caller must explicitly pass the pointer to the appropriate foreign cleanup function (e.g., `free(ptr)` or `sqlite3_close(db)`).
2. **No Automatic Drops for Raw Pointers**: Cco's scope analyzer never frees a raw pointer. Letting a `*mut void` leave scope without calling its foreign destructor will leak memory in the foreign domain, just as in C.
3. **Defensive Ingestion**: When foreign data must enter Cco's managed domain, use `str_from_cstr()` to create an owned copy. Once ingested, Cco assumes full ownership and guarantees automated cleanup.
4. **Non-Escaping Borrows**: When passing Cco data to foreign functions (`str_to_cstr(s)` or `&var`), the borrow is temporary. The foreign function must not retain the address beyond the duration of the call.

---

## 13. Current Status & Implementation Boundaries

### Implemented & Fully Verified (FFI-1 through FFI-6)
- In-source `extern "C"` declarations and grouped blocks.
- External dynamic and static library linking via `-L` and `-l`.
- Raw pointers (`*const T`, `*mut T`, `*void`, `null`) with const-subtyping.
- Explicit C string conversion (`str_to_cstr`, `str_from_cstr`).
- Out parameters via address-taking (`&var`, `&s.f`, `&arr[i]`).
- Quarantined `unsafe { ... }` blocks with dereferencing (`*p`, `*p = val`).
- Scaled pointer arithmetic (`p + n`, `p - n`, `p[i]`).
- Pointer difference (`p1 - p2 -> int`).
- System V AMD64 aggregate pass-by-value parameter passing (GPR, XMM, stack rollback).
- System V AMD64 aggregate return handling (`%rax`, `%rdx`, `%xmm0`, `%xmm1`, hidden `sret`).
- Dual-backend parity across all positive fixtures and negative rejection diagnostics.

### Future Possibilities (Not Currently Implemented)
- Variadic C foreign function calls (`...`).
- Self-hosted compiler FFI bootstrap parity (`selfhost/cco`).
- ARM64 (AArch64) ABI support for native backend FFI.
- Direct slice passing (`&[T]`) across foreign boundaries.
