// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "module_resolver.h"
#include "trait_resolver.h"
#include "scope_analysis.h"
#include "codegen.h"
#include "ir.h"
#include "ir_verify.h"
#include "ir_print.h"
#include "ir_lower.h"
#include "ir_codegen_c.h"
#include "x86_64_target.h"
#include "x86_64_regalloc.h"
#include "x86_64_codegen.h"
#include "x86_64_link.h"
#include "ir_opt.h"
#include "ir_dominance.h"
#include "ir_ssa.h"
#include "ir_loop.h"
#include "ir_ssa_opt.h"
#include "ir_ipa.h"
#include "ir_profile.h"
#include "timing.h"
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


static IrModule *build_ir_module_timed(AstNode *ast, const char *input_path, int opt_level, bool use_ssa, const CcoProfile *profile, CcoPassTimings *timings) {
    double t_low = timings ? cco_get_time_ms() : 0.0;
    IrModule *ir_mod = ir_lower_ast(ast, input_path);
    if (!ir_mod) return NULL;
    ir_mod->profile = profile;
    char *verify_err = NULL;
    if (!ir_verify_module(ir_mod, &verify_err)) {
        fprintf(stderr, "%s", verify_err ? verify_err : "Cco IR verification failed\n");
        if (verify_err) free(verify_err);
        ir_module_free(ir_mod);
        return NULL;
    }
    if (timings) timings->ir_lower_ms += (cco_get_time_ms() - t_low);

    if (use_ssa) {
        double t_ssa = timings ? cco_get_time_ms() : 0.0;
        IrSsaOptions ssa_opts = {
            .dump_ssa = false,
            .dump_ssa_opt = false,
            .optimize_ssa = (opt_level > 0),
            .verify_ssa = true,
            .verbose = false,
            .opt_level = opt_level,
            .enable_inlining = (opt_level >= 2),
            .inline_threshold = IR_DEFAULT_INLINE_THRESHOLD,
            .enable_dfe = (opt_level >= 2),
            .dump_callgraph = false,
            .profile = profile
        };
        char *ssa_err = NULL;
        if (!ir_ssa_pipeline_module(ir_mod, &ssa_opts, NULL, &ssa_err)) {
            fprintf(stderr, "SSA pipeline error: %s\n", ssa_err ? ssa_err : "unknown");
            if (ssa_err) free(ssa_err);
            ir_module_free(ir_mod);
            return NULL;
        }
        double ssa_elapsed = timings ? (cco_get_time_ms() - t_ssa) : 0.0;
        if (timings) {
            if (opt_level >= 2) {
                timings->ipa_ms += ssa_elapsed * 0.35;
                timings->ssa_ms += ssa_elapsed * 0.65;
            } else {
                timings->ssa_ms += ssa_elapsed;
            }
        }
    }

    if (opt_level > 0) {
        double t_opt = timings ? cco_get_time_ms() : 0.0;
        IrOptOptions opt_opts = {
            .opt_level = opt_level,
            .dump_passes = false,
            .verify_each_pass = true,
            .verbose = false
        };
        char *opt_err = NULL;
        if (!ir_optimize_module(ir_mod, &opt_opts, &opt_err)) {
            fprintf(stderr, "IR optimization error: %s\n", opt_err ? opt_err : "unknown");
            if (opt_err) free(opt_err);
            ir_module_free(ir_mod);
            return NULL;
        }
        if (timings) timings->opt_ms += (cco_get_time_ms() - t_opt);
    }
    return ir_mod;
}

