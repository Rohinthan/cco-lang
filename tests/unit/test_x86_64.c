// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir.h"
#include "ir_verify.h"
#include "ir_lower.h"
#include "ir_codegen_c.h"
#include "x86_64_target.h"
#include "x86_64_codegen.h"
#include "x86_64_instr.h"
#include "x86_64_encode.h"
#include "x86_64_elf.h"
#include "lexer.h"
#include "parser.h"
#include "scope_analysis.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>

static void ensure_build_dir(void) {
    struct stat st;
    if (stat("build", &st) != 0) {
        int r = system("mkdir -p build");
        (void)r;
    }
}

/* Helper to compile and run a small Cco source string natively via x86-64 backend,
 * returning the process exit code and capturing stdout into out_buf.
 */
static int compile_and_run_native(const char *source, const char *test_name, char *out_buf, size_t out_buf_size) {
    ensure_build_dir();
    TokenArray tokens = lex_source(source);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    char src_name[128];
    snprintf(src_name, sizeof(src_name), "%s.cco", test_name);
    desugar_top_level_program(ast, arena, src_name);
    analyze_scopes(ast, arena);

    IrModule *ir_mod = ir_lower_ast(ast, src_name);
    assert(ir_mod != NULL);

    char *verify_err = NULL;
    bool valid = ir_verify_module(ir_mod, &verify_err);
    if (!valid) {
        fprintf(stderr, "IR verification failed: %s\n", verify_err);
    }
    assert(valid);

    char *asm_err = NULL;
    char *asm_code = x86_64_generate_assembly(ir_mod, &asm_err);
    if (!asm_code) {
        fprintf(stderr, "x86-64 codegen error: %s\n", asm_err);
    }
    assert(asm_code != NULL);

    char asm_path[256];
    char bin_path[256];
    snprintf(asm_path, sizeof(asm_path), "build/%s.s", test_name);
    snprintf(bin_path, sizeof(bin_path), "build/%s_bin", test_name);

    FILE *f = fopen(asm_path, "w");
    assert(f != NULL);
    fputs(asm_code, f);
    fclose(f);

    free(asm_code);
    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);

    /* Assemble and link using host GCC */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc -no-pie \"%s\" -o \"%s\" -lm", asm_path, bin_path);
    int res = system(cmd);
    assert(res == 0);

    /* Run the binary and capture stdout */
    char run_cmd[1024];
    if (out_buf) {
        snprintf(run_cmd, sizeof(run_cmd), "./\"%s\" > build/%s_out.txt 2>&1", bin_path, test_name);
    } else {
        snprintf(run_cmd, sizeof(run_cmd), "./\"%s\"", bin_path);
    }
    int run_res = system(run_cmd);
    int exit_status = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(run_res)) exit_status = WEXITSTATUS(run_res);
#endif

    if (out_buf) {
        char out_path[256];
        snprintf(out_path, sizeof(out_path), "build/%s_out.txt", test_name);
        FILE *rf = fopen(out_path, "r");
        if (rf) {
            size_t bytes = fread(out_buf, 1, out_buf_size - 1, rf);
            out_buf[bytes] = '\0';
            fclose(rf);
        } else {
            out_buf[0] = '\0';
        }
    }

    return exit_status;
}

static void test_return_constant(void) {
    printf("[TEST] test_return_constant (return 42)... ");
    const char *src =
        "fn main() -> int {\n"
        "    return 42;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_ret_42", NULL, 0);
    assert(status == 42);
    printf("PASS\n");
}

static void test_arithmetic_operations(void) {
    printf("[TEST] test_arithmetic_operations (+, -, *, /, %%, neg)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let a = 20;\n"
        "    let b = 6;\n"
        "    let sum = a + b;       // 26\n"
        "    let diff = a - b;      // 14\n"
        "    let prod = a * b;      // 120\n"
        "    let quot = a / b;      // 3\n"
        "    let rem = a % b;       // 2\n"
        "    let neg_b = -b;        // -6\n"
        "    let check = sum + diff + quot + rem + neg_b; // 26 + 14 + 3 + 2 - 6 = 39\n"
        "    print(check);\n"
        "    return check;\n"
        "}\n";
    char out[128];
    int status = compile_and_run_native(src, "test_arith", out, sizeof(out));
    assert(status == 39);
    assert(strstr(out, "39") != NULL);
    printf("PASS\n");
}

static void test_variables_and_assignment(void) {
    printf("[TEST] test_variables_and_assignment... ");
    const char *src =
        "fn main() -> int {\n"
        "    let x = 10;\n"
        "    let y = 20;\n"
        "    x = x + 5;\n"
        "    y = y * 2;\n"
        "    let z = x + y;\n"
        "    return z; // 15 + 40 = 55\n"
        "}\n";
    int status = compile_and_run_native(src, "test_vars", NULL, 0);
    assert(status == 55);
    printf("PASS\n");
}

static void test_comparisons(void) {
    printf("[TEST] test_comparisons (==, !=, <, <=, >, >=)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let count = 0;\n"
        "    if (10 == 10) { count = count + 1; }\n"
        "    if (10 != 20) { count = count + 1; }\n"
        "    if (5 < 10)   { count = count + 1; }\n"
        "    if (10 <= 10) { count = count + 1; }\n"
        "    if (20 > 10)  { count = count + 1; }\n"
        "    if (20 >= 20) { count = count + 1; }\n"
        "    if (10 == 20) { count = count + 100; }\n"
        "    return count; // 6\n"
        "}\n";
    int status = compile_and_run_native(src, "test_cmps", NULL, 0);
    assert(status == 6);
    printf("PASS\n");
}

static void test_control_flow_branches_and_loops(void) {
    printf("[TEST] test_control_flow_branches_and_loops (if, while, for)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let sum = 0;\n"
        "    let i = 1;\n"
        "    while (i <= 10) {\n"
        "        sum = sum + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    // sum is 55\n"
        "    for (let j = 0; j < 5; j = j + 1) {\n"
        "        sum = sum + 1;\n"
        "    }\n"
        "    // sum is 60\n"
        "    return sum;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_control_flow", NULL, 0);
    assert(status == 60);
    printf("PASS\n");
}

static void test_functions_and_multiple_parameters(void) {
    printf("[TEST] test_functions_and_multiple_parameters... ");
    const char *src =
        "fn add3(a: int, b: int, c: int) -> int {\n"
        "    return a + b + c;\n"
        "}\n"
        "fn compute(x: int, y: int) -> int {\n"
        "    let t = add3(x, y, 10);\n"
        "    return t * 2;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let ans = compute(3, 4); // (3 + 4 + 10) * 2 = 34\n"
        "    return ans;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_funcs", NULL, 0);
    assert(status == 34);
    printf("PASS\n");
}

static void test_recursive_function(void) {
    printf("[TEST] test_recursive_function (fibonacci)... ");
    const char *src =
        "fn fib(n: int) -> int {\n"
        "    if (n <= 1) {\n"
        "        return n;\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "fn main() -> int {\n"
        "    let f7 = fib(7); // 13\n"
        "    print(f7);\n"
        "    return f7;\n"
        "}\n";
    char out[128];
    int status = compile_and_run_native(src, "test_fib", out, sizeof(out));
    assert(status == 13);
    assert(strstr(out, "13") != NULL);
    printf("PASS\n");
}

static void test_stack_behavior_deep_locals(void) {
    printf("[TEST] test_stack_behavior_deep_locals... ");
    const char *src =
        "fn main() -> int {\n"
        "    let v1 = 1;\n"
        "    let v2 = 2;\n"
        "    let v3 = 3;\n"
        "    let v4 = 4;\n"
        "    let v5 = 5;\n"
        "    let v6 = 6;\n"
        "    let v7 = 7;\n"
        "    let v8 = 8;\n"
        "    let sum = v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8;\n"
        "    return sum; // 36\n"
        "}\n";
    int status = compile_and_run_native(src, "test_deep_locals", NULL, 0);
    assert(status == 36);
    printf("PASS\n");
}

static void test_differential_parity_with_c11_backend(void) {
    printf("[TEST] test_differential_parity_with_c11_backend... ");
    const char *src =
        "fn collatz(n: int) -> int {\n"
        "    let steps = 0;\n"
        "    while (n != 1) {\n"
        "        if (n % 2 == 0) {\n"
        "            n = n / 2;\n"
        "        } else {\n"
        "            n = 3 * n + 1;\n"
        "        }\n"
        "        steps = steps + 1;\n"
        "    }\n"
        "    return steps;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let s = collatz(27);\n"
        "    print(s);\n"
        "    return 0;\n"
        "}\n";

    /* Run with native backend */
    char native_out[128];
    int native_status = compile_and_run_native(src, "test_collatz_native", native_out, sizeof(native_out));

    /* Run with reference C11 backend */
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, arena, "test_collatz_ref.cco");
    analyze_scopes(ast, arena);
    IrModule *ir_mod = ir_lower_ast(ast, "test_collatz_ref.cco");
    char *c_code = ir_generate_c(ir_mod);

    FILE *cf = fopen("build/test_collatz_ref.c", "w");
    assert(cf != NULL);
    fputs(c_code, cf);
    fclose(cf);

    free(c_code);
    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);

    int compile_res = system("gcc -Wall -Wextra -std=c11 build/test_collatz_ref.c -o build/test_collatz_ref_bin -lm");
    assert(compile_res == 0);

    int c_run_res = system("./build/test_collatz_ref_bin > build/test_collatz_ref_out.txt 2>&1");
    int c_status = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(c_run_res)) c_status = WEXITSTATUS(c_run_res);
