#define _POSIX_C_SOURCE 200809L
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "scope_analysis.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", path);
        exit(1);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed for file buffer\n");
        fclose(file);
        exit(1);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

static void write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", path);
        exit(1);
    }
    fputs(content, file);
    fclose(file);
}

#include "errors.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: cco <source.cco> [-o output.c] [--emit-c] [--run]\n");
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_c_path = "build/output.c";
    bool emit_c = false;
    bool run_binary = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_c_path = argv[++i];
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            emit_c = true;
        } else if (strcmp(argv[i], "--run") == 0) {
            run_binary = true;
        }
    }

    char *source = read_file(input_path);
    init_error_reporter(input_path, source);

    // 1. Lexical Analysis
    TokenArray tokens = lex_source(source);

    // 2. Parsing AST
    AstArena *arena = create_ast_arena();
    Parser parser = create_parser(tokens, arena);
    AstNode *ast = parse_program(&parser);

    // 3. Scope Analysis & Auto-free Annotation Pass
    analyze_scopes(ast, arena);

    // 4. Code Generation
    char *c_code = generate_c_code(ast, arena);

    if (emit_c) {
        printf("%s", c_code);
    }

    // Write generated C code to file
    write_file(output_c_path, c_code);

    // Cleanup Compiler Memory
    free(c_code);
    free_ast_arena(arena);
    free_tokens(&tokens);
    free(source);

    // 5. Optional compilation and execution with gcc
    if (run_binary) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "gcc -Wall -Wextra -std=c11 %s -o build/cco_out && ./build/cco_out", output_c_path);
        int res = system(cmd);
        return res;
    }

    return 0;
}
