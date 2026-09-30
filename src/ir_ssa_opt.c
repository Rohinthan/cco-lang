// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_ssa_opt.h"
#include "ir_dominance.h"
#include "ir_loop.h"
#include "ir_verify.h"
#include "ir_print.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* Helper Functions                                                          */

static bool same_val(const IrValue *a, const IrValue *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->kind == IR_VAL_REG) return (a->id == b->id);
    if (a->kind == IR_VAL_CONST_INT) return (a->const_val.int_val == b->const_val.int_val);
    if (a->kind == IR_VAL_CONST_FLOAT) return (a->const_val.float_val == b->const_val.float_val);
    if (a->kind == IR_VAL_CONST_BOOL) return (a->const_val.bool_val == b->const_val.bool_val);
    if (a->kind == IR_VAL_CONST_CHAR) return (a->const_val.char_val == b->const_val.char_val);
    if (a->kind == IR_VAL_CONST_STRING) {
        if (!a->const_val.str_val || !b->const_val.str_val) return false;
        return (strcmp(a->const_val.str_val, b->const_val.str_val) == 0);
    }
    return false;
}

static void replace_uses_in_fn(IrFunction *fn, const IrValue *old_val, IrValue *new_val) {
    if (!fn || !old_val || !new_val || same_val(old_val, new_val)) return;

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (same_val(inst->lhs, old_val)) inst->lhs = new_val;
            if (same_val(inst->rhs, old_val)) inst->rhs = new_val;
            for (int i = 0; i < inst->arg_count; i++) {
                if (same_val(inst->args[i], old_val)) inst->args[i] = new_val;
            }
            for (int p = 0; p < inst->phi_count; p++) {
                if (same_val(inst->phi_values[p], old_val)) inst->phi_values[p] = new_val;
            }
        }
    }
}

static void remove_inst(IrBasicBlock *bb, IrInstruction *inst) {
    if (!bb || !inst) return;
    if (inst->prev) inst->prev->next = inst->next;
    else bb->first_inst = inst->next;

    if (inst->next) inst->next->prev = inst->prev;
    else bb->last_inst = inst->prev;

    inst->prev = NULL;
    inst->next = NULL;
    bb->inst_count--;
}

static void remove_block_from_fn(IrFunction *fn, IrBasicBlock *target) {
    if (!fn || !target) return;
    if (fn->first_block == target) {
        fn->first_block = target->next;
        if (fn->last_block == target) fn->last_block = NULL;
        fn->block_count--;
        return;
    }
    for (IrBasicBlock *curr = fn->first_block; curr; curr = curr->next) {
        if (curr->next == target) {
            curr->next = target->next;
            if (fn->last_block == target) fn->last_block = curr;
            fn->block_count--;
            return;
        }
    }
}

static bool is_pure_operation(IrOpcode op) {
    switch (op) {
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
            return true;
        default:
            return false;
    }
}

static bool is_commutative(IrOpcode op) {
    switch (op) {
        case IR_OP_ADD:
        case IR_OP_MUL:
        case IR_OP_EQ:
        case IR_OP_NE:
        case IR_OP_AND:
        case IR_OP_OR:
            return true;
        default:
            return false;
    }
}

/*Sparse Conditional Constant Propagation (SCCP)                */


typedef enum {
    LATTICE_TOP = 0,     /* Uninitialized / unreached */
    LATTICE_CONST,       /* Known constant */
    LATTICE_BOT          /* Overdefined / varying */
} LatticeKind;

typedef struct {
    LatticeKind kind;
    IrValue *val;        /* Constant value if LATTICE_CONST */
} LatticeVal;

typedef struct {
    IrBasicBlock *from;
    IrBasicBlock *to;
} CfgEdge;

static IrValue *eval_binary_op(IrArena *arena, IrOpcode op, IrType *type, const IrValue *lhs, const IrValue *rhs) {
    if (!lhs || !rhs) return NULL;

    /* Integer Arithmetic */
    if (lhs->kind == IR_VAL_CONST_INT && rhs->kind == IR_VAL_CONST_INT) {
        int64_t a = lhs->const_val.int_val;
        int64_t b = rhs->const_val.int_val;
        switch (op) {
            case IR_OP_ADD: return ir_val_const_int(arena, type, a + b);
            case IR_OP_SUB: return ir_val_const_int(arena, type, a - b);
            case IR_OP_MUL: return ir_val_const_int(arena, type, a * b);
            case IR_OP_DIV: if (b != 0) return ir_val_const_int(arena, type, a / b); break;
            case IR_OP_MOD: if (b != 0) return ir_val_const_int(arena, type, a % b); break;
            case IR_OP_EQ:  return ir_val_const_bool(arena, a == b);
            case IR_OP_NE:  return ir_val_const_bool(arena, a != b);
            case IR_OP_LT:  return ir_val_const_bool(arena, a < b);
            case IR_OP_LE:  return ir_val_const_bool(arena, a <= b);
            case IR_OP_GT:  return ir_val_const_bool(arena, a > b);
            case IR_OP_GE:  return ir_val_const_bool(arena, a >= b);
            default: break;
        }
    }

    /* Floating Point Arithmetic (Preserve NaN / signed zero behavior, no unsafe reassociation) */
    if (lhs->kind == IR_VAL_CONST_FLOAT && rhs->kind == IR_VAL_CONST_FLOAT) {
        double a = lhs->const_val.float_val;
        double b = rhs->const_val.float_val;
        switch (op) {
            case IR_OP_ADD: return ir_val_const_float(arena, a + b);
            case IR_OP_SUB: return ir_val_const_float(arena, a - b);
            case IR_OP_MUL: return ir_val_const_float(arena, a * b);
            case IR_OP_DIV: if (b != 0.0) return ir_val_const_float(arena, a / b); break;
            case IR_OP_EQ:  return ir_val_const_bool(arena, a == b);
            case IR_OP_NE:  return ir_val_const_bool(arena, a != b);
            case IR_OP_LT:  return ir_val_const_bool(arena, a < b);
            case IR_OP_LE:  return ir_val_const_bool(arena, a <= b);
            case IR_OP_GT:  return ir_val_const_bool(arena, a > b);
            case IR_OP_GE:  return ir_val_const_bool(arena, a >= b);
            default: break;
        }
    }

    /* Boolean Logic */
    if (lhs->kind == IR_VAL_CONST_BOOL && rhs->kind == IR_VAL_CONST_BOOL) {
        bool a = lhs->const_val.bool_val;
        bool b = rhs->const_val.bool_val;
        switch (op) {
            case IR_OP_AND: return ir_val_const_bool(arena, a && b);
            case IR_OP_OR:  return ir_val_const_bool(arena, a || b);
            case IR_OP_EQ:  return ir_val_const_bool(arena, a == b);
            case IR_OP_NE:  return ir_val_const_bool(arena, a != b);
            default: break;
        }
    }

    return NULL;
}