#endif

    char c_out[128] = {0};
    FILE *rf = fopen("build/test_collatz_ref_out.txt", "r");
    if (rf) {
        size_t b = fread(c_out, 1, sizeof(c_out) - 1, rf);
        c_out[b] = '\0';
        fclose(rf);
    }

    /* Verify exact equivalence */
    assert(native_status == c_status);
    assert(strcmp(native_out, c_out) == 0);
    assert(strstr(native_out, "111") != NULL); /* Collatz 27 takes 111 steps */
    printf("PASS (exact match: %s)\n", native_out);
}

static void test_regalloc_reuse(void) {
    printf("[TEST] test_regalloc_reuse (short-lived arithmetic reuse)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let a = 1 + 2;\n"
        "    let b = 3 + 4;\n"
        "    let c = a * b;\n"
        "    let d = 5 + 6;\n"
        "    let e = 7 + 8;\n"
        "    let f = d * e;\n"
        "    return f - c; // 165 - 21 = 144\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_reuse", NULL, 0);
    assert(status == 144);
    printf("PASS\n");
}

static void test_regalloc_pressure(void) {
    printf("[TEST] test_regalloc_pressure (16 live values with spilling)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let v1 = 1;\n"
        "    let v2 = 2;\n"
        "    let v3 = 3;\n"
        "    let v4 = 4;\n"
        "    let v5 = 5;\n"
        "    let v6 = 6;\n"
        "    let v7 = 7;\n"
        "    let v8 = 8;\n"
        "    let v9 = 9;\n"
        "    let v10 = 10;\n"
        "    let v11 = 11;\n"
        "    let v12 = 12;\n"
        "    let v13 = 13;\n"
        "    let v14 = 14;\n"
        "    let v15 = 15;\n"
        "    let v16 = 16;\n"
        "    let sum = v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16;\n"
        "    return sum; // 136\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_pressure", NULL, 0);
    assert(status == 136);
    printf("PASS\n");
}

static void test_regalloc_call_preservation(void) {
    printf("[TEST] test_regalloc_call_preservation (values live across call)... ");
    const char *src =
        "fn add_ten(x: int) -> int {\n"
        "    return x + 10;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let a = 15;\n"
        "    let b = 25;\n"
        "    let c = add_ten(a);\n"
        "    return a + b + c; // 15 + 25 + 25 = 65\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_call_preservation", NULL, 0);
    assert(status == 65);
    printf("PASS\n");
}

static void test_regalloc_multiple_calls(void) {
    printf("[TEST] test_regalloc_multiple_calls (several calls with live values)... ");
    const char *src =
        "fn inc(x: int) -> int { return x + 1; }\n"
        "fn dec(x: int) -> int { return x - 1; }\n"
        "fn main() -> int {\n"
        "    let x = 10;\n"
        "    let y = 20;\n"
        "    let z = 30;\n"
        "    let r1 = inc(x);\n"
        "    let r2 = dec(y);\n"
        "    let r3 = inc(z);\n"
        "    return (x + y + z) + (r1 + r2 + r3); // 60 + 61 = 121\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_multiple_calls", NULL, 0);
    assert(status == 121);
    printf("PASS\n");
}

static void test_regalloc_loop_carried(void) {
    printf("[TEST] test_regalloc_loop_carried (loop accumulator and counter)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let sum = 0;\n"
        "    let factor = 2;\n"
        "    for (let i = 1; i <= 10; i = i + 1) {\n"
        "        sum = sum + i * factor;\n"
        "    }\n"
        "    return sum; // 110\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_loop_carried", NULL, 0);
    assert(status == 110);
    printf("PASS\n");
}

static void test_regalloc_nested_branches(void) {
    printf("[TEST] test_regalloc_nested_branches (nested if/else merges)... ");
    const char *src =
        "fn branch_eval(x: int, y: int) -> int {\n"
        "    let res = 0;\n"
        "    if (x > 10) {\n"
        "        if (y > 20) {\n"
        "            res = x + y;\n"
        "        } else {\n"
        "            res = x - y;\n"
        "        }\n"
        "    } else {\n"
        "        if (y > 5) {\n"
        "            res = x * y;\n"
        "        } else {\n"
        "            res = x / y;\n"
        "        }\n"
        "    }\n"
        "    return res;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let a = branch_eval(15, 25); // 40\n"
        "    let b = branch_eval(15, 5);  // 10\n"
        "    let c = branch_eval(5, 6);   // 30\n"
        "    let d = branch_eval(8, 2);   // 4\n"
        "    return a + b + c + d; // 84\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_nested_branches", NULL, 0);
    assert(status == 84);
    printf("PASS\n");
}

