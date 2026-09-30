// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_dominance.h"
#include "ir_ssa.h"
#include "ir_ssa_opt.h"
#include "ir_loop.h"
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
    desugar_top_level_program(ast, *out_arena, "test_opt_ssa.cco");
    analyze_scopes(ast, *out_arena);

    IrModule *ir_mod = ir_lower_ast(ast, "test_opt_ssa.cco");
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

static int run_binary_capture(const char *cmd, char *out_buf, size_t buf_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    size_t total = 0;
    while (fgets(out_buf + total, (int)(buf_size - total), fp) != NULL) {
        total = strlen(out_buf);
        if (total >= buf_size - 1) break;
    }

    int status = pclose(fp);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

/* Sparse Conditional Constant Propagation (SCCP) tests */

static void test_sccp_branch_folding(void) {
    printf("Testing SCCP: Constant Branch Pruning & Propagation...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let x: int = 10;\n"
        "    if (1 < 2) {\n"
        "        x = 20;\n"
        "    } else {\n"
        "        x = 30;\n"
        "    }\n"
        "    print(x);\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    char *err = NULL;
    bool ssa_ok = ir_ssa_construct_module(mod, NULL, &err);
    assert(ssa_ok);

    IrFunction *fn = mod->first_fn;
    assert(fn != NULL);

    IrSsaOptMetrics metrics = {0};
    bool changed = false;
    bool sccp_ok = ir_ssa_sccp(fn, &metrics, &changed, &err);
    assert(sccp_ok);
    assert(changed);
    assert(metrics.sccp_branches_simplified >= 1);
    assert(metrics.unreachable_blocks_removed >= 1);

    /* Verify SSA invariants remain intact */
    char *v_err = NULL;
    assert(ir_ssa_verify_function(fn, NULL, &v_err));

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (constant branch simplified, unreachable block removed)\n");
}

static void test_sccp_constant_phi(void) {
    printf("Testing SCCP: Constant Phi Evaluation...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let a: int = 42;\n"
        "    let b: int = 42;\n"
        "    let res: int = 0;\n"
        "    if (1 == 1) {\n"
        "        res = a;\n"
        "    } else {\n"
        "        res = b;\n"
        "    }\n"
        "    print(res);\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    char *err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &err));

    IrFunction *fn = mod->first_fn;
    IrSsaOptMetrics metrics = {0};
    bool changed = false;
    assert(ir_ssa_sccp(fn, &metrics, &changed, &err));
    assert(changed);

    char *v_err = NULL;
    assert(ir_ssa_verify_function(fn, NULL, &v_err));

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (constant phi evaluated)\n");
}

/* Global value numbering and common subexpression elimination (GVN/CSE) */

static void test_gvn_cse_same_block(void) {
    printf("Testing GVN/CSE: Redundant Expression in Same Block...\n");
    IrModule *mod = ir_module_create("gvn_test");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "foo", i32_type);

    IrBasicBlock *entry = ir_block_create(fn, "entry");

    ir_function_add_param(fn, "a", i32_type);
    ir_function_add_param(fn, "b", i32_type);
    IrValue *v_a = fn->param_values[0];
    IrValue *v_b = fn->param_values[1];

    /* First: %r1 = add %v_a, %v_b */
    IrInstruction *add1 = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    add1->op = IR_OP_ADD;
    add1->type = i32_type;
    add1->lhs = v_a;
    add1->rhs = v_b;
    add1->result = ir_val_reg(mod->arena, i32_type, fn->next_reg_id++);
    ir_block_add_instruction(entry, add1);

    /* Second: %r2 = add %v_a, %v_b (Identical expression) */
    IrInstruction *add2 = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    add2->op = IR_OP_ADD;
    add2->type = i32_type;
    add2->lhs = v_a;
    add2->rhs = v_b;
    add2->result = ir_val_reg(mod->arena, i32_type, fn->next_reg_id++);
    ir_block_add_instruction(entry, add2);

    /* Return %r2 */
    IrInstruction *ret = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    ret->op = IR_OP_RET;
    ret->type = i32_type;
    ret->lhs = add2->result;
    ir_block_add_instruction(entry, ret);

    ir_recompute_cfg(fn);

    IrSsaOptMetrics metrics = {0};
    bool changed = false;
    char *err = NULL;
    assert(ir_ssa_gvn_cse(fn, &metrics, &changed, &err));
    assert(changed);
    assert(metrics.gvn_expressions_eliminated == 1);
    assert(metrics.cse_eliminations == 1);

    /* add2 must be eliminated, ret must now use add1->result */
    assert(entry->inst_count == 2);
    assert(ret->lhs == add1->result);

    char *v_err = NULL;
    if (!ir_ssa_verify_function(fn, NULL, &v_err)) {
        fprintf(stderr, "Verify error in GVN: %s\n", v_err ? v_err : "unknown");
        assert(false);
    }

    ir_module_free(mod);
    printf("  -> PASSED (duplicate expression in same block eliminated)\n");
}

static void test_gvn_commutative_canonicalization(void) {
    printf("Testing GVN/CSE: Commutative vs Non-Commutative Operations...\n");
    IrModule *mod = ir_module_create("gvn_comm_test");
    IrType *i32_type = ir_type_i32(mod->arena);
    IrFunction *fn = ir_function_create(mod, "comm_test", i32_type);

    IrBasicBlock *entry = ir_block_create(fn, "entry");

    ir_function_add_param(fn, "a", i32_type);
    ir_function_add_param(fn, "b", i32_type);
    IrValue *v_a = fn->param_values[0];
    IrValue *v_b = fn->param_values[1];

    /* Commutative: %add1 = add %v_a, %v_b and %add2 = add %v_b, %v_a */
    IrInstruction *add1 = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    add1->op = IR_OP_ADD;
    add1->type = i32_type;
    add1->lhs = v_a;
    add1->rhs = v_b;
    add1->result = ir_val_reg(mod->arena, i32_type, fn->next_reg_id++);
    ir_block_add_instruction(entry, add1);

    IrInstruction *add2 = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    add2->op = IR_OP_ADD;
    add2->type = i32_type;
    add2->lhs = v_b;
    add2->rhs = v_a;
    add2->result = ir_val_reg(mod->arena, i32_type, fn->next_reg_id++);
    ir_block_add_instruction(entry, add2);

    /* Non-commutative: %sub1 = sub %v_a, %v_b and %sub2 = sub %v_b, %v_a */
    IrInstruction *sub1 = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    sub1->op = IR_OP_SUB;
    sub1->type = i32_type;
    sub1->lhs = v_a;
    sub1->rhs = v_b;
    sub1->result = ir_val_reg(mod->arena, i32_type, fn->next_reg_id++);
    ir_block_add_instruction(entry, sub1);

    IrInstruction *sub2 = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    sub2->op = IR_OP_SUB;
    sub2->type = i32_type;
    sub2->lhs = v_b;
    sub2->rhs = v_a;
    sub2->result = ir_val_reg(mod->arena, i32_type, fn->next_reg_id++);
    ir_block_add_instruction(entry, sub2);

    IrInstruction *ret = (IrInstruction *)ir_arena_alloc(mod->arena, sizeof(IrInstruction));
    ret->op = IR_OP_RET;
    ret->type = i32_type;
    ret->lhs = add2->result;
    ir_block_add_instruction(entry, ret);

    ir_recompute_cfg(fn);

    IrSsaOptMetrics metrics = {0};
    bool changed = false;
    char *err = NULL;
    assert(ir_ssa_gvn_cse(fn, &metrics, &changed, &err));
    assert(changed);
    /* Exactly 1 elimination: add2 eliminated; sub1 and sub2 MUST both remain */
    assert(metrics.gvn_expressions_eliminated == 1);

    bool has_sub1 = false, has_sub2 = false;
    for (IrInstruction *i = entry->first_inst; i; i = i->next) {
        if (i == sub1) has_sub1 = true;
        if (i == sub2) has_sub2 = true;
    }
    assert(has_sub1 && has_sub2);

    ir_module_free(mod);
    printf("  -> PASSED (commutative reordered, non-commutative preserved)\n");
}

/* Natural loop analysis infrastructure tests */

static void test_loop_analysis_simple(void) {
    printf("Testing Natural Loop Analysis: Single While Loop...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let i: int = 0;\n"
        "    while (i < 5) {\n"
        "        print(i);\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrFunction *fn = mod->first_fn;
    IrDomInfo *dom = ir_dominance_compute(fn);
    assert(dom != NULL);

    IrLoopInfo *loops = ir_loop_analysis_compute(fn, dom);
    assert(loops != NULL);
    assert(loops->loop_count == 1);

    IrLoop *loop = loops->loops[0];
    assert(loop != NULL);
    assert(loop->header != NULL);
    assert(loop->latch_count == 1);
    assert(loop->depth == 1);
    assert(loop->parent == NULL);

    /* Header and latch must be in loop */
    assert(ir_loop_contains(loop, loop->header));
    assert(ir_loop_contains(loop, loop->latches[0]));

    ir_loop_info_free(loops);
    ir_dominance_free(dom);
    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (detected 1 loop, correct header/latch/depth)\n");
}

static void test_loop_analysis_nested(void) {
    printf("Testing Natural Loop Analysis: Nested While Loops...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let i: int = 0;\n"
        "    while (i < 3) {\n"
        "        let j: int = 0;\n"
        "        while (j < 4) {\n"
        "            j = j + 1;\n"
        "        }\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrFunction *fn = mod->first_fn;
    IrDomInfo *dom = ir_dominance_compute(fn);
    assert(dom != NULL);

    IrLoopInfo *loops = ir_loop_analysis_compute(fn, dom);
    assert(loops != NULL);
    assert(loops->loop_count == 2);

    /* Find inner and outer loops */
    IrLoop *outer = NULL;
    IrLoop *inner = NULL;
    for (int l = 0; l < loops->loop_count; l++) {
        if (loops->loops[l]->depth == 1) outer = loops->loops[l];
        if (loops->loops[l]->depth == 2) inner = loops->loops[l];
    }

    assert(outer != NULL);
    assert(inner != NULL);
    assert(inner->parent == outer);
    assert(outer->child_count == 1);
    assert(outer->children[0] == inner);

    ir_loop_info_free(loops);
    ir_dominance_free(dom);
    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (detected nested loops, parent-child hierarchy, depth 1 and 2)\n");
}

/* Loop-invariant code motion (LICM) tests */

static void test_licm_hoisting(void) {
    printf("Testing LICM: Hoisting Invariant Pure Arithmetic...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let i: int = 0;\n"
        "    let sum: int = 0;\n"
        "    let a: int = 10;\n"
        "    let b: int = 20;\n"
        "    while (i < 5) {\n"
        "        let inv: int = a + b;\n"
        "        sum = sum + inv;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    print(sum);\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    char *err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &err));

    IrFunction *fn = mod->first_fn;
    IrDomInfo *dom = ir_dominance_compute(fn);
    IrLoopInfo *loops = ir_loop_analysis_compute(fn, dom);

    IrSsaOptMetrics metrics = {0};
    bool changed = false;
    assert(ir_ssa_licm(fn, loops, &metrics, &changed, &err));
    assert(changed);
    assert(metrics.licm_instructions_hoisted >= 1);

    char *v_err = NULL;
    assert(ir_ssa_verify_function(fn, NULL, &v_err));

    ir_loop_info_free(loops);
    ir_dominance_free(dom);
    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (invariant arithmetic hoisted to preheader)\n");
}

/* Improved SSA dead code elimination (SSA DCE) tests */

static void test_ssa_dce_pure_chain(void) {
    printf("Testing SSA DCE: Dead Pure Arithmetic Chain Removal...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let used: int = 100;\n"
        "    let dead1: int = 50 + 25;\n"
        "    let dead2: int = dead1 * 2;\n"
        "    print(used);\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    char *err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &err));

    IrFunction *fn = mod->first_fn;
    IrSsaOptMetrics metrics = {0};
    bool changed = false;
    assert(ir_ssa_dce(fn, &metrics, &changed, &err));
    assert(changed);
    assert(metrics.dce_instructions_removed >= 2);

    char *v_err = NULL;
    assert(ir_ssa_verify_function(fn, NULL, &v_err));

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (dead instruction chain removed, print preserved)\n");
}

/* End-to-end differential testing matrix */

static void test_phase6_e2e_differential(void) {
    printf("Testing End-to-End Phase 6 Differential Execution...\n");

    const char *test_progs[] = {
        /* 1. SCCP constant branch pruning */
        "fn main() -> int {\n"
        "    let x: int = 10;\n"
        "    if (1 < 2) {\n"
        "        x = 20;\n"
        "    } else {\n"
        "        x = 30;\n"
        "    }\n"
        "    print(x);\n"
        "    return 0;\n"
        "}\n",

        /* 2. GVN redundant expression elimination */
        "fn main() -> int {\n"
        "    let a: int = 7;\n"
        "    let b: int = 8;\n"
        "    let c: int = a * b;\n"
        "    let d: int = b * a;\n"
        "    print(c + d);\n"
        "    return 0;\n"
        "}\n",

        /* 3. LICM loop invariant hoisting */
        "fn main() -> int {\n"
        "    let i: int = 0;\n"
        "    let sum: int = 0;\n"
        "    let k: int = 5 * 10;\n"
        "    while (i < 4) {\n"
        "        sum = sum + k;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    print(sum);\n"
        "    return 0;\n"
        "}\n",

        /* 4. Nested loops with computations */
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

        /* 5. Dead code elimination with conditional reassignments */
        "fn main() -> int {\n"
        "    let dead: int = 999 * 888;\n"
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
        printf("  Testing differential program %zu/%zu...\n", p + 1, prog_count);
        fflush(stdout);

        const char *src_file = "build/tmp_diff_p6.cco";
        FILE *f = fopen(src_file, "w");
        assert(f != NULL);
        fputs(test_progs[p], f);
        fclose(f);

        /* 1. Build and run reference (non-SSA, -O1) */
        char cmd_ref[512];
        snprintf(cmd_ref, sizeof(cmd_ref), "./cco %s -O1 -o build/tmp_p6_ref && ./build/tmp_p6_ref", src_file);
        char out_ref[1024] = {0};
        int code_ref = run_binary_capture(cmd_ref, out_ref, sizeof(out_ref));

        /* 2. Build and run with Phase 6 SSA-level optimizations (--ssa -O1) */
        char cmd_ssa[512];
        snprintf(cmd_ssa, sizeof(cmd_ssa), "./cco %s --ssa -O1 -o build/tmp_p6_ssa && ./build/tmp_p6_ssa", src_file);
        char out_ssa[1024] = {0};
        int code_ssa = run_binary_capture(cmd_ssa, out_ssa, sizeof(out_ssa));

        if (code_ref != code_ssa || strcmp(out_ref, out_ssa) != 0) {
            fprintf(stderr, "Diff mismatch on prog %zu: ref(code=%d, out='%s') vs ssa(code=%d, out='%s')\n",
                    p + 1, code_ref, out_ref, code_ssa, out_ssa);
            assert(false);
        }

        /* 3. Build and run with Phase 6 SSA native internal linker */
        char cmd_ssa_link[512];
        snprintf(cmd_ssa_link, sizeof(cmd_ssa_link), "./cco %s --ssa -O1 --use-internal-linker -o build/tmp_p6_link && ./build/tmp_p6_link", src_file);
        char out_ssa_link[1024] = {0};
        int code_ssa_link = run_binary_capture(cmd_ssa_link, out_ssa_link, sizeof(out_ssa_link));

        if (code_ref != code_ssa_link || strcmp(out_ref, out_ssa_link) != 0) {
            fprintf(stderr, "Diff mismatch on prog %zu: ref(code=%d, out='%s') vs ssa_link(code=%d, out='%s')\n",
                    p + 1, code_ref, out_ref, code_ssa_link, out_ssa_link);
            assert(false);
        }
    }

    unlink("build/tmp_diff_p6.cco");
    unlink("build/tmp_p6_ref");
    unlink("build/tmp_p6_ssa");
    unlink("build/tmp_p6_link");
    printf("  -> PASSED (100%% differential match between non-SSA, SSA -O1, and internal linker)\n");
}

/* Main test runner */

int main(void) {
    printf("Running SSA Optimization Unit Tests...\n");

    /* 1. SCCP Tests */
    test_sccp_branch_folding();
    test_sccp_constant_phi();

    /* 2. GVN/CSE Tests */
    test_gvn_cse_same_block();
    test_gvn_commutative_canonicalization();

    /* 3. Loop Analysis Tests */
    test_loop_analysis_simple();
    test_loop_analysis_nested();

    /* 4. LICM Tests */
    test_licm_hoisting();

    /* 5. SSA DCE Tests */
    test_ssa_dce_pure_chain();

    /* 6. End-to-End Differential Tests */
    test_phase6_e2e_differential();

    printf("All SSA optimization tests passed.\n");
    return 0;
}