static IrValue *eval_unary_op(IrArena *arena, IrOpcode op, IrType *type, const IrValue *val) {
    if (!val) return NULL;
    if (op == IR_OP_NEG) {
        if (val->kind == IR_VAL_CONST_INT) {
            return ir_val_const_int(arena, type, -val->const_val.int_val);
        } else if (val->kind == IR_VAL_CONST_FLOAT) {
            return ir_val_const_float(arena, -val->const_val.float_val);
        }
    } else if (op == IR_OP_NOT) {
        if (val->kind == IR_VAL_CONST_BOOL) {
            return ir_val_const_bool(arena, !val->const_val.bool_val);
        }
    }
    return NULL;
}

bool ir_ssa_sccp(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error) {
    (void)out_error;
    if (!fn) return false;
    bool changed = false;

    ir_recompute_cfg(fn);

    int max_regs = fn->next_reg_id + 64;
    LatticeVal *lat = (LatticeVal *)calloc(max_regs, sizeof(LatticeVal));

    /* Parameters are incoming dynamic arguments -> OVERDEFINED */
    for (int p = 0; p < fn->param_count; p++) {
        if (fn->param_values && fn->param_values[p] && fn->param_values[p]->id < max_regs) {
            lat[fn->param_values[p]->id].kind = LATTICE_BOT;
        }
    }

    /* Assign dense indices to basic blocks */
    int block_idx = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        bb->id = block_idx++;
    }
    int total_blocks = block_idx;

    /* Executable states */
    bool *block_exec = (bool *)calloc(total_blocks, sizeof(bool));
    bool **edge_exec = (bool **)calloc(total_blocks, sizeof(bool *));
    for (int i = 0; i < total_blocks; i++) {
        edge_exec[i] = (bool *)calloc(total_blocks, sizeof(bool));
    }

    /* Worklists */
    CfgEdge *cfg_wl = (CfgEdge *)malloc((total_blocks * total_blocks + 16) * sizeof(CfgEdge));
    int cfg_head = 0, cfg_tail = 0;

    int *ssa_wl = (int *)malloc(max_regs * 4 * sizeof(int));
    int ssa_head = 0, ssa_tail = 0;
    bool *in_ssa_wl = (bool *)calloc(max_regs, sizeof(bool));

    /* Entry block is executable from a synthetic start */
    if (fn->entry_block) {
        cfg_wl[cfg_tail++] = (CfgEdge){ .from = NULL, .to = fn->entry_block };
    }

    IrArena *arena = fn->module->arena;

    /* Fixed-point iteration */
    while (cfg_head < cfg_tail || ssa_head < ssa_tail) {
        /* 1. Process CFG Worklist */
        while (cfg_head < cfg_tail) {
            CfgEdge edge = cfg_wl[cfg_head++];
            IrBasicBlock *dest = edge.to;
            if (!dest) continue;

            bool first_visit = !block_exec[dest->id];
            block_exec[dest->id] = true;

            if (edge.from) {
                edge_exec[edge.from->id][dest->id] = true;
            }

            if (first_visit) {
                /* Evaluate all instructions in dest */
                for (IrInstruction *inst = dest->first_inst; inst; inst = inst->next) {
                    if (inst->op == IR_OP_PHI) {
                        /* Evaluate phi */
                        LatticeKind res_kind = LATTICE_TOP;
                        IrValue *res_val = NULL;

                        for (int p = 0; p < inst->phi_count; p++) {
                            IrBasicBlock *pred = inst->phi_blocks[p];
                            if (!pred || !edge_exec[pred->id][dest->id]) {
                                continue; /* Unexecuted incoming edge */
                            }

                            IrValue *in_val = inst->phi_values[p];
                            LatticeVal in_lat = { .kind = LATTICE_TOP, .val = NULL };
                            if (in_val->kind == IR_VAL_REG && in_val->id < max_regs) {
                                in_lat = lat[in_val->id];
                            } else if (in_val->kind >= IR_VAL_CONST_INT && in_val->kind <= IR_VAL_CONST_STRING) {
                                in_lat.kind = LATTICE_CONST;
                                in_lat.val = in_val;
                            } else {
                                in_lat.kind = LATTICE_BOT;
                            }

                            if (in_lat.kind == LATTICE_BOT) {
                                res_kind = LATTICE_BOT;
                                break;
                            } else if (in_lat.kind == LATTICE_CONST) {
                                if (res_kind == LATTICE_TOP) {
                                    res_kind = LATTICE_CONST;
                                    res_val = in_lat.val;
                                } else if (res_kind == LATTICE_CONST) {
                                    if (!same_val(res_val, in_lat.val)) {
                                        res_kind = LATTICE_BOT;
                                        break;
                                    }
                                }
                            }
                        }

                        if (inst->result && inst->result->id < max_regs) {
                            int rid = inst->result->id;
                            if (lat[rid].kind != res_kind || (res_kind == LATTICE_CONST && !same_val(lat[rid].val, res_val))) {
                                lat[rid].kind = res_kind;
                                lat[rid].val = res_val;
                                if (!in_ssa_wl[rid]) {
                                    ssa_wl[ssa_tail++] = rid;
                                    in_ssa_wl[rid] = true;
                                }
                            }
                        }
                    } else if (inst->op == IR_OP_CONST && inst->result && inst->result->id < max_regs) {
                        int rid = inst->result->id;
                        if (lat[rid].kind != LATTICE_CONST) {
                            lat[rid].kind = LATTICE_CONST;
                            lat[rid].val = inst->lhs;
                            if (!in_ssa_wl[rid]) {
                                ssa_wl[ssa_tail++] = rid;
                                in_ssa_wl[rid] = true;
                            }
                        }
                    } else if (is_pure_operation(inst->op) && inst->result && inst->result->id < max_regs) {
                        /* Evaluate pure operation */
                        LatticeVal l_lat = { .kind = LATTICE_BOT, .val = NULL };
                        LatticeVal r_lat = { .kind = LATTICE_BOT, .val = NULL };

                        if (inst->lhs) {
                            if (inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_regs) l_lat = lat[inst->lhs->id];
                            else if (inst->lhs->kind >= IR_VAL_CONST_INT && inst->lhs->kind <= IR_VAL_CONST_STRING) {
                                l_lat.kind = LATTICE_CONST; l_lat.val = inst->lhs;
                            }
                        }
                        if (inst->rhs) {
                            if (inst->rhs->kind == IR_VAL_REG && inst->rhs->id < max_regs) r_lat = lat[inst->rhs->id];
                            else if (inst->rhs->kind >= IR_VAL_CONST_INT && inst->rhs->kind <= IR_VAL_CONST_STRING) {
                                r_lat.kind = LATTICE_CONST; r_lat.val = inst->rhs;
                            }
                        }

                        int rid = inst->result->id;
                        if (inst->op == IR_OP_NEG || inst->op == IR_OP_NOT) {
                            if (l_lat.kind == LATTICE_CONST) {
                                IrValue *folded = eval_unary_op(arena, inst->op, inst->type, l_lat.val);
                                if (folded) {
                                    lat[rid].kind = LATTICE_CONST;
                                    lat[rid].val = folded;
                                } else {
                                    lat[rid].kind = LATTICE_BOT;
                                }
                            } else {
                                lat[rid].kind = l_lat.kind;
                            }
                        } else {
                            if (l_lat.kind == LATTICE_BOT || r_lat.kind == LATTICE_BOT) {
                                lat[rid].kind = LATTICE_BOT;
                            } else if (l_lat.kind == LATTICE_CONST && r_lat.kind == LATTICE_CONST) {
                                IrValue *folded = eval_binary_op(arena, inst->op, inst->type, l_lat.val, r_lat.val);
                                if (folded) {
                                    lat[rid].kind = LATTICE_CONST;
                                    lat[rid].val = folded;
                                } else {
                                    lat[rid].kind = LATTICE_BOT;
                                }
                            } else {
                                lat[rid].kind = LATTICE_TOP;
                            }
                        }

                        if (lat[rid].kind != LATTICE_TOP && !in_ssa_wl[rid]) {
                            ssa_wl[ssa_tail++] = rid;
                            in_ssa_wl[rid] = true;
                        }
                    } else if (inst->op == IR_OP_BR && inst->target_true) {
                        cfg_wl[cfg_tail++] = (CfgEdge){ .from = dest, .to = inst->target_true };
                    } else if (inst->op == IR_OP_CONDBR && inst->lhs) {
                        LatticeVal c_lat = { .kind = LATTICE_BOT, .val = NULL };
                        if (inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_regs) c_lat = lat[inst->lhs->id];
                        else if (inst->lhs->kind >= IR_VAL_CONST_INT && inst->lhs->kind <= IR_VAL_CONST_STRING) {
                            c_lat.kind = LATTICE_CONST; c_lat.val = inst->lhs;
                        }

                        if (c_lat.kind == LATTICE_CONST) {
                            bool take_true = false;
                            if (c_lat.val->kind == IR_VAL_CONST_BOOL) take_true = c_lat.val->const_val.bool_val;
                            else if (c_lat.val->kind == IR_VAL_CONST_INT) take_true = (c_lat.val->const_val.int_val != 0);

                            if (take_true && inst->target_true) {
                                cfg_wl[cfg_tail++] = (CfgEdge){ .from = dest, .to = inst->target_true };
                            } else if (!take_true && inst->target_false) {
                                cfg_wl[cfg_tail++] = (CfgEdge){ .from = dest, .to = inst->target_false };
                            }
                        } else {
                            if (inst->target_true) cfg_wl[cfg_tail++] = (CfgEdge){ .from = dest, .to = inst->target_true };
                            if (inst->target_false) cfg_wl[cfg_tail++] = (CfgEdge){ .from = dest, .to = inst->target_false };
                        }
                    } else {
                        /* Side-effecting / call / load / etc. -> OVERDEFINED */
                        if (inst->result && inst->result->id < max_regs) {
                            lat[inst->result->id].kind = LATTICE_BOT;
                        }
                    }
                }
            } else {
                /* Block was already visited: re-evaluate phis only */
                for (IrInstruction *phi = dest->first_inst; phi && phi->op == IR_OP_PHI; phi = phi->next) {
                    LatticeKind res_kind = LATTICE_TOP;
                    IrValue *res_val = NULL;

                    for (int p = 0; p < phi->phi_count; p++) {
                        IrBasicBlock *pred = phi->phi_blocks[p];
                        if (!pred || !edge_exec[pred->id][dest->id]) continue;

                        IrValue *in_val = phi->phi_values[p];
                        LatticeVal in_lat = { .kind = LATTICE_TOP, .val = NULL };
                        if (in_val->kind == IR_VAL_REG && in_val->id < max_regs) in_lat = lat[in_val->id];
                        else if (in_val->kind >= IR_VAL_CONST_INT && in_val->kind <= IR_VAL_CONST_STRING) {
                            in_lat.kind = LATTICE_CONST; in_lat.val = in_val;
                        } else {
                            in_lat.kind = LATTICE_BOT;
                        }

                        if (in_lat.kind == LATTICE_BOT) {
                            res_kind = LATTICE_BOT;
                            break;
                        } else if (in_lat.kind == LATTICE_CONST) {
                            if (res_kind == LATTICE_TOP) {
                                res_kind = LATTICE_CONST;
                                res_val = in_lat.val;
                            } else if (res_kind == LATTICE_CONST) {
                                if (!same_val(res_val, in_lat.val)) {
                                    res_kind = LATTICE_BOT;
                                    break;
                                }
                            }
                        }
                    }

                    if (phi->result && phi->result->id < max_regs) {
                        int rid = phi->result->id;
                        if (lat[rid].kind != res_kind || (res_kind == LATTICE_CONST && !same_val(lat[rid].val, res_val))) {
                            lat[rid].kind = res_kind;
                            lat[rid].val = res_val;
                            if (!in_ssa_wl[rid]) {
                                ssa_wl[ssa_tail++] = rid;
                                in_ssa_wl[rid] = true;
                            }
                        }
                    }
                }
            }
        }

        /* 2. Process SSA Worklist */
        while (ssa_head < ssa_tail) {
            int changed_reg = ssa_wl[ssa_head++];
            in_ssa_wl[changed_reg] = false;

            /* Find all instructions in executable blocks using changed_reg */
            for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
                if (!block_exec[bb->id]) continue;

                for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                    bool uses_reg = false;
                    if (inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id == changed_reg) uses_reg = true;
                    if (inst->rhs && inst->rhs->kind == IR_VAL_REG && inst->rhs->id == changed_reg) uses_reg = true;
                    for (int a = 0; a < inst->arg_count; a++) {
                        if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && inst->args[a]->id == changed_reg) uses_reg = true;
                    }
                    for (int p = 0; p < inst->phi_count; p++) {
                        if (inst->phi_values[p] && inst->phi_values[p]->kind == IR_VAL_REG && inst->phi_values[p]->id == changed_reg) uses_reg = true;
                    }

                    if (!uses_reg) continue;

                    /* Re-evaluate inst */
                    if (inst->op == IR_OP_PHI && inst->result && inst->result->id < max_regs) {
                        LatticeKind res_kind = LATTICE_TOP;
                        IrValue *res_val = NULL;

                        for (int p = 0; p < inst->phi_count; p++) {
                            IrBasicBlock *pred = inst->phi_blocks[p];
                            if (!pred || !edge_exec[pred->id][bb->id]) continue;

                            IrValue *in_val = inst->phi_values[p];
                            LatticeVal in_lat = { .kind = LATTICE_TOP, .val = NULL };
                            if (in_val->kind == IR_VAL_REG && in_val->id < max_regs) in_lat = lat[in_val->id];
                            else if (in_val->kind >= IR_VAL_CONST_INT && in_val->kind <= IR_VAL_CONST_STRING) {
                                in_lat.kind = LATTICE_CONST; in_lat.val = in_val;
                            } else {
                                in_lat.kind = LATTICE_BOT;
                            }

                            if (in_lat.kind == LATTICE_BOT) {
                                res_kind = LATTICE_BOT;
                                break;
                            } else if (in_lat.kind == LATTICE_CONST) {
                                if (res_kind == LATTICE_TOP) {
                                    res_kind = LATTICE_CONST;
                                    res_val = in_lat.val;
                                } else if (res_kind == LATTICE_CONST) {
                                    if (!same_val(res_val, in_lat.val)) {
                                        res_kind = LATTICE_BOT;
                                        break;
                                    }
                                }
                            }
                        }

                        int rid = inst->result->id;
                        if (lat[rid].kind != res_kind || (res_kind == LATTICE_CONST && !same_val(lat[rid].val, res_val))) {
                            lat[rid].kind = res_kind;
                            lat[rid].val = res_val;
                            if (!in_ssa_wl[rid]) {
                                ssa_wl[ssa_tail++] = rid;
                                in_ssa_wl[rid] = true;
                            }
                        }
                    } else if (is_pure_operation(inst->op) && inst->result && inst->result->id < max_regs) {
                        LatticeVal l_lat = { .kind = LATTICE_BOT, .val = NULL };
                        LatticeVal r_lat = { .kind = LATTICE_BOT, .val = NULL };

                        if (inst->lhs) {
                            if (inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_regs) l_lat = lat[inst->lhs->id];
                            else if (inst->lhs->kind >= IR_VAL_CONST_INT && inst->lhs->kind <= IR_VAL_CONST_STRING) {
                                l_lat.kind = LATTICE_CONST; l_lat.val = inst->lhs;
                            }
                        }
                        if (inst->rhs) {
                            if (inst->rhs->kind == IR_VAL_REG && inst->rhs->id < max_regs) r_lat = lat[inst->rhs->id];
                            else if (inst->rhs->kind >= IR_VAL_CONST_INT && inst->rhs->kind <= IR_VAL_CONST_STRING) {
                                r_lat.kind = LATTICE_CONST; r_lat.val = inst->rhs;
                            }
                        }

                        int rid = inst->result->id;
                        LatticeKind old_kind = lat[rid].kind;
                        IrValue *old_val = lat[rid].val;

                        if (inst->op == IR_OP_NEG || inst->op == IR_OP_NOT) {
                            if (l_lat.kind == LATTICE_CONST) {
                                IrValue *folded = eval_unary_op(arena, inst->op, inst->type, l_lat.val);
                                if (folded) {
                                    lat[rid].kind = LATTICE_CONST;
                                    lat[rid].val = folded;
                                } else {
                                    lat[rid].kind = LATTICE_BOT;
                                }
                            } else {
                                lat[rid].kind = l_lat.kind;
                            }
                        } else {
                            if (l_lat.kind == LATTICE_BOT || r_lat.kind == LATTICE_BOT) {
                                lat[rid].kind = LATTICE_BOT;
                            } else if (l_lat.kind == LATTICE_CONST && r_lat.kind == LATTICE_CONST) {
                                IrValue *folded = eval_binary_op(arena, inst->op, inst->type, l_lat.val, r_lat.val);
                                if (folded) {
                                    lat[rid].kind = LATTICE_CONST;
                                    lat[rid].val = folded;
                                } else {
                                    lat[rid].kind = LATTICE_BOT;
                                }
                            } else {
                                lat[rid].kind = LATTICE_TOP;
                            }
                        }

                        if (lat[rid].kind != old_kind || (lat[rid].kind == LATTICE_CONST && !same_val(lat[rid].val, old_val))) {
                            if (!in_ssa_wl[rid]) {
                                ssa_wl[ssa_tail++] = rid;
                                in_ssa_wl[rid] = true;
                            }
                        }
                    } else if (inst->op == IR_OP_CONDBR && inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id == changed_reg) {
                        if (lat[changed_reg].kind == LATTICE_CONST) {
                            bool take_true = false;
                            if (lat[changed_reg].val->kind == IR_VAL_CONST_BOOL) take_true = lat[changed_reg].val->const_val.bool_val;
                            else if (lat[changed_reg].val->kind == IR_VAL_CONST_INT) take_true = (lat[changed_reg].val->const_val.int_val != 0);

                            if (take_true && inst->target_true) {
                                cfg_wl[cfg_tail++] = (CfgEdge){ .from = bb, .to = inst->target_true };
                            } else if (!take_true && inst->target_false) {
                                cfg_wl[cfg_tail++] = (CfgEdge){ .from = bb, .to = inst->target_false };
                            }
                        } else if (lat[changed_reg].kind == LATTICE_BOT) {
                            if (inst->target_true) cfg_wl[cfg_tail++] = (CfgEdge){ .from = bb, .to = inst->target_true };
                            if (inst->target_false) cfg_wl[cfg_tail++] = (CfgEdge){ .from = bb, .to = inst->target_false };
                        }
                    }
                }
            }
        }
    }

    /* 3. Apply SCCP Results */

    /* A. Replace constant registers with literals */
    for (int r = 0; r < max_regs; r++) {
        if (lat[r].kind == LATTICE_CONST && lat[r].val) {
            IrValue *reg_val = ir_val_reg(arena, lat[r].val->type, r);
            replace_uses_in_fn(fn, reg_val, lat[r].val);
            if (metrics) metrics->sccp_constants_found++;
            changed = true;
        }
    }

    /* B. Simplify conditional branches with constant conditions */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (!block_exec[bb->id]) continue;

        if (bb->last_inst && bb->last_inst->op == IR_OP_CONDBR) {
            IrInstruction *br = bb->last_inst;
            LatticeVal c_lat = { .kind = LATTICE_BOT, .val = NULL };
            if (br->lhs) {
                if (br->lhs->kind == IR_VAL_REG && br->lhs->id < max_regs) c_lat = lat[br->lhs->id];
                else if (br->lhs->kind >= IR_VAL_CONST_INT && br->lhs->kind <= IR_VAL_CONST_STRING) {
                    c_lat.kind = LATTICE_CONST; c_lat.val = br->lhs;
                }
            }

            if (c_lat.kind == LATTICE_CONST && c_lat.val) {
                bool take_true = false;
                if (c_lat.val->kind == IR_VAL_CONST_BOOL) take_true = c_lat.val->const_val.bool_val;
                else if (c_lat.val->kind == IR_VAL_CONST_INT) take_true = (c_lat.val->const_val.int_val != 0);

                IrBasicBlock *taken = take_true ? br->target_true : br->target_false;
                br->op = IR_OP_BR;
                br->lhs = NULL;
                br->target_true = taken;
                br->target_false = NULL;

                if (metrics) metrics->sccp_branches_simplified++;
                changed = true;
            }
        }
    }

    /* C. Remove unreachable basic blocks */
    IrBasicBlock *bb = fn->first_block;
    while (bb) {
        IrBasicBlock *next_bb = bb->next;
        if (!block_exec[bb->id] && bb != fn->entry_block) {
            /* Block is unreachable */
            remove_block_from_fn(fn, bb);
            if (metrics) metrics->unreachable_blocks_removed++;
            changed = true;
        }
        bb = next_bb;
    }

    ir_recompute_cfg(fn);

    /* D. Prune unreachable predecessors from phi nodes in remaining blocks */
    for (IrBasicBlock *rbb = fn->first_block; rbb; rbb = rbb->next) {
        IrInstruction *inst = rbb->first_inst;
        while (inst && inst->op == IR_OP_PHI) {
            IrInstruction *next_inst = inst->next;

            int new_phi_cnt = 0;
            IrBasicBlock **new_blocks = (IrBasicBlock **)ir_arena_alloc(arena, inst->phi_count * sizeof(IrBasicBlock *));
            IrValue **new_vals = (IrValue **)ir_arena_alloc(arena, inst->phi_count * sizeof(IrValue *));

            for (int p = 0; p < inst->phi_count; p++) {
                IrBasicBlock *pred = inst->phi_blocks[p];
                if (!pred || pred->id >= total_blocks) continue;

                bool is_real_pred = false;
                for (int bp = 0; bp < rbb->pred_count; bp++) {
                    if (rbb->predecessors[bp] == pred) {
                        is_real_pred = true;
                        break;
                    }
                }

                if (is_real_pred && edge_exec[pred->id][rbb->id]) {
                    new_blocks[new_phi_cnt] = pred;
                    new_vals[new_phi_cnt] = inst->phi_values[p];
                    new_phi_cnt++;
                }
            }

            if (new_phi_cnt == 1 && inst->result) {
                /* Phi has single incoming value -> replace uses directly */
                replace_uses_in_fn(fn, inst->result, new_vals[0]);
                remove_inst(rbb, inst);
                changed = true;
            } else if (new_phi_cnt == 0 && inst->result) {
                remove_inst(rbb, inst);
                changed = true;
            } else {
                inst->phi_blocks = new_blocks;
                inst->phi_values = new_vals;
                inst->phi_count = new_phi_cnt;
            }

            inst = next_inst;
        }
    }

    /* Free SCCP analysis structures */
    free(lat);
    free(block_exec);
    for (int i = 0; i < total_blocks; i++) free(edge_exec[i]);
    free(edge_exec);
    free(cfg_wl);
    free(ssa_wl);
    free(in_ssa_wl);

    ir_recompute_cfg(fn);

    if (out_changed) *out_changed = changed;
    return true;
}

