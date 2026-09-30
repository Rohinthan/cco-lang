// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_ssa.h"
#include "ir_ssa_opt.h"
#include "ir_ipa.h"
#include "ir_verify.h"
#include "ir_print.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* Helper Data Structures for Variable Promotion                             */

typedef struct {
    char *name;                     /* Variable name without suffix, e.g. "x" */
    char *slot_name;                /* Stack slot name, e.g. "x.addr" */
    IrType *type;                   /* Promoted scalar type */
    IrInstruction *alloca_inst;     /* Defining alloca in entry block */
    IrBasicBlock **def_blocks;      /* Blocks with stores to this variable */
    int def_count;
    int def_cap;

    /* Value stack for SSA renaming */
    IrValue **stack;
    int stack_len;
    int stack_cap;
} PromotedVar;

typedef struct {
    PromotedVar **vars;
    int count;
    int cap;
} PromotedVarList;

static void add_def_block(PromotedVar *pv, IrBasicBlock *bb) {
    for (int i = 0; i < pv->def_count; i++) {
        if (pv->def_blocks[i] == bb) return;
    }
    if (pv->def_count >= pv->def_cap) {
        int new_cap = pv->def_cap == 0 ? 4 : pv->def_cap * 2;
        pv->def_blocks = (IrBasicBlock **)realloc(pv->def_blocks, new_cap * sizeof(IrBasicBlock *));
        pv->def_cap = new_cap;
    }
    pv->def_blocks[pv->def_count++] = bb;
}

static void stack_push(PromotedVar *pv, IrValue *val) {
    if (pv->stack_len >= pv->stack_cap) {
        int new_cap = pv->stack_cap == 0 ? 8 : pv->stack_cap * 2;
        pv->stack = (IrValue **)realloc(pv->stack, new_cap * sizeof(IrValue *));
        pv->stack_cap = new_cap;
    }
    pv->stack[pv->stack_len++] = val;
}

static IrValue *stack_top(PromotedVar *pv, IrArena *arena) {
    if (pv->stack_len > 0) {
        return pv->stack[pv->stack_len - 1];
    }
    /* Default undef/zero constant if read before write */
    if (pv->type->kind == IR_TYPE_F64) {
        return ir_val_const_float(arena, 0.0);
    } else if (pv->type->kind == IR_TYPE_BOOL) {
        return ir_val_const_bool(arena, false);
    }
    return ir_val_const_int(arena, pv->type, 0);
}

static void stack_pop(PromotedVar *pv) {
    if (pv->stack_len > 0) {
        pv->stack_len--;
    }
}

static bool same_val(const IrValue *a, const IrValue *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->kind == IR_VAL_REG && a->id == b->id) return true;
    if (a->kind == IR_VAL_CONST_INT && a->const_val.int_val == b->const_val.int_val) return true;
    if (a->kind == IR_VAL_CONST_BOOL && a->const_val.bool_val == b->const_val.bool_val) return true;
    if (a->kind == IR_VAL_CONST_FLOAT && a->const_val.float_val == b->const_val.float_val) return true;
    if (a->kind == IR_VAL_CONST_CHAR && a->const_val.char_val == b->const_val.char_val) return true;
    if (a->kind == IR_VAL_VAR && a->name && b->name && strcmp(a->name, b->name) == 0) return true;
    return false;
}

static void replace_uses_in_fn(IrFunction *fn, IrValue *old_val, IrValue *new_val) {
    if (!fn || !old_val || !new_val) return;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->lhs && same_val(inst->lhs, old_val)) inst->lhs = new_val;
            if (inst->rhs && same_val(inst->rhs, old_val)) inst->rhs = new_val;
            for (int a = 0; a < inst->arg_count; a++) {
                if (inst->args[a] && same_val(inst->args[a], old_val)) {
                    inst->args[a] = new_val;
                }
            }
            for (int p = 0; p < inst->phi_count; p++) {
                if (inst->phi_values[p] && same_val(inst->phi_values[p], old_val)) {
                    inst->phi_values[p] = new_val;
                }
            }
        }
    }
}

static void remove_inst(IrBasicBlock *bb, IrInstruction *inst) {
    if (!bb || !inst) return;
    if (inst->prev) {
        inst->prev->next = inst->next;
    } else {
        bb->first_inst = inst->next;
    }
    if (inst->next) {
        inst->next->prev = inst->prev;
    } else {
        bb->last_inst = inst->prev;
    }
    bb->inst_count--;
}


/* Promotable Variable Analysis                                   */

static bool is_scalar_promotable(IrType *type) {
    if (!type) return false;
    switch (type->kind) {
        case IR_TYPE_I32:
        case IR_TYPE_I64:
        case IR_TYPE_F64:
        case IR_TYPE_BOOL:
        case IR_TYPE_CHAR:
        case IR_TYPE_PTR:
            return true;
        default:
            return false;
    }
}

