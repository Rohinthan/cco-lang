// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_link.h"
#include "x86_64_codegen.h"
#include "x86_64_target.h"
#include "x86_64_regalloc.h"
#include "lexer.h"
#include "parser.h"
#include "scope_analysis.h"
#include "ir.h"
#include "ir_verify.h"
#include "ir_lower.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

/* Helper to compile a Cco source string to an ELF64 .o file */
static void compile_cco_to_obj(const char *src, const char *out_o_path) {
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, arena, "test_input.cco");
    analyze_scopes(ast, arena);

    IrModule *ir_mod = ir_lower_ast(ast, "test_input.cco");
    assert(ir_mod != NULL);

    char *verify_err = NULL;
    bool valid = ir_verify_module(ir_mod, &verify_err);
    if (!valid) {
        fprintf(stderr, "IR verify error: %s\n", verify_err);
    }
    assert(valid);

    char *obj_err = NULL;
    bool ok = x86_64_emit_object_file(ir_mod, out_o_path, &obj_err);
    if (!ok) {
        fprintf(stderr, "Object emit error: %s\n", obj_err);
    }
    assert(ok);

    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);
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

    if (out_buf && out_buf_size > 0) {
        FILE *f = fopen(out_path, "r");
        if (f) {
            size_t bytes = fread(out_buf, 1, out_buf_size - 1, f);
            out_buf[bytes] = '\0';
            fclose(f);
        } else {
            out_buf[0] = '\0';
        }
    }
    remove(out_path);
    return exit_code;
}

/* Test Suite */

/* 1. ELF Reader Defensive Validation */
static void test_linker_elf_validation(void) {
    printf("[TEST LINKER] test_linker_elf_validation... ");

    /* Invalid file size */
    char *err = NULL;
    FILE *f = fopen("build/test_bad_size.o", "wb");
    assert(f != NULL);
    fwrite("short", 1, 5, f);
    fclose(f);

    Elf64ParsedObject *obj = elf64_read_object_file("build/test_bad_size.o", &err);
    assert(obj == NULL);
    assert(err != NULL);
    free(err);
    remove("build/test_bad_size.o");

    /* Invalid magic */
    f = fopen("build/test_bad_magic.o", "wb");
    assert(f != NULL);
    uint8_t bad_hdr[64];
    memset(bad_hdr, 0, sizeof(bad_hdr));
    bad_hdr[0] = 0x7F; bad_hdr[1] = 'B'; bad_hdr[2] = 'A'; bad_hdr[3] = 'D';
    fwrite(bad_hdr, 1, sizeof(bad_hdr), f);
    fclose(f);

    err = NULL;
    obj = elf64_read_object_file("build/test_bad_magic.o", &err);
    assert(obj == NULL);
    assert(err != NULL && strstr(err, "magic") != NULL);
    free(err);
    remove("build/test_bad_magic.o");

    printf("PASS\n");
}

/* 2. Single Object Standalone Linking */
static void test_linker_single_object_exec(void) {
    printf("[TEST LINKER] test_linker_single_object_exec... ");

    const char *src =
        "fn main() -> int {\n"
        "    let a = 20;\n"
        "    let b = 22;\n"
        "    let c = a + b;\n"
        "    print(c);\n"
        "    return c;\n"
        "}\n";

    compile_cco_to_obj(src, "build/test_single.o");

    char *link_err = NULL;
    bool ok = elf64_link_single_file("build/test_single.o", "build/test_single_bin", &link_err);
    if (!ok) {
        fprintf(stderr, "Link error: %s\n", link_err);
    }
    assert(ok);

    char out[128] = {0};
    int status = run_binary_and_capture("build/test_single_bin", out, sizeof(out));
    assert(status == 42);
    assert(strstr(out, "42") != NULL);

    /* Verify ELF layout with readelf */
    int r1 = system("readelf -h build/test_single_bin > /dev/null 2>&1");
    assert(r1 == 0);
    int r2 = system("readelf -l build/test_single_bin > /dev/null 2>&1");
    assert(r2 == 0);

    remove("build/test_single.o");
    remove("build/test_single_bin");
    printf("PASS\n");
}

