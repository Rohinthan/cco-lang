// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_dominance.h"
#include "ir_ssa.h"
#include "ir_ssa_opt.h"
#include "ir_ipa.h"
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
    desugar_top_level_program(ast, *out_arena, "test_opt_ipa.cco");
    analyze_scopes(ast, *out_arena);

    IrModule *ir_mod = ir_lower_ast(ast, "test_opt_ipa.cco");
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
        if (total + 1 >= buf_size) break;
    }
    int status = pclose(fp);
    int exit_code = 0;
    #ifdef WEXITSTATUS
    if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
    #endif
    return exit_code;
}

/* 1. Call Graph Analysis Tests */

static void test_callgraph_simple(void) {
    printf("Testing Call Graph: Simple Chain (main -> foo -> bar)...\n");
    const char *src =
        "fn bar() -> int { return 42; }\n"
        "fn foo() -> int { return bar() + 1; }\n"
        "fn main() -> int { print(foo()); return 0; }\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrCallGraph *cg = ir_callgraph_build(mod);
    assert(cg != NULL);
    assert(cg->node_count == 3);
    assert(cg->entry_node != NULL);
    assert(strcmp(cg->entry_node->fn->name, "main") == 0);

    IrCallGraphNode *n_main = ir_callgraph_find_node(cg, "main");
    IrCallGraphNode *n_foo = ir_callgraph_find_node(cg, "foo");
    IrCallGraphNode *n_bar = ir_callgraph_find_node(cg, "bar");

    assert(n_main && n_foo && n_bar);
    assert(n_main->is_entry == true);
    assert(n_main->caller_count == 0);
    assert(n_main->call_count == 1);

    assert(n_foo->caller_count == 1);
    assert(n_foo->call_count == 1);
    assert(n_foo->is_recursive == false);

    assert(n_bar->caller_count == 1);
    assert(n_bar->call_count == 0);
    assert(n_bar->is_recursive == false);
    assert(n_bar->can_inline == true);

    ir_callgraph_free(cg);
    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED\n");
}

static void test_callgraph_recursion(void) {
    printf("Testing Call Graph: Self-Recursion Detection...\n");
    const char *src =
        "fn fact(n: int) -> int {\n"
        "    if (n <= 1) { return 1; }\n"
        "    return n * fact(n - 1);\n"
        "}\n"
        "fn main() -> int { print(fact(5)); return 0; }\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrCallGraph *cg = ir_callgraph_build(mod);
    assert(cg != NULL);

    IrCallGraphNode *n_fact = ir_callgraph_find_node(cg, "fact");
    assert(n_fact != NULL);
    assert(n_fact->is_recursive == true);
    assert(n_fact->in_recursive_cycle == true);
    assert(n_fact->can_inline == false); /* Must never inline recursive function */

    ir_callgraph_free(cg);
    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (self-recursion detected, inlining disabled)\n");
}

static void test_callgraph_mutual_recursion(void) {
    printf("Testing Call Graph: Mutual Recursion Cycle Detection...\n");
    const char *src =
        "fn is_odd(n: int) -> bool {\n"
        "    if (n == 0) { return false; }\n"
        "    return is_even(n - 1);\n"
        "}\n"
        "fn is_even(n: int) -> bool {\n"
        "    if (n == 0) { return true; }\n"
        "    return is_odd(n - 1);\n"
        "}\n"
        "fn main() -> int { print(is_even(10)); return 0; }\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrCallGraph *cg = ir_callgraph_build(mod);
    assert(cg != NULL);

    IrCallGraphNode *n_even = ir_callgraph_find_node(cg, "is_even");
    IrCallGraphNode *n_odd = ir_callgraph_find_node(cg, "is_odd");
    assert(n_even != NULL && n_odd != NULL);

    assert(n_even->in_recursive_cycle == true);
    assert(n_odd->in_recursive_cycle == true);
    assert(n_even->can_inline == false);
    assert(n_odd->can_inline == false);

    ir_callgraph_free(cg);
    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (mutual recursion cycle detected via SCC)\n");
}

/* 2. Function Purity Analysis Tests */

