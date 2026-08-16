#include "../../src/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_basic_tokens() {
    const char *src = "fn main() -> int { let x: int = 10; let p = alloc(int, x); print(x); return 0; }";
    TokenArray arr = lex_source(src);
    assert(arr.count > 0);
    assert(arr.tokens[0].type == TOKEN_FN);
    assert(arr.tokens[1].type == TOKEN_IDENT);
    assert(strcmp(arr.tokens[1].lexeme, "main") == 0);
    assert(arr.tokens[2].type == TOKEN_LPAREN);
    assert(arr.tokens[3].type == TOKEN_RPAREN);
    assert(arr.tokens[4].type == TOKEN_ARROW);
    assert(arr.tokens[5].type == TOKEN_TYPE_INT);
    assert(arr.tokens[6].type == TOKEN_LBRACE);
    assert(arr.tokens[7].type == TOKEN_LET);
    free_tokens(&arr);
    printf("[PASS] test_basic_tokens\n");
}

void test_string_and_comments() {
    const char *src = "// comment here\nlet s: string = \"hello \\\"world\\\"\";";
    TokenArray arr = lex_source(src);
    assert(arr.tokens[0].type == TOKEN_LET);
    assert(arr.tokens[1].type == TOKEN_IDENT);
    assert(arr.tokens[2].type == TOKEN_COLON);
    assert(arr.tokens[3].type == TOKEN_TYPE_STRING);
    assert(arr.tokens[4].type == TOKEN_ASSIGN);
    assert(arr.tokens[5].type == TOKEN_STRING_LIT);
    assert(strcmp(arr.tokens[5].lexeme, "hello \\\"world\\\"") == 0);
    free_tokens(&arr);
    printf("[PASS] test_string_and_comments\n");
}

int main() {
    printf("Running Lexer Unit Tests...\n");
    test_basic_tokens();
    test_string_and_comments();
    printf("All Lexer tests passed!\n");
    return 0;
}
