// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_loop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Helper Functions

static IrLoop *create_loop(int id, IrBasicBlock *header) {
    IrLoop *loop = (IrLoop *)calloc(1, sizeof(IrLoop));
    loop->id = id;
    loop->header = header;
    loop->depth = 1;
    return loop;
}

static void loop_add_latch(IrLoop *loop, IrBasicBlock *latch) {
    for (int i = 0; i < loop->latch_count; i++) {
        if (loop->latches[i] == latch) return;
    }
    if (loop->latch_count >= loop->latch_cap) {
        int new_cap = loop->latch_cap == 0 ? 4 : loop->latch_cap * 2;
        loop->latches = (IrBasicBlock **)realloc(loop->latches, new_cap * sizeof(IrBasicBlock *));
        loop->latch_cap = new_cap;
    }
    loop->latches[loop->latch_count++] = latch;
}

static void loop_add_block(IrLoop *loop, IrBasicBlock *bb) {
    for (int i = 0; i < loop->block_count; i++) {
        if (loop->blocks[i] == bb) return;
    }
    if (loop->block_count >= loop->block_cap) {
        int new_cap = loop->block_cap == 0 ? 8 : loop->block_cap * 2;
        loop->blocks = (IrBasicBlock **)realloc(loop->blocks, new_cap * sizeof(IrBasicBlock *));
        loop->block_cap = new_cap;
    }
    loop->blocks[loop->block_count++] = bb;
}

static void loop_add_child(IrLoop *parent, IrLoop *child) {
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) return;
    }
    if (parent->child_count >= parent->child_cap) {
        int new_cap = parent->child_cap == 0 ? 4 : parent->child_cap * 2;
        parent->children = (IrLoop **)realloc(parent->children, new_cap * sizeof(IrLoop *));
        parent->child_cap = new_cap;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

static void free_loop(IrLoop *loop) {
    if (!loop) return;
    if (loop->latches) free(loop->latches);
    if (loop->blocks) free(loop->blocks);
    if (loop->children) free(loop->children);
    free(loop);
}

// Loop Analysis Implementation

IrLoopInfo *ir_loop_analysis_compute(IrFunction *fn, const IrDomInfo *dom) {
    if (!fn || !dom) return NULL;

    IrLoopInfo *info = (IrLoopInfo *)calloc(1, sizeof(IrLoopInfo));
    info->fn = fn;

    /* 1. Detect back-edges B -> H where H dominates B */
    typedef struct {
        IrBasicBlock *latch;
        IrBasicBlock *header;
    } BackEdge;

    BackEdge *back_edges = NULL;
    int be_count = 0;
    int be_cap = 0;

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (int s = 0; s < bb->succ_count; s++) {
            IrBasicBlock *succ = bb->successors[s];
            if (ir_dominates(dom, succ, bb)) {
                /* Back-edge bb -> succ found */
                if (be_count >= be_cap) {
                    be_cap = be_cap == 0 ? 8 : be_cap * 2;
                    back_edges = (BackEdge *)realloc(back_edges, be_cap * sizeof(BackEdge));
                }
                back_edges[be_count].latch = bb;
                back_edges[be_count].header = succ;
                be_count++;
            }
        }
    }

    if (be_count == 0) {
        free(back_edges);
        return info;
    }

    /* 2. Group back-edges by header to form unified natural loops */
    for (int e = 0; e < be_count; e++) {
        IrBasicBlock *header = back_edges[e].header;
        IrBasicBlock *latch  = back_edges[e].latch;

        /* Look for existing loop with same header */
        IrLoop *target_loop = NULL;
        for (int l = 0; l < info->loop_count; l++) {
            if (info->loops[l]->header == header) {
                target_loop = info->loops[l];
                break;
            }
        }

        if (!target_loop) {
            target_loop = create_loop(info->loop_count, header);
            if (info->loop_count >= info->loop_cap) {
                int new_cap = info->loop_cap == 0 ? 4 : info->loop_cap * 2;
                info->loops = (IrLoop **)realloc(info->loops, new_cap * sizeof(IrLoop *));
                info->loop_cap = new_cap;
            }
            info->loops[info->loop_count++] = target_loop;
            loop_add_block(target_loop, header);
        }

        loop_add_latch(target_loop, latch);

        /* 3. Discover all blocks in the natural loop */
        loop_add_block(target_loop, latch);

        if (latch != header) {
            /* Worklist traversal of predecessors */
            IrBasicBlock **worklist = (IrBasicBlock **)malloc(fn->block_count * sizeof(IrBasicBlock *));
            int wl_head = 0;
            int wl_tail = 0;

            worklist[wl_tail++] = latch;

            while (wl_head < wl_tail) {
                IrBasicBlock *curr = worklist[wl_head++];

                for (int p = 0; p < curr->pred_count; p++) {
                    IrBasicBlock *pred = curr->predecessors[p];
                    if (!ir_loop_contains(target_loop, pred)) {
                        loop_add_block(target_loop, pred);
                        if (pred != header) {
                            worklist[wl_tail++] = pred;
                        }
                    }
                }
            }

            free(worklist);
        }
    }

    free(back_edges);

    /* 4. Determine loop nesting / hierarchy */
    for (int i = 0; i < info->loop_count; i++) {
        IrLoop *sub = info->loops[i];
        IrLoop *best_parent = NULL;

        for (int j = 0; j < info->loop_count; j++) {
            if (i == j) continue;
            IrLoop *cand = info->loops[j];

            /* Check if sub is strictly contained in cand */
            if (sub->block_count < cand->block_count) {
                bool all_inside = true;
                for (int b = 0; b < sub->block_count; b++) {
                    if (!ir_loop_contains(cand, sub->blocks[b])) {
                        all_inside = false;
                        break;
                    }
                }
                if (all_inside) {
                    /* cand contains sub. Pick smallest enclosing loop */
                    if (!best_parent || cand->block_count < best_parent->block_count) {
                        best_parent = cand;
                    }
                }
            }
        }

        if (best_parent) {
            loop_add_child(best_parent, sub);
        }
    }

    /* 5. Calculate loop depths */
    for (int i = 0; i < info->loop_count; i++) {
        IrLoop *curr = info->loops[i];
        int d = 1;
        for (IrLoop *p = curr->parent; p; p = p->parent) {
            d++;
        }
        curr->depth = d;
    }

    return info;
}