static IrModule *build_ir_module(AstNode *ast, const char *input_path, int opt_level, bool use_ssa, const CcoProfile *profile) {
    return build_ir_module_timed(ast, input_path, opt_level, use_ssa, profile, NULL);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf(
            "Usage: cco <source.cco> [options]\n"
            "\n"
            "Compilation modes:\n"
            "  (default)              Transpile to C and compile via gcc/clang\n"
            "  --use-native           Compile to native x86-64 (via .o + external linker)\n"
            "  --use-internal-linker  Compile to native x86-64 (fully internal linker, no gcc)\n"
            "  --use-ir               Transpile using Cco IR -> C path\n"
            "\n"
            "Output options:\n"
            "  -o <file>              Output file path\n"
            "  --emit-c               Print generated C code to stdout\n"
            "  --emit-ir              Print Cco IR to stdout and exit\n"
            "  --emit-asm             Emit x86-64 assembly (.s) and exit\n"
            "  --emit-object          Emit ELF64 relocatable object (.o) and exit\n"
            "  --run                  Run binary after compilation\n"
            "\n"
            "Optimization:\n"
            "  -O0                    No optimization (default)\n"
            "  -O1, -O, --opt         Enable IR optimization (constant folding, DCE, etc.)\n"
            "\n"
            "SSA / Analysis:\n"
            "  --ssa                  Enable SSA construction, optimize, and lower before codegen\n"
            "  --dump-ssa             Dump SSA IR to stdout (in SSA form, before destruction)\n"
            "  --dump-ssa-opt         Dump SSA IR after optimizations to stdout\n"
            "  --dump-loops           Dump natural loop analysis\n"
            "  --dump-callgraph       Dump call graph and function analysis\n"
            "  --dump-code-stats      Dump comprehensive compiler code statistics\n"
            "\n"
            "Profile-Guided Optimization (PGO):\n"
            "  --profile-generate[=<f>]  Instrument program to collect runtime profile data\n"
            "  --profile-use=<file>      Use runtime profile data to guide optimization\n"
            "  --profile-file <file>     Specify profile output/input file path (default: cco.profile)\n"
            "  --dump-profile            Dump parsed profile data to stdout and exit\n"
            "  --dump-inlining           Dump inlining and PGO decisions to stdout and exit\n"
            "\n"
            "Debug / Diagnostics:\n"
            "  --dump-regalloc        Dump register allocation result to stdout\n"
            "  --dump-tokens          Dump lexer token stream to stdout and exit\n"
            "\n"
            "Multi-object linking (cco-link mode):\n"
            "  cco-link a.o b.o -o out\n"
            "  cco --link-internal a.o b.o -o out\n"
            "\n"
            "Examples:\n"
            "  cco hello.cco -o hello --run\n"
            "  cco hello.cco --use-native -o hello --run\n"
            "  cco hello.cco --use-internal-linker -o hello --run\n"
            "  cco hello.cco --ssa -O1 -o hello --run\n"
            "  cco hello.cco --profile-generate -o hello && ./hello\n"
            "  cco hello.cco --profile-use cco.profile -O2 -o hello\n"
            "  cco hello.cco --emit-ir\n"
            "  cco hello.cco --dump-ssa\n"
        );
        return 1;
    }

    /* Handle --help anywhere in args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf(
                "Usage: cco <source.cco> [options]\n"
                "\n"
                "Compilation modes:\n"
                "  (default)              Transpile to C and compile via gcc/clang\n"
                "  --use-native           Compile to native x86-64 (via .o + external linker)\n"
                "  --use-internal-linker  Compile to native x86-64 (fully internal linker, no gcc)\n"
                "  --use-ir               Transpile using Cco IR -> C path\n"
                "\n"
                "Output options:\n"
                "  -o <file>              Output file path\n"
                "  --emit-c               Print generated C code to stdout\n"
                "  --emit-ir              Print Cco IR to stdout and exit\n"
                "  --emit-asm             Emit x86-64 assembly (.s) and exit\n"
                "  --emit-object          Emit ELF64 relocatable object (.o) and exit\n"
                "  --run                  Run binary after compilation\n"
                "\n"
                "Optimization:\n"
                "  -O0                    No optimization (default)\n"
                "  -O1, -O, --opt         Enable IR optimization (constant folding, DCE, etc.)\n"
                "  -O2                    Enable SSA optimizations, function inlining & DFE\n"
                "\n"
                "SSA / Analysis:\n"
                "  --ssa                  Enable SSA construction, optimize, and lower before codegen\n"
                "  --dump-ssa             Dump SSA IR to stdout (in SSA form, before destruction)\n"
                "  --dump-ssa-opt         Dump SSA IR after optimizations to stdout\n"
                "  --dump-loops           Dump natural loop analysis\n"
                "  --dump-callgraph       Dump call graph and function analysis\n"
                "  --dump-code-stats      Dump comprehensive compiler code statistics\n"
                "\n"
                "Profile-Guided Optimization (PGO):\n"
                "  --profile-generate[=<f>]  Instrument program to collect runtime profile data\n"
                "  --profile-use=<file>      Use runtime profile data to guide optimization\n"
                "  --profile-file <file>     Specify profile output/input file path (default: cco.profile)\n"
                "  --dump-profile            Dump parsed profile data to stdout and exit\n"
                "  --dump-inlining           Dump inlining and PGO decisions to stdout and exit\n"
                "\n"
                "Debug / Diagnostics:\n"
                "  --dump-regalloc        Dump register allocation result to stdout\n"
                "  --dump-tokens          Dump lexer token stream to stdout and exit\n"
                "\n"
                "Multi-object linking (cco-link mode):\n"
                "  cco-link a.o b.o -o out\n"
                "  cco --link-internal a.o b.o -o out\n"
                "\n"
                "Examples:\n"
                "  cco hello.cco -o hello --run\n"
                "  cco hello.cco --use-native -o hello --run\n"
                "  cco hello.cco --use-internal-linker -o hello --run\n"
                "  cco hello.cco --ssa -O1 -o hello --run\n"
                "  cco hello.cco --profile-generate -o hello && ./hello\n"
                "  cco hello.cco --profile-use cco.profile -O2 -o hello\n"
                "  cco hello.cco --emit-ir\n"
                "  cco hello.cco --dump-ssa\n"
            );
            return 0;
        }
    }

    const char *input_path = NULL;
    const char *output_arg = NULL;
    int opt_level = 0;
    bool use_ssa = false;
    bool dump_ssa = false;
    bool dump_ssa_opt = false;
    bool dump_loops = false;
    bool dump_callgraph = false;
    bool dump_code_stats = false;
    bool profile_generate = false;
    const char *profile_use_path = NULL;
    const char *profile_file_path = NULL;
    bool dump_profile = false;
    bool dump_inlining = false;
    bool emit_c = false;
    bool emit_ir = false;
    bool use_ir = false;
    bool use_native = false;
    bool use_internal_linker = false;
    bool emit_asm = false;
    bool emit_object = false;
    bool dump_regalloc = false;
    bool run_binary = false;
    bool dump_tokens_mode = false;
    bool time_passes = false;
    bool time_passes_csv = false;
    CcoPassTimings pass_timings = {0};
    double t_total_start = cco_get_time_ms();

    /* Direct multi-object linker mode: cco-link or --link-internal with .o files */
    bool is_cco_link = (strstr(argv[0], "cco-link") != NULL);
    if (is_cco_link || (argc >= 2 && strcmp(argv[1], "--link-internal") == 0 && (argc < 3 || str_ends_with(argv[2], ".o") || strcmp(argv[2], "-o") == 0))) {
        const char *link_out = "a.out";
        const char *obj_files[256];
        size_t obj_count = 0;
        int start_arg = is_cco_link ? 1 : 2;
        for (int i = start_arg; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                link_out = argv[++i];
            } else if (argv[i][0] != '-') {
                if (obj_count < 256) {
                    obj_files[obj_count++] = argv[i];
                }
            }
        }
        if (obj_count == 0) {
            fprintf(stderr, "Error: No input object files specified for linking\n");
            return 1;
        }
        char *link_err = NULL;
        if (!elf64_link_files(obj_files, obj_count, link_out, &link_err)) {
            fprintf(stderr, "Link error: %s\n", link_err ? link_err : "linking failed");
            if (link_err) free(link_err);
            return 1;
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_arg = argv[++i];
        } else if (strcmp(argv[i], "-O0") == 0) {
            opt_level = 0;
        } else if (strcmp(argv[i], "-O1") == 0 || strcmp(argv[i], "-O") == 0 || strcmp(argv[i], "--opt") == 0 || strcmp(argv[i], "--optimize") == 0) {
            opt_level = 1;
        } else if (strcmp(argv[i], "-O2") == 0) {
            opt_level = 2;
            use_ssa = true;
        } else if (strcmp(argv[i], "--dump-callgraph") == 0 || strcmp(argv[i], "--dump-call-graph") == 0 || strcmp(argv[i], "--dump-ipa") == 0) {
            dump_callgraph = true;
        } else if (strcmp(argv[i], "--dump-code-stats") == 0 || strcmp(argv[i], "--code-stats") == 0) {
            dump_code_stats = true;
        } else if (strcmp(argv[i], "--time-passes") == 0) {
            time_passes = true;
        } else if (strcmp(argv[i], "--time-passes=csv") == 0 || strcmp(argv[i], "--time-passes-csv") == 0) {
            time_passes = true;
            time_passes_csv = true;
        } else if (strcmp(argv[i], "--profile-generate") == 0 || strcmp(argv[i], "-fprofile-generate") == 0) {
            profile_generate = true;
            use_ir = true;
        } else if (strncmp(argv[i], "--profile-generate=", 19) == 0) {
            profile_generate = true;
            use_ir = true;
            profile_file_path = argv[i] + 19;
        } else if (strncmp(argv[i], "-fprofile-generate=", 19) == 0) {
            profile_generate = true;
            use_ir = true;
            profile_file_path = argv[i] + 19;
        } else if (strcmp(argv[i], "--profile-use") == 0 || strcmp(argv[i], "-fprofile-use") == 0) {
            if (i + 1 < argc) {
                profile_use_path = argv[++i];
            }
        } else if (strncmp(argv[i], "--profile-use=", 14) == 0) {
            profile_use_path = argv[i] + 14;
        } else if (strncmp(argv[i], "-fprofile-use=", 14) == 0) {
            profile_use_path = argv[i] + 14;
        } else if (strcmp(argv[i], "--profile-file") == 0) {
            if (i + 1 < argc) {
                profile_file_path = argv[++i];
            }
        } else if (strncmp(argv[i], "--profile-file=", 15) == 0) {
            profile_file_path = argv[i] + 15;
        } else if (strcmp(argv[i], "--dump-profile") == 0) {
            dump_profile = true;
        } else if (strcmp(argv[i], "--dump-inlining") == 0 || strcmp(argv[i], "--dump-inline-decisions") == 0) {
            dump_inlining = true;
        } else if (strcmp(argv[i], "--ssa") == 0) {
            use_ssa = true;
        } else if (strcmp(argv[i], "--dump-ssa") == 0) {
            dump_ssa = true;
        } else if (strcmp(argv[i], "--dump-ssa-opt") == 0) {
            dump_ssa_opt = true;
        } else if (strcmp(argv[i], "--dump-loops") == 0) {
            dump_loops = true;
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            emit_c = true;
        } else if (strcmp(argv[i], "--emit-ir") == 0) {
            emit_ir = true;
        } else if (strcmp(argv[i], "--use-ir") == 0) {
            use_ir = true;
        } else if (strcmp(argv[i], "--use-native") == 0) {
            use_native = true;
        } else if (strcmp(argv[i], "--use-internal-linker") == 0 || strcmp(argv[i], "--link-internal") == 0) {
            use_internal_linker = true;
            use_native = true;
        } else if (strcmp(argv[i], "--emit-asm") == 0) {
            emit_asm = true;
        } else if (strcmp(argv[i], "--emit-object") == 0) {
            emit_object = true;
        } else if (strcmp(argv[i], "--dump-regalloc") == 0) {
            dump_regalloc = true;
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

    if (dump_profile) {
        const char *pfile = profile_use_path ? profile_use_path : (profile_file_path ? profile_file_path : input_path);
        if (!pfile) {
            fprintf(stderr, "Error: No profile file specified for --dump-profile\n");
            return 1;
        }
        char *load_err = NULL;
        CcoProfile *prof = cco_profile_load(pfile, &load_err);
        if (!prof) {
            fprintf(stderr, "Error loading profile '%s': %s\n", pfile, load_err ? load_err : "failed to parse profile");
            if (load_err) free(load_err);
            return 1;
        }
        cco_profile_dump(stdout, prof);
        cco_profile_free(prof);
        return 0;
    }

    if (!input_path) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    CcoProfile *loaded_profile = NULL;
    if (profile_use_path) {
        double t_prof = time_passes ? cco_get_time_ms() : 0.0;
        char *load_err = NULL;
        loaded_profile = cco_profile_load(profile_use_path, &load_err);
        if (time_passes) pass_timings.pgo_ms += (cco_get_time_ms() - t_prof);
        if (!loaded_profile) {
            fprintf(stderr, "Profile load warning: %s (continuing with non-PGO optimization)\n", load_err ? load_err : "failed to load profile");
            if (load_err) free(load_err);
        }
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

    double t_front_start = time_passes ? cco_get_time_ms() : 0.0;
    AstArena *arena = create_ast_arena();
    AstNode *ast = resolve_program(input_path, arena);

    // Trait conformance checking and monomorphization
    resolve_and_monomorphize_traits(ast, arena);

    // 3. Scope Analysis & Auto-free Annotation Pass
    analyze_scopes(ast, arena);
    if (time_passes) pass_timings.frontend_ms = cco_get_time_ms() - t_front_start;

    // Optional SSA Form Debug Dump
    if (dump_ssa) {
        IrModule *ir_mod = ir_lower_ast(ast, input_path);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        ir_mod->profile = loaded_profile;
        char *verify_err = NULL;
        if (!ir_verify_module(ir_mod, &verify_err)) {
            fprintf(stderr, "%s", verify_err ? verify_err : "Cco IR verification failed\n");
            if (verify_err) free(verify_err);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        char *ssa_err = NULL;
        if (!ir_ssa_construct_module(ir_mod, NULL, &ssa_err)) {
            fprintf(stderr, "SSA construction error: %s\n", ssa_err ? ssa_err : "unknown");
            if (ssa_err) free(ssa_err);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        if (opt_level > 0) {
            bool changed = false;
            ir_ssa_optimize_module(ir_mod, &changed, NULL);
        }
        char *ssa_verr = NULL;
        if (!ir_ssa_verify_module(ir_mod, &ssa_verr)) {
            fprintf(stderr, "SSA verification error: %s\n", ssa_verr ? ssa_verr : "unknown");
            if (ssa_verr) free(ssa_verr);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        ir_dump_ssa_module(stdout, ir_mod);
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // Optional Post-Optimization SSA Form Debug Dump
    if (dump_ssa_opt) {
        IrModule *ir_mod = ir_lower_ast(ast, input_path);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        ir_mod->profile = loaded_profile;
        char *verify_err = NULL;
        if (!ir_verify_module(ir_mod, &verify_err)) {
            fprintf(stderr, "%s", verify_err ? verify_err : "Cco IR verification failed\n");
            if (verify_err) free(verify_err);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        char *ssa_err = NULL;
        if (!ir_ssa_construct_module(ir_mod, NULL, &ssa_err)) {
            fprintf(stderr, "SSA construction error: %s\n", ssa_err ? ssa_err : "unknown");
            if (ssa_err) free(ssa_err);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        IrSsaOptions ssa_opts = {
            .dump_ssa = false,
            .dump_ssa_opt = false,
            .optimize_ssa = true,
            .verify_ssa = true,
            .verbose = false,
            .opt_level = opt_level,
            .enable_inlining = (opt_level >= 2),
            .inline_threshold = IR_DEFAULT_INLINE_THRESHOLD,
            .enable_dfe = (opt_level >= 2),
            .dump_callgraph = false,
            .profile = loaded_profile
        };
        char *opt_err = NULL;
        if (!ir_ssa_advanced_optimize_module(ir_mod, &ssa_opts, NULL, &opt_err)) {
            fprintf(stderr, "Advanced SSA optimization error: %s\n", opt_err ? opt_err : "unknown");
            if (opt_err) free(opt_err);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        if (opt_level >= 2) {
            IrIpaOptions ipa_opts = {
                .inline_threshold = IR_DEFAULT_INLINE_THRESHOLD,
                .enable_inlining = true,
                .enable_dfe = true,
                .verbose = false,
                .profile = loaded_profile
            };
            if (!ir_ipa_optimize_module(ir_mod, &ipa_opts, NULL, &opt_err)) {
                fprintf(stderr, "IPA optimization error: %s\n", opt_err ? opt_err : "unknown");
                if (opt_err) free(opt_err);
                ir_module_free(ir_mod);
                if (loaded_profile) cco_profile_free(loaded_profile);
                free_ast_arena(arena);
                return 1;
            }
        }
        ir_dump_ssa_module(stdout, ir_mod);
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // Optional Natural Loops Debug Dump
    if (dump_loops) {
        IrModule *ir_mod = ir_lower_ast(ast, input_path);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        ir_mod->profile = loaded_profile;
        for (IrFunction *fn = ir_mod->first_fn; fn; fn = fn->next) {
            IrDomInfo *dom = ir_dominance_compute(fn);
            if (dom) {
                IrLoopInfo *loop_info = ir_loop_analysis_compute(fn, dom);
                if (loop_info) {
                    if (loaded_profile) {
                        ir_loop_apply_profile(loop_info, loaded_profile);
                        ir_loop_dump_profile(stdout, loop_info);
                    } else {
                        ir_loop_dump(stdout, loop_info);
                    }
                    ir_loop_info_free(loop_info);
                }
                ir_dominance_free(dom);
            }
        }
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // Optional Call Graph Debug Dump
    if (dump_callgraph) {
        IrModule *ir_mod = ir_lower_ast(ast, input_path);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        IrCallGraph *cg = ir_callgraph_build(ir_mod);
        if (cg) {
            ir_callgraph_dump(stdout, cg);
            ir_callgraph_free(cg);
        }
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // Optional Inlining & PGO Decision Log Dump
    if (dump_inlining) {
        cco_profile_clear_inline_decisions();
        IrModule *ir_mod = build_ir_module(ast, input_path, opt_level >= 2 ? opt_level : 2, true, loaded_profile);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            cco_profile_clear_inline_decisions();
            free_ast_arena(arena);
            return 1;
        }
        cco_profile_dump_inline_decisions(stdout);
        cco_profile_clear_inline_decisions();
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // Optional Code Stats Dump
    if (dump_code_stats) {
        IrModule *ir_mod = build_ir_module(ast, input_path, opt_level, use_ssa, loaded_profile);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        CcoCodeStats stats;
        if (loaded_profile) {
            cco_code_stats_collect_pgo(ir_mod, loaded_profile, &stats);
        } else {
            cco_code_stats_collect(ir_mod, &stats);
        }
        cco_code_stats_dump(stdout, &stats);
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // Optional Register Allocation Debug Dump
    if (dump_regalloc) {
        IrModule *ir_mod = build_ir_module(ast, input_path, opt_level, use_ssa, loaded_profile);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        for (IrFunction *fn = ir_mod->first_fn; fn; fn = fn->next) {
            RegAllocResult *res = regalloc_run(fn);
            regalloc_dump(res, stdout);
            regalloc_free(res);
        }
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // Optional Native x86-64 backend path
    if (emit_asm || emit_object || use_native) {
        IrModule *ir_mod = build_ir_module_timed(ast, input_path, opt_level, use_ssa, loaded_profile, time_passes ? &pass_timings : NULL);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }

        /* 1. Explicit assembly emission requested */
        if (emit_asm) {
            char *asm_err = NULL;
            char *asm_code = x86_64_generate_assembly(ir_mod, &asm_err);
            if (!asm_code) {
                fprintf(stderr, "%s\n", asm_err ? asm_err : "x86-64 code generation failed");
                if (asm_err) free(asm_err);
                ir_module_free(ir_mod);
                if (loaded_profile) cco_profile_free(loaded_profile);
                free_ast_arena(arena);
                return 1;
            }

            if (output_arg && str_ends_with(output_arg, ".s")) {
                write_file(output_arg, asm_code);
            } else {
                printf("%s", asm_code);
            }
            free(asm_code);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 0;
        }

        /* 2. Direct Object File emission or native executable generation */
        char base_name[PATH_MAX];
        snprintf(base_name, sizeof(base_name), "%s", input_path);
        char *dot = strrchr(base_name, '.');
        if (dot && strcmp(dot, ".cco") == 0) *dot = '\0';

        char output_o_path[PATH_MAX * 2] = {0};
        char binary_out_path[PATH_MAX * 2] = {0};
        bool compile_to_binary = true;

        if (emit_object || (output_arg && str_ends_with(output_arg, ".o"))) {
            compile_to_binary = run_binary;
            if (output_arg) {
                snprintf(output_o_path, sizeof(output_o_path), "%s", output_arg);
            } else {
                snprintf(output_o_path, sizeof(output_o_path), "%s.o", base_name);
            }
            if (run_binary) {
                snprintf(binary_out_path, sizeof(binary_out_path), "build/cco_native_out");
            }
        } else if (output_arg) {
            snprintf(binary_out_path, sizeof(binary_out_path), "%s", output_arg);
            struct stat st;
            if (stat("build", &st) == 0 && S_ISDIR(st.st_mode)) {
                const char *base = strrchr(output_arg, '/');
                base = base ? base + 1 : output_arg;
                snprintf(output_o_path, sizeof(output_o_path), "build/%s.o", base);
            } else {
                snprintf(output_o_path, sizeof(output_o_path), "%s.o", output_arg);
            }
        } else {
            snprintf(binary_out_path, sizeof(binary_out_path), "%s", base_name);
            struct stat st;
            if (stat("build", &st) == 0 && S_ISDIR(st.st_mode)) {
                const char *base = strrchr(base_name, '/');
                base = base ? base + 1 : base_name;
                snprintf(output_o_path, sizeof(output_o_path), "build/%s.o", base);
            } else {
                snprintf(output_o_path, sizeof(output_o_path), "%s.o", base_name);
            }
        }

        ensure_parent_dir(output_o_path);
        char *obj_err = NULL;
        if (!x86_64_emit_object_file_timed(ir_mod, output_o_path, time_passes ? &pass_timings : NULL, &obj_err)) {
            fprintf(stderr, "%s\n", obj_err ? obj_err : "x86-64 direct object generation failed");
            if (obj_err) free(obj_err);
            ir_module_free(ir_mod);
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }

        ir_module_free(ir_mod);
        if (loaded_profile) {
            cco_profile_free(loaded_profile);
            loaded_profile = NULL;
        }
        free_ast_arena(arena);

        if (compile_to_binary) {
            ensure_parent_dir(binary_out_path);
            if (use_internal_linker) {
                char *link_err = NULL;
                double t_link = time_passes ? cco_get_time_ms() : 0.0;
                if (!elf64_link_single_file(output_o_path, binary_out_path, &link_err)) {
                    fprintf(stderr, "Link error: %s\n", link_err ? link_err : "internal linker failed");
                    if (link_err) free(link_err);
                    return 1;
                }
                if (time_passes) pass_timings.link_ms += (cco_get_time_ms() - t_link);
            } else {
                const char *cc = getenv("CC");
                if (!cc || strlen(cc) == 0) cc = "gcc";
                char cmd[PATH_MAX * 4 + 512];
                snprintf(cmd, sizeof(cmd), "%s -no-pie \"%s\" -o \"%s\" -lm", cc, output_o_path, binary_out_path);
                double t_link = time_passes ? cco_get_time_ms() : 0.0;
                int res = system(cmd);
                if (time_passes) pass_timings.link_ms += (cco_get_time_ms() - t_link);
                if (res != 0) {
                    int exit_code = 1;
                    #ifdef WEXITSTATUS
                    if (WIFEXITED(res)) exit_code = WEXITSTATUS(res);
                    #endif
                    return exit_code;
                }
            }

            if (time_passes) {
                pass_timings.total_ms = cco_get_time_ms() - t_total_start;
                if (time_passes_csv) {
                    cco_pass_timings_dump_csv(stdout, &pass_timings);
                } else {
                    cco_pass_timings_dump(stdout, &pass_timings);
                }
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
        } else {
            if (time_passes) {
                pass_timings.total_ms = cco_get_time_ms() - t_total_start;
                if (time_passes_csv) {
                    cco_pass_timings_dump_csv(stdout, &pass_timings);
                } else {
                    cco_pass_timings_dump(stdout, &pass_timings);
                }
            }
        }
        return 0;
    }

    // Optional IR emission
    if (emit_ir) {
        IrModule *ir_mod = build_ir_module(ast, input_path, opt_level, use_ssa, loaded_profile);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        ir_dump_module(stdout, ir_mod);
        ir_module_free(ir_mod);
        if (loaded_profile) cco_profile_free(loaded_profile);
        free_ast_arena(arena);
        return 0;
    }

    // 4. Code Generation
    char *c_code = NULL;
    if (profile_generate) {
        IrModule *ir_mod = build_ir_module(ast, input_path, opt_level, use_ssa, NULL);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        const char *prof_out = profile_file_path ? profile_file_path : "cco.profile";
        c_code = ir_generate_c_instrumented(ir_mod, prof_out);
        ir_module_free(ir_mod);
    } else if (use_ir) {
        IrModule *ir_mod = build_ir_module(ast, input_path, opt_level, use_ssa, loaded_profile);
        if (!ir_mod) {
            if (loaded_profile) cco_profile_free(loaded_profile);
            free_ast_arena(arena);
            return 1;
        }
        c_code = ir_generate_c(ir_mod);
        ir_module_free(ir_mod);
    } else {
        c_code = generate_c_code(ast, arena);
    }

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
    if (loaded_profile) {
        cco_profile_free(loaded_profile);
        loaded_profile = NULL;
    }
    cco_profile_clear_inline_decisions();

    // 5. Optional compilation and execution with gcc/clang
    if (compile_to_binary) {
        const char *cc = getenv("CC");
        if (!cc || strlen(cc) == 0) cc = "gcc";

        ensure_parent_dir(binary_out_path);

        char cmd[PATH_MAX * 4 + 512];
        snprintf(cmd, sizeof(cmd), "%s -O3 -Wall -Wextra -std=c11 -Wno-unused-function -Wno-parentheses-equality \"%s\" -o \"%s\" -lm", cc, output_c_path, binary_out_path);
        double t_link = time_passes ? cco_get_time_ms() : 0.0;
        int res = system(cmd);
        if (time_passes) pass_timings.link_ms += (cco_get_time_ms() - t_link);
        if (res != 0) {
            int exit_code = 1;
            #ifdef WEXITSTATUS
            if (WIFEXITED(res)) exit_code = WEXITSTATUS(res);
            #endif
            return exit_code;
        }

        if (time_passes) {
            pass_timings.total_ms = cco_get_time_ms() - t_total_start;
            if (time_passes_csv) {
                cco_pass_timings_dump_csv(stdout, &pass_timings);
            } else {
                cco_pass_timings_dump(stdout, &pass_timings);
            }
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

    if (time_passes) {
        pass_timings.total_ms = cco_get_time_ms() - t_total_start;
        if (time_passes_csv) {
            cco_pass_timings_dump_csv(stdout, &pass_timings);
        } else {
            cco_pass_timings_dump(stdout, &pass_timings);
        }
    }

    return 0;
}