/* Global Value Numbering & Common Subexpression Elimination      */


typedef struct GvnEntry {
    IrOpcode op;
    IrTypeKind type_kind;
    int lhs_reg;
    int rhs_reg;
    int64_t imm_int;
    double imm_float;
    bool imm_bool;

    IrValue *leader;
    struct GvnEntry *next;
} GvnEntry;

typedef struct GvnScope {
    GvnEntry *entries;
    struct GvnScope *parent;
} GvnScope;

static GvnEntry *lookup_gvn(GvnScope *scope, IrOpcode op, IrTypeKind tk, int l_reg, int r_reg, int64_t imm_i, double imm_f, bool imm_b) {
    for (GvnScope *s = scope; s; s = s->parent) {
        for (GvnEntry *e = s->entries; e; e = e->next) {
            if (e->op == op && e->type_kind == tk &&
                e->lhs_reg == l_reg && e->rhs_reg == r_reg &&
                e->imm_int == imm_i && e->imm_float == imm_f && e->imm_bool == imm_b) {
                return e;
            }
        }
    }
    return NULL;
}

static void add_gvn(GvnScope *scope, IrOpcode op, IrTypeKind tk, int l_reg, int r_reg, int64_t imm_i, double imm_f, bool imm_b, IrValue *leader) {
    GvnEntry *e = (GvnEntry *)malloc(sizeof(GvnEntry));
    e->op = op;
    e->type_kind = tk;
    e->lhs_reg = l_reg;
    e->rhs_reg = r_reg;
    e->imm_int = imm_i;
    e->imm_float = imm_f;
    e->imm_bool = imm_b;
    e->leader = leader;
    e->next = scope->entries;
    scope->entries = e;
}