static bool is_var_escaped(IrFunction *fn, const char *slot_name) {
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            /* Loads only read from slot */
            if (inst->op == IR_OP_LOAD) {
                if (inst->rhs && inst->rhs->name && strcmp(inst->rhs->name, slot_name) == 0) return true;
                continue;
            }
            /* Stores write value to slot */
            if (inst->op == IR_OP_STORE) {
                if (inst->lhs && inst->lhs->name && strcmp(inst->lhs->name, slot_name) == 0) return true;
                continue;
            }
            /* If slot appears in any other instruction, it escapes */
            if (inst->lhs && inst->lhs->name && strcmp(inst->lhs->name, slot_name) == 0) return true;
            if (inst->rhs && inst->rhs->name && strcmp(inst->rhs->name, slot_name) == 0) return true;
            for (int a = 0; a < inst->arg_count; a++) {
                if (inst->args[a] && inst->args[a]->name && strcmp(inst->args[a]->name, slot_name) == 0) return true;
            }
        }
    }
    return false;
}

static PromotedVarList *find_promotable_vars(IrFunction *fn) {
    PromotedVarList *list = (PromotedVarList *)calloc(1, sizeof(PromotedVarList));
    if (!fn->entry_block) return list;

    for (IrInstruction *inst = fn->entry_block->first_inst; inst; inst = inst->next) {
        if (inst->op == IR_OP_ALLOCA && inst->result && inst->result->name) {
            IrType *elem_t = inst->type->elem_type ? inst->type->elem_type : inst->type;
            if (!is_scalar_promotable(elem_t)) continue;
            if (is_var_escaped(fn, inst->result->name)) continue;

            PromotedVar *pv = (PromotedVar *)calloc(1, sizeof(PromotedVar));
            pv->slot_name = strdup(inst->result->name);
            pv->name = strdup(inst->result->name);
            char *dot = strrchr(pv->name, '.');
            if (dot) *dot = '\0'; /* Strip ".addr" */
            pv->type = elem_t;
            pv->alloca_inst = inst;

            if (list->count >= list->cap) {
                int new_cap = list->cap == 0 ? 8 : list->cap * 2;
                list->vars = (PromotedVar **)realloc(list->vars, new_cap * sizeof(PromotedVar *));
                list->cap = new_cap;
            }
            list->vars[list->count++] = pv;
        }
    }

    /* Record all defining blocks for each promotable variable */
    for (int v = 0; v < list->count; v++) {
        PromotedVar *pv = list->vars[v];
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->op == IR_OP_STORE && inst->rhs && inst->rhs->name && strcmp(inst->rhs->name, pv->slot_name) == 0) {
                    add_def_block(pv, bb);
                }
            }
        }
    }

    return list;
}

static void free_promotable_vars(PromotedVarList *list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free(list->vars[i]->name);
        free(list->vars[i]->slot_name);
        free(list->vars[i]->def_blocks);
        free(list->vars[i]->stack);
        free(list->vars[i]);
    }
    free(list->vars);
    free(list);
}

/* Phi-Node Placement via Iterated Dominance Frontier (IDF)       */

static void insert_phi_node_at_block_front(IrBasicBlock *bb, PromotedVar *pv, IrArena *arena, IrFunction *fn) {
    /* Check if phi for this variable already exists */
    for (IrInstruction *cur = bb->first_inst; cur && cur->op == IR_OP_PHI; cur = cur->next) {
        if (cur->callee_name && strcmp(cur->callee_name, pv->slot_name) == 0) {
            return;
        }
    }

    IrInstruction *phi = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
    phi->op = IR_OP_PHI;
    phi->type = pv->type;
    phi->result = ir_val_reg(arena, pv->type, fn->next_reg_id++);
    phi->callee_name = ir_arena_strdup(arena, pv->slot_name); /* Tag with slot name */

    int pred_cnt = bb->pred_count;
    phi->phi_count = pred_cnt;
    phi->phi_blocks = (IrBasicBlock **)ir_arena_alloc(arena, pred_cnt * sizeof(IrBasicBlock *));
    phi->phi_values = (IrValue **)ir_arena_alloc(arena, pred_cnt * sizeof(IrValue *));

    for (int p = 0; p < pred_cnt; p++) {
        phi->phi_blocks[p] = bb->predecessors[p];
        phi->phi_values[p] = NULL;
    }

    /* Insert at the very front of the block */
    phi->prev = NULL;
    phi->next = bb->first_inst;
    if (bb->first_inst) {
        bb->first_inst->prev = phi;
    } else {
        bb->last_inst = phi;
    }
    bb->first_inst = phi;
    bb->inst_count++;
}

