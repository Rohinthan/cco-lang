// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_DOMINANCE_H
#define IR_DOMINANCE_H

#include "ir.h"
#include <stdio.h>
#include <stdbool.h>

// Dominance Data Structures

typedef struct IrDomNode {
    IrBasicBlock *block;
    int index;                       /* Dense index 0 <= index < block_count */
    bool reachable;

    struct IrDomNode *idom;          /* Immediate dominator (NULL for entry/unreachable) */
    struct IrDomNode **children;     /* Dominator tree children */
    int child_count;
    int child_cap;
    int depth;                       /* Depth in dominator tree (0 for entry) */

    /* Dominance sets: dom_set[i] == true iff this node is dominated by block i */
    bool *dom_by;                    /* Blocks that dominate this node */

    /* Dominance Frontier */
    IrBasicBlock **df;
    int df_count;
    int df_cap;
} IrDomNode;

typedef struct IrDomInfo {
    IrFunction *fn;
    IrBasicBlock **blocks;           /* Dense array of blocks in function */
    int block_count;
    IrDomNode **nodes;               /* Dense array of dom nodes matching blocks */
    IrDomNode *root;                 /* Entry block dom node */
} IrDomInfo;

/* Dominator Computation & Lifecycle */

IrDomInfo *ir_dominance_compute(IrFunction *fn);
void ir_dominance_free(IrDomInfo *dom);


// Dominance Queries

/* Returns true if block 'a' dominates block 'b' (a dom b) */
bool ir_dominates(const IrDomInfo *dom, const IrBasicBlock *a, const IrBasicBlock *b);

/* Returns true if block 'a' strictly dominates block 'b' (a dom b and a != b) */
bool ir_strictly_dominates(const IrDomInfo *dom, const IrBasicBlock *a, const IrBasicBlock *b);

/* Returns immediate dominator block of 'b', or NULL if none */
IrBasicBlock *ir_get_idom(const IrDomInfo *dom, const IrBasicBlock *b);

/* Returns dominance frontier of block 'b' */
IrBasicBlock *const *ir_get_dominance_frontier(const IrDomInfo *dom, const IrBasicBlock *b, int *out_count);


// Verification & Diagnostics


/* Independently verifies dominator tree and dominance frontier properties */
bool ir_dominance_verify(const IrDomInfo *dom, char **out_error);

/* Debug dump of dominator tree and dominance frontiers */
void ir_dominance_dump(FILE *out, const IrDomInfo *dom);

#endif /* IR_DOMINANCE_H */