void ir_loop_info_free(IrLoopInfo *info) {
    if (!info) return;
    for (int i = 0; i < info->loop_count; i++) {
        free_loop(info->loops[i]);
    }
    if (info->loops) free(info->loops);
    free(info);
}

// Query API

int ir_loop_count(const IrLoopInfo *info) {
    return info ? info->loop_count : 0;
}

IrLoop *ir_loop_get(const IrLoopInfo *info, int idx) {
    if (!info || idx < 0 || idx >= info->loop_count) return NULL;
    return info->loops[idx];
}

IrBasicBlock *ir_loop_header(const IrLoop *loop) {
    return loop ? loop->header : NULL;
}

bool ir_loop_contains(const IrLoop *loop, const IrBasicBlock *bb) {
    if (!loop || !bb) return false;
    for (int i = 0; i < loop->block_count; i++) {
        if (loop->blocks[i] == bb) return true;
    }
    return false;
}

IrLoop *ir_loop_for_block(const IrLoopInfo *info, const IrBasicBlock *bb) {
    if (!info || !bb) return NULL;
    IrLoop *best = NULL;
    for (int i = 0; i < info->loop_count; i++) {
        IrLoop *cand = info->loops[i];
        if (ir_loop_contains(cand, bb)) {
            if (!best || cand->depth > best->depth) {
                best = cand;
            }
        }
    }
    return best;
}

int ir_loop_depth(const IrLoopInfo *info, const IrBasicBlock *bb) {
    IrLoop *loop = ir_loop_for_block(info, bb);
    return loop ? loop->depth : 0;
}

