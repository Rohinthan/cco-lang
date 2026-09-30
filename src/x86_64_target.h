// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X86_64_TARGET_H
#define X86_64_TARGET_H

#include "ir.h"
#include "x86_64_regalloc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* System V AMD64 ABI Definitions                                            */

#define X86_64_MAX_ARG_REGS 6
#define X86_64_MAX_FLOAT_ARG_REGS 8

extern const char * const X86_64_ARG_REGS_64[X86_64_MAX_ARG_REGS];
extern const char * const X86_64_ARG_REGS_32[X86_64_MAX_ARG_REGS];
extern const char * const X86_64_FLOAT_ARG_REGS[X86_64_MAX_FLOAT_ARG_REGS];

/* Stack Frame and Slot Layout                                               */

typedef enum {
    SLOT_VAR,     /* Alloca local variable (%x.addr) */
    SLOT_SPILL,   /* Spilled virtual register */
    SLOT_PARAM    /* Function parameter home slot */
} SlotKind;

typedef struct StackSlot {
    SlotKind kind;
    int id;           /* Virtual register ID or parameter index */
    char *name;       /* Symbolic name (for SLOT_VAR) */
    int offset;       /* Negative offset from %rbp (e.g. -8, -16) */
    int size;         /* Size in bytes (8 bytes) */
    IrType *type;
    struct StackSlot *next;
} StackSlot;

typedef struct X86FunctionFrame {
    IrFunction *fn;
    RegAllocResult *regalloc;
    StackSlot *slots;
    int total_stack_size;      /* Size passed to subq, maintains 16-byte alignment */
    int slot_count;

    /* Callee-saved registers preserved by this function */
    int callee_saved_count;
    X86Reg callee_saved_regs[X86_TOTAL_REG_COUNT];
} X86FunctionFrame;

/* Builds the stack frame layout for an IR function using register allocation.
 * Only allocated locals and actual spills receive stack slots.
 */
X86FunctionFrame *x86_frame_build(IrFunction *fn, RegAllocResult *regalloc);
void x86_frame_free(X86FunctionFrame *frame);

/* Finds the stack offset for a given IR value */
bool x86_frame_get_offset(X86FunctionFrame *frame, IrValue *val, int *out_offset, int *out_size);

/* Finds the spill offset for a virtual register ID */
bool x86_frame_get_spill_offset(X86FunctionFrame *frame, int val_id, int *out_offset);

/* Returns register name based on size: 1 -> "%al", 4 -> "%eax", 8 -> "%rax" */
const char *x86_reg_name(const char *base_reg, int size);

#endif /* X86_64_TARGET_H */