static void test_purity_analysis(void) {
    printf("Testing Function Purity Classification...\n");
    const char *src =
        "fn pure_add(a: int, b: int) -> int { return a + b; }\n"
        "fn side_print(x: int) -> void { print(x); }\n"
        "fn main() -> int {\n"
        "    let x: int = pure_add(10, 20);\n"
        "    side_print(x);\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    /* Construct SSA so stack allocas are eliminated into registers */
    char *ssa_err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &ssa_err));

    IrCallGraph *cg = ir_callgraph_build(mod);
    assert(cg != NULL);

    IrCallGraphNode *n_add = ir_callgraph_find_node(cg, "pure_add");
    IrCallGraphNode *n_print = ir_callgraph_find_node(cg, "side_print");
    IrCallGraphNode *n_main = ir_callgraph_find_node(cg, "main");

    assert(n_add && n_print && n_main);
    assert(n_add->purity == IR_PURITY_PURE);
    assert(n_print->purity == IR_PURITY_SIDE_EFFECTING);
    assert(n_main->purity == IR_PURITY_SIDE_EFFECTING);

    ir_callgraph_free(cg);
    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (PURE vs SIDE_EFFECTING proved)\n");
}

/* 3. Inlining and Constant Propagation Tests */

static void test_inline_simple(void) {
    printf("Testing Inlining: Simple Function...\n");
    const char *src =
        "fn double_val(x: int) -> int { return x + x; }\n"
        "fn main() -> int {\n"
        "    let res: int = double_val(21);\n"
        "    print(res);\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    char *ssa_err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &ssa_err));

    int inlined = 0;
    char *inl_err = NULL;
    bool ok = ir_ipa_inline_module(mod, IR_DEFAULT_INLINE_THRESHOLD, &inlined, &inl_err);
    if (!ok) {
        fprintf(stderr, "Inline error: %s\n", inl_err ? inl_err : "unknown");
    }
    assert(ok);
    assert(inlined >= 1);

    /* Verify call site was replaced in main */
    IrFunction *fn_main = ir_module_find_function(mod, "main");
    assert(fn_main != NULL);
    for (IrBasicBlock *bb = fn_main->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            assert(inst->op != IR_OP_CALL); /* Call should be eliminated */
        }
    }

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED\n");
}

static void test_inline_constant_propagation(void) {
    printf("Testing Inlining: Interprocedural Constant Folding (square(5) -> 25)...\n");
    const char *src =
        "fn square(x: int) -> int { return x * x; }\n"
        "fn main() -> int {\n"
        "    print(square(5));\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    char *ssa_err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &ssa_err));

    /* Full Phase 7 SSA optimization */
    IrIpaOptions ipa_opts = {
        .inline_threshold = IR_DEFAULT_INLINE_THRESHOLD,
        .enable_inlining = true,
        .enable_dfe = true,
        .verbose = false
    };
    IrIpaMetrics metrics;
    char *opt_err = NULL;
    bool ok = ir_ipa_optimize_module(mod, &ipa_opts, &metrics, &opt_err);
    if (!ok) {
        fprintf(stderr, "IPA opt error: %s\n", opt_err ? opt_err : "unknown");
    }
    assert(ok);
    assert(metrics.inlined_call_sites >= 1);
    assert(metrics.dead_functions_removed >= 1); /* square was eliminated */

    /* After inlining and SCCP, main should directly print 25 */
    IrFunction *fn_main = ir_module_find_function(mod, "main");
    assert(fn_main != NULL);
    bool found_const_25 = false;
    for (IrBasicBlock *bb = fn_main->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_PRINT && inst->lhs && inst->lhs->kind == IR_VAL_CONST_INT) {
                if (inst->lhs->const_val.int_val == 25) {
                    found_const_25 = true;
                }
            }
        }
    }
    assert(found_const_25);

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (constant 25 folded directly into print)\n");
}

static void test_inline_multi_return(void) {
    printf("Testing Inlining: Multi-Return Function with Phi Synthesis...\n");
    const char *src =
        "fn max_val(a: int, b: int) -> int {\n"
        "    if (a > b) { return a; }\n"
        "    return b;\n"
        "}\n"
        "fn main() -> int {\n"
        "    print(max_val(50, 20));\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    char *ssa_err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &ssa_err));

    IrIpaOptions ipa_opts = {
        .inline_threshold = IR_DEFAULT_INLINE_THRESHOLD,
        .enable_inlining = true,
        .enable_dfe = true,
        .verbose = false
    };
    char *opt_err = NULL;
    assert(ir_ipa_optimize_module(mod, &ipa_opts, NULL, &opt_err));

    /* After inlining and SCCP constant branch pruning, main prints 50 */
    IrFunction *fn_main = ir_module_find_function(mod, "main");
    assert(fn_main != NULL);
    bool found_50 = false;
    for (IrBasicBlock *bb = fn_main->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_PRINT && inst->lhs && inst->lhs->kind == IR_VAL_CONST_INT) {
                if (inst->lhs->const_val.int_val == 50) {
                    found_50 = true;
                }
            }
        }
    }
    assert(found_50);

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (multi-return phi synthesized and constant 50 folded)\n");
}