static void test_regalloc_recursion(void) {
    printf("[TEST] test_regalloc_recursion (recursive fibonacci 10 = 55)... ");
    const char *src =
        "fn fib(n: int) -> int {\n"
        "    if (n <= 1) {\n"
        "        return n;\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "fn main() -> int {\n"
        "    return fib(10); // 55\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_recursion", NULL, 0);
    assert(status == 55);
    printf("PASS\n");
}

static void test_regalloc_many_params(void) {
    printf("[TEST] test_regalloc_many_params (6 System V ABI parameters)... ");
    const char *src =
        "fn sum6(a: int, b: int, c: int, d: int, e: int, f: int) -> int {\n"
        "    return a + b + c + d + e + f;\n"
        "}\n"
        "fn main() -> int {\n"
        "    return sum6(1, 2, 3, 4, 5, 6); // 21\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_many_params", NULL, 0);
    assert(status == 21);
    printf("PASS\n");
}

static void test_regalloc_large_locals(void) {
    printf("[TEST] test_regalloc_large_locals (20 local variables + cross arithmetic)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let a0 = 1;  let a1 = 2;  let a2 = 3;  let a3 = 4;\n"
        "    let a4 = 5;  let a5 = 6;  let a6 = 7;  let a7 = 8;\n"
        "    let a8 = 9;  let a9 = 10; let a10 = 11; let a11 = 12;\n"
        "    let a12 = 13; let a13 = 14; let a14 = 15; let a15 = 16;\n"
        "    let a16 = 17; let a17 = 18; let a18 = 19; let a19 = 20;\n"
        "    let s1 = a0 + a1 + a2 + a3 + a4;\n"
        "    let s2 = a5 + a6 + a7 + a8 + a9;\n"
        "    let s3 = a10 + a11 + a12 + a13 + a14;\n"
        "    let s4 = a15 + a16 + a17 + a18 + a19;\n"
        "    let total = s1 + s2 + s3 + s4;\n"
        "    if (total == 210) {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_regalloc_large_locals", NULL, 0);
    assert(status == 42);
    printf("PASS\n");
}

/* Floating-point and XMM register test suite */

static void test_float_arithmetic(void) {
    printf("[TEST] test_float_arithmetic (+, -, *, /, neg)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let a: float = 20.5;\n"
        "    let b: float = 4.0;\n"
        "    let add: float = a + b;   // 24.5\n"
        "    let sub: float = a - b;   // 16.5\n"
        "    let mul: float = b * 2.5; // 10.0\n"
        "    let div: float = a / b;   // 5.125\n"
        "    let neg: float = -a;      // -20.5\n"
        "    let pos: float = -neg;    // 20.5\n"
        "    let count = 0;\n"
        "    if (add == 24.5) { count = count + 1; }\n"
        "    if (sub == 16.5) { count = count + 1; }\n"
        "    if (mul == 10.0) { count = count + 1; }\n"
        "    if (div == 5.125) { count = count + 1; }\n"
        "    if (pos == 20.5) { count = count + 1; }\n"
        "    if (neg == -20.5) { count = count + 1; }\n"
        "    return count; // 6\n"
        "}\n";
    int status = compile_and_run_native(src, "test_float_arithmetic", NULL, 0);
    assert(status == 6);
    printf("PASS\n");
}

static void test_float_comparisons(void) {
    printf("[TEST] test_float_comparisons (==, !=, <, <=, >, >=, signed zeros, NaN)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let count = 0;\n"
        "    let x: float = 2.5;\n"
        "    let y: float = 5.0;\n"
        "    if (x < y) { count = count + 1; }\n"
        "    if (x <= y) { count = count + 1; }\n"
        "    if (y > x) { count = count + 1; }\n"
        "    if (y >= x) { count = count + 1; }\n"
        "    if (x == 2.5) { count = count + 1; }\n"
        "    if (x != y) { count = count + 1; }\n"
        "    // Signed zeros: +0.0 and -0.0 compare equal in IEEE 754\n"
        "    let z1: float = 0.0;\n"
        "    let z2: float = -0.0;\n"
        "    if (z1 == z2) { count = count + 1; }\n"
        "    if (z1 <= z2) { count = count + 1; }\n"
        "    if (z1 >= z2) { count = count + 1; }\n"
        "    if (z1 != z2) { count = count + 100; }\n"
        "    // NaN behavior: unordered comparisons\n"
        "    let nan: float = 0.0 / 0.0;\n"
        "    if (nan != nan) { count = count + 1; }\n"
        "    if (nan == nan) { count = count + 100; }\n"
        "    if (nan < 1.0) { count = count + 100; }\n"
        "    if (nan > 1.0) { count = count + 100; }\n"
        "    if (nan <= 1.0) { count = count + 100; }\n"
        "    if (nan >= 1.0) { count = count + 100; }\n"
        "    return count; // 10\n"
        "}\n";
    int status = compile_and_run_native(src, "test_float_comparisons", NULL, 0);
    assert(status == 10);
    printf("PASS\n");
}

static void test_float_constants_and_variables(void) {
    printf("[TEST] test_float_constants_and_variables (pool + assignment)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let c1: float = 3.141592653589793;\n"
        "    let c2: float = 2.718281828459045;\n"
        "    let c3: float = 1.414213562373095;\n"
        "    let x: float = c1;\n"
        "    x = x + c2;\n"
        "    x = x + c3;\n"
        "    // 3.141592653589793 + 2.718281828459045 + 1.414213562373095 = 7.274088044421933\n"
        "    if (x > 7.274 && x < 7.275) {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_float_constants_and_variables", NULL, 0);
    assert(status == 42);
    printf("PASS\n");
}

static void test_float_abi_parameters(void) {
    printf("[TEST] test_float_abi_parameters (8 float args + mixed int/float args)... ");
    const char *src =
        "fn sum8(a: float, b: float, c: float, d: float, e: float, f: float, g: float, h: float) -> float {\n"
        "    return a + b + c + d + e + f + g + h;\n"
        "}\n"
        "fn mix(i1: int, f1: float, i2: int, f2: float, i3: int, f3: float) -> float {\n"
        "    let isum: int = i1 + i2 + i3; // 60\n"
        "    let fsum: float = f1 + f2 + f3; // 7.5\n"
        "    if (isum == 60) {\n"
        "        return fsum;\n"
        "    }\n"
        "    return 0.0;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let s: float = sum8(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0); // 36.0\n"
        "    let m: float = mix(10, 1.5, 20, 2.5, 30, 3.5); // 7.5\n"
        "    if (s == 36.0 && m == 7.5) {\n"
        "        return 88;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_float_abi_parameters", NULL, 0);
    assert(status == 88);
    printf("PASS\n");
}

static void test_float_recursion_and_nesting(void) {
    printf("[TEST] test_float_recursion_and_nesting (recursive power + nested calls)... ");
    const char *src =
        "fn pow_float(base: float, exp: int) -> float {\n"
        "    if (exp == 0) {\n"
        "        return 1.0;\n"
        "    }\n"
        "    return base * pow_float(base, exp - 1);\n"
        "}\n"
        "fn scale(val: float, factor: float) -> float {\n"
        "    return val * factor;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let p: float = scale(pow_float(2.0, 8), 0.5); // 256.0 * 0.5 = 128.0\n"
        "    if (p == 128.0) {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_float_recursion_and_nesting", NULL, 0);
    assert(status == 42);
    printf("PASS\n");
}

