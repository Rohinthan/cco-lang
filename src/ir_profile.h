// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_PROFILE_H
#define IR_PROFILE_H

#include "ir.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CCO_PROFILE_MAGIC "CCO_PROFILE_V1"

/* Profile entry records */
typedef struct {
    char *fn_name;
    uint64_t count;
    uint64_t call_count;
} CcoFnProfile;

typedef struct {
    char *fn_name;
    char *bb_name;
    uint64_t count;
} CcoBlockProfile;

typedef struct {
    char *fn_name;
    char *bb_name;
    uint64_t true_count;
    uint64_t false_count;
} CcoBranchProfile;

typedef struct {
    char *fn_name;
    char *header_bb;
    uint64_t entry_count;
    uint64_t iter_count;
    double avg_iters;
} CcoLoopProfile;

typedef struct CcoProfile {
    char *version;

    CcoFnProfile *functions;
    size_t function_count;
    size_t function_capacity;

    CcoBlockProfile *blocks;
    size_t block_count;
    size_t block_capacity;

    CcoBranchProfile *branches;
    size_t branch_count;
    size_t branch_capacity;

    CcoLoopProfile *loops;
    size_t loop_count;
    size_t loop_capacity;

    uint64_t max_fn_count;
    uint64_t max_block_count;
} CcoProfile;

/* Inlining Decision Record for PGO Transparency */
typedef struct IrInlineDecision {
    char *caller;
    char *callee;
    uint64_t call_count;
    int static_cost;
    char *profile_weight; /* "hot", "warm", "cold", "unknown" */
    bool inlined;
    char *reason;
    struct IrInlineDecision *next;
} IrInlineDecision;

/* Profile lifecycle */
CcoProfile *cco_profile_create(void);
void cco_profile_free(CcoProfile *prof);

/* Profile record mutation */
void cco_profile_add_function(CcoProfile *prof, const char *fn_name, uint64_t count);
void cco_profile_add_block(CcoProfile *prof, const char *fn_name, const char *bb_name, uint64_t count);
void cco_profile_add_branch(CcoProfile *prof, const char *fn_name, const char *bb_name, uint64_t true_count, uint64_t false_count);
void cco_profile_add_loop(CcoProfile *prof, const char *fn_name, const char *header_bb, uint64_t entry_count, uint64_t iter_count);

/* Profile queries */
uint64_t cco_profile_get_function_count(const CcoProfile *prof, const char *fn_name);
uint64_t cco_profile_get_block_count(const CcoProfile *prof, const char *fn_name, const char *bb_name);
bool cco_profile_get_branch_counts(const CcoProfile *prof, const char *fn_name, const char *bb_name, uint64_t *out_true, uint64_t *out_false);
bool cco_profile_is_function_hot(const CcoProfile *prof, const char *fn_name);
bool cco_profile_is_block_cold(const CcoProfile *prof, const char *fn_name, const char *bb_name);
bool cco_profile_is_branch_biased(const CcoProfile *prof, const char *fn_name, const char *bb_name, bool *out_take_true);

/* File Serialization & Parsing */
bool cco_profile_save(const CcoProfile *prof, const char *filepath, char **out_err);
CcoProfile *cco_profile_load(const char *filepath, char **out_err);

/* Debug dump */
void cco_profile_dump(FILE *out, const CcoProfile *prof);

/* Inlining Decision Logging */
void cco_profile_record_inline_decision(const char *caller, const char *callee, uint64_t call_count, int static_cost, const char *weight, bool inlined, const char *reason);
void cco_profile_dump_inline_decisions(FILE *out);
void cco_profile_clear_inline_decisions(void);

/* Instrumentation generation for --profile-generate */
char *ir_generate_c_instrumented(IrModule *module, const char *profile_out_path);

#endif /* IR_PROFILE_H */
