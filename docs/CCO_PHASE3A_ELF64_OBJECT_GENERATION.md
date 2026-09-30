# Cco x86-64 Backend Phase 3A: Direct Machine-Code Encoding & ELF64 Relocatable Object Generation

## 1. Overview and Evolution

The Cco programming language native backend has evolved from a text-based assembly emitter into a high-performance native compiler capable of emitting relocatable ELF64 machine-code objects directly, without invoking the GNU Assembler (`as`).

```text
Phase 1:
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → C11 Codegen → GCC/Clang → Executable

Phase 2A (Stack-Centric Native Foundation):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → Frame Lowering → GNU/AT&T Assembly (.s) → GNU as → Host Linker → Linux ELF64

Phase 2B (GPR Register Allocation & Integer Code Quality):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → CFG Liveness Analysis → Linear-Scan Register Allocation → Frame Spilling & Lowering → Peephole Optimization → GNU/AT&T Assembly (.s) → GNU as → Host Linker → Linux ELF64

Phase 2C (Floating-Point and XMM Register Backend):
Cco Source → AST → Semantic Analysis → Cco IR → IR Verification → Type-Aware CFG Liveness Analysis → Dual-Class Linear-Scan Register Allocation (GPR + XMM) → Frame Spilling & Lowering → IEEE 754 SSE2 Code Generation → Peephole Optimization → GNU/AT&T Assembly (.s) → GNU as → Host Linker → Linux ELF64

Phase 2D (Stabilization, Complete System V AMD64 ABI & 3-Way Differential Verification):
Cco Source → Full System V AMD64 ABI (Stack Args >6 Int / >8 Float + Mixed + Return Zero-Extensions + 16-Byte Stack Alignment Invariants) → Dual-Class Linear Scan & Callee Spill Management → 3-Way Differential Testing Engine (AST→C11 vs IR→C11 vs Native x86-64) → Extreme Register Pressure & Nested Call Verification → Linux ELF64

👉 Phase 3A (Direct Machine-Code Encoding & ELF64 Relocatable Object Generation):
Cco Source → Full System V AMD64 ABI → Dual-Class Linear Scan Allocator → Unified Backend In-Memory Instruction Stream (X86InstrList)
             ├─ Assembly Text Serializer (.s) [Preserved Reference Pipeline] → GNU as ──┐
             └─ Direct x86-64 Machine Encoder (Opcode/ModRM/SIB/Imm) → ELF64 Writer ───┴─→ Linux ELF64 Relocatable (.o) → Host Linker (gcc/ld) → Linux Executable

Phase 3B (Upcoming): Builtin Internal Linker (Direct ELF64 Executable Generation)
Phase 3C (Upcoming): Advanced SSA/Machine Optimizations
```

In **Phase 3A**, the compiler eliminates the external assembler dependency for production native builds. Cco IR instructions are converted to an in-memory x86-64 instruction sequence, encoded directly into binary CPU instructions, organized into standard ELF64 sections with symbol tables and relocation tables, and written to disk as standard relocatable object files (`.o`).

---

## 2. Architectural Boundaries and Invariants

### 2.1 Scope Boundary: Phase 3A vs Phase 3B
* **Phase 3A Scope**:
  - Implement direct x86-64 instruction machine-code binary encoding (opcodes, REX prefixes, ModR/M bytes, SIB bytes, displacements, immediates).
  - Implement ELF64 relocatable object file generation (`ET_REL`, `EM_X86_64`) containing `.text`, `.rodata`, `.rela.text`, `.symtab`, `.strtab`, `.shstrtab`, and `.note.GNU-stack`.
  - Produce relocations (`R_X86_64_PLT32` and `R_X86_64_PC32`) for resolution by a linker.
  - Rely on the host linker (`gcc` / `ld`) to link the resulting `.o` files with system libraries (`libc`, `libm`).
* **Phase 3B Boundary (Strictly Future Work)**:
  - Phase 3A does **NOT** implement an internal linker.
  - Resolving relocations into absolute virtual addresses, synthesizing `PT_LOAD` program headers, generating static/dynamic `ET_EXEC` / `ET_DYN` executables, and replacing `gcc -no-pie` / `ld` are strictly reserved for Phase 3B.

