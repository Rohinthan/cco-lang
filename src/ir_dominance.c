// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_dominance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/* Helper Functions */

static int get_block_index(const IrDomInfo *dom, const IrBasicBlock *bb) {
    if (!dom || !bb) return -1;
    for (int i = 0; i < dom->block_count; i++) {
        if (dom->blocks[i] == bb) return i;
    }
    return -1;
}

static void add_child(IrDomNode *parent, IrDomNode *child) {
    if (parent->child_count >= parent->child_cap) {
        int new_cap = parent->child_cap == 0 ? 4 : parent->child_cap * 2;
        parent->children = (IrDomNode **)realloc(parent->children, new_cap * sizeof(IrDomNode *));
        parent->child_cap = new_cap;
    }
    parent->children[parent->child_count++] = child;
}

static void add_to_df(IrDomNode *node, IrBasicBlock *bb) {
    for (int i = 0; i < node->df_count; i++) {
        if (node->df[i] == bb) return; /* Already in DF */
    }
    if (node->df_count >= node->df_cap) {
        int new_cap = node->df_cap == 0 ? 4 : node->df_cap * 2;
        node->df = (IrBasicBlock **)realloc(node->df, new_cap * sizeof(IrBasicBlock *));
        node->df_cap = new_cap;
    }
    node->df[node->df_count++] = bb;
}

static void compute_depths_recursive(IrDomNode *node, int curr_depth) {
    if (!node) return;
    node->depth = curr_depth;
    for (int i = 0; i < node->child_count; i++) {
        compute_depths_recursive(node->children[i], curr_depth + 1);
    }
}

/* Dominance Computation Implementation */