static int place_phi_nodes(IrFunction *fn, const IrDomInfo *dom, PromotedVarList *pvl, IrArena *arena) {
    int phi_count = 0;

    for (int v = 0; v < pvl->count; v++) {
        PromotedVar *pv = pvl->vars[v];

        /* Worklist for IDF calculation */
        int queue_cap = fn->block_count + 16;
        IrBasicBlock **worklist = (IrBasicBlock **)malloc(queue_cap * sizeof(IrBasicBlock *));
        bool *in_worklist = (bool *)calloc(fn->block_count + 16, sizeof(bool));
        bool *has_phi = (bool *)calloc(fn->block_count + 16, sizeof(bool));
        int head = 0, tail = 0;

        for (int d = 0; d < pv->def_count; d++) {
            IrBasicBlock *db = pv->def_blocks[d];
            worklist[tail++] = db;
            if (db->id < fn->block_count + 16) in_worklist[db->id] = true;
        }

        while (head < tail) {
            IrBasicBlock *b = worklist[head++];
            int df_count = 0;
            IrBasicBlock *const *df = ir_get_dominance_frontier(dom, b, &df_count);

            for (int d = 0; d < df_count; d++) {
                IrBasicBlock *y = df[d];
                int y_id = y->id;
                if (y_id < fn->block_count + 16 && !has_phi[y_id]) {
                    has_phi[y_id] = true;
                    insert_phi_node_at_block_front(y, pv, arena, fn);
                    phi_count++;

                    if (!in_worklist[y_id]) {
                        in_worklist[y_id] = true;
                        worklist[tail++] = y;
                    }
                }
            }
        }

        free(worklist);
        free(in_worklist);
        free(has_phi);
    }

    return phi_count;
}

/* SSA Renaming Walk over Dominator Tree                          */

