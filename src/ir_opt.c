// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_opt.h"
#include "ir_verify.h"
#include "ir_print.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper Functions


static bool same_value(const IrValue *a, const IrValue *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->kind == IR_VAL_REG && a->id == b->id) return true;
    if (a->kind == IR_VAL_VAR && a->name && b->name && strcmp(a->name, b->name) == 0) return true;
    if (a->kind == IR_VAL_CONST_INT && a->const_val.int_val == b->const_val.int_val) return true;
    if (a->kind == IR_VAL_CONST_BOOL && a->const_val.bool_val == b->const_val.bool_val) return true;
    if (a->kind == IR_VAL_CONST_FLOAT && a->const_val.float_val == b->const_val.float_val) return true;
    if (a->kind == IR_VAL_CONST_CHAR && a->const_val.char_val == b->const_val.char_val) return true;
    if (a->kind == IR_VAL_CONST_STRING && a->const_val.str_val && b->const_val.str_val && strcmp(a->const_val.str_val, b->const_val.str_val) == 0) return true;
    return false;
}

static bool replace_value_uses_in_inst(IrInstruction *inst, IrValue *old_val, IrValue *new_val) {
    if (!inst || !old_val || !new_val) return false;
    bool replaced = false;

    if (inst->lhs && same_value(inst->lhs, old_val)) {
        inst->lhs = new_val;
        replaced = true;
    }
    if (inst->rhs && same_value(inst->rhs, old_val)) {
        inst->rhs = new_val;
        replaced = true;
    }
    for (int a = 0; a < inst->arg_count; a++) {
        if (inst->args[a] && same_value(inst->args[a], old_val)) {
            inst->args[a] = new_val;
            replaced = true;
        }
    }
    return replaced;
}

static bool replace_value_uses_in_fn(IrFunction *fn, IrValue *old_val, IrValue *new_val) {
    if (!fn || !old_val || !new_val) return false;
    bool replaced = false;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (replace_value_uses_in_inst(inst, old_val, new_val)) {
                replaced = true;
            }
        }
    }
    return replaced;
}