static void test_dead_function_elimination(void) {
    printf("Testing Dead Function Elimination: Pruning Unreachable Functions...\n");
    const char *src =
        "fn unused_dead1() -> int { return 111; }\n"
        "fn unused_dead2() -> int { return unused_dead1() + 222; }\n"
        "fn main() -> int {\n"
        "    print(42);\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    assert(mod->fn_count == 3);

    int removed = 0;
    char *err = NULL;
    assert(ir_ipa_eliminate_dead_functions(mod, NULL, &removed, &err));
    assert(removed == 2);
    assert(mod->fn_count == 1);
    assert(ir_module_find_function(mod, "main") != NULL);
    assert(ir_module_find_function(mod, "unused_dead1") == NULL);
    assert(ir_module_find_function(mod, "unused_dead2") == NULL);

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED (2 dead functions eliminated, entry preserved)\n");
}

/* 4. Code Quality Metrics Collection Test */

static void test_code_stats_collection(void) {
    printf("Testing Native Code Quality Metrics Collection...\n");
    const char *src =
        "fn compute(a: int, b: int) -> int {\n"
        "    let x: int = a * 2;\n"
        "    let y: int = b * 3;\n"
        "    return x + y;\n"
        "}\n"
        "fn main() -> int {\n"
        "    print(compute(10, 20));\n"
        "    return 0;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    CcoCodeStats stats;
    cco_code_stats_collect(mod, &stats);

    assert(stats.function_count == 2);
    assert(stats.post_ssa_inst_count > 0);
    assert(stats.call_count >= 1);
    assert(stats.virtual_regs_count > 0);
    assert(stats.int_regs_used > 0);

    cleanup_ir(mod, arena, &tokens);
    printf("  -> PASSED\n");
}

/* 5. End-to-End Differential Execution Matrix */

