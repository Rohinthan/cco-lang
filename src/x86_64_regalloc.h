// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X86_64_REGALLOC_H
#define X86_64_REGALLOC_H

#include "ir.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Register Classes & Physical Registers for x86-64                         */

typedef enum {
    REG_CLASS_INT,
    REG_CLASS_FLOAT
} RegClass;

typedef enum {
    X86_REG_NONE = -1,
    /* General-Purpose Registers (0..15) */
    X86_REG_RAX = 0,
    X86_REG_RCX = 1,
    X86_REG_RDX = 2,
    X86_REG_RBX = 3,
    X86_REG_RSP = 4,
    X86_REG_RBP = 5,
    X86_REG_RSI = 6,
    X86_REG_RDI = 7,
    X86_REG_R8  = 8,
    X86_REG_R9  = 9,
    X86_REG_R10 = 10,
    X86_REG_R11 = 11,
    X86_REG_R12 = 12,
    X86_REG_R13 = 13,
    X86_REG_R14 = 14,
    X86_REG_R15 = 15,

    /* SSE XMM Registers (16..31) */
    X86_REG_XMM0 = 16,
    X86_REG_XMM1 = 17,
    X86_REG_XMM2 = 18,
    X86_REG_XMM3 = 19,
    X86_REG_XMM4 = 20,
    X86_REG_XMM5 = 21,
    X86_REG_XMM6 = 22,
    X86_REG_XMM7 = 23,
    X86_REG_XMM8 = 24,
    X86_REG_XMM9 = 25,
    X86_REG_XMM10 = 26,
    X86_REG_XMM11 = 27,
    X86_REG_XMM12 = 28,
    X86_REG_XMM13 = 29,
    X86_REG_XMM14 = 30,
    X86_REG_XMM15 = 31,
    X86_TOTAL_REG_COUNT = 32
} X86Reg;

/* Returns standard GNU/AT&T register name by size (1, 2, 4, 8 bytes) */
const char *x86_reg_to_str(X86Reg reg, int size);

/* Returns register class of a physical register */
RegClass x86_reg_class(X86Reg reg);

/* Returns true if the register is callee-saved per System V AMD64 ABI */
bool x86_reg_is_callee_saved(X86Reg reg);

/* Returns true if the register is caller-saved per System V AMD64 ABI */
bool x86_reg_is_caller_saved(X86Reg reg);

/* Liveness Intervals                                                        */

typedef struct LiveInterval {
    int val_id;              /* Virtual register ID */
    IrType *type;
    RegClass reg_class;      /* REG_CLASS_INT or REG_CLASS_FLOAT */
    int start;               /* Instruction index of definition */
    int end;                 /* Instruction index of last use */
    bool crosses_call;       /* True if a call instruction occurs within (start, end) */
    int call_count;          /* Number of calls crossed */
    int *call_positions;     /* Array of instruction indices of crossed calls */

    X86Reg phys_reg;         /* Allocated physical register, or X86_REG_NONE if spilled */
    bool is_spilled;         /* True if assigned to a stack spill slot */
    int spill_offset;        /* Offset relative to %rbp (e.g. -24) */
} LiveInterval;

/* Register Allocation Result                                                */

typedef struct RegAllocResult {
    IrFunction *fn;
    int total_regs;          /* Number of virtual registers (fn->next_reg_id) */
    LiveInterval *intervals; /* Array of intervals [0 .. total_regs - 1] */

    /* Physical register usage */
    bool used_regs[X86_TOTAL_REG_COUNT];
    int callee_saved_used[X86_TOTAL_REG_COUNT];
    int callee_saved_count;

    /* Spilling statistics */
    int spilled_count;
    int int_spill_count;
    int float_spill_count;
    int total_spill_bytes;

    /* Total instructions in function */
    int inst_count;
} RegAllocResult;

/* Runs liveness analysis and type-aware linear scan register allocation on fn */
RegAllocResult *regalloc_run(IrFunction *fn);
void regalloc_free(RegAllocResult *res);

/* Lookup helpers for code generation */
X86Reg regalloc_get_phys_reg(RegAllocResult *res, int val_id);
bool regalloc_is_spilled(RegAllocResult *res, int val_id);
int regalloc_get_spill_offset(RegAllocResult *res, int val_id);

/* Debug dumping */
void regalloc_dump(RegAllocResult *res, FILE *out);

#endif /* X86_64_REGALLOC_H */
