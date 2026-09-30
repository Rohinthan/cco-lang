// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_ipa.h"
#include "ir_profile.h"
#include "ir_ssa_opt.h"
#include "ir_verify.h"
#include "ir_dominance.h"
#include "x86_64_regalloc.h"
#include "x86_64_target.h"
#include "x86_64_instr.h"
#include "x86_64_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Function Purity

const char *ir_purity_to_string(IrPurity purity) {
    switch (purity) {
        case IR_PURITY_PURE:           return "PURE";
        case IR_PURITY_READONLY:       return "READONLY";
        case IR_PURITY_SIDE_EFFECTING: return "SIDE_EFFECTING";
        default:                       return "UNKNOWN";
    }
}

// Call Graph Construction & Analysis

static bool is_const_val(IrValue *v) {
    if (!v) return false;
    return (v->kind == IR_VAL_CONST_INT ||
            v->kind == IR_VAL_CONST_FLOAT ||
            v->kind == IR_VAL_CONST_BOOL ||
            v->kind == IR_VAL_CONST_CHAR ||
            v->kind == IR_VAL_CONST_STRING);
}

static int count_const_args(IrInstruction *call_inst) {
    if (!call_inst || call_inst->op != IR_OP_CALL) return 0;
    int count = 0;
    for (int i = 0; i < call_inst->arg_count; i++) {
        if (is_const_val(call_inst->args[i])) {
            count++;
        }
    }
    return count;
}

static int calculate_inline_cost(IrFunction *fn) {
    if (!fn) return INT_MAX;
    int cost = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            switch (inst->op) {
                case IR_OP_CONST:
                    cost += 0;
                    break;
                case IR_OP_ADD: case IR_OP_SUB: case IR_OP_MUL: case IR_OP_DIV:
                case IR_OP_MOD: case IR_OP_NEG: case IR_OP_EQ: case IR_OP_NE:
                case IR_OP_LT: case IR_OP_LE: case IR_OP_GT: case IR_OP_GE:
                case IR_OP_AND: case IR_OP_OR: case IR_OP_NOT:
                    cost += 1;
                    break;
                case IR_OP_PHI:
                    cost += 1;
                    break;
                case IR_OP_BR:
                    cost += 1;
                    break;
                case IR_OP_CONDBR:
                    cost += 2;
                    break;
                case IR_OP_RET:
                    cost += 0;
                    break;
                case IR_OP_CALL:
                    cost += 3;
                    break;
                case IR_OP_LOAD:
                case IR_OP_STORE:
                    cost += 2;
                    break;
                case IR_OP_PRINT:
                case IR_OP_ALLOC:
                case IR_OP_FREE:
                case IR_OP_RELEASE:
                    cost += 2;
                    break;
                default:
                    cost += 1;
                    break;
            }
        }
    }
    return cost;
}

IrCallGraphNode *ir_callgraph_find_node(IrCallGraph *cg, const char *fn_name) {
    if (!cg || !fn_name) return NULL;
    for (int i = 0; i < cg->node_count; i++) {
        if (cg->nodes[i]->fn && strcmp(cg->nodes[i]->fn->name, fn_name) == 0) {
            return cg->nodes[i];
        }
    }
    return NULL;
}

/* Tarjan SCC state */
typedef struct {
    int index;
    IrCallGraphNode **stack;
    int stack_top;
    int scc_count;
} TarjanState;

static void tarjan_scc(IrCallGraphNode *u, TarjanState *ts) {
    u->dfs_index = ts->index;
    u->dfs_lowlink = ts->index;
    ts->index++;

    ts->stack[ts->stack_top++] = u;
    u->on_stack = true;

    for (IrCallSite *cs = u->call_sites; cs; cs = cs->next_in_caller) {
        IrCallGraphNode *v = cs->callee;
        if (!v) continue;

        if (v->dfs_index == -1) {
            tarjan_scc(v, ts);
            if (v->dfs_lowlink < u->dfs_lowlink) {
                u->dfs_lowlink = v->dfs_lowlink;
            }
        } else if (v->on_stack) {
            if (v->dfs_index < u->dfs_lowlink) {
                u->dfs_lowlink = v->dfs_index;
            }
        }
    }

    if (u->dfs_lowlink == u->dfs_index) {
        int scc_id = ts->scc_count++;
        int scc_size = 0;
        IrCallGraphNode *scc_nodes[64];

        while (ts->stack_top > 0) {
            IrCallGraphNode *w = ts->stack[--ts->stack_top];
            w->on_stack = false;
            w->scc_id = scc_id;
            if (scc_size < 64) {
                scc_nodes[scc_size++] = w;
            }
            if (w == u) break;
        }

        /* If SCC has multiple nodes or single self-recursive node, mark recursive cycle */
        if (scc_size > 1) {
            for (int i = 0; i < scc_size; i++) {
                scc_nodes[i]->in_recursive_cycle = true;
                scc_nodes[i]->can_inline = false;
            }
        } else if (scc_size == 1 && u->is_recursive) {
            u->in_recursive_cycle = true;
            u->can_inline = false;
        }
    }
}