static void test_float_loop_carried(void) {
    printf("[TEST] test_float_loop_carried (loop accumulator)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let sum: float = 0.0;\n"
        "    let i: int = 0;\n"
        "    while (i < 20) {\n"
        "        sum = sum + 2.5;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    if (sum == 50.0) {\n"
        "        return 50;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_float_loop_carried", NULL, 0);
    assert(status == 50);
    printf("PASS\n");
}

static void test_float_register_pressure_spilling(void) {
    printf("[TEST] test_float_register_pressure_spilling (16 concurrent live floats)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let f0: float = 1.0;  let f1: float = 2.0;\n"
        "    let f2: float = 3.0;  let f3: float = 4.0;\n"
        "    let f4: float = 5.0;  let f5: float = 6.0;\n"
        "    let f6: float = 7.0;  let f7: float = 8.0;\n"
        "    let f8: float = 9.0;  let f9: float = 10.0;\n"
        "    let f10: float = 11.0; let f11: float = 12.0;\n"
        "    let f12: float = 13.0; let f13: float = 14.0;\n"
        "    let f14: float = 15.0; let f15: float = 16.0;\n"
        "    let s1: float = f0 + f1 + f2 + f3;\n"
        "    let s2: float = f4 + f5 + f6 + f7;\n"
        "    let s3: float = f8 + f9 + f10 + f11;\n"
        "    let s4: float = f12 + f13 + f14 + f15;\n"
        "    let total: float = s1 + s2 + s3 + s4;\n"
        "    // Sum of 1..16 = 136.0\n"
        "    if (total == 136.0) {\n"
        "        return 136;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    int status = compile_and_run_native(src, "test_float_register_pressure_spilling", NULL, 0);
    assert(status == 136);
    printf("PASS\n");
}

static void test_float_differential_parity(void) {
    printf("[TEST] test_float_differential_parity (Newton-Raphson sqrt approximation)... ");
    const char *src =
        "fn sqrt_approx(val: float) -> float {\n"
        "    let x: float = val / 2.0;\n"
        "    let i: int = 0;\n"
        "    while (i < 10) {\n"
        "        x = 0.5 * (x + val / x);\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return x;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let r1: float = sqrt_approx(2.0);\n"
        "    print(r1);\n"
        "    let r2: float = sqrt_approx(100.0);\n"
        "    print(r2);\n"
        "    let r3: float = sqrt_approx(144.0);\n"
        "    print(r3);\n"
        "    return 0;\n"
        "}\n";

    /* Run native backend */
    char native_out[256];
    int native_status = compile_and_run_native(src, "test_float_diff_native", native_out, sizeof(native_out));

    /* Run reference C11 backend */
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, arena, "test_float_diff_ref.cco");
    analyze_scopes(ast, arena);
    IrModule *ir_mod = ir_lower_ast(ast, "test_float_diff_ref.cco");
    char *c_code = ir_generate_c(ir_mod);

    FILE *cf = fopen("build/test_float_diff_ref.c", "w");
    assert(cf != NULL);
    fputs(c_code, cf);
    fclose(cf);

    free(c_code);
    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);

    int compile_res = system("gcc -Wall -Wextra -std=c11 build/test_float_diff_ref.c -o build/test_float_diff_ref_bin -lm");
    assert(compile_res == 0);

    int c_run_res = system("./build/test_float_diff_ref_bin > build/test_float_diff_ref_out.txt 2>&1");
    int c_status = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(c_run_res)) c_status = WEXITSTATUS(c_run_res);
#endif

    char c_out[256] = {0};
    FILE *rf = fopen("build/test_float_diff_ref_out.txt", "r");
    if (rf) {
        size_t b = fread(c_out, 1, sizeof(c_out) - 1, rf);
        c_out[b] = '\0';
        fclose(rf);
    }

    assert(native_status == c_status);
    assert(strcmp(native_out, c_out) == 0);
    printf("PASS (exact match across native and C11: %s)\n", native_out);
}

/* Stabilization, ABI, and 3-way differential test suite */

static void run_3way_differential_test(const char *src, const char *test_name, int expected_status, bool check_exact_stdout) {
    ensure_build_dir();

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    char src_name[128];
    snprintf(src_name, sizeof(src_name), "%s.cco", test_name);
    desugar_top_level_program(ast, arena, src_name);
    analyze_scopes(ast, arena);

    /* --- Pipeline 1: Reference C11 Backend --- */
    char *c11_code = generate_c_code(ast, arena);
    assert(c11_code != NULL);
    char p1_c_path[256], p1_bin_path[256], p1_out_path[256];
    snprintf(p1_c_path, sizeof(p1_c_path), "build/%s_p1.c", test_name);
    snprintf(p1_bin_path, sizeof(p1_bin_path), "build/%s_p1_bin", test_name);
    snprintf(p1_out_path, sizeof(p1_out_path), "build/%s_p1_out.txt", test_name);

    FILE *f1 = fopen(p1_c_path, "w");
    assert(f1 != NULL);
    fputs(c11_code, f1);
    fclose(f1);
    free(c11_code);

    char cmd1[1024];
    snprintf(cmd1, sizeof(cmd1), "gcc -Wall -Wextra -std=c11 \"%s\" -o \"%s\" -lm", p1_c_path, p1_bin_path);
    int compile_res1 = system(cmd1);
    assert(compile_res1 == 0);

    char run_cmd1[1024];
    snprintf(run_cmd1, sizeof(run_cmd1), "./\"%s\" > \"%s\" 2>&1", p1_bin_path, p1_out_path);
    int run_res1 = system(run_cmd1);
    int p1_status = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(run_res1)) p1_status = WEXITSTATUS(run_res1);
