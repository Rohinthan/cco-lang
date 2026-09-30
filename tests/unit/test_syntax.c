// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "../../src/lexer.h"
#include "../../src/ast.h"
#include "../../src/parser.h"
#include "../../src/module_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_syntax_comments(void) {
    const char *src =
        "# This is a Python-style single line comment\n"
        "let x = 10; # trailing comment\n"
        "# Another comment\n"
        "let y = 20;\n";

    TokenArray tokens = lex_source(src);
    assert(tokens.count > 0);

    // Should only have: let, x, =, 10, ;, let, y, =, 20, ;, EOF
    assert(tokens.tokens[0].type == TOKEN_LET);
    assert(tokens.tokens[1].type == TOKEN_IDENT);
    assert(strcmp(tokens.tokens[1].lexeme, "x") == 0);
    assert(tokens.tokens[2].type == TOKEN_ASSIGN);
    assert(tokens.tokens[3].type == TOKEN_INT_LIT);
    assert(tokens.tokens[4].type == TOKEN_SEMICOLON);

    assert(tokens.tokens[5].type == TOKEN_LET);
    assert(tokens.tokens[6].type == TOKEN_IDENT);
    assert(strcmp(tokens.tokens[6].lexeme, "y") == 0);
    assert(tokens.tokens[7].type == TOKEN_ASSIGN);
    assert(tokens.tokens[8].type == TOKEN_INT_LIT);
    assert(tokens.tokens[9].type == TOKEN_SEMICOLON);
    assert(tokens.tokens[10].type == TOKEN_EOF);

    free_tokens(&tokens);
    printf("[PASS] test_syntax_comments\n");
}

void test_syntax_boolean_keywords(void) {
    const char *src =
        "fn check(a: bool, b: bool, c: bool) -> bool {\n"
        "    if a and b or not c {\n"
        "        return true;\n"
        "    }\n"
        "    return false;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    assert(prog->as.program.count == 1);
    AstNode *fn = prog->as.program.functions[0];
    assert(fn->type == NODE_FUNCTION);

    AstNode *body = fn->as.function.body;
    assert(body->type == NODE_BLOCK);
    AstNode *if_node = body->as.block.stmts[0];
    assert(if_node->type == NODE_IF);

    // The condition should be (a and b) or (not c)
    AstNode *cond = if_node->as.if_stmt.cond;
    assert(cond->type == NODE_BINARY);
    assert(strcmp(cond->as.binary.op, "||") == 0); // Desugared to canonical ||

    // Left child is (a and b) -> op "&&"
    AstNode *left = cond->as.binary.left;
    assert(left->type == NODE_BINARY);
    assert(strcmp(left->as.binary.op, "&&") == 0); // Desugared to canonical &&

    // Right child is (not c) -> op "!"
    AstNode *right = cond->as.binary.right;
    assert(right->type == NODE_UNARY);
    assert(strcmp(right->as.unary.op, "!") == 0); // Desugared to canonical !

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_boolean_keywords\n");
}

void test_syntax_unparenthesized_conditionals(void) {
    const char *src =
        "fn test_flow(x: int) -> int {\n"
        "    if x > 10 {\n"
        "        return 1;\n"
        "    } else if x > 0 {\n"
        "        return 2;\n"
        "    } else {\n"
        "        return 0;\n"
        "    }\n"
        "}\n"
        "fn test_loop(n: int) -> int {\n"
        "    let count = 0;\n"
        "    while count < n {\n"
        "        count += 1;\n"
        "    }\n"
        "    return count;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    assert(prog->as.program.count == 2);

    // Function 1: if-else without parentheses
    AstNode *fn1 = prog->as.program.functions[0];
    AstNode *if1 = fn1->as.function.body->as.block.stmts[0];
    assert(if1->type == NODE_IF);
    assert(if1->as.if_stmt.cond->type == NODE_BINARY);
    assert(strcmp(if1->as.if_stmt.cond->as.binary.op, ">") == 0);
    assert(if1->as.if_stmt.then_b->type == NODE_BLOCK);
    assert(if1->as.if_stmt.else_b != NULL);
    assert(if1->as.if_stmt.else_b->type == NODE_IF); // else if

    // Function 2: while without parentheses
    AstNode *fn2 = prog->as.program.functions[1];
    AstNode *while_node = fn2->as.function.body->as.block.stmts[1];
    assert(while_node->type == NODE_WHILE);
    assert(while_node->as.while_stmt.cond->type == NODE_BINARY);
    assert(strcmp(while_node->as.while_stmt.cond->as.binary.op, "<") == 0);
    assert(while_node->as.while_stmt.body->type == NODE_BLOCK);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_unparenthesized_conditionals\n");
}

