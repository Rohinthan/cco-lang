// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_target.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char * const X86_64_ARG_REGS_64[X86_64_MAX_ARG_REGS] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};

const char * const X86_64_ARG_REGS_32[X86_64_MAX_ARG_REGS] = {
    "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"
};

const char * const X86_64_FLOAT_ARG_REGS[X86_64_MAX_FLOAT_ARG_REGS] = {
    "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
};

static StackSlot *find_var_slot(X86FunctionFrame *frame, const char *name) {
    if (!name) return NULL;
    for (StackSlot *s = frame->slots; s; s = s->next) {
        if (s->kind == SLOT_VAR && s->name && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static StackSlot *find_spill_slot(X86FunctionFrame *frame, int id) {
    for (StackSlot *s = frame->slots; s; s = s->next) {
        if (s->kind == SLOT_SPILL && s->id == id) {
            return s;
        }
    }
    return NULL;
}

static StackSlot *find_param_slot(X86FunctionFrame *frame, int id) {
    for (StackSlot *s = frame->slots; s; s = s->next) {
        if (s->kind == SLOT_PARAM && s->id == id) {
            return s;
        }
    }
    return NULL;
}

static StackSlot *add_fixed_slot(X86FunctionFrame *frame, SlotKind kind, int id, const char *name, IrType *type, int fixed_offset) {
    StackSlot *slot = (StackSlot *)malloc(sizeof(StackSlot));
    slot->kind = kind;
    slot->id = id;
    slot->name = name ? strdup(name) : NULL;
    slot->type = type;
    slot->size = 8;
    slot->offset = fixed_offset;

    slot->next = frame->slots;
    frame->slots = slot;
    frame->slot_count++;
    return slot;
}

static StackSlot *add_slot(X86FunctionFrame *frame, SlotKind kind, int id, const char *name, IrType *type, int *offset_counter) {
    StackSlot *slot = (StackSlot *)malloc(sizeof(StackSlot));
    slot->kind = kind;
    slot->id = id;
    slot->name = name ? strdup(name) : NULL;
    slot->type = type;
    slot->size = 8; /* Every slot on stack is 8-byte aligned for safety */
    *offset_counter += 8;
    slot->offset = -(*offset_counter);

    slot->next = frame->slots;
    frame->slots = slot;
    frame->slot_count++;
    return slot;
}

X86FunctionFrame *x86_frame_build(IrFunction *fn, RegAllocResult *regalloc) {
    if (!fn) return NULL;

    X86FunctionFrame *frame = (X86FunctionFrame *)calloc(1, sizeof(X86FunctionFrame));
    frame->fn = fn;
    frame->regalloc = regalloc;
    frame->slots = NULL;
    frame->slot_count = 0;
    frame->total_stack_size = 0;

    /* Copy callee-saved registers used */
    if (regalloc) {
        frame->callee_saved_count = regalloc->callee_saved_count;
        for (int i = 0; i < regalloc->callee_saved_count; i++) {
            frame->callee_saved_regs[i] = regalloc->callee_saved_used[i];
        }
    }

    /* Account for callee-saved registers pushed in prologue */
    int K = frame->callee_saved_count;
    int offset_counter = K * 8;

    /* 1. Allocate slots for incoming function parameters according to System V AMD64 ABI */
    int int_arg_idx = 0;
    int float_arg_idx = 0;
    int stack_arg_idx = 0;

    for (int p = 0; p < fn->param_count; p++) {
        IrType *ptype = fn->param_types[p];
        bool is_float = (ptype && ptype->kind == IR_TYPE_F64);
        if (is_float) {
            if (float_arg_idx < X86_64_MAX_FLOAT_ARG_REGS) {
                float_arg_idx++;
                add_slot(frame, SLOT_PARAM, p, fn->param_names[p], ptype, &offset_counter);
            } else {
                /* System V AMD64: Stack argument at positive offset: 16(%rbp), 24(%rbp), etc. */
                int stack_offset = 16 + stack_arg_idx * 8;
                stack_arg_idx++;
                add_fixed_slot(frame, SLOT_PARAM, p, fn->param_names[p], ptype, stack_offset);
            }
        } else {
            if (int_arg_idx < X86_64_MAX_ARG_REGS) {
                int_arg_idx++;
                add_slot(frame, SLOT_PARAM, p, fn->param_names[p], ptype, &offset_counter);
            } else {
                /* System V AMD64: Stack argument at positive offset: 16(%rbp), 24(%rbp), etc. */
                int stack_offset = 16 + stack_arg_idx * 8;
                stack_arg_idx++;
                add_fixed_slot(frame, SLOT_PARAM, p, fn->param_names[p], ptype, stack_offset);
            }
        }
    }

    /* 2. Allocate slots for local variables (allocas) */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_ALLOCA && inst->result && inst->result->kind == IR_VAL_VAR) {
                if (!find_var_slot(frame, inst->result->name)) {
                    IrType *elem = inst->result->type->elem_type ? inst->result->type->elem_type : inst->result->type;
                    add_slot(frame, SLOT_VAR, -1, inst->result->name, elem, &offset_counter);
                }
            }
        }
    }

    /* 3. Allocate spill slots ONLY for actually spilled virtual registers or caller-saved crossing calls */
    if (regalloc) {
        for (int v = 0; v < regalloc->total_regs; v++) {
            LiveInterval *inv = &regalloc->intervals[v];
            bool needs_spill = inv->is_spilled || (inv->crosses_call && x86_reg_is_caller_saved(inv->phys_reg));
            if (needs_spill) {
                StackSlot *ss = add_slot(frame, SLOT_SPILL, v, NULL, inv->type, &offset_counter);
                inv->spill_offset = ss->offset;
            }
        }
    } else {
        /* Fallback if regalloc is NULL (Phase 2A compatibility) */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->result && inst->result->kind == IR_VAL_REG) {
                    if (!find_spill_slot(frame, inst->result->id)) {
                        add_slot(frame, SLOT_SPILL, inst->result->id, NULL, inst->result->type, &offset_counter);
                    }
                }
            }
        }
    }

    /* Calculate 16-byte alignment taking into account K pushed callee-saved registers.
     * Stack alignment formula:
     * Total stack adjustment from entry must be a multiple of 16 immediately before any call:
     * RetAddr (8) + %rbp (8) + K*8 + FrameSize = 16 + K*8 + FrameSize.
     * We require (K*8 + FrameSize) % 16 == 0.
     */
    int raw_locals = offset_counter - K * 8;
    int total_needed = K * 8 + raw_locals;
    int total_aligned = (total_needed + 15) & ~15;
    frame->total_stack_size = total_aligned - K * 8;

    return frame;
}