static void rename_dominator_tree(IrDomNode *node, PromotedVarList *pvl, IrArena *arena, IrFunction *fn) {
    if (!node || !node->reachable) return;
    IrBasicBlock *bb = node->block;

    /* Track number of pushes for each variable in this block */
    int *push_counts = (int *)calloc(pvl->count, sizeof(int));

    /* 1. For each phi in this block, push its result onto the corresponding variable stack */
    for (IrInstruction *inst = bb->first_inst; inst && inst->op == IR_OP_PHI; inst = inst->next) {
        if (!inst->callee_name) continue;
        for (int v = 0; v < pvl->count; v++) {
            if (strcmp(pvl->vars[v]->slot_name, inst->callee_name) == 0) {
                stack_push(pvl->vars[v], inst->result);
                push_counts[v]++;
                break;
            }
        }
    }

    /* 2. Walk non-phi instructions in basic block */
    IrInstruction *inst = bb->first_inst;
    while (inst) {
        IrInstruction *next = inst->next;

        if (inst->op == IR_OP_LOAD && inst->lhs && inst->lhs->name) {
            for (int v = 0; v < pvl->count; v++) {
                if (strcmp(pvl->vars[v]->slot_name, inst->lhs->name) == 0) {
                    IrValue *curr_val = stack_top(pvl->vars[v], arena);
                    replace_uses_in_fn(fn, inst->result, curr_val);
                    remove_inst(bb, inst);
                    break;
                }
            }
        } else if (inst->op == IR_OP_STORE && inst->rhs && inst->rhs->name) {
            for (int v = 0; v < pvl->count; v++) {
                if (strcmp(pvl->vars[v]->slot_name, inst->rhs->name) == 0) {
                    stack_push(pvl->vars[v], inst->lhs);
                    push_counts[v]++;
                    remove_inst(bb, inst);
                    break;
                }
            }
        }

        inst = next;
    }

    /* 3. Fill incoming phi operands in CFG successors */
    for (int s = 0; s < bb->succ_count; s++) {
        IrBasicBlock *succ = bb->successors[s];
        for (IrInstruction *phi = succ->first_inst; phi && phi->op == IR_OP_PHI; phi = phi->next) {
            if (!phi->callee_name) continue;
            for (int v = 0; v < pvl->count; v++) {
                if (strcmp(pvl->vars[v]->slot_name, phi->callee_name) == 0) {
                    /* Find slot for this predecessor */
                    for (int p = 0; p < phi->phi_count; p++) {
                        if (phi->phi_blocks[p] == bb) {
                            phi->phi_values[p] = stack_top(pvl->vars[v], arena);
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }

    /* 4. Recurse to dominator tree children */
    for (int c = 0; c < node->child_count; c++) {
        rename_dominator_tree(node->children[c], pvl, arena, fn);
    }

    /* 5. Pop all pushed definitions to restore stacks for sibling subtrees */
    for (int v = 0; v < pvl->count; v++) {
        for (int p = 0; p < push_counts[v]; p++) {
            stack_pop(pvl->vars[v]);
        }
    }
    free(push_counts);
}

/* SSA Construction Core API                                      */

bool ir_ssa_construct_function(IrFunction *fn, IrSsaStats *out_stats, char **out_error) {
    if (!fn) {
        if (out_error) *out_error = strdup("Function pointer is NULL");
        return false;
    }
    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));

    ir_recompute_cfg(fn);

    /* 1. Compute dominator analysis */
    IrDomInfo *dom = ir_dominance_compute(fn);
    if (!dom) {
        if (out_error) *out_error = strdup("Failed to compute dominator info for function");
        return false;
    }

    /* 2. Identify promotable scalar allocas */
    PromotedVarList *pvl = find_promotable_vars(fn);
    if (out_stats) out_stats->promoted_allocas = pvl->count;

    if (pvl->count == 0) {
        /* No promotable allocas: function already in register-only SSA form */
        free_promotable_vars(pvl);
        ir_dominance_free(dom);
        return true;
    }

    /* 3. Insert Phi-nodes at Iterated Dominance Frontiers */
    int phis_inserted = place_phi_nodes(fn, dom, pvl, fn->module->arena);
    if (out_stats) out_stats->phi_nodes_created = phis_inserted;

    /* 4. Deterministic SSA Renaming walk */
    rename_dominator_tree(dom->root, pvl, fn->module->arena, fn);

    /* 5. Remove promoted alloca instructions from entry block */
    for (int v = 0; v < pvl->count; v++) {
        if (pvl->vars[v]->alloca_inst && fn->entry_block) {
            remove_inst(fn->entry_block, pvl->vars[v]->alloca_inst);
        }
    }

    free_promotable_vars(pvl);
    ir_dominance_free(dom);

    /* Recompute CFG after removals */
    ir_recompute_cfg(fn);

    /* Verify SSA invariants */
    char *v_err = NULL;
    if (!ir_ssa_verify_function(fn, NULL, &v_err)) {
        if (out_error) *out_error = v_err;
        else if (v_err) free(v_err);
        return false;
    }

    return true;
}

bool ir_ssa_construct_module(IrModule *module, IrSsaStats *out_stats, char **out_error) {
    if (!module) return false;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_ssa_construct_function(fn, out_stats, out_error)) {
            return false;
        }
    }
    return true;
}

/* SSA Verification                                               */

bool ir_ssa_verify_function(IrFunction *fn, const IrDomInfo *in_dom, char **out_error) {
    if (!fn) return false;

    IrDomInfo *dom = in_dom ? (IrDomInfo *)in_dom : ir_dominance_compute(fn);
    if (!dom) {
        if (out_error) *out_error = strdup("Failed to compute dominator info for verification");
        return false;
    }

    int max_regs = fn->next_reg_id + 32;
    int *def_counts = (int *)calloc(max_regs, sizeof(int));
    IrBasicBlock **def_blocks = (IrBasicBlock **)calloc(max_regs, sizeof(IrBasicBlock *));

    /* 1. Check Single-Definition property */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_regs) {
                def_counts[inst->result->id]++;
                def_blocks[inst->result->id] = bb;
                if (def_counts[inst->result->id] > 1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "SSA violation: virtual register %%%d defined multiple times in @%s",
                             inst->result->id, fn->name);
                    if (out_error) *out_error = strdup(buf);
                    free(def_counts);
                    free(def_blocks);
                    if (!in_dom) ir_dominance_free(dom);
                    return false;
                }
            }
        }
    }

    /* 2. Check Dominance for all non-phi uses */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_PHI) continue;

            IrValue *operands[32];
            int op_cnt = 0;
            if (inst->lhs && inst->lhs->kind == IR_VAL_REG) operands[op_cnt++] = inst->lhs;
            if (inst->rhs && inst->rhs->kind == IR_VAL_REG) operands[op_cnt++] = inst->rhs;
            for (int a = 0; a < inst->arg_count; a++) {
                if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && op_cnt < 32) {
                    operands[op_cnt++] = inst->args[a];
                }
            }

            for (int i = 0; i < op_cnt; i++) {
                int reg_id = operands[i]->id;
                if (reg_id >= max_regs || def_counts[reg_id] == 0) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "SSA violation: use of undefined virtual register %%%d in @%s", reg_id, fn->name);
                    if (out_error) *out_error = strdup(buf);
                    free(def_counts);
                    free(def_blocks);
                    if (!in_dom) ir_dominance_free(dom);
                    return false;
                }

                IrBasicBlock *def_bb = def_blocks[reg_id];
                if (!ir_dominates(dom, def_bb, bb)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "SSA dominance violation: def of %%%d in '%s' does not dominate use in '%s' (@%s)",
                             reg_id, def_bb->name, bb->name, fn->name);
                    if (out_error) *out_error = strdup(buf);
                    free(def_counts);
                    free(def_blocks);
                    if (!in_dom) ir_dominance_free(dom);
                    return false;
                }
            }
        }
    }

    /* 3. Check Phi-node invariants */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        bool seen_non_phi = false;
        for (IrInstruction *phi = bb->first_inst; phi; phi = phi->next) {
            if (phi->op == IR_OP_PHI) {
                if (seen_non_phi) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "SSA phi violation: phi node in '%s' appears after non-phi instruction in @%s",
                             bb->name, fn->name);
                    if (out_error) *out_error = strdup(buf);
                    free(def_counts);
                    free(def_blocks);
                    if (!in_dom) ir_dominance_free(dom);
                    return false;
                }
                for (int p = 0; p < phi->phi_count; p++) {
                    IrBasicBlock *pred = phi->phi_blocks[p];
                    /* Predecessor must be in bb->predecessors */
                    bool is_pred = false;
                    for (int bp = 0; bp < bb->pred_count; bp++) {
                        if (bb->predecessors[bp] == pred) {
                            is_pred = true;
                            break;
                        }
                    }
                    if (!is_pred) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "SSA phi violation: incoming block '%s' is not a predecessor of '%s' in @%s",
                                 pred ? pred->name : "null", bb->name, fn->name);
                        if (out_error) *out_error = strdup(buf);
                        free(def_counts);
                        free(def_blocks);
                        if (!in_dom) ir_dominance_free(dom);
                        return false;
                    }
                }
            } else {
                seen_non_phi = true;
            }
        }
    }

    free(def_counts);
    free(def_blocks);
    if (!in_dom) ir_dominance_free(dom);
    return true;
}