IrCallGraph *ir_callgraph_build(IrModule *module) {
    if (!module) return NULL;

    IrCallGraph *cg = (IrCallGraph *)calloc(1, sizeof(IrCallGraph));
    cg->module = module;

    /* 1. Allocate nodes */
    int fn_count = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) fn_count++;

    cg->node_count = fn_count;
    cg->nodes = (IrCallGraphNode **)calloc(fn_count > 0 ? fn_count : 1, sizeof(IrCallGraphNode *));

    int idx = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next, idx++) {
        IrCallGraphNode *node = (IrCallGraphNode *)calloc(1, sizeof(IrCallGraphNode));
        node->fn = fn;
        node->dfs_index = -1;
        node->dfs_lowlink = -1;
        node->scc_id = -1;
        node->purity = IR_PURITY_PURE;

        /* Basic block and instruction count */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            node->block_count++;
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                node->inst_count++;
            }
        }

        if (strcmp(fn->name, "main") == 0) {
            node->is_entry = true;
            node->is_reachable_from_entry = true;
            cg->entry_node = node;
        }

        cg->nodes[idx] = node;
    }

    /* 2. Build call edges */
    for (int i = 0; i < cg->node_count; i++) {
        IrCallGraphNode *caller = cg->nodes[i];
        IrFunction *fn = caller->fn;

        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->op == IR_OP_CALL) {
                    caller->call_count++;
                    IrCallGraphNode *callee = ir_callgraph_find_node(cg, inst->callee_name);

                    IrCallSite *cs = (IrCallSite *)calloc(1, sizeof(IrCallSite));
                    cs->caller = caller;
                    cs->callee = callee;
                    cs->call_inst = inst;
                    cs->caller_block = bb;
                    cs->const_arg_count = count_const_args(inst);

                    /* Link into caller's outgoing list */
                    cs->next_in_caller = caller->call_sites;
                    caller->call_sites = cs;

                    if (callee) {
                        /* Link into callee's incoming list */
                        cs->next_in_callee = callee->incoming_calls;
                        callee->incoming_calls = cs;
                        callee->caller_count++;

                        if (callee == caller) {
                            caller->is_recursive = true;
                        }
                    }
                }
            }
        }
    }

    /* 3. Tarjan SCC for Cycle and Recursion Detection */
    TarjanState ts;
    ts.index = 0;
    ts.stack = (IrCallGraphNode **)calloc(cg->node_count + 1, sizeof(IrCallGraphNode *));
    ts.stack_top = 0;
    ts.scc_count = 0;

    for (int i = 0; i < cg->node_count; i++) {
        if (cg->nodes[i]->dfs_index == -1) {
            tarjan_scc(cg->nodes[i], &ts);
        }
    }
    free(ts.stack);

    /* 4. Reachability Analysis from Entry */
    if (cg->entry_node) {
        IrCallGraphNode **queue = (IrCallGraphNode **)calloc(cg->node_count + 1, sizeof(IrCallGraphNode *));
        int q_head = 0, q_tail = 0;

        queue[q_tail++] = cg->entry_node;
        cg->entry_node->is_reachable_from_entry = true;

        while (q_head < q_tail) {
            IrCallGraphNode *curr = queue[q_head++];
            for (IrCallSite *cs = curr->call_sites; cs; cs = cs->next_in_caller) {
                if (cs->callee && !cs->callee->is_reachable_from_entry) {
                    cs->callee->is_reachable_from_entry = true;
                    queue[q_tail++] = cs->callee;
                }
            }
        }
        free(queue);
    } else {
        /* If no explicit main, mark all nodes reachable to be safe */
        for (int i = 0; i < cg->node_count; i++) {
            cg->nodes[i]->is_reachable_from_entry = true;
        }
    }

    /* 5. Function Purity Analysis */
    for (int i = 0; i < cg->node_count; i++) {
        IrCallGraphNode *node = cg->nodes[i];
        IrFunction *fn = node->fn;
        bool has_load = false;
        bool has_side_effects = false;

        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                switch (inst->op) {
                    case IR_OP_PRINT:
                    case IR_OP_STORE:
                    case IR_OP_ALLOC:
                    case IR_OP_FREE:
                    case IR_OP_RELEASE:
                        has_side_effects = true;
                        break;
                    case IR_OP_LOAD:
                        has_load = true;
                        break;
                    case IR_OP_CALL:
                        if (inst->callee_name) {
                            /* Libc / runtime calls */
                            if (strcmp(inst->callee_name, "printf") == 0 ||
                                strcmp(inst->callee_name, "malloc") == 0 ||
                                strcmp(inst->callee_name, "free") == 0 ||
                                strcmp(inst->callee_name, "exit") == 0) {
                                has_side_effects = true;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        if (has_side_effects) {
            node->purity = IR_PURITY_SIDE_EFFECTING;
        } else if (has_load) {
            node->purity = IR_PURITY_READONLY;
        } else {
            node->purity = IR_PURITY_PURE;
        }
    }

    /* Propagate purity across call edges (fixed-point) */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < cg->node_count; i++) {
            IrCallGraphNode *node = cg->nodes[i];
            if (node->purity == IR_PURITY_SIDE_EFFECTING) continue;

            for (IrCallSite *cs = node->call_sites; cs; cs = cs->next_in_caller) {
                if (!cs->callee) {
                    if (node->purity != IR_PURITY_SIDE_EFFECTING) {
                        node->purity = IR_PURITY_SIDE_EFFECTING;
                        changed = true;
                    }
                    break;
                }
                if (cs->callee->purity == IR_PURITY_SIDE_EFFECTING) {
                    if (node->purity != IR_PURITY_SIDE_EFFECTING) {
                        node->purity = IR_PURITY_SIDE_EFFECTING;
                        changed = true;
                    }
                    break;
                }
                if (cs->callee->purity == IR_PURITY_READONLY && node->purity == IR_PURITY_PURE) {
                    node->purity = IR_PURITY_READONLY;
                    changed = true;
                }
            }
        }
    }

    /* 6. Inlining Cost and Candidate Classification */
    for (int i = 0; i < cg->node_count; i++) {
        IrCallGraphNode *node = cg->nodes[i];
        node->inline_cost = calculate_inline_cost(node->fn);

        if (!node->is_entry &&
            !node->is_recursive &&
            !node->in_recursive_cycle &&
            node->inline_cost <= IR_DEFAULT_INLINE_THRESHOLD) {
            node->can_inline = true;
        } else {
            node->can_inline = false;
        }
    }

    return cg;
}

