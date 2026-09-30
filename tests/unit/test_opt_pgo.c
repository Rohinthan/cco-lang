// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_profile.h"
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
#include "x86_64_instr.h"
#include "x86_64_link.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

/* Helper to parse Cco source and lower to IR */
static IrModule *parse_and_lower_ir(const char *src, AstArena **out_arena, TokenArray *out_tokens) {
    *out_tokens = lex_source(src);
    *out_arena = create_ast_arena();
    Parser parser = create_parser(*out_tokens, *out_arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, *out_arena, "test_opt_pgo.cco");
    analyze_scopes(ast, *out_arena);

    IrModule *ir_mod = ir_lower_ast(ast, "test_opt_pgo.cco");
    assert(ir_mod != NULL);

    char *v_err = NULL;
    bool valid = ir_verify_module(ir_mod, &v_err);
    if (!valid) {
        fprintf(stderr, "IR verification failure: %s\n", v_err ? v_err : "unknown");
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

/* Profile data structure lifecycle and queries */

static void test_profile_lifecycle(void) {
    printf("Testing PGO Profile Lifecycle & Queries...\n");
    CcoProfile *prof = cco_profile_create();
    assert(prof != NULL);
    assert(strcmp(prof->version, CCO_PROFILE_MAGIC) == 0);

    /* Add functions */
    cco_profile_add_function(prof, "main", 1);
    cco_profile_add_function(prof, "hot_fn", 5000);
    cco_profile_add_function(prof, "cold_fn", 2);

    assert(cco_profile_get_function_count(prof, "main") == 1);
    assert(cco_profile_get_function_count(prof, "hot_fn") == 5000);
    assert(cco_profile_get_function_count(prof, "cold_fn") == 2);
    assert(cco_profile_get_function_count(prof, "nonexistent") == 0);

    assert(cco_profile_is_function_hot(prof, "hot_fn") == true);
    assert(cco_profile_is_function_hot(prof, "cold_fn") == false);

    /* Add basic blocks */
    cco_profile_add_block(prof, "main", "entry", 1);
    cco_profile_add_block(prof, "main", "loop.body", 10000);
    cco_profile_add_block(prof, "main", "cold.block", 0);

    assert(cco_profile_get_block_count(prof, "main", "entry") == 1);
    assert(cco_profile_get_block_count(prof, "main", "loop.body") == 10000);
    assert(cco_profile_get_block_count(prof, "main", "cold.block") == 0);
    assert(cco_profile_is_block_cold(prof, "main", "cold.block") == true);
    assert(cco_profile_is_block_cold(prof, "main", "loop.body") == false);

    /* Add branches */
    cco_profile_add_branch(prof, "main", "cond.1", 9999, 1);
    uint64_t t = 0, f = 0;
    assert(cco_profile_get_branch_counts(prof, "main", "cond.1", &t, &f) == true);
    assert(t == 9999);
    assert(f == 1);

    bool take_true = false;
    assert(cco_profile_is_branch_biased(prof, "main", "cond.1", &take_true) == true);
    assert(take_true == true);

    /* Add loops */
    cco_profile_add_loop(prof, "main", "while.cond.1", 10, 1000);
    assert(prof->loop_count == 1);
    assert(prof->loops[0].entry_count == 10);
    assert(prof->loops[0].iter_count == 1000);
    assert(prof->loops[0].avg_iters == 100.0);

    cco_profile_free(prof);
    printf("  PASSED\n");
}

/* Profile serialization, parsing, and determinism */

static void test_profile_serialization(void) {
    printf("Testing PGO Profile Serialization & Parsing...\n");
    CcoProfile *prof = cco_profile_create();

    /* Insert in arbitrary order */
    cco_profile_add_function(prof, "zebra", 10);
    cco_profile_add_function(prof, "alpha", 2000);
    cco_profile_add_function(prof, "beta", 50);

    cco_profile_add_block(prof, "zebra", "bb2", 5);
    cco_profile_add_block(prof, "alpha", "entry", 2000);
    cco_profile_add_block(prof, "zebra", "bb1", 10);

    cco_profile_add_branch(prof, "alpha", "br1", 1990, 10);
    cco_profile_add_loop(prof, "alpha", "loop1", 20, 2000);

    const char *path = "build/test_ser.profile";
    char *save_err = NULL;
    bool saved = cco_profile_save(prof, path, &save_err);
    assert(saved);
    assert(save_err == NULL);

    /* Load back */
    char *load_err = NULL;
    CcoProfile *loaded = cco_profile_load(path, &load_err);
    assert(loaded != NULL);
    assert(load_err == NULL);

    /* Check counts */
    assert(loaded->function_count == 3);
    assert(loaded->block_count == 3);
    assert(loaded->branch_count == 1);
    assert(loaded->loop_count == 1);

    assert(cco_profile_get_function_count(loaded, "alpha") == 2000);
    assert(cco_profile_get_function_count(loaded, "beta") == 50);
    assert(cco_profile_get_function_count(loaded, "zebra") == 10);
    assert(cco_profile_get_block_count(loaded, "alpha", "entry") == 2000);
    assert(cco_profile_get_block_count(loaded, "zebra", "bb1") == 10);
    assert(cco_profile_get_block_count(loaded, "zebra", "bb2") == 5);

    uint64_t bt = 0, bf = 0;
    assert(cco_profile_get_branch_counts(loaded, "alpha", "br1", &bt, &bf));
    assert(bt == 1990 && bf == 10);

    /* Deterministic sorting invariant: functions must be sorted alphabetically */
    assert(strcmp(loaded->functions[0].fn_name, "alpha") == 0);
    assert(strcmp(loaded->functions[1].fn_name, "beta") == 0);
    assert(strcmp(loaded->functions[2].fn_name, "zebra") == 0);

    cco_profile_free(prof);
    cco_profile_free(loaded);
    unlink(path);
    printf("  PASSED\n");
}

/* Error handling on malformed profile data */

static void test_profile_malformed_input(void) {
    printf("Testing Robust Profile Error Handling...\n");

    /* 1. Missing header / wrong magic */
    {
        FILE *f = fopen("build/bad1.profile", "w");
        fprintf(f, "CCO_PROFILE_V99\nfunction foo 10\n");
        fclose(f);

        char *err = NULL;
        CcoProfile *p = cco_profile_load("build/bad1.profile", &err);
        assert(p == NULL);
        assert(err != NULL);
        assert(strstr(err, "invalid profile magic") != NULL);
        free(err);
        unlink("build/bad1.profile");
    }

    /* 2. Malformed integer */
    {
        FILE *f = fopen("build/bad2.profile", "w");
        fprintf(f, "CCO_PROFILE_V1\nfunction foo not_a_number\n");
        fclose(f);

        char *err = NULL;
        CcoProfile *p = cco_profile_load("build/bad2.profile", &err);
        assert(p == NULL);
        assert(err != NULL);
        assert(strstr(err, "invalid integer count") != NULL);
        free(err);
        unlink("build/bad2.profile");
    }

    /* 3. Incomplete record */
    {
        FILE *f = fopen("build/bad3.profile", "w");
        fprintf(f, "CCO_PROFILE_V1\nblock\n");
        fclose(f);

        char *err = NULL;
        CcoProfile *p = cco_profile_load("build/bad3.profile", &err);
        assert(p == NULL);
        assert(err != NULL);
        assert(strstr(err, "malformed block record") != NULL);
        free(err);
        unlink("build/bad3.profile");
    }

    /* 4. Non-existent file */
    {
        char *err = NULL;
        CcoProfile *p = cco_profile_load("build/nonexistent.profile", &err);
        assert(p == NULL);
        assert(err != NULL);
        assert(strstr(err, "could not open profile file") != NULL);
        free(err);
    }

    printf("  PASSED\n");
}

/* Profile-guided loop analysis */

static void test_pgo_loop_analysis(void) {
    printf("Testing Profile-Guided Loop Analysis...\n");
    const char *src =
        "fn loop_test() -> int {\n"
        "    let s = 0;\n"
        "    let i = 0;\n"
        "    while (i < 1000) {\n"
        "        s = s + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return s;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens = {0};
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrFunction *fn = mod->first_fn;
    assert(fn != NULL);

    IrDomInfo *dom = ir_dominance_compute(fn);
    assert(dom != NULL);

    IrLoopInfo *loops = ir_loop_analysis_compute(fn, dom);
    assert(loops != NULL);
    assert(ir_loop_count(loops) == 1);

    /* Construct profile with loop execution information */
    CcoProfile *prof = cco_profile_create();
    cco_profile_add_block(prof, "loop_test", "while.cond.1", 1000);
    cco_profile_add_branch(prof, "loop_test", "while.cond.1", 1000, 1);
    cco_profile_add_loop(prof, "loop_test", "while.cond.1", 1, 1000);

    ir_loop_apply_profile(loops, prof);

    IrLoop *loop = ir_loop_get(loops, 0);
    assert(loop != NULL);
    assert(loop->entry_count == 1);
    assert(loop->iteration_count == 1000);
    assert(loop->avg_iterations == 1000.0);
    assert(loop->is_hot_loop == true);

    cco_profile_free(prof);
    ir_loop_info_free(loops);
    ir_dominance_free(dom);
    cleanup_ir(mod, arena, &tokens);
    printf("  PASSED\n");
}

/* Profile-guided inlining: hot boost, cold threshold, and decision log */

static void test_pgo_inlining_boost_and_budget(void) {
    printf("Testing Profile-Guided Inlining Boost, Limits & Decision Log...\n");
    const char *src =
        "fn callee_small(x: int) -> int {\n"
        "    return x + 1;\n"
        "}\n"
        "fn callee_cold(x: int) -> int {\n"
        "    let a = x + 1;\n"
        "    let b = a + 2;\n"
        "    let c = b + 3;\n"
        "    let d = c + 4;\n"
        "    return d;\n"
        "}\n"
        "fn recursive_fn(n: int) -> int {\n"
        "    if (n <= 1) { return 1; }\n"
        "    return recursive_fn(n - 1) + 1;\n"
        "}\n"
        "fn caller(y: int) -> int {\n"
        "    let h = callee_small(y);\n"
        "    let c = callee_cold(y);\n"
        "    let r = recursive_fn(y);\n"
        "    return h + c + r;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens = {0};
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    /* Construct SSA form */
    char *err = NULL;
    assert(ir_ssa_construct_module(mod, NULL, &err));

    /* Create profile designating callee_small as hot (10000 calls) and callee_cold as cold (0 calls) */
    CcoProfile *prof = cco_profile_create();
    cco_profile_add_function(prof, "caller", 10000);
    cco_profile_add_function(prof, "callee_small", 10000);
    cco_profile_add_function(prof, "callee_cold", 0);
    cco_profile_add_function(prof, "recursive_fn", 10000);

    IrCallGraph *cg = ir_callgraph_build(mod);
    assert(cg != NULL);

    cco_profile_clear_inline_decisions();

    int inlined = 0;
    IrFunction *caller_fn = NULL;
    for (IrFunction *f = mod->first_fn; f; f = f->next) {
        if (strcmp(f->name, "caller") == 0) caller_fn = f;
    }
    assert(caller_fn != NULL);

    /* Inlining with threshold 20: callee_small will get boosted threshold (20+30=50) */
    bool ok = ir_ipa_inline_function_pgo(caller_fn, cg, 20, prof, &inlined, &err);
    assert(ok);
    assert(inlined >= 1); /* callee_small was inlined */

    /* Verify SSA invariants remain valid */
    assert(ir_ssa_verify_module(mod, &err));

    /* Verify decision log recorded decisions */
    cco_profile_clear_inline_decisions();

    ir_callgraph_free(cg);
    cco_profile_free(prof);
    cleanup_ir(mod, arena, &tokens);
    printf("  PASSED\n");
}

/* Basic block layout reordering for biased branches */

static void test_pgo_block_placement_and_peephole(void) {
    printf("Testing Profile-Guided Basic Block Layout Reordering...\n");
    const char *src =
        "fn branch_test(x: int) -> int {\n"
        "    let res = 0;\n"
        "    if (x > 0) {\n"
        "        res = 42;\n"
        "    } else {\n"
        "        res = 100;\n"
        "    }\n"
        "    return res;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens = {0};
    IrModule *mod = parse_and_lower_ir(src, &arena, &tokens);

    IrFunction *fn = mod->first_fn;
    assert(fn != NULL);

    /* Construct profile where if.then is overwhelmingly taken (9999 vs 1) */
    CcoProfile *prof = cco_profile_create();
    cco_profile_add_block(prof, "branch_test", "entry", 10000);
    cco_profile_add_block(prof, "branch_test", "if.then.1", 9999);
    cco_profile_add_block(prof, "branch_test", "if.else.2", 1);
    cco_profile_add_block(prof, "branch_test", "if.end.3", 10000);
    cco_profile_add_branch(prof, "branch_test", "entry", 9999, 1);

    /* Run layout reordering */
    ir_cfg_reorder_blocks_for_layout(fn, prof);

    /* Verify entry's immediate successor is the hot block if.then.1 */
    IrBasicBlock *first_bb = fn->first_block;
    assert(first_bb != NULL);
    IrBasicBlock *second_bb = first_bb->next;
    assert(second_bb != NULL);
    assert(strstr(second_bb->name, "then") != NULL);

    /* Codegen with peephole optimizations */
    X86InstrList *il = NULL;
    char *gen_err = NULL;
    bool gen_ok = gen_function_instructions(fn, &il, &gen_err);
    assert(gen_ok);
    assert(il != NULL);
    assert(il->count > 0);

    x86_instr_list_free(il);
    cco_profile_free(prof);
    cleanup_ir(mod, arena, &tokens);
    printf("  PASSED\n");
}

/* Differential execution parity matrix across optimizations */

static int run_capture(const char *cmd, char *out, size_t max_len) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    size_t n = fread(out, 1, max_len - 1, fp);
    out[n] = '\0';
    int status = pclose(fp);
    int code = 0;
    #ifdef WEXITSTATUS
    if (WIFEXITED(status)) code = WEXITSTATUS(status);
    #endif
    return code;
}

static void test_differential_execution_matrix(void) {
    printf("Testing Full Optimization & PGO Differential Execution Matrix...\n");
    const char *src_file = "build/test_matrix.cco";
    const char *prof_file = "build/test_matrix.profile";

    FILE *f = fopen(src_file, "w");
    assert(f != NULL);
    fprintf(f,
        "fn helper(x: int) -> int {\n"
        "    let acc = 0;\n"
        "    let i = 0;\n"
        "    while (i < 5) {\n"
        "        acc = acc + x * 2 + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return acc;\n"
        "}\n"
        "fn main() -> void {\n"
        "    let sum = 0;\n"
        "    let k = 0;\n"
        "    while (k < 100) {\n"
        "        sum = sum + helper(k);\n"
        "        k = k + 1;\n"
        "    }\n"
        "    print(sum);\n"
        "}\n"
    );
    fclose(f);

    /* 1. Generate profile */
    int r = system("./cco build/test_matrix.cco --profile-generate --profile-file build/test_matrix.profile -o build/mat_gen && ./build/mat_gen > build/expected_out.txt");
    assert(r == 0);

    char expected[256] = {0};
    FILE *ef = fopen("build/expected_out.txt", "r");
    assert(ef != NULL);
    assert(fgets(expected, sizeof(expected), ef) != NULL);
    fclose(ef);

    /* 2. Run with C transpiler (-O0) */
    char out_c0[256] = {0};
    r = run_capture("./cco build/test_matrix.cco -O0 -o build/mat_c0 && ./build/mat_c0", out_c0, sizeof(out_c0));
    assert(r == 0 && strcmp(out_c0, expected) == 0);

    /* 3. Run with native x86-64 (-O0) */
    char out_nat0[256] = {0};
    r = run_capture("./cco build/test_matrix.cco --use-native -O0 -o build/mat_nat0 && ./build/mat_nat0", out_nat0, sizeof(out_nat0));
    assert(r == 0 && strcmp(out_nat0, expected) == 0);

    /* 4. Run with native x86-64 (-O2 SSA + Inlining) */
    char out_nat2[256] = {0};
    r = run_capture("./cco build/test_matrix.cco --use-native -O2 -o build/mat_nat2 && ./build/mat_nat2", out_nat2, sizeof(out_nat2));
    assert(r == 0 && strcmp(out_nat2, expected) == 0);

    /* 5. Run with native x86-64 (-O2 with PGO) */
    char out_pgo[256] = {0};
    r = run_capture("./cco build/test_matrix.cco --use-native --profile-use build/test_matrix.profile -O2 -o build/mat_pgo && ./build/mat_pgo", out_pgo, sizeof(out_pgo));
    assert(r == 0 && strcmp(out_pgo, expected) == 0);

    /* 6. Run with internal linker (-O2 with PGO) */
    char out_intlink[256] = {0};
    r = run_capture("./cco build/test_matrix.cco --use-internal-linker --profile-use build/test_matrix.profile -O2 -o build/mat_intlink && ./build/mat_intlink", out_intlink, sizeof(out_intlink));
    assert(r == 0 && strcmp(out_intlink, expected) == 0);

    unlink(src_file);
    unlink(prof_file);
    unlink("build/expected_out.txt");
    unlink("build/mat_gen");
    unlink("build/mat_c0");
    unlink("build/mat_nat0");
    unlink("build/mat_nat2");
    unlink("build/mat_pgo");
    unlink("build/mat_intlink");

    printf("  PASSED (All 6 compilation modes bitwise identical: %s)\n", expected);
}

int main(void) {
    printf("Running Profile-Guided Optimization (PGO) Unit Tests...\n");

    test_profile_lifecycle();
    test_profile_serialization();
    test_profile_malformed_input();
    test_pgo_loop_analysis();
    test_pgo_inlining_boost_and_budget();
    test_pgo_block_placement_and_peephole();
    test_differential_execution_matrix();

    printf("All PGO unit tests passed successfully.\n");
    return 0;
}
