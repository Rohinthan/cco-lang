// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_dominance.h"
#include "ir_ssa.h"
#include "ir_opt.h"
#include "ir.h"
#include "ir_verify.h"
#include "ir_lower.h"
#include "ir_print.h"
#include "lexer.h"
#include "parser.h"
#include "scope_analysis.h"
#include "x86_64_codegen.h"
#include "x86_64_link.h"
#include "codegen.h"
#include "ir_codegen_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

/* Helper to compile Cco source into an IrModule */
static IrModule *parse_and_lower_ir(const char *src, AstArena **out_arena, TokenArray *out_tokens) {
    *out_tokens = lex_source(src);
    *out_arena = create_ast_arena();
    Parser parser = create_parser(*out_tokens, *out_arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, *out_arena, "test_ssa_input.cco");
    analyze_scopes(ast, *out_arena);

    IrModule *ir_mod = ir_lower_ast(ast, "test_ssa_input.cco");
    assert(ir_mod != NULL);

    char *v_err = NULL;
    bool valid = ir_verify_module(ir_mod, &v_err);
    if (!valid) {
        fprintf(stderr, "Initial IR verify failed: %s\n", v_err ? v_err : "unknown");
        if (v_err) free(v_err);
    }
    assert(valid);
    return ir_mod;
}

static void cleanup_ir(IrModule *ir_mod, AstArena *arena, TokenArray *tokens) {
    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(tokens);
}

/* Synthetic CFG dominance tests */

static void test_dominance_straight_line(void) {
    printf("Testing CFG Dominance: Straight-line...\n");
    IrModule *mod = ir_module_create("dom_straight");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "straight", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBasicBlock *b_mid   = ir_block_create(fn, "mid");
    IrBasicBlock *b_exit  = ir_block_create(fn, "exit");

    IrBuilder builder;
    ir_builder_init(&builder, mod);

    ir_builder_set_insert_block(&builder, b_entry);
    ir_emit_br(&builder, b_mid);

    ir_builder_set_insert_block(&builder, b_mid);
    ir_emit_br(&builder, b_exit);

    ir_builder_set_insert_block(&builder, b_exit);
    IrValue *v0 = ir_val_const_int(mod->arena, i32_type, 42);
    ir_emit_ret(&builder, v0);

    ir_recompute_cfg(fn);

    IrDomInfo *dom = ir_dominance_compute(fn);
    assert(dom != NULL);
    assert(ir_dominance_verify(dom, NULL));

    /* dom(entry) = {entry} */
    assert(ir_dominates(dom, b_entry, b_entry));
    assert(!ir_dominates(dom, b_mid, b_entry));
    assert(!ir_dominates(dom, b_exit, b_entry));

    /* dom(mid) = {entry, mid} */
    assert(ir_dominates(dom, b_entry, b_mid));
    assert(ir_dominates(dom, b_mid, b_mid));
    assert(!ir_dominates(dom, b_exit, b_mid));

    /* dom(exit) = {entry, mid, exit} */
    assert(ir_dominates(dom, b_entry, b_exit));
    assert(ir_dominates(dom, b_mid, b_exit));
    assert(ir_dominates(dom, b_exit, b_exit));

    /* idom checks */
    assert(ir_get_idom(dom, b_entry) == NULL);
    assert(ir_get_idom(dom, b_mid) == b_entry);
    assert(ir_get_idom(dom, b_exit) == b_mid);

    /* Dominance Frontiers: all empty for straight-line */
    int count = 0;
    IrBasicBlock *const *df = ir_get_dominance_frontier(dom, b_entry, &count);
    assert(count == 0 && df == NULL);
    df = ir_get_dominance_frontier(dom, b_mid, &count);
    assert(count == 0 && df == NULL);
    df = ir_get_dominance_frontier(dom, b_exit, &count);
    assert(count == 0 && df == NULL);

    ir_dominance_free(dom);
    ir_module_free(mod);
    printf("  -> PASSED\n");
}