IrDomInfo *ir_dominance_compute(IrFunction *fn) {
    if (!fn || !fn->first_block) return NULL;

    IrDomInfo *dom = (IrDomInfo *)calloc(1, sizeof(IrDomInfo));
    dom->fn = fn;

    /* 1. Collect dense array of basic blocks */
    dom->block_count = fn->block_count;
    dom->blocks = (IrBasicBlock **)malloc(dom->block_count * sizeof(IrBasicBlock *));
    dom->nodes = (IrDomNode **)calloc(dom->block_count, sizeof(IrDomNode *));

    int idx = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        dom->blocks[idx] = bb;
        IrDomNode *n = (IrDomNode *)calloc(1, sizeof(IrDomNode));
        n->block = bb;
        n->index = idx;
        n->dom_by = (bool *)calloc(dom->block_count, sizeof(bool));
        dom->nodes[idx] = n;
        idx++;
    }

    int entry_idx = get_block_index(dom, fn->entry_block ? fn->entry_block : fn->first_block);
    if (entry_idx < 0) entry_idx = 0;
    dom->root = dom->nodes[entry_idx];

    /* 2. Compute reachability via BFS from entry */
    IrDomNode **queue = (IrDomNode **)malloc(dom->block_count * sizeof(IrDomNode *));
    int q_head = 0, q_tail = 0;

    dom->root->reachable = true;
    queue[q_tail++] = dom->root;

    while (q_head < q_tail) {
        IrDomNode *curr = queue[q_head++];
        for (int s = 0; s < curr->block->succ_count; s++) {
            int succ_idx = get_block_index(dom, curr->block->successors[s]);
            if (succ_idx >= 0 && !dom->nodes[succ_idx]->reachable) {
                dom->nodes[succ_idx]->reachable = true;
                queue[q_tail++] = dom->nodes[succ_idx];
            }
        }
    }
    free(queue);

    /* 3. Initialize dominator sets:
     *    dom_by[entry] = {entry}
     *    dom_by[b] = all reachable blocks for b != entry
     */
    dom->root->dom_by[entry_idx] = true;

    for (int i = 0; i < dom->block_count; i++) {
        if (i == entry_idx || !dom->nodes[i]->reachable) continue;
        for (int j = 0; j < dom->block_count; j++) {
            if (dom->nodes[j]->reachable) {
                dom->nodes[i]->dom_by[j] = true;
            }
        }
    }

    /* 4. Fixed-point iterative dataflow computation:
     *    dom(b) = {b} U (Intersection over p in pred(b): dom(p))
     */
    bool changed = true;
    bool *temp_set = (bool *)malloc(dom->block_count * sizeof(bool));

    while (changed) {
        changed = false;

        for (int i = 0; i < dom->block_count; i++) {
            if (i == entry_idx || !dom->nodes[i]->reachable) continue;

            IrBasicBlock *bb = dom->nodes[i]->block;

            /* Compute intersection of dominators of reachable predecessors */
            bool first_pred = true;
            memset(temp_set, 0, dom->block_count * sizeof(bool));

            for (int p = 0; p < bb->pred_count; p++) {
                int pred_idx = get_block_index(dom, bb->predecessors[p]);
                if (pred_idx < 0 || !dom->nodes[pred_idx]->reachable) continue;

                if (first_pred) {
                    for (int j = 0; j < dom->block_count; j++) {
                        temp_set[j] = dom->nodes[pred_idx]->dom_by[j];
                    }
                    first_pred = false;
                } else {
                    for (int j = 0; j < dom->block_count; j++) {
                        temp_set[j] = temp_set[j] && dom->nodes[pred_idx]->dom_by[j];
                    }
                }
            }

            /* Include self */
            temp_set[i] = true;

            /* Check if set changed */
            for (int j = 0; j < dom->block_count; j++) {
                if (dom->nodes[i]->dom_by[j] != temp_set[j]) {
                    dom->nodes[i]->dom_by[j] = temp_set[j];
                    changed = true;
                }
            }
        }
    }
    free(temp_set);

    /* 5. Compute Immediate Dominators (idom):
     *    idom(b) is the unique strict dominator d of b that is dominated
     *    by every other strict dominator of b.
     */
    for (int i = 0; i < dom->block_count; i++) {
        if (i == entry_idx || !dom->nodes[i]->reachable) continue;

        /* Candidates are strict dominators of i */
        int best_candidate = -1;
        for (int c = 0; c < dom->block_count; c++) {
            if (c == i || !dom->nodes[i]->dom_by[c]) continue;

            /* Check if c is dominated by all other strict dominators of i */
            bool is_immediate = true;
            for (int o = 0; o < dom->block_count; o++) {
                if (o == i || o == c || !dom->nodes[i]->dom_by[o]) continue;
                if (!dom->nodes[c]->dom_by[o]) {
                    /* o is a strict dominator of i, but o does not dominate c -> c cannot be closest */
                    is_immediate = false;
                    break;
                }
            }
            if (is_immediate) {
                best_candidate = c;
                break;
            }
        }

        if (best_candidate >= 0) {
            dom->nodes[i]->idom = dom->nodes[best_candidate];
            add_child(dom->nodes[best_candidate], dom->nodes[i]);
        }
    }

    /* 6. Compute depths in dominator tree */
    compute_depths_recursive(dom->root, 0);

    /* 7. Compute Dominance Frontiers (DF):
     *    Y in DF[X] iff X dominates some P in pred(Y), but X does not strictly dominate Y.
     */
    for (int x = 0; x < dom->block_count; x++) {
        if (!dom->nodes[x]->reachable) continue;

        for (int y = 0; y < dom->block_count; y++) {
            if (!dom->nodes[y]->reachable) continue;

            /* Does X dominate a predecessor of Y? */
            bool doms_pred = false;
            IrBasicBlock *bb_y = dom->nodes[y]->block;
            for (int p = 0; p < bb_y->pred_count; p++) {
                int pred_idx = get_block_index(dom, bb_y->predecessors[p]);
                if (pred_idx >= 0 && dom->nodes[pred_idx]->dom_by[x]) {
                    doms_pred = true;
                    break;
                }
            }

            if (!doms_pred) continue;

            /* Does X strictly dominate Y? */
            bool strictly_doms = (x != y) && dom->nodes[y]->dom_by[x];
            if (!strictly_doms) {
                add_to_df(dom->nodes[x], dom->nodes[y]->block);
            }
        }
    }

    return dom;
}

void ir_dominance_free(IrDomInfo *dom) {
    if (!dom) return;

    for (int i = 0; i < dom->block_count; i++) {
        if (dom->nodes[i]) {
            free(dom->nodes[i]->dom_by);
            free(dom->nodes[i]->children);
            free(dom->nodes[i]->df);
            free(dom->nodes[i]);
        }
    }
    free(dom->nodes);
    free(dom->blocks);
    free(dom);
}

/* Dominance Queries Implementation */

bool ir_dominates(const IrDomInfo *dom, const IrBasicBlock *a, const IrBasicBlock *b) {
    if (!dom || !a || !b) return false;
    if (a == b) return true;
    int a_idx = get_block_index(dom, a);
    int b_idx = get_block_index(dom, b);
    if (a_idx < 0 || b_idx < 0) return false;
    if (!dom->nodes[b_idx]->reachable) return false;
    return dom->nodes[b_idx]->dom_by[a_idx];
}

bool ir_strictly_dominates(const IrDomInfo *dom, const IrBasicBlock *a, const IrBasicBlock *b) {
    if (!dom || !a || !b || a == b) return false;
    return ir_dominates(dom, a, b);
}

