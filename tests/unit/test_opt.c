// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
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
    desugar_top_level_program(ast, *out_arena, "test_opt_input.cco");
    analyze_scopes(ast, *out_arena);

    IrModule *ir_mod = ir_lower_ast(ast, "test_opt_input.cco");
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

/* Helper to count total instructions in a function */
static int count_fn_instructions(IrFunction *fn) {
    int count = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        count += bb->inst_count;
    }
    return count;
}

/* Helper to execute a binary and capture exit code and stdout */
static int run_binary_and_capture(const char *bin_path, char *out_buf, size_t out_buf_size) {
    char out_path[256];
    snprintf(out_path, sizeof(out_path), "%s_out.txt", bin_path);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "./\"%s\" > \"%s\" 2>&1", bin_path, out_path);
    int res = system(cmd);
    int exit_code = 0;
    #ifdef WEXITSTATUS
    if (WIFEXITED(res)) exit_code = WEXITSTATUS(res);
    #endif

    FILE *f = fopen(out_path, "r");
    if (f) {
        size_t n = fread(out_buf, 1, out_buf_size - 1, f);
        out_buf[n] = '\0';
        fclose(f);
        unlink(out_path);
    } else {
        out_buf[0] = '\0';
    }
    return exit_code;
}

/* Unit Tests for Individual Passes */

static void test_constant_folding(void) {
    printf("[Phase 4 Opt] Testing Constant Folding...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let a = 10 + 20;\n"
        "    let b = 100 - 35;\n"
        "    let c = 6 * 7;\n"
        "    let d = 80 / 4;\n"
        "    let e = 29 % 7;\n"
        "    let f = -(42);\n"
        "    let cmp1 = 10 < 20;\n"
        "    let cmp2 = 30 == 30;\n"
        "    let log1 = true && false;\n"
        "    let log2 = true || false;\n"
        "    let log3 = !false;\n"
        "    return a + b + c + d + e;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *ir_mod = parse_and_lower_ir(src, &arena, &tokens);
    IrFunction *fn = ir_mod->first_fn;
    bool prop_changed = false;
    ir_opt_constant_propagation(fn, &prop_changed);

    bool fold_changed = false;
    bool ok = ir_opt_constant_folding(fn, &fold_changed);
    assert(ok);
    assert(fold_changed);

    char *v_err = NULL;
    assert(ir_verify_function(fn, &v_err));

    /* Verify that instructions were folded into const operations */
    int const_count = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_CONST) const_count++;
        }
    }
    assert(const_count >= 10);

    cleanup_ir(ir_mod, arena, &tokens);
    printf("  -> Constant Folding PASSED\n");
}

static void test_constant_propagation(void) {
    printf("[Phase 4 Opt] Testing Constant Propagation...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let x = 40;\n"
        "    let y = 2;\n"
        "    let z = x + y;\n"
        "    return z;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *ir_mod = parse_and_lower_ir(src, &arena, &tokens);

    IrOptOptions opts = {
        .opt_level = 1,
        .dump_passes = false,
        .verify_each_pass = true,
        .verbose = false
    };

    char *opt_err = NULL;
    bool ok = ir_optimize_module(ir_mod, &opts, &opt_err);
    if (!ok) fprintf(stderr, "Opt error: %s\n", opt_err);
    assert(ok);

    /* Verify full pipeline folded 40 + 2 = 42 into a constant return */
    IrFunction *fn = ir_mod->first_fn;
    bool found_ret_const = false;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (bb->last_inst && bb->last_inst->op == IR_OP_RET) {
            if (bb->last_inst->lhs && bb->last_inst->lhs->kind == IR_VAL_CONST_INT) {
                assert(bb->last_inst->lhs->const_val.int_val == 42);
                found_ret_const = true;
            }
        }
    }
    assert(found_ret_const);

    cleanup_ir(ir_mod, arena, &tokens);
    printf("  -> Constant Propagation PASSED\n");
}

static void test_algebraic_simplification(void) {
    printf("[Phase 4 Opt] Testing Algebraic Simplification...\n");
    const char *src =
        "fn test_algebra(x: int) -> int {\n"
        "    let a = x + 0;\n"
        "    let b = 0 + a;\n"
        "    let c = b * 1;\n"
        "    let d = 1 * c;\n"
        "    let e = d - 0;\n"
        "    let f = e / 1;\n"
        "    return f;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *ir_mod = parse_and_lower_ir(src, &arena, &tokens);
    IrFunction *fn = ir_mod->first_fn;

    int initial_inst_count = count_fn_instructions(fn);

    IrOptOptions opts = {
        .opt_level = 1,
        .dump_passes = false,
        .verify_each_pass = true,
        .verbose = false
    };

    char *opt_err = NULL;
    bool ok = ir_optimize_function(fn, &opts, &opt_err);
    assert(ok);

    int final_inst_count = count_fn_instructions(fn);
    /* Algebraic simplification + DCE must significantly reduce instructions */
    assert(final_inst_count < initial_inst_count);

    cleanup_ir(ir_mod, arena, &tokens);
    printf("  -> Algebraic Simplification PASSED\n");
}