bool ir_ssa_verify_module(IrModule *module, char **out_error) {
    if (!module) return false;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) {
            return false;
        }
    }
    return true;
}

/* SSA Optimizations (Constant Prop / Folding and SSA DCE)        */

bool ir_ssa_optimize_function(IrFunction *fn, bool *out_changed, char **out_error) {
    if (!fn) return false;
    bool any_changed = false;
    IrArena *arena = fn->module->arena;

    /* 1. Trivial Phi Folding:
     *    phi(x, x) -> x, or phi with identical incoming values
     */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        IrInstruction *inst = bb->first_inst;
        while (inst && inst->op == IR_OP_PHI) {
            IrInstruction *next = inst->next;

            if (inst->phi_count > 0 && inst->result) {
                IrValue *first_val = inst->phi_values[0];
                bool all_same = true;
                for (int p = 1; p < inst->phi_count; p++) {
                    if (!same_val(inst->phi_values[p], first_val) && !same_val(inst->phi_values[p], inst->result)) {
                        all_same = false;
                        break;
                    }
                }
                if (all_same && first_val != NULL) {
                    replace_uses_in_fn(fn, inst->result, first_val);
                    remove_inst(bb, inst);
                    any_changed = true;
                }
            }

            inst = next;
        }
    }

    /* 2. SSA Constant Propagation & Folding */
    int max_id = fn->next_reg_id + 32;
    IrValue **const_map = (IrValue **)calloc(max_id, sizeof(IrValue *));

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_CONST && inst->result && inst->lhs) {
                if (inst->result->id < max_id) {
                    const_map[inst->result->id] = inst->lhs;
                }
            }
        }
    }

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_CONST) continue;

            if (inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_id && const_map[inst->lhs->id]) {
                inst->lhs = const_map[inst->lhs->id];
                any_changed = true;
            }
            if (inst->rhs && inst->rhs->kind == IR_VAL_REG && inst->rhs->id < max_id && const_map[inst->rhs->id]) {
                inst->rhs = const_map[inst->rhs->id];
                any_changed = true;
            }
            for (int a = 0; a < inst->arg_count; a++) {
                if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && inst->args[a]->id < max_id && const_map[inst->args[a]->id]) {
                    inst->args[a] = const_map[inst->args[a]->id];
                    any_changed = true;
                }
            }
            for (int p = 0; p < inst->phi_count; p++) {
                if (inst->phi_values[p] && inst->phi_values[p]->kind == IR_VAL_REG && inst->phi_values[p]->id < max_id && const_map[inst->phi_values[p]->id]) {
                    inst->phi_values[p] = const_map[inst->phi_values[p]->id];
                    any_changed = true;
                }
            }

            /* Fold constant arithmetic */
            if (inst->lhs && inst->rhs && inst->lhs->kind == IR_VAL_CONST_INT && inst->rhs->kind == IR_VAL_CONST_INT) {
                int64_t a = inst->lhs->const_val.int_val;
                int64_t b = inst->rhs->const_val.int_val;
                int64_t res = 0;
                bool can_fold = true;
                switch (inst->op) {
                    case IR_OP_ADD: res = a + b; break;
                    case IR_OP_SUB: res = a - b; break;
                    case IR_OP_MUL: res = a * b; break;
                    case IR_OP_DIV: if (b != 0) res = a / b; else can_fold = false; break;
                    case IR_OP_MOD: if (b != 0) res = a % b; else can_fold = false; break;
                    default: can_fold = false; break;
                }
                if (can_fold) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_int(arena, inst->type, res);
                    inst->rhs = NULL;
                    any_changed = true;
                }
            }
        }
    }
    free(const_map);

    /* 3. SSA Dead Code Elimination */
    int *use_counts = (int *)calloc(max_id, sizeof(int));
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_id) use_counts[inst->lhs->id]++;
            if (inst->rhs && inst->rhs->kind == IR_VAL_REG && inst->rhs->id < max_id) use_counts[inst->rhs->id]++;
            for (int a = 0; a < inst->arg_count; a++) {
                if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && inst->args[a]->id < max_id) use_counts[inst->args[a]->id]++;
            }
            for (int p = 0; p < inst->phi_count; p++) {
                if (inst->phi_values[p] && inst->phi_values[p]->kind == IR_VAL_REG && inst->phi_values[p]->id < max_id) use_counts[inst->phi_values[p]->id]++;
            }
        }
    }

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        IrInstruction *inst = bb->first_inst;
        while (inst) {
            IrInstruction *next = inst->next;
            if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_id) {
                if (use_counts[inst->result->id] == 0) {
                    /* Only eliminate pure instructions */
                    switch (inst->op) {
                        case IR_OP_CONST:
                        case IR_OP_ADD:
                        case IR_OP_SUB:
                        case IR_OP_MUL:
                        case IR_OP_DIV:
                        case IR_OP_MOD:
                        case IR_OP_NEG:
                        case IR_OP_EQ:
                        case IR_OP_NE:
                        case IR_OP_LT:
                        case IR_OP_LE:
                        case IR_OP_GT:
                        case IR_OP_GE:
                        case IR_OP_AND:
                        case IR_OP_OR:
                        case IR_OP_NOT:
                        case IR_OP_PHI:
                            remove_inst(bb, inst);
                            any_changed = true;
                            break;
                        default:
                            break;
                    }
                }
            }
            inst = next;
        }
    }
    free(use_counts);

    if (out_changed) *out_changed = any_changed;
    return ir_ssa_verify_function(fn, NULL, out_error);
}