#endif

    char p1_out[512] = {0};
    FILE *rf1 = fopen(p1_out_path, "r");
    if (rf1) {
        size_t b = fread(p1_out, 1, sizeof(p1_out) - 1, rf1);
        p1_out[b] = '\0';
        fclose(rf1);
    }

    /* --- Lower to Cco IR --- */
    IrModule *ir_mod = ir_lower_ast(ast, src_name);
    assert(ir_mod != NULL);
    char *verify_err = NULL;
    bool valid = ir_verify_module(ir_mod, &verify_err);
    if (!valid) {
        fprintf(stderr, "IR verify failed: %s\n", verify_err);
    }
    assert(valid);

    /* --- Pipeline 2: IR -> C11 Backend --- */
    char *ir_c_code = ir_generate_c(ir_mod);
    assert(ir_c_code != NULL);
    char p2_c_path[256], p2_bin_path[256], p2_out_path[256];
    snprintf(p2_c_path, sizeof(p2_c_path), "build/%s_p2.c", test_name);
    snprintf(p2_bin_path, sizeof(p2_bin_path), "build/%s_p2_bin", test_name);
    snprintf(p2_out_path, sizeof(p2_out_path), "build/%s_p2_out.txt", test_name);

    FILE *f2 = fopen(p2_c_path, "w");
    assert(f2 != NULL);
    fputs(ir_c_code, f2);
    fclose(f2);
    free(ir_c_code);

    char cmd2[1024];
    snprintf(cmd2, sizeof(cmd2), "gcc -Wall -Wextra -std=c11 \"%s\" -o \"%s\" -lm", p2_c_path, p2_bin_path);
    int compile_res2 = system(cmd2);
    assert(compile_res2 == 0);

    char run_cmd2[1024];
    snprintf(run_cmd2, sizeof(run_cmd2), "./\"%s\" > \"%s\" 2>&1", p2_bin_path, p2_out_path);
    int run_res2 = system(run_cmd2);
    int p2_status = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(run_res2)) p2_status = WEXITSTATUS(run_res2);
#endif

    char p2_out[512] = {0};
    FILE *rf2 = fopen(p2_out_path, "r");
    if (rf2) {
        size_t b = fread(p2_out, 1, sizeof(p2_out) - 1, rf2);
        p2_out[b] = '\0';
        fclose(rf2);
    }

    /* --- Pipeline 3: Native x86-64 Backend --- */
    char *asm_err = NULL;
    char *asm_code = x86_64_generate_assembly(ir_mod, &asm_err);
    if (!asm_code) {
        fprintf(stderr, "x86-64 codegen failed: %s\n", asm_err);
    }
    assert(asm_code != NULL);
    char p3_s_path[256], p3_bin_path[256], p3_out_path[256];
    snprintf(p3_s_path, sizeof(p3_s_path), "build/%s_p3.s", test_name);
    snprintf(p3_bin_path, sizeof(p3_bin_path), "build/%s_p3_bin", test_name);
    snprintf(p3_out_path, sizeof(p3_out_path), "build/%s_p3_out.txt", test_name);

    FILE *f3 = fopen(p3_s_path, "w");
    assert(f3 != NULL);
    fputs(asm_code, f3);
    fclose(f3);
    free(asm_code);

    char cmd3[1024];
    snprintf(cmd3, sizeof(cmd3), "gcc -no-pie \"%s\" -o \"%s\" -lm", p3_s_path, p3_bin_path);
    int compile_res3 = system(cmd3);
    assert(compile_res3 == 0);

    char run_cmd3[1024];
    snprintf(run_cmd3, sizeof(run_cmd3), "./\"%s\" > \"%s\" 2>&1", p3_bin_path, p3_out_path);
    int run_res3 = system(run_cmd3);
    int p3_status = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(run_res3)) p3_status = WEXITSTATUS(run_res3);
#endif

    char p3_out[512] = {0};
    FILE *rf3 = fopen(p3_out_path, "r");
    if (rf3) {
        size_t b = fread(p3_out, 1, sizeof(p3_out) - 1, rf3);
        p3_out[b] = '\0';
        fclose(rf3);
    }

    /* --- Pipeline 4: Direct ELF64 Object Generator --- */
    char p4_o_path[256], p4_bin_path[256], p4_out_path[256];
    snprintf(p4_o_path, sizeof(p4_o_path), "build/%s_p4.o", test_name);
    snprintf(p4_bin_path, sizeof(p4_bin_path), "build/%s_p4_bin", test_name);
    snprintf(p4_out_path, sizeof(p4_out_path), "build/%s_p4_out.txt", test_name);

    char *obj_err = NULL;
    bool obj_ok = x86_64_emit_object_file(ir_mod, p4_o_path, &obj_err);
    if (!obj_ok) {
        fprintf(stderr, "direct object generation failed: %s\n", obj_err);
    }
    assert(obj_ok);

    char cmd4[1024];
    snprintf(cmd4, sizeof(cmd4), "gcc -no-pie \"%s\" -o \"%s\" -lm", p4_o_path, p4_bin_path);
    int compile_res4 = system(cmd4);
    assert(compile_res4 == 0);

    char run_cmd4[1024];
    snprintf(run_cmd4, sizeof(run_cmd4), "./\"%s\" > \"%s\" 2>&1", p4_bin_path, p4_out_path);
    int run_res4 = system(run_cmd4);
    int p4_status = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(run_res4)) p4_status = WEXITSTATUS(run_res4);
#endif

    char p4_out[512] = {0};
    FILE *rf4 = fopen(p4_out_path, "r");
    if (rf4) {
        size_t b = fread(p4_out, 1, sizeof(p4_out) - 1, rf4);
        p4_out[b] = '\0';
        fclose(rf4);
    }

    /* Clean up AST, Tokens, and IR */
    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);

    /* Assert 4-way exact status parity */
    assert(p1_status == expected_status);
    assert(p2_status == expected_status);
    assert(p3_status == expected_status);
    assert(p4_status == expected_status);

    if (check_exact_stdout) {
        assert(strcmp(p1_out, p2_out) == 0);
        assert(strcmp(p2_out, p3_out) == 0);
        assert(strcmp(p3_out, p4_out) == 0);
    }
}

static void test_diff_abi_stack_args_int(void) {
    printf("[TEST 3-WAY] test_diff_abi_stack_args_int (10 integer parameters)... ");
    const char *src =
        "fn sum10(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int, j: int) -> int {\n"
        "    return a + b + c + d + e + f + g + h + i + j;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let res: int = sum10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);\n"
        "    print(res);\n"
        "    if (res == 55) { return 55; }\n"
        "    return 0;\n"
        "}\n";
    run_3way_differential_test(src, "diff_abi_int10", 55, true);
    printf("PASS\n");
}

static void test_diff_abi_stack_args_float(void) {
    printf("[TEST 3-WAY] test_diff_abi_stack_args_float (12 float parameters)... ");
    const char *src =
        "fn sum12(f1: float, f2: float, f3: float, f4: float, f5: float, f6: float,\n"
        "         f7: float, f8: float, f9: float, f10: float, f11: float, f12: float) -> float {\n"
        "    return f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10 + f11 + f12;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let s: float = sum12(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0);\n"
        "    print(s);\n"
        "    if (s == 78.0) { return 78; }\n"
        "    return 0;\n"
        "}\n";
    run_3way_differential_test(src, "diff_abi_float12", 78, true);
    printf("PASS\n");
}