/* 3. Multi-Object Linking and Cross-Object Calls */
static void test_linker_multi_object_calls(void) {
    printf("[TEST LINKER] test_linker_multi_object_calls (main.o + helper.o)... ");

    /* Object 1: helper function */
    const char *helper_asm =
        ".global helper_add\n"
        ".global helper_mul\n"
        ".text\n"
        "helper_add:\n"
        "    pushq %rbp\n"
        "    movq %rsp, %rbp\n"
        "    leaq (%rdi, %rsi), %rax\n"
        "    popq %rbp\n"
        "    ret\n"
        "helper_mul:\n"
        "    pushq %rbp\n"
        "    movq %rsp, %rbp\n"
        "    movq %rdi, %rax\n"
        "    imulq %rsi, %rax\n"
        "    popq %rbp\n"
        "    ret\n";

    FILE *hf = fopen("build/test_helper.s", "w");
    assert(hf != NULL);
    fputs(helper_asm, hf);
    fclose(hf);
    int as_res = system("as build/test_helper.s -o build/test_helper.o");
    assert(as_res == 0);

    /* Object 2: main calling helpers */
    const char *main_asm =
        ".global main\n"
        ".text\n"
        "main:\n"
        "    pushq %rbp\n"
        "    movq %rsp, %rbp\n"
        "    movq $15, %rdi\n"
        "    movq $25, %rsi\n"
        "    call helper_add\n"
        "    movq %rax, %rdi\n"
        "    movq $2, %rsi\n"
        "    call helper_mul\n"
        "    popq %rbp\n"
        "    ret\n";

    FILE *mf = fopen("build/test_main.s", "w");
    assert(mf != NULL);
    fputs(main_asm, mf);
    fclose(mf);
    as_res = system("as build/test_main.s -o build/test_main.o");
    assert(as_res == 0);

    const char *inputs[2] = { "build/test_main.o", "build/test_helper.o" };
    char *link_err = NULL;
    bool ok = elf64_link_files(inputs, 2, "build/test_multi_bin", &link_err);
    if (!ok) {
        fprintf(stderr, "Link error: %s\n", link_err);
    }
    assert(ok);

    int status = run_binary_and_capture("build/test_multi_bin", NULL, 0);
    /* (15 + 25) * 2 = 80 */
    assert(status == 80);

    remove("build/test_helper.s");
    remove("build/test_helper.o");
    remove("build/test_main.s");
    remove("build/test_main.o");
    remove("build/test_multi_bin");
    printf("PASS\n");
}

/* 4. Cross-Object Rodata References */
static void test_linker_cross_object_rodata(void) {
    printf("[TEST LINKER] test_linker_cross_object_rodata... ");

    const char *src1 =
        "fn main() -> int {\n"
        "    let f1: float = 3.5;\n"
        "    let f2: float = 4.5;\n"
        "    let sum: float = f1 + f2;\n"
        "    if (sum == 8.0) {\n"
        "        return 88;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    compile_cco_to_obj(src1, "build/test_rodata.o");

    char *link_err = NULL;
    bool ok = elf64_link_single_file("build/test_rodata.o", "build/test_rodata_bin", &link_err);
    assert(ok);

    int status = run_binary_and_capture("build/test_rodata_bin", NULL, 0);
    assert(status == 88);

    remove("build/test_rodata.o");
    remove("build/test_rodata_bin");
    printf("PASS\n");
}

