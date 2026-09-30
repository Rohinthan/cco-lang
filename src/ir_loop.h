// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_LOOP_H
#define IR_LOOP_H

#include "ir.h"
#include "ir_dominance.h"
#include <stdio.h>
#include <stdbool.h>


// Natural Loop Data Structures


typedef struct IrLoop {
    int id;                          /* Deterministic loop ID */
    IrBasicBlock *header;            /* Loop header block */

    IrBasicBlock **latches;          /* Back-edge source blocks */
    int latch_count;
    int latch_cap;

    IrBasicBlock **blocks;           /* All basic blocks in this natural loop */
    int block_count;
    int block_cap;

    struct IrLoop *parent;           /* Enclosing loop, or NULL if top-level */
    struct IrLoop **children;        /* Sub-loops directly nested within this loop */
    int child_count;
    int child_cap;

    int depth;                       /* Nesting depth (1 for outermost loop) */

    /* Phase 8: Profile-guided loop metrics */
    uint64_t entry_count;            /* Measured/estimated loop entries */
    uint64_t iteration_count;        /* Measured/estimated loop iterations */
    double avg_iterations;           /* Average iterations per loop entry */
    bool is_hot_loop;                /* Whether loop is frequently executed */
} IrLoop;

typedef struct IrLoopInfo {
    IrFunction *fn;
    IrLoop **loops;                  /* All natural loops in the function */
    int loop_count;
    int loop_cap;
} IrLoopInfo;


// Loop Analysis API


/* Compute natural loop info for a function using dominance info */
IrLoopInfo *ir_loop_analysis_compute(IrFunction *fn, const IrDomInfo *dom);
void ir_loop_info_free(IrLoopInfo *info);

/* Query loop properties */
int ir_loop_count(const IrLoopInfo *info);
IrLoop *ir_loop_get(const IrLoopInfo *info, int idx);

IrBasicBlock *ir_loop_header(const IrLoop *loop);
bool ir_loop_contains(const IrLoop *loop, const IrBasicBlock *bb);
int ir_loop_depth(const IrLoopInfo *info, const IrBasicBlock *bb);
IrLoop *ir_loop_get_parent(const IrLoop *loop);
IrLoop *ir_loop_for_block(const IrLoopInfo *info, const IrBasicBlock *bb);

/* Find or insert a dedicated loop preheader */
IrBasicBlock *ir_loop_get_or_create_preheader(IrFunction *fn, IrLoop *loop);

/* Deterministic debug dump */
void ir_loop_dump(FILE *out, const IrLoopInfo *info);

/* Phase 8: Profile Annotation & Profile-guided Loop Diagnostics */
struct CcoProfile;
void ir_loop_apply_profile(IrLoopInfo *info, const struct CcoProfile *prof);
void ir_loop_dump_profile(FILE *out, const IrLoopInfo *info);

#endif /* IR_LOOP_H */