static void test_diff_abi_mixed_stack_args(void) {
    printf("[TEST 3-WAY] test_diff_abi_mixed_stack_args (18 mixed parameters)... ");
    const char *src =
        "fn compute18(\n"
        "    i1: int, f1: float, i2: int, f2: float,\n"
        "    i3: int, f3: float, i4: int, f4: float,\n"
        "    i5: int, f5: float, i6: int, f6: float,\n"
        "    i7: int, f7: float, i8: int, f8: float,\n"
        "    f9: float, f10: float\n"
        ") -> float {\n"
        "    let isum: int = i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8; // 36\n"
        "    let fsum: float = f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10; // 55.0\n"
        "    if (isum == 36) { return fsum; }\n"
        "    return 0.0;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let res: float = compute18(\n"
        "        1, 1.0, 2, 2.0,\n"
        "        3, 3.0, 4, 4.0,\n"
        "        5, 5.0, 6, 6.0,\n"
        "        7, 7.0, 8, 8.0,\n"
        "        9.0, 10.0\n"
        "    );\n"
        "    print(res);\n"
        "    if (res == 55.0) { return 55; }\n"
        "    return 0;\n"
        "}\n";
    run_3way_differential_test(src, "diff_abi_mixed18", 55, true);
    printf("PASS\n");
}

static void test_diff_return_extensions(void) {
    printf("[TEST 3-WAY] test_diff_return_extensions (bool/char zero-extension)... ");
    const char *src =
        "fn get_true() -> bool { return true; }\n"
        "fn get_false() -> bool { return false; }\n"
        "fn get_char() -> char { return 'Z'; }\n"
        "fn main() -> int {\n"
        "    let b1: bool = get_true();\n"
        "    let b2: bool = get_false();\n"
        "    let c: char = get_char();\n"
        "    if (b1 && !b2 && c == 'Z') { return 42; }\n"
        "    return 0;\n"
        "}\n";
    run_3way_differential_test(src, "diff_return_ext", 42, true);
    printf("PASS\n");
}

static void test_diff_nested_stack_calls(void) {
    printf("[TEST 3-WAY] test_diff_nested_stack_calls (nested calls preserving stack alignment)... ");
    const char *src =
        "fn inner_sum8(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int) -> int {\n"
        "    return a + b + c + d + e + f + g + h;\n"
        "}\n"
        "fn outer_caller(x: int) -> int {\n"
        "    let s1: int = inner_sum8(x, x+1, x+2, x+3, x+4, x+5, x+6, x+7);\n"
        "    let s2: int = inner_sum8(1, 2, 3, 4, 5, 6, 7, 8);\n"
        "    return s1 + s2;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let r: int = outer_caller(10);\n"
        "    print(r); // 108 + 36 = 144\n"
        "    if (r == 144) { return 144; }\n"
        "    return 0;\n"
        "}\n";
    run_3way_differential_test(src, "diff_nested_stack", 144, true);
    printf("PASS\n");
}

static void test_diff_nested_control_flow(void) {
    printf("[TEST 3-WAY] test_diff_nested_control_flow (nested loops with early return)... ");
    const char *src =
        "fn search_matrix(target: int) -> int {\n"
        "    let r: int = 0;\n"
        "    while (r < 10) {\n"
        "        let c: int = 0;\n"
        "        while (c < 10) {\n"
        "            let val: int = r * 10 + c;\n"
        "            if (val == target) {\n"
        "                return val;\n"
        "            }\n"
        "            c = c + 1;\n"
        "        }\n"
        "        r = r + 1;\n"
        "    }\n"
        "    return -1;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let found: int = search_matrix(47);\n"
        "    print(found);\n"
        "    if (found == 47) { return 47; }\n"
        "    return 0;\n"
        "}\n";
    run_3way_differential_test(src, "diff_nested_flow", 47, true);
    printf("PASS\n");
}

static void test_diff_extreme_register_pressure(void) {
    printf("[TEST 3-WAY] test_diff_extreme_register_pressure (16 live ints + 16 live floats)... ");
    const char *src =
        "fn main() -> int {\n"
        "    let i0 = 1;  let i1 = 2;  let i2 = 3;  let i3 = 4;\n"
        "    let i4 = 5;  let i5 = 6;  let i6 = 7;  let i7 = 8;\n"
        "    let i8 = 9;  let i9 = 10; let i10 = 11; let i11 = 12;\n"
        "    let i12 = 13; let i13 = 14; let i14 = 15; let i15 = 16;\n"
        "    let f0: float = 1.5;  let f1: float = 2.5;\n"
        "    let f2: float = 3.5;  let f3: float = 4.5;\n"
        "    let f4: float = 5.5;  let f5: float = 6.5;\n"
        "    let f6: float = 7.5;  let f7: float = 8.5;\n"
        "    let f8: float = 9.5;  let f9: float = 10.5;\n"
        "    let f10: float = 11.5; let f11: float = 12.5;\n"
        "    let f12: float = 13.5; let f13: float = 14.5;\n"
        "    let f14: float = 15.5; let f15: float = 16.5;\n"
        "    let isum = i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10 + i11 + i12 + i13 + i14 + i15;\n"
        "    let fsum: float = f0 + f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10 + f11 + f12 + f13 + f14 + f15;\n"
        "    // isum = 136, fsum = 144.0\n"
        "    if (isum == 136 && fsum == 144.0) {\n"
        "        return 99;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    run_3way_differential_test(src, "diff_extreme_pressure", 99, true);
    printf("PASS\n");
}

