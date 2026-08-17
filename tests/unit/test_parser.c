#include "../../src/lexer.h"
#include "../../src/ast.h"
#include "../../src/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_parse_simple_func() {
    const char *src = "fn add(a: int, b: int) -> int { return a + b; }";
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);

    AstNode *prog = parse_program(&parser);
    assert(prog != NULL);
    assert(prog->type == NODE_PROGRAM);
    assert(prog->as.program.count == 1);

    AstNode *fn = prog->as.program.functions[0];
    assert(fn->type == NODE_FUNCTION);
    assert(strcmp(fn->as.function.name, "add") == 0);
    assert(fn->as.function.param_count == 2);
    assert(strcmp(fn->as.function.param_names[0], "a") == 0);
    assert(fn->as.function.param_types[0] == TY_INT);
    assert(strcmp(fn->as.function.param_names[1], "b") == 0);
    assert(fn->as.function.param_types[1] == TY_INT);
    assert(fn->as.function.return_type == TY_INT);

    AstNode *body = fn->as.function.body;
    assert(body->type == NODE_BLOCK);
    assert(body->as.block.count == 1);
    assert(body->as.block.stmts[0]->type == NODE_RETURN);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_parse_simple_func\n");
}

void test_parse_alloc_and_for() {
    const char *src = "fn main() -> void { let arr: int = alloc(int, 10); for (let i: int = 0; i < 10; i = i + 1) { print(i); } }";
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);

    AstNode *prog = parse_program(&parser);
    assert(prog != NULL);
    assert(prog->as.program.count == 1);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;
    assert(body->as.block.count == 2);

    AstNode *let_stmt = body->as.block.stmts[0];
    assert(let_stmt->type == NODE_LET);
    assert(strcmp(let_stmt->as.let.name, "arr") == 0);
    assert(let_stmt->as.let.value->type == NODE_ALLOC);
    assert(let_stmt->as.let.value->as.alloc.elem_type == TY_INT);

    AstNode *for_stmt = body->as.block.stmts[1];
    assert(for_stmt->type == NODE_FOR);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_parse_alloc_and_for\n");
}

void test_parse_class() {
    const char *src =
        "class Point {\n"
        "    x: int;\n"
        "    y: int;\n"
        "    fn dist(self, other: Point) -> float {\n"
        "        return 0.0;\n"
        "    }\n"
        "}\n"
        "fn main() -> void {\n"
        "    let p: Point = Point { x: 1, y: 2 };\n"
        "    let d: float = p.dist(p);\n"
        "}\n";
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);

    AstNode *prog = parse_program(&parser);
    assert(prog != NULL);
    assert(prog->as.program.class_count == 1);
    assert(prog->as.program.count == 1);

    AstNode *cls = prog->as.program.classes[0];
    assert(cls->type == NODE_CLASS);
    assert(strcmp(cls->as.class_decl.name, "Point") == 0);
    assert(cls->as.class_decl.field_count == 2);
    assert(cls->as.class_decl.method_count == 1);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *let_p = fn->as.function.body->as.block.stmts[0];
    assert(let_p->as.let.var_type == TY_CLASS);
    assert(strcmp(let_p->as.let.class_name, "Point") == 0);
    assert(let_p->as.let.value->type == NODE_NEW);

    AstNode *let_d = fn->as.function.body->as.block.stmts[1];
    assert(let_d->as.let.value->type == NODE_METHOD_CALL);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_parse_class\n");
}

int main() {
    printf("Running Parser Unit Tests...\n");
    test_parse_simple_func();
    test_parse_alloc_and_for();
    test_parse_class();
    printf("All Parser tests passed!\n");
    return 0;
}