void ir_callgraph_free(IrCallGraph *cg) {
    if (!cg) return;
    for (int i = 0; i < cg->node_count; i++) {
        IrCallGraphNode *node = cg->nodes[i];
        IrCallSite *cs = node->call_sites;
        while (cs) {
            IrCallSite *next = cs->next_in_caller;
            free(cs);
            cs = next;
        }
        free(node);
    }
    free(cg->nodes);
    free(cg);
}

void ir_callgraph_dump(FILE *out, IrCallGraph *cg) {
    if (!out || !cg) return;
    fprintf(out, "====================================================\n");
    fprintf(out, "CCO CALL GRAPH & FUNCTION ANALYSIS\n");
    fprintf(out, "====================================================\n");

    for (int i = 0; i < cg->node_count; i++) {
        IrCallGraphNode *node = cg->nodes[i];
        fprintf(out, "Function: %s\n", node->fn->name ? node->fn->name : "<anon>");
        fprintf(out, "  Instructions: %d\n", node->inst_count);
        fprintf(out, "  Blocks: %d\n", node->block_count);
        fprintf(out, "  Calls: %d\n", node->call_count);
        fprintf(out, "  Callers: %d\n", node->caller_count);
        fprintf(out, "  Recursive: %s\n", node->is_recursive ? "yes" : "no");
        fprintf(out, "  In recursive cycle: %s\n", node->in_recursive_cycle ? "yes" : "no");
        fprintf(out, "  Purity: %s\n", ir_purity_to_string(node->purity));
        fprintf(out, "  Entry: %s\n", node->is_entry ? "yes" : "no");
        fprintf(out, "  Reachable: %s\n", node->is_reachable_from_entry ? "yes" : "no");
        fprintf(out, "  Inline candidate: %s (cost: %d)\n",
                node->can_inline ? "yes" : "no", node->inline_cost);

        if (node->call_sites) {
            fprintf(out, "  Outgoing calls:\n");
            for (IrCallSite *cs = node->call_sites; cs; cs = cs->next_in_caller) {
                fprintf(out, "    -> @%s (const args: %d/%d)\n",
                        cs->call_inst->callee_name ? cs->call_inst->callee_name : "<unknown>",
                        cs->const_arg_count, cs->call_inst->arg_count);
            }
        }
        fprintf(out, "\n");
    }
    fprintf(out, "====================================================\n");
}

/* Controlled Function Inlining */

typedef struct {
    IrBasicBlock *ret_bb;
    IrValue *ret_val;
} ReturnEdge;

static IrValue *remap_val(IrValue *v, IrValue **val_map, int val_map_size,
                          IrValue **param_map, int param_count, IrArena *arena) {
    (void)arena;
    if (!v) return NULL;
    if (v->kind == IR_VAL_PARAM) {
        if (v->id >= 0 && v->id < param_count && param_map[v->id]) {
            return param_map[v->id];
        }
        return v;
    }
    if (v->kind == IR_VAL_REG) {
        if (v->id >= 0 && v->id < val_map_size && val_map[v->id]) {
            return val_map[v->id];
        }
        return v;
    }
    /* Literals / Constants */
    if (is_const_val(v)) {
        return v;
    }
    return v;
}

