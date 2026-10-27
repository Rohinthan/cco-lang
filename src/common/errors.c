// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *g_filename = NULL;
static char *g_source = NULL;
static bool g_diagnostic_codes_enabled = false;

void set_diagnostic_codes_enabled(bool enabled) {
    g_diagnostic_codes_enabled = enabled;
}

bool are_diagnostic_codes_enabled(void) {
    if (g_diagnostic_codes_enabled) return true;
    const char *env = getenv("CCO_DIAGNOSTICS_CODES");
    return (env && (strcmp(env, "1") == 0 || strcmp(env, "true") == 0));
}

void init_error_reporter(const char *filename, const char *source) {
    if (filename) {
        if (g_filename) free(g_filename);
        g_filename = strdup(filename);
    }
    if (source) {
        if (g_source) free(g_source);
        g_source = strdup(source);
    }
}

const char *get_error_filename(void) {
    return g_filename ? g_filename : "unknown";
}

const char *get_error_source(void) {
    return g_source ? g_source : "";
}

static char *get_line_source(const char *source, int line_num) {
    if (!source || line_num <= 0) return NULL;
    int current_line = 1;
    const char *p = source;
    while (*p && current_line < line_num) {
        if (*p == '\n') current_line++;
        p++;
    }
    if (current_line != line_num || !*p) return NULL;

    const char *start = p;
    while (*p && *p != '\n' && *p != '\r') p++;
    size_t len = p - start;

    char *line_buf = (char *)malloc(len + 1);
    if (!line_buf) return NULL;
    memcpy(line_buf, start, len);
    line_buf[len] = '\0';
    return line_buf;
}

static int get_line_width(int line) {
    if (line < 10) return 2;
    int w = 0;
    int temp = line;
    while (temp > 0) {
        w++;
        temp /= 10;
    }
    return w > 2 ? w : 2;
}

typedef struct {
    char *filename;
    char *source;
} FileRegistryEntry;

static FileRegistryEntry g_file_registry[64];
static int g_file_registry_count = 0;

void register_file_source(const char *filename, const char *source) {
    if (!filename || !source) return;
    for (int i = 0; i < g_file_registry_count; i++) {
        if (strcmp(g_file_registry[i].filename, filename) == 0) {
            return;
        }
    }
    if (g_file_registry_count < 64) {
        g_file_registry[g_file_registry_count].filename = strdup(filename);
        g_file_registry[g_file_registry_count].source = strdup(source);
        g_file_registry_count++;
    }
    init_error_reporter(filename, source);
}

static const char *find_file_source(const char *filename) {
    if (filename) {
        for (int i = 0; i < g_file_registry_count; i++) {
            if (strcmp(g_file_registry[i].filename, filename) == 0) {
                return g_file_registry[i].source;
            }
        }
    }
    return g_source;
}

static char *get_line_source_for_file(const char *filename, int line_num) {
    const char *src = find_file_source(filename);
    return get_line_source(src, line_num);
}

static void print_caret_padding(FILE *stream, const char *line_src, int col) {
    for (int i = 1; i < col; i++) {
        char c = (line_src && (size_t)(i - 1) < strlen(line_src)) ? line_src[i - 1] : ' ';
        fputc(c == '\t' ? '\t' : ' ', stream);
    }
}

