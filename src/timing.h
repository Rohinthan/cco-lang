// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CCO_TIMING_H
#define CCO_TIMING_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

typedef struct CcoPassTimings {
    double frontend_ms;       /* Lexer, parser, traits, scopes */
    double ir_lower_ms;       /* AST -> Cco IR lowering & initial verification */
    double ssa_ms;            /* SSA construction, intra-SSA opt, SSA destruction */
    double opt_ms;            /* Non-SSA IR optimization (ir_optimize_module) */
    double ipa_ms;            /* Call graph, inlining, dead function elimination */
    double pgo_ms;            /* Profile loading and PGO analysis */
    double regalloc_ms;       /* Linear-scan register allocation */
    double codegen_ms;        /* Machine instruction selection & peephole */
    double elf_gen_ms;        /* Direct ELF64 object writing */
    double link_ms;           /* Internal or external linking */
    double total_ms;          /* Total compilation time */
} CcoPassTimings;

static inline double cco_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static inline void cco_pass_timings_dump(FILE *out, const CcoPassTimings *t) {
    if (!out || !t) return;
    fprintf(out, "\n==================================================\n");
    fprintf(out, " Cco Compilation Timing\n");
    fprintf(out, "--------------------------------------------------\n");
    fprintf(out, "Frontend:          %7.2f ms\n", t->frontend_ms);
    fprintf(out, "IR lowering:       %7.2f ms\n", t->ir_lower_ms);
    fprintf(out, "SSA:               %7.2f ms\n", t->ssa_ms);
    fprintf(out, "Optimization:      %7.2f ms\n", t->opt_ms);
    fprintf(out, "IPA:               %7.2f ms\n", t->ipa_ms);
    if (t->pgo_ms > 0.0) {
        fprintf(out, "PGO processing:    %7.2f ms\n", t->pgo_ms);
    }
    fprintf(out, "RegAlloc:          %7.2f ms\n", t->regalloc_ms);
    fprintf(out, "Codegen:           %7.2f ms\n", t->codegen_ms);
    fprintf(out, "ELF generation:    %7.2f ms\n", t->elf_gen_ms);
    fprintf(out, "Linking:           %7.2f ms\n", t->link_ms);
    fprintf(out, "--------------------------------------------------\n");
    fprintf(out, "Total:             %7.2f ms\n", t->total_ms);
    fprintf(out, "==================================================\n");
}

static inline void cco_pass_timings_dump_csv(FILE *out, const CcoPassTimings *t) {
    if (!out || !t) return;
    fprintf(out, "pass,time_ms\n");
    fprintf(out, "Frontend,%.4f\n", t->frontend_ms);
    fprintf(out, "IR lowering,%.4f\n", t->ir_lower_ms);
    fprintf(out, "SSA,%.4f\n", t->ssa_ms);
    fprintf(out, "Optimization,%.4f\n", t->opt_ms);
    fprintf(out, "IPA,%.4f\n", t->ipa_ms);
    fprintf(out, "PGO processing,%.4f\n", t->pgo_ms);
    fprintf(out, "RegAlloc,%.4f\n", t->regalloc_ms);
    fprintf(out, "Codegen,%.4f\n", t->codegen_ms);
    fprintf(out, "ELF generation,%.4f\n", t->elf_gen_ms);
    fprintf(out, "Linking,%.4f\n", t->link_ms);
    fprintf(out, "Total,%.4f\n", t->total_ms);
}

#endif /* CCO_TIMING_H */
