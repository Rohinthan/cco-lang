#define _POSIX_C_SOURCE 200809L
#include "lexer.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void init_array(TokenArray *array) {
    array->capacity = 32;
    array->count = 0;
    array->tokens = malloc(array->capacity * sizeof(Token));
}

static void append_token(TokenArray *array, Token token) {
    if (array->count >= array->capacity) {
        array->capacity *= 2;
        array->tokens = realloc(array->tokens, array->capacity * sizeof(Token));
    }
    array->tokens[array->count++] = token;
}

void free_tokens(TokenArray *array) {
    if (!array) return;
    for (int i = 0; i < array->count; i++) {
        if (array->tokens[i].lexeme) {
            free(array->tokens[i].lexeme);
        }
    }
    free(array->tokens);
    array->tokens = NULL;
    array->count = 0;
    array->capacity = 0;
}

static TokenType check_keyword(const char *str) {
    if (strcmp(str, "import") == 0) return TOKEN_IMPORT;
    if (strcmp(str, "class") == 0) return TOKEN_CLASS;
    if (strcmp(str, "struct") == 0) return TOKEN_STRUCT;
    if (strcmp(str, "self") == 0) return TOKEN_SELF;
    if (strcmp(str, "fn") == 0) return TOKEN_FN;
    if (strcmp(str, "let") == 0) return TOKEN_LET;
    if (strcmp(str, "if") == 0) return TOKEN_IF;
    if (strcmp(str, "else") == 0) return TOKEN_ELSE;
    if (strcmp(str, "while") == 0) return TOKEN_WHILE;
    if (strcmp(str, "for") == 0) return TOKEN_FOR;
    if (strcmp(str, "in") == 0) return TOKEN_IN;
    if (strcmp(str, "return") == 0) return TOKEN_RETURN;
    if (strcmp(str, "break") == 0) return TOKEN_BREAK;
    if (strcmp(str, "continue") == 0) return TOKEN_CONTINUE;
    if (strcmp(str, "print") == 0) return TOKEN_PRINT;
    if (strcmp(str, "alloc") == 0) return TOKEN_ALLOC;
    if (strcmp(str, "list_new") == 0) return TOKEN_LIST_NEW;
    if (strcmp(str, "map") == 0) return TOKEN_MAP;
    if (strcmp(str, "map_new") == 0) return TOKEN_MAP_NEW;
    if (strcmp(str, "enum") == 0) return TOKEN_ENUM;
    if (strcmp(str, "match") == 0) return TOKEN_MATCH;
    if (strcmp(str, "operator") == 0) return TOKEN_OPERATOR;
    if (strcmp(str, "interface") == 0) return TOKEN_INTERFACE;
    if (strcmp(str, "impl") == 0) return TOKEN_IMPL;
    if (strcmp(str, "Self") == 0) return TOKEN_SELF_TYPE;
    if (strcmp(str, "_") == 0) return TOKEN_UNDERSCORE;
    if (strcmp(str, "int") == 0) return TOKEN_TYPE_INT;
    if (strcmp(str, "float") == 0) return TOKEN_TYPE_FLOAT;
    if (strcmp(str, "char") == 0) return TOKEN_TYPE_CHAR;
    if (strcmp(str, "bool") == 0) return TOKEN_TYPE_BOOL;
    if (strcmp(str, "string") == 0) return TOKEN_TYPE_STRING;
    if (strcmp(str, "void") == 0) return TOKEN_TYPE_VOID;
    if (strcmp(str, "true") == 0) return TOKEN_TRUE;
    if (strcmp(str, "false") == 0) return TOKEN_FALSE;
    return TOKEN_IDENT;
}