IrLoop *ir_loop_get_parent(const IrLoop *loop) {
    return loop ? loop->parent : NULL;
}

// Preheader Management

IrBasicBlock *ir_loop_get_or_create_preheader(IrFunction *fn, IrLoop *loop) {
    if (!fn || !loop || !loop->header) return NULL;

    IrBasicBlock *header = loop->header;

    /* Find predecessors from outside the loop */
    IrBasicBlock *outside_preds[32];
    int outside_count = 0;

    for (int p = 0; p < header->pred_count; p++) {
        IrBasicBlock *pred = header->predecessors[p];
        if (!ir_loop_contains(loop, pred) && outside_count < 32) {
            outside_preds[outside_count++] = pred;
        }
    }

    /* If exactly one outside predecessor and its only successor is header, it's a dedicated preheader */
    if (outside_count == 1 && outside_preds[0]->succ_count == 1) {
        return outside_preds[0];
    }

    /* Otherwise, create a dedicated preheader block */
    char preheader_name[64];
    snprintf(preheader_name, sizeof(preheader_name), "preheader.%s", header->name ? header->name : "loop");
    IrBasicBlock *preheader = ir_block_create(fn, preheader_name);

    IrArena *arena = fn->module->arena;

    /* Branch from preheader to header */
    IrInstruction *br = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
    br->op = IR_OP_BR;
    br->type = ir_type_void(arena);
    br->target_true = header;
    ir_block_add_instruction(preheader, br);

    /* Retarget outside predecessors to preheader */
    for (int i = 0; i < outside_count; i++) {
        IrBasicBlock *op = outside_preds[i];
        if (op->last_inst) {
            if (op->last_inst->target_true == header) {
                op->last_inst->target_true = preheader;
            }
            if (op->last_inst->target_false == header) {
                op->last_inst->target_false = preheader;
            }
        }
    }

    /* Update phi nodes in header */
    for (IrInstruction *phi = header->first_inst; phi && phi->op == IR_OP_PHI; phi = phi->next) {
        if (outside_count <= 1) {
            for (int p = 0; p < phi->phi_count; p++) {
                if (outside_count == 1 && phi->phi_blocks[p] == outside_preds[0]) {
                    phi->phi_blocks[p] = preheader;
                }
            }
        } else {
            /* If multiple outside predecessors, create a phi in preheader */
            IrInstruction *p_phi = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
            p_phi->op = IR_OP_PHI;
            p_phi->type = phi->type;
            p_phi->result = ir_val_reg(arena, phi->type, fn->next_reg_id++);
            p_phi->phi_blocks = (IrBasicBlock **)ir_arena_alloc(arena, outside_count * sizeof(IrBasicBlock *));
            p_phi->phi_values = (IrValue **)ir_arena_alloc(arena, outside_count * sizeof(IrValue *));
            p_phi->phi_count = outside_count;

            int p_idx = 0;
            int new_phi_count = 0;
            IrBasicBlock **new_blocks = (IrBasicBlock **)ir_arena_alloc(arena, phi->phi_count * sizeof(IrBasicBlock *));
            IrValue **new_vals = (IrValue **)ir_arena_alloc(arena, phi->phi_count * sizeof(IrValue *));

            for (int p = 0; p < phi->phi_count; p++) {
                bool is_outside = false;
                for (int o = 0; o < outside_count; o++) {
                    if (phi->phi_blocks[p] == outside_preds[o]) {
                        is_outside = true;
                        p_phi->phi_blocks[p_idx] = phi->phi_blocks[p];
                        p_phi->phi_values[p_idx] = phi->phi_values[p];
                        p_idx++;
                        break;
                    }
                }
                if (!is_outside) {
                    new_blocks[new_phi_count] = phi->phi_blocks[p];
                    new_vals[new_phi_count] = phi->phi_values[p];
                    new_phi_count++;
                }
            }

            /* Add preheader incoming to header phi */
            new_blocks[new_phi_count] = preheader;
            new_vals[new_phi_count] = p_phi->result;
            new_phi_count++;

            phi->phi_blocks = new_blocks;
            phi->phi_values = new_vals;
            phi->phi_count = new_phi_count;

            /* Insert p_phi at beginning of preheader */
            p_phi->next = preheader->first_inst;
            if (preheader->first_inst) preheader->first_inst->prev = p_phi;
            preheader->first_inst = p_phi;
            preheader->inst_count++;
        }
    }

    ir_recompute_cfg(fn);
    return preheader;
}

