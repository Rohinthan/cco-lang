// SPDX-License-Identifier: Apache-2.0

#ifndef ERRORS_H
#define ERRORS_H

#include <stdbool.h>

void init_error_reporter(const char *filename, const char *source);
void register_file_source(const char *filename, const char *source);
const char *get_error_filename(void);
const char *get_error_source(void);

void set_diagnostic_codes_enabled(bool enabled);
bool are_diagnostic_codes_enabled(void);

typedef struct {
    const char *filename;
    int line;
    int col;
} ErrorLocation;

typedef struct {
    const char *filename;
    int line;
    int col;
    int end_line;
    int end_col;
} SourceSpan;

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
} DiagnosticSeverity;

typedef struct {
    DiagnosticSeverity severity;
    const char *code;            // e.g. "E1001", or NULL for unnumbered
    const char *message;         // concise error message
    ErrorLocation primary_loc;   // primary source location
    const char *primary_label;   // caret label, e.g. "expected ';' at the end of this statement"
    ErrorLocation secondary_loc; // optional secondary location
    const char *secondary_label; // caret label on secondary location
    const char *secondary_note;  // note title on secondary location
    const char *note;            // optional explanatory footer note
    const char *extra_note;      // optional additional footer note
    const char *help;            // optional suggestion, e.g. "add ';' after `21`"
} Diagnostic;

_Noreturn void emit_diagnostic(const Diagnostic *diag);

_Noreturn void print_formatted_error(const char *short_msg,
                          ErrorLocation primary,
                          const char *primary_caret_note,
                          const char *note_msg,
                          ErrorLocation *note_loc,
                          const char *note_caret_note,
                          const char *extra_footer_note);

_Noreturn void print_formatted_error_code(const char *code,
                                          const char *short_msg,
                                          ErrorLocation primary,
                                          const char *primary_caret_note,
                                          const char *note_msg,
                                          ErrorLocation *note_loc,
                                          const char *note_caret_note,
                                          const char *extra_footer_note);

_Noreturn void fatal_lexer_error(int line, int col, const char *msg);
_Noreturn void fatal_lexer_error_code(const char *code, int line, int col, const char *msg);

_Noreturn void fatal_parser_error(int line, int col, const char *token_str, const char *msg);
_Noreturn void fatal_parser_error_code(const char *code, int line, int col, const char *token_str, const char *msg);

#endif // ERRORS_H