static void remove_instruction_from_block(IrBasicBlock *bb, IrInstruction *inst) {
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


// Pass 1: Constant Folding

bool ir_opt_constant_folding(IrFunction *fn, bool *out_changed) {
    if (!fn) return false;
    bool changed = false;
    IrArena *arena = fn->module->arena;

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (!inst->result) continue;

            /* Integer Binary Arithmetic Folding */
            if (inst->lhs && inst->rhs &&
                inst->lhs->kind == IR_VAL_CONST_INT &&
                inst->rhs->kind == IR_VAL_CONST_INT) {
                int64_t a = inst->lhs->const_val.int_val;
                int64_t b = inst->rhs->const_val.int_val;
                int64_t res = 0;
                bool can_fold = true;

                switch (inst->op) {
                    case IR_OP_ADD: res = a + b; break;
                    case IR_OP_SUB: res = a - b; break;
                    case IR_OP_MUL: res = a * b; break;
                    case IR_OP_DIV:
                        if (b != 0) res = a / b;
                        else can_fold = false; /* Preserve runtime division by zero */
                        break;
                    case IR_OP_MOD:
                        if (b != 0) res = a % b;
                        else can_fold = false;
                        break;
                    default:
                        can_fold = false;
                        break;
                }

                if (can_fold) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_int(arena, inst->type, res);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Unary Negation Folding */
            if (inst->op == IR_OP_NEG && inst->lhs) {
                if (inst->lhs->kind == IR_VAL_CONST_INT) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_int(arena, inst->type, -inst->lhs->const_val.int_val);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                } else if (inst->lhs->kind == IR_VAL_CONST_FLOAT) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_float(arena, -inst->lhs->const_val.float_val);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Floating-Point Binary Arithmetic Folding (Safe finite cases only) */
            if (inst->lhs && inst->rhs &&
                inst->lhs->kind == IR_VAL_CONST_FLOAT &&
                inst->rhs->kind == IR_VAL_CONST_FLOAT) {
                double a = inst->lhs->const_val.float_val;
                double b = inst->rhs->const_val.float_val;
                double res = 0.0;
                bool can_fold = isfinite(a) && isfinite(b);

                if (can_fold) {
                    switch (inst->op) {
                        case IR_OP_ADD: res = a + b; break;
                        case IR_OP_SUB: res = a - b; break;
                        case IR_OP_MUL: res = a * b; break;
                        case IR_OP_DIV:
                            if (b != 0.0 && b != -0.0) res = a / b;
                            else can_fold = false;
                            break;
                        default:
                            can_fold = false;
                            break;
                    }
                }

                if (can_fold && isfinite(res)) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_float(arena, res);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Integer Comparisons Folding */
            if (inst->lhs && inst->rhs &&
                inst->lhs->kind == IR_VAL_CONST_INT &&
                inst->rhs->kind == IR_VAL_CONST_INT) {
                int64_t a = inst->lhs->const_val.int_val;
                int64_t b = inst->rhs->const_val.int_val;
                bool bres = false;
                bool is_cmp = true;

                switch (inst->op) {
                    case IR_OP_EQ: bres = (a == b); break;
                    case IR_OP_NE: bres = (a != b); break;
                    case IR_OP_LT: bres = (a < b); break;
                    case IR_OP_LE: bres = (a <= b); break;
                    case IR_OP_GT: bres = (a > b); break;
                    case IR_OP_GE: bres = (a >= b); break;
                    default: is_cmp = false; break;
                }

                if (is_cmp) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_bool(arena, bres);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Floating-Point Comparisons Folding */
            if (inst->lhs && inst->rhs &&
                inst->lhs->kind == IR_VAL_CONST_FLOAT &&
                inst->rhs->kind == IR_VAL_CONST_FLOAT) {
                double a = inst->lhs->const_val.float_val;
                double b = inst->rhs->const_val.float_val;
                if (isfinite(a) && isfinite(b)) {
                    bool bres = false;
                    bool is_cmp = true;
                    switch (inst->op) {
                        case IR_OP_EQ: bres = (a == b); break;
                        case IR_OP_NE: bres = (a != b); break;
                        case IR_OP_LT: bres = (a < b); break;
                        case IR_OP_LE: bres = (a <= b); break;
                        case IR_OP_GT: bres = (a > b); break;
                        case IR_OP_GE: bres = (a >= b); break;
                        default: is_cmp = false; break;
                    }
                    if (is_cmp) {
                        inst->op = IR_OP_CONST;
                        inst->lhs = ir_val_const_bool(arena, bres);
                        inst->rhs = NULL;
                        changed = true;
                        continue;
                    }
                }
            }

            /* Logical Operations Folding */
            if (inst->lhs && inst->rhs &&
                inst->lhs->kind == IR_VAL_CONST_BOOL &&
                inst->rhs->kind == IR_VAL_CONST_BOOL) {
                bool a = inst->lhs->const_val.bool_val;
                bool b = inst->rhs->const_val.bool_val;
                bool bres = false;
                bool is_log = true;
                switch (inst->op) {
                    case IR_OP_AND: bres = a && b; break;
                    case IR_OP_OR:  bres = a || b; break;
                    default: is_log = false; break;
                }
                if (is_log) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_bool(arena, bres);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            if (inst->op == IR_OP_NOT && inst->lhs && inst->lhs->kind == IR_VAL_CONST_BOOL) {
                inst->op = IR_OP_CONST;
                inst->lhs = ir_val_const_bool(arena, !inst->lhs->const_val.bool_val);
                inst->rhs = NULL;
                changed = true;
                continue;
            }
        }
    }

    if (out_changed) *out_changed = changed;
    return true;
}

// Pass 2: Constant Propagation


bool ir_opt_constant_propagation(IrFunction *fn, bool *out_changed) {
    if (!fn) return false;
    bool changed = false;

    /* Build map of known constant virtual registers */
    int max_id = fn->next_reg_id + 1;
    IrValue **const_map = (IrValue **)calloc(max_id, sizeof(IrValue *));
    int *def_count = (int *)calloc(max_id, sizeof(int));

    /* Count total definitions of each virtual register */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_id) {
                def_count[inst->result->id]++;
            }
        }
    }

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_CONST && inst->result && inst->lhs) {
                if (inst->result->id < max_id && def_count[inst->result->id] == 1) {
                    if (inst->lhs->kind == IR_VAL_CONST_INT ||
                        inst->lhs->kind == IR_VAL_CONST_FLOAT ||
                        inst->lhs->kind == IR_VAL_CONST_BOOL ||
                        inst->lhs->kind == IR_VAL_CONST_CHAR ||
                        inst->lhs->kind == IR_VAL_CONST_STRING) {
                        const_map[inst->result->id] = inst->lhs;
                    }
                }
            }
        }
    }
    free(def_count);

    /* Propagate constants into instruction operands */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_CONST) continue;

            if (inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_id) {
                if (const_map[inst->lhs->id]) {
                    inst->lhs = const_map[inst->lhs->id];
                    changed = true;
                }
            }
            if (inst->rhs && inst->rhs->kind == IR_VAL_REG && inst->rhs->id < max_id) {
                if (const_map[inst->rhs->id]) {
                    inst->rhs = const_map[inst->rhs->id];
                    changed = true;
                }
            }
            for (int a = 0; a < inst->arg_count; a++) {
                if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && inst->args[a]->id < max_id) {
                    if (const_map[inst->args[a]->id]) {
                        inst->args[a] = const_map[inst->args[a]->id];
                        changed = true;
                    }
                }
            }
        }
    }

    free(const_map);
    if (out_changed) *out_changed = changed;
    return true;
}

