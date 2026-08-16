#include "../../src/lexer.h"
#include "../../src/ast.h"
#include "../../src/parser.h"
#include "../../src/scope_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_early_return_frees() {
    const char *src =
        "fn find_first_even(count: int) -> int {\n"
        "    let nums: int = alloc(int, count);\n"
        "    let temp: int = alloc(int, count);\n"
        "    for (let i: int = 0; i < count; i = i + 1) {\n"
        "        if (i == 4) {\n"
        "            return i;\n"
        "        }\n"
        "    }\n"
        "    return -1;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    analyze_scopes(prog, arena);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;

    // Inside body: nums and temp are allocated.
    // For loop -> if (i == 4) -> return i;
    AstNode *for_stmt = body->as.block.stmts[2];
    AstNode *for_body = for_stmt->as.for_stmt.body;
    AstNode *if_stmt = for_body->as.block.stmts[0];
    AstNode *then_b = if_stmt->as.if_stmt.then_b;
    AstNode *ret_stmt = then_b->as.block.stmts[0];

    assert(ret_stmt->type == NODE_RETURN);
    assert(ret_stmt->frees_count == 2); // nums and temp must be freed before early return!
    bool found_nums = false, found_temp = false;
    for (int i = 0; i < ret_stmt->frees_count; i++) {
        if (strcmp(ret_stmt->frees_to_emit[i], "nums") == 0) found_nums = true;
        if (strcmp(ret_stmt->frees_to_emit[i], "temp") == 0) found_temp = true;
    }
    assert(found_nums && found_temp);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_early_return_frees\n");
}

void test_ownership_transfer() {
    const char *src =
        "fn make_array(size: int) -> int {\n"
        "    let arr: int = alloc(int, size);\n"
        "    let unused: int = alloc(int, size);\n"
        "    return arr;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    analyze_scopes(prog, arena);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;
    AstNode *ret_stmt = body->as.block.stmts[2];

    assert(ret_stmt->type == NODE_RETURN);
    // arr is returned so ownership is transferred out. unused must be freed!
    assert(ret_stmt->frees_count == 1);
    assert(strcmp(ret_stmt->frees_to_emit[0], "unused") == 0);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_ownership_transfer\n");
}

int main() {
    printf("Running Scope Analysis Unit Tests...\n");
    test_early_return_frees();
    test_ownership_transfer();
    printf("All Scope Analysis tests passed!\n");
    return 0;
}