TokenArray lex_source(const char *source) {
    TokenArray array;
    init_array(&array);

    int pos = 0;
    int line = 1;
    int col = 1;

    while (source[pos] != '\0') {
        char c = source[pos];

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r') {
            pos++;
            col++;
            continue;
        }
        if (c == '\n') {
            pos++;
            line++;
            col = 1;
            continue;
        }

        // Skip line comments
        if (c == '/' && source[pos + 1] == '/') {
            pos += 2;
            col += 2;
            while (source[pos] != '\0' && source[pos] != '\n') {
                pos++;
                col++;
            }
            continue;
        }

        // Strings
        if (c == '"') {
            int start_col = col;
            pos++; // Skip opening quote
            col++;
            int start_pos = pos;
            while (source[pos] != '\0' && source[pos] != '"') {
                if (source[pos] == '\\' && source[pos + 1] != '\0') {
                    pos += 2;
                    col += 2;
                } else {
                    if (source[pos] == '\n') {
                        line++;
                        col = 1;
                    } else {
                        col++;
                    }
                    pos++;
                }
            }

            if (source[pos] == '\0') {
                fatal_lexer_error(line, start_col, "unterminated string literal");
            }

            int len = pos - start_pos;
            char *buf = malloc(len + 1);
            strncpy(buf, source + start_pos, len);
            buf[len] = '\0';

            pos++; // Skip closing quote
            col++;

            Token tok = {TOKEN_STRING_LIT, buf, line, start_col};
            append_token(&array, tok);
            continue;
        }

        // F-Strings (f"...")
        if (c == 'f' && source[pos + 1] == '"') {
            int start_col = col;
            int start_line = line;
            pos += 2; // Skip f"
            col += 2;
            int start_pos = pos;
            int brace_depth = 0;
            int last_open_brace_col = start_col;
            int last_open_brace_line = start_line;

            while (source[pos] != '\0') {
                if (source[pos] == '"' && brace_depth == 0) {
                    break;
                }
                if (source[pos] == '{') {
                    if (source[pos + 1] == '{') {
                        // {{ escaped brace
                        pos += 2;
                        col += 2;
                        continue;
                    }
                    brace_depth++;
                    last_open_brace_col = col;
                    last_open_brace_line = line;
                    pos++;
                    col++;
                } else if (source[pos] == '}') {
                    if (source[pos + 1] == '}') {
                        // }} escaped brace
                        pos += 2;
                        col += 2;
                        continue;
                    }
                    if (brace_depth > 0) {
                        brace_depth--;
                    } else {
                        fatal_lexer_error(line, col, "unmatched '}' in f-string");
                    }
                    pos++;
                    col++;
                } else if (source[pos] == '\\' && source[pos + 1] != '\0') {
                    pos += 2;
                    col += 2;
                } else if (source[pos] == '\n') {
                    if (brace_depth > 0) {
                        fatal_lexer_error(last_open_brace_line, last_open_brace_col, "unbalanced '{' in f-string");
                    } else {
                        fatal_lexer_error(start_line, start_col, "unterminated f-string literal");
                    }
                } else {
                    col++;
                    pos++;
                }
            }

            if (source[pos] == '\0') {
                if (brace_depth > 0) {
                    fatal_lexer_error(last_open_brace_line, last_open_brace_col, "unbalanced '{' in f-string");
                } else {
                    fatal_lexer_error(start_line, start_col, "unterminated f-string literal");
                }
            }

            if (brace_depth > 0) {
                fatal_lexer_error(last_open_brace_line, last_open_brace_col, "unbalanced '{' in f-string");
            }

            int len = pos - start_pos;
            char *buf = malloc(len + 1);
            strncpy(buf, source + start_pos, len);
            buf[len] = '\0';

            pos++; // Skip closing quote
            col++;

            Token tok = {TOKEN_FSTRING_LIT, buf, start_line, start_col};
            append_token(&array, tok);
            continue;
        }

        // Character Literals
        if (c == '\'') {
            int start_col = col;
            pos++; // Skip opening quote
            col++;
            char ch_val = '\0';
            if (source[pos] == '\\' && source[pos + 1] != '\0') {
                pos++; col++;
                if (source[pos] == 'n') ch_val = '\n';
                else if (source[pos] == 't') ch_val = '\t';
                else if (source[pos] == 'r') ch_val = '\r';
                else if (source[pos] == '0') ch_val = '\0';
                else if (source[pos] == '\\') ch_val = '\\';
                else if (source[pos] == '\'') ch_val = '\'';
                else ch_val = source[pos];
                pos++; col++;
            } else {
                ch_val = source[pos];
                pos++; col++;
            }
            if (source[pos] == '\'') {
                pos++; col++;
            }
            char *buf = malloc(2);
            buf[0] = ch_val;
            buf[1] = '\0';
            Token tok = {TOKEN_CHAR_LIT, buf, line, start_col};
            append_token(&array, tok);
            continue;
        }

        // Numbers (Int or Float)
        if (isdigit(c)) {
            int start_col = col;
            int start_pos = pos;
            int is_float = 0;

            while (isdigit(source[pos]) || source[pos] == '.') {
                if (source[pos] == '.') {
                    if (is_float) break; // Second dot, stop
                    is_float = 1;
                }
                pos++;
                col++;
            }

            int len = pos - start_pos;
            char *buf = malloc(len + 1);
            strncpy(buf, source + start_pos, len);
            buf[len] = '\0';

            Token tok = {is_float ? TOKEN_FLOAT_LIT : TOKEN_INT_LIT, buf, line, start_col};
            append_token(&array, tok);
            continue;
        }

        // Identifiers and Keywords
        if (isalpha(c) || c == '_') {
            int start_col = col;
            int start_pos = pos;

            while (isalnum(source[pos]) || source[pos] == '_') {
                pos++;
                col++;
            }

            int len = pos - start_pos;
            char *buf = malloc(len + 1);
            strncpy(buf, source + start_pos, len);
            buf[len] = '\0';

            TokenType type = check_keyword(buf);
            Token tok = {type, buf, line, start_col};
            append_token(&array, tok);
            continue;
        }

        // Two-character operators
        int start_col = col;
        if (c == '-' && source[pos + 1] == '>') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_ARROW, strdup("->"), line, start_col});
            continue;
        }
        if (c == '=' && source[pos + 1] == '>') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_FAT_ARROW, strdup("=>"), line, start_col});
            continue;
        }
        if (c == '=' && source[pos + 1] == '=') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_EQ, strdup("=="), line, start_col});
            continue;
        }
        if (c == '!' && source[pos + 1] == '=') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_NE, strdup("!="), line, start_col});
            continue;
        }
        if (c == '<' && source[pos + 1] == '=') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_LE, strdup("<="), line, start_col});
            continue;
        }
        if (c == '>' && source[pos + 1] == '=') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_GE, strdup(">="), line, start_col});
            continue;
        }
        if (c == '&' && source[pos + 1] == '&') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_AND, strdup("&&"), line, start_col});
            continue;
        }
        if (c == '|' && source[pos + 1] == '|') {
            pos += 2; col += 2;
            append_token(&array, (Token){TOKEN_OR, strdup("||"), line, start_col});
            continue;
        }

        // Single-character symbols
        TokenType single_type = TOKEN_ERROR;
        switch (c) {
            case '(': single_type = TOKEN_LPAREN; break;
            case ')': single_type = TOKEN_RPAREN; break;
            case '{': single_type = TOKEN_LBRACE; break;
            case '}': single_type = TOKEN_RBRACE; break;
            case '[': single_type = TOKEN_LBRACKET; break;
            case ']': single_type = TOKEN_RBRACKET; break;
            case ',': single_type = TOKEN_COMMA; break;
            case ':': single_type = TOKEN_COLON; break;
            case ';': single_type = TOKEN_SEMICOLON; break;
            case '=': single_type = TOKEN_ASSIGN; break;
            case '<': single_type = TOKEN_LT; break;
            case '>': single_type = TOKEN_GT; break;
            case '+': single_type = TOKEN_PLUS; break;
            case '-': single_type = TOKEN_MINUS; break;
            case '*': single_type = TOKEN_STAR; break;
            case '/': single_type = TOKEN_SLASH; break;
            case '%': single_type = TOKEN_PERCENT; break;
            case '!': single_type = TOKEN_NOT; break;
            case '.': single_type = TOKEN_DOT; break;
            case '&': single_type = TOKEN_AMP; break;
            default: break;
        }

        if (single_type != TOKEN_ERROR) {
            char buf[2] = {c, '\0'};
            append_token(&array, (Token){single_type, strdup(buf), line, start_col});
            pos++;
            col++;
            continue;
        }

        // Unknown character
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "unexpected character '%c'", c);
        fatal_lexer_error(line, col, err_msg);
    }

    append_token(&array, (Token){TOKEN_EOF, strdup("EOF"), line, col});
    return array;
}

