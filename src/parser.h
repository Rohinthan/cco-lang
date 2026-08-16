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

#endif // PARSER_H