bool ir_ssa_optimize_module(IrModule *module, bool *out_changed, char **out_error) {
    if (!module) return false;
    bool any_changed = false;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        bool step_changed = false;
        if (!ir_ssa_optimize_function(fn, &step_changed, out_error)) {
            return false;
        }
        any_changed |= step_changed;
    }
    if (out_changed) *out_changed = any_changed;
    return true;
}

/* SSA Destruction / Lowering (Critical Edges & Parallel Copies)  */

typedef struct {
    IrValue *dst;
    IrValue *src;
} ParallelCopy;

static bool has_phi_nodes(IrBasicBlock *bb) {
    return (bb->first_inst && bb->first_inst->op == IR_OP_PHI);
}

static void split_critical_edges(IrFunction *fn, IrSsaStats *out_stats) {
    IrArena *arena = fn->module->arena;

    IrBasicBlock *bb = fn->first_block;
    while (bb) {
        IrBasicBlock *next_bb = bb->next;

        if (bb->succ_count > 1) {
            for (int s = 0; s < bb->succ_count; s++) {
                IrBasicBlock *succ = bb->successors[s];

                /* Edge bb -> succ is critical if succ has multiple predecessors and contains phi nodes */
                if (succ->pred_count > 1 && has_phi_nodes(succ)) {
                    /* Create intermediate split block */
                    char split_name[64];
                    snprintf(split_name, sizeof(split_name), "split.%d.%d", bb->id, succ->id);
                    IrBasicBlock *split_bb = ir_block_create(fn, split_name);

                    /* Split block jumps unconditionally to succ */
                    IrInstruction *br_inst = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
                    br_inst->op = IR_OP_BR;
                    br_inst->type = ir_type_void(arena);
                    br_inst->target_true = succ;
                    ir_block_add_instruction(split_bb, br_inst);

                    /* In bb, update branch target to split_bb */
                    if (bb->last_inst) {
                        if (bb->last_inst->target_true == succ) bb->last_inst->target_true = split_bb;
                        if (bb->last_inst->target_false == succ) bb->last_inst->target_false = split_bb;
                    }

                    /* In succ's phi nodes, update incoming predecessor pointer from bb to split_bb */
                    for (IrInstruction *phi = succ->first_inst; phi && phi->op == IR_OP_PHI; phi = phi->next) {
                        for (int p = 0; p < phi->phi_count; p++) {
                            if (phi->phi_blocks[p] == bb) {
                                phi->phi_blocks[p] = split_bb;
                            }
                        }
                    }

                    if (out_stats) out_stats->split_edges++;
                }
            }
        }

        bb = next_bb;
    }

    ir_recompute_cfg(fn);
}