void test_syntax_range_loops(void) {
    const char *src =
        "fn count_sum(limit: int) -> int {\n"
        "    let total = 0;\n"
        "    for i in 0..limit {\n"
        "        total += i;\n"
        "    }\n"
        "    return total;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    AstNode *fn = prog->as.program.functions[0];
    AstNode *for_node = fn->as.function.body->as.block.stmts[1];

    // Range loop desugars to NODE_FOR
    assert(for_node->type == NODE_FOR);

    // 1. init is 'let i: int = 0'
    AstNode *init = for_node->as.for_stmt.init;
    assert(init != NULL);
    assert(init->type == NODE_LET);
    assert(strcmp(init->as.let.name, "i") == 0);
    assert(init->as.let.var_type == TY_INT);
    assert(init->as.let.value->type == NODE_LITERAL);
    assert(init->as.let.value->as.literal.val.i == 0);

    // 2. cond is 'i < limit'
    AstNode *cond = for_node->as.for_stmt.cond;
    assert(cond != NULL);
    assert(cond->type == NODE_BINARY);
    assert(strcmp(cond->as.binary.op, "<") == 0);
    assert(cond->as.binary.left->type == NODE_IDENT);
    assert(strcmp(cond->as.binary.left->as.ident.name, "i") == 0);
    assert(cond->as.binary.right->type == NODE_IDENT);
    assert(strcmp(cond->as.binary.right->as.ident.name, "limit") == 0);

    // 3. step is 'i++' (desugared to i = i + 1)
    AstNode *step = for_node->as.for_stmt.step;
    assert(step != NULL);
    assert(step->type == NODE_ASSIGN || step->type == NODE_COMPOUND_ASSIGN);
    if (step->type == NODE_ASSIGN) {
        assert(strcmp(step->as.assign.name, "i") == 0);
        assert(step->as.assign.value->type == NODE_BINARY);
        assert(strcmp(step->as.assign.value->as.binary.op, "+") == 0);
    } else {
        assert(strcmp(step->as.compound_assign.op, "++") == 0);
    }

    // 4. body
    assert(for_node->as.for_stmt.body != NULL);
    assert(for_node->as.for_stmt.body->type == NODE_BLOCK);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_range_loops\n");
}

void test_syntax_expression_bodied_functions(void) {
    const char *src =
        "fn square(x: int) -> int = x * x;\n"
        "class Calculator {\n"
        "    scale: int;\n"
        "    fn mult(self, v: int) -> int = self.scale * v;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    assert(prog->as.program.count == 1); // 1 standalone function
    assert(prog->as.program.class_count == 1); // 1 class

    // Standalone function
    AstNode *fn = prog->as.program.functions[0];
    assert(strcmp(fn->as.function.name, "square") == 0);
    assert(fn->as.function.body->type == NODE_BLOCK);
    assert(fn->as.function.body->as.block.count == 1);
    assert(fn->as.function.body->as.block.stmts[0]->type == NODE_RETURN);
    AstNode *ret_expr = fn->as.function.body->as.block.stmts[0]->as.return_stmt.value;
    assert(ret_expr->type == NODE_BINARY);
    assert(strcmp(ret_expr->as.binary.op, "*") == 0);

    // Class method
    AstNode *cls = prog->as.program.classes[0];
    assert(cls->as.class_decl.method_count == 1);
    AstNode *method = cls->as.class_decl.methods[0];
    assert(strcmp(method->as.method.name, "mult") == 0);
    assert(method->as.method.body->type == NODE_BLOCK);
    assert(method->as.method.body->as.block.count == 1);
    assert(method->as.method.body->as.block.stmts[0]->type == NODE_RETURN);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_expression_bodied_functions\n");
}