### 2.2 Dual-Pipeline Architecture via Unified Instruction Stream
Rather than maintaining separate, duplicate lowering passes for text assembly and machine code, Phase 3A establishes a single unified intermediate representation for machine instructions:

```text
                     Cco IR
                       │
             Register Allocation & ABI
                       │
             gen_function_instructions()
                       │
                 X86InstrList
                /            \
               /              \
     x86_instr_list_to_asm()   x86_encode_function()
             │                         │
     GNU/AT&T Assembly (.s)      Machine-Code Bytes (.text)
             │                         │
          GNU as             elf64_write_object_file()
             │                         │
             └─────────┬───────────────┘
                       ▼
                 Relocatable .o
                       │
                  Host Linker
                       │
                  Executable
```

This guarantees **100% semantic identity**: peephole optimizations, register assignments, spills, and instruction selection are executed once on `X86InstrList`.

---

## 3. Machine-Code Encoder (`src/x86_64_encode.c`)

The x86-64 instruction encoder translates `X86Instr` structures into raw bytes conforming to Intel 64 and AMD64 Architecture Manuals.

### 3.1 Instruction Inventory
The encoder fully implements all 38 instruction opcodes utilized across integer and floating-point lowering:

| Category | Opcodes |
| :--- | :--- |
| **Control Flow** | `PUSH`, `POP`, `RET`, `CALL`, `JMP`, `JE`, `JNE`, `JL`, `JLE`, `JG`, `JGE`, `JB`, `JBE`, `JA`, `JAE` |
| **Integer Data Movement** | `MOV`, `MOVZB`, `LEA` |
| **Integer Arithmetic / Logic**| `ADD`, `SUB`, `IMUL`, `IDIV`, `NEG`, `AND`, `OR`, `XOR`, `CMP`, `TEST`, `SETCC`, `CQO`, `CDQ` |
| **SSE2 Floating-Point** | `MOVSD`, `ADDSD`, `SUBSD`, `MULSD`, `DIVSD`, `UCOMISD`, `XORPD`, `CVTSI2SD`, `CVTTSD2SI` |

### 3.2 REX Prefix Generation
In 64-bit mode, the 1-byte REX prefix (`0100WRXBb` = `0x40 .. 0x4F`) is prepended when:
* A 64-bit operand size is selected (`W = 1`).
* An extended register (`%r8` through `%r15`, or `%xmm8` through `%xmm15`) is addressed in the `reg` field (`R = 1`).
* An extended register is addressed in the `index` field of a SIB byte (`X = 1`).
* An extended register is addressed in the `rm` or opcode-embedded base field (`B = 1`).

### 3.3 ModR/M and SIB Byte Mechanics
The encoder systematically handles all required addressing modes while adhering to x86-64 hardware special cases:

1. **Register-to-Register (`mod = 11b`)**:
   `ModRM = (3 << 6) | (reg_code << 3) | rm_code`.
2. **Memory Addressing with Displacement**:
   - `0(%reg)`: Mod `00b` (no displacement), except for `%rbp` / `%r13` where Mod `00b` denotes RIP-relative addressing. For `%rbp` / `%r13` with displacement 0, Mod `01b` with an explicit `disp8 = 0` is emitted (`0x45 0x00`).
   - `disp8(%reg)` (signed -128 to 127): Mod `01b` followed by 1 byte displacement.
   - `disp32(%reg)`: Mod `10b` followed by 4 bytes little-endian displacement.
3. **Mandatory SIB Byte for Stack and Extended Stack Addressing (`%rsp` & `%r12`)**:
   - The hardware register code for `%rsp` and `%r12` is `4` (`100b`). In x86-64 ModR/M, `rm = 4` instructs the CPU to decode an adjacent SIB byte.
   - For `[rsp + disp]` or `[r12 + disp]`, the encoder appends SIB byte `0x24` (`scale=00b`, `index=100b [none]`, `base=100b [%rsp/%r12]`).
