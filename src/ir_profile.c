// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>


// Profile Lifecycle and Constructors


CcoProfile *cco_profile_create(void) {
    CcoProfile *prof = (CcoProfile *)calloc(1, sizeof(CcoProfile));
    prof->version = strdup(CCO_PROFILE_MAGIC);
    prof->function_capacity = 16;
    prof->functions = (CcoFnProfile *)calloc(prof->function_capacity, sizeof(CcoFnProfile));
    prof->block_capacity = 64;
    prof->blocks = (CcoBlockProfile *)calloc(prof->block_capacity, sizeof(CcoBlockProfile));
    prof->branch_capacity = 32;
    prof->branches = (CcoBranchProfile *)calloc(prof->branch_capacity, sizeof(CcoBranchProfile));
    prof->loop_capacity = 16;
    prof->loops = (CcoLoopProfile *)calloc(prof->loop_capacity, sizeof(CcoLoopProfile));
    return prof;
}

void cco_profile_free(CcoProfile *prof) {
    if (!prof) return;
    if (prof->version) free(prof->version);

    for (size_t i = 0; i < prof->function_count; i++) {
        if (prof->functions[i].fn_name) free(prof->functions[i].fn_name);
    }
    free(prof->functions);

    for (size_t i = 0; i < prof->block_count; i++) {
        if (prof->blocks[i].fn_name) free(prof->blocks[i].fn_name);
        if (prof->blocks[i].bb_name) free(prof->blocks[i].bb_name);
    }
    free(prof->blocks);

    for (size_t i = 0; i < prof->branch_count; i++) {
        if (prof->branches[i].fn_name) free(prof->branches[i].fn_name);
        if (prof->branches[i].bb_name) free(prof->branches[i].bb_name);
    }
    free(prof->branches);

    for (size_t i = 0; i < prof->loop_count; i++) {
        if (prof->loops[i].fn_name) free(prof->loops[i].fn_name);
        if (prof->loops[i].header_bb) free(prof->loops[i].header_bb);
    }
    free(prof->loops);

    free(prof);
}

// Mutation Helpers

void cco_profile_add_function(CcoProfile *prof, const char *fn_name, uint64_t count) {
    if (!prof || !fn_name) return;
    for (size_t i = 0; i < prof->function_count; i++) {
        if (strcmp(prof->functions[i].fn_name, fn_name) == 0) {
            prof->functions[i].count += count;
            if (prof->functions[i].count > prof->max_fn_count) {
                prof->max_fn_count = prof->functions[i].count;
            }
            return;
        }
    }

    if (prof->function_count >= prof->function_capacity) {
        prof->function_capacity *= 2;
        prof->functions = (CcoFnProfile *)realloc(prof->functions, prof->function_capacity * sizeof(CcoFnProfile));
    }

    prof->functions[prof->function_count].fn_name = strdup(fn_name);
    prof->functions[prof->function_count].count = count;
    prof->functions[prof->function_count].call_count = 0;
    prof->function_count++;

    if (count > prof->max_fn_count) {
        prof->max_fn_count = count;
    }
}

void cco_profile_add_block(CcoProfile *prof, const char *fn_name, const char *bb_name, uint64_t count) {
    if (!prof || !fn_name || !bb_name) return;
    for (size_t i = 0; i < prof->block_count; i++) {
        if (strcmp(prof->blocks[i].fn_name, fn_name) == 0 &&
            strcmp(prof->blocks[i].bb_name, bb_name) == 0) {
            prof->blocks[i].count += count;
            if (prof->blocks[i].count > prof->max_block_count) {
                prof->max_block_count = prof->blocks[i].count;
            }
            return;
        }
    }

    if (prof->block_count >= prof->block_capacity) {
        prof->block_capacity *= 2;
        prof->blocks = (CcoBlockProfile *)realloc(prof->blocks, prof->block_capacity * sizeof(CcoBlockProfile));
    }

    prof->blocks[prof->block_count].fn_name = strdup(fn_name);
    prof->blocks[prof->block_count].bb_name = strdup(bb_name);
    prof->blocks[prof->block_count].count = count;
    prof->block_count++;

    if (count > prof->max_block_count) {
        prof->max_block_count = count;
    }
}

void cco_profile_add_branch(CcoProfile *prof, const char *fn_name, const char *bb_name, uint64_t true_count, uint64_t false_count) {
    if (!prof || !fn_name || !bb_name) return;
    for (size_t i = 0; i < prof->branch_count; i++) {
        if (strcmp(prof->branches[i].fn_name, fn_name) == 0 &&
            strcmp(prof->branches[i].bb_name, bb_name) == 0) {
            prof->branches[i].true_count += true_count;
            prof->branches[i].false_count += false_count;
            return;
        }
    }

    if (prof->branch_count >= prof->branch_capacity) {
        prof->branch_capacity *= 2;
        prof->branches = (CcoBranchProfile *)realloc(prof->branches, prof->branch_capacity * sizeof(CcoBranchProfile));
    }

    prof->branches[prof->branch_count].fn_name = strdup(fn_name);
    prof->branches[prof->branch_count].bb_name = strdup(bb_name);
    prof->branches[prof->branch_count].true_count = true_count;
    prof->branches[prof->branch_count].false_count = false_count;
    prof->branch_count++;
}

void cco_profile_add_loop(CcoProfile *prof, const char *fn_name, const char *header_bb, uint64_t entry_count, uint64_t iter_count) {
    if (!prof || !fn_name || !header_bb) return;
    for (size_t i = 0; i < prof->loop_count; i++) {
        if (strcmp(prof->loops[i].fn_name, fn_name) == 0 &&
            strcmp(prof->loops[i].header_bb, header_bb) == 0) {
            prof->loops[i].entry_count += entry_count;
            prof->loops[i].iter_count += iter_count;
            prof->loops[i].avg_iters = prof->loops[i].entry_count > 0 ? (double)prof->loops[i].iter_count / (double)prof->loops[i].entry_count : 0.0;
            return;
        }
    }

    if (prof->loop_count >= prof->loop_capacity) {
        prof->loop_capacity *= 2;
        prof->loops = (CcoLoopProfile *)realloc(prof->loops, prof->loop_capacity * sizeof(CcoLoopProfile));
    }

    prof->loops[prof->loop_count].fn_name = strdup(fn_name);
    prof->loops[prof->loop_count].header_bb = strdup(header_bb);
    prof->loops[prof->loop_count].entry_count = entry_count;
    prof->loops[prof->loop_count].iter_count = iter_count;
    prof->loops[prof->loop_count].avg_iters = entry_count > 0 ? (double)iter_count / (double)entry_count : 0.0;
    prof->loop_count++;
}


