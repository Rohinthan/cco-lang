// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_regalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Register String Names                                                     */

static const char * const REG_NAMES_64[16] = {
    "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
    "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
};

static const char * const REG_NAMES_32[16] = {
    "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi",
    "%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d"
};

static const char * const REG_NAMES_8[16] = {
    "%al",   "%cl",   "%dl",   "%bl",   "%spl",  "%bpl",  "%sil",  "%dil",
    "%r8b",  "%r9b",  "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b"
};

static const char * const XMM_NAMES[16] = {
    "%xmm0",  "%xmm1",  "%xmm2",  "%xmm3",
    "%xmm4",  "%xmm5",  "%xmm6",  "%xmm7",
    "%xmm8",  "%xmm9",  "%xmm10", "%xmm11",
    "%xmm12", "%xmm13", "%xmm14", "%xmm15"
};

const char *x86_reg_to_str(X86Reg reg, int size) {
    if (reg < 0 || reg >= X86_TOTAL_REG_COUNT) return "%rax";
    if (reg >= X86_REG_XMM0 && reg <= X86_REG_XMM15) {
        return XMM_NAMES[reg - X86_REG_XMM0];
    }
    if (size == 1) return REG_NAMES_8[reg];
    if (size == 4) return REG_NAMES_32[reg];
    return REG_NAMES_64[reg];
}

RegClass x86_reg_class(X86Reg reg) {
    if (reg >= X86_REG_XMM0 && reg <= X86_REG_XMM15) {
        return REG_CLASS_FLOAT;
    }
    return REG_CLASS_INT;
}

bool x86_reg_is_callee_saved(X86Reg reg) {
    switch (reg) {
        case X86_REG_RBX:
        case X86_REG_R12:
        case X86_REG_R13:
        case X86_REG_R14:
        case X86_REG_R15:
            return true;
        default:
            /* Note: All XMM registers are caller-saved in System V AMD64 ABI */
            return false;
    }
}

bool x86_reg_is_caller_saved(X86Reg reg) {
    if (reg < 0 || reg >= X86_TOTAL_REG_COUNT) return false;
    return !x86_reg_is_callee_saved(reg) && reg != X86_REG_RSP && reg != X86_REG_RBP;
}

/* Register Pools                                                            */

/* Integer Pools:
 * Caller-saved: %r10, %r11 (disjoint from argument registers)
 * Callee-saved: %rbx, %r12, %r13, %r14, %r15
 */
static const X86Reg CALLER_SAVED_INT_POOL[] = {
    X86_REG_R10, X86_REG_R11
};
#define CALLER_SAVED_INT_COUNT (sizeof(CALLER_SAVED_INT_POOL) / sizeof(CALLER_SAVED_INT_POOL[0]))

static const X86Reg CALLEE_SAVED_INT_POOL[] = {
    X86_REG_RBX, X86_REG_R12, X86_REG_R13, X86_REG_R14, X86_REG_R15
};
#define CALLEE_SAVED_INT_COUNT (sizeof(CALLEE_SAVED_INT_POOL) / sizeof(CALLEE_SAVED_INT_POOL[0]))

/* Floating-Point Pool:
 * Allocatable XMM: %xmm8..%xmm13 (non-argument registers) + %xmm0..%xmm7 (argument registers)
 * Reserved XMM scratch: %xmm14, %xmm15
 */
static const X86Reg FLOAT_POOL[] = {
    X86_REG_XMM8, X86_REG_XMM9, X86_REG_XMM10, X86_REG_XMM11, X86_REG_XMM12, X86_REG_XMM13,
    X86_REG_XMM0, X86_REG_XMM1, X86_REG_XMM2,  X86_REG_XMM3,  X86_REG_XMM4,  X86_REG_XMM5,
    X86_REG_XMM6, X86_REG_XMM7
};
#define FLOAT_POOL_COUNT (sizeof(FLOAT_POOL) / sizeof(FLOAT_POOL[0]))