void test_syntax_top_level_program(void) {
    const char *src =
        "let x = 100;\n"
        "let y = 200;\n"
        "print(x + y);\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    assert(prog->as.program.top_level_count == 3);
    assert(prog->as.program.count == 0); // No explicit functions yet

    // Desugar top level program into main
    desugar_top_level_program(prog, arena, "test_top_level.cco");

    assert(prog->as.program.count == 1);
    AstNode *synth_main = prog->as.program.functions[0];
    assert(strcmp(synth_main->as.function.name, "main") == 0);
    assert(synth_main->as.function.return_type == TY_INT);
    assert(synth_main->as.function.body->type == NODE_BLOCK);
    // 3 statements + synthetic return 0 = 4 statements
    assert(synth_main->as.function.body->as.block.count == 4);
    assert(synth_main->as.function.body->as.block.stmts[3]->type == NODE_RETURN);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_top_level_program\n");
}

void test_syntax_variable_declarations(void) {
    const char *src =
        "fn main() -> int {\n"
        "    let a = 10;\n"
        "    let b: int = 20;\n"
        "    a += 5;\n"
        "    b -= 2;\n"
        "    a *= 2;\n"
        "    b /= 2;\n"
        "    a %= 3;\n"
        "    return a + b;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;

    // let a = 10 (has_explicit_type = false)
    assert(body->as.block.stmts[0]->type == NODE_LET);
    assert(!body->as.block.stmts[0]->as.let.has_explicit_type);

    // let b: int = 20 (has_explicit_type = true)
    assert(body->as.block.stmts[1]->type == NODE_LET);
    assert(body->as.block.stmts[1]->as.let.has_explicit_type);

    // Compound assignments (may be desugared to NODE_ASSIGN)
    for (int i = 2; i <= 6; i++) {
        assert(body->as.block.stmts[i]->type == NODE_ASSIGN || body->as.block.stmts[i]->type == NODE_COMPOUND_ASSIGN);
    }

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_variable_declarations\n");
}

void test_syntax_legacy_compatibility(void) {
    const char *src =
        "// Legacy comment\n"
        "/* Legacy block comment */\n"
        "fn main() -> int {\n"
        "    let x: int = 10;\n"
        "    if ((x > 5) && (x < 20)) {\n"
        "        x = x + 1;\n"
        "    }\n"
        "    while (x < 15) {\n"
        "        x = x + 1;\n"
        "    }\n"
        "    for (let i: int = 0; i < 5; i++) {\n"
        "        x = x + 1;\n"
        "    }\n"
        "    return x;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;

    // Legacy if with outer parens
    AstNode *if_node = body->as.block.stmts[1];
    assert(if_node->type == NODE_IF);

    // Legacy while with outer parens
    AstNode *while_node = body->as.block.stmts[2];
    assert(while_node->type == NODE_WHILE);

    // Legacy C-style for loop
    AstNode *for_node = body->as.block.stmts[3];
    assert(for_node->type == NODE_FOR);
    assert(for_node->as.for_stmt.init->type == NODE_LET);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_legacy_compatibility\n");
}

void test_syntax_declaration_assignment(void) {
    const char *src =
        "x = 10;\n"
        "y = 20;\n"
        "print(x + y);\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    assert(prog->as.program.top_level_count == 3);

    desugar_top_level_program(prog, arena, "test_decl_assign.cco");

    assert(prog->as.program.count == 1);
    AstNode *synth_main = prog->as.program.functions[0];
    AstNode *body = synth_main->as.function.body;

    // x = 10 -> desugared to NODE_LET with inferred TY_INT
    AstNode *stmt0 = body->as.block.stmts[0];
    assert(stmt0->type == NODE_LET);
    assert(strcmp(stmt0->as.let.name, "x") == 0);
    assert(stmt0->as.let.var_type == TY_INT);
    assert(!stmt0->as.let.has_explicit_type);

    // y = 20 -> desugared to NODE_LET with inferred TY_INT
    AstNode *stmt1 = body->as.block.stmts[1];
    assert(stmt1->type == NODE_LET);
    assert(strcmp(stmt1->as.let.name, "y") == 0);
    assert(stmt1->as.let.var_type == TY_INT);
    assert(!stmt1->as.let.has_explicit_type);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_declaration_assignment\n");
}