static void free_gvn_entries(GvnEntry *e) {
    while (e) {
        GvnEntry *next = e->next;
        free(e);
        e = next;
    }
}

static void gvn_walk_dom_tree(IrDomNode *node, GvnScope *parent_scope, IrFunction *fn, IrSsaOptMetrics *metrics, bool *changed) {
    if (!node || !node->block) return;
    IrBasicBlock *bb = node->block;

    GvnScope current_scope = { .entries = NULL, .parent = parent_scope };

    IrInstruction *inst = bb->first_inst;
    while (inst) {
        IrInstruction *next_inst = inst->next;

        if (is_pure_operation(inst->op) && inst->result && inst->result->kind == IR_VAL_REG) {
            IrOpcode op = inst->op;
            IrTypeKind tk = inst->type ? inst->type->kind : IR_TYPE_VOID;
            int l_reg = -1;
            int r_reg = -1;
            int64_t imm_i = 0;
            double imm_f = 0.0;
            bool imm_b = false;

            if (inst->lhs) {
                if (inst->lhs->kind == IR_VAL_REG) l_reg = inst->lhs->id;
                else if (inst->lhs->kind == IR_VAL_PARAM) l_reg = -(inst->lhs->id + 10);
                else if (inst->lhs->kind == IR_VAL_CONST_INT) imm_i = inst->lhs->const_val.int_val;
                else if (inst->lhs->kind == IR_VAL_CONST_FLOAT) imm_f = inst->lhs->const_val.float_val;
                else if (inst->lhs->kind == IR_VAL_CONST_BOOL) imm_b = inst->lhs->const_val.bool_val;
            }
            if (inst->rhs) {
                if (inst->rhs->kind == IR_VAL_REG) r_reg = inst->rhs->id;
                else if (inst->rhs->kind == IR_VAL_PARAM) r_reg = -(inst->rhs->id + 10);
                else if (inst->rhs->kind == IR_VAL_CONST_INT) imm_i = inst->rhs->const_val.int_val;
                else if (inst->rhs->kind == IR_VAL_CONST_FLOAT) imm_f = inst->rhs->const_val.float_val;
                else if (inst->rhs->kind == IR_VAL_CONST_BOOL) imm_b = inst->rhs->const_val.bool_val;
            }

            /* Canonicalize commutative operations: sort registers */
            if (is_commutative(op) && l_reg != -1 && r_reg != -1 && l_reg > r_reg) {
                int tmp = l_reg;
                l_reg = r_reg;
                r_reg = tmp;
            }

            GvnEntry *existing = lookup_gvn(&current_scope, op, tk, l_reg, r_reg, imm_i, imm_f, imm_b);
            if (existing && existing->leader) {
                /* Eliminate redundant computation! */
                replace_uses_in_fn(fn, inst->result, existing->leader);
                remove_inst(bb, inst);

                if (metrics) {
                    metrics->gvn_expressions_eliminated++;
                    metrics->cse_eliminations++;
                }
                *changed = true;
            } else {
                /* Add as leader in current scope */
                add_gvn(&current_scope, op, tk, l_reg, r_reg, imm_i, imm_f, imm_b, inst->result);
            }
        }

        inst = next_inst;
    }

    /* Recurse to dominator tree children */
    for (int c = 0; c < node->child_count; c++) {
        gvn_walk_dom_tree(node->children[c], &current_scope, fn, metrics, changed);
    }

    free_gvn_entries(current_scope.entries);
}