/* Inlines a single call site inst in caller_bb */
static bool inline_call_site(IrFunction *caller, IrBasicBlock *caller_bb,
                             IrInstruction *call_inst, IrFunction *callee, char **out_error) {
    if (!caller || !caller_bb || !call_inst || !callee) return false;
    IrArena *arena = caller->module->arena;

    /* 1. Create continuation block (split_bb) for instructions after call_inst */
    char cont_name[64];
    snprintf(cont_name, sizeof(cont_name), "inline.cont.%d", caller->next_block_id++);
    IrBasicBlock *split_bb = ir_block_create(caller, cont_name);

    /* Move all instructions after call_inst to split_bb */
    IrInstruction *post_inst = call_inst->next;
    if (post_inst) {
        post_inst->prev = NULL;
        split_bb->first_inst = post_inst;
        split_bb->last_inst = caller_bb->last_inst;
        caller_bb->last_inst = call_inst->prev;
        if (caller_bb->last_inst) caller_bb->last_inst->next = NULL;
        else caller_bb->first_inst = NULL;
    } else {
        caller_bb->last_inst = call_inst->prev;
        if (caller_bb->last_inst) caller_bb->last_inst->next = NULL;
        else caller_bb->first_inst = NULL;
    }

    /* Update successors of caller_bb: any phi in successors pointing to caller_bb must point to split_bb */
    for (int s = 0; s < caller_bb->succ_count; s++) {
        IrBasicBlock *succ = caller_bb->successors[s];
        for (IrInstruction *phi = succ->first_inst; phi && phi->op == IR_OP_PHI; phi = phi->next) {
            for (int p = 0; p < phi->phi_count; p++) {
                if (phi->phi_blocks[p] == caller_bb) {
                    phi->phi_blocks[p] = split_bb;
                }
            }
        }
    }

    /* 2. Setup Value & Parameter Mappings */
    int val_map_size = callee->next_reg_id + 64;
    IrValue **val_map = (IrValue **)calloc(val_map_size, sizeof(IrValue *));

    int param_count = callee->param_count;
    IrValue **param_map = (IrValue **)calloc(param_count > 0 ? param_count : 1, sizeof(IrValue *));
    for (int p = 0; p < param_count; p++) {
        if (p < call_inst->arg_count) {
            param_map[p] = call_inst->args[p];
        } else {
            param_map[p] = ir_val_const_int(arena, callee->param_types[p], 0);
        }
    }

    /* Map all callee registers to fresh caller registers */
    for (IrBasicBlock *cbb = callee->first_block; cbb; cbb = cbb->next) {
        for (IrInstruction *cinst = cbb->first_inst; cinst; cinst = cinst->next) {
            if (cinst->result && cinst->result->kind == IR_VAL_REG) {
                int rid = cinst->result->id;
                if (rid >= 0 && rid < val_map_size) {
                    val_map[rid] = ir_val_reg(arena, cinst->result->type, caller->next_reg_id++);
                }
            }
        }
    }

    /* 3. Setup Basic Block Mappings */
    int bb_map_size = callee->next_block_id + 32;
    IrBasicBlock **bb_map = (IrBasicBlock **)calloc(bb_map_size, sizeof(IrBasicBlock *));

    for (IrBasicBlock *cbb = callee->first_block; cbb; cbb = cbb->next) {
        char clone_name[128];
        snprintf(clone_name, sizeof(clone_name), "inl.%s.%s.%d",
                 callee->name, cbb->name ? cbb->name : "bb", caller->next_block_id++);
        bb_map[cbb->id] = ir_block_create(caller, clone_name);
    }

    /* 4. Connect caller_bb to callee entry block */
    IrBuilder builder;
    ir_builder_init(&builder, caller->module);
    ir_builder_set_insert_block(&builder, caller_bb);
    ir_emit_br(&builder, bb_map[callee->entry_block->id]);

    /* 5. Clone all blocks and instructions */
    ReturnEdge ret_edges[64];
    int ret_edge_count = 0;

    for (IrBasicBlock *cbb = callee->first_block; cbb; cbb = cbb->next) {
        IrBasicBlock *dst_bb = bb_map[cbb->id];
        ir_builder_set_insert_block(&builder, dst_bb);

        for (IrInstruction *cinst = cbb->first_inst; cinst; cinst = cinst->next) {
            if (cinst->op == IR_OP_RET) {
                if (ret_edge_count < 64) {
                    ret_edges[ret_edge_count].ret_bb = dst_bb;
                    ret_edges[ret_edge_count].ret_val = cinst->lhs ? remap_val(cinst->lhs, val_map, val_map_size, param_map, param_count, arena) : NULL;
                    ret_edge_count++;
                }
                ir_emit_br(&builder, split_bb);
            } else if (cinst->op == IR_OP_ALLOCA) {
                /* Place allocas at the end of caller entry block */
                IrBasicBlock *save_bb = builder.block;
                builder.block = caller->entry_block;
                IrInstruction *cloned_alloca = ir_emit_alloca(&builder, cinst->type, cinst->field_name ? cinst->field_name : "inl.var");
                builder.block = save_bb;
                if (cinst->result && cinst->result->id < val_map_size) {
                    val_map[cinst->result->id] = cloned_alloca->result;
                }
            } else if (cinst->op == IR_OP_PHI) {
                IrInstruction *cloned_phi = ir_emit_phi(&builder, cinst->type);
                if (cinst->result && cinst->result->id < val_map_size) {
                    cloned_phi->result = val_map[cinst->result->id];
                }
                for (int p = 0; p < cinst->phi_count; p++) {
                    IrBasicBlock *orig_pred = cinst->phi_blocks[p];
                    IrValue *orig_val = cinst->phi_values[p];
                    IrBasicBlock *mapped_pred = orig_pred ? bb_map[orig_pred->id] : NULL;
                    IrValue *mapped_val = remap_val(orig_val, val_map, val_map_size, param_map, param_count, arena);
                    if (mapped_pred && mapped_val) {
                        ir_phi_add_incoming(cloned_phi, mapped_pred, mapped_val, arena);
                    }
                }
            } else if (cinst->op == IR_OP_BR) {
                IrBasicBlock *target = cinst->target_true ? bb_map[cinst->target_true->id] : split_bb;
                ir_emit_br(&builder, target);
            } else if (cinst->op == IR_OP_CONDBR) {
                IrValue *cond = remap_val(cinst->lhs, val_map, val_map_size, param_map, param_count, arena);
                IrBasicBlock *t_true = cinst->target_true ? bb_map[cinst->target_true->id] : split_bb;
                IrBasicBlock *t_false = cinst->target_false ? bb_map[cinst->target_false->id] : split_bb;
                ir_emit_condbr(&builder, cond, t_true, t_false);
            } else {
                /* General operations: ADD, SUB, CALL, PRINT, LOAD, STORE, etc. */
                IrInstruction *cloned = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
                memcpy(cloned, cinst, sizeof(IrInstruction));
                cloned->prev = NULL;
                cloned->next = NULL;

                if (cinst->result && cinst->result->id < val_map_size) {
                    cloned->result = val_map[cinst->result->id];
                }
                cloned->lhs = remap_val(cinst->lhs, val_map, val_map_size, param_map, param_count, arena);
                cloned->rhs = remap_val(cinst->rhs, val_map, val_map_size, param_map, param_count, arena);

                if (cinst->arg_count > 0 && cinst->args) {
                    cloned->args = (IrValue **)ir_arena_alloc(arena, cinst->arg_count * sizeof(IrValue *));
                    for (int a = 0; a < cinst->arg_count; a++) {
                        cloned->args[a] = remap_val(cinst->args[a], val_map, val_map_size, param_map, param_count, arena);
                    }
                }
                ir_block_add_instruction(dst_bb, cloned);
            }
        }
    }

    /* 6. Wire return value in split_bb */
    if (call_inst->result) {
        if (ret_edge_count == 1) {
            /* Single return edge: substitute call_inst->result directly */
            IrValue *final_val = ret_edges[0].ret_val;
            if (!final_val) final_val = ir_val_const_int(arena, call_inst->result->type, 0);

            /* Replace all uses of call_inst->result in caller with final_val */
            for (IrBasicBlock *bb = caller->first_block; bb; bb = bb->next) {
                for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                    if (inst->lhs && inst->lhs->kind == IR_VAL_REG && inst->lhs->id == call_inst->result->id) {
                        inst->lhs = final_val;
                    }
                    if (inst->rhs && inst->rhs->kind == IR_VAL_REG && inst->rhs->id == call_inst->result->id) {
                        inst->rhs = final_val;
                    }
                    for (int a = 0; a < inst->arg_count; a++) {
                        if (inst->args[a] && inst->args[a]->kind == IR_VAL_REG && inst->args[a]->id == call_inst->result->id) {
                            inst->args[a] = final_val;
                        }
                    }
                    for (int p = 0; p < inst->phi_count; p++) {
                        if (inst->phi_values[p] && inst->phi_values[p]->kind == IR_VAL_REG && inst->phi_values[p]->id == call_inst->result->id) {
                            inst->phi_values[p] = final_val;
                        }
                    }
                }
            }
        } else if (ret_edge_count > 1) {
            /* Multiple return edges: insert phi node at the top of split_bb */
            IrInstruction *phi = (IrInstruction *)ir_arena_alloc(arena, sizeof(IrInstruction));
            memset(phi, 0, sizeof(IrInstruction));
            phi->op = IR_OP_PHI;
            phi->type = call_inst->result->type;
            phi->result = call_inst->result;

            for (int r = 0; r < ret_edge_count; r++) {
                IrValue *v = ret_edges[r].ret_val;
                if (!v) v = ir_val_const_int(arena, call_inst->result->type, 0);
                ir_phi_add_incoming(phi, ret_edges[r].ret_bb, v, arena);
            }

            /* Insert phi at the very top of split_bb */
            phi->next = split_bb->first_inst;
            if (split_bb->first_inst) {
                split_bb->first_inst->prev = phi;
            } else {
                split_bb->last_inst = phi;
            }
            split_bb->first_inst = phi;
            split_bb->inst_count++;
        }
    }

    /* 7. Recompute CFG and verify */
    ir_recompute_cfg(caller);

    char *v_err = NULL;
    if (!ir_ssa_verify_function(caller, NULL, &v_err)) {
        if (out_error) *out_error = v_err;
        else if (v_err) free(v_err);
        free(val_map);
        free(param_map);
        free(bb_map);
        return false;
    }

    free(val_map);
    free(param_map);
    free(bb_map);
    return true;
}

