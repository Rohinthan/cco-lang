#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#include "module_resolver.h"
#include "lexer.h"
#include "parser.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>

typedef struct {
    char *canonical_path;
    char *rel_path;
    char *imported_by;
    int import_line;
    int import_col;
} StackFrame;

typedef struct {
    char *resolved_paths[128];
    int resolved_count;

    StackFrame stack[128];
    int stack_count;

    AstNode **merged_classes;
    int class_count;
    int class_cap;

    AstNode **merged_structs;
    int struct_count;
    int struct_cap;

    AstNode **merged_enums;
    int enum_count;
    int enum_cap;

    AstNode **merged_functions;
    int fn_count;
    int fn_cap;
} ResolverCtx;

static char *read_file_text(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

static char *get_directory(const char *file_path) {
    char *tmp = strdup(file_path);
    char *dir = strdup(dirname(tmp));
    free(tmp);
    return dir;
}

static char *resolve_path(const char *raw_path, const char *importing_file_path) {
    char combined[PATH_MAX];
    if (raw_path[0] == '/' || !importing_file_path) {
        snprintf(combined, sizeof(combined), "%s", raw_path);
    } else {
        char *dir = get_directory(importing_file_path);
        snprintf(combined, sizeof(combined), "%s/%s", dir, raw_path);
        free(dir);
    }

    // NOTE: realpath() is a POSIX standard function (POSIX.1-2001) used here for
    // canonical path resolution in the Cco compiler itself. While Cco-generated C output
    // is strictly portable standard C11, running the Cco compiler binary requires a POSIX environment
    // (Linux/macOS/WSL). On Windows host builds, this would require _fullpath().
    char resolved[PATH_MAX];
    if (realpath(combined, resolved) != NULL) {
        return strdup(resolved);
    }
    return NULL;
}

static bool is_already_resolved(ResolverCtx *ctx, const char *canonical_path) {
    for (int i = 0; i < ctx->resolved_count; i++) {
        if (strcmp(ctx->resolved_paths[i], canonical_path) == 0) {
            return true;
        }
    }
    return false;
}

static int find_in_stack(ResolverCtx *ctx, const char *canonical_path) {
    for (int i = 0; i < ctx->stack_count; i++) {
        if (strcmp(ctx->stack[i].canonical_path, canonical_path) == 0) {
            return i;
        }
    }
    return -1;
}

static void tag_nodes_with_source(AstNode *node, const char *source_file) {
    if (!node) return;
    node->source_file = source_file;
}

static void check_and_add_class(ResolverCtx *ctx, AstNode *cls, const char *canonical_path) {
    cls->source_file = canonical_path;
    const char *cname = cls->as.class_decl.name;
    for (int i = 0; i < ctx->class_count; i++) {
        AstNode *existing = ctx->merged_classes[i];
        if (strcmp(existing->as.class_decl.name, cname) == 0) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", cname);

            ErrorLocation primary = {cls->source_file, cls->line, cls->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }
    for (int i = 0; i < ctx->struct_count; i++) {
        AstNode *existing = ctx->merged_structs[i];
        if (strcmp(existing->as.struct_decl.name, cname) == 0) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", cname);

            ErrorLocation primary = {cls->source_file, cls->line, cls->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }

    if (ctx->class_count >= ctx->class_cap) {
        ctx->class_cap = ctx->class_cap == 0 ? 4 : ctx->class_cap * 2;
        ctx->merged_classes = realloc(ctx->merged_classes, ctx->class_cap * sizeof(AstNode *));
    }
    ctx->merged_classes[ctx->class_count++] = cls;
}

static void check_and_add_struct(ResolverCtx *ctx, AstNode *st, const char *canonical_path) {
    st->source_file = canonical_path;
    const char *sname = st->as.struct_decl.name;
    for (int i = 0; i < ctx->struct_count; i++) {
        AstNode *existing = ctx->merged_structs[i];
        if (strcmp(existing->as.struct_decl.name, sname) == 0) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", sname);

            ErrorLocation primary = {st->source_file, st->line, st->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }
    for (int i = 0; i < ctx->class_count; i++) {
        AstNode *existing = ctx->merged_classes[i];
        if (strcmp(existing->as.class_decl.name, sname) == 0) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", sname);

            ErrorLocation primary = {st->source_file, st->line, st->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }

    if (ctx->struct_count >= ctx->struct_cap) {
        ctx->struct_cap = ctx->struct_cap == 0 ? 4 : ctx->struct_cap * 2;
        ctx->merged_structs = realloc(ctx->merged_structs, ctx->struct_cap * sizeof(AstNode *));
    }
    ctx->merged_structs[ctx->struct_count++] = st;
}

static void check_and_add_enum(ResolverCtx *ctx, AstNode *en, const char *canonical_path) {
    en->source_file = canonical_path;
    const char *ename = en->as.enum_decl.name;
    for (int i = 0; i < ctx->enum_count; i++) {
        AstNode *existing = ctx->merged_enums[i];
        if (strcmp(existing->as.enum_decl.name, ename) == 0) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", ename);

            ErrorLocation primary = {en->source_file, en->line, en->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }
    for (int i = 0; i < ctx->class_count; i++) {
        AstNode *existing = ctx->merged_classes[i];
        if (strcmp(existing->as.class_decl.name, ename) == 0) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", ename);

            ErrorLocation primary = {en->source_file, en->line, en->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }
    for (int i = 0; i < ctx->struct_count; i++) {
        AstNode *existing = ctx->merged_structs[i];
        if (strcmp(existing->as.struct_decl.name, ename) == 0) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", ename);

            ErrorLocation primary = {en->source_file, en->line, en->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }

    if (ctx->enum_count >= ctx->enum_cap) {
        ctx->enum_cap = ctx->enum_cap == 0 ? 4 : ctx->enum_cap * 2;
        ctx->merged_enums = realloc(ctx->merged_enums, ctx->enum_cap * sizeof(AstNode *));
    }
    ctx->merged_enums[ctx->enum_count++] = en;
}

static void check_and_add_function(ResolverCtx *ctx, AstNode *fn, const char *canonical_path) {
    fn->source_file = canonical_path;
    const char *fname = fn->as.function.name;
    for (int i = 0; i < ctx->fn_count; i++) {
        AstNode *existing = ctx->merged_functions[i];
        bool is_dup = false;
        if (fn->as.function.is_operator && existing->as.function.is_operator) {
            if (fn->as.function.operator_symbol && existing->as.function.operator_symbol &&
                strcmp(fn->as.function.operator_symbol, existing->as.function.operator_symbol) == 0 &&
                fn->as.function.param_count == existing->as.function.param_count) {
                const char *cls1 = (fn->as.function.param_count > 0) ? fn->as.function.param_class_names[0] : "";
                const char *cls2 = (existing->as.function.param_count > 0) ? existing->as.function.param_class_names[0] : "";
                if (cls1 && cls2 && strcmp(cls1, cls2) == 0) {
                    is_dup = true;
                }
            }
        } else if (!fn->as.function.is_operator && !existing->as.function.is_operator) {
            if (strcmp(existing->as.function.name, fname) == 0) {
                is_dup = true;
            }
        }
        if (is_dup) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "duplicate definition of '%s'", fname);

            ErrorLocation primary = {fn->source_file, fn->line, fn->col};
            ErrorLocation note_loc = {existing->source_file, existing->line, existing->col};

            print_formatted_error(short_msg, primary, "duplicate definition", "first defined here:", &note_loc, "first defined here", NULL);
        }
    }

    if (ctx->fn_count >= ctx->fn_cap) {
        ctx->fn_cap = ctx->fn_cap == 0 ? 4 : ctx->fn_cap * 2;
        ctx->merged_functions = realloc(ctx->merged_functions, ctx->fn_cap * sizeof(AstNode *));
    }
    ctx->merged_functions[ctx->fn_count++] = fn;
}

static char *compute_display_path(const char *raw_path, const char *importing_display_path) {
    if (raw_path[0] == '/' || !importing_display_path || strcmp(raw_path, importing_display_path) == 0) {
        return strdup(raw_path);
    }
    char *dir = get_directory(importing_display_path);
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/%s", dir, raw_path);
    free(dir);
    return strdup(buf);
}

static void resolve_file_rec(ResolverCtx *ctx, const char *raw_path, const char *importing_canonical, const char *importing_display_path, int import_line, int import_col, AstArena *arena) {
    char *display_path = compute_display_path(raw_path, importing_display_path);
    char *canonical = resolve_path(raw_path, importing_canonical);
    if (!canonical) {
        if (importing_display_path) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "cannot find imported file '%s'", raw_path);
            ErrorLocation loc = {importing_display_path, import_line, import_col};
            print_formatted_error(short_msg, loc, "cannot find imported file", NULL, NULL, NULL, NULL);
        } else {
            fprintf(stderr, "error: cannot find entry file '%s'\n", raw_path);
            exit(1);
        }
    }

    int cycle_idx = find_in_stack(ctx, canonical);
    if (cycle_idx != -1) {
        fprintf(stderr, "error: circular import detected\n");
        for (int i = cycle_idx; i < ctx->stack_count; i++) {
            const char *src_name = ctx->stack[i].rel_path ? ctx->stack[i].rel_path : ctx->stack[i].canonical_path;
            const char *dst_name = (i + 1 < ctx->stack_count) ? (ctx->stack[i + 1].rel_path ? ctx->stack[i + 1].rel_path : ctx->stack[i + 1].canonical_path) : raw_path;
            int line_no = (i + 1 < ctx->stack_count) ? ctx->stack[i + 1].import_line : import_line;
            fprintf(stderr, "  %s imports %s (line %d)\n", src_name, dst_name, line_no);
        }
        free(canonical);
        free(display_path);
        exit(1);
    }

    // If already fully resolved, skip
    if (is_already_resolved(ctx, canonical)) {
        free(canonical);
        free(display_path);
        return;
    }

    // Push onto resolution stack
    ctx->stack[ctx->stack_count].canonical_path = canonical;
    ctx->stack[ctx->stack_count].rel_path = display_path;
    ctx->stack[ctx->stack_count].imported_by = importing_display_path ? strdup(importing_display_path) : NULL;
    ctx->stack[ctx->stack_count].import_line = import_line;
    ctx->stack[ctx->stack_count].import_col = import_col;
    ctx->stack_count++;

    // Read and parse
    char *source_text = read_file_text(canonical);
    if (!source_text) {
        char short_msg[256];
        snprintf(short_msg, sizeof(short_msg), "cannot read module file '%s'", raw_path);
        ErrorLocation primary = {importing_display_path ? importing_display_path : "entry", import_line, import_col};
        print_formatted_error(short_msg, primary, "read error", NULL, NULL, NULL, NULL);
    }

    register_file_source(canonical, source_text);
    register_file_source(display_path, source_text);

    TokenArray tokens = lex_source(source_text);
    Parser parser = create_parser(tokens, arena);
    AstNode *file_ast = parse_program(&parser);

    // 1. Recurse on imports depth-first
    for (int i = 0; i < file_ast->as.program.import_count; i++) {
        AstNode *imp = file_ast->as.program.imports[i];
        imp->source_file = arena_strdup(arena, display_path);
        resolve_file_rec(ctx, imp->as.import_stmt.path, canonical, display_path, imp->line, imp->col, arena);
    }

    // 2. Merge declarations
    char *arena_display_path = arena_strdup(arena, display_path);
    for (int i = 0; i < file_ast->as.program.class_count; i++) {
        AstNode *cls = file_ast->as.program.classes[i];
        tag_nodes_with_source(cls, arena_display_path);
        check_and_add_class(ctx, cls, arena_display_path);
    }

    for (int i = 0; i < file_ast->as.program.struct_count; i++) {
        AstNode *st = file_ast->as.program.structs[i];
        tag_nodes_with_source(st, arena_display_path);
        check_and_add_struct(ctx, st, arena_display_path);
    }

    for (int i = 0; i < file_ast->as.program.enum_count; i++) {
        AstNode *en = file_ast->as.program.enums[i];
        tag_nodes_with_source(en, arena_display_path);
        check_and_add_enum(ctx, en, arena_display_path);
    }

    for (int i = 0; i < file_ast->as.program.count; i++) {
        AstNode *fn = file_ast->as.program.functions[i];
        tag_nodes_with_source(fn, arena_display_path);
        check_and_add_function(ctx, fn, arena_display_path);
    }

    // Mark canonical as resolved
    ctx->resolved_paths[ctx->resolved_count++] = strdup(canonical);

    // Pop stack
    free(ctx->stack[ctx->stack_count - 1].rel_path);
    if (ctx->stack[ctx->stack_count - 1].imported_by) {
        free(ctx->stack[ctx->stack_count - 1].imported_by);
    }
    ctx->stack_count--;
}

AstNode *resolve_program(const char *entry_path, AstArena *arena) {
    ResolverCtx ctx;
    memset(&ctx, 0, sizeof(ctx));

    resolve_file_rec(&ctx, entry_path, NULL, entry_path, 0, 0, arena);

    AstNode *merged_prog = arena_alloc_node(arena, NODE_PROGRAM, 1, 1);
    merged_prog->source_file = entry_path;
    merged_prog->as.program.classes = (AstNode **)arena_alloc_array(arena, ctx.class_count, sizeof(AstNode *));
    if (ctx.class_count > 0 && ctx.merged_classes) {
        memcpy(merged_prog->as.program.classes, ctx.merged_classes, ctx.class_count * sizeof(AstNode *));
    }
    merged_prog->as.program.class_count = ctx.class_count;

    merged_prog->as.program.structs = (AstNode **)arena_alloc_array(arena, ctx.struct_count, sizeof(AstNode *));
    if (ctx.struct_count > 0 && ctx.merged_structs) {
        memcpy(merged_prog->as.program.structs, ctx.merged_structs, ctx.struct_count * sizeof(AstNode *));
    }
    merged_prog->as.program.struct_count = ctx.struct_count;

    merged_prog->as.program.enums = (AstNode **)arena_alloc_array(arena, ctx.enum_count, sizeof(AstNode *));
    if (ctx.enum_count > 0 && ctx.merged_enums) {
        memcpy(merged_prog->as.program.enums, ctx.merged_enums, ctx.enum_count * sizeof(AstNode *));
    }
    merged_prog->as.program.enum_count = ctx.enum_count;

    merged_prog->as.program.functions = (AstNode **)arena_alloc_array(arena, ctx.fn_count, sizeof(AstNode *));
    if (ctx.fn_count > 0 && ctx.merged_functions) {
        memcpy(merged_prog->as.program.functions, ctx.merged_functions, ctx.fn_count * sizeof(AstNode *));
    }
    merged_prog->as.program.count = ctx.fn_count;

    if (ctx.merged_classes) free(ctx.merged_classes);
    if (ctx.merged_structs) free(ctx.merged_structs);
    if (ctx.merged_enums) free(ctx.merged_enums);
    if (ctx.merged_functions) free(ctx.merged_functions);
    for (int i = 0; i < ctx.resolved_count; i++) {
        free(ctx.resolved_paths[i]);
    }

    return merged_prog;
}
