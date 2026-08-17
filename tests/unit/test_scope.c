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
        if (strcmp(ret_stmt->frees_to_emit[i].var_name, "nums") == 0) found_nums = true;
        if (strcmp(ret_stmt->frees_to_emit[i].var_name, "temp") == 0) found_temp = true;
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
    assert(strcmp(ret_stmt->frees_to_emit[0].var_name, "unused") == 0);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_ownership_transfer\n");
}

void test_single_ownership_move() {
    const char *src =
        "class Point { x: int; y: int; }\n"
        "fn main() -> void {\n"
        "    let a: Point = Point { x: 1, y: 2 };\n"
        "    let b: Point = a;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    analyze_scopes(prog, arena);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;

    // End of block frees ONLY b (since a was moved into b)
    assert(body->releases_count == 1);
    assert(strcmp(body->releases_to_emit[0].var_name, "b") == 0);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_single_ownership_move\n");
}

void test_refcount_reassignment() {
    const char *src =
        "class Point { x: int; y: int; }\n"
        "fn main() -> void {\n"
        "    let a: Point = Point { x: 1, y: 2 };\n"
        "    a = Point { x: 3, y: 4 };\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    analyze_scopes(prog, arena);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;
    AstNode *assign_stmt = body->as.block.stmts[1];

    assert(assign_stmt->type == NODE_ASSIGN);
    assert(assign_stmt->as.assign.release_old == true);
    assert(strcmp(assign_stmt->as.assign.class_name, "Point") == 0);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_refcount_reassignment\n");
}

void test_refcount_early_return() {
    const char *src =
        "class Point { x: int; y: int; }\n"
        "fn make_point(skip: bool) -> Point {\n"
        "    let p: Point = Point { x: 1, y: 2 };\n"
        "    let dummy: Point = Point { x: 99, y: 99 };\n"
        "    if (skip) {\n"
        "        return p;\n"
        "    }\n"
        "    return dummy;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    analyze_scopes(prog, arena);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;
    AstNode *if_stmt = body->as.block.stmts[2];
    AstNode *then_b = if_stmt->as.if_stmt.then_b;
    AstNode *ret_p = then_b->as.block.stmts[0];

    assert(ret_p->type == NODE_RETURN);
    assert(ret_p->releases_count == 1);
    assert(strcmp(ret_p->releases_to_emit[0].var_name, "dummy") == 0);

    AstNode *ret_dummy = body->as.block.stmts[3];
    assert(ret_dummy->type == NODE_RETURN);
    assert(ret_dummy->releases_count == 1);
    assert(strcmp(ret_dummy->releases_to_emit[0].var_name, "p") == 0);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_refcount_early_return\n");
}

int main() {
    printf("Running Scope Analysis Unit Tests...\n");
    test_early_return_frees();
    test_ownership_transfer();
    test_single_ownership_move();
    test_refcount_reassignment();
    test_refcount_early_return();
    printf("All Scope Analysis tests passed!\n");
    return 0;
}