bool ir_ipa_inline_function_pgo(IrFunction *fn, IrCallGraph *cg, int threshold, const struct CcoProfile *prof, int *out_inlined, char **out_error) {
    if (!fn || !cg) return false;
    int inlined_total = 0;
    bool changed = true;

    while (changed) {
        changed = false;
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->op == IR_OP_CALL && inst->callee_name) {
                    IrCallGraphNode *callee_node = ir_callgraph_find_node(cg, inst->callee_name);
                    if (!callee_node || !callee_node->fn) continue;

                    /* Profile counts and classification */
                    uint64_t call_count = 0;
                    if (prof) {
                        uint64_t bb_count = cco_profile_get_block_count(prof, fn->name, bb->name);
                        uint64_t fn_count = cco_profile_get_function_count(prof, callee_node->fn->name);
                        call_count = bb_count > 0 ? bb_count : fn_count;
                    }

                    const char *weight = "unknown";
                    int effective_threshold = threshold;
                    if (prof) {
                        if (call_count >= 1000 || cco_profile_is_function_hot(prof, callee_node->fn->name)) {
                            weight = "hot";
                            effective_threshold = threshold + 30; /* Boost threshold for hot calls */
                            if (effective_threshold > IR_MAX_INLINE_HARD_CEILING) {
                                effective_threshold = IR_MAX_INLINE_HARD_CEILING;
                            }
                        } else if (call_count == 0 || cco_profile_is_block_cold(prof, fn->name, bb->name)) {
                            weight = "cold";
                            effective_threshold = 15; /* Lower threshold for cold call sites */
                        } else {
                            weight = "warm";
                            effective_threshold = threshold;
                        }
                    }

                    if (callee_node->fn == fn) {
                        cco_profile_record_inline_decision(fn->name, callee_node->fn->name, call_count, callee_node->inline_cost, weight, false, "self-recursion");
                        continue;
                    }
                    if (callee_node->is_recursive || callee_node->in_recursive_cycle) {
                        cco_profile_record_inline_decision(fn->name, callee_node->fn->name, call_count, callee_node->inline_cost, weight, false, "recursive / cyclic");
                        continue;
                    }
                    if (callee_node->is_entry) {
                        cco_profile_record_inline_decision(fn->name, callee_node->fn->name, call_count, callee_node->inline_cost, weight, false, "entry function");
                        continue;
                    }
                    if (callee_node->inline_cost > IR_MAX_INLINE_HARD_CEILING) {
                        cco_profile_record_inline_decision(fn->name, callee_node->fn->name, call_count, callee_node->inline_cost, weight, false, "size threshold exceeded");
                        continue;
                    }
                    if (callee_node->inline_cost > effective_threshold) {
                        cco_profile_record_inline_decision(fn->name, callee_node->fn->name, call_count, callee_node->inline_cost, weight, false, "cost exceeds threshold");
                        continue;
                    }

                    /* Inline this call site */
                    cco_profile_record_inline_decision(fn->name, callee_node->fn->name, call_count, callee_node->inline_cost, weight, true,
                        (strcmp(weight, "hot") == 0) ? "hot call within boosted threshold" : "within threshold");

                    if (inline_call_site(fn, bb, inst, callee_node->fn, out_error)) {
                        inlined_total++;
                        changed = true;
                        break;
                    }
                }
            }
            if (changed) break;
        }
    }

    if (out_inlined) *out_inlined = inlined_total;
    return true;
}