// Profile Queries


uint64_t cco_profile_get_function_count(const CcoProfile *prof, const char *fn_name) {
    if (!prof || !fn_name) return 0;
    for (size_t i = 0; i < prof->function_count; i++) {
        if (strcmp(prof->functions[i].fn_name, fn_name) == 0) {
            return prof->functions[i].count;
        }
    }
    return 0;
}

uint64_t cco_profile_get_block_count(const CcoProfile *prof, const char *fn_name, const char *bb_name) {
    if (!prof || !fn_name || !bb_name) return 0;
    for (size_t i = 0; i < prof->block_count; i++) {
        if (strcmp(prof->blocks[i].fn_name, fn_name) == 0 &&
            strcmp(prof->blocks[i].bb_name, bb_name) == 0) {
            return prof->blocks[i].count;
        }
    }
    return 0;
}

bool cco_profile_get_branch_counts(const CcoProfile *prof, const char *fn_name, const char *bb_name, uint64_t *out_true, uint64_t *out_false) {
    if (!prof || !fn_name || !bb_name) return false;
    for (size_t i = 0; i < prof->branch_count; i++) {
        if (strcmp(prof->branches[i].fn_name, fn_name) == 0 &&
            strcmp(prof->branches[i].bb_name, bb_name) == 0) {
            if (out_true) *out_true = prof->branches[i].true_count;
            if (out_false) *out_false = prof->branches[i].false_count;
            return true;
        }
    }
    return false;
}

bool cco_profile_is_function_hot(const CcoProfile *prof, const char *fn_name) {
    if (!prof || !fn_name) return false;
    uint64_t count = cco_profile_get_function_count(prof, fn_name);
    if (count >= 1000) return true;
    if (prof->max_fn_count > 0 && count * 10 >= prof->max_fn_count) return true;
    return false;
}

bool cco_profile_is_block_cold(const CcoProfile *prof, const char *fn_name, const char *bb_name) {
    if (!prof || !fn_name || !bb_name) return false;
    uint64_t count = cco_profile_get_block_count(prof, fn_name, bb_name);
    if (count == 0) return true;
    if (prof->max_block_count >= 1000 && count * 1000 < prof->max_block_count) return true;
    return false;
}

bool cco_profile_is_branch_biased(const CcoProfile *prof, const char *fn_name, const char *bb_name, bool *out_take_true) {
    uint64_t t = 0, f = 0;
    if (!cco_profile_get_branch_counts(prof, fn_name, bb_name, &t, &f)) return false;
    uint64_t total = t + f;
    if (total < 10) return false;

    if (t >= 4 * f) {
        if (out_take_true) *out_take_true = true;
        return true;
    }
    if (f >= 4 * t) {
        if (out_take_true) *out_take_true = false;
        return true;
    }
    return false;
}


// Deterministic Serialization & Sorting

static int cmp_fn(const void *a, const void *b) {
    const CcoFnProfile *fa = (const CcoFnProfile *)a;
    const CcoFnProfile *fb = (const CcoFnProfile *)b;
    return strcmp(fa->fn_name, fb->fn_name);
}

static int cmp_block(const void *a, const void *b) {
    const CcoBlockProfile *ba = (const CcoBlockProfile *)a;
    const CcoBlockProfile *bb = (const CcoBlockProfile *)b;
    int c = strcmp(ba->fn_name, bb->fn_name);
    if (c != 0) return c;
    return strcmp(ba->bb_name, bb->bb_name);
}

static int cmp_branch(const void *a, const void *b) {
    const CcoBranchProfile *ba = (const CcoBranchProfile *)a;
    const CcoBranchProfile *bb = (const CcoBranchProfile *)b;
    int c = strcmp(ba->fn_name, bb->fn_name);
    if (c != 0) return c;
    return strcmp(ba->bb_name, bb->bb_name);
}

static int cmp_loop(const void *a, const void *b) {
    const CcoLoopProfile *la = (const CcoLoopProfile *)a;
    const CcoLoopProfile *lb = (const CcoLoopProfile *)b;
    int c = strcmp(la->fn_name, lb->fn_name);
    if (c != 0) return c;
    return strcmp(la->header_bb, lb->header_bb);
}

bool cco_profile_save(const CcoProfile *prof, const char *filepath, char **out_err) {
    if (!prof || !filepath) {
        if (out_err) *out_err = strdup("invalid profile or filepath");
        return false;
    }

    FILE *f = fopen(filepath, "w");
    if (!f) {
        if (out_err) {
            char buf[256];
            snprintf(buf, sizeof(buf), "could not open profile output file '%s': %s", filepath, strerror(errno));
            *out_err = strdup(buf);
        }
        return false;
    }

    /* Sort entries to guarantee deterministic output */
    if (prof->function_count > 1) {
        qsort(prof->functions, prof->function_count, sizeof(CcoFnProfile), cmp_fn);
    }
    if (prof->block_count > 1) {
        qsort(prof->blocks, prof->block_count, sizeof(CcoBlockProfile), cmp_block);
    }
    if (prof->branch_count > 1) {
        qsort(prof->branches, prof->branch_count, sizeof(CcoBranchProfile), cmp_branch);
    }
    if (prof->loop_count > 1) {
        qsort(prof->loops, prof->loop_count, sizeof(CcoLoopProfile), cmp_loop);
    }

    fprintf(f, "%s\n\n", CCO_PROFILE_MAGIC);

    /* 1. Functions */
    for (size_t i = 0; i < prof->function_count; i++) {
        fprintf(f, "function %s %lu\n", prof->functions[i].fn_name, (unsigned long)prof->functions[i].count);
    }
    if (prof->function_count > 0) fprintf(f, "\n");

    /* 2. Blocks */
    for (size_t i = 0; i < prof->block_count; i++) {
        fprintf(f, "block %s.%s %lu\n", prof->blocks[i].fn_name, prof->blocks[i].bb_name, (unsigned long)prof->blocks[i].count);
    }
    if (prof->block_count > 0) fprintf(f, "\n");

    /* 3. Branches */
    for (size_t i = 0; i < prof->branch_count; i++) {
        fprintf(f, "branch %s.%s true %lu\n", prof->branches[i].fn_name, prof->branches[i].bb_name, (unsigned long)prof->branches[i].true_count);
        fprintf(f, "branch %s.%s false %lu\n", prof->branches[i].fn_name, prof->branches[i].bb_name, (unsigned long)prof->branches[i].false_count);
    }
    if (prof->branch_count > 0) fprintf(f, "\n");

    /* 4. Loops */
    for (size_t i = 0; i < prof->loop_count; i++) {
        fprintf(f, "loop %s.%s %lu %lu\n", prof->loops[i].fn_name, prof->loops[i].header_bb,
                (unsigned long)prof->loops[i].entry_count, (unsigned long)prof->loops[i].iter_count);
    }

    fclose(f);
    return true;
}