4. **RIP-Relative Data Addressing**:
   - For accesses to global constants and float pool literals in `.rodata`, Mod `00b` with `rm = 5` (`101b`) is emitted, followed by a 4-byte displacement placeholder and a `R_X86_64_PC32` relocation.

### 3.4 Two-Pass Branch Displacement Resolution
Conditional and unconditional jumps (`jmp`, `je`, `jne`, etc.) are resolved using a two-pass assembler:
1. **Pass 1 (Layout & Label Recording)**:
   Instructions are encoded into a preliminary byte buffer. As each instruction is laid out, the exact byte offsets of all symbolic labels are recorded in a label symbol map.
2. **Pass 2 (Displacement Fixup)**:
   Jump instructions calculate relative displacement:
   $$\text{disp32} = \text{target\_offset} - (\text{instr\_offset} + \text{instr\_length})$$
   The 32-bit signed displacement is patched in little-endian byte order directly into the machine-code buffer.

---

## 4. ELF64 Relocatable Object Writer (`src/x86_64_elf.c`)

The ELF64 generator constructs standard System V AMD64 relocatable object files (`ET_REL`) with no external dependencies.

### 4.1 Section Layout and Headers
Each generated `.o` file contains 8 structured sections:

| Index | Section Name | Type (`sh_type`) | Flags (`sh_flags`) | Alignment | Purpose |
| :---: | :--- | :--- | :--- | :---: | :--- |
| `0` | *(null)* | `SHT_NULL` | `0` | 0 | Standard ELF undef entry |
| `1` | `.text` | `SHT_PROGBITS` | `SHF_ALLOC \| SHF_EXECINSTR` | 16 | Executable x86-64 machine instructions |
| `2` | `.rela.text` | `SHT_RELA` | `SHF_INFO_LINK` | 8 | Relocation table for `.text` code |
| `3` | `.rodata` | `SHT_PROGBITS` | `SHF_ALLOC` | 16 | String literals & IEEE 754 float constants |
| `4` | `.note.GNU-stack` | `SHT_PROGBITS` | `0` | 1 | Marks stack non-executable (NX bit) |
| `5` | `.symtab` | `SHT_SYMTAB` | `0` | 8 | Symbol table |
| `6` | `.strtab` | `SHT_STRTAB` | `0` | 1 | Symbol string table |
| `7` | `.shstrtab` | `SHT_STRTAB` | `0` | 1 | Section header string table |

### 4.2 Symbol Table Sorting Invariant
ELF specifications require that in `.symtab`:
1. All local symbols (`STB_LOCAL`) must precede all global symbols (`STB_GLOBAL`).
2. The `sh_info` field of the `.symtab` section header must be set to the index of the first non-local (`STB_GLOBAL`) symbol.

The object writer strictly partitions symbols:
* **Local Symbols**: `SHN_UNDEF` null symbol, `.text` section symbol (`STT_SECTION`), `.rodata` section symbol (`STT_SECTION`).
* **Global Symbols**: Function entry points (`STT_FUNC`, `STV_DEFAULT`, `SHN_TEXT`), external standard library functions (`STT_NOTYPE`, `SHN_UNDEF`).

### 4.3 Relocation Formulation
All relocations use explicit addends (`Elf64_Rela`):
* **Function Calls (`R_X86_64_PLT32`)**:
  - Emitted for `call <symbol>` instructions (`0xE8 0x00000000`).
  - Addend is set to `-4` (accounting for the 4-byte length of the displacement itself).
  - Symbol reference points to the external or local function symbol.
* **Read-Only Data References (`R_X86_64_PC32`)**:
  - Emitted for `movsd <literal>(%rip), %xmmN` or `leaq <literal>(%rip), %rax`.
  - Addend is set to `rodata_offset - 4`.
  - Symbol reference points to the `.rodata` section symbol.

---

## 5. CLI Integration & Direct Compilation

The `cco` command-line driver exposes native direct object generation:

```bash
# 1. Compile directly to an ELF64 relocatable object file (no GNU as invoked)
./cco program.cco --emit-object -o program.o

# 2. Compile directly and link to executable via host linker
./cco program.cco --use-native -o program

# 3. Reference assembly generation remains accessible for audit/inspection
./cco program.cco --emit-native -o program.s
```