bool ir_ipa_inline_function(IrFunction *fn, IrCallGraph *cg, int threshold, int *out_inlined, char **out_error) {
    return ir_ipa_inline_function_pgo(fn, cg, threshold, NULL, out_inlined, out_error);
}

bool ir_ipa_inline_module_pgo(IrModule *module, int threshold, const struct CcoProfile *prof, int *out_inlined, char **out_error) {
    if (!module) return false;
    IrCallGraph *cg = ir_callgraph_build(module);
    if (!cg) return false;

    int total_inlined = 0;

    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        int fn_inlined = 0;
        if (!ir_ipa_inline_function_pgo(fn, cg, threshold, prof, &fn_inlined, out_error)) {
            ir_callgraph_free(cg);
            return false;
        }
        total_inlined += fn_inlined;
    }

    ir_callgraph_free(cg);
    if (out_inlined) *out_inlined = total_inlined;
    return true;
}

bool ir_ipa_inline_module(IrModule *module, int threshold, int *out_inlined, char **out_error) {
    return ir_ipa_inline_module_pgo(module, threshold, NULL, out_inlined, out_error);
}

// Dead Function Elimination (DFE)

bool ir_ipa_eliminate_dead_functions(IrModule *module, IrCallGraph *cg, int *out_removed, char **out_error) {
    (void)out_error;
    if (!module) return false;

    bool own_cg = false;
    if (!cg) {
        cg = ir_callgraph_build(module);
        own_cg = true;
    }

    int removed = 0;
    IrFunction *prev = NULL;
    IrFunction *curr = module->first_fn;

    while (curr) {
        IrCallGraphNode *node = ir_callgraph_find_node(cg, curr->name);
        bool is_dead = (node && !node->is_entry && !node->is_reachable_from_entry);

        if (is_dead) {
            IrFunction *next = curr->next;
            if (prev) {
                prev->next = next;
            } else {
                module->first_fn = next;
            }
            if (curr == module->last_fn) {
                module->last_fn = prev;
            }
            module->fn_count--;
            removed++;
            curr = next;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    if (own_cg) {
        ir_callgraph_free(cg);
    }
    if (out_removed) *out_removed = removed;
    return true;
}

// Phase 7 Full Interprocedural Pipeline Driver

bool ir_ipa_optimize_module(IrModule *module, const IrIpaOptions *opts, IrIpaMetrics *metrics, char **out_error) {
    if (!module) return false;

    int threshold = (opts && opts->inline_threshold > 0) ? opts->inline_threshold : IR_DEFAULT_INLINE_THRESHOLD;
    bool enable_inline = opts ? opts->enable_inlining : true;
    bool enable_dfe = opts ? opts->enable_dfe : true;
    const struct CcoProfile *prof = opts ? opts->profile : NULL;

    /* 1. Build initial call graph */
    IrCallGraph *cg = ir_callgraph_build(module);
    if (!cg) return false;

    if (opts && opts->verbose) {
        ir_callgraph_dump(stdout, cg);
    }

    /* 2. Controlled Inlining */
    int total_inlined = 0;
    if (enable_inline) {
        for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
            int fn_inlined = 0;
            if (!ir_ipa_inline_function_pgo(fn, cg, threshold, prof, &fn_inlined, out_error)) {
                ir_callgraph_free(cg);
                return false;
            }
            if (fn_inlined > 0) {
                total_inlined += fn_inlined;
                /* Run post-inline SSA cleanup on modified function */
                IrSsaOptions ssa_opt = {
                    .dump_ssa = false,
                    .dump_ssa_opt = false,
                    .optimize_ssa = true,
                    .verify_ssa = true,
                    .verbose = false
                };
                ir_ssa_advanced_optimize_function(fn, &ssa_opt, NULL, out_error);
            }
        }
    }
    if (metrics) metrics->inlined_call_sites = total_inlined;

    ir_callgraph_free(cg);

    /* 3. Dead Function Elimination (after inlining has opened unreferenced functions) */
    int total_removed = 0;
    if (enable_dfe) {
        IrCallGraph *post_cg = ir_callgraph_build(module);
        if (post_cg) {
            ir_ipa_eliminate_dead_functions(module, post_cg, &total_removed, out_error);
            ir_callgraph_free(post_cg);
        }
    }
    if (metrics) metrics->dead_functions_removed = total_removed;

    return true;
}

// Native Code Quality Metrics Collection & Reporting

void cco_code_stats_collect_pgo(IrModule *module, const struct CcoProfile *prof, CcoCodeStats *stats) {
    if (!module || !stats) return;
    memset(stats, 0, sizeof(CcoCodeStats));

    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        stats->function_count++;
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            stats->basic_block_count++;
            if (prof) {
                if (cco_profile_is_block_cold(prof, fn->name, bb->name)) {
                    stats->cold_block_count++;
                } else {
                    stats->hot_block_count++;
                }
            }

            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                stats->post_ssa_inst_count++;
                if (inst->op == IR_OP_CONDBR) {
                    stats->branch_count++;
                    stats->cond_branch_count++;
                } else if (inst->op == IR_OP_BR) {
                    stats->branch_count++;
                    stats->uncond_branch_count++;
                } else if (inst->op == IR_OP_CALL) {
                    stats->call_count++;
                } else if (inst->op == IR_OP_LOAD) {
                    stats->mem_load_count++;
                } else if (inst->op == IR_OP_STORE) {
                    stats->mem_store_count++;
                } else if (inst->op == IR_OP_RET) {
                    stats->ret_count++;
                }

                if (inst->op == IR_OP_ADD || inst->op == IR_OP_SUB || inst->op == IR_OP_MUL ||
                    inst->op == IR_OP_DIV || inst->op == IR_OP_MOD || inst->op == IR_OP_NEG) {
                    if (inst->type && inst->type->kind == IR_TYPE_F64) {
                        stats->float_ops_count++;
                    } else {
                        stats->integer_ops_count++;
                    }
                }
            }
        }

        /* Run register allocator in analysis mode */
        RegAllocResult *ra = regalloc_run(fn);
        if (ra) {
            stats->virtual_regs_count += ra->total_regs;
            stats->live_intervals_count += ra->total_regs;
            stats->spill_count += ra->spilled_count;
            stats->callee_saved_regs_used += ra->callee_saved_count;

            int int_used_fn = 0;
            int xmm_used_fn = 0;
            for (int r = 0; r < X86_TOTAL_REG_COUNT; r++) {
                if (ra->used_regs[r]) {
                    if (x86_reg_class(r) == REG_CLASS_FLOAT) {
                        xmm_used_fn++;
                        stats->xmm_regs_used++;
                    } else {
                        int_used_fn++;
                        stats->int_regs_used++;
                    }
                }
            }
            if (int_used_fn > stats->peak_int_live) stats->peak_int_live = int_used_fn;
            if (xmm_used_fn > stats->peak_float_live) stats->peak_float_live = xmm_used_fn;

            X86FunctionFrame *frame = x86_frame_build(fn, ra);
            if (frame) {
                stats->total_stack_frame_size += frame->total_stack_size;
                if (frame->total_stack_size > stats->max_stack_frame_size) {
                    stats->max_stack_frame_size = frame->total_stack_size;
                }
                x86_frame_free(frame);
            }
            regalloc_free(ra);
        }

        /* Count exact x86 machine instructions */
        X86InstrList *xlist = NULL;
        char *xerr = NULL;
        if (gen_function_instructions(fn, &xlist, &xerr)) {
            x86_instr_list_optimize(xlist);
            stats->x86_inst_count += (int)xlist->count;
            x86_instr_list_free(xlist);
        } else {
            if (xerr) free(xerr);
        }
    }

    if (stats->function_count > 0) {
        int avail = stats->function_count * 14; /* 14 allocatable GPRs */
        stats->register_utilization_pct = (double)stats->int_regs_used * 100.0 / (double)avail;
    }
}