static void test_dominance_diamond(void) {
    printf("Testing CFG Dominance: Diamond (if-then-else)...\n");
    IrModule *mod = ir_module_create("dom_diamond");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "diamond", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBasicBlock *b_then  = ir_block_create(fn, "then");
    IrBasicBlock *b_else  = ir_block_create(fn, "else");
    IrBasicBlock *b_merge = ir_block_create(fn, "merge");

    IrBuilder builder;
    ir_builder_init(&builder, mod);

    ir_builder_set_insert_block(&builder, b_entry);
    IrValue *c = ir_val_const_bool(mod->arena, true);
    ir_emit_condbr(&builder, c, b_then, b_else);

    ir_builder_set_insert_block(&builder, b_then);
    ir_emit_br(&builder, b_merge);

    ir_builder_set_insert_block(&builder, b_else);
    ir_emit_br(&builder, b_merge);

    ir_builder_set_insert_block(&builder, b_merge);
    IrValue *v0 = ir_val_const_int(mod->arena, i32_type, 1);
    ir_emit_ret(&builder, v0);

    ir_recompute_cfg(fn);

    IrDomInfo *dom = ir_dominance_compute(fn);
    assert(dom != NULL);
    assert(ir_dominance_verify(dom, NULL));

    /* entry dominates all */
    assert(ir_dominates(dom, b_entry, b_then));
    assert(ir_dominates(dom, b_entry, b_else));
    assert(ir_dominates(dom, b_entry, b_merge));

    /* then does NOT dominate merge, else does NOT dominate merge */
    assert(!ir_dominates(dom, b_then, b_merge));
    assert(!ir_dominates(dom, b_else, b_merge));

    /* idom checks: idom(then)=entry, idom(else)=entry, idom(merge)=entry */
    assert(ir_get_idom(dom, b_then) == b_entry);
    assert(ir_get_idom(dom, b_else) == b_entry);
    assert(ir_get_idom(dom, b_merge) == b_entry);

    /* Dominance Frontiers:
     * DF(entry) = {}
     * DF(then) = {merge}
     * DF(else) = {merge}
     * DF(merge) = {}
     */
    int count = 0;
    IrBasicBlock *const *df = ir_get_dominance_frontier(dom, b_entry, &count);
    assert(count == 0);

    df = ir_get_dominance_frontier(dom, b_then, &count);
    assert(count == 1 && df[0] == b_merge);

    df = ir_get_dominance_frontier(dom, b_else, &count);
    assert(count == 1 && df[0] == b_merge);

    df = ir_get_dominance_frontier(dom, b_merge, &count);
    assert(count == 0);

    ir_dominance_free(dom);
    ir_module_free(mod);
    printf("  -> PASSED\n");
}

static void test_dominance_loop(void) {
    printf("Testing CFG Dominance: Natural Loop...\n");
    IrModule *mod = ir_module_create("dom_loop");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "loop_fn", i32_type);

    IrBasicBlock *b_entry  = ir_block_create(fn, "entry");
    IrBasicBlock *b_header = ir_block_create(fn, "header");
    IrBasicBlock *b_body   = ir_block_create(fn, "body");
    IrBasicBlock *b_exit   = ir_block_create(fn, "exit");

    IrBuilder builder;
    ir_builder_init(&builder, mod);

    ir_builder_set_insert_block(&builder, b_entry);
    ir_emit_br(&builder, b_header);

    ir_builder_set_insert_block(&builder, b_header);
    IrValue *c = ir_val_const_bool(mod->arena, true);
    ir_emit_condbr(&builder, c, b_body, b_exit);

    ir_builder_set_insert_block(&builder, b_body);
    ir_emit_br(&builder, b_header);

    ir_builder_set_insert_block(&builder, b_exit);
    IrValue *v0 = ir_val_const_int(mod->arena, i32_type, 0);
    ir_emit_ret(&builder, v0);

    ir_recompute_cfg(fn);

    IrDomInfo *dom = ir_dominance_compute(fn);
    assert(dom != NULL);
    assert(ir_dominance_verify(dom, NULL));

    /* header dominates body and exit */
    assert(ir_dominates(dom, b_header, b_body));
    assert(ir_dominates(dom, b_header, b_exit));

    /* body does NOT dominate header or exit */
    assert(!ir_dominates(dom, b_body, b_header));
    assert(!ir_dominates(dom, b_body, b_exit));

    /* idom checks:
     * idom(header) = entry
     * idom(body) = header
     * idom(exit) = header
     */
    assert(ir_get_idom(dom, b_header) == b_entry);
    assert(ir_get_idom(dom, b_body) == b_header);
    assert(ir_get_idom(dom, b_exit) == b_header);

    /* Dominance Frontier:
     * DF(body) = {header}
     * DF(header) = {header}
     * DF(exit) = {}
     */
    int count = 0;
    IrBasicBlock *const *df = ir_get_dominance_frontier(dom, b_body, &count);
    assert(count == 1 && df[0] == b_header);

    df = ir_get_dominance_frontier(dom, b_header, &count);
    assert(count == 1 && df[0] == b_header);

    df = ir_get_dominance_frontier(dom, b_exit, &count);
    assert(count == 0);

    ir_dominance_free(dom);
    ir_module_free(mod);
    printf("  -> PASSED\n");
}