static void test_phase3a_byte_encodings(void) {
    printf("[TEST PHASE 3A] test_phase3a_byte_encodings (50+ CPU instruction encodings)... ");
    uint8_t buf[64];
    size_t len = 0;

    /* 1. pushq %rbp -> 0x55 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_PUSH;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RBP;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 1 && buf[0] == 0x55);
    }
    /* 2. pushq %r12 -> 0x41 0x54 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_PUSH;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_R12;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0x41 && buf[1] == 0x54);
    }
    /* 3. popq %rbp -> 0x5D */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_POP;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RBP;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 1 && buf[0] == 0x5D);
    }
    /* 4. popq %r12 -> 0x41 0x5C */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_POP;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_R12;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0x41 && buf[1] == 0x5C);
    }
    /* 5. ret -> 0xC3 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_RET;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 1 && buf[0] == 0xC3);
    }
    /* 6. subq $16, %rsp -> 0x48 0x83 0xEC 0x10 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_SUB_IMM;
        inst.src.kind = X86_OPERAND_IMM;
        inst.src.imm = 16;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0x48 && buf[1] == 0x83 && buf[2] == 0xEC && buf[3] == 0x10);
    }
    /* 7. subq $128, %rsp -> 0x48 0x81 0xEC 0x80 0x00 0x00 0x00 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_SUB_IMM;
        inst.src.kind = X86_OPERAND_IMM;
        inst.src.imm = 128;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 7 && buf[0] == 0x48 && buf[1] == 0x81 && buf[2] == 0xEC && buf[3] == 0x80);
    }
    /* 8. addq $128, %rsp -> 0x48 0x81 0xC4 0x80 0x00 0x00 0x00 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_ADD_IMM;
        inst.src.kind = X86_OPERAND_IMM;
        inst.src.imm = 128;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 7 && buf[0] == 0x48 && buf[1] == 0x81 && buf[2] == 0xC4 && buf[3] == 0x80);
    }
    /* 9. movl $42, %eax -> 0xB8 0x2A 0x00 0x00 0x00 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        inst.src.kind = X86_OPERAND_IMM;
        inst.src.imm = 42;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 5 && buf[0] == 0xB8 && buf[1] == 42);
    }
    /* 10. movl $42, %r8d -> 0x41 0xB8 0x2A 0x00 0x00 0x00 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_R8;
        inst.dst.size = 4;
        inst.src.kind = X86_OPERAND_IMM;
        inst.src.imm = 42;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 6 && buf[0] == 0x41 && buf[1] == 0xB8 && buf[2] == 42);
    }
    /* 11. movq $42, %rax -> 0x48 0xC7 0xC0 0x2A 0x00 0x00 0x00 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 8;
        inst.src.kind = X86_OPERAND_IMM;
        inst.src.imm = 42;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 7 && buf[0] == 0x48 && buf[1] == 0xC7 && buf[2] == 0xC0 && buf[3] == 42);
    }
    /* 12. movl %eax, %edx -> 0x89 0xC2 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RAX;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RDX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0x89 && buf[1] == 0xC2);
    }
    /* 13. movl %r8d, %edx -> 0x44 0x89 0xC2 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_R8;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RDX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x44 && buf[1] == 0x89 && buf[2] == 0xC2);
    }
    /* 14. movq %rax, %rdx -> 0x48 0x89 0xC2 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RAX;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RDX;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x48 && buf[1] == 0x89 && buf[2] == 0xC2);
    }
    /* 15. movl -8(%rbp), %eax -> 0x8B 0x45 0xF8 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.src.kind = X86_OPERAND_MEM_BASE;
        inst.src.reg = X86_REG_RBP;
        inst.src.imm = -8;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x8B && buf[1] == 0x45 && buf[2] == 0xF8);
    }
    /* 16. movl 16(%rsp), %eax -> 0x8B 0x44 0x24 0x10 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOV;
        inst.src.kind = X86_OPERAND_MEM_BASE;
        inst.src.reg = X86_REG_RSP;
        inst.src.imm = 16;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0x8B && buf[1] == 0x44 && buf[2] == 0x24 && buf[3] == 0x10);
    }
    /* 17. movslq %eax, %rax -> 0x48 0x63 0xC0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOVSLQ;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RAX;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x48 && buf[1] == 0x63 && buf[2] == 0xC0);
    }
    /* 18. movzbl %al, %eax -> 0x0F 0xB6 0xC0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOVZBL;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RAX;
        inst.src.size = 1;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x0F && buf[1] == 0xB6 && buf[2] == 0xC0);
    }
    /* 19. cmoveq %rdx, %rax -> 0x48 0x0F 0x44 0xC2 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_CMOVE;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RDX;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0x48 && buf[1] == 0x0F && buf[2] == 0x44 && buf[3] == 0xC2);
    }
    /* 20. addl %edx, %eax -> 0x01 0xD0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_ADD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RDX;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0x01 && buf[1] == 0xD0);
    }
    /* 21. subl %edx, %eax -> 0x29 0xD0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_SUB;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RDX;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0x29 && buf[1] == 0xD0);
    }
    /* 22. imull %edx, %eax -> 0x0F 0xAF 0xC2 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_IMUL;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RDX;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x0F && buf[1] == 0xAF && buf[2] == 0xC2);
    }
    /* 23. cltd -> 0x99 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_CLTD;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 1 && buf[0] == 0x99);
    }
    /* 24. idivl %ecx -> 0xF7 0xF9 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_IDIV;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RCX;
        inst.src.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0xF7 && buf[1] == 0xF9);
    }
    /* 25. negl %eax -> 0xF7 0xD8 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_NEG;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0xF7 && buf[1] == 0xD8);
    }
    /* 26. xorl %eax, %eax -> 0x31 0xC0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_XOR;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RAX;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0x31 && buf[1] == 0xC0);
    }
    /* 27. cmpl %edx, %eax -> 0x39 0xD0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_CMP;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_RDX;
        inst.src.size = 4;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 2 && buf[0] == 0x39 && buf[1] == 0xD0);
    }
    /* 28. cmpl $0, %eax -> 0x83 0xF8 0x00 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_CMP_IMM;
        inst.src.kind = X86_OPERAND_IMM;
        inst.src.imm = 0;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 4;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x83 && buf[1] == 0xF8 && buf[2] == 0);
    }
    /* 29. sete %al -> 0x0F 0x94 0xC0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_SETCC;
        inst.cond = X86_CC_E;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 1;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x0F && buf[1] == 0x94 && buf[2] == 0xC0);
    }
    /* 30. setne %al -> 0x0F 0x95 0xC0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_SETCC;
        inst.cond = X86_CC_NE;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_RAX;
        inst.dst.size = 1;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 3 && buf[0] == 0x0F && buf[1] == 0x95 && buf[2] == 0xC0);
    }
    /* 31. movsd %xmm0, %xmm1 -> 0xF2 0x0F 0x10 0xC8 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOVSD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM0;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM1;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0xF2 && buf[1] == 0x0F && buf[2] == 0x10 && buf[3] == 0xC8);
    }
    /* 32. movsd %xmm15, %xmm0 -> 0xF2 0x41 0x0F 0x10 0xC7 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MOVSD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM15;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM0;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 5 && buf[0] == 0xF2 && buf[1] == 0x41 && buf[2] == 0x0F && buf[3] == 0x10 && buf[4] == 0xC7);
    }
    /* 33. addsd %xmm1, %xmm0 -> 0xF2 0x0F 0x58 0xC1 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_ADDSD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM1;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM0;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0xF2 && buf[1] == 0x0F && buf[2] == 0x58 && buf[3] == 0xC1);
    }
    /* 34. subsd %xmm1, %xmm0 -> 0xF2 0x0F 0x5C 0xC1 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_SUBSD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM1;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM0;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0xF2 && buf[1] == 0x0F && buf[2] == 0x5C && buf[3] == 0xC1);
    }
    /* 35. mulsd %xmm1, %xmm0 -> 0xF2 0x0F 0x59 0xC1 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_MULSD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM1;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM0;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0xF2 && buf[1] == 0x0F && buf[2] == 0x59 && buf[3] == 0xC1);
    }
    /* 36. divsd %xmm1, %xmm0 -> 0xF2 0x0F 0x5E 0xC1 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_DIVSD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM1;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM0;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0xF2 && buf[1] == 0x0F && buf[2] == 0x5E && buf[3] == 0xC1);
    }
    /* 37. xorpd %xmm0, %xmm0 -> 0x66 0x0F 0x57 0xC0 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_XORPD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM0;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM0;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0x66 && buf[1] == 0x0F && buf[2] == 0x57 && buf[3] == 0xC0);
    }
    /* 38. ucomisd %xmm1, %xmm0 -> 0x66 0x0F 0x2E 0xC1 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_UCOMISD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM1;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM0;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 4 && buf[0] == 0x66 && buf[1] == 0x0F && buf[2] == 0x2E && buf[3] == 0xC1);
    }
    /* 39. ucomisd %xmm15, %xmm14 -> 0x66 0x45 0x0F 0x2E 0xF7 */
    {
        X86Instr inst = {0};
        inst.op = X86_OP_UCOMISD;
        inst.src.kind = X86_OPERAND_REG;
        inst.src.reg = X86_REG_XMM15;
        inst.src.size = 8;
        inst.dst.kind = X86_OPERAND_REG;
        inst.dst.reg = X86_REG_XMM14;
        inst.dst.size = 8;
        len = x86_encode_single_instruction(&inst, buf, sizeof(buf), NULL);
        assert(len == 5 && buf[0] == 0x66 && buf[1] == 0x45 && buf[2] == 0x0F && buf[3] == 0x2E && buf[4] == 0xF7);
    }

    printf("PASS\n");
}