// Pass 3: Algebraic Simplification

bool ir_opt_algebraic_simplification(IrFunction *fn, bool *out_changed) {
    if (!fn) return false;
    bool changed = false;
    IrArena *arena = fn->module->arena;

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (!inst->result || !inst->lhs) continue;

            /* Integer addition: x + 0 = x, 0 + x = x */
            if (inst->op == IR_OP_ADD && inst->rhs) {
                if (inst->rhs->kind == IR_VAL_CONST_INT && inst->rhs->const_val.int_val == 0) {
                    replace_value_uses_in_fn(fn, inst->result, inst->lhs);
                    inst->op = IR_OP_CONST;
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
                if (inst->lhs->kind == IR_VAL_CONST_INT && inst->lhs->const_val.int_val == 0) {
                    replace_value_uses_in_fn(fn, inst->result, inst->rhs);
                    inst->op = IR_OP_CONST;
                    inst->lhs = inst->rhs;
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Integer subtraction: x - 0 = x, x - x = 0 */
            if (inst->op == IR_OP_SUB && inst->rhs) {
                if (inst->rhs->kind == IR_VAL_CONST_INT && inst->rhs->const_val.int_val == 0) {
                    replace_value_uses_in_fn(fn, inst->result, inst->lhs);
                    inst->op = IR_OP_CONST;
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
                if (same_value(inst->lhs, inst->rhs)) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_int(arena, inst->type, 0);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Integer multiplication: x * 1 = x, 1 * x = x, x * 0 = 0, 0 * x = 0 */
            if (inst->op == IR_OP_MUL && inst->rhs) {
                if (inst->rhs->kind == IR_VAL_CONST_INT && inst->rhs->const_val.int_val == 1) {
                    replace_value_uses_in_fn(fn, inst->result, inst->lhs);
                    inst->op = IR_OP_CONST;
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
                if (inst->lhs->kind == IR_VAL_CONST_INT && inst->lhs->const_val.int_val == 1) {
                    replace_value_uses_in_fn(fn, inst->result, inst->rhs);
                    inst->op = IR_OP_CONST;
                    inst->lhs = inst->rhs;
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
                if ((inst->rhs->kind == IR_VAL_CONST_INT && inst->rhs->const_val.int_val == 0) ||
                    (inst->lhs->kind == IR_VAL_CONST_INT && inst->lhs->const_val.int_val == 0)) {
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_int(arena, inst->type, 0);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Integer division: x / 1 = x */
            if (inst->op == IR_OP_DIV && inst->rhs) {
                if (inst->rhs->kind == IR_VAL_CONST_INT && inst->rhs->const_val.int_val == 1) {
                    replace_value_uses_in_fn(fn, inst->result, inst->lhs);
                    inst->op = IR_OP_CONST;
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }

            /* Comparisons: x == x -> true, x != x -> false */
            if ((inst->op == IR_OP_EQ || inst->op == IR_OP_NE) && inst->rhs) {
                if (same_value(inst->lhs, inst->rhs)) {
                    bool val = (inst->op == IR_OP_EQ);
                    inst->op = IR_OP_CONST;
                    inst->lhs = ir_val_const_bool(arena, val);
                    inst->rhs = NULL;
                    changed = true;
                    continue;
                }
            }
        }
    }

    if (out_changed) *out_changed = changed;
    return true;
}

// Pass 4: Copy & Load-Store Forwarding                                      */

typedef struct {
    char *slot_name;
    IrValue *val;
} SlotMapEntry;

bool ir_opt_copy_propagation(IrFunction *fn, bool *out_changed) {
    if (!fn) return false;
    bool changed = false;

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        SlotMapEntry map[64];
        int map_count = 0;

        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_STORE && inst->rhs && inst->rhs->name) {
                /* store %val -> %slot */
                int found = -1;
                for (int i = 0; i < map_count; i++) {
                    if (strcmp(map[i].slot_name, inst->rhs->name) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found >= 0) {
                    map[found].val = inst->lhs;
                } else if (map_count < 64) {
                    map[map_count].slot_name = inst->rhs->name;
                    map[map_count].val = inst->lhs;
                    map_count++;
                }
            } else if (inst->op == IR_OP_LOAD && inst->lhs && inst->lhs->name && inst->result) {
                /* load %res = *%slot */
                for (int i = 0; i < map_count; i++) {
                    if (strcmp(map[i].slot_name, inst->lhs->name) == 0 && map[i].val != NULL) {
                        if (replace_value_uses_in_fn(fn, inst->result, map[i].val)) {
                            changed = true;
                        }
                        break;
                    }
                }
            } else if (inst->op == IR_OP_CALL) {
                /* Clear map on calls to be safe against side-effects */
                map_count = 0;
            }
        }
    }

    if (out_changed) *out_changed = changed;
    return true;
}

// Pass 5: Dead Code Elimination (DCE) 

static bool is_instruction_side_effecting(IrOpcode op) {
    switch (op) {
        case IR_OP_CALL:
        case IR_OP_PRINT:
        case IR_OP_STORE:
        case IR_OP_ALLOCA:
        case IR_OP_SET_FIELD:
        case IR_OP_SET_INDEX:
        case IR_OP_ALLOC:
        case IR_OP_FREE:
        case IR_OP_RELEASE:
        case IR_OP_BR:
        case IR_OP_CONDBR:
        case IR_OP_RET:
            return true;
        default:
            return false;
    }
}

bool ir_opt_dead_code_elimination(IrFunction *fn, bool *out_changed) {
    if (!fn) return false;
    bool any_changed = false;
    int max_id = fn->next_reg_id + 1;

    bool local_changed = true;
    while (local_changed) {
        local_changed = false;

        /* 1. Compute use counts of virtual registers */
        int *use_counts = (int *)calloc(max_id, sizeof(int));

        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id < max_id) {
                    use_counts[inst->lhs->id]++;
                }
                if (inst->rhs && inst->rhs->kind == IR_VAL_REG && inst->rhs->id < max_id) {
                    use_counts[inst->rhs->id]++;
                }
                for (int a = 0; a < inst->arg_count; a++) {
                    if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && inst->args[a]->id < max_id) {
                        use_counts[inst->args[a]->id]++;
                    }
                }
            }
        }

        /* 2. Eliminate unused, non-side-effecting instructions */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            IrInstruction *inst = bb->first_inst;
            while (inst) {
                IrInstruction *next = inst->next;

                if (inst->result && inst->result->kind == IR_VAL_REG && inst->result->id < max_id) {
                    if (use_counts[inst->result->id] == 0 && !is_instruction_side_effecting(inst->op)) {
                        remove_instruction_from_block(bb, inst);
                        local_changed = true;
                        any_changed = true;
                    }
                }
                inst = next;
            }
        }

        free(use_counts);
    }

    if (out_changed) *out_changed = any_changed;
    return true;
}

// Pass 6: Control-Flow & Branch Simplification

bool ir_opt_cfg_simplification(IrFunction *fn, bool *out_changed) {
    if (!fn) return false;
    bool changed = false;

    /* 1. Branch Simplification on constant conditions */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (!bb->last_inst || bb->last_inst->op != IR_OP_CONDBR) continue;

        IrInstruction *term = bb->last_inst;
        bool have_const = false;
        bool cond_val = false;

        if (term->lhs) {
            if (term->lhs->kind == IR_VAL_CONST_BOOL) {
                have_const = true;
                cond_val = term->lhs->const_val.bool_val;
            } else if (term->lhs->kind == IR_VAL_CONST_INT) {
                have_const = true;
                cond_val = (term->lhs->const_val.int_val != 0);
            }
        }

        if (have_const) {
            IrBasicBlock *target = cond_val ? term->target_true : term->target_false;
            term->op = IR_OP_BR;
            term->target_true = target;
            term->target_false = NULL;
            term->lhs = NULL;
            changed = true;
        } else if (term->target_true == term->target_false) {
            /* Trivial branch with identical targets */
            term->op = IR_OP_BR;
            term->target_false = NULL;
            term->lhs = NULL;
            changed = true;
        }
    }

    /* 2. Rebuild CFG edges */
    ir_recompute_cfg(fn);

    /* 3. Unreachable Block Elimination */
    int max_blocks = (fn->next_block_id > fn->block_count ? fn->next_block_id : fn->block_count) + 16;
    bool *reachable = (bool *)calloc(max_blocks, sizeof(bool));
    IrBasicBlock **queue = (IrBasicBlock **)malloc(max_blocks * sizeof(IrBasicBlock *));
    int q_head = 0, q_tail = 0;

    if (fn->entry_block && fn->entry_block->id < max_blocks) {
        reachable[fn->entry_block->id] = true;
        queue[q_tail++] = fn->entry_block;
    }

    while (q_head < q_tail) {
        IrBasicBlock *curr = queue[q_head++];
        for (int s = 0; s < curr->succ_count; s++) {
            IrBasicBlock *succ = curr->successors[s];
            if (succ && succ->id < max_blocks && !reachable[succ->id]) {
                reachable[succ->id] = true;
                queue[q_tail++] = succ;
            }
        }
    }

    /* Remove unreachable blocks */
    IrBasicBlock *prev = NULL;
    IrBasicBlock *curr = fn->first_block;
    while (curr) {
        IrBasicBlock *next = curr->next;
        if (curr != fn->entry_block && curr->id < max_blocks && !reachable[curr->id]) {
            if (prev) {
                prev->next = next;
            } else {
                fn->first_block = next;
            }
            if (curr == fn->last_block) {
                fn->last_block = prev;
            }
            fn->block_count--;
            changed = true;
        } else {
            prev = curr;
        }
        curr = next;
    }

    free(reachable);
    free(queue);

    /* Rebuild CFG edges after block removals */
    ir_recompute_cfg(fn);

    if (out_changed) *out_changed = changed;
    return true;
}