/* Liveness Analysis Helpers                                                 */

typedef struct BlockLiveness {
    IrBasicBlock *bb;
    int start_inst;
    int end_inst;
    bool *use;
    bool *def;
    bool *live_in;
    bool *live_out;
} BlockLiveness;

static bool is_call_op(IrOpcode op) {
    return (op == IR_OP_CALL || op == IR_OP_PRINT || op == IR_OP_FREE || op == IR_OP_RELEASE);
}

static void mark_use(BlockLiveness *bl, IrValue *val, int total_regs) {
    if (val && val->kind == IR_VAL_REG && val->id >= 0 && val->id < total_regs) {
        if (!bl->def[val->id]) {
            bl->use[val->id] = true;
        }
    }
}

static void mark_def(BlockLiveness *bl, IrValue *val, int total_regs) {
    if (val && val->kind == IR_VAL_REG && val->id >= 0 && val->id < total_regs) {
        bl->def[val->id] = true;
    }
}

/* Linear Scan Allocation                                                    */

typedef struct IntervalRef {
    LiveInterval *interval;
} IntervalRef;

static int compare_start_pos(const void *a, const void *b) {
    const IntervalRef *ia = (const IntervalRef *)a;
    const IntervalRef *ib = (const IntervalRef *)b;
    if (ia->interval->start != ib->interval->start) {
        return ia->interval->start - ib->interval->start;
    }
    return ia->interval->val_id - ib->interval->val_id;
}