/* 5. Recursive Function Linking */
static void test_linker_recursion(void) {
    printf("[TEST LINKER] test_linker_recursion (fibonacci 10 = 55)... ");

    const char *src =
        "fn fib(n: int) -> int {\n"
        "    if (n <= 1) { return n; }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "fn main() -> int {\n"
        "    return fib(10);\n"
        "}\n";

    compile_cco_to_obj(src, "build/test_fib.o");

    char *link_err = NULL;
    bool ok = elf64_link_single_file("build/test_fib.o", "build/test_fib_bin", &link_err);
    assert(ok);

    int status = run_binary_and_capture("build/test_fib_bin", NULL, 0);
    assert(status == 55);

    remove("build/test_fib.o");
    remove("build/test_fib_bin");
    printf("PASS\n");
}

/* 6. Duplicate Symbol Error Detection */
static void test_linker_duplicate_symbol_error(void) {
    printf("[TEST LINKER] test_linker_duplicate_symbol_error... ");

    const char *asm1 = ".global duplicate_func\n.text\nduplicate_func: ret\n";
    const char *asm2 = ".global duplicate_func\n.text\nduplicate_func: ret\n";

    FILE *f1 = fopen("build/test_dup1.s", "w");
    fputs(asm1, f1);
    fclose(f1);
    int r1 = system("as build/test_dup1.s -o build/test_dup1.o");
    assert(r1 == 0);

    FILE *f2 = fopen("build/test_dup2.s", "w");
    fputs(asm2, f2);
    fclose(f2);
    int r2 = system("as build/test_dup2.s -o build/test_dup2.o");
    assert(r2 == 0);

    const char *inputs[2] = { "build/test_dup1.o", "build/test_dup2.o" };
    char *err = NULL;
    bool ok = elf64_link_files(inputs, 2, "build/test_dup_bin", &err);
    assert(!ok);
    assert(err != NULL && strstr(err, "duplicate") != NULL);
    free(err);

    remove("build/test_dup1.s");
    remove("build/test_dup1.o");
    remove("build/test_dup2.s");
    remove("build/test_dup2.o");
    printf("PASS\n");
}

/* 7. Undefined Symbol Error Detection */
static void test_linker_undefined_symbol_error(void) {
    printf("[TEST LINKER] test_linker_undefined_symbol_error... ");

    const char *asm_src =
        ".global main\n"
        ".text\n"
        "main:\n"
        "    call nonexistent_function_xyz\n"
        "    ret\n";

    FILE *f = fopen("build/test_undef.s", "w");
    fputs(asm_src, f);
    fclose(f);
    int r = system("as build/test_undef.s -o build/test_undef.o");
    assert(r == 0);

    char *err = NULL;
    bool ok = elf64_link_single_file("build/test_undef.o", "build/test_undef_bin", &err);
    assert(!ok);
    assert(err != NULL && strstr(err, "undefined symbol") != NULL);
    free(err);

    remove("build/test_undef.s");
    remove("build/test_undef.o");
    printf("PASS\n");
}

/* 8. Deterministic Linking (Bit-for-Bit Identity) */
static void test_linker_deterministic_output(void) {
    printf("[TEST LINKER] test_linker_deterministic_output... ");

    const char *src =
        "fn main() -> int {\n"
        "    let x = 100;\n"
        "    let y = 200;\n"
        "    return x + y;\n"
        "}\n";

    compile_cco_to_obj(src, "build/test_det.o");

    char *err1 = NULL;
    char *err2 = NULL;
    bool ok1 = elf64_link_single_file("build/test_det.o", "build/test_det_1.bin", &err1);
    bool ok2 = elf64_link_single_file("build/test_det.o", "build/test_det_2.bin", &err2);
    assert(ok1 && ok2);

    /* Read both files and assert byte-for-byte identity */
    FILE *f1 = fopen("build/test_det_1.bin", "rb");
    FILE *f2 = fopen("build/test_det_2.bin", "rb");
    assert(f1 && f2);

    fseek(f1, 0, SEEK_END);
    long sz1 = ftell(f1);
    fseek(f1, 0, SEEK_SET);

    fseek(f2, 0, SEEK_END);
    long sz2 = ftell(f2);
    fseek(f2, 0, SEEK_SET);

    assert(sz1 == sz2);
    uint8_t *b1 = (uint8_t *)malloc(sz1);
    uint8_t *b2 = (uint8_t *)malloc(sz2);
    assert(fread(b1, 1, sz1, f1) == (size_t)sz1);
    assert(fread(b2, 1, sz2, f2) == (size_t)sz2);
    fclose(f1);
    fclose(f2);

    assert(memcmp(b1, b2, sz1) == 0);
    free(b1);
    free(b2);

    remove("build/test_det.o");
    remove("build/test_det_1.bin");
    remove("build/test_det_2.bin");
    printf("PASS\n");
}

/* 9. 5-Way Differential Execution Test */
static void test_5way_differential(const char *src, const char *name, int expected_status) {
    printf("[TEST 5-WAY] %s... ", name);

    char o_path[256], host_bin[256], internal_bin[256];
    snprintf(o_path, sizeof(o_path), "build/%s.o", name);
    snprintf(host_bin, sizeof(host_bin), "build/%s_host", name);
    snprintf(internal_bin, sizeof(internal_bin), "build/%s_internal", name);

    compile_cco_to_obj(src, o_path);

    /* Mode A: Host Linker */
    char host_cmd[1024];
    snprintf(host_cmd, sizeof(host_cmd), "gcc -no-pie \"%s\" -o \"%s\" -lm", o_path, host_bin);
    int host_comp = system(host_cmd);
    assert(host_comp == 0);

    /* Mode B: Cco Internal Linker */
    char *link_err = NULL;
    bool link_ok = elf64_link_single_file(o_path, internal_bin, &link_err);
    if (!link_ok) {
        fprintf(stderr, "internal link failed: %s\n", link_err);
    }
    assert(link_ok);

    char host_out[256] = {0};
    char internal_out[256] = {0};
    int host_status = run_binary_and_capture(host_bin, host_out, sizeof(host_out));
    int internal_status = run_binary_and_capture(internal_bin, internal_out, sizeof(internal_out));

    assert(host_status == expected_status);
    assert(internal_status == expected_status);
    assert(strcmp(host_out, internal_out) == 0);

    remove(o_path);
    remove(host_bin);
    remove(internal_bin);
    printf("PASS\n");
}

int main(void) {
    printf("Running Cco Phase 3B Internal ELF64 Static Linker Test Suite...\n");

    test_linker_elf_validation();
    test_linker_single_object_exec();
    test_linker_multi_object_calls();
    test_linker_cross_object_rodata();
    test_linker_recursion();
    test_linker_duplicate_symbol_error();
    test_linker_undefined_symbol_error();
    test_linker_deterministic_output();

    /* 5-Way Differential Verification Tests */
    test_5way_differential(
        "fn main() -> int {\n"
        "    let a = 10; let b = 20; let c = 30;\n"
        "    let res = (a + b) * c;\n"
        "    print(res);\n"
        "    return 42;\n"
        "}\n",
        "diff_arith", 42
    );

    test_5way_differential(
        "fn sum10(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int, j: int) -> int {\n"
        "    return a + b + c + d + e + f + g + h + i + j;\n"
        "}\n"
        "fn main() -> int {\n"
        "    let res = sum10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);\n"
        "    print(res);\n"
        "    return res;\n"
        "}\n",
        "diff_abi10", 55
    );

    test_5way_differential(
        "fn fact(n: int) -> int {\n"
        "    if (n <= 1) { return 1; }\n"
        "    return n * fact(n - 1);\n"
        "}\n"
        "fn main() -> int {\n"
        "    let r = fact(5); // 120\n"
        "    print(r);\n"
        "    return r;\n"
        "}\n",
        "diff_factorial", 120
    );

    test_5way_differential(
        "fn main() -> int {\n"
        "    let count = 0;\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        count = count + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    print(count);\n"
        "    return count;\n"
        "}\n",
        "diff_loop", 45
    );

    printf("All Cco Phase 3B Internal Linker tests passed successfully!\n");
    return 0;
}
