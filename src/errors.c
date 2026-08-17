#define _POSIX_C_SOURCE 200809L
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_filename = "unknown";
static const char *g_source = "";

void init_error_reporter(const char *filename, const char *source) {
    if (filename) g_filename = filename;
    if (source) g_source = source;
}

const char *get_error_filename(void) {
    return g_filename;
}

const char *get_error_source(void) {
    return g_source;
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

void print_formatted_error(const char *short_msg,
                          ErrorLocation primary,
                          const char *primary_caret_note,
                          const char *note_msg,
                          ErrorLocation *note_loc,
                          const char *note_caret_note,
                          const char *extra_footer_note) {
    const char *fn1 = primary.filename ? primary.filename : g_filename;
    fprintf(stderr, "error: %s\n", short_msg);
    fprintf(stderr, "  --> %s:%d:%d\n", fn1, primary.line, primary.col);

    int w1 = get_line_width(primary.line);
    fprintf(stderr, " %*s |\n", w1, "");
    char *line1 = get_line_source(g_source, primary.line);
    fprintf(stderr, " %*d | %s\n", w1, primary.line, line1 ? line1 : "");
    fprintf(stderr, " %*s | ", w1, "");
    for (int i = 1; i < primary.col; i++) fputc(' ', stderr);
    fputc('^', stderr);
    if (primary_caret_note) fprintf(stderr, " %s", primary_caret_note);
    fputc('\n', stderr);
    fprintf(stderr, " %*s |\n", w1, "");
    if (line1) free(line1);

    if (note_msg) {
        fprintf(stderr, "note: %s\n", note_msg);
        if (note_loc && note_loc->line > 0) {
            const char *fn2 = note_loc->filename ? note_loc->filename : g_filename;
            fprintf(stderr, "  --> %s:%d:%d\n", fn2, note_loc->line, note_loc->col);
            int w2 = get_line_width(note_loc->line);
            fprintf(stderr, " %*s |\n", w2, "");
            char *line2 = get_line_source(g_source, note_loc->line);
            fprintf(stderr, " %*d | %s\n", w2, note_loc->line, line2 ? line2 : "");
            fprintf(stderr, " %*s | ", w2, "");
            for (int i = 1; i < note_loc->col; i++) fputc(' ', stderr);
            fputc('^', stderr);
            if (note_caret_note) fprintf(stderr, " %s", note_caret_note);
            fputc('\n', stderr);
            fprintf(stderr, " %*s |\n", w2, "");
            if (line2) free(line2);
        }
    }

    if (extra_footer_note) {
        fprintf(stderr, "note: %s\n", extra_footer_note);
    }

    exit(1);
}

void fatal_lexer_error(int line, int col, const char *msg) {
    char short_msg[256];
    snprintf(short_msg, sizeof(short_msg), "lexer error: %s", msg);
    ErrorLocation loc = {g_filename, line, col};
    print_formatted_error(short_msg, loc, "unexpected input", NULL, NULL, NULL, NULL);
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