// Profile Parsing and Robust Validation 

static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

CcoProfile *cco_profile_load(const char *filepath, char **out_err) {
    if (!filepath) {
        if (out_err) *out_err = strdup("null filepath");
        return NULL;
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        if (out_err) {
            char buf[256];
            snprintf(buf, sizeof(buf), "could not open profile file '%s': %s", filepath, strerror(errno));
            *out_err = strdup(buf);
        }
        return NULL;
    }

    CcoProfile *prof = cco_profile_create();
    char line[1024];
    int line_num = 0;
    bool header_found = false;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        char *p = trim_whitespace(line);
        if (*p == '\0' || *p == '#') continue;

        if (!header_found) {
            if (strncmp(p, CCO_PROFILE_MAGIC, strlen(CCO_PROFILE_MAGIC)) != 0) {
                if (out_err) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "invalid profile magic: expected '%s' at line %d, got '%s'", CCO_PROFILE_MAGIC, line_num, p);
                    *out_err = strdup(buf);
                }
                cco_profile_free(prof);
                fclose(f);
                return NULL;
            }
            header_found = true;
            continue;
        }

        /* Tokenize line */
        char tag[64] = {0};
        char arg1[256] = {0};
        char arg2[256] = {0};
        char arg3[256] = {0};
        int tokens = sscanf(p, "%63s %255s %255s %255s", tag, arg1, arg2, arg3);

        if (strcmp(tag, "function") == 0) {
            if (tokens < 3) {
                if (out_err) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "malformed function record at line %d: '%s'", line_num, p);
                    *out_err = strdup(buf);
                }
                cco_profile_free(prof);
                fclose(f);
                return NULL;
            }
            char *endptr = NULL;
            errno = 0;
            uint64_t count = strtoull(arg2, &endptr, 10);
            if (errno != 0 || !endptr || *endptr != '\0') {
                if (out_err) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "invalid integer count '%s' at line %d", arg2, line_num);
                    *out_err = strdup(buf);
                }
                cco_profile_free(prof);
                fclose(f);
                return NULL;
            }
            cco_profile_add_function(prof, arg1, count);
        } else if (strcmp(tag, "block") == 0) {
            if (tokens < 3) {
                if (out_err) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "malformed block record at line %d: '%s'", line_num, p);
                    *out_err = strdup(buf);
                }
                cco_profile_free(prof);
                fclose(f);
                return NULL;
            }
            char fn_name[256] = {0};
            char bb_name[256] = {0};
            uint64_t count = 0;

            if (tokens == 3) {
                /* Format: block fn.bb count */
                char *dot = strchr(arg1, '.');
                if (dot) {
                    size_t fn_len = dot - arg1;
                    if (fn_len >= sizeof(fn_name)) fn_len = sizeof(fn_name) - 1;
                    strncpy(fn_name, arg1, fn_len);
                    fn_name[fn_len] = '\0';
                    snprintf(bb_name, sizeof(bb_name), "%s", dot + 1);
                } else {
                    snprintf(fn_name, sizeof(fn_name), "%s", arg1);
                    snprintf(bb_name, sizeof(bb_name), "entry");
                }
                char *endptr = NULL;
                count = strtoull(arg2, &endptr, 10);
                if (!endptr || *endptr != '\0') {
                    if (out_err) {
                        char buf[512];
                        snprintf(buf, sizeof(buf), "invalid block count '%s' at line %d", arg2, line_num);
                        *out_err = strdup(buf);
                    }
                    cco_profile_free(prof);
                    fclose(f);
                    return NULL;
                }
            } else if (tokens >= 4) {
                /* Format: block fn bb count */
                snprintf(fn_name, sizeof(fn_name), "%s", arg1);
                snprintf(bb_name, sizeof(bb_name), "%s", arg2);
                char *endptr = NULL;
                count = strtoull(arg3, &endptr, 10);
                if (!endptr || *endptr != '\0') {
                    if (out_err) {
                        char buf[512];
                        snprintf(buf, sizeof(buf), "invalid block count '%s' at line %d", arg3, line_num);
                        *out_err = strdup(buf);
                    }
                    cco_profile_free(prof);
                    fclose(f);
                    return NULL;
                }
            }
            cco_profile_add_block(prof, fn_name, bb_name, count);
        } else if (strcmp(tag, "branch") == 0) {
            /* branch fn.bb <true|false> count */
            if (tokens < 4) {
                if (out_err) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "malformed branch record at line %d: '%s'", line_num, p);
                    *out_err = strdup(buf);
                }
                cco_profile_free(prof);
                fclose(f);
                return NULL;
            }
            char fn_name[256] = {0};
            char bb_name[256] = {0};
            char *dot = strchr(arg1, '.');
            if (dot) {
                size_t fn_len = dot - arg1;
                if (fn_len >= sizeof(fn_name)) fn_len = sizeof(fn_name) - 1;
                strncpy(fn_name, arg1, fn_len);
                fn_name[fn_len] = '\0';
                snprintf(bb_name, sizeof(bb_name), "%s", dot + 1);
            } else {
                snprintf(fn_name, sizeof(fn_name), "%s", arg1);
                snprintf(bb_name, sizeof(bb_name), "entry");
            }

            bool is_true = (strcmp(arg2, "true") == 0 || strcmp(arg2, "1") == 0);
            char *endptr = NULL;
            uint64_t count = strtoull(arg3, &endptr, 10);
            if (!endptr || *endptr != '\0') {
                if (out_err) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "invalid branch count '%s' at line %d", arg3, line_num);
                    *out_err = strdup(buf);
                }
                cco_profile_free(prof);
                fclose(f);
                return NULL;
            }

            if (is_true) {
                cco_profile_add_branch(prof, fn_name, bb_name, count, 0);
            } else {
                cco_profile_add_branch(prof, fn_name, bb_name, 0, count);
            }
        } else if (strcmp(tag, "loop") == 0) {
            if (tokens >= 4) {
                char fn_name[256] = {0};
                char header_bb[256] = {0};
                char *dot = strchr(arg1, '.');
                if (dot) {
                    size_t fn_len = dot - arg1;
                    if (fn_len >= sizeof(fn_name)) fn_len = sizeof(fn_name) - 1;
                    strncpy(fn_name, arg1, fn_len);
                    fn_name[fn_len] = '\0';
                    snprintf(header_bb, sizeof(header_bb), "%s", dot + 1);
                } else {
                    snprintf(fn_name, sizeof(fn_name), "%s", arg1);
                    snprintf(header_bb, sizeof(header_bb), "loop");
                }
                uint64_t entries = strtoull(arg2, NULL, 10);
                uint64_t iters = strtoull(arg3, NULL, 10);
                cco_profile_add_loop(prof, fn_name, header_bb, entries, iters);
            }
        } else {
            /* Unrecognized directive */
            if (out_err) {
                char buf[512];
                snprintf(buf, sizeof(buf), "unknown profile directive '%s' at line %d", tag, line_num);
                *out_err = strdup(buf);
            }
            cco_profile_free(prof);
            fclose(f);
            return NULL;
        }
    }

    fclose(f);

    if (!header_found) {
        if (out_err) *out_err = strdup("empty profile file or missing header");
        cco_profile_free(prof);
        return NULL;
    }

    return prof;
}