const char *token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_IMPORT: return "import";
        case TOKEN_CLASS: return "class";
        case TOKEN_STRUCT: return "struct";
        case TOKEN_SELF: return "self";
        case TOKEN_FN: return "fn";
        case TOKEN_LET: return "let";
        case TOKEN_IF: return "if";
        case TOKEN_ELSE: return "else";
        case TOKEN_WHILE: return "while";
        case TOKEN_FOR: return "for";
        case TOKEN_RETURN: return "return";
        case TOKEN_BREAK: return "break";
        case TOKEN_CONTINUE: return "continue";
        case TOKEN_PRINT: return "print";
        case TOKEN_ALLOC: return "alloc";
        case TOKEN_LIST_NEW: return "list_new";
        case TOKEN_MAP: return "map";
        case TOKEN_MAP_NEW: return "map_new";
        case TOKEN_ENUM: return "enum";
        case TOKEN_MATCH: return "match";
        case TOKEN_OPERATOR: return "operator";
        case TOKEN_INTERFACE: return "interface";
        case TOKEN_IMPL: return "impl";
        case TOKEN_SELF_TYPE: return "Self";
        case TOKEN_UNDERSCORE: return "_";
        case TOKEN_TYPE_INT: return "int";
        case TOKEN_TYPE_FLOAT: return "float";
        case TOKEN_TYPE_CHAR: return "char";
        case TOKEN_TYPE_BOOL: return "bool";
        case TOKEN_TYPE_STRING: return "string";
        case TOKEN_TYPE_VOID: return "void";
        case TOKEN_TRUE: return "true";
        case TOKEN_FALSE: return "false";
        case TOKEN_IDENT: return "IDENT";
        case TOKEN_INT_LIT: return "INT_LIT";
        case TOKEN_FLOAT_LIT: return "FLOAT_LIT";
        case TOKEN_STRING_LIT: return "STRING_LIT";
        case TOKEN_CHAR_LIT: return "CHAR_LIT";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_COMMA: return ",";
        case TOKEN_COLON: return ":";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_ARROW: return "->";
        case TOKEN_FAT_ARROW: return "=>";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_EQ: return "==";
        case TOKEN_NE: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_GT: return ">";
        case TOKEN_LE: return "<=";
        case TOKEN_GE: return ">=";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_NOT: return "!";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_DOT: return ".";
        case TOKEN_AMP: return "&";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void dump_tokens(const TokenArray *array) {
    if (!array) return;
    for (int i = 0; i < array->count; i++) {
        Token tok = array->tokens[i];
        switch (tok.type) {
            case TOKEN_IMPORT:
            case TOKEN_CLASS:
            case TOKEN_STRUCT:
            case TOKEN_SELF:
            case TOKEN_FN:
            case TOKEN_LET:
            case TOKEN_IF:
            case TOKEN_ELSE:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_IN:
            case TOKEN_RETURN:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
            case TOKEN_PRINT:
            case TOKEN_ALLOC:
            case TOKEN_LIST_NEW:
            case TOKEN_MAP:
            case TOKEN_MAP_NEW:
            case TOKEN_ENUM:
            case TOKEN_MATCH:
            case TOKEN_OPERATOR:
            case TOKEN_INTERFACE:
            case TOKEN_IMPL:
            case TOKEN_SELF_TYPE:
            case TOKEN_TYPE_INT:
            case TOKEN_TYPE_FLOAT:
            case TOKEN_TYPE_CHAR:
            case TOKEN_TYPE_BOOL:
            case TOKEN_TYPE_STRING:
            case TOKEN_TYPE_VOID:
            case TOKEN_TRUE:
            case TOKEN_FALSE:
                printf("%d:%d KEYWORD %s\n", tok.line, tok.col, tok.lexeme);
                break;
            case TOKEN_IDENT:
                printf("%d:%d IDENT %s\n", tok.line, tok.col, tok.lexeme);
                break;
            case TOKEN_INT_LIT:
                printf("%d:%d INT %s\n", tok.line, tok.col, tok.lexeme);
                break;
            case TOKEN_FLOAT_LIT:
                printf("%d:%d FLOAT %s\n", tok.line, tok.col, tok.lexeme);
                break;
            case TOKEN_STRING_LIT:
                printf("%d:%d STRING %s\n", tok.line, tok.col, tok.lexeme);
                break;
            case TOKEN_FSTRING_LIT:
                printf("%d:%d FSTRING %s\n", tok.line, tok.col, tok.lexeme);
                break;
            case TOKEN_CHAR_LIT:
                printf("%d:%d CHAR %s\n", tok.line, tok.col, tok.lexeme);
                break;
            case TOKEN_EOF:
                printf("%d:%d EOF\n", tok.line, tok.col);
                break;
            default:
                // Symbols & Operators
                printf("%d:%d SYMBOL %s\n", tok.line, tok.col, tok.lexeme);
                break;
        }
    }
}