bool ir_ssa_gvn_cse(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error) {
    (void)out_error;
    if (!fn) return false;
    bool changed = false;

    ir_recompute_cfg(fn);
    IrDomInfo *dom = ir_dominance_compute(fn);
    if (!dom || !dom->root) {
        if (dom) ir_dominance_free(dom);
        return false;
    }

    gvn_walk_dom_tree(dom->root, NULL, fn, metrics, &changed);

    ir_dominance_free(dom);
    if (out_changed) *out_changed = changed;
    return true;
}

/* Loop-Invariant Code Motion (LICM — Conservative)               */

static bool is_instruction_invariant(const IrInstruction *inst, const IrLoop *loop, const bool *is_invariant_reg, int max_regs) {
    (void)loop;
    if (!inst || !is_pure_operation(inst->op) || !inst->result) return false;

    /* Divisor must be non-zero to hoist safely without throwing fault */
    if (inst->op == IR_OP_DIV || inst->op == IR_OP_MOD) {
        if (!inst->rhs) return false;
        if (inst->rhs->kind == IR_VAL_CONST_INT && inst->rhs->const_val.int_val == 0) return false;
        if (inst->rhs->kind == IR_VAL_CONST_FLOAT && inst->rhs->const_val.float_val == 0.0) return false;
    }

    /* Check operand 1 (lhs) */
    if (inst->lhs && inst->lhs->kind == IR_VAL_REG) {
        int id = inst->lhs->id;
        if (id < max_regs) {
            if (!is_invariant_reg[id]) return false;
        }
    }

    /* Check operand 2 (rhs) */
    if (inst->rhs && inst->rhs->kind == IR_VAL_REG) {
        int id = inst->rhs->id;
        if (id < max_regs) {
            if (!is_invariant_reg[id]) return false;
        }
    }

    return true;
}