static void test_dominance_unreachable(void) {
    printf("Testing CFG Dominance: Unreachable Block Handling...\n");
    IrModule *mod = ir_module_create("dom_unreach");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "unreach_fn", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBasicBlock *b_dead  = ir_block_create(fn, "dead");
    IrBasicBlock *b_exit  = ir_block_create(fn, "exit");

    IrBuilder builder;
    ir_builder_init(&builder, mod);

    ir_builder_set_insert_block(&builder, b_entry);
    ir_emit_br(&builder, b_exit);

    ir_builder_set_insert_block(&builder, b_dead);
    ir_emit_br(&builder, b_exit);

    ir_builder_set_insert_block(&builder, b_exit);
    IrValue *v0 = ir_val_const_int(mod->arena, i32_type, 0);
    ir_emit_ret(&builder, v0);

    ir_recompute_cfg(fn);

    IrDomInfo *dom = ir_dominance_compute(fn);
    assert(dom != NULL);
    assert(ir_dominance_verify(dom, NULL));

    /* b_dead is unreachable from entry */
    assert(!ir_dominates(dom, b_entry, b_dead));
    assert(ir_get_idom(dom, b_dead) == NULL);

    /* b_entry dominates b_exit directly */
    assert(ir_dominates(dom, b_entry, b_exit));
    assert(ir_get_idom(dom, b_exit) == b_entry);

    ir_dominance_free(dom);
    ir_module_free(mod);
    printf("  -> PASSED\n");
}

/* SSA construction tests */

static void test_ssa_construction_if_else(void) {
    printf("Testing SSA Construction: If-Else mutable scalar promotion...\n");
    const char *src =
        "fn test_branch(cond: bool) -> int {\n"
        "    let x: int = 10;\n"
        "    if (cond) {\n"
        "        x = 20;\n"
        "    } else {\n"
        "        x = 30;\n"
        "    }\n"
        "    return x;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrFunction *fn = ir_module_find_function(mod, "test_branch");
    assert(fn != NULL);

    IrSsaStats stats;
    char *err = NULL;
    bool ok = ir_ssa_construct_function(fn, &stats, &err);
    if (!ok) {
        fprintf(stderr, "SSA construct failed: %s\n", err ? err : "unknown");
        if (err) free(err);
    }
    assert(ok);
    assert(stats.promoted_allocas >= 1);
    assert(stats.phi_nodes_created >= 1);

    /* Verify SSA invariants */
    char *verr = NULL;
    bool valid = ir_ssa_verify_function(fn, NULL, &verr);
    if (!valid) {
        fprintf(stderr, "SSA verify failed: %s\n", verr ? verr : "unknown");
        if (verr) free(verr);
    }
    assert(valid);

    /* Check that a phi instruction exists in the join block */
    bool found_phi = false;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_PHI) {
                found_phi = true;
                assert(inst->phi_count == 2);
            }
        }
    }
    assert(found_phi);

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED\n");
}

