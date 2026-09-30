// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_SSA_OPT_H
#define IR_SSA_OPT_H

#include "ir.h"
#include "ir_ssa.h"
#include "ir_dominance.h"
#include "ir_loop.h"
#include <stdio.h>
#include <stdbool.h>

/* Optimization Metrics                                              */

typedef struct {
    int ssa_inst_count_before;
    int ssa_inst_count_after;
    int ssa_block_count_before;
    int ssa_block_count_after;
    int phi_count_before;
    int phi_count_after;

    int sccp_constants_found;
    int sccp_branches_simplified;
    int unreachable_blocks_removed;

    int gvn_expressions_eliminated;
    int cse_eliminations;

    int dce_instructions_removed;

    int loops_detected;
    int licm_instructions_hoisted;
} IrSsaOptMetrics;

/* Individual Passes                                                 */

/* Sparse Conditional Constant Propagation (SCCP) */
bool ir_ssa_sccp(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error);

/* Global Value Numbering & Common Subexpression Elimination */
bool ir_ssa_gvn_cse(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error);

/* Loop-Invariant Code Motion (Conservative) */
bool ir_ssa_licm(IrFunction *fn, const IrLoopInfo *loop_info, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error);

/* SSA Dead Code Elimination */
bool ir_ssa_dce(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error);

/* CFG Simplification (pruning unreachable blocks, constant branches) */
bool ir_ssa_cfg_simplify(IrFunction *fn, IrSsaOptMetrics *metrics, bool *out_changed, char **out_error);

/* Full Advanced SSA Optimization Pipeline                                   */

bool ir_ssa_advanced_optimize_function(IrFunction *fn, const IrSsaOptions *opts, IrSsaOptMetrics *metrics, char **out_error);
bool ir_ssa_advanced_optimize_module(IrModule *module, const IrSsaOptions *opts, IrSsaOptMetrics *metrics, char **out_error);

void ir_ssa_opt_metrics_dump(FILE *out, const IrSsaOptMetrics *m);

#endif /* IR_SSA_OPT_H */