static void test_phase3a_object_structure_validation(void) {
    printf("[TEST PHASE 3A] test_phase3a_object_structure_validation (readelf & objdump)... ");
    ensure_build_dir();
    const char *src =
        "fn add(a: int, b: int) -> int {\n"
        "    return a + b;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let s = add(10, 20);\n"
        "    print(s);\n"
        "    return 0;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, arena, "struct_val.cco");
    analyze_scopes(ast, arena);

    IrModule *ir_mod = ir_lower_ast(ast, "struct_val.cco");
    assert(ir_mod != NULL);

    char *obj_err = NULL;
    bool ok = x86_64_emit_object_file(ir_mod, "build/struct_val.o", &obj_err);
    assert(ok);

    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);

    /* Verify ELF headers using readelf tools */
    assert(system("readelf -h build/struct_val.o > /dev/null 2>&1") == 0);
    assert(system("readelf -S build/struct_val.o > /dev/null 2>&1") == 0);
    assert(system("readelf -s build/struct_val.o > /dev/null 2>&1") == 0);
    assert(system("readelf -r build/struct_val.o > /dev/null 2>&1") == 0);
    assert(system("objdump -dr build/struct_val.o > /dev/null 2>&1") == 0);

    /* Verify linking with host gcc */
    assert(system("gcc -no-pie build/struct_val.o -o build/struct_val_bin -lm") == 0);
    assert(system("./build/struct_val_bin > /dev/null 2>&1") == 0);

    printf("PASS\n");
}

static void test_phase3a_deterministic_object_generation(void) {
    printf("[TEST PHASE 3A] test_phase3a_deterministic_object_generation (bit-exact .o identity)... ");
    ensure_build_dir();
    const char *src =
        "fn compute(x: float, y: float) -> float {\n"
        "    return (x * y) + 3.14159;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let res = compute(2.0, 4.5);\n"
        "    print(res);\n"
        "    return 42;\n"
        "}\n";

    /* Compile run 1 */
    {
        TokenArray tokens = lex_source(src);
        AstArena *arena = create_ast_arena();
        Parser parser = create_parser(tokens, arena);
        AstNode *ast = parse_program(&parser);
        desugar_top_level_program(ast, arena, "det.cco");
        analyze_scopes(ast, arena);
        IrModule *ir_mod = ir_lower_ast(ast, "det.cco");
        char *err = NULL;
        bool ok = x86_64_emit_object_file(ir_mod, "build/det1.o", &err);
        assert(ok);
        ir_module_free(ir_mod);
        free_ast_arena(arena);
        free_tokens(&tokens);
    }

    /* Compile run 2 */
    {
        TokenArray tokens = lex_source(src);
        AstArena *arena = create_ast_arena();
        Parser parser = create_parser(tokens, arena);
        AstNode *ast = parse_program(&parser);
        desugar_top_level_program(ast, arena, "det.cco");
        analyze_scopes(ast, arena);
        IrModule *ir_mod = ir_lower_ast(ast, "det.cco");
        char *err = NULL;
        bool ok = x86_64_emit_object_file(ir_mod, "build/det2.o", &err);
        assert(ok);
        ir_module_free(ir_mod);
        free_ast_arena(arena);
        free_tokens(&tokens);
    }

    /* Compare file contents byte-by-byte */
    FILE *f1 = fopen("build/det1.o", "rb");
    FILE *f2 = fopen("build/det2.o", "rb");
    assert(f1 && f2);

    fseek(f1, 0, SEEK_END);
    long sz1 = ftell(f1);
    fseek(f1, 0, SEEK_SET);

    fseek(f2, 0, SEEK_END);
    long sz2 = ftell(f2);
    fseek(f2, 0, SEEK_SET);

    assert(sz1 == sz2);
    char *buf1 = (char *)malloc(sz1);
    char *buf2 = (char *)malloc(sz2);
    assert(fread(buf1, 1, sz1, f1) == (size_t)sz1);
    assert(fread(buf2, 1, sz2, f2) == (size_t)sz2);
    fclose(f1);
    fclose(f2);

    assert(memcmp(buf1, buf2, sz1) == 0);
    free(buf1);
    free(buf2);

    printf("PASS\n");
}

int main(void) {
    printf("Running x86-64 Native Backend Unit & Differential Tests...\n");
    test_return_constant();
    test_arithmetic_operations();
    test_variables_and_assignment();
    test_comparisons();
    test_control_flow_branches_and_loops();
    test_functions_and_multiple_parameters();
    test_recursive_function();
    test_stack_behavior_deep_locals();
    test_differential_parity_with_c11_backend();

    /* Phase 2B Register Allocation Test Suite */
    test_regalloc_reuse();
    test_regalloc_pressure();
    test_regalloc_call_preservation();
    test_regalloc_multiple_calls();
    test_regalloc_loop_carried();
    test_regalloc_nested_branches();
    test_regalloc_recursion();
    test_regalloc_many_params();
    test_regalloc_large_locals();

    /* Phase 2C Floating-Point and XMM Register Test Suite */
    test_float_arithmetic();
    test_float_comparisons();
    test_float_constants_and_variables();
    test_float_abi_parameters();
    test_float_recursion_and_nesting();
    test_float_loop_carried();
    test_float_register_pressure_spilling();
    test_float_differential_parity();

    /* Phase 2D Stabilization + ABI + 3-Way Differential Test Suite */
    test_diff_abi_stack_args_int();
    test_diff_abi_stack_args_float();
    test_diff_abi_mixed_stack_args();
    test_diff_return_extensions();
    test_diff_nested_stack_calls();
    test_diff_nested_control_flow();
    test_diff_extreme_register_pressure();

    /* Phase 3A Direct Machine Code & ELF64 Object Generation Test Suite */
    test_phase3a_byte_encodings();
    test_phase3a_object_structure_validation();
    test_phase3a_deterministic_object_generation();

    printf("All x86-64 Native Backend tests passed successfully!\n");
    return 0;
}