static void emit_sequential_copies_before_term(IrBasicBlock *p, ParallelCopy *copies, int count, IrArena *arena, IrFunction *fn) {
    if (count == 0) return;

    bool *emitted = (bool *)calloc(count, sizeof(bool));
    int remaining = count;

    /* Cycle-breaking loop */
    while (remaining > 0) {
        /* Look for a copy whose destination is not used as a source in any other remaining copy */
        int found = -1;
        for (int i = 0; i < count; i++) {
            if (emitted[i]) continue;

            bool dst_used_as_src = false;
            for (int j = 0; j < count; j++) {
                if (!emitted[j] && i != j && same_val(copies[j].src, copies[i].dst)) {
                    dst_used_as_src = true;
                    break;
                }
            }

            if (!dst_used_as_src) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            /* Emit copy: dst = src */
            IrInstruction *cp = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
            cp->op = IR_OP_CONST;
            cp->type = copies[found].dst->type;
            cp->lhs = copies[found].src;
            cp->result = copies[found].dst;

            /* Insert immediately before terminator */
            IrInstruction *term = p->last_inst;
            if (term) {
                cp->prev = term->prev;
                cp->next = term;
                if (term->prev) term->prev->next = cp;
                else p->first_inst = cp;
                term->prev = cp;
            } else {
                ir_block_add_instruction(p, cp);
            }
            p->inst_count++;

            emitted[found] = true;
            remaining--;
        } else {
            /* A cycle exists! (e.g. a <- b, b <- a). Break cycle with temporary */
            int cycle_idx = -1;
            for (int i = 0; i < count; i++) {
                if (!emitted[i]) { cycle_idx = i; break; }
            }

            IrValue *tmp = ir_val_reg(arena, copies[cycle_idx].src->type, fn->next_reg_id++);
            IrInstruction *cp_tmp = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
            cp_tmp->op = IR_OP_CONST;
            cp_tmp->type = tmp->type;
            cp_tmp->lhs = copies[cycle_idx].src;
            cp_tmp->result = tmp;

            IrInstruction *term = p->last_inst;
            if (term) {
                cp_tmp->prev = term->prev;
                cp_tmp->next = term;
                if (term->prev) term->prev->next = cp_tmp;
                else p->first_inst = cp_tmp;
                term->prev = cp_tmp;
            } else {
                ir_block_add_instruction(p, cp_tmp);
            }
            p->inst_count++;

            /* Update the source of cycle_idx to tmp, breaking cycle */
            copies[cycle_idx].src = tmp;
        }
    }

    free(emitted);
}

bool ir_ssa_deconstruct_function(IrFunction *fn, IrSsaStats *out_stats, char **out_error) {
    if (!fn) return false;
    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));
    IrArena *arena = fn->module->arena;

    /* 1. Split Critical Edges to guarantee safe insertion points */
    split_critical_edges(fn, out_stats);

    /* 2. Lower Phi nodes to parallel copies in predecessors */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (!has_phi_nodes(bb)) continue;

        /* For each predecessor, collect needed copies */
        for (int p_idx = 0; p_idx < bb->pred_count; p_idx++) {
            IrBasicBlock *pred = bb->predecessors[p_idx];

            ParallelCopy copies[64];
            int copy_count = 0;

            for (IrInstruction *phi = bb->first_inst; phi && phi->op == IR_OP_PHI; phi = phi->next) {
                for (int i = 0; i < phi->phi_count; i++) {
                    if (phi->phi_blocks[i] == pred) {
                        IrValue *src = phi->phi_values[i];
                        IrValue *dst = phi->result;
                        if (!same_val(src, dst) && copy_count < 64) {
                            copies[copy_count].dst = dst;
                            copies[copy_count].src = src;
                            copy_count++;
                        }
                        break;
                    }
                }
            }

            emit_sequential_copies_before_term(pred, copies, copy_count, arena, fn);
        }

        /* 3. Remove all phi nodes from bb */
        IrInstruction *inst = bb->first_inst;
        while (inst && inst->op == IR_OP_PHI) {
            IrInstruction *next = inst->next;
            remove_inst(bb, inst);
            inst = next;
        }
    }

    /* 4. Recompute CFG edges and verify lowered IR */
    ir_recompute_cfg(fn);

    char *v_err = NULL;
    if (!ir_verify_function(fn, &v_err)) {
        if (out_error) *out_error = v_err;
        else if (v_err) free(v_err);
        return false;
    }

    return true;
}

bool ir_ssa_deconstruct_module(IrModule *module, IrSsaStats *out_stats, char **out_error) {
    if (!module) return false;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_ssa_deconstruct_function(fn, out_stats, out_error)) {
            return false;
        }
    }
    return true;
}