// Debug Dump 

void cco_profile_dump(FILE *out, const CcoProfile *prof) {
    if (!out) out = stdout;
    if (!prof) {
        fprintf(out, "Profile: <none>\n");
        return;
    }

    fprintf(out, "====================================================\n");
    fprintf(out, "CCO PROFILE EXECUTION SUMMARY (%s)\n", prof->version ? prof->version : "V1");
    fprintf(out, "====================================================\n");

    fprintf(out, "Functions (%zu total, max count: %lu):\n", prof->function_count, (unsigned long)prof->max_fn_count);
    for (size_t i = 0; i < prof->function_count; i++) {
        bool hot = cco_profile_is_function_hot(prof, prof->functions[i].fn_name);
        fprintf(out, "  %-24s: %12lu calls %s\n", prof->functions[i].fn_name,
                (unsigned long)prof->functions[i].count, hot ? "[HOT]" : "");
    }

    fprintf(out, "\nBasic Blocks (%zu total, max count: %lu):\n", prof->block_count, (unsigned long)prof->max_block_count);
    for (size_t i = 0; i < prof->block_count; i++) {
        bool cold = cco_profile_is_block_cold(prof, prof->blocks[i].fn_name, prof->blocks[i].bb_name);
        fprintf(out, "  %s.%-20s: %12lu %s\n", prof->blocks[i].fn_name, prof->blocks[i].bb_name,
                (unsigned long)prof->blocks[i].count, cold ? "[COLD]" : "");
    }

    fprintf(out, "\nBranches (%zu total):\n", prof->branch_count);
    for (size_t i = 0; i < prof->branch_count; i++) {
        uint64_t total = prof->branches[i].true_count + prof->branches[i].false_count;
        double pct_true = total > 0 ? (double)prof->branches[i].true_count * 100.0 / (double)total : 0.0;
        fprintf(out, "  %s.%-20s: true=%lu (%.1f%%), false=%lu (%.1f%%)\n",
                prof->branches[i].fn_name, prof->branches[i].bb_name,
                (unsigned long)prof->branches[i].true_count, pct_true,
                (unsigned long)prof->branches[i].false_count, 100.0 - pct_true);
    }

    if (prof->loop_count > 0) {
        fprintf(out, "\nLoops (%zu total):\n", prof->loop_count);
        for (size_t i = 0; i < prof->loop_count; i++) {
            fprintf(out, "  %s.%-20s: entries=%lu, iters=%lu, avg=%.1f\n",
                    prof->loops[i].fn_name, prof->loops[i].header_bb,
                    (unsigned long)prof->loops[i].entry_count,
                    (unsigned long)prof->loops[i].iter_count,
                    prof->loops[i].avg_iters);
        }
    }

    fprintf(out, "====================================================\n");
}

// Inlining Decision Records 

static IrInlineDecision *g_inline_decisions = NULL;

void cco_profile_record_inline_decision(const char *caller, const char *callee, uint64_t call_count, int static_cost, const char *weight, bool inlined, const char *reason) {
    IrInlineDecision *rec = (IrInlineDecision *)calloc(1, sizeof(IrInlineDecision));
    rec->caller = strdup(caller ? caller : "unknown");
    rec->callee = strdup(callee ? callee : "unknown");
    rec->call_count = call_count;
    rec->static_cost = static_cost;
    rec->profile_weight = strdup(weight ? weight : "unknown");
    rec->inlined = inlined;
    rec->reason = strdup(reason ? reason : "");
    rec->next = g_inline_decisions;
    g_inline_decisions = rec;
}