bool ir_ssa_licm(IrFunction *fn, const IrLoopInfo *loop_info, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error) {
    (void)out_error;
    if (!fn || !loop_info || loop_info->loop_count == 0) return true;
    bool changed = false;

    int max_regs = fn->next_reg_id + 64;
    bool *is_invariant = (bool *)calloc(max_regs, sizeof(bool));

    /* Mark all registers defined outside each loop as invariant */
    for (int l = 0; l < loop_info->loop_count; l++) {
        IrLoop *loop = loop_info->loops[l];
        memset(is_invariant, 0, max_regs * sizeof(bool));

        /* Registers defined outside this loop are invariant */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            if (!ir_loop_contains(loop, bb)) {
                for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                    if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_regs) {
                        is_invariant[inst->result->id] = true;
                    }
                }
            }
        }
        for (int p = 0; p < fn->param_count; p++) {
            if (fn->param_values && fn->param_values[p] && fn->param_values[p]->id < max_regs) {
                is_invariant[fn->param_values[p]->id] = true;
            }
        }

        /* Collect candidates iteratively in the loop */
        bool found_new = true;
        while (found_new) {
            found_new = false;
            for (int b = 0; b < loop->block_count; b++) {
                IrBasicBlock *bb = loop->blocks[b];
                for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                    if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_regs) {
                        if (!is_invariant[inst->result->id]) {
                            if (is_instruction_invariant(inst, loop, is_invariant, max_regs)) {
                                is_invariant[inst->result->id] = true;
                                found_new = true;
                            }
                        }
                    }
                }
            }
        }

        /* Find invariant instructions to hoist */
        IrInstruction **to_hoist = (IrInstruction **)malloc(max_regs * sizeof(IrInstruction *));
        IrBasicBlock **hoist_source = (IrBasicBlock **)malloc(max_regs * sizeof(IrBasicBlock *));
        int hoist_count = 0;

        for (int b = 0; b < loop->block_count; b++) {
            IrBasicBlock *bb = loop->blocks[b];
            IrInstruction *inst = bb->first_inst;
            while (inst) {
                IrInstruction *next_inst = inst->next;
                if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_regs) {
                    if (is_invariant[inst->result->id]) {
                        to_hoist[hoist_count] = inst;
                        hoist_source[hoist_count] = bb;
                        hoist_count++;
                    }
                }
                inst = next_inst;
            }
        }

        if (hoist_count > 0) {
            /* Get or create preheader block for this loop */
            IrBasicBlock *preheader = ir_loop_get_or_create_preheader(fn, loop);
            assert(preheader != NULL);

            /* Insert hoisted instructions before preheader's terminator */
            for (int h = 0; h < hoist_count; h++) {
                IrInstruction *h_inst = to_hoist[h];
                IrBasicBlock *src_bb = hoist_source[h];

                /* Remove from src block */
                remove_inst(src_bb, h_inst);

                /* Insert into preheader before terminator */
                IrInstruction *term = preheader->last_inst;
                if (term) {
                    h_inst->prev = term->prev;
                    h_inst->next = term;
                    if (term->prev) term->prev->next = h_inst;
                    else preheader->first_inst = h_inst;
                    term->prev = h_inst;
                } else {
                    h_inst->prev = NULL;
                    h_inst->next = NULL;
                    preheader->first_inst = h_inst;
                    preheader->last_inst = h_inst;
                }
                preheader->inst_count++;

                if (metrics) metrics->licm_instructions_hoisted++;
                changed = true;
            }
        }

        free(to_hoist);
        free(hoist_source);
    }

    free(is_invariant);
    if (out_changed) *out_changed = changed;
    return true;
}

