// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_OPT_H
#define IR_OPT_H

#include "ir.h"
#include <stdbool.h>


/* Optimization Options                                                      */

typedef struct {
    int opt_level;          /* 0 = no optimization, 1 = safe IR optimization */
    bool dump_passes;       /* Print IR before and after optimization */
    bool verify_each_pass;  /* Run IR verifier after each individual pass */
    bool verbose;
} IrOptOptions;

/* High-level optimization entry points */
bool ir_optimize_module(IrModule *module, const IrOptOptions *opts, char **out_error);
bool ir_optimize_function(IrFunction *fn, const IrOptOptions *opts, char **out_error);

/* Individual Optimization Passes (return true on success, set out_changed) */
bool ir_opt_constant_folding(IrFunction *fn, bool *out_changed);
bool ir_opt_constant_propagation(IrFunction *fn, bool *out_changed);
bool ir_opt_algebraic_simplification(IrFunction *fn, bool *out_changed);
bool ir_opt_copy_propagation(IrFunction *fn, bool *out_changed);
bool ir_opt_dead_code_elimination(IrFunction *fn, bool *out_changed);
bool ir_opt_cfg_simplification(IrFunction *fn, bool *out_changed);

#endif /* IR_OPT_H */
