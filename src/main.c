#define _POSIX_C_SOURCE 200809L
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "module_resolver.h"
#include "trait_resolver.h"
#include "scope_analysis.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        printf("Usage: cco <source.cco> [-o output.c] [--emit-c] [--run] [--dump-tokens]\n");
        return 1;
    }

    const char *input_path = NULL;
    const char *output_c_path = "build/output.c";
    bool emit_c = false;
    bool run_binary = false;
    bool dump_tokens_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_c_path = argv[++i];
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            emit_c = true;
        } else if (strcmp(argv[i], "--run") == 0) {
            run_binary = true;
        } else if (strcmp(argv[i], "--dump-tokens") == 0) {
            dump_tokens_mode = true;
        } else if (argv[i][0] != '-') {
            if (!input_path) {
                input_path = argv[i];
            }
        }
    }

    if (!input_path) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    if (dump_tokens_mode) {
        FILE *file = fopen(input_path, "rb");
        if (!file) {
            fprintf(stderr, "Error: Could not open file '%s'\n", input_path);
            return 1;
        }
        fseek(file, 0L, SEEK_END);
        size_t file_size = ftell(file);
        rewind(file);
        char *buffer = (char *)malloc(file_size + 1);
        if (!buffer) {
            fclose(file);
            fprintf(stderr, "Error: Memory allocation failed\n");
            return 1;
        }
        size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
        buffer[bytes_read] = '\0';
        fclose(file);

        TokenArray tokens = lex_source(buffer);
        dump_tokens(&tokens);

        free(buffer);
        free_tokens(&tokens);
        return 0;
    }

    AstArena *arena = create_ast_arena();
    AstNode *ast = resolve_program(input_path, arena);

    // Trait conformance checking and monomorphization
    resolve_and_monomorphize_traits(ast, arena);

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

    // 5. Optional compilation and execution with gcc
    if (run_binary) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "gcc -O3 -Wall -Wextra -std=c11 %s -o build/cco_out -lm && ./build/cco_out", output_c_path);
        int res = system(cmd);
        return res;
    }

    return 0;
}
