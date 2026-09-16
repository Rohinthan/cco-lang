#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    TokenArray tokens;
    int current;
    AstArena *arena;
} Parser;

Parser create_parser(TokenArray tokens, AstArena *arena);
AstNode *parse_program(Parser *parser);
void desugar_top_level_program(AstNode *prog, AstArena *arena, const char *source_file);

#endif // PARSER_H