static void test_ssa_construction_while_loop(void) {
    printf("Testing SSA Construction: While Loop loop-carried values...\n");
    const char *src =
        "fn sum_to_n(n: int) -> int {\n"
        "    let sum: int = 0;\n"
        "    let i: int = 0;\n"
        "    while (i < n) {\n"
        "        sum = sum + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return sum;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrFunction *fn = ir_module_find_function(mod, "sum_to_n");
    assert(fn != NULL);

    IrSsaStats stats;
    char *err = NULL;
    bool ok = ir_ssa_construct_function(fn, &stats, &err);
    if (!ok) {
        fprintf(stderr, "SSA construct failed: %s\n", err ? err : "unknown");
        if (err) free(err);
    }
    assert(ok);
    assert(stats.promoted_allocas >= 2); /* both sum and i promoted */
    assert(stats.phi_nodes_created >= 2);   /* phis in while loop header */

    /* Verify SSA invariants */
    char *verr = NULL;
    bool valid = ir_ssa_verify_function(fn, NULL, &verr);
    if (!valid) {
        fprintf(stderr, "SSA verify failed: %s\n", verr ? verr : "unknown");
        if (verr) free(verr);
    }
    assert(valid);

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED\n");
}

/* SSA verifier negative tests */

static void test_ssa_verifier_negative_non_dominating_use(void) {
    printf("Testing SSA Verifier Negative: Non-dominating definition use...\n");
    /* Create a diamond CFG where merge uses a register defined only in then branch */
    IrModule *mod = ir_module_create("verif_neg1");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "neg1", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBasicBlock *b_then  = ir_block_create(fn, "then");
    IrBasicBlock *b_else  = ir_block_create(fn, "else");
    IrBasicBlock *b_merge = ir_block_create(fn, "merge");

    IrBuilder builder;
    ir_builder_init(&builder, mod);

    ir_builder_set_insert_block(&builder, b_entry);
    IrValue *c = ir_val_const_bool(mod->arena, true);
    ir_emit_condbr(&builder, c, b_then, b_else);

    /* In then branch, define %r10 = const 42 */
    ir_builder_set_insert_block(&builder, b_then);
    IrInstruction *def_inst = ir_emit_const_int(&builder, i32_type, 42);
    def_inst->result = ir_val_reg(mod->arena, i32_type, 10);
    ir_emit_br(&builder, b_merge);

    /* In else branch, do nothing */
    ir_builder_set_insert_block(&builder, b_else);
    ir_emit_br(&builder, b_merge);

    /* In merge block, use %r10 directly in ret! */
    ir_builder_set_insert_block(&builder, b_merge);
    IrValue *use_r10 = ir_val_reg(mod->arena, i32_type, 10);
    ir_emit_ret(&builder, use_r10);

    ir_recompute_cfg(fn);

    char *verr = NULL;
    bool valid = ir_ssa_verify_function(fn, NULL, &verr);
    assert(!valid);
    assert(verr != NULL);
    assert(strstr(verr, "does not dominate") != NULL || strstr(verr, "dominated") != NULL);
    free(verr);

    ir_module_free(mod);
    printf("  -> PASSED (correctly rejected non-dominating definition)\n");
}

static void test_ssa_verifier_negative_duplicate_def(void) {
    printf("Testing SSA Verifier Negative: Duplicate definition of SSA register...\n");
    IrModule *mod = ir_module_create("verif_neg2");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "neg2", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBuilder builder;
    ir_builder_init(&builder, mod);
    ir_builder_set_insert_block(&builder, b_entry);

    /* First def of %r5 */
    IrInstruction *i1 = ir_emit_const_int(&builder, i32_type, 10);
    i1->result = ir_val_reg(mod->arena, i32_type, 5);

    /* Second def of %r5 in same function */
    IrInstruction *i2 = ir_emit_const_int(&builder, i32_type, 20);
    i2->result = ir_val_reg(mod->arena, i32_type, 5);

    ir_emit_ret(&builder, i2->result);
    ir_recompute_cfg(fn);

    char *verr = NULL;
    bool valid = ir_ssa_verify_function(fn, NULL, &verr);
    assert(!valid);
    assert(verr != NULL);
    assert(strstr(verr, "Multiple definitions") != NULL || strstr(verr, "multiple") != NULL);
    free(verr);

    ir_module_free(mod);
    printf("  -> PASSED (correctly rejected duplicate SSA definition)\n");
}