/* High-Level Optimization Pipeline Driver                                   */

bool ir_optimize_function(IrFunction *fn, const IrOptOptions *opts, char **out_error) {
    if (!fn) return false;
    if (!opts || opts->opt_level <= 0) return true;

    /* Verify initial function IR */
    char *v_err = NULL;
    if (!ir_verify_function(fn, &v_err)) {
        if (out_error) *out_error = v_err;
        else if (v_err) free(v_err);
        return false;
    }

    /* Fixed-point loop over safe optimization passes */
    int max_iterations = 10;
    for (int iter = 0; iter < max_iterations; iter++) {
        bool pass_changed = false;
        bool step_changed = false;

        /* 1. Copy & load-store propagation */
        ir_opt_copy_propagation(fn, &step_changed);
        pass_changed |= step_changed;

        /* 2. Constant propagation */
        ir_opt_constant_propagation(fn, &step_changed);
        pass_changed |= step_changed;

        /* 3. Constant folding */
        ir_opt_constant_folding(fn, &step_changed);
        pass_changed |= step_changed;

        /* 4. Algebraic simplification */
        ir_opt_algebraic_simplification(fn, &step_changed);
        pass_changed |= step_changed;

        /* 5. Dead code elimination */
        ir_opt_dead_code_elimination(fn, &step_changed);
        pass_changed |= step_changed;

        /* 6. CFG & branch simplification */
        ir_opt_cfg_simplification(fn, &step_changed);
        pass_changed |= step_changed;

        /* Verification invariant after each iteration */
        if (opts->verify_each_pass) {
            char *pass_err = NULL;
            if (!ir_verify_function(fn, &pass_err)) {
                if (out_error) *out_error = pass_err;
                else if (pass_err) free(pass_err);
                return false;
            }
        }

        if (!pass_changed) break;
    }

    /* Final verification */
    char *final_err = NULL;
    if (!ir_verify_function(fn, &final_err)) {
        if (out_error) *out_error = final_err;
        else if (final_err) free(final_err);
        return false;
    }

    return true;
}

bool ir_optimize_module(IrModule *module, const IrOptOptions *opts, char **out_error) {
    if (!module) return false;
    if (!opts || opts->opt_level <= 0) return true;

    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_optimize_function(fn, opts, out_error)) {
            return false;
        }
    }
    return true;
}