void cco_code_stats_collect(IrModule *module, CcoCodeStats *stats) {
    cco_code_stats_collect_pgo(module, NULL, stats);
}

void cco_code_stats_dump(FILE *out, const CcoCodeStats *stats) {
    if (!out || !stats) return;
    fprintf(out, "====================================================\n");
    fprintf(out, "CCO NATIVE CODE QUALITY & COMPILER METRICS\n");
    fprintf(out, "====================================================\n");
    fprintf(out, "Functions:\n");
    fprintf(out, "  Total Functions:             %d\n", stats->function_count);
    fprintf(out, "  Inlined Call Sites:          %d\n", stats->inlined_functions_count);
    fprintf(out, "  Dead Functions Pruned:       %d\n", stats->removed_functions_count);
    fprintf(out, "\nInstruction & CFG Counts:\n");
    if (stats->ir_inst_count > 0)
        fprintf(out, "  Initial IR Instructions:     %d\n", stats->ir_inst_count);
    if (stats->ssa_inst_count > 0)
        fprintf(out, "  SSA Instructions:            %d\n", stats->ssa_inst_count);
    fprintf(out, "  Post-SSA IR Instructions:    %d\n", stats->post_ssa_inst_count);
    fprintf(out, "  Basic Blocks:                %d\n", stats->basic_block_count);
    if (stats->hot_block_count > 0 || stats->cold_block_count > 0) {
        fprintf(out, "  Hot Blocks:                  %d\n", stats->hot_block_count);
        fprintf(out, "  Cold Blocks:                 %d\n", stats->cold_block_count);
    }
    if (stats->x86_inst_count > 0)
        fprintf(out, "  x86 Machine Instructions:    %d\n", stats->x86_inst_count);
    fprintf(out, "  Integer Operations:          %d\n", stats->integer_ops_count);
    fprintf(out, "  Floating-Point Operations:   %d\n", stats->float_ops_count);
    fprintf(out, "  Branches:                    %d\n", stats->branch_count);
    fprintf(out, "  Conditional Branches:        %d\n", stats->cond_branch_count);
    fprintf(out, "  Unconditional Branches:      %d\n", stats->uncond_branch_count);
    fprintf(out, "  Memory Loads:                %d\n", stats->mem_load_count);
    fprintf(out, "  Memory Stores:               %d\n", stats->mem_store_count);
    fprintf(out, "  Calls:                       %d\n", stats->call_count);
    fprintf(out, "  Returns:                     %d\n", stats->ret_count);
    fprintf(out, "\nRegister Allocation & Memory:\n");
    fprintf(out, "  Virtual Registers:           %d\n", stats->virtual_regs_count);
    fprintf(out, "  Live Intervals:              %d\n", stats->live_intervals_count);
    fprintf(out, "  Integer Registers Used:      %d\n", stats->int_regs_used);
    fprintf(out, "  XMM Registers Used:          %d\n", stats->xmm_regs_used);
    fprintf(out, "  Callee-Saved Preserved:      %d\n", stats->callee_saved_regs_used);
    fprintf(out, "  Spilled Registers:           %d\n", stats->spill_count);
    fprintf(out, "  Reload Count:                %d\n", stats->reload_count);
    fprintf(out, "  Peak Integer Live:           %d\n", stats->peak_int_live);
    fprintf(out, "  Peak Float Live:             %d\n", stats->peak_float_live);
    fprintf(out, "  Register Utilization:        %.1f%%\n", stats->register_utilization_pct);
    fprintf(out, "  Total Stack Frame Bytes:     %d\n", stats->total_stack_frame_size);
    fprintf(out, "  Max Stack Frame Bytes:       %d\n", stats->max_stack_frame_size);
    if (stats->x86_text_bytes > 0 || stats->x86_rodata_bytes > 0 || stats->elf_file_size > 0) {
        fprintf(out, "\nBinary Sizes:\n");
        if (stats->x86_text_bytes > 0)
            fprintf(out, "  .text Section:               %zu bytes\n", stats->x86_text_bytes);
        if (stats->x86_rodata_bytes > 0)
            fprintf(out, "  .rodata Section:             %zu bytes\n", stats->x86_rodata_bytes);
        if (stats->elf_file_size > 0)
            fprintf(out, "  Final ELF Size:              %zu bytes\n", stats->elf_file_size);
    }
    fprintf(out, "====================================================\n");
}