RegAllocResult *regalloc_run(IrFunction *fn) {
    if (!fn) return NULL;

    RegAllocResult *res = (RegAllocResult *)calloc(1, sizeof(RegAllocResult));
    res->fn = fn;
    res->total_regs = fn->next_reg_id;
    if (res->total_regs <= 0) {
        return res;
    }

    res->intervals = (LiveInterval *)calloc(res->total_regs, sizeof(LiveInterval));
    for (int i = 0; i < res->total_regs; i++) {
        res->intervals[i].val_id = i;
        res->intervals[i].reg_class = REG_CLASS_INT;
        res->intervals[i].start = INT_MAX;
        res->intervals[i].end = -1;
        res->intervals[i].phys_reg = X86_REG_NONE;
        res->intervals[i].is_spilled = false;
        res->intervals[i].spill_offset = 0;
        res->intervals[i].crosses_call = false;
        res->intervals[i].call_count = 0;
        res->intervals[i].call_positions = NULL;
    }

    /* 1. Count basic blocks and deterministic instruction numbering */
    int block_count = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        block_count++;
    }

    BlockLiveness *bls = (BlockLiveness *)calloc(block_count, sizeof(BlockLiveness));
    int total_insts = 0;

    int b_idx = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next, b_idx++) {
        bls[b_idx].bb = bb;
        bls[b_idx].start_inst = total_insts;
        bls[b_idx].use = (bool *)calloc(res->total_regs, sizeof(bool));
        bls[b_idx].def = (bool *)calloc(res->total_regs, sizeof(bool));
        bls[b_idx].live_in = (bool *)calloc(res->total_regs, sizeof(bool));
        bls[b_idx].live_out = (bool *)calloc(res->total_regs, sizeof(bool));

        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            /* Check operands used */
            if (inst->lhs) mark_use(&bls[b_idx], inst->lhs, res->total_regs);
            if (inst->rhs) mark_use(&bls[b_idx], inst->rhs, res->total_regs);
            for (int a = 0; a < inst->arg_count; a++) {
                if (inst->args[a]) mark_use(&bls[b_idx], inst->args[a], res->total_regs);
            }

            /* Check definition */
            if (inst->result) {
                mark_def(&bls[b_idx], inst->result, res->total_regs);
                if (inst->result->kind == IR_VAL_REG && inst->result->id >= 0 && inst->result->id < res->total_regs) {
                    res->intervals[inst->result->id].type = inst->result->type;
                    if (inst->result->type && inst->result->type->kind == IR_TYPE_F64) {
                        res->intervals[inst->result->id].reg_class = REG_CLASS_FLOAT;
                    } else {
                        res->intervals[inst->result->id].reg_class = REG_CLASS_INT;
                    }
                }
            }

            total_insts++;
        }
        bls[b_idx].end_inst = (total_insts > bls[b_idx].start_inst) ? (total_insts - 1) : bls[b_idx].start_inst;
    }
    res->inst_count = total_insts;

    /* 2. Record instruction pointers and call positions */
    IrInstruction **instructions = (IrInstruction **)malloc(sizeof(IrInstruction *) * (total_insts > 0 ? total_insts : 1));
    int *call_positions = (int *)malloc(sizeof(int) * (total_insts > 0 ? total_insts : 1));
    int call_count = 0;

    int cur_idx = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            instructions[cur_idx] = inst;
            if (is_call_op(inst->op)) {
                call_positions[call_count++] = cur_idx;
            }
            cur_idx++;
        }
    }

    /* 3. Backwards dataflow analysis for basic block liveness */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = block_count - 1; b >= 0; b--) {
            IrBasicBlock *bb = bls[b].bb;

            /* live_out[b] = union of live_in[s] for all successors */
            for (int s = 0; s < bb->succ_count; s++) {
                IrBasicBlock *succ = bb->successors[s];
                int succ_idx = -1;
                for (int sb = 0; sb < block_count; sb++) {
                    if (bls[sb].bb == succ) {
                        succ_idx = sb;
                        break;
                    }
                }
                if (succ_idx >= 0) {
                    for (int v = 0; v < res->total_regs; v++) {
                        if (bls[succ_idx].live_in[v] && !bls[b].live_out[v]) {
                            bls[b].live_out[v] = true;
                            changed = true;
                        }
                    }
                }
            }

            /* live_in[b] = use[b] | (live_out[b] & ~def[b]) */
            for (int v = 0; v < res->total_regs; v++) {
                bool in_val = bls[b].use[v] || (bls[b].live_out[v] && !bls[b].def[v]);
                if (in_val != bls[b].live_in[v]) {
                    bls[b].live_in[v] = in_val;
                    changed = true;
                }
            }
        }
    }

    /* 4. Construct live intervals combining block liveness and intra-block instruction index bounds */
    for (int b = 0; b < block_count; b++) {
        for (int v = 0; v < res->total_regs; v++) {
            if (bls[b].live_in[v]) {
                if (bls[b].start_inst < res->intervals[v].start) {
                    res->intervals[v].start = bls[b].start_inst;
                }
                if (bls[b].start_inst > res->intervals[v].end) {
                    res->intervals[v].end = bls[b].start_inst;
                }
            }
            if (bls[b].live_out[v]) {
                if (bls[b].end_inst > res->intervals[v].end) {
                    res->intervals[v].end = bls[b].end_inst;
                }
            }
        }
    }

    /* Intra-block scan for definitions and uses */
    for (int i = 0; i < total_insts; i++) {
        IrInstruction *inst = instructions[i];
        if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id >= 0 && inst->result->id < res->total_regs) {
            int v = inst->result->id;
            if (i < res->intervals[v].start) res->intervals[v].start = i;
            if (i > res->intervals[v].end) res->intervals[v].end = i;
        }

        #define CHECK_USE(val_ptr) do { \
            if ((val_ptr) && (val_ptr)->kind == IR_VAL_REG && (val_ptr)->id >= 0 && (val_ptr)->id < res->total_regs) { \
                int v = (val_ptr)->id; \
                if (i > res->intervals[v].end) res->intervals[v].end = i; \
                if (res->intervals[v].start == INT_MAX) res->intervals[v].start = i; \
            } \
        } while (0)

        CHECK_USE(inst->lhs);
        CHECK_USE(inst->rhs);
        for (int a = 0; a < inst->arg_count; a++) {
            CHECK_USE(inst->args[a]);
        }
        #undef CHECK_USE
    }

    /* Normalize intervals and identify call crossings */
    for (int v = 0; v < res->total_regs; v++) {
        if (res->intervals[v].start == INT_MAX) {
            res->intervals[v].start = 0;
            res->intervals[v].end = 0;
        }
        if (res->intervals[v].end < res->intervals[v].start) {
            res->intervals[v].end = res->intervals[v].start;
        }

        /* Check which calls fall strictly inside (start, end) */
        int crossed[256];
        int num_crossed = 0;
        for (int c = 0; c < call_count && num_crossed < 256; c++) {
            if (res->intervals[v].start < call_positions[c] && res->intervals[v].end > call_positions[c]) {
                crossed[num_crossed++] = call_positions[c];
            }
        }
        if (num_crossed > 0) {
            res->intervals[v].crosses_call = true;
            res->intervals[v].call_count = num_crossed;
            res->intervals[v].call_positions = (int *)malloc(sizeof(int) * num_crossed);
            memcpy(res->intervals[v].call_positions, crossed, sizeof(int) * num_crossed);
        }
    }

    /* 5. Linear Scan Register Allocation */
    IntervalRef *sorted_intervals = (IntervalRef *)malloc(sizeof(IntervalRef) * res->total_regs);
    for (int v = 0; v < res->total_regs; v++) {
        sorted_intervals[v].interval = &res->intervals[v];
    }
    qsort(sorted_intervals, res->total_regs, sizeof(IntervalRef), compare_start_pos);

    /* Track active intervals */
    LiveInterval *active[X86_TOTAL_REG_COUNT * 2];
    int active_count = 0;

    bool reg_busy[X86_TOTAL_REG_COUNT];
    memset(reg_busy, 0, sizeof(reg_busy));

    for (int i = 0; i < res->total_regs; i++) {
        LiveInterval *curr = sorted_intervals[i].interval;

        /* Expire old intervals */
        for (int a = 0; a < active_count; ) {
            if (active[a]->end < curr->start) {
                if (active[a]->phys_reg != X86_REG_NONE) {
                    reg_busy[active[a]->phys_reg] = false;
                }
                /* Remove from active */
                for (int k = a; k < active_count - 1; k++) {
                    active[k] = active[k + 1];
                }
                active_count--;
            } else {
                a++;
            }
        }

        X86Reg chosen_reg = X86_REG_NONE;

        if (curr->reg_class == REG_CLASS_FLOAT) {
            /* Floating-point register allocation from FLOAT_POOL */
            for (size_t c = 0; c < FLOAT_POOL_COUNT; c++) {
                X86Reg r = FLOAT_POOL[c];
                if (!reg_busy[r]) {
                    chosen_reg = r;
                    break;
                }
            }

            if (chosen_reg != X86_REG_NONE) {
                curr->phys_reg = chosen_reg;
                reg_busy[chosen_reg] = true;
                res->used_regs[chosen_reg] = true;

                /* Insert into active list sorted by end position */
                int insert_pos = active_count;
                for (int a = 0; a < active_count; a++) {
                    if (active[a]->end > curr->end) {
                        insert_pos = a;
                        break;
                    }
                }
                for (int k = active_count; k > insert_pos; k--) {
                    active[k] = active[k - 1];
                }
                active[insert_pos] = curr;
                active_count++;
            } else {
                /* Spill: Find the candidate of REG_CLASS_FLOAT with furthest end position */
                int victim_idx = -1;
                for (int a = active_count - 1; a >= 0; a--) {
                    if (active[a]->reg_class == REG_CLASS_FLOAT) {
                        victim_idx = a;
                        break;
                    }
                }

                if (victim_idx >= 0 && active[victim_idx]->end > curr->end) {
                    LiveInterval *victim = active[victim_idx];
                    curr->phys_reg = victim->phys_reg;
                    victim->phys_reg = X86_REG_NONE;
                    victim->is_spilled = true;

                    /* Remove victim */
                    for (int k = victim_idx; k < active_count - 1; k++) {
                        active[k] = active[k + 1];
                    }
                    active_count--;

                    /* Insert curr into active */
                    int insert_pos = active_count;
                    for (int a = 0; a < active_count; a++) {
                        if (active[a]->end > curr->end) {
                            insert_pos = a;
                            break;
                        }
                    }
                    for (int k = active_count; k > insert_pos; k--) {
                        active[k] = active[k - 1];
                    }
                    active[insert_pos] = curr;
                    active_count++;
                } else {
                    curr->phys_reg = X86_REG_NONE;
                    curr->is_spilled = true;
                }
            }
        } else {
            /* Integer/Pointer allocation from GPR pools */
            if (curr->crosses_call) {
                for (size_t c = 0; c < CALLEE_SAVED_INT_COUNT; c++) {
                    X86Reg r = CALLEE_SAVED_INT_POOL[c];
                    if (!reg_busy[r]) {
                        chosen_reg = r;
                        break;
                    }
                }
                if (chosen_reg == X86_REG_NONE) {
                    for (size_t c = 0; c < CALLER_SAVED_INT_COUNT; c++) {
                        X86Reg r = CALLER_SAVED_INT_POOL[c];
                        if (!reg_busy[r]) {
                            chosen_reg = r;
                            break;
                        }
                    }
                }
            } else {
                for (size_t c = 0; c < CALLER_SAVED_INT_COUNT; c++) {
                    X86Reg r = CALLER_SAVED_INT_POOL[c];
                    if (!reg_busy[r]) {
                        chosen_reg = r;
                        break;
                    }
                }
                if (chosen_reg == X86_REG_NONE) {
                    for (size_t c = 0; c < CALLEE_SAVED_INT_COUNT; c++) {
                        X86Reg r = CALLEE_SAVED_INT_POOL[c];
                        if (!reg_busy[r]) {
                            chosen_reg = r;
                            break;
                        }
                    }
                }
            }

            if (chosen_reg != X86_REG_NONE) {
                curr->phys_reg = chosen_reg;
                reg_busy[chosen_reg] = true;
                res->used_regs[chosen_reg] = true;

                int insert_pos = active_count;
                for (int a = 0; a < active_count; a++) {
                    if (active[a]->end > curr->end) {
                        insert_pos = a;
                        break;
                    }
                }
                for (int k = active_count; k > insert_pos; k--) {
                    active[k] = active[k - 1];
                }
                active[insert_pos] = curr;
                active_count++;
            } else {
                /* Spill: Find victim candidate of REG_CLASS_INT with furthest end position */
                int victim_idx = -1;
                for (int a = active_count - 1; a >= 0; a--) {
                    if (active[a]->reg_class == REG_CLASS_INT) {
                        victim_idx = a;
                        break;
                    }
                }

                if (victim_idx >= 0 && active[victim_idx]->end > curr->end) {
                    LiveInterval *victim = active[victim_idx];
                    curr->phys_reg = victim->phys_reg;
                    victim->phys_reg = X86_REG_NONE;
                    victim->is_spilled = true;

                    for (int k = victim_idx; k < active_count - 1; k++) {
                        active[k] = active[k + 1];
                    }
                    active_count--;

                    int insert_pos = active_count;
                    for (int a = 0; a < active_count; a++) {
                        if (active[a]->end > curr->end) {
                            insert_pos = a;
                            break;
                        }
                    }
                    for (int k = active_count; k > insert_pos; k--) {
                        active[k] = active[k - 1];
                    }
                    active[insert_pos] = curr;
                    active_count++;
                } else {
                    curr->phys_reg = X86_REG_NONE;
                    curr->is_spilled = true;
                }
            }
        }
    }

    /* 6. Identify callee-saved registers actually used */
    res->callee_saved_count = 0;
    for (size_t c = 0; c < CALLEE_SAVED_INT_COUNT; c++) {
        X86Reg r = CALLEE_SAVED_INT_POOL[c];
        if (res->used_regs[r]) {
            res->callee_saved_used[res->callee_saved_count++] = r;
        }
    }

    /* Count spilled intervals and allocate spill slots */
    res->spilled_count = 0;
    res->int_spill_count = 0;
    res->float_spill_count = 0;
    for (int v = 0; v < res->total_regs; v++) {
        LiveInterval *inv = &res->intervals[v];
        if (inv->is_spilled || (inv->crosses_call && x86_reg_is_caller_saved(inv->phys_reg))) {
            res->spilled_count++;
            if (inv->reg_class == REG_CLASS_FLOAT) {
                res->float_spill_count++;
            } else {
                res->int_spill_count++;
            }
        }
    }
    res->total_spill_bytes = res->spilled_count * 8;

    /* Clean up temporary dataflow and instruction tracking structures */
    for (int b = 0; b < block_count; b++) {
        free(bls[b].use);
        free(bls[b].def);
        free(bls[b].live_in);
        free(bls[b].live_out);
    }
    free(bls);
    free(instructions);
    free(call_positions);
    free(sorted_intervals);

    return res;
}