void x86_frame_free(X86FunctionFrame *frame) {
    if (!frame) return;
    StackSlot *curr = frame->slots;
    while (curr) {
        StackSlot *next = curr->next;
        if (curr->name) free(curr->name);
        free(curr);
        curr = next;
    }
    free(frame);
}

bool x86_frame_get_offset(X86FunctionFrame *frame, IrValue *val, int *out_offset, int *out_size) {
    if (!frame || !val) return false;

    StackSlot *slot = NULL;
    switch (val->kind) {
        case IR_VAL_VAR:
            slot = find_var_slot(frame, val->name);
            break;
        case IR_VAL_REG:
            slot = find_spill_slot(frame, val->id);
            break;
        case IR_VAL_PARAM:
            slot = find_param_slot(frame, val->id);
            if (!slot && val->name) {
                for (StackSlot *s = frame->slots; s; s = s->next) {
                    if (s->kind == SLOT_PARAM && s->name && strcmp(s->name, val->name) == 0) {
                        slot = s;
                        break;
                    }
                }
            }
            break;
        default:
            return false;
    }

    if (slot) {
        if (out_offset) *out_offset = slot->offset;
        if (out_size) *out_size = slot->size;
        return true;
    }
    return false;
}

bool x86_frame_get_spill_offset(X86FunctionFrame *frame, int val_id, int *out_offset) {
    if (!frame) return false;
    StackSlot *s = find_spill_slot(frame, val_id);
    if (s) {
        if (out_offset) *out_offset = s->offset;
        return true;
    }
    return false;
}

const char *x86_reg_name(const char *base_reg, int size) {
    if (!base_reg) return "%rax";
    if (strcmp(base_reg, "a") == 0) {
        if (size == 1) return "%al";
        if (size == 2) return "%ax";
        if (size == 4) return "%eax";
        return "%rax";
    }
    if (strcmp(base_reg, "c") == 0) {
        if (size == 1) return "%cl";
        if (size == 2) return "%cx";
        if (size == 4) return "%ecx";
        return "%rcx";
    }
    if (strcmp(base_reg, "d") == 0) {
        if (size == 1) return "%dl";
        if (size == 2) return "%dx";
        if (size == 4) return "%edx";
        return "%rdx";
    }
    return "%rax";
}