static void test_ssa_verifier_negative_phi_placement(void) {
    printf("Testing SSA Verifier Negative: Phi node after normal instructions...\n");
    IrModule *mod = ir_module_create("verif_neg3");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "neg3", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBuilder builder;
    ir_builder_init(&builder, mod);
    ir_builder_set_insert_block(&builder, b_entry);

    /* Normal instruction first */
    ir_emit_const_int(&builder, i32_type, 1);

    /* Phi instruction second (illegal in basic block) */
    ir_emit_phi(&builder, i32_type);

    IrValue *v0 = ir_val_const_int(mod->arena, i32_type, 0);
    ir_emit_ret(&builder, v0);
    ir_recompute_cfg(fn);

    char *verr = NULL;
    bool valid = ir_ssa_verify_function(fn, NULL, &verr);
    assert(!valid);
    assert(verr != NULL);
    assert(strstr(verr, "after non-phi") != NULL || strstr(verr, "before non-phi") != NULL || strstr(verr, "top of basic block") != NULL);
    free(verr);

    ir_module_free(mod);
    printf("  -> PASSED (correctly rejected non-top phi instruction)\n");
}

/* SSA optimization tests */

static void test_ssa_optimizations(void) {
    printf("Testing SSA Optimizations: Constant Folding & DCE on SSA form...\n");
    const char *src =
        "fn calc() -> int {\n"
        "    let a: int = 15;\n"
        "    let b: int = 25;\n"
        "    let dead: int = a * 100;\n"
        "    let c: int = a + b;\n"
        "    return c;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrFunction *fn = ir_module_find_function(mod, "calc");
    assert(fn != NULL);

    /* Construct SSA */
    IrSsaStats stats;
    char *err = NULL;
    bool ok = ir_ssa_construct_function(fn, &stats, &err);
    assert(ok);

    /* Optimize SSA */
    bool changed = false;
    ok = ir_ssa_optimize_function(fn, &changed, &err);
    assert(ok);
    assert(changed);

    /* Verify SSA remains valid after optimization */
    char *verr = NULL;
    assert(ir_ssa_verify_function(fn, NULL, &verr));

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED\n");
}

/* SSA destruction tests */

static void test_ssa_destruction_critical_edge(void) {
    printf("Testing SSA Destruction: Critical edge splitting...\n");
    /* Construct a CFG with a critical edge:
     * P has 2 successors (S, other)
     * S has 2 predecessors (P, Q)
     * S has a phi node: phi [P: %p_val], [Q: %q_val]
     */
    IrModule *mod = ir_module_create("crit_edge_mod");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "crit_edge_fn", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBasicBlock *b_p     = ir_block_create(fn, "b_p");
    IrBasicBlock *b_q     = ir_block_create(fn, "b_q");
    IrBasicBlock *b_other = ir_block_create(fn, "b_other");
    IrBasicBlock *b_s     = ir_block_create(fn, "b_s");

    IrBuilder builder;
    ir_builder_init(&builder, mod);

    /* entry branches to P or Q */
    ir_builder_set_insert_block(&builder, b_entry);
    IrValue *c1 = ir_val_const_bool(mod->arena, true);
    ir_emit_condbr(&builder, c1, b_p, b_q);

    /* P branches to S or other (P has 2 successors) */
    ir_builder_set_insert_block(&builder, b_p);
    IrInstruction *p_def = ir_emit_const_int(&builder, i32_type, 111);
    IrValue *c2 = ir_val_const_bool(mod->arena, true);
    ir_emit_condbr(&builder, c2, b_s, b_other);

    /* Q branches to S (S has 2 predecessors: P, Q) */
    ir_builder_set_insert_block(&builder, b_q);
    IrInstruction *q_def = ir_emit_const_int(&builder, i32_type, 222);
    ir_emit_br(&builder, b_s);

    /* other branches to ret */
    ir_builder_set_insert_block(&builder, b_other);
    IrValue *v_oth = ir_val_const_int(mod->arena, i32_type, 0);
    ir_emit_ret(&builder, v_oth);

    /* S has a phi node */
    ir_builder_set_insert_block(&builder, b_s);
    IrInstruction *phi = ir_emit_phi(&builder, i32_type);
    ir_phi_add_incoming(phi, b_p, p_def->result, mod->arena);
    ir_phi_add_incoming(phi, b_q, q_def->result, mod->arena);
    ir_emit_ret(&builder, phi->result);

    ir_recompute_cfg(fn);

    /* Verify SSA valid prior to destruction */
    char *verr = NULL;
    bool valid = ir_ssa_verify_function(fn, NULL, &verr);
    assert(valid);

    /* Deconstruct SSA */
    IrSsaStats stats = {0};
    char *derr = NULL;
    bool dok = ir_ssa_deconstruct_function(fn, &stats, &derr);
    if (!derr && !dok) {
        fprintf(stderr, "Deconstruct error: %s\n", derr ? derr : "unknown");
    }
    assert(dok);
    assert(stats.split_edges >= 1);

    /* Verify that a split block was created */
    bool found_split_block = false;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (strncmp(bb->name, "split.", 6) == 0) {
            found_split_block = true;
            break;
        }
    }
    assert(found_split_block);

    /* Verify standard IR verifier passes after destruction */
    char *std_err = NULL;
    assert(ir_verify_function(fn, &std_err));

    ir_module_free(mod);
    printf("  -> PASSED (critical edge correctly split)\n");
}

