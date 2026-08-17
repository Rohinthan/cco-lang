#ifndef ERRORS_H
#define ERRORS_H

#include <stdbool.h>

void init_error_reporter(const char *filename, const char *source);
void register_file_source(const char *filename, const char *source);
const char *get_error_filename(void);
const char *get_error_source(void);

typedef struct {
    const char *filename;
    int line;
    int col;
} ErrorLocation;

void print_formatted_error(const char *short_msg,
                          ErrorLocation primary,
                          const char *primary_caret_note,
                          const char *note_msg,
                          ErrorLocation *note_loc,
                          const char *note_caret_note,
                          const char *extra_footer_note);

void fatal_lexer_error(int line, int col, const char *msg);
void fatal_parser_error(int line, int col, const char *token_str, const char *msg);

#endif // ERRORS_H