void emit_diagnostic(const Diagnostic *diag) {
    if (!diag) exit(1);

    const char *sev_str = "error";
    if (diag->severity == DIAG_WARNING) sev_str = "warning";
    else if (diag->severity == DIAG_NOTE) sev_str = "note";

    const char *code = diag->code;
    if (!code && are_diagnostic_codes_enabled() && diag->message) {
        if (strstr(diag->message, "undefined variable")) code = "E2001";
        else if (strstr(diag->message, "undefined function") || strstr(diag->message, "unknown function")) code = "E2002";
        else if (strstr(diag->message, "duplicate")) code = "E2003";
        else if (strstr(diag->message, "cannot assign")) code = "E3002";
        else if (strstr(diag->message, "return type")) code = "E3004";
        else if (strstr(diag->message, "type mismatch")) code = "E3001";
        else if (strstr(diag->message, "argument count") || strstr(diag->message, "arguments, got") || strstr(diag->message, "expects")) code = "E3003";
        else if (strstr(diag->message, "operator")) code = "E3005";
        else if (strstr(diag->message, "move")) code = "E4001";
        else if (strstr(diag->message, "syntax error") || strstr(diag->message, "expected")) code = "E1005";
    }

    if (code && code[0] != '\0' && (are_diagnostic_codes_enabled() || strncmp(code, "E100", 4) == 0)) {
        fprintf(stderr, "%s[%s]: %s\n", sev_str, code, diag->message ? diag->message : "");
    } else {
        fprintf(stderr, "%s: %s\n", sev_str, diag->message ? diag->message : "");
    }

    const char *fn1 = diag->primary_loc.filename ? diag->primary_loc.filename : g_filename;
    int line1_num = diag->primary_loc.line > 0 ? diag->primary_loc.line : 1;
    int col1_num = diag->primary_loc.col > 0 ? diag->primary_loc.col : 1;
    fprintf(stderr, "  --> %s:%d:%d\n", fn1 ? fn1 : "unknown", line1_num, col1_num);

    int w1 = get_line_width(line1_num);
    fprintf(stderr, " %*s |\n", w1, "");
    char *line1 = get_line_source_for_file(fn1, line1_num);
    fprintf(stderr, " %*d | %s\n", w1, line1_num, line1 ? line1 : "");
    fprintf(stderr, " %*s | ", w1, "");
    print_caret_padding(stderr, line1, col1_num);
    fputc('^', stderr);
    if (diag->primary_label) {
        fprintf(stderr, " %s", diag->primary_label);
    }
    fputc('\n', stderr);
    fprintf(stderr, " %*s |\n", w1, "");
    if (line1) free(line1);

    if (diag->secondary_note) {
        fprintf(stderr, "note: %s\n", diag->secondary_note);
    }
    if (diag->secondary_loc.line > 0) {
        const char *fn2 = diag->secondary_loc.filename ? diag->secondary_loc.filename : g_filename;
        int line2_num = diag->secondary_loc.line;
        int col2_num = diag->secondary_loc.col > 0 ? diag->secondary_loc.col : 1;
        fprintf(stderr, "  --> %s:%d:%d\n", fn2 ? fn2 : "unknown", line2_num, col2_num);

        int w2 = get_line_width(line2_num);
        fprintf(stderr, " %*s |\n", w2, "");
        char *line2 = get_line_source_for_file(fn2, line2_num);
        fprintf(stderr, " %*d | %s\n", w2, line2_num, line2 ? line2 : "");
        fprintf(stderr, " %*s | ", w2, "");
        print_caret_padding(stderr, line2, col2_num);
        fputc('^', stderr);
        if (diag->secondary_label) {
            fprintf(stderr, " %s", diag->secondary_label);
        }
        fputc('\n', stderr);
        fprintf(stderr, " %*s |\n", w2, "");
        if (line2) free(line2);
    }

    if (diag->note) {
        fprintf(stderr, "note: %s\n", diag->note);
    }

    if (diag->extra_note) {
        fprintf(stderr, "note: %s\n", diag->extra_note);
    }

    if (diag->help) {
        fprintf(stderr, "help: %s\n", diag->help);
    }

    exit(1);
}

void print_formatted_error_code(const char *code,
                                const char *short_msg,
                                ErrorLocation primary,
                                const char *primary_caret_note,
                                const char *note_msg,
                                ErrorLocation *note_loc,
                                const char *note_caret_note,
                                const char *extra_footer_note) {
    Diagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.severity = DIAG_ERROR;
    diag.code = code;
    diag.message = short_msg;
    diag.primary_loc = primary;
    diag.primary_label = primary_caret_note;
    if (note_loc && note_loc->line > 0) {
        diag.secondary_loc = *note_loc;
        diag.secondary_label = note_caret_note;
        diag.secondary_note = note_msg;
    } else if (note_msg) {
        diag.note = note_msg;
    }
    diag.extra_note = extra_footer_note;
    emit_diagnostic(&diag);
}

void print_formatted_error(const char *short_msg,
                          ErrorLocation primary,
                          const char *primary_caret_note,
                          const char *note_msg,
                          ErrorLocation *note_loc,
                          const char *note_caret_note,
                          const char *extra_footer_note) {
    print_formatted_error_code(NULL, short_msg, primary, primary_caret_note,
                               note_msg, note_loc, note_caret_note, extra_footer_note);
}

void fatal_lexer_error(int line, int col, const char *msg) {
    char short_msg[256];
    snprintf(short_msg, sizeof(short_msg), "lexer error: %s", msg);
    ErrorLocation loc = {g_filename, line, col};
    print_formatted_error(short_msg, loc, "unexpected input", NULL, NULL, NULL, NULL);
}

void fatal_lexer_error_code(const char *code, int line, int col, const char *msg) {
    Diagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.severity = DIAG_ERROR;
    diag.code = code;
    diag.message = msg;
    diag.primary_loc.filename = g_filename;
    diag.primary_loc.line = line;
    diag.primary_loc.col = col;
    diag.primary_label = "lexer error here";
    emit_diagnostic(&diag);
}

void fatal_parser_error(int line, int col, const char *token_str, const char *msg) {
    char short_msg[256];
    if (token_str) {
        snprintf(short_msg, sizeof(short_msg), "syntax error: %s at '%s'", msg, token_str);
    } else {
        snprintf(short_msg, sizeof(short_msg), "syntax error: %s", msg);
    }
    ErrorLocation loc = {g_filename, line, col};
    print_formatted_error(short_msg, loc, "syntax error here", NULL, NULL, NULL, NULL);
}

void fatal_parser_error_code(const char *code, int line, int col, const char *token_str, const char *msg) {
    char short_msg[256];
    if (token_str) {
        snprintf(short_msg, sizeof(short_msg), "%s at '%s'", msg, token_str);
    } else {
        snprintf(short_msg, sizeof(short_msg), "%s", msg);
    }
    Diagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.severity = DIAG_ERROR;
    diag.code = code;
    diag.message = short_msg;
    diag.primary_loc.filename = g_filename;
    diag.primary_loc.line = line;
    diag.primary_loc.col = col;
    diag.primary_label = "syntax error here";
    emit_diagnostic(&diag);
}