/* Improved SSA Dead Code Elimination                             */

bool ir_ssa_dce(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error) {
    (void)out_error;
    if (!fn) return false;
    bool any_changed = false;

    int max_regs = fn->next_reg_id + 64;
    int *use_counts = (int *)calloc(max_regs, sizeof(int));

    /* Iterative DCE until fixed point */
    bool pass_changed = true;
    while (pass_changed) {
        pass_changed = false;
        memset(use_counts, 0, max_regs * sizeof(int));

        /* 1. Count uses for all registers */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_regs) {
                    use_counts[inst->lhs->id]++;
                }
                if (inst->rhs && inst->rhs->kind == IR_VAL_REG && inst->rhs->id < max_regs) {
                    use_counts[inst->rhs->id]++;
                }
                for (int a = 0; a < inst->arg_count; a++) {
                    if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && inst->args[a]->id < max_regs) {
                        use_counts[inst->args[a]->id]++;
                    }
                }
                for (int p = 0; p < inst->phi_count; p++) {
                    if (inst->phi_values[p] && inst->phi_values[p]->kind == IR_VAL_REG && inst->phi_values[p]->id < max_regs) {
                        use_counts[inst->phi_values[p]->id]++;
                    }
                }
            }
        }

        /* 2. Sweep: remove pure instructions with 0 uses */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            IrInstruction *inst = bb->first_inst;
            while (inst) {
                IrInstruction *next_inst = inst->next;

                if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_regs) {
                    if (use_counts[inst->result->id] == 0) {
                        if (is_pure_operation(inst->op) || inst->op == IR_OP_PHI) {
                            remove_inst(bb, inst);
                            if (metrics) metrics->dce_instructions_removed++;
                            pass_changed = true;
                            any_changed = true;
                        }
                    }
                }

                inst = next_inst;
            }
        }
    }

    free(use_counts);
    if (out_changed) *out_changed = any_changed;
    return true;
}