static void test_dead_code_elimination(void) {
    printf("[Phase 4 Opt] Testing Dead Code Elimination (DCE)...\n");
    const char *src =
        "fn helper() -> int { return 100; }\n"
        "fn main() -> int {\n"
        "    let dead1 = 100 * 200;\n"
        "    let dead2 = dead1 + 300;\n"
        "    let dead3 = dead2 - 50;\n"
        "    let kept_call = helper(); // Must preserve side-effect/call\n"
        "    return kept_call;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *ir_mod = parse_and_lower_ir(src, &arena, &tokens);
    IrFunction *fn = ir_mod->last_fn; /* main */

    int initial_inst_count = count_fn_instructions(fn);

    IrOptOptions opts = {
        .opt_level = 1,
        .dump_passes = false,
        .verify_each_pass = true,
        .verbose = false
    };

    char *opt_err = NULL;
    bool ok = ir_optimize_function(fn, &opts, &opt_err);
    assert(ok);

    int final_inst_count = count_fn_instructions(fn);
    assert(final_inst_count < initial_inst_count);

    /* Verify helper call is strictly preserved */
    bool found_call = false;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_CALL && inst->callee_name && strcmp(inst->callee_name, "helper") == 0) {
                found_call = true;
            }
        }
    }
    assert(found_call);

    cleanup_ir(ir_mod, arena, &tokens);
    printf("  -> Dead Code Elimination PASSED\n");
}

static void test_cfg_simplification(void) {
    printf("[Phase 4 Opt] Testing CFG & Branch Simplification...\n");
    const char *src =
        "fn main() -> int {\n"
        "    let res = 0;\n"
        "    if (true) {\n"
        "        res = 42;\n"
        "    } else {\n"
        "        res = 999; // Dead unreachable block\n"
        "    }\n"
        "    return res;\n"
        "}\n";

    AstArena *arena = NULL;
    TokenArray tokens;
    IrModule *ir_mod = parse_and_lower_ir(src, &arena, &tokens);
    IrFunction *fn = ir_mod->first_fn;

    int initial_blocks = fn->block_count;

    IrOptOptions opts = {
        .opt_level = 1,
        .dump_passes = false,
        .verify_each_pass = true,
        .verbose = false
    };

    char *opt_err = NULL;
    bool ok = ir_optimize_function(fn, &opts, &opt_err);
    assert(ok);

    int final_blocks = fn->block_count;
    /* Unreachable else-block eliminated */
    assert(final_blocks < initial_blocks);

    char *v_err = NULL;
    assert(ir_verify_function(fn, &v_err));

    cleanup_ir(ir_mod, arena, &tokens);
    printf("  -> CFG & Branch Simplification PASSED\n");
}

/* End-to-End & 6-Way Differential Verification Tests */

