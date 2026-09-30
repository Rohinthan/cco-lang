// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_SSA_H
#define IR_SSA_H

#include "ir.h"
#include "ir_dominance.h"
#include <stdio.h>
#include <stdbool.h>

/* SSA Pipeline Options & Statistics                                         */

struct CcoProfile;

typedef struct {
    bool dump_ssa;          /* Print SSA representation after construction */
    bool dump_ssa_opt;      /* Print SSA representation after optimization (Phase 6/7) */
    bool optimize_ssa;      /* Run SSA-level constant prop & DCE */
    bool verify_ssa;        /* Strict SSA verification during pipeline */
    bool verbose;
    int opt_level;          /* Optimization level: 0, 1, 2 */
    bool enable_inlining;   /* Enable controlled function inlining */
    int inline_threshold;   /* Inlining instruction cost threshold (default 35) */
    bool enable_dfe;        /* Enable Dead Function Elimination */
    bool dump_callgraph;    /* Dump call graph and function analysis */
    const struct CcoProfile *profile; /* Phase 8: Profile data for PGO inlining & layout */
} IrSsaOptions;

typedef struct {
    int promoted_allocas;   /* Number of local variable allocas converted to SSA */
    int phi_nodes_created;  /* Number of phi nodes inserted */
    int split_edges;        /* Number of critical edges split during lowering */
    int ssa_inst_count;     /* Total instructions in SSA form */
} IrSsaStats;

/* SSA Core Lifecycle APIs                                                   */

/* SSA Construction (mem2reg): promotes scalar stack allocas to SSA form */
bool ir_ssa_construct_function(IrFunction *fn, IrSsaStats *out_stats, char **out_error);
bool ir_ssa_construct_module(IrModule *module, IrSsaStats *out_stats, char **out_error);

/* SSA Verification: independently verifies all SSA dominance and phi invariants */
bool ir_ssa_verify_function(IrFunction *fn, const IrDomInfo *dom, char **out_error);
bool ir_ssa_verify_module(IrModule *module, char **out_error);

/* SSA Optimizations (SSA-aware Constant Propagation / Folding and SSA DCE) */
bool ir_ssa_optimize_function(IrFunction *fn, bool *out_changed, char **out_error);
bool ir_ssa_optimize_module(IrModule *module, bool *out_changed, char **out_error);

/* SSA Destruction (deconstruction): lowers phi nodes to parallel copies with edge splitting */
bool ir_ssa_deconstruct_function(IrFunction *fn, IrSsaStats *out_stats, char **out_error);
bool ir_ssa_deconstruct_module(IrModule *module, IrSsaStats *out_stats, char **out_error);

/* High-level complete SSA pipeline (construct -> verify -> optimize -> destruct -> verify) */
bool ir_ssa_pipeline_function(IrFunction *fn, const IrSsaOptions *opts, IrSsaStats *out_stats, char **out_error);
bool ir_ssa_pipeline_module(IrModule *module, const IrSsaOptions *opts, IrSsaStats *out_stats, char **out_error);

/* Diagnostics & Dumps                                                       */

void ir_dump_ssa_function(FILE *out, IrFunction *fn);
void ir_dump_ssa_module(FILE *out, IrModule *module);

#endif /* IR_SSA_H */