/* High-Level SSA Pipeline Driver                                 */

bool ir_ssa_pipeline_function(IrFunction *fn, const IrSsaOptions *opts, IrSsaStats *out_stats, char **out_error) {
    if (!fn) return false;

    /* 1. SSA Construction */
    if (!ir_ssa_construct_function(fn, out_stats, out_error)) {
        return false;
    }

    /* 2. Optional Dump */
    if (opts && opts->dump_ssa) {
        ir_dump_ssa_function(stdout, fn);
    }

    /* 3. Optional SSA-level Optimization */
    if (opts && opts->optimize_ssa) {
        if (!ir_ssa_advanced_optimize_function(fn, opts, NULL, out_error)) {
            return false;
        }
    }

    /* 4. Optional Post-Optimization SSA Dump */
    if (opts && opts->dump_ssa_opt) {
        ir_dump_ssa_function(stdout, fn);
    }

    /* 5. SSA Verification */
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) {
            return false;
        }
    }

    /* 6. SSA Destruction to standard non-SSA IR */
    if (!ir_ssa_deconstruct_function(fn, out_stats, out_error)) {
        return false;
    }

    return true;
}

bool ir_ssa_pipeline_module(IrModule *module, const IrSsaOptions *opts, IrSsaStats *out_stats, char **out_error) {
    if (!module) return false;

    bool is_ipa = (opts && (opts->opt_level >= 2 || opts->enable_inlining || opts->enable_dfe || opts->dump_callgraph));

    if (!is_ipa) {
        /* Standard Phase 5/6 function-by-function SSA pipeline */
        for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
            if (!ir_ssa_pipeline_function(fn, opts, out_stats, out_error)) {
                return false;
            }
        }
        return true;
    }

    /* Phase 7: Full Interprocedural Pipeline */
    /* 1. SSA Construction across all functions */
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_ssa_construct_function(fn, out_stats, out_error)) {
            return false;
        }
    }

    /* 2. Optional initial SSA dump */
    if (opts && opts->dump_ssa) {
        ir_dump_ssa_module(stdout, module);
    }

    /* 3. Initial Intra-procedural SSA optimizations */
    if (opts && opts->optimize_ssa) {
        for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
            if (!ir_ssa_advanced_optimize_function(fn, opts, NULL, out_error)) {
                return false;
            }
        }
    }

    /* 4. Call Graph Analysis */
    IrCallGraph *cg = ir_callgraph_build(module);
    if (!cg) return false;

    if (opts && opts->dump_callgraph) {
        ir_callgraph_dump(stdout, cg);
    }

    /* 5. Controlled Inlining */
    bool do_inline = (opts && (opts->enable_inlining || opts->opt_level >= 2));
    if (do_inline) {
        int threshold = (opts && opts->inline_threshold > 0) ? opts->inline_threshold : IR_DEFAULT_INLINE_THRESHOLD;

        for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
            int fn_inlined = 0;
            if (!ir_ipa_inline_function_pgo(fn, cg, threshold, opts ? opts->profile : NULL, &fn_inlined, out_error)) {
                ir_callgraph_free(cg);
                return false;
            }
            if (fn_inlined > 0) {
                /* Post-inline SSA optimization: folds propagated constant arguments */
                ir_ssa_advanced_optimize_function(fn, opts, NULL, out_error);
            }
        }
    }

    ir_callgraph_free(cg);

    /* 6. Dead Function Elimination (DFE) */
    bool do_dfe = (opts && (opts->enable_dfe || opts->opt_level >= 2));
    if (do_dfe) {
        IrCallGraph *post_cg = ir_callgraph_build(module);
        if (post_cg) {
            int removed = 0;
            ir_ipa_eliminate_dead_functions(module, post_cg, &removed, out_error);
            ir_callgraph_free(post_cg);
        }
    }

    /* 7. Optional post-optimization SSA dump */
    if (opts && opts->dump_ssa_opt) {
        ir_dump_ssa_module(stdout, module);
    }

    /* 8. SSA Verification */
    if (opts && opts->verify_ssa) {
        for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
            if (!ir_ssa_verify_function(fn, NULL, out_error)) {
                return false;
            }
        }
    }

    /* 9. SSA Deconstruction to standard non-SSA IR */
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_ssa_deconstruct_function(fn, out_stats, out_error)) {
            return false;
        }
    }

    return true;
}

/* Diagnostics & Dumps                                                       */

void ir_dump_ssa_function(FILE *out, IrFunction *fn) {
    if (!out || !fn) return;
    ir_dump_function(out, fn);
}

void ir_dump_ssa_module(FILE *out, IrModule *module) {
    if (!out || !module) return;
    ir_dump_module(out, module);
}