void regalloc_free(RegAllocResult *res) {
    if (!res) return;
    if (res->intervals) {
        for (int i = 0; i < res->total_regs; i++) {
            if (res->intervals[i].call_positions) {
                free(res->intervals[i].call_positions);
            }
        }
        free(res->intervals);
    }
    free(res);
}

X86Reg regalloc_get_phys_reg(RegAllocResult *res, int val_id) {
    if (!res || val_id < 0 || val_id >= res->total_regs) return X86_REG_NONE;
    return res->intervals[val_id].phys_reg;
}

bool regalloc_is_spilled(RegAllocResult *res, int val_id) {
    if (!res || val_id < 0 || val_id >= res->total_regs) return false;
    return res->intervals[val_id].is_spilled;
}

int regalloc_get_spill_offset(RegAllocResult *res, int val_id) {
    if (!res || val_id < 0 || val_id >= res->total_regs) return 0;
    return res->intervals[val_id].spill_offset;
}

void regalloc_dump(RegAllocResult *res, FILE *out) {
    if (!res || !out) return;
    fprintf(out, "=== Register Allocation for Function '%s' (Total Virtual Regs: %d, Total Insts: %d) ===\n",
            res->fn->name ? res->fn->name : "anon", res->total_regs, res->inst_count);
    fprintf(out, "Callee-saved used (%d):", res->callee_saved_count);
    for (int i = 0; i < res->callee_saved_count; i++) {
        fprintf(out, " %s", x86_reg_to_str(res->callee_saved_used[i], 8));
    }
    fprintf(out, "\nSpilled count: %d (INT: %d, FLOAT: %d, Spill bytes: %d)\n",
            res->spilled_count, res->int_spill_count, res->float_spill_count, res->total_spill_bytes);

    for (int i = 0; i < res->total_regs; i++) {
        LiveInterval *inv = &res->intervals[i];
        const char *cls_str = (inv->reg_class == REG_CLASS_FLOAT) ? "FLOAT" : "INTEGER";
        fprintf(out, "  %%%d: class=%s [%d, %d] -> ", inv->val_id, cls_str, inv->start, inv->end);
        if (inv->phys_reg != X86_REG_NONE) {
            fprintf(out, "%s", x86_reg_to_str(inv->phys_reg, 8));
            if (inv->crosses_call) {
                if (x86_reg_is_caller_saved(inv->phys_reg)) {
                    fprintf(out, " (crosses %d calls, caller-save spill [%d(%%rbp)])", inv->call_count, inv->spill_offset);
                } else {
                    fprintf(out, " (crosses %d calls, callee-preserved)", inv->call_count);
                }
            }
        } else {
            fprintf(out, "SPILL[%d(%%rbp)]", inv->spill_offset);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "=================================================================================\n");
}
