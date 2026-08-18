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

void test_parse_borrowed_param() {
    const char *src = "fn f(p: &Point, q: Point) -> void {}";
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);

    AstNode *prog = parse_program(&parser);
    assert(prog != NULL);
    assert(prog->as.program.count == 1);

    AstNode *fn = prog->as.program.functions[0];
    assert(fn->as.function.param_count == 2);
    assert(fn->as.function.param_is_borrowed[0] == true);
    assert(fn->as.function.param_is_borrowed[1] == false);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_parse_borrowed_param\n");
}

void test_parse_map() {
    const char *src = "fn main() -> void { let m: map[string]int = map_new(string, int); }";
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);

    AstNode *prog = parse_program(&parser);
    assert(prog != NULL);
    assert(prog->as.program.count == 1);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *let_stmt = fn->as.function.body->as.block.stmts[0];
    assert(let_stmt->type == NODE_LET);
    assert(let_stmt->as.let.is_map == true);
    assert(let_stmt->as.let.key_type == TY_STRING);
    assert(let_stmt->as.let.var_type == TY_INT);
    assert(let_stmt->as.let.value->type == NODE_ALLOC);
    assert(let_stmt->as.let.value->as.alloc.is_map == true);
    assert(let_stmt->as.let.value->as.alloc.key_type == TY_STRING);
    assert(let_stmt->as.let.value->as.alloc.elem_type == TY_INT);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_parse_map\n");
}

void test_parse_enum() {
    const char *src =
        "enum NodeKind {\n"
        "    Number { value: int },\n"
        "    Add { left: NodeKind, right: NodeKind },\n"
        "    Ident { name: string },\n"
        "    Eof,\n"
        "}\n"
        "fn main() -> void {\n"
        "    let n: NodeKind = NodeKind.Number { value: 5 };\n"
        "    let e: NodeKind = NodeKind.Eof;\n"
        "    match n {\n"
        "        NodeKind.Number { value } => {\n"
        "            print(value);\n"
        "        }\n"
        "        _ => {}\n"
        "    }\n"
        "}\n";
    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);

    AstNode *prog = parse_program(&parser);
    assert(prog != NULL);
    assert(prog->as.program.enum_count == 1);
    assert(prog->as.program.count == 1);

    AstNode *en = prog->as.program.enums[0];
    assert(en->type == NODE_ENUM);
    assert(strcmp(en->as.enum_decl.name, "NodeKind") == 0);
    assert(en->as.enum_decl.variant_count == 4);

    AstNode *v0 = en->as.enum_decl.variants[0];
    assert(strcmp(v0->as.variant_decl.name, "Number") == 0);
    assert(v0->as.variant_decl.is_unit == false);
    assert(v0->as.variant_decl.field_count == 1);
    assert(strcmp(v0->as.variant_decl.fields[0]->as.field.name, "value") == 0);
    assert(v0->as.variant_decl.fields[0]->as.field.type == TY_INT);

    AstNode *v3 = en->as.enum_decl.variants[3];
    assert(strcmp(v3->as.variant_decl.name, "Eof") == 0);
    assert(v3->as.variant_decl.is_unit == true);

    AstNode *fn = prog->as.program.functions[0];
    AstNode *stmt0 = fn->as.function.body->as.block.stmts[0];
    assert(stmt0->type == NODE_LET);
    assert(stmt0->as.let.value->type == NODE_NEW);
    assert(stmt0->as.let.value->as.new_expr.constructs_enum == true);
    assert(strcmp(stmt0->as.let.value->as.new_expr.class_name, "NodeKind") == 0);
    assert(strcmp(stmt0->as.let.value->as.new_expr.variant_name, "Number") == 0);
    assert(stmt0->as.let.value->as.new_expr.field_count == 1);

    AstNode *stmt1 = fn->as.function.body->as.block.stmts[1];
    assert(stmt1->type == NODE_LET);
    assert(stmt1->as.let.value->type == NODE_NEW);
    assert(stmt1->as.let.value->as.new_expr.constructs_enum == true);
    assert(strcmp(stmt1->as.let.value->as.new_expr.variant_name, "Eof") == 0);
    assert(stmt1->as.let.value->as.new_expr.field_count == 0);

    AstNode *stmt2 = fn->as.function.body->as.block.stmts[2];
    assert(stmt2->type == NODE_MATCH);
    assert(stmt2->as.match_stmt.arm_count == 2);
    assert(stmt2->as.match_stmt.arms[0]->as.match_arm.is_wildcard == false);
    assert(strcmp(stmt2->as.match_stmt.arms[0]->as.match_arm.variant_name, "Number") == 0);
    assert(stmt2->as.match_stmt.arms[0]->as.match_arm.bind_count == 1);
    assert(strcmp(stmt2->as.match_stmt.arms[0]->as.match_arm.bind_names[0], "value") == 0);
    assert(stmt2->as.match_stmt.arms[1]->as.match_arm.is_wildcard == true);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_parse_enum\n");
}

int main() {
    printf("Running Parser Unit Tests...\n");
    test_parse_simple_func();
    test_parse_alloc_and_for();
    test_parse_class();
    test_parse_borrowed_param();
    test_parse_map();
    test_parse_enum();
    printf("All Parser tests passed!\n");
    return 0;
}