/* CFG Simplification on SSA Form                                 */

bool ir_ssa_cfg_simplify(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error) {
    (void)out_error;
    if (!fn) return false;
    bool changed = false;

    ir_recompute_cfg(fn);

    /* 1. Simplify condbr with identical targets: condbr %c, L1, L1 -> br L1 */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (bb->last_inst && bb->last_inst->op == IR_OP_CONDBR) {
            if (bb->last_inst->target_true == bb->last_inst->target_false && bb->last_inst->target_true != NULL) {
                bb->last_inst->op = IR_OP_BR;
                bb->last_inst->lhs = NULL;
                bb->last_inst->target_false = NULL;
                changed = true;
            }
        }
    }

    (void)metrics;

    ir_recompute_cfg(fn);
    if (out_changed) *out_changed = changed;
    return true;
}

/* Advanced SSA Optimization Pipeline Driver                      */

static int count_instructions(const IrFunction *fn) {
    int cnt = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        cnt += bb->inst_count;
    }
    return cnt;
}

static int count_blocks(const IrFunction *fn) {
    int cnt = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) cnt++;
    return cnt;
}

static int count_phis(const IrFunction *fn) {
    int cnt = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst && inst->op == IR_OP_PHI; inst = inst->next) {
            cnt++;
        }
    }
    return cnt;
}

bool ir_ssa_advanced_optimize_function(IrFunction *fn, const IrSsaOptions *opts, IrSsaOptMetrics *metrics, char **out_error) {
    if (!fn) return false;

    if (metrics) {
        metrics->ssa_inst_count_before = count_instructions(fn);
        metrics->ssa_block_count_before = count_blocks(fn);
        metrics->phi_count_before = count_phis(fn);
    }

    bool changed = false;

    /* 1. SCCP: Sparse Conditional Constant Propagation */
    if (!ir_ssa_sccp(fn, metrics, &changed, out_error)) return false;
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) return false;
    }

    /* 2. CFG Simplification */
    ir_ssa_cfg_simplify(fn, metrics, &changed, out_error);
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) return false;
    }

    /* 3. GVN / CSE: Global Value Numbering & Common Subexpression Elimination */
    if (!ir_ssa_gvn_cse(fn, metrics, &changed, out_error)) return false;
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) return false;
    }

    /* 4. SSA DCE */
    ir_ssa_dce(fn, metrics, &changed, out_error);
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) return false;
    }

    /* 5. Natural Loop Analysis & LICM */
    IrDomInfo *dom = ir_dominance_compute(fn);
    if (dom) {
        IrLoopInfo *loop_info = ir_loop_analysis_compute(fn, dom);
        if (loop_info) {
            if (metrics) metrics->loops_detected += loop_info->loop_count;
            ir_ssa_licm(fn, loop_info, metrics, &changed, out_error);
            ir_loop_info_free(loop_info);
        }
        ir_dominance_free(dom);
    }
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) return false;
    }

    /* 6. Second SSA DCE cleanup */
    ir_ssa_dce(fn, metrics, &changed, out_error);
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) return false;
    }

    /* 7. Final CFG Simplification */
    ir_ssa_cfg_simplify(fn, metrics, &changed, out_error);
    if (opts && opts->verify_ssa) {
        if (!ir_ssa_verify_function(fn, NULL, out_error)) return false;
    }

    if (metrics) {
        metrics->ssa_inst_count_after = count_instructions(fn);
        metrics->ssa_block_count_after = count_blocks(fn);
        metrics->phi_count_after = count_phis(fn);
    }

    return true;
}

bool ir_ssa_advanced_optimize_module(IrModule *module, const IrSsaOptions *opts, IrSsaOptMetrics *metrics, char **out_error) {
    if (!module) return false;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_ssa_advanced_optimize_function(fn, opts, metrics, out_error)) {
            return false;
        }
    }
    return true;
}

void ir_ssa_opt_metrics_dump(FILE *out, const IrSsaOptMetrics *m) {
    if (!out || !m) return;
    fprintf(out, "====================================================\n");
    fprintf(out, "Cco Phase 6: Advanced SSA Optimization Metrics\n");
    fprintf(out, "====================================================\n");
    fprintf(out, "  SSA Instructions (before/after): %d / %d\n", m->ssa_inst_count_before, m->ssa_inst_count_after);
    fprintf(out, "  SSA Basic Blocks (before/after): %d / %d\n", m->ssa_block_count_before, m->ssa_block_count_after);
    fprintf(out, "  Phi Nodes (before/after):        %d / %d\n", m->phi_count_before, m->phi_count_after);
    fprintf(out, "  SCCP Constants Discovered:       %d\n", m->sccp_constants_found);
    fprintf(out, "  SCCP Branches Simplified:        %d\n", m->sccp_branches_simplified);
    fprintf(out, "  Unreachable Blocks Removed:      %d\n", m->unreachable_blocks_removed);
    fprintf(out, "  GVN Expressions Eliminated:      %d\n", m->gvn_expressions_eliminated);
    fprintf(out, "  CSE Eliminations:                %d\n", m->cse_eliminations);
    fprintf(out, "  Natural Loops Detected:          %d\n", m->loops_detected);
    fprintf(out, "  LICM Instructions Hoisted:       %d\n", m->licm_instructions_hoisted);
    fprintf(out, "  SSA DCE Instructions Removed:    %d\n", m->dce_instructions_removed);
    fprintf(out, "====================================================\n");
}