static void test_ssa_destruction_cyclic_swap(void) {
    printf("Testing SSA Destruction: Parallel copy cycle breaking (x <-> y swap)...\n");
    /* Construct a block with a parallel swap on an incoming edge:
     * %phi_a = phi [entry: %b]
     * %phi_b = phi [entry: %a]
     */
    IrModule *mod = ir_module_create("cycle_swap_mod");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "cycle_fn", i32_type);

    IrBasicBlock *b_entry = ir_block_create(fn, "entry");
    IrBasicBlock *b_swap  = ir_block_create(fn, "swap");

    IrBuilder builder;
    ir_builder_init(&builder, mod);

    ir_builder_set_insert_block(&builder, b_entry);
    IrInstruction *def_a = ir_emit_const_int(&builder, i32_type, 10);
    IrInstruction *def_b = ir_emit_const_int(&builder, i32_type, 20);
    ir_emit_br(&builder, b_swap);

    ir_builder_set_insert_block(&builder, b_swap);
    IrInstruction *phi_a = ir_emit_phi(&builder, i32_type);
    IrInstruction *phi_b = ir_emit_phi(&builder, i32_type);

    ir_phi_add_incoming(phi_a, b_entry, def_b->result, mod->arena);
    ir_phi_add_incoming(phi_b, b_entry, def_a->result, mod->arena);

    IrInstruction *add = ir_emit_binary(&builder, IR_OP_ADD, i32_type, phi_a->result, phi_b->result);
    ir_emit_ret(&builder, add->result);

    ir_recompute_cfg(fn);

    /* Verify SSA valid prior to destruction */
    char *verr = NULL;
    assert(ir_ssa_verify_function(fn, NULL, &verr));

    /* Deconstruct SSA */
    IrSsaStats stats;
    char *derr = NULL;
    bool dok = ir_ssa_deconstruct_function(fn, &stats, &derr);
    assert(dok);

    /* Verify standard IR passes */
    char *std_err = NULL;
    assert(ir_verify_function(fn, &std_err));

    ir_module_free(mod);
    printf("  -> PASSED (parallel copy cycle resolved cleanly)\n");
}

/* End-to-end execution and differential tests */

