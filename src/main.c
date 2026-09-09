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
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

static bool str_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (str_len < suffix_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static void ensure_parent_dir(const char *path) {
    if (!path) return;
    char tmp[PATH_MAX];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *dir = dirname(tmp);
    if (dir && strcmp(dir, ".") != 0 && strcmp(dir, "/") != 0) {
        char cmd[PATH_MAX + 64];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir);
        int r = system(cmd);
        (void)r;
    }
}

static void write_file(const char *path, const char *content) {
    ensure_parent_dir(path);
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
        printf("Usage: cco <source.cco> [-o output] [--emit-c] [--run] [--dump-tokens]\n");
        return 1;
    }

    const char *input_path = NULL;
    const char *output_arg = NULL;
    bool emit_c = false;
    bool run_binary = false;
    bool dump_tokens_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_arg = argv[++i];
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

    char output_c_path[PATH_MAX * 2] = {0};
    char binary_out_path[PATH_MAX * 2] = {0};
    bool compile_to_binary = false;

    if (output_arg) {
        if (str_ends_with(output_arg, ".c")) {
            snprintf(output_c_path, sizeof(output_c_path), "%s", output_arg);
            if (run_binary) {
                compile_to_binary = true;
                snprintf(binary_out_path, sizeof(binary_out_path), "build/cco_out");
            }
        } else {
            compile_to_binary = true;
            snprintf(binary_out_path, sizeof(binary_out_path), "%s", output_arg);

            struct stat st;
            if (stat("build", &st) == 0 && S_ISDIR(st.st_mode)) {
                const char *base = strrchr(output_arg, '/');
                base = base ? base + 1 : output_arg;
                snprintf(output_c_path, sizeof(output_c_path), "build/%s.c", base);
            } else {
                snprintf(output_c_path, sizeof(output_c_path), "%s.c", output_arg);
            }
        }
    } else {
        if (run_binary) {
            compile_to_binary = true;
            snprintf(output_c_path, sizeof(output_c_path), "build/output.c");
            snprintf(binary_out_path, sizeof(binary_out_path), "build/cco_out");
        } else if (emit_c) {
            snprintf(output_c_path, sizeof(output_c_path), "build/output.c");
            compile_to_binary = false;
        } else {
            compile_to_binary = true;
            char base_name[PATH_MAX];
            snprintf(base_name, sizeof(base_name), "%s", input_path);
            char *dot = strrchr(base_name, '.');
            if (dot && strcmp(dot, ".cco") == 0) {
                *dot = '\0';
            }
            snprintf(binary_out_path, sizeof(binary_out_path), "%s", base_name);

            struct stat st;
            if (stat("build", &st) == 0 && S_ISDIR(st.st_mode)) {
                const char *base = strrchr(base_name, '/');
                base = base ? base + 1 : base_name;
                snprintf(output_c_path, sizeof(output_c_path), "build/%s.c", base);
            } else {
                snprintf(output_c_path, sizeof(output_c_path), "%s.c", base_name);
            }
        }
    }

    // Write generated C code to file
    write_file(output_c_path, c_code);

    // Cleanup Compiler Memory
    free(c_code);
    free_ast_arena(arena);

    // 5. Optional compilation and execution with gcc/clang
    if (compile_to_binary) {
        const char *cc = getenv("CC");
        if (!cc || strlen(cc) == 0) cc = "gcc";

        ensure_parent_dir(binary_out_path);

        char cmd[PATH_MAX * 4 + 512];
        snprintf(cmd, sizeof(cmd), "%s -O3 -Wall -Wextra -std=c11 -Wno-unused-function -Wno-parentheses-equality \"%s\" -o \"%s\" -lm", cc, output_c_path, binary_out_path);
        int res = system(cmd);
        if (res != 0) {
            int exit_code = 1;
            #ifdef WEXITSTATUS
            if (WIFEXITED(res)) exit_code = WEXITSTATUS(res);
            #endif
            return exit_code;
        }

        if (run_binary) {
            char run_cmd[PATH_MAX * 2 + 64];
            if (binary_out_path[0] == '/' || (binary_out_path[0] == '.' && binary_out_path[1] == '/')) {
                snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", binary_out_path);
            } else {
                snprintf(run_cmd, sizeof(run_cmd), "./\"%s\"", binary_out_path);
            }
            int run_res = system(run_cmd);
            int exit_code = 0;
            #ifdef WEXITSTATUS
            if (WIFEXITED(run_res)) exit_code = WEXITSTATUS(run_res);
            #endif
            return exit_code;
        }

        return 0;
    }

    return 0;
}

