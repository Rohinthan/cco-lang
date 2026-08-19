#define _POSIX_C_SOURCE 200809L
#include "../../src/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

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

void test_class_tokens() {
    const char *src = "class Point { self }";
    TokenArray arr = lex_source(src);
    assert(arr.count == 6); // 5 tokens + EOF
    assert(arr.tokens[0].type == TOKEN_CLASS);
    assert(arr.tokens[1].type == TOKEN_IDENT);
    assert(strcmp(arr.tokens[1].lexeme, "Point") == 0);
    assert(arr.tokens[2].type == TOKEN_LBRACE);
    assert(arr.tokens[3].type == TOKEN_SELF);
    assert(arr.tokens[4].type == TOKEN_RBRACE);
    assert(arr.tokens[5].type == TOKEN_EOF);
    free_tokens(&arr);
    printf("[PASS] test_class_tokens\n");
}

void test_dump_tokens_format() {
    const char *src = "fn main() -> int {\n    print(\"Hello\");\n    return 0;\n}\n";
    TokenArray arr = lex_source(src);
    int pipefd[2];
    assert(pipe(pipefd) == 0);
    int old_stdout = dup(fileno(stdout));
    dup2(pipefd[1], fileno(stdout));
    close(pipefd[1]);

    dump_tokens(&arr);
    fflush(stdout);

    dup2(old_stdout, fileno(stdout));
    close(old_stdout);

    char buf[1024];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    close(pipefd[0]);

    const char *expected =
        "1:1 KEYWORD fn\n"
        "1:4 IDENT main\n"
        "1:8 SYMBOL (\n"
        "1:9 SYMBOL )\n"
        "1:11 SYMBOL ->\n"
        "1:14 KEYWORD int\n"
        "1:18 SYMBOL {\n"
        "2:5 KEYWORD print\n"
        "2:10 SYMBOL (\n"
        "2:11 STRING Hello\n"
        "2:18 SYMBOL )\n"
        "2:19 SYMBOL ;\n"
        "3:5 KEYWORD return\n"
        "3:12 INT 0\n"
        "3:13 SYMBOL ;\n"
        "4:1 SYMBOL }\n"
        "5:1 EOF\n";

    assert(strcmp(buf, expected) == 0);
    free_tokens(&arr);
    printf("[PASS] test_dump_tokens_format\n");
}

int main() {
    printf("Running Lexer Unit Tests...\n");
    test_basic_tokens();
    test_string_and_comments();
    test_class_tokens();
    test_dump_tokens_format();
    printf("All Lexer tests passed!\n");
    return 0;
}