IrBasicBlock *ir_get_idom(const IrDomInfo *dom, const IrBasicBlock *b) {
    if (!dom || !b) return NULL;
    int b_idx = get_block_index(dom, b);
    if (b_idx < 0 || !dom->nodes[b_idx]->reachable) return NULL;
    return dom->nodes[b_idx]->idom ? dom->nodes[b_idx]->idom->block : NULL;
}

IrBasicBlock *const *ir_get_dominance_frontier(const IrDomInfo *dom, const IrBasicBlock *b, int *out_count) {
    if (!dom || !b) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    int b_idx = get_block_index(dom, b);
    if (b_idx < 0 || !dom->nodes[b_idx]->reachable) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = dom->nodes[b_idx]->df_count;
    return dom->nodes[b_idx]->df;
}

/* Verification & Diagnostics Implementation */

bool ir_dominance_verify(const IrDomInfo *dom, char **out_error) {
    if (!dom) {
        if (out_error) *out_error = strdup("Dominance info is NULL");
        return false;
    }

    if (!dom->root) {
        if (out_error) *out_error = strdup("Dominance root (entry block) is NULL");
        return false;
    }

    /* 1. Root must have no idom */
    if (dom->root->idom != NULL) {
        if (out_error) *out_error = strdup("Entry block has an immediate dominator, expected NULL");
        return false;
    }

    /* 2. Verify tree structure and idom for every reachable block */
    for (int i = 0; i < dom->block_count; i++) {
        IrDomNode *n = dom->nodes[i];
        if (!n->reachable) continue;

        if (n == dom->root) continue;

        if (!n->idom) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Reachable block '%s' has no immediate dominator", n->block->name ? n->block->name : "?");
            if (out_error) *out_error = strdup(buf);
            return false;
        }

        /* idom must strictly dominate n */
        if (!ir_strictly_dominates(dom, n->idom->block, n->block)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Block '%s' is marked idom of '%s', but does not strictly dominate it",
                     n->idom->block->name, n->block->name);
            if (out_error) *out_error = strdup(buf);
            return false;
        }

        /* Check that idom is an ancestor: walk idoms to root */
        IrDomNode *curr = n;
        int steps = 0;
        while (curr && curr != dom->root) {
            curr = curr->idom;
            steps++;
            if (steps > dom->block_count) {
                if (out_error) *out_error = strdup("Cycle detected in dominator tree idom chain");
                return false;
            }
        }
        if (curr != dom->root) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Block '%s' does not trace idom path to entry root", n->block->name);
            if (out_error) *out_error = strdup(buf);
            return false;
        }
    }

    /* 3. Verify Dominance Frontier correctness */
    for (int x = 0; x < dom->block_count; x++) {
        IrDomNode *nx = dom->nodes[x];
        if (!nx->reachable) continue;

        for (int d = 0; d < nx->df_count; d++) {
            IrBasicBlock *y = nx->df[d];
            int y_idx = get_block_index(dom, y);
            if (y_idx < 0) {
                if (out_error) *out_error = strdup("DF entry block not in function");
                return false;
            }

            /* X must not strictly dominate Y */
            if (ir_strictly_dominates(dom, nx->block, y)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Invalid DF: '%s' in DF['%s'], but '%s' strictly dominates '%s'",
                         y->name, nx->block->name, nx->block->name, y->name);
                if (out_error) *out_error = strdup(buf);
                return false;
            }

            /* X must dominate some predecessor of Y */
            bool doms_pred = false;
            for (int p = 0; p < y->pred_count; p++) {
                if (ir_dominates(dom, nx->block, y->predecessors[p])) {
                    doms_pred = true;
                    break;
                }
            }
            if (!doms_pred) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Invalid DF: '%s' in DF['%s'], but '%s' does not dominate any predecessor of '%s'",
                         y->name, nx->block->name, nx->block->name, y->name);
                if (out_error) *out_error = strdup(buf);
                return false;
            }
        }
    }

    return true;
}

void ir_dominance_dump(FILE *out, const IrDomInfo *dom) {
    if (!out || !dom) return;

    fprintf(out, "Dominator Analysis for function @%s:\n", dom->fn->name ? dom->fn->name : "fn");
    for (int i = 0; i < dom->block_count; i++) {
        IrDomNode *n = dom->nodes[i];
        if (!n->reachable) {
            fprintf(out, "  Block %-16s [UNREACHABLE]\n", n->block->name ? n->block->name : "?");
            continue;
        }

        fprintf(out, "  Block %-16s | idom: %-16s | depth: %-2d | DF: {",
                n->block->name ? n->block->name : "?",
                n->idom && n->idom->block->name ? n->idom->block->name : "(none)",
                n->depth);

        for (int d = 0; d < n->df_count; d++) {
            fprintf(out, "%s%s", d > 0 ? ", " : "", n->df[d]->name ? n->df[d]->name : "?");
        }
        fprintf(out, "}\n");
    }
}