void cco_profile_dump_inline_decisions(FILE *out) {
    if (!out) out = stdout;
    fprintf(out, "====================================================\n");
    fprintf(out, "PROFILE-GUIDED INLINING DECISION LOG\n");
    fprintf(out, "====================================================\n");
    if (!g_inline_decisions) {
        fprintf(out, "No inlining decisions recorded.\n");
        fprintf(out, "====================================================\n");
        return;
    }

    /* Print in chronological order (reverse of singly-linked list) */
    int count = 0;
    for (IrInlineDecision *cur = g_inline_decisions; cur; cur = cur->next) count++;
    IrInlineDecision **arr = (IrInlineDecision **)malloc(count * sizeof(IrInlineDecision *));
    int idx = count - 1;
    for (IrInlineDecision *cur = g_inline_decisions; cur; cur = cur->next, idx--) {
        arr[idx] = cur;
    }

    for (int i = 0; i < count; i++) {
        IrInlineDecision *d = arr[i];
        fprintf(out, "Call: %s -> %s\n", d->caller, d->callee);
        fprintf(out, "  Calls:          %lu\n", (unsigned long)d->call_count);
        fprintf(out, "  Static Cost:    %d\n", d->static_cost);
        fprintf(out, "  Profile Weight: %s\n", d->profile_weight);
        fprintf(out, "  Decision:       %s\n", d->inlined ? "inline" : "no-inline");
        fprintf(out, "  Reason:         %s\n", d->reason);
        fprintf(out, "\n");
    }
    free(arr);
    fprintf(out, "====================================================\n");
}

void cco_profile_clear_inline_decisions(void) {
    IrInlineDecision *cur = g_inline_decisions;
    while (cur) {
        IrInlineDecision *next = cur->next;
        if (cur->caller) free(cur->caller);
        if (cur->callee) free(cur->callee);
        if (cur->profile_weight) free(cur->profile_weight);
        if (cur->reason) free(cur->reason);
        free(cur);
        cur = next;
    }
    g_inline_decisions = NULL;
}

// Profile Instrumentation Code Generation

#include "ir_codegen_c.h"

/* Structure describing an instrumented location */
typedef enum {
    PROF_LOC_FN,
    PROF_LOC_BB,
    PROF_LOC_BR_TRUE,
    PROF_LOC_BR_FALSE
} ProfLocKind;

typedef struct {
    ProfLocKind kind;
    char fn_name[128];
    char bb_name[128];
    int counter_id;
} ProfLoc;

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} Buffer;

static Buffer create_buf(void) {
    Buffer b;
    b.capacity = 4096;
    b.length = 0;
    b.buffer = (char *)malloc(b.capacity);
    if (b.buffer) b.buffer[0] = '\0';
    return b;
}

static void buf_append(Buffer *b, const char *str) {
    if (!str) return;
    size_t len = strlen(str);
    if (b->length + len + 1 > b->capacity) {
        while (b->length + len + 1 > b->capacity) {
            b->capacity *= 2;
        }
        b->buffer = (char *)realloc(b->buffer, b->capacity);
    }
    memcpy(b->buffer + b->length, str, len);
    b->length += len;
    b->buffer[b->length] = '\0';
}

static void buf_appendf(Buffer *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[1024];
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len < 0) return;
    if ((size_t)len < sizeof(tmp)) {
        buf_append(b, tmp);
    } else {
        char *heap = (char *)malloc(len + 1);
        va_start(args, fmt);
        vsnprintf(heap, len + 1, fmt, args);
        va_end(args);
        buf_append(b, heap);
        free(heap);
    }
}

static const char *c_type_str(IrType *type) {
    if (!type) return "int";
    switch (type->kind) {
        case IR_TYPE_VOID:   return "void";
        case IR_TYPE_I32:    return "int";
        case IR_TYPE_I64:    return "long";
        case IR_TYPE_F64:    return "double";
        case IR_TYPE_BOOL:   return "bool";
        case IR_TYPE_CHAR:   return "char";
        case IR_TYPE_PTR:    return "void*";
        case IR_TYPE_STRUCT: return type->name ? type->name : "struct_unnamed";
        default:             return "int";
    }
}

static void sanitize_name(const char *in, char *out, size_t out_sz) {
    if (!in || out_sz == 0) return;
    size_t i = 0, j = 0;
    while (in[i] && j < out_sz - 1) {
        if (isalnum((unsigned char)in[i]) || in[i] == '_') {
            out[j++] = in[i];
        } else {
            out[j++] = '_';
        }
        i++;
    }
    out[j] = '\0';
}

static void print_c_val(Buffer *b, IrValue *val) {
    if (!val) {
        buf_append(b, "0");
        return;
    }
    switch (val->kind) {
        case IR_VAL_CONST_INT:
            buf_appendf(b, "%lld", (long long)val->const_val.int_val);
            break;
        case IR_VAL_CONST_FLOAT:
            buf_appendf(b, "%.17g", val->const_val.float_val);
            break;
        case IR_VAL_CONST_BOOL:
            buf_append(b, val->const_val.bool_val ? "true" : "false");
            break;
        case IR_VAL_CONST_CHAR:
            buf_appendf(b, "'%c'", val->const_val.char_val);
            break;
        case IR_VAL_CONST_STRING:
            buf_appendf(b, "\"%s\"", val->const_val.str_val ? val->const_val.str_val : "");
            break;
        case IR_VAL_REG:
            buf_appendf(b, "__reg_%d", val->id);
            break;
        case IR_VAL_VAR: {
            char clean[128];
            sanitize_name(val->name ? val->name : "var", clean, sizeof(clean));
            buf_appendf(b, "__var_%s", clean);
            break;
        }
        case IR_VAL_PARAM: {
            char clean[128];
            sanitize_name(val->name ? val->name : "param", clean, sizeof(clean));
            buf_appendf(b, "__param_%s", clean);
            break;
        }
        case IR_VAL_GLOBAL:
            buf_append(b, val->name ? val->name : "global");
            break;
    }
}