static void test_e2e_differential(void) {
    printf("Testing End-to-End Phase 7 Differential Execution Matrix...\n");

    const char *test_progs[] = {
        /* 1. inline_simple.cco */
        "fn add_five(x: int) -> int { return x + 5; }\n"
        "fn main() -> int {\n"
        "    let a: int = add_five(10);\n"
        "    let b: int = add_five(20);\n"
        "    print(a + b);\n"
        "    return 0;\n"
        "}\n",

        /* 2. inline_multi_return.cco */
        "fn clamp(v: int, lo: int, hi: int) -> int {\n"
        "    if (v < lo) { return lo; }\n"
        "    if (v > hi) { return hi; }\n"
        "    return v;\n"
        "}\n"
        "fn main() -> int {\n"
        "    print(clamp(5, 10, 20));\n"
        "    print(clamp(15, 10, 20));\n"
        "    print(clamp(25, 10, 20));\n"
        "    return 0;\n"
        "}\n",

        /* 3. inline_nested.cco */
        "fn step1(x: int) -> int { return x + 1; }\n"
        "fn step2(x: int) -> int { return step1(x) * 2; }\n"
        "fn step3(x: int) -> int { return step2(x) - 3; }\n"
        "fn main() -> int {\n"
        "    print(step3(10));\n"
        "    return 0;\n"
        "}\n",

        /* 4. inline_constant.cco */
        "fn mul_const(x: int) -> int { return x * 7; }\n"
        "fn main() -> int {\n"
        "    let res: int = mul_const(6);\n"
        "    print(res);\n"
        "    return 0;\n"
        "}\n",

        /* 5. recursive_no_inline.cco */
        "fn fib(n: int) -> int {\n"
        "    if (n <= 1) { return n; }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "fn main() -> int {\n"
        "    print(fib(8));\n"
        "    return 0;\n"
        "}\n",

        /* 6. dead_function.cco */
        "fn dead_calc(x: int) -> int { return x * 100; }\n"
        "fn alive_calc(x: int) -> int { return x + 10; }\n"
        "fn main() -> int {\n"
        "    print(alive_calc(5));\n"
        "    return 0;\n"
        "}\n",

        /* 7. call_stack_args.cco (>6 integer args testing stack passing and 16-byte alignment) */
        "fn sum8(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int) -> int {\n"
        "    return a + b + c + d + e + f + g + h;\n"
        "}\n"
        "fn main() -> int {\n"
        "    print(sum8(1, 2, 3, 4, 5, 6, 7, 8));\n"
        "    return 0;\n"
        "}\n",

        /* 8. mixed_abi_optimized.cco */
        "fn calc_mixed(count: int, rate: float, factor: float) -> float {\n"
        "    let base: float = rate * factor;\n"
        "    return base;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let r: float = calc_mixed(5, 2.5, 4.0);\n"
        "    if (r > 9.0) {\n"
        "        print(1);\n"
        "    } else {\n"
        "        print(0);\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
    };

    size_t prog_count = sizeof(test_progs) / sizeof(test_progs[0]);

    for (size_t p = 0; p < prog_count; p++) {
        printf("  Testing differential program %zu/%zu...\n", p + 1, prog_count);
        fflush(stdout);

        const char *src_file = "build/tmp_diff_p7.cco";
        FILE *f = fopen(src_file, "w");
        assert(f != NULL);
        fputs(test_progs[p], f);
        fclose(f);

        /* 1. Reference C11 (-O0) */
        char cmd_ref[512];
        snprintf(cmd_ref, sizeof(cmd_ref), "./cco %s -O0 -o build/tmp_p7_ref && ./build/tmp_p7_ref", src_file);
        char out_ref[1024] = {0};
        int code_ref = run_binary_capture(cmd_ref, out_ref, sizeof(out_ref));

        /* 2. Cco Phase 6 (-O1) */
        char cmd_o1[512];
        snprintf(cmd_o1, sizeof(cmd_o1), "./cco %s -O1 -o build/tmp_p7_o1 && ./build/tmp_p7_o1", src_file);
        char out_o1[1024] = {0};
        int code_o1 = run_binary_capture(cmd_o1, out_o1, sizeof(out_o1));

        if (code_ref != code_o1 || strcmp(out_ref, out_o1) != 0) {
            fprintf(stderr, "Diff mismatch on prog %zu: ref(code=%d, out='%s') vs -O1(code=%d, out='%s')\n",
                    p + 1, code_ref, out_ref, code_o1, out_o1);
            assert(false);
        }

        /* 3. Cco Phase 7 (-O2: Inlining + Interprocedural Optimization) */
        char cmd_o2[512];
        snprintf(cmd_o2, sizeof(cmd_o2), "./cco %s -O2 -o build/tmp_p7_o2 && ./build/tmp_p7_o2", src_file);
        char out_o2[1024] = {0};
        int code_o2 = run_binary_capture(cmd_o2, out_o2, sizeof(out_o2));

        if (code_ref != code_o2 || strcmp(out_ref, out_o2) != 0) {
            fprintf(stderr, "Diff mismatch on prog %zu: ref(code=%d, out='%s') vs -O2(code=%d, out='%s')\n",
                    p + 1, code_ref, out_ref, code_o2, out_o2);
            assert(false);
        }

        /* 4. Cco Phase 7 (-O2 Native Internal Linker) */
        char cmd_o2_link[512];
        snprintf(cmd_o2_link, sizeof(cmd_o2_link), "./cco %s -O2 --use-internal-linker -o build/tmp_p7_link && ./build/tmp_p7_link", src_file);
        char out_o2_link[1024] = {0};
        int code_o2_link = run_binary_capture(cmd_o2_link, out_o2_link, sizeof(out_o2_link));

        if (code_ref != code_o2_link || strcmp(out_ref, out_o2_link) != 0) {
            fprintf(stderr, "Diff mismatch on prog %zu: ref(code=%d, out='%s') vs -O2-link(code=%d, out='%s')\n",
                    p + 1, code_ref, out_ref, code_o2_link, out_o2_link);
            assert(false);
        }
    }

    unlink("build/tmp_diff_p7.cco");
    unlink("build/tmp_p7_ref");
    unlink("build/tmp_p7_o1");
    unlink("build/tmp_p7_o2");
    unlink("build/tmp_p7_link");
    printf("  -> PASSED (100%% differential match between -O0, -O1, -O2, and Internal Linker)\n");
}

/* Main Runner */

int main(void) {
    printf("Running Interprocedural Analysis & Inlining Tests...\n");

    test_callgraph_simple();
    test_callgraph_recursion();
    test_callgraph_mutual_recursion();
    test_purity_analysis();
    test_inline_simple();
    test_inline_constant_propagation();
    test_inline_multi_return();
    test_dead_function_elimination();
    test_code_stats_collection();
    test_e2e_differential();

    printf("All Interprocedural tests passed.\n");
    return 0;
}