// Deterministic Debug Dump

void ir_loop_dump(FILE *out, const IrLoopInfo *info) {
    if (!out || !info) return;

    fprintf(out, "Function: %s\n\n", info->fn->name ? info->fn->name : "anon");

    for (int i = 0; i < info->loop_count; i++) {
        IrLoop *loop = info->loops[i];
        fprintf(out, "Loop %d\n", loop->id);
        fprintf(out, "  Header: %s\n", loop->header && loop->header->name ? loop->header->name : "none");

        fprintf(out, "  Latch:  ");
        for (int l = 0; l < loop->latch_count; l++) {
            fprintf(out, "%s%s", loop->latches[l]->name ? loop->latches[l]->name : "anon",
                    (l + 1 < loop->latch_count) ? " " : "");
        }
        fprintf(out, "\n");

        fprintf(out, "  Depth:  %d\n", loop->depth);

        fprintf(out, "  Blocks: ");
        for (int b = 0; b < loop->block_count; b++) {
            fprintf(out, "%s%s", loop->blocks[b]->name ? loop->blocks[b]->name : "anon",
                    (b + 1 < loop->block_count) ? " " : "");
        }
        fprintf(out, "\n\n");
    }
}

// Phase 8: Profile-Guided Loop Analysis

#include "ir_profile.h"

void ir_loop_apply_profile(IrLoopInfo *info, const struct CcoProfile *prof) {
    if (!info || !prof) return;

    for (int i = 0; i < info->loop_count; i++) {
        IrLoop *loop = info->loops[i];
        if (!loop->header || !loop->header->name) continue;

        uint64_t header_count = cco_profile_get_block_count(prof, info->fn->name, loop->header->name);

        /* Sum entries from predecessor blocks outside the loop */
        uint64_t entries = 0;
        for (int p = 0; p < loop->header->pred_count; p++) {
            IrBasicBlock *pred = loop->header->predecessors[p];
            if (!ir_loop_contains(loop, pred)) {
                uint64_t pred_count = cco_profile_get_block_count(prof, info->fn->name, pred->name);
                entries += pred_count;
            }
        }
        if (entries == 0 && header_count > 0) entries = 1;

        loop->iteration_count = header_count;
        loop->entry_count = entries;
        loop->avg_iterations = (entries > 0) ? ((double)header_count / (double)entries) : (double)header_count;
        loop->is_hot_loop = (header_count >= 1000 || loop->avg_iterations >= 10.0);
    }
}

void ir_loop_dump_profile(FILE *out, const IrLoopInfo *info) {
    if (!out || !info) return;

    fprintf(out, "====================================================\n");
    fprintf(out, "PROFILE-GUIDED LOOP ANALYSIS: %s\n", info->fn->name ? info->fn->name : "anon");
    fprintf(out, "====================================================\n");

    for (int i = 0; i < info->loop_count; i++) {
        IrLoop *loop = info->loops[i];
        fprintf(out, "Loop %d\n", loop->id);
        fprintf(out, "  Header:     %s\n", loop->header && loop->header->name ? loop->header->name : "none");
        fprintf(out, "  Entries:    %lu\n", (unsigned long)loop->entry_count);
        fprintf(out, "  Iterations: %lu\n", (unsigned long)loop->iteration_count);
        fprintf(out, "  Avg Iters:  %.1f\n", loop->avg_iterations);
        fprintf(out, "  Depth:      %d\n", loop->depth);
        fprintf(out, "  Hot Loop:   %s\n\n", loop->is_hot_loop ? "yes" : "no");
    }
}