void test_syntax_explicit_type_annotation(void) {
    const char *src =
        "fn main() -> int {\n"
        "    x: int = 10;\n"
        "    val: float = 3.14;\n"
        "    name: string = \"Cco\";\n"
        "    return x;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;

    // x: int = 10
    AstNode *stmt0 = body->as.block.stmts[0];
    assert(stmt0->type == NODE_LET);
    assert(strcmp(stmt0->as.let.name, "x") == 0);
    assert(stmt0->as.let.var_type == TY_INT);
    assert(stmt0->as.let.has_explicit_type);

    // val: float = 3.14
    AstNode *stmt1 = body->as.block.stmts[1];
    assert(stmt1->type == NODE_LET);
    assert(strcmp(stmt1->as.let.name, "val") == 0);
    assert(stmt1->as.let.var_type == TY_FLOAT);
    assert(stmt1->as.let.has_explicit_type);

    // name: string = "Cco"
    AstNode *stmt2 = body->as.block.stmts[2];
    assert(stmt2->type == NODE_LET);
    assert(strcmp(stmt2->as.let.name, "name") == 0);
    assert(stmt2->as.let.var_type == TY_STRING);
    assert(stmt2->as.let.has_explicit_type);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_explicit_type_annotation\n");
}

void test_syntax_reassignment_type_preservation(void) {
    const char *src =
        "fn main() -> int {\n"
        "    x = 10;\n"
        "    x = 20;\n"
        "    x += 5;\n"
        "    return x;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;

    // First: x = 10 -> NODE_LET
    assert(body->as.block.stmts[0]->type == NODE_LET);
    assert(strcmp(body->as.block.stmts[0]->as.let.name, "x") == 0);

    // Second: x = 20 -> NODE_ASSIGN
    assert(body->as.block.stmts[1]->type == NODE_ASSIGN);
    assert(strcmp(body->as.block.stmts[1]->as.assign.name, "x") == 0);

    // Third: x += 5 -> desugared to NODE_ASSIGN
    assert(body->as.block.stmts[2]->type == NODE_ASSIGN);
    assert(strcmp(body->as.block.stmts[2]->as.assign.name, "x") == 0);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_reassignment_type_preservation\n");
}

void test_syntax_nested_block_scoping(void) {
    const char *src =
        "fn main() -> int {\n"
        "    x = 10;\n"
        "    if true {\n"
        "        y = 20;\n"
        "        x = 30;\n"
        "    }\n"
        "    return x;\n"
        "}\n";

    TokenArray tokens = lex_source(src);
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *prog = parse_program(&parser);

    assert(prog != NULL);
    AstNode *fn = prog->as.program.functions[0];
    AstNode *body = fn->as.function.body;

    // x = 10 -> NODE_LET
    assert(body->as.block.stmts[0]->type == NODE_LET);

    // if block
    AstNode *if_stmt = body->as.block.stmts[1];
    assert(if_stmt->type == NODE_IF);
    AstNode *then_b = if_stmt->as.if_stmt.then_b;
    assert(then_b->type == NODE_BLOCK);

    // y = 20 inside then_b -> NODE_LET (new variable in inner scope)
    assert(then_b->as.block.stmts[0]->type == NODE_LET);
    assert(strcmp(then_b->as.block.stmts[0]->as.let.name, "y") == 0);

    // x = 30 inside then_b -> NODE_ASSIGN (modifies x from outer scope)
    assert(then_b->as.block.stmts[1]->type == NODE_ASSIGN);
    assert(strcmp(then_b->as.block.stmts[1]->as.assign.name, "x") == 0);

    free_ast_arena(arena);
    free_tokens(&tokens);
    printf("[PASS] test_syntax_nested_block_scoping\n");
}

int main(void) {
    printf("Running Syntax Ergonomics Unit Tests...\n");
    test_syntax_comments();
    test_syntax_boolean_keywords();
    test_syntax_unparenthesized_conditionals();
    test_syntax_range_loops();
    test_syntax_expression_bodied_functions();
    test_syntax_top_level_program();
    test_syntax_variable_declarations();
    test_syntax_legacy_compatibility();
    test_syntax_declaration_assignment();
    test_syntax_explicit_type_annotation();
    test_syntax_reassignment_type_preservation();
    test_syntax_nested_block_scoping();
    printf("All Syntax Ergonomics Unit Tests Passed!\n");
    return 0;
}
