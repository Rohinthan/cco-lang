// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir.h"
#include "ir_verify.h"
#include "ir_print.h"
#include "ir_lower.h"
#include "ir_codegen_c.h"
#include "lexer.h"
#include "parser.h"
#include "scope_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_ir_construction_and_printing(void) {
    printf("[TEST] test_ir_construction_and_printing... ");
    IrModule *mod = ir_module_create("math_test");
    assert(mod != NULL);

    IrFunction *fn = ir_function_create(mod, "add_mul", ir_type_i32(mod->arena));
    ir_function_add_param(fn, "a", ir_type_i32(mod->arena));
    ir_function_add_param(fn, "b", ir_type_i32(mod->arena));

    IrBasicBlock *entry = ir_block_create(fn, "entry");

    IrBuilder builder;
    ir_builder_init(&builder, mod);
    ir_builder_set_insert_block(&builder, entry);

    /* %0 = add i32 %arg.a, %arg.b */
    IrInstruction *add_inst = ir_emit_binary(&builder, IR_OP_ADD, ir_type_i32(mod->arena),
                                             fn->param_values[0], fn->param_values[1]);
    /* %1 = const i32 2 */
    IrInstruction *two = ir_emit_const_int(&builder, ir_type_i32(mod->arena), 2);

    /* %2 = mul i32 %0, %1 */
    IrInstruction *mul_inst = ir_emit_binary(&builder, IR_OP_MUL, ir_type_i32(mod->arena),
                                             add_inst->result, two->result);

    /* ret i32 %2 */
    ir_emit_ret(&builder, mul_inst->result);

    /* Verify IR */
    char *err = NULL;
    bool valid = ir_verify_module(mod, &err);
    if (!valid) {
        fprintf(stderr, "\nVerification error:\n%s\n", err);
    }
    assert(valid);
    assert(err == NULL);

    /* Print IR */
    char *dump = ir_print_module(mod);
    assert(dump != NULL);
    assert(strstr(dump, "fn @add_mul(%arg.a: i32, %arg.b: i32) -> i32 {") != NULL);
    assert(strstr(dump, "add i32 %arg.a, %arg.b") != NULL);
    assert(strstr(dump, "mul i32") != NULL);
    assert(strstr(dump, "ret i32") != NULL);

    free(dump);
    ir_module_free(mod);
    printf("PASS\n");
}

static void test_ir_verifier_rejection(void) {
    printf("[TEST] test_ir_verifier_rejection... ");

    /* Case 1: Type mismatch in addition (i32 + bool) */
    {
        IrModule *mod = ir_module_create("err_mod1");
        IrFunction *fn = ir_function_create(mod, "bad_add", ir_type_i32(mod->arena));
        IrBasicBlock *entry = ir_block_create(fn, "entry");
        IrBuilder builder;
        ir_builder_init(&builder, mod);
        ir_builder_set_insert_block(&builder, entry);

        IrInstruction *c_int = ir_emit_const_int(&builder, ir_type_i32(mod->arena), 10);
        IrInstruction *c_bool = ir_emit_const_bool(&builder, true);
        ir_emit_binary(&builder, IR_OP_ADD, ir_type_i32(mod->arena), c_int->result, c_bool->result);
        ir_emit_ret_void(&builder);

        char *err = NULL;
        bool ok = ir_verify_module(mod, &err);
        assert(!ok);
        assert(err != NULL);
        assert(strstr(err, "Cco IR verification failed:") != NULL);
        assert(strstr(err, "operand types do not match") != NULL);

        free(err);
        ir_module_free(mod);
    }

    /* Case 2: Unterminated basic block */
    {
        IrModule *mod = ir_module_create("err_mod2");
        IrFunction *fn = ir_function_create(mod, "unterminated", ir_type_void(mod->arena));
        IrBasicBlock *entry = ir_block_create(fn, "entry");
        IrBuilder builder;
        ir_builder_init(&builder, mod);
        ir_builder_set_insert_block(&builder, entry);

        ir_emit_const_int(&builder, ir_type_i32(mod->arena), 10);
        /* No terminator instruction emitted! */

        char *err = NULL;
        bool ok = ir_verify_module(mod, &err);
        assert(!ok);
        assert(err != NULL);
        assert(strstr(err, "does not end with a terminator") != NULL);

        free(err);
        ir_module_free(mod);
    }

    /* Case 3: Incompatible return type */
    {
        IrModule *mod = ir_module_create("err_mod3");
        IrFunction *fn = ir_function_create(mod, "bad_ret", ir_type_i32(mod->arena));
        IrBasicBlock *entry = ir_block_create(fn, "entry");
        IrBuilder builder;
        ir_builder_init(&builder, mod);
        ir_builder_set_insert_block(&builder, entry);

        IrInstruction *c_bool = ir_emit_const_bool(&builder, false);
        ir_emit_ret(&builder, c_bool->result);

        char *err = NULL;
        bool ok = ir_verify_module(mod, &err);
        assert(!ok);
        assert(err != NULL);
        assert(strstr(err, "returned value type (bool) does not match function return type (i32)") != NULL);

        free(err);
        ir_module_free(mod);
    }

    printf("PASS\n");
}