char *ir_generate_c_instrumented(IrModule *module, const char *profile_out_path) {
    if (!module) return NULL;
    Buffer b = create_buf();

    /* 1. Collect all instrumentation points */
    size_t loc_cap = 256;
    size_t loc_count = 0;
    ProfLoc *locs = (ProfLoc *)malloc(loc_cap * sizeof(ProfLoc));

    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        /* Function entry counter */
        if (loc_count >= loc_cap) { loc_cap *= 2; locs = (ProfLoc *)realloc(locs, loc_cap * sizeof(ProfLoc)); }
        locs[loc_count].kind = PROF_LOC_FN;
        snprintf(locs[loc_count].fn_name, sizeof(locs[loc_count].fn_name), "%s", fn->name ? fn->name : "fn");
        locs[loc_count].bb_name[0] = '\0';
        locs[loc_count].counter_id = (int)loc_count;
        loc_count++;

        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            /* Basic block counter */
            if (loc_count >= loc_cap) { loc_cap *= 2; locs = (ProfLoc *)realloc(locs, loc_cap * sizeof(ProfLoc)); }
            locs[loc_count].kind = PROF_LOC_BB;
            snprintf(locs[loc_count].fn_name, sizeof(locs[loc_count].fn_name), "%s", fn->name ? fn->name : "fn");
            snprintf(locs[loc_count].bb_name, sizeof(locs[loc_count].bb_name), "%s", bb->name ? bb->name : "bb");
            locs[loc_count].counter_id = (int)loc_count;
            loc_count++;

            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->op == IR_OP_CONDBR) {
                    /* Branch true counter */
                    if (loc_count >= loc_cap) { loc_cap *= 2; locs = (ProfLoc *)realloc(locs, loc_cap * sizeof(ProfLoc)); }
                    locs[loc_count].kind = PROF_LOC_BR_TRUE;
                    snprintf(locs[loc_count].fn_name, sizeof(locs[loc_count].fn_name), "%s", fn->name ? fn->name : "fn");
                    snprintf(locs[loc_count].bb_name, sizeof(locs[loc_count].bb_name), "%s", bb->name ? bb->name : "bb");
                    locs[loc_count].counter_id = (int)loc_count;
                    loc_count++;

                    /* Branch false counter */
                    if (loc_count >= loc_cap) { loc_cap *= 2; locs = (ProfLoc *)realloc(locs, loc_cap * sizeof(ProfLoc)); }
                    locs[loc_count].kind = PROF_LOC_BR_FALSE;
                    snprintf(locs[loc_count].fn_name, sizeof(locs[loc_count].fn_name), "%s", fn->name ? fn->name : "fn");
                    snprintf(locs[loc_count].bb_name, sizeof(locs[loc_count].bb_name), "%s", bb->name ? bb->name : "bb");
                    locs[loc_count].counter_id = (int)loc_count;
                    loc_count++;
                }
            }
        }
    }

    /* 2. Emit C Prelude & Instrumentation Infrastructure */
    buf_append(&b, "// Generated by Cco Compiler with Profile-Guided Instrumentation\n");
    buf_append(&b, "#define _POSIX_C_SOURCE 200809L\n");
    buf_append(&b, "#if defined(__GNUC__) || defined(__clang__)\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wunused-label\"\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wunused-variable\"\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wunused-but-set-variable\"\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wunused-but-set-parameter\"\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wmain\"\n");
    buf_append(&b, "#endif\n");
    buf_append(&b, "#include <stdio.h>\n");
    buf_append(&b, "#include <stdlib.h>\n");
    buf_append(&b, "#include <stdbool.h>\n");
    buf_append(&b, "#include <stdint.h>\n");
    buf_append(&b, "#include <string.h>\n");
    buf_append(&b, "#include <math.h>\n\n");

    /* Total counter array */
    buf_appendf(&b, "#define __CCO_PROF_TOTAL_COUNTERS %zu\n", loc_count > 0 ? loc_count : 1);
    buf_append(&b, "static uint64_t __cco_prof_counters[__CCO_PROF_TOTAL_COUNTERS] = {0};\n\n");

    /* Metadata table */
    buf_append(&b, "typedef struct { int kind; const char *fn; const char *bb; } __CcoProfMeta;\n");
    buf_append(&b, "static const __CcoProfMeta __cco_prof_meta[__CCO_PROF_TOTAL_COUNTERS] = {\n");
    for (size_t i = 0; i < loc_count; i++) {
        buf_appendf(&b, "    { %d, \"%s\", \"%s\" },\n", (int)locs[i].kind, locs[i].fn_name, locs[i].bb_name);
    }
    if (loc_count == 0) {
        buf_append(&b, "    { 0, \"\", \"\" }\n");
    }
    buf_append(&b, "};\n\n");

    /* Profile dump function */
    const char *out_file = profile_out_path ? profile_out_path : "cco.profdata";
    buf_append(&b, "static void __cco_profile_dump_runtime(void) {\n");
    buf_append(&b, "    const char *out_path = getenv(\"CCO_PROFILE_FILE\");\n");
    buf_appendf(&b, "    if (!out_path || strlen(out_path) == 0) out_path = \"%s\";\n", out_file);
    buf_append(&b, "    FILE *f = fopen(out_path, \"w\");\n");
    buf_append(&b, "    if (!f) return;\n");
    buf_append(&b, "    fprintf(f, \"CCO_PROFILE_V1\\n\\n\");\n");
    buf_append(&b, "    for (size_t i = 0; i < __CCO_PROF_TOTAL_COUNTERS; i++) {\n");
    buf_append(&b, "        if (__cco_prof_meta[i].kind == 0) {\n"); /* Function */
    buf_append(&b, "            fprintf(f, \"function %s %lu\\n\", __cco_prof_meta[i].fn, (unsigned long)__cco_prof_counters[i]);\n");
    buf_append(&b, "        }\n");
    buf_append(&b, "    }\n");
    buf_append(&b, "    fprintf(f, \"\\n\");\n");
    buf_append(&b, "    for (size_t i = 0; i < __CCO_PROF_TOTAL_COUNTERS; i++) {\n");
    buf_append(&b, "        if (__cco_prof_meta[i].kind == 1) {\n"); /* Block */
    buf_append(&b, "            fprintf(f, \"block %s.%s %lu\\n\", __cco_prof_meta[i].fn, __cco_prof_meta[i].bb, (unsigned long)__cco_prof_counters[i]);\n");
    buf_append(&b, "        }\n");
    buf_append(&b, "    }\n");
    buf_append(&b, "    fprintf(f, \"\\n\");\n");
    buf_append(&b, "    for (size_t i = 0; i < __CCO_PROF_TOTAL_COUNTERS; i++) {\n");
    buf_append(&b, "        if (__cco_prof_meta[i].kind == 2) {\n");
    buf_append(&b, "            fprintf(f, \"branch %s.%s true %lu\\n\", __cco_prof_meta[i].fn, __cco_prof_meta[i].bb, (unsigned long)__cco_prof_counters[i]);\n");
    buf_append(&b, "        } else if (__cco_prof_meta[i].kind == 3) {\n");
    buf_append(&b, "            fprintf(f, \"branch %s.%s false %lu\\n\", __cco_prof_meta[i].fn, __cco_prof_meta[i].bb, (unsigned long)__cco_prof_counters[i]);\n");
    buf_append(&b, "        }\n");
    buf_append(&b, "    }\n");
    buf_append(&b, "    fclose(f);\n");
    buf_append(&b, "}\n\n");

    /* Struct definitions */
    for (IrStructDecl *st = module->first_struct; st; st = st->next) {
        buf_appendf(&b, "typedef struct %s %s;\n", st->name, st->name);
    }
    for (IrStructDecl *st = module->first_struct; st; st = st->next) {
        buf_appendf(&b, "struct %s {\n", st->name);
        for (int f = 0; f < st->field_count; f++) {
            buf_appendf(&b, "    %s %s;\n", c_type_str(st->fields[f].type), st->fields[f].name);
        }
        buf_append(&b, "};\n\n");
    }

    /* Function Prototypes */
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (strcmp(fn->name, "main") == 0) continue;
        buf_appendf(&b, "%s %s(", c_type_str(fn->return_type), fn->name ? fn->name : "unnamed");
        if (fn->param_count == 0) {
            buf_append(&b, "void");
        } else {
            for (int p = 0; p < fn->param_count; p++) {
                if (p > 0) buf_append(&b, ", ");
                char clean[128];
                sanitize_name(fn->param_names[p], clean, sizeof(clean));
                buf_appendf(&b, "%s __param_%s", c_type_str(fn->param_types[p]), clean);
            }
        }
        buf_append(&b, ");\n");
    }
    if (module->first_fn) buf_append(&b, "\n");

    /* 3. Emit Instrumented Functions */
    size_t cur_loc = 0;

    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        const char *ret_str = (strcmp(fn->name, "main") == 0) ? "int" : c_type_str(fn->return_type);
        buf_appendf(&b, "%s %s(", ret_str, fn->name ? fn->name : "unnamed");
        if (fn->param_count == 0) {
            buf_append(&b, "void");
        } else {
            for (int p = 0; p < fn->param_count; p++) {
                if (p > 0) buf_append(&b, ", ");
                char clean[128];
                sanitize_name(fn->param_names[p], clean, sizeof(clean));
                buf_appendf(&b, "%s __param_%s", c_type_str(fn->param_types[p]), clean);
            }
        }
        buf_append(&b, ") {\n");

        /* If this is main, register atexit */
        if (strcmp(fn->name, "main") == 0) {
            buf_append(&b, "    atexit(__cco_profile_dump_runtime);\n");
        }

        /* Function entry instrumentation */
        if (cur_loc < loc_count && locs[cur_loc].kind == PROF_LOC_FN) {
            buf_appendf(&b, "    __cco_prof_counters[%d]++;\n", locs[cur_loc].counter_id);
            cur_loc++;
        }

        /* Emit local variables */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                if (inst->op == IR_OP_ALLOCA && inst->result && inst->result->kind == IR_VAL_VAR) {
                    char clean[128];
                    sanitize_name(inst->result->name ? inst->result->name : "var", clean, sizeof(clean));
                    IrType *elem = inst->result->type->elem_type ? inst->result->type->elem_type : inst->result->type;
                    buf_appendf(&b, "    %s __var_%s = 0;\n", c_type_str(elem), clean);
                }
                if (inst->result && inst->result->kind == IR_VAL_REG) {
                    buf_appendf(&b, "    %s __reg_%d = 0;\n", c_type_str(inst->result->type), inst->result->id);
                }
            }
        }

        /* Emit basic blocks */
        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            char bb_clean[128];
            sanitize_name(bb->name ? bb->name : "block", bb_clean, sizeof(bb_clean));
            buf_appendf(&b, "\n__bb_%s:\n", bb_clean);

            /* Block entry instrumentation */
            if (cur_loc < loc_count && locs[cur_loc].kind == PROF_LOC_BB) {
                buf_appendf(&b, "    __cco_prof_counters[%d]++;\n", locs[cur_loc].counter_id);
                cur_loc++;
            }

            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                switch (inst->op) {
                    case IR_OP_ALLOCA:
                        break;

                    case IR_OP_CONST:
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->result);
                        buf_append(&b, " = ");
                        print_c_val(&b, inst->lhs);
                        buf_append(&b, ";\n");
                        break;

                    case IR_OP_LOAD:
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->result);
                        buf_append(&b, " = ");
                        print_c_val(&b, inst->lhs);
                        buf_append(&b, ";\n");
                        break;

                    case IR_OP_STORE:
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->rhs);
                        buf_append(&b, " = ");
                        print_c_val(&b, inst->lhs);
                        buf_append(&b, ";\n");
                        break;

                    case IR_OP_ADD:
                    case IR_OP_SUB:
                    case IR_OP_MUL:
                    case IR_OP_DIV:
                    case IR_OP_MOD: {
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->result);
                        buf_append(&b, " = (");
                        print_c_val(&b, inst->lhs);
                        const char *op_sym = "+";
                        if (inst->op == IR_OP_SUB) op_sym = "-";
                        else if (inst->op == IR_OP_MUL) op_sym = "*";
                        else if (inst->op == IR_OP_DIV) op_sym = "/";
                        else if (inst->op == IR_OP_MOD) op_sym = "%";
                        buf_appendf(&b, " %s ", op_sym);
                        print_c_val(&b, inst->rhs);
                        buf_append(&b, ");\n");
                        break;
                    }

                    case IR_OP_NEG:
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->result);
                        buf_append(&b, " = -(");
                        print_c_val(&b, inst->lhs);
                        buf_append(&b, ");\n");
                        break;

                    case IR_OP_EQ:
                    case IR_OP_NE:
                    case IR_OP_LT:
                    case IR_OP_LE:
                    case IR_OP_GT:
                    case IR_OP_GE: {
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->result);
                        buf_append(&b, " = (");
                        print_c_val(&b, inst->lhs);
                        const char *cmp_sym = "==";
                        if (inst->op == IR_OP_NE) cmp_sym = "!=";
                        else if (inst->op == IR_OP_LT) cmp_sym = "<";
                        else if (inst->op == IR_OP_LE) cmp_sym = "<=";
                        else if (inst->op == IR_OP_GT) cmp_sym = ">";
                        else if (inst->op == IR_OP_GE) cmp_sym = ">=";
                        buf_appendf(&b, " %s ", cmp_sym);
                        print_c_val(&b, inst->rhs);
                        buf_append(&b, ");\n");
                        break;
                    }

                    case IR_OP_AND:
                    case IR_OP_OR: {
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->result);
                        buf_append(&b, " = (");
                        print_c_val(&b, inst->lhs);
                        buf_appendf(&b, " %s ", inst->op == IR_OP_AND ? "&&" : "||");
                        print_c_val(&b, inst->rhs);
                        buf_append(&b, ");\n");
                        break;
                    }

                    case IR_OP_NOT:
                        buf_append(&b, "    ");
                        print_c_val(&b, inst->result);
                        buf_append(&b, " = !(");
                        print_c_val(&b, inst->lhs);
                        buf_append(&b, ");\n");
                        break;

                    case IR_OP_CALL: {
                        buf_append(&b, "    ");
                        if (inst->result) {
                            print_c_val(&b, inst->result);
                            buf_append(&b, " = ");
                        }
                        buf_appendf(&b, "%s(", inst->callee_name ? inst->callee_name : "fn");
                        for (int a = 0; a < inst->arg_count; a++) {
                            if (a > 0) buf_append(&b, ", ");
                            print_c_val(&b, inst->args[a]);
                        }
                        buf_append(&b, ");\n");
                        break;
                    }

                    case IR_OP_PRINT: {
                        buf_append(&b, "    ");
                        if (inst->lhs->type->kind == IR_TYPE_BOOL) {
                            buf_append(&b, "printf(\"%s\\n\", (");
                            print_c_val(&b, inst->lhs);
                            buf_append(&b, ") ? \"true\" : \"false\");\n");
                        } else if (inst->lhs->type->kind == IR_TYPE_I32 || inst->lhs->type->kind == IR_TYPE_I64) {
                            buf_append(&b, "printf(\"%ld\\n\", (long)(");
                            print_c_val(&b, inst->lhs);
                            buf_append(&b, "));\n");
                        } else if (inst->lhs->type->kind == IR_TYPE_F64) {
                            buf_append(&b, "printf(\"%g\\n\", (double)(");
                            print_c_val(&b, inst->lhs);
                            buf_append(&b, "));\n");
                        } else if (inst->lhs->type->kind == IR_TYPE_CHAR) {
                            buf_append(&b, "printf(\"%c\\n\", (char)(");
                            print_c_val(&b, inst->lhs);
                            buf_append(&b, "));\n");
                        } else {
                            buf_append(&b, "printf(\"%ld\\n\", (long)(");
                            print_c_val(&b, inst->lhs);
                            buf_append(&b, "));\n");
                        }
                        break;
                    }

                    case IR_OP_BR: {
                        char target_clean[128];
                        sanitize_name(inst->target_true ? inst->target_true->name : "block", target_clean, sizeof(target_clean));
                        buf_appendf(&b, "    goto __bb_%s;\n", target_clean);
                        break;
                    }

                    case IR_OP_CONDBR: {
                        char true_clean[128];
                        char false_clean[128];
                        sanitize_name(inst->target_true ? inst->target_true->name : "block", true_clean, sizeof(true_clean));
                        sanitize_name(inst->target_false ? inst->target_false->name : "block", false_clean, sizeof(false_clean));

                        int br_true_id = -1, br_false_id = -1;
                        if (cur_loc < loc_count && locs[cur_loc].kind == PROF_LOC_BR_TRUE) {
                            br_true_id = locs[cur_loc].counter_id;
                            cur_loc++;
                        }
                        if (cur_loc < loc_count && locs[cur_loc].kind == PROF_LOC_BR_FALSE) {
                            br_false_id = locs[cur_loc].counter_id;
                            cur_loc++;
                        }

                        buf_append(&b, "    if (");
                        print_c_val(&b, inst->lhs);
                        buf_append(&b, ") {\n");
                        if (br_true_id >= 0) buf_appendf(&b, "        __cco_prof_counters[%d]++;\n", br_true_id);
                        buf_appendf(&b, "        goto __bb_%s;\n", true_clean);
                        buf_append(&b, "    } else {\n");
                        if (br_false_id >= 0) buf_appendf(&b, "        __cco_prof_counters[%d]++;\n", br_false_id);
                        buf_appendf(&b, "        goto __bb_%s;\n", false_clean);
                        buf_append(&b, "    }\n");
                        break;
                    }

                    case IR_OP_RET:
                        if (inst->lhs) {
                            buf_append(&b, "    return ");
                            print_c_val(&b, inst->lhs);
                            buf_append(&b, ";\n");
                        } else {
                            if (strcmp(fn->name, "main") == 0) {
                                buf_append(&b, "    return 0;\n");
                            } else {
                                buf_append(&b, "    return;\n");
                            }
                        }
                        break;

                    default:
                        break;
                }
            }
        }

        if (strcmp(fn->name, "main") == 0) {
            buf_append(&b, "    return 0;\n");
        }
        buf_append(&b, "}\n\n");
    }

    free(locs);
    return b.buffer;
}