static int run_binary_capture(const char *cmd, char *out_buf, size_t out_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    size_t n = fread(out_buf, 1, out_size - 1, fp);
    out_buf[n] = '\0';
    int status = pclose(fp);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static void test_ssa_e2e_differential(void) {
    printf("Testing End-to-End: Differential Execution (Default vs SSA)...\n");

    const char *test_progs[] = {
        /* 1. Branching & scalar assignments */
        "fn main() -> int {\n"
        "    let x: int = 5;\n"
        "    if (x > 3) {\n"
        "        x = x * 2;\n"
        "    } else {\n"
        "        x = x + 1;\n"
        "    }\n"
        "    print(x);\n"
        "    return 0;\n"
        "}\n",

        /* 2. While loop accumulator */
        "fn main() -> int {\n"
        "    let sum: int = 0;\n"
        "    let i: int = 1;\n"
        "    while (i <= 10) {\n"
        "        sum = sum + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    print(sum);\n"
        "    return 0;\n"
        "}\n",

        /* 3. Nested while loops */
        "fn main() -> int {\n"
        "    let count: int = 0;\n"
        "    let i: int = 0;\n"
        "    while (i < 3) {\n"
        "        let j: int = 0;\n"
        "        while (j < 4) {\n"
        "            count = count + 1;\n"
        "            j = j + 1;\n"
        "        }\n"
        "        i = i + 1;\n"
        "    }\n"
        "    print(count);\n"
        "    return 0;\n"
        "}\n",

        /* 4. Multiple variables and condition reassignments */
        "fn main() -> int {\n"
        "    let a: int = 100;\n"
        "    let b: int = 200;\n"
        "    if (a < b) {\n"
        "        let tmp: int = a;\n"
        "        a = b;\n"
        "        b = tmp;\n"
        "    }\n"
        "    print(a);\n"
        "    print(b);\n"
        "    return 0;\n"
        "}\n"
    };

    size_t prog_count = sizeof(test_progs) / sizeof(test_progs[0]);

    for (size_t p = 0; p < prog_count; p++) {
        const char *src_file = "build/tmp_diff_test.cco";
        FILE *f = fopen(src_file, "w");
        assert(f != NULL);
        fputs(test_progs[p], f);
        fclose(f);

        /* 1. Build and run reference (non-SSA, -O1) */
        char cmd_ref[512];
        snprintf(cmd_ref, sizeof(cmd_ref), "./cco %s -O1 -o build/tmp_ref && ./build/tmp_ref", src_file);
        char out_ref[1024] = {0};
        int code_ref = run_binary_capture(cmd_ref, out_ref, sizeof(out_ref));

        /* 2. Build and run with SSA enabled (--ssa, -O1) */
        char cmd_ssa[512];
        snprintf(cmd_ssa, sizeof(cmd_ssa), "./cco %s --ssa -O1 -o build/tmp_ssa && ./build/tmp_ssa", src_file);
        char out_ssa[1024] = {0};
        int code_ssa = run_binary_capture(cmd_ssa, out_ssa, sizeof(out_ssa));

        /* Compare exit codes and stdout */
        assert(code_ref == code_ssa);
        assert(strcmp(out_ref, out_ssa) == 0);

        /* 3. Build and run with SSA native internal linker */
        char cmd_ssa_link[512];
        snprintf(cmd_ssa_link, sizeof(cmd_ssa_link), "./cco %s --ssa --use-internal-linker -o build/tmp_ssa_link && ./build/tmp_ssa_link", src_file);
        char out_ssa_link[1024] = {0};
        int code_ssa_link = run_binary_capture(cmd_ssa_link, out_ssa_link, sizeof(out_ssa_link));

        assert(code_ref == code_ssa_link);
        assert(strcmp(out_ref, out_ssa_link) == 0);
    }

    unlink("build/tmp_diff_test.cco");
    unlink("build/tmp_ref");
    unlink("build/tmp_ssa");
    unlink("build/tmp_ssa_link");
    printf("  -> PASSED (100%% differential match between non-SSA, SSA, and native internal linker)\n");
}

/* Main test runner */

int main(void) {
    printf("Running SSA & Dominance Analysis Unit Tests...\n");

    /* 1. CFG Dominance Tests */
    test_dominance_straight_line();
    test_dominance_diamond();
    test_dominance_loop();
    test_dominance_unreachable();

    /* 2. SSA Construction Tests */
    test_ssa_construction_if_else();
    test_ssa_construction_while_loop();

    /* 3. SSA Verifier Negative Tests */
    test_ssa_verifier_negative_non_dominating_use();
    test_ssa_verifier_negative_duplicate_def();
    test_ssa_verifier_negative_phi_placement();

    /* 4. SSA Optimization Tests */
    test_ssa_optimizations();

    /* 5. SSA Destruction Tests */
    test_ssa_destruction_critical_edge();
    test_ssa_destruction_cyclic_swap();

    /* 6. End-to-End Differential Tests */
    test_ssa_e2e_differential();

    printf("All SSA unit tests passed successfully.\n");
    return 0;
}