static void test_6way_differential(const char *src, const char *test_name, int expected_exit, const char *expected_stdout) {
    printf("[Phase 4 Opt] Running 6-Way Differential Test: %s...\n", test_name);

    char src_path[256];
    snprintf(src_path, sizeof(src_path), "build/test_opt_%s.cco", test_name);
    FILE *sf = fopen(src_path, "w");
    assert(sf != NULL);
    fputs(src, sf);
    fclose(sf);

    char cmd[1024];
    char out_buf[1024];

    /* 1. Baseline -O0 C11 Backend */
    char bin_c0[256];
    snprintf(bin_c0, sizeof(bin_c0), "build/test_opt_%s_c0", test_name);
    snprintf(cmd, sizeof(cmd), "./cco \"%s\" -O0 -o \"%s\"", src_path, bin_c0);
    assert(system(cmd) == 0);
    int exit_c0 = run_binary_and_capture(bin_c0, out_buf, sizeof(out_buf));
    assert(exit_c0 == expected_exit);
    if (expected_stdout) assert(strcmp(out_buf, expected_stdout) == 0);

    /* 2. Optimized -O1 C11 Backend */
    char bin_c1[256];
    snprintf(bin_c1, sizeof(bin_c1), "build/test_opt_%s_c1", test_name);
    snprintf(cmd, sizeof(cmd), "./cco \"%s\" -O1 -o \"%s\"", src_path, bin_c1);
    assert(system(cmd) == 0);
    int exit_c1 = run_binary_and_capture(bin_c1, out_buf, sizeof(out_buf));
    assert(exit_c1 == expected_exit);
    if (expected_stdout) assert(strcmp(out_buf, expected_stdout) == 0);

    /* 3. Optimized -O1 IR-C11 Backend */
    char bin_irc1[256];
    snprintf(bin_irc1, sizeof(bin_irc1), "build/test_opt_%s_irc1", test_name);
    snprintf(cmd, sizeof(cmd), "./cco \"%s\" -O1 --use-ir -o \"%s\"", src_path, bin_irc1);
    assert(system(cmd) == 0);
    int exit_irc1 = run_binary_and_capture(bin_irc1, out_buf, sizeof(out_buf));
    assert(exit_irc1 == expected_exit);
    if (expected_stdout) assert(strcmp(out_buf, expected_stdout) == 0);

    /* 4. Optimized -O1 Native Assembly Backend */
    char asm_path[256];
    char bin_asm[256];
    snprintf(asm_path, sizeof(asm_path), "build/test_opt_%s.s", test_name);
    snprintf(bin_asm, sizeof(bin_asm), "build/test_opt_%s_asm", test_name);
    snprintf(cmd, sizeof(cmd), "./cco \"%s\" -O1 --emit-asm -o \"%s\"", src_path, asm_path);
    assert(system(cmd) == 0);
    snprintf(cmd, sizeof(cmd), "gcc -no-pie \"%s\" -o \"%s\" -lm", asm_path, bin_asm);
    assert(system(cmd) == 0);
    int exit_asm = run_binary_and_capture(bin_asm, out_buf, sizeof(out_buf));
    assert(exit_asm == expected_exit);
    if (expected_stdout) assert(strcmp(out_buf, expected_stdout) == 0);

    /* 5. Optimized -O1 Direct Object + Host Linker */
    char bin_obj[256];
    snprintf(bin_obj, sizeof(bin_obj), "build/test_opt_%s_obj", test_name);
    snprintf(cmd, sizeof(cmd), "./cco \"%s\" -O1 --use-native -o \"%s\"", src_path, bin_obj);
    assert(system(cmd) == 0);
    int exit_obj = run_binary_and_capture(bin_obj, out_buf, sizeof(out_buf));
    assert(exit_obj == expected_exit);
    if (expected_stdout) assert(strcmp(out_buf, expected_stdout) == 0);

    /* 6. Optimized -O1 Direct Object + Internal Linker */
    char bin_int[256];
    snprintf(bin_int, sizeof(bin_int), "build/test_opt_%s_internal", test_name);
    snprintf(cmd, sizeof(cmd), "./cco \"%s\" -O1 --use-internal-linker -o \"%s\"", src_path, bin_int);
    assert(system(cmd) == 0);
    int exit_int = run_binary_and_capture(bin_int, out_buf, sizeof(out_buf));
    assert(exit_int == expected_exit);
    if (expected_stdout) assert(strcmp(out_buf, expected_stdout) == 0);

    /* Cleanup binaries */
    unlink(src_path);
    unlink(bin_c0);
    unlink(bin_c1);
    unlink(bin_irc1);
    unlink(asm_path);
    unlink(bin_asm);
    unlink(bin_obj);
    unlink(bin_int);

    printf("  -> 6-Way Differential Test %s PASSED (exit=%d)\n", test_name, expected_exit);
}

int main(void) {
    printf("=================================================================\n");
    printf("Cco Phase 4: IR Optimization Pipeline & Differential Tests\n");
    printf("=================================================================\n");

    /* 1. Unit pass tests */
    test_constant_folding();
    test_constant_propagation();
    test_algebraic_simplification();
    test_dead_code_elimination();
    test_cfg_simplification();

    /* 2. 6-Way differential end-to-end test cases across all backends */
    test_6way_differential(
        "fn main() -> int {\n"
        "    let a = 10 + 5;\n"
        "    let b = a * 2;\n"
        "    let c = b - 0;\n"
        "    let d = c * 1;\n"
        "    print(d);\n"
        "    return d;\n"
        "}\n",
        "arith_identities", 30, "30\n"
    );

    test_6way_differential(
        "fn fib(n: int) -> int {\n"
        "    if (n <= 1) { return n; }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "fn main() -> int {\n"
        "    let res = fib(10);\n"
        "    print(res);\n"
        "    return res;\n"
        "}\n",
        "recursive_fib", 55, "55\n"
    );

    test_6way_differential(
        "fn main() -> int {\n"
        "    let sum = 0;\n"
        "    let i = 0;\n"
        "    while (i <= 10) {\n"
        "        sum = sum + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    print(sum);\n"
        "    return sum;\n"
        "}\n",
        "while_loop_sum", 55, "55\n"
    );

    test_6way_differential(
        "fn test_branch(x: int) -> int {\n"
        "    if (x > 5) {\n"
        "        return x * 10;\n"
        "    } else {\n"
        "        return x * 2;\n"
        "    }\n"
        "}\n"
        "fn main() -> int {\n"
        "    let a = test_branch(3);\n"
        "    let b = test_branch(7);\n"
        "    print(a);\n"
        "    print(b);\n"
        "    return a + b;\n"
        "}\n",
        "branches_cond", 76, "6\n70\n"
    );

    test_6way_differential(
        "fn main() -> int {\n"
        "    let x = 12.5 + 7.5;\n"
        "    let y = x * 2.0;\n"
        "    if (y == 40.0) { return 42; }\n"
        "    return 1;\n"
        "}\n",
        "float_opt", 42, NULL
    );

    printf("All IR Optimization tests passed successfully.\n");
    return 0;
}