static void test_ast_lowering_and_verification(void) {
    printf("[TEST] test_ast_lowering_and_verification... ");
    const char *source =
        "fn calculate(x: int) -> int {\n"
        "    let result = 0;\n"
        "    if (x > 10) {\n"
        "        result = x * 2;\n"
        "    } else {\n"
        "        result = x + 5;\n"
        "    }\n"
        "    return result;\n"
        "}\n"
        "\n"
        "let a = 15;\n"
        "let b = calculate(a);\n"
        "print(b);\n";

    TokenArray tokens = lex_source(source);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, arena, "test_input.cco");
    analyze_scopes(ast, arena);

    /* Lower AST to Cco IR */
    IrModule *ir_mod = ir_lower_ast(ast, "test_input.cco");
    assert(ir_mod != NULL);

    /* Verify lowered IR */
    char *err = NULL;
    bool valid = ir_verify_module(ir_mod, &err);
    if (!valid) {
        fprintf(stderr, "\nVerification failure:\n%s\n", err);
    }
    assert(valid);
    assert(err == NULL);

    /* Check function existence */
    IrFunction *calc_fn = ir_module_find_function(ir_mod, "calculate");
    assert(calc_fn != NULL);
    assert(calc_fn->param_count == 1);
    assert(calc_fn->block_count >= 4); /* entry, if.then, if.else, if.merge */

    IrFunction *main_fn = ir_module_find_function(ir_mod, "main");
    assert(main_fn != NULL);

    char *dump = ir_print_module(ir_mod);
    assert(dump != NULL);
    assert(strstr(dump, "fn @calculate") != NULL);
    assert(strstr(dump, "condbr") != NULL);
    assert(strstr(dump, "call i32 @calculate") != NULL);
    assert(strstr(dump, "print") != NULL);

    free(dump);
    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("PASS\n");
}

static void test_ir_codegen_c_pipeline(void) {
    printf("[TEST] test_ir_codegen_c_pipeline... ");
    const char *source =
        "fn fib(n: int) -> int {\n"
        "    if (n <= 1) {\n"
        "        return n;\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "\n"
        "let res = fib(7);\n"
        "print(res);\n";

    TokenArray tokens = lex_source(source);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);
    desugar_top_level_program(ast, arena, "fib_test.cco");
    analyze_scopes(ast, arena);

    IrModule *ir_mod = ir_lower_ast(ast, "fib_test.cco");
    assert(ir_mod != NULL);

    char *err = NULL;
    assert(ir_verify_module(ir_mod, &err));

    char *c_code = ir_generate_c(ir_mod);
    assert(c_code != NULL);
    assert(strstr(c_code, "int32_t fib(") != NULL);
    assert(strstr(c_code, "int32_t main(") != NULL);
    assert(strstr(c_code, "goto __bb_") != NULL);

    free(c_code);
    ir_module_free(ir_mod);
    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("PASS\n");
}

int main(void) {
    printf("Running Cco IR Unit Tests...\n");
    test_ir_construction_and_printing();
    test_ir_verifier_rejection();
    test_ast_lowering_and_verification();
    test_ir_codegen_c_pipeline();
    printf("All Cco IR Unit Tests passed successfully!\n");
    return 0;
}