---

## 6. Verification and Parity Matrix

Phase 3A was validated using a multi-tiered verification harness.

### 6.1 4-Way Differential Verification Engine
The differential engine checks behavioral and output parity across 4 independent pipelines:
1. **Pipeline 1**: Cco Source → AST → C11 Reference Code → GCC → Executable
2. **Pipeline 2**: Cco Source → Cco IR → IR-to-C11 Translation → GCC → Executable
3. **Pipeline 3**: Cco Source → Cco IR → Native AT&T Assembly (`.s`) → GNU `as` + `gcc` → Executable
4. **Pipeline 4**: Cco Source → Cco IR → **Direct x86-64 Machine Encoder** → **ELF64 Object Writer (`.o`)** → `gcc` → Executable

All 4 pipelines were verified across:
* 10 integer stack arguments (`diff_abi_int10`)
* 12 float stack arguments (`diff_abi_float12`)
* 18 mixed integer/float stack arguments (`diff_abi_mixed18`)
* Upper-bit zero extensions for `bool` and `char` (`diff_return_ext`)
* Deeply nested stack calls maintaining 16-byte alignment (`diff_nested_stack_calls`)
* Complex control-flow branches, loops, and early returns (`diff_nested_control_flow`)
* Extreme register pressure with 16 concurrent live ints and 16 live floats (`diff_extreme_pressure`)

### 6.2 Instruction Encoding Unit Verification
53 distinct instruction byte patterns were checked directly against Intel/AMD reference hex encodings in `tests/unit/test_x86_64.c`, covering:
* REX prefix variations (`0x48`, `0x49`, `0x4C`, `0x4D`, etc.)
* Base pointer and stack pointer SIB mechanics (`%rsp`, `%rbp`, `%r12`, `%r13`)
* Short (`disp8`) and long (`disp32`) memory offsets
* SSE2 scalar floating-point opcodes (`movsd`, `addsd`, `subsd`, `mulsd`, `divsd`, `ucomisd`, `xorpd`)
* Immediate boundaries (signed 8-bit vs signed 32-bit)

### 6.3 Standard Binary Tool Validation
Directly generated `.o` files were analyzed with standard Linux binary utilities:
* `readelf -h`: Validated `ELF64`, `2's complement, little endian`, `ET_REL`, machine `Advanced Micro Devices X86-64`.
* `readelf -S`: Validated correct section offsets, alignments, and flags.
* `readelf -s`: Validated symbol sorting (`STB_LOCAL` strictly before `STB_GLOBAL`).
* `readelf -r`: Validated `R_X86_64_PLT32` and `R_X86_64_PC32` relocation entries.
* `objdump -d`: Disassembled machine code confirmed identical instruction stream to `.s` emission.

### 6.4 Determinism & Memory Safety
* **Deterministic Output**: Successive compilations of the same Cco source produce 100% bit-identical `.o` files byte-for-byte.
* **Valgrind Memcheck**:
  - `make unit_tests`: 6/6 test suites passed with **0 leaks, 0 errors**.
  - `make test_selfhost`: 135/135 tests passed.
  - `make test_bootstrap`: 4-stage self-hosted compiler bootstrap passed with **0 leaks, 0 errors**.
  - `bash tests/run_tests.sh`: 111 integration and network tests passed with **0 leaks, 0 FD leaks**.

---

## 7. Roadmap to Phase 3B: Internal Linker

With Phase 3A complete, Cco possesses complete standalone machine-code and relocatable object generation capabilities.

**Next Milestone (Phase 3B)**:
* Implement an internal ELF64 static linker within Cco.
* Read and merge `.text`, `.rodata`, and data sections into contiguous memory blocks.
* Resolve internal `R_X86_64_PLT32` and `R_X86_64_PC32` relocations directly against target virtual addresses.
* Emit executable ELF64 binaries (`ET_EXEC` / `ET_DYN`) with valid `Elf64_Phdr` program headers, removing the final dependency on `gcc` / `ld` for standalone native binaries.
