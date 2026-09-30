// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_IPA_H
#define IR_IPA_H

#include "ir.h"
#include "ir_ssa.h"
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

/* Function Purity and Side-Effect Classification */

typedef enum {
    IR_PURITY_UNKNOWN = 0,
    IR_PURITY_PURE,           /* Pure computation from arguments; no memory access, no side effects */
    IR_PURITY_READONLY,       /* Only reads memory/arguments; no writes/prints/alloc/free */
    IR_PURITY_SIDE_EFFECTING  /* Performs writes, prints, alloc/free, or calls side-effecting functions */
} IrPurity;

const char *ir_purity_to_string(IrPurity purity);

/* Call Graph Structures  */

struct IrCallGraphNode;

typedef struct IrCallSite {
    struct IrCallGraphNode *caller;
    struct IrCallGraphNode *callee;
    IrInstruction *call_inst;     /* The IR_OP_CALL instruction */
    IrBasicBlock *caller_block;   /* Basic block containing the call */
    int const_arg_count;          /* Number of arguments that are compile-time constants */
    struct IrCallSite *next_in_caller;
    struct IrCallSite *next_in_callee;
} IrCallSite;

typedef struct IrCallGraphNode {
    IrFunction *fn;
    int inst_count;
    int block_count;
    int call_count;               /* Number of outgoing call sites in this function */
    int caller_count;             /* Number of incoming call sites targeting this function */
    bool is_recursive;            /* Directly calls itself */
    bool in_recursive_cycle;      /* Part of a cycle (mutual recursion) */
    bool is_reachable_from_entry; /* Reachable from main / export */
    bool is_entry;                /* main or externally visible */
    IrPurity purity;
    bool can_inline;
    int inline_cost;

    IrCallSite *call_sites;       /* Outgoing call sites */
    IrCallSite *incoming_calls;   /* Incoming call sites */

    /* Tarjan SCC analysis state */
    int dfs_index;
    int dfs_lowlink;
    bool on_stack;
    int scc_id;
} IrCallGraphNode;

typedef struct IrCallGraph {
    IrModule *module;
    IrCallGraphNode **nodes;
    int node_count;
    IrCallGraphNode *entry_node;  /* main */
} IrCallGraph;

IrCallGraph *ir_callgraph_build(IrModule *module);
void ir_callgraph_free(IrCallGraph *cg);
IrCallGraphNode *ir_callgraph_find_node(IrCallGraph *cg, const char *fn_name);
void ir_callgraph_dump(FILE *out, IrCallGraph *cg);

/* Controlled Function Inlining Options & API */

struct CcoProfile;

#define IR_DEFAULT_INLINE_THRESHOLD 35
#define IR_MAX_INLINE_HARD_CEILING 80

typedef struct {
    int inline_threshold;              /* Max cost to inline (default 35) */
    bool enable_inlining;              /* Enable inlining at -O2 */
    bool enable_dfe;                   /* Enable Dead Function Elimination */
    bool verbose;
    const struct CcoProfile *profile;  /* Optional profile for PGO inlining & layout */
} IrIpaOptions;

typedef struct {
    int inlined_call_sites;       /* Total call sites inlined */
    int functions_inlined;        /* Distinct functions inlined at least once */
    int dead_functions_removed;   /* Unreachable functions pruned */
    int constants_propagated;     /* Interprocedural constant arguments folded */
} IrIpaMetrics;

/* Inlines eligible call sites in fn or across module while in SSA form */
bool ir_ipa_inline_function(IrFunction *fn, IrCallGraph *cg, int threshold, int *out_inlined, char **out_error);
bool ir_ipa_inline_function_pgo(IrFunction *fn, IrCallGraph *cg, int threshold, const struct CcoProfile *prof, int *out_inlined, char **out_error);
bool ir_ipa_inline_module(IrModule *module, int threshold, int *out_inlined, char **out_error);
bool ir_ipa_inline_module_pgo(IrModule *module, int threshold, const struct CcoProfile *prof, int *out_inlined, char **out_error);

/* Dead Function Elimination: removes non-entry unreachable functions */
bool ir_ipa_eliminate_dead_functions(IrModule *module, IrCallGraph *cg, int *out_removed, char **out_error);

/* Full Interprocedural Pipeline */
bool ir_ipa_optimize_module(IrModule *module, const IrIpaOptions *opts, IrIpaMetrics *metrics, char **out_error);

// Native Code Quality & Extended Diagnostics

typedef struct {
    int ir_inst_count;
    int ssa_inst_count;
    int post_ssa_inst_count;
    int x86_inst_count;
    size_t x86_text_bytes;
    size_t x86_rodata_bytes;
    size_t elf_file_size;

    int spill_count;
    int reload_count;
    int total_stack_frame_size;
    int max_stack_frame_size;

    int function_count;
    int inlined_functions_count;
    int removed_functions_count;
    int branch_count;
    int call_count;

    int virtual_regs_count;
    int live_intervals_count;
    int int_regs_used;
    int xmm_regs_used;
    int callee_saved_regs_used;

    /* Phase 8 Extended Instruction & CFG Diagnostics */
    int basic_block_count;
    int cond_branch_count;
    int uncond_branch_count;
    int mem_load_count;
    int mem_store_count;
    int ret_count;

    int hot_block_count;
    int cold_block_count;
    int integer_ops_count;
    int float_ops_count;
    int peak_int_live;
    int peak_float_live;
    double register_utilization_pct;
} CcoCodeStats;

void cco_code_stats_collect(IrModule *module, CcoCodeStats *stats);
void cco_code_stats_collect_pgo(IrModule *module, const struct CcoProfile *prof, CcoCodeStats *stats);
void cco_code_stats_dump(FILE *out, const CcoCodeStats *stats);

#endif /* IR_IPA_H */
