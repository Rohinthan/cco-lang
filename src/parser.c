#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static Token peek(Parser *p) {
    return p->tokens.tokens[p->current];
}

static Token previous(Parser *p) {
    return p->tokens.tokens[p->current - 1];
}

static bool is_at_end(Parser *p) {
    return peek(p).type == TOKEN_EOF;
}

static Token advance(Parser *p) {
    if (!is_at_end(p)) p->current++;
    return previous(p);
}

static bool check(Parser *p, TokenType type) {
    if (is_at_end(p)) return false;
    return peek(p).type == type;
}

static bool match(Parser *p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

#include "errors.h"

static void error_at(Token token, const char *message) {
    const char *tok_str = (token.type == TOKEN_EOF) ? "end" : token.lexeme;
    fatal_parser_error(token.line, token.col, tok_str, message);
}

static Token consume(Parser *p, TokenType type, const char *message) {
    if (check(p, type)) return advance(p);
    error_at(peek(p), message);
    return (Token){0};
}

Parser create_parser(TokenArray tokens, AstArena *arena) {
    Parser p;
    p.tokens = tokens;
    p.current = 0;
    p.arena = arena;
    return p;
}

// Forward declarations
static AstNode *parse_expr(Parser *p);
static AstNode *parse_block(Parser *p);
static AstNode *parse_statement(Parser *p);

static Type parse_type_full(Parser *p, char **out_class_name, bool *out_is_borrowed, bool *out_is_array, bool *out_is_map, Type *out_key_type, bool *out_is_impl_trait, bool allow_self, bool allow_impl) {
    if (out_class_name) *out_class_name = NULL;
    if (out_is_borrowed) *out_is_borrowed = false;
    if (out_is_array) *out_is_array = false;
    if (out_is_map) *out_is_map = false;
    if (out_key_type) *out_key_type = TY_INT;
    if (out_is_impl_trait) *out_is_impl_trait = false;

    bool is_borrowed = match(p, TOKEN_AMP);

    if (match(p, TOKEN_IMPL)) {
        if (!allow_impl) {
            fatal_parser_error(previous(p).line, previous(p).col, "impl", "'impl Interface' is only supported in function parameter position in v17");
        }
        Token iface_tok = consume(p, TOKEN_IDENT, "Expected interface name after 'impl'");
        if (out_is_impl_trait) *out_is_impl_trait = true;
        if (out_class_name) *out_class_name = arena_strdup(p->arena, iface_tok.lexeme);
        if (out_is_borrowed) *out_is_borrowed = is_borrowed;
        return TY_CLASS;
    }

    if (match(p, TOKEN_MAP)) {
        consume(p, TOKEN_LBRACKET, "Expected '[' after 'map'");
        Type key_t = TY_INT;
        if (match(p, TOKEN_TYPE_INT)) {
            key_t = TY_INT;
        } else if (match(p, TOKEN_TYPE_STRING)) {
            key_t = TY_STRING;
        } else {
            Token err_tok = peek(p);
            const char *got_name = err_tok.lexeme ? err_tok.lexeme : "unknown";
            if (err_tok.type == TOKEN_TYPE_FLOAT) got_name = "float";
            else if (err_tok.type == TOKEN_TYPE_CHAR) got_name = "char";
            else if (err_tok.type == TOKEN_TYPE_BOOL) got_name = "bool";
            else if (err_tok.type == TOKEN_TYPE_VOID) got_name = "void";

            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "map keys must be 'int' or 'string' — '%s' is not allowed in v10", got_name);
            ErrorLocation loc = {get_error_filename(), err_tok.line, err_tok.col};
            print_formatted_error(
                short_msg,
                loc,
                "not allowed in v10",
                "map keys must be hashable and comparable — only 'int' and 'string' keys are supported in v10",
                NULL,
                NULL,
                NULL
            );
            exit(1);
        }
        consume(p, TOKEN_RBRACKET, "Expected ']' after map key type");

        Type val_t = parse_type_full(p, out_class_name, out_is_borrowed, out_is_array, NULL, NULL, NULL, allow_self, false);
        if (out_is_map) *out_is_map = true;
        if (out_key_type) *out_key_type = key_t;
        return val_t;
    }

    Type t = TY_VOID;
    if (match(p, TOKEN_TYPE_INT)) t = TY_INT;
    else if (match(p, TOKEN_TYPE_FLOAT)) t = TY_FLOAT;
    else if (match(p, TOKEN_TYPE_CHAR)) t = TY_CHAR;
    else if (match(p, TOKEN_TYPE_BOOL)) t = TY_BOOL;
    else if (match(p, TOKEN_TYPE_STRING)) t = TY_STRING;
    else if (match(p, TOKEN_TYPE_VOID)) t = TY_VOID;
    else if (match(p, TOKEN_SELF_TYPE)) {
        if (!allow_self) {
            fatal_parser_error(previous(p).line, previous(p).col, "Self", "'Self' is only allowed in interface method signatures");
        }
        if (out_class_name) {
            *out_class_name = arena_strdup(p->arena, "Self");
        }
        if (out_is_borrowed) {
            *out_is_borrowed = is_borrowed;
        }
        t = TY_CLASS;
    }
    else if (match(p, TOKEN_IDENT)) {
        Token tok = previous(p);
        if (out_class_name) {
            *out_class_name = arena_strdup(p->arena, tok.lexeme);
        }
        if (out_is_borrowed) {
            *out_is_borrowed = is_borrowed;
        }
        t = TY_CLASS;
    } else {
        error_at(peek(p), "Expected type specification (int, float, char, bool, string, void, or class identifier)");
        return TY_VOID;
    }

    if (match(p, TOKEN_LBRACKET)) {
        consume(p, TOKEN_RBRACKET, "Expected ']' after '[' in array type");
        if (out_is_array) *out_is_array = true;
    }

    return t;
}

static Type parse_type_with_class(Parser *p, char **out_class_name, bool *out_is_borrowed, bool *out_is_array, bool *out_is_map, Type *out_key_type) {
    return parse_type_full(p, out_class_name, out_is_borrowed, out_is_array, out_is_map, out_key_type, NULL, false, false);
}

static AstNode *parse_fstring_lit(Parser *p, Token tok) {
    const char *raw = tok.lexeme;
    int len = (int)strlen(raw);
    int capacity = 8;
    int count = 0;
    AstNode **parts = (AstNode **)arena_alloc_array(p->arena, capacity, sizeof(AstNode *));

    char *text_buf = (char *)malloc(len + 1);
    int text_len = 0;

    int i = 0;
    while (i < len) {
        if (raw[i] == '{') {
            if (i + 1 < len && raw[i + 1] == '{') {
                text_buf[text_len++] = '{';
                i += 2;
                continue;
            }

            if (text_len > 0) {
                text_buf[text_len] = '\0';
                AstNode *t_node = arena_alloc_node(p->arena, NODE_FSTRING_TEXT, tok.line, tok.col);
                t_node->as.fstring_text.text = arena_strdup(p->arena, text_buf);
                if (count >= capacity) {
                    int new_cap = capacity * 2;
                    AstNode **new_parts = (AstNode **)arena_alloc_array(p->arena, new_cap, sizeof(AstNode *));
                    memcpy(new_parts, parts, count * sizeof(AstNode *));
                    parts = new_parts;
                    capacity = new_cap;
                }
                parts[count++] = t_node;
                text_len = 0;
            }

            i++; // skip '{'
            int expr_start = i;
            int depth = 1;
            while (i < len && depth > 0) {
                if (raw[i] == '{') {
                    if (i + 1 < len && raw[i + 1] == '{') {
                        i += 2;
                        continue;
                    }
                    depth++;
                } else if (raw[i] == '}') {
                    if (i + 1 < len && raw[i + 1] == '}') {
                        i += 2;
                        continue;
                    }
                    depth--;
                    if (depth == 0) {
                        break;
                    }
                }
                i++;
            }

            if (depth > 0) {
                free(text_buf);
                fatal_parser_error(tok.line, tok.col, "{", "unbalanced '{' in f-string");
            }

            int expr_len = i - expr_start;
            i++; // skip closing '}'

            int s = expr_start;
            while (s < expr_start + expr_len && isspace((unsigned char)raw[s])) s++;
            int e = expr_start + expr_len - 1;
            while (e >= s && isspace((unsigned char)raw[e])) e--;
            if (s > e) {
                free(text_buf);
                fatal_parser_error(tok.line, tok.col, "{}", "expected expression inside '{...}'");
            }

            char *expr_str = (char *)malloc(expr_len + 1);
            strncpy(expr_str, raw + expr_start, expr_len);
            expr_str[expr_len] = '\0';

            TokenArray sub_tokens = lex_source(expr_str);
            free(expr_str);

            Parser sub_p = create_parser(sub_tokens, p->arena);
            AstNode *sub_expr = parse_expr(&sub_p);

            if (sub_p.tokens.tokens[sub_p.current].type != TOKEN_EOF) {
                Token extra_tok = sub_p.tokens.tokens[sub_p.current];
                free(text_buf);
                free_tokens(&sub_tokens);
                fatal_parser_error(tok.line, tok.col, extra_tok.lexeme, "unexpected tokens in f-string expression");
            }

            free_tokens(&sub_tokens);

            if (count >= capacity) {
                int new_cap = capacity * 2;
                AstNode **new_parts = (AstNode **)arena_alloc_array(p->arena, new_cap, sizeof(AstNode *));
                memcpy(new_parts, parts, count * sizeof(AstNode *));
                parts = new_parts;
                capacity = new_cap;
            }
            parts[count++] = sub_expr;
        } else if (raw[i] == '}') {
            if (i + 1 < len && raw[i + 1] == '}') {
                text_buf[text_len++] = '}';
                i += 2;
                continue;
            } else {
                free(text_buf);
                fatal_parser_error(tok.line, tok.col, "}", "unmatched '}' in f-string");
            }
        } else {
            text_buf[text_len++] = raw[i++];
        }
    }

    if (text_len > 0) {
        text_buf[text_len] = '\0';
        AstNode *t_node = arena_alloc_node(p->arena, NODE_FSTRING_TEXT, tok.line, tok.col);
        t_node->as.fstring_text.text = arena_strdup(p->arena, text_buf);
        if (count >= capacity) {
            int new_cap = capacity * 2;
            AstNode **new_parts = (AstNode **)arena_alloc_array(p->arena, new_cap, sizeof(AstNode *));
            memcpy(new_parts, parts, count * sizeof(AstNode *));
            parts = new_parts;
            capacity = new_cap;
        }
        parts[count++] = t_node;
    }

    free(text_buf);

    if (count == 0) {
        AstNode *empty_t = arena_alloc_node(p->arena, NODE_FSTRING_TEXT, tok.line, tok.col);
        empty_t->as.fstring_text.text = arena_strdup(p->arena, "");
        parts = (AstNode **)arena_alloc_array(p->arena, 1, sizeof(AstNode *));
        parts[0] = empty_t;
        count = 1;
    }

    AstNode *node = arena_alloc_node(p->arena, NODE_FSTRING, tok.line, tok.col);
    node->as.fstring.parts = parts;
    node->as.fstring.part_count = count;
    return node;
}

static AstNode *parse_primary(Parser *p) {
    Token tok = peek(p);

    if (match(p, TOKEN_INT_LIT)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line, tok.col);
        node->as.literal.lit_type = TY_INT;
        node->as.literal.val.i = atol(tok.lexeme);
        return node;
    }

    if (match(p, TOKEN_FLOAT_LIT)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line, tok.col);
        node->as.literal.lit_type = TY_FLOAT;
        node->as.literal.val.f = atof(tok.lexeme);
        return node;
    }

    if (match(p, TOKEN_STRING_LIT)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line, tok.col);
        node->as.literal.lit_type = TY_STRING;
        node->as.literal.val.s = arena_strdup(p->arena, tok.lexeme);
        return node;
    }

    if (match(p, TOKEN_FSTRING_LIT)) {
        Token f_tok = previous(p);
        return parse_fstring_lit(p, f_tok);
    }

    if (match(p, TOKEN_CHAR_LIT)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line, tok.col);
        node->as.literal.lit_type = TY_CHAR;
        node->as.literal.val.c = tok.lexeme[0];
        return node;
    }

    if (match(p, TOKEN_TRUE)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line, tok.col);
        node->as.literal.lit_type = TY_BOOL;
        node->as.literal.val.b = true;
        return node;
    }

    if (match(p, TOKEN_FALSE)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line, tok.col);
        node->as.literal.lit_type = TY_BOOL;
        node->as.literal.val.b = false;
        return node;
    }

    if (match(p, TOKEN_ALLOC)) {
        Token tok = previous(p);
        consume(p, TOKEN_LPAREN, "Expected '(' after 'alloc'");
        char *alloc_cls = NULL;
        Type elem_type = parse_type_with_class(p, &alloc_cls, NULL, NULL, NULL, NULL);
        consume(p, TOKEN_COMMA, "Expected ',' after alloc type");
        AstNode *count_expr = parse_expr(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after alloc count expression");

        AstNode *node = arena_alloc_node(p->arena, NODE_ALLOC, tok.line, tok.col);
        node->as.alloc.elem_type = elem_type;
        node->as.alloc.class_name = alloc_cls;
        node->as.alloc.count_expr = count_expr;
        node->as.alloc.is_list = false;
        return node;
    }

    if (match(p, TOKEN_LIST_NEW)) {
        Token tok = previous(p);
        consume(p, TOKEN_LPAREN, "Expected '(' after 'list_new'");
        char *alloc_cls = NULL;
        Type elem_type = parse_type_with_class(p, &alloc_cls, NULL, NULL, NULL, NULL);
        consume(p, TOKEN_RPAREN, "Expected ')' after list_new type");

        AstNode *node = arena_alloc_node(p->arena, NODE_ALLOC, tok.line, tok.col);
        node->as.alloc.elem_type = elem_type;
        node->as.alloc.class_name = alloc_cls;
        node->as.alloc.count_expr = NULL;
        node->as.alloc.is_list = true;
        node->as.alloc.is_map = false;
        return node;
    }

    if (match(p, TOKEN_MAP_NEW)) {
        Token tok = previous(p);
        consume(p, TOKEN_LPAREN, "Expected '(' after 'map_new'");
        Type key_t = TY_INT;
        if (match(p, TOKEN_TYPE_INT)) {
            key_t = TY_INT;
        } else if (match(p, TOKEN_TYPE_STRING)) {
            key_t = TY_STRING;
        } else {
            Token err_tok = peek(p);
            const char *got_name = err_tok.lexeme ? err_tok.lexeme : "unknown";
            if (err_tok.type == TOKEN_TYPE_FLOAT) got_name = "float";
            else if (err_tok.type == TOKEN_TYPE_CHAR) got_name = "char";
            else if (err_tok.type == TOKEN_TYPE_BOOL) got_name = "bool";
            else if (err_tok.type == TOKEN_TYPE_VOID) got_name = "void";

            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "map keys must be 'int' or 'string' — '%s' is not allowed in v10", got_name);
            ErrorLocation loc = {get_error_filename(), err_tok.line, err_tok.col};
            print_formatted_error(
                short_msg,
                loc,
                "not allowed in v10",
                "map keys must be hashable and comparable — only 'int' and 'string' keys are supported in v10",
                NULL,
                NULL,
                NULL
            );
            exit(1);
        }
        consume(p, TOKEN_COMMA, "Expected ',' after key type in map_new");
        char *alloc_cls = NULL;
        Type elem_type = parse_type_with_class(p, &alloc_cls, NULL, NULL, NULL, NULL);
        consume(p, TOKEN_RPAREN, "Expected ')' after map_new types");

        AstNode *node = arena_alloc_node(p->arena, NODE_ALLOC, tok.line, tok.col);
        node->as.alloc.elem_type = elem_type;
        node->as.alloc.class_name = alloc_cls;
        node->as.alloc.count_expr = NULL;
        node->as.alloc.is_list = false;
        node->as.alloc.is_map = true;
        node->as.alloc.key_type = key_t;
        return node;
    }

    if (match(p, TOKEN_IDENT) || match(p, TOKEN_SELF)) {
        Token id_tok = previous(p);
        if (isupper((unsigned char)id_tok.lexeme[0]) && check(p, TOKEN_DOT) && p->current + 1 < p->tokens.count && p->tokens.tokens[p->current + 1].type == TOKEN_IDENT && isupper((unsigned char)p->tokens.tokens[p->current + 1].lexeme[0])) {
            advance(p); // consume '.'
            Token var_tok = advance(p); // consume variant name
            char **field_names = NULL;
            AstNode **field_values = NULL;
            int field_count = 0;
            int field_cap = 0;

            if (match(p, TOKEN_LBRACE)) {
                if (!check(p, TOKEN_RBRACE)) {
                    do {
                        if (check(p, TOKEN_RBRACE)) break;
                        Token f_tok = consume(p, TOKEN_IDENT, "Expected field name in variant instantiation");
                        consume(p, TOKEN_COLON, "Expected ':' after field name");
                        AstNode *val = parse_expr(p);

                        if (field_count >= field_cap) {
                            field_cap = field_cap == 0 ? 4 : field_cap * 2;
                            char **new_names = (char **)arena_alloc_array(p->arena, field_cap, sizeof(char *));
                            AstNode **new_vals = (AstNode **)arena_alloc_array(p->arena, field_cap, sizeof(AstNode *));
                            if (field_names) memcpy(new_names, field_names, field_count * sizeof(char *));
                            if (field_values) memcpy(new_vals, field_values, field_count * sizeof(AstNode *));
                            field_names = new_names;
                            field_values = new_vals;
                        }
                        field_names[field_count] = arena_strdup(p->arena, f_tok.lexeme);
                        field_values[field_count] = val;
                        field_count++;
                    } while (match(p, TOKEN_COMMA));
                }
                consume(p, TOKEN_RBRACE, "Expected '}' after variant field initializers");
            }

            AstNode *node = arena_alloc_node(p->arena, NODE_NEW, id_tok.line, id_tok.col);
            node->as.new_expr.class_name = arena_strdup(p->arena, id_tok.lexeme);
            node->as.new_expr.variant_name = arena_strdup(p->arena, var_tok.lexeme);
            node->as.new_expr.field_names = field_names;
            node->as.new_expr.field_values = field_values;
            node->as.new_expr.field_count = field_count;
            node->as.new_expr.constructs_enum = true;
            node->as.new_expr.constructs_struct = false;
            return node;
        }

        if (isupper((unsigned char)id_tok.lexeme[0]) && match(p, TOKEN_LBRACE)) {
            // New expression: ClassName { field1: expr1, field2: expr2 }
            char **field_names = NULL;
            AstNode **field_values = NULL;
            int field_count = 0;
            int field_cap = 0;

            if (!check(p, TOKEN_RBRACE)) {
                do {
                    if (check(p, TOKEN_RBRACE)) break;
                    Token f_tok = consume(p, TOKEN_IDENT, "Expected field name in object instantiation");
                    consume(p, TOKEN_COLON, "Expected ':' after field name");
                    AstNode *val = parse_expr(p);

                    if (field_count >= field_cap) {
                        field_cap = field_cap == 0 ? 4 : field_cap * 2;
                        char **new_names = (char **)arena_alloc_array(p->arena, field_cap, sizeof(char *));
                        AstNode **new_vals = (AstNode **)arena_alloc_array(p->arena, field_cap, sizeof(AstNode *));
                        if (field_names) memcpy(new_names, field_names, field_count * sizeof(char *));
                        if (field_values) memcpy(new_vals, field_values, field_count * sizeof(AstNode *));
                        field_names = new_names;
                        field_values = new_vals;
                    }
                    field_names[field_count] = arena_strdup(p->arena, f_tok.lexeme);
                    field_values[field_count] = val;
                    field_count++;
                } while (match(p, TOKEN_COMMA));
            }

            consume(p, TOKEN_RBRACE, "Expected '}' after object field initializers");

            AstNode *node = arena_alloc_node(p->arena, NODE_NEW, id_tok.line, id_tok.col);
            node->as.new_expr.class_name = arena_strdup(p->arena, id_tok.lexeme);
            node->as.new_expr.variant_name = NULL;
            node->as.new_expr.field_names = field_names;
            node->as.new_expr.field_values = field_values;
            node->as.new_expr.field_count = field_count;
            node->as.new_expr.constructs_enum = false;
            node->as.new_expr.constructs_struct = false;
            return node;
        }

        AstNode *node = arena_alloc_node(p->arena, NODE_IDENT, id_tok.line, id_tok.col);
        node->as.ident.name = arena_strdup(p->arena, id_tok.lexeme);
        return node;
    }

    if (match(p, TOKEN_LPAREN)) {
        AstNode *expr = parse_expr(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    error_at(tok, "Expected expression");
    return NULL;
}

static AstNode *parse_call_or_index(Parser *p) {
    AstNode *expr = parse_primary(p);

    while (true) {
        if (match(p, TOKEN_DOT)) {
            Token m_tok = consume(p, TOKEN_IDENT, "Expected field or method name after '.'");
            if (match(p, TOKEN_LPAREN)) {
                // Method call: expr.method(args)
                AstNode **args = NULL;
                int arg_count = 0;
                int arg_cap = 0;

                if (!check(p, TOKEN_RPAREN)) {
                    do {
                        AstNode *arg = parse_expr(p);
                        if (arg_count >= arg_cap) {
                            arg_cap = arg_cap == 0 ? 4 : arg_cap * 2;
                            AstNode **new_args = (AstNode **)arena_alloc_array(p->arena, arg_cap, sizeof(AstNode *));
                            if (args) memcpy(new_args, args, arg_count * sizeof(AstNode *));
                            args = new_args;
                        }
                        args[arg_count++] = arg;
                    } while (match(p, TOKEN_COMMA));
                }
                consume(p, TOKEN_RPAREN, "Expected ')' after method arguments");

                AstNode *mc_node = arena_alloc_node(p->arena, NODE_METHOD_CALL, m_tok.line, m_tok.col);
                mc_node->as.method_call.object = expr;
                mc_node->as.method_call.method_name = arena_strdup(p->arena, m_tok.lexeme);
                mc_node->as.method_call.args = args;
                mc_node->as.method_call.arg_count = arg_count;
                expr = mc_node;
            } else {
                // Member access: expr.member
                AstNode *m_node = arena_alloc_node(p->arena, NODE_MEMBER, m_tok.line, m_tok.col);
                m_node->as.member.object = expr;
                m_node->as.member.member_name = arena_strdup(p->arena, m_tok.lexeme);
                expr = m_node;
            }
        } else if (match(p, TOKEN_LPAREN)) {
            // Function call
            if (expr->type != NODE_IDENT) {
                error_at(previous(p), "Expected identifier before '(' in call");
            }
            char *callee = arena_strdup(p->arena, expr->as.ident.name);
            AstNode **args = NULL;
            int arg_count = 0;
            int arg_cap = 0;

            if (!check(p, TOKEN_RPAREN)) {
                do {
                    AstNode *arg = parse_expr(p);
                    if (arg_count >= arg_cap) {
                        arg_cap = arg_cap == 0 ? 4 : arg_cap * 2;
                        AstNode **new_args = (AstNode **)arena_alloc_array(p->arena, arg_cap, sizeof(AstNode *));
                        if (args) memcpy(new_args, args, arg_count * sizeof(AstNode *));
                        args = new_args;
                    }
                    args[arg_count++] = arg;
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after arguments");

            AstNode *call_node = arena_alloc_node(p->arena, NODE_CALL, expr->line, expr->col);
            call_node->as.call.callee = callee;
            call_node->as.call.args = args;
            call_node->as.call.arg_count = arg_count;
            expr = call_node;
        } else if (match(p, TOKEN_LBRACKET)) {
            // Array indexing
            AstNode *array_expr = expr;
            char *arr_name = NULL;
            if (expr->type == NODE_IDENT) {
                arr_name = arena_strdup(p->arena, expr->as.ident.name);
            } else if (expr->type == NODE_MEMBER) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%s->%s",
                         (expr->as.member.object->type == NODE_IDENT) ? expr->as.member.object->as.ident.name : "self",
                         expr->as.member.member_name);
                arr_name = arena_strdup(p->arena, buf);
            }
            AstNode *index_expr = parse_expr(p);
            consume(p, TOKEN_RBRACKET, "Expected ']' after array index expression");

            AstNode *idx_node = arena_alloc_node(p->arena, NODE_INDEX, expr->line, expr->col);
            idx_node->as.index.array_expr = array_expr;
            idx_node->as.index.array_name = arr_name;
            idx_node->as.index.index = index_expr;
            expr = idx_node;
        } else {
            break;
        }
    }

    return expr;
}

static AstNode *parse_unary(Parser *p) {
    if (match(p, TOKEN_NOT) || match(p, TOKEN_MINUS) || match(p, TOKEN_AMP)) {
        Token op = previous(p);
        AstNode *operand = parse_unary(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_UNARY, op.line, op.col);
        snprintf(node->as.unary.op, sizeof(node->as.unary.op), "%s", op.lexeme);
        node->as.unary.operand = operand;
        return node;
    }

    return parse_call_or_index(p);
}

static AstNode *parse_factor(Parser *p) {
    AstNode *expr = parse_unary(p);

    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        Token op = previous(p);
        AstNode *right = parse_unary(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line, op.col);
        snprintf(node->as.binary.op, sizeof(node->as.binary.op), "%s", op.lexeme);
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }

    return expr;
}

static AstNode *parse_term(Parser *p) {
    AstNode *expr = parse_factor(p);

    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        Token op = previous(p);
        AstNode *right = parse_factor(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line, op.col);
        snprintf(node->as.binary.op, sizeof(node->as.binary.op), "%s", op.lexeme);
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }

    return expr;
}

static AstNode *parse_comparison(Parser *p) {
    AstNode *expr = parse_term(p);

    while (match(p, TOKEN_LT) || match(p, TOKEN_GT) || match(p, TOKEN_LE) || match(p, TOKEN_GE)) {
        Token op = previous(p);
        AstNode *right = parse_term(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line, op.col);
        snprintf(node->as.binary.op, sizeof(node->as.binary.op), "%s", op.lexeme);
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }

    return expr;
}

static AstNode *parse_equality(Parser *p) {
    AstNode *expr = parse_comparison(p);

    while (match(p, TOKEN_EQ) || match(p, TOKEN_NE)) {
        Token op = previous(p);
        AstNode *right = parse_comparison(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line, op.col);
        snprintf(node->as.binary.op, sizeof(node->as.binary.op), "%s", op.lexeme);
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }

    return expr;
}

static AstNode *parse_logic_and(Parser *p) {
    AstNode *expr = parse_equality(p);

    while (match(p, TOKEN_AND)) {
        Token op = previous(p);
        AstNode *right = parse_equality(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line, op.col);
        snprintf(node->as.binary.op, sizeof(node->as.binary.op), "%s", op.lexeme);
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }

    return expr;
}

static AstNode *parse_logic_or(Parser *p) {
    AstNode *expr = parse_logic_and(p);

    while (match(p, TOKEN_OR)) {
        Token op = previous(p);
        AstNode *right = parse_logic_and(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line, op.col);
        snprintf(node->as.binary.op, sizeof(node->as.binary.op), "%s", op.lexeme);
        node->as.binary.left = expr;
        node->as.binary.right = right;
        expr = node;
    }

    return expr;
}

static AstNode *parse_expr(Parser *p) {
    return parse_logic_or(p);
}

static AstNode *parse_let_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_LET, "Expected 'let'");
    Token name_tok = consume(p, TOKEN_IDENT, "Expected variable name after 'let'");

    Type var_type = TY_INT;
    char *class_name = NULL;
    bool is_array = false;
    bool is_map = false;
    Type key_type = TY_INT;
    if (match(p, TOKEN_COLON)) {
        var_type = parse_type_with_class(p, &class_name, NULL, &is_array, &is_map, &key_type);
    }

    consume(p, TOKEN_ASSIGN, "Expected '=' in variable declaration");
    AstNode *value = parse_expr(p);
    consume(p, TOKEN_SEMICOLON, "Expected ';' after let statement");

    AstNode *node = arena_alloc_node(p->arena, NODE_LET, tok.line, tok.col);
    node->as.let.name = arena_strdup(p->arena, name_tok.lexeme);
    node->as.let.var_type = var_type;
    node->as.let.is_array = is_array;
    node->as.let.is_map = is_map;
    node->as.let.key_type = key_type;
    node->as.let.class_name = class_name;
    node->as.let.value = value;
    return node;
}

static AstNode *parse_assign_or_expr_stmt(Parser *p) {
    AstNode *lhs = parse_expr(p);

    if (match(p, TOKEN_ASSIGN)) {
        AstNode *value = parse_expr(p);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after assignment");

        if (lhs->type == NODE_IDENT) {
            AstNode *node = arena_alloc_node(p->arena, NODE_ASSIGN, lhs->line, lhs->col);
            node->as.assign.name = arena_strdup(p->arena, lhs->as.ident.name);
            node->as.assign.value = value;
            return node;
        } else if (lhs->type == NODE_INDEX) {
            AstNode *node = arena_alloc_node(p->arena, NODE_INDEX_ASSIGN, lhs->line, lhs->col);
            node->as.index_assign.array_expr = lhs->as.index.array_expr;
            node->as.index_assign.array_name = lhs->as.index.array_name;
            node->as.index_assign.index = lhs->as.index.index;
            node->as.index_assign.value = value;
            return node;
        } else if (lhs->type == NODE_MEMBER) {
            AstNode *node = arena_alloc_node(p->arena, NODE_MEMBER_ASSIGN, lhs->line, lhs->col);
            node->as.member_assign.object = lhs->as.member.object;
            node->as.member_assign.member_name = arena_strdup(p->arena, lhs->as.member.member_name);
            node->as.member_assign.value = value;
            return node;
        } else {
            error_at(previous(p), "Invalid lvalue for assignment");
            return lhs;
        }
    }

    consume(p, TOKEN_SEMICOLON, "Expected ';' after expression statement");
    AstNode *node = arena_alloc_node(p->arena, NODE_EXPR_STMT, lhs->line, lhs->col);
    node->as.expr_stmt.expr = lhs;
    return node;
}

static AstNode *parse_if_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_IF, "Expected 'if'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'if'");
    AstNode *cond = parse_expr(p);
    consume(p, TOKEN_RPAREN, "Expected ')' after if condition");

    AstNode *then_b = parse_block(p);
    AstNode *else_b = NULL;

    if (match(p, TOKEN_ELSE)) {
        if (check(p, TOKEN_IF)) {
            else_b = parse_if_stmt(p);
        } else {
            else_b = parse_block(p);
        }
    }

    AstNode *node = arena_alloc_node(p->arena, NODE_IF, tok.line, tok.col);
    node->as.if_stmt.cond = cond;
    node->as.if_stmt.then_b = then_b;
    node->as.if_stmt.else_b = else_b;
    return node;
}

static AstNode *parse_while_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_WHILE, "Expected 'while'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'while'");
    AstNode *cond = parse_expr(p);
    consume(p, TOKEN_RPAREN, "Expected ')' after while condition");

    AstNode *body = parse_block(p);

    AstNode *node = arena_alloc_node(p->arena, NODE_WHILE, tok.line, tok.col);
    node->as.while_stmt.cond = cond;
    node->as.while_stmt.body = body;
    return node;
}

static AstNode *parse_for_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_FOR, "Expected 'for'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'for'");

    AstNode *init = NULL;
    if (check(p, TOKEN_LET)) {
        init = parse_let_stmt(p);
    } else {
        init = parse_assign_or_expr_stmt(p);
    }

    AstNode *cond = parse_expr(p);
    consume(p, TOKEN_SEMICOLON, "Expected ';' after for condition");

    // Step statement
    Token step_tok = peek(p);
    AstNode *step = NULL;
    if (check(p, TOKEN_IDENT)) {
        Token name_tok = advance(p);
        if (match(p, TOKEN_ASSIGN)) {
            AstNode *val = parse_expr(p);
            step = arena_alloc_node(p->arena, NODE_ASSIGN, step_tok.line, step_tok.col);
            step->as.assign.name = arena_strdup(p->arena, name_tok.lexeme);
            step->as.assign.value = val;
        } else if (match(p, TOKEN_LBRACKET)) {
            AstNode *idx = parse_expr(p);
            consume(p, TOKEN_RBRACKET, "Expected ']'");
            consume(p, TOKEN_ASSIGN, "Expected '='");
            AstNode *val = parse_expr(p);
            step = arena_alloc_node(p->arena, NODE_INDEX_ASSIGN, step_tok.line, step_tok.col);
            step->as.index_assign.array_name = arena_strdup(p->arena, name_tok.lexeme);
            step->as.index_assign.index = idx;
            step->as.index_assign.value = val;
        } else {
            p->current--;
            step = parse_expr(p);
        }
    } else {
        step = parse_expr(p);
    }

    consume(p, TOKEN_RPAREN, "Expected ')' after for clauses");
    AstNode *body = parse_block(p);

    AstNode *node = arena_alloc_node(p->arena, NODE_FOR, tok.line, tok.col);
    node->as.for_stmt.init = init;
    node->as.for_stmt.cond = cond;
    node->as.for_stmt.step = step;
    node->as.for_stmt.body = body;
    return node;
}

static AstNode *parse_return_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_RETURN, "Expected 'return'");
    AstNode *val = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        val = parse_expr(p);
    }
    consume(p, TOKEN_SEMICOLON, "Expected ';' after return");

    AstNode *node = arena_alloc_node(p->arena, NODE_RETURN, tok.line, tok.col);
    node->as.return_stmt.value = val;
    return node;
}

static AstNode *parse_break_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_BREAK, "Expected 'break'");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after break");
    return arena_alloc_node(p->arena, NODE_BREAK, tok.line, tok.col);
}

static AstNode *parse_continue_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_CONTINUE, "Expected 'continue'");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after continue");
    return arena_alloc_node(p->arena, NODE_CONTINUE, tok.line, tok.col);
}

static AstNode *parse_print_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_PRINT, "Expected 'print'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'print'");
    AstNode *val = parse_expr(p);
    consume(p, TOKEN_RPAREN, "Expected ')' after print argument");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after print statement");

    AstNode *node = arena_alloc_node(p->arena, NODE_PRINT, tok.line, tok.col);
    node->as.print_stmt.value = val;
    return node;
}

static AstNode *parse_block(Parser *p) {
    Token tok = consume(p, TOKEN_LBRACE, "Expected '{'");
    AstNode **stmts = NULL;
    int count = 0;
    int cap = 0;

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        AstNode *stmt = parse_statement(p);
        if (count >= cap) {
            cap = cap == 0 ? 8 : cap * 2;
            AstNode **new_stmts = (AstNode **)arena_alloc_array(p->arena, cap, sizeof(AstNode *));
            if (stmts) memcpy(new_stmts, stmts, count * sizeof(AstNode *));
            stmts = new_stmts;
        }
        stmts[count++] = stmt;
    }

    consume(p, TOKEN_RBRACE, "Expected '}' after block");

    AstNode *node = arena_alloc_node(p->arena, NODE_BLOCK, tok.line, tok.col);
    node->as.block.stmts = stmts;
    node->as.block.count = count;
    return node;
}

static AstNode *parse_for_each_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_FOR, "Expected 'for'");
    Token var_tok = consume(p, TOKEN_IDENT, "Expected loop variable after 'for'");
    consume(p, TOKEN_IN, "Expected 'in' after loop variable");
    AstNode *coll_expr = parse_expr(p);
    AstNode *body = parse_block(p);

    AstNode *node = arena_alloc_node(p->arena, NODE_FOR_EACH, tok.line, tok.col);
    node->as.for_each.loop_var_name = arena_strdup(p->arena, var_tok.lexeme);
    node->as.for_each.collection_expr = coll_expr;
    node->as.for_each.body = body;
    return node;
}

static AstNode *parse_match_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_MATCH, "Expected 'match'");
    AstNode *expr = parse_expr(p);
    consume(p, TOKEN_LBRACE, "Expected '{' after match expression");

    AstNode **arms = NULL;
    int arm_count = 0;
    int arm_cap = 0;

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        Token arm_tok = peek(p);
        bool is_wildcard = false;
        char *enum_name = NULL;
        char *variant_name = NULL;
        char **bind_names = NULL;
        int bind_count = 0;
        int bind_cap = 0;

        if (match(p, TOKEN_UNDERSCORE)) {
            is_wildcard = true;
        } else {
            Token e_tok = consume(p, TOKEN_IDENT, "Expected enum name or '_' in match arm");
            enum_name = arena_strdup(p->arena, e_tok.lexeme);
            consume(p, TOKEN_DOT, "Expected '.' after enum name in match arm");
            Token v_tok = consume(p, TOKEN_IDENT, "Expected variant name after '.'");
            variant_name = arena_strdup(p->arena, v_tok.lexeme);

            if (match(p, TOKEN_LBRACE)) {
                if (!check(p, TOKEN_RBRACE)) {
                    do {
                        Token b_tok = consume(p, TOKEN_IDENT, "Expected field name in pattern binding");
                        if (check(p, TOKEN_COLON)) {
                            Token col_tok = advance(p);
                            Token ren_tok = peek(p);
                            char short_msg[256];
                            snprintf(short_msg, sizeof(short_msg), "match bindings must use the field's own name — expected '%s', found '%s'", b_tok.lexeme, ren_tok.lexeme ? ren_tok.lexeme : "");
                            ErrorLocation loc = {get_error_filename(), col_tok.line, col_tok.col};
                            print_formatted_error(
                                short_msg,
                                loc,
                                "field renaming not allowed in v11",
                                "bound local variable names must exactly match the variant's declared field names in v11",
                                NULL,
                                NULL,
                                NULL
                            );
                            exit(1);
                        }

                        if (bind_count >= bind_cap) {
                            bind_cap = bind_cap == 0 ? 4 : bind_cap * 2;
                            char **new_b = (char **)arena_alloc_array(p->arena, bind_cap, sizeof(char *));
                            if (bind_names) memcpy(new_b, bind_names, bind_count * sizeof(char *));
                            bind_names = new_b;
                        }
                        bind_names[bind_count++] = arena_strdup(p->arena, b_tok.lexeme);
                    } while (match(p, TOKEN_COMMA));
                }
                consume(p, TOKEN_RBRACE, "Expected '}' after pattern bindings");
            }
        }

        consume(p, TOKEN_FAT_ARROW, "Expected '=>' after pattern");
        AstNode *body = parse_block(p);

        AstNode *arm_node = arena_alloc_node(p->arena, NODE_MATCH_ARM, arm_tok.line, arm_tok.col);
        arm_node->as.match_arm.enum_name = enum_name;
        arm_node->as.match_arm.variant_name = variant_name;
        arm_node->as.match_arm.is_wildcard = is_wildcard;
        arm_node->as.match_arm.bind_names = bind_names;
        arm_node->as.match_arm.bind_count = bind_count;
        arm_node->as.match_arm.body = body;
        arm_node->as.match_arm.arm_line = arm_tok.line;
        arm_node->as.match_arm.arm_col = arm_tok.col;

        if (arm_count >= arm_cap) {
            arm_cap = arm_cap == 0 ? 4 : arm_cap * 2;
            AstNode **new_arms = (AstNode **)arena_alloc_array(p->arena, arm_cap, sizeof(AstNode *));
            if (arms) memcpy(new_arms, arms, arm_count * sizeof(AstNode *));
            arms = new_arms;
        }
        arms[arm_count++] = arm_node;
    }

    consume(p, TOKEN_RBRACE, "Expected '}' after match arms");

    AstNode *match_node = arena_alloc_node(p->arena, NODE_MATCH, tok.line, tok.col);
    match_node->as.match_stmt.expr = expr;
    match_node->as.match_stmt.arms = arms;
    match_node->as.match_stmt.arm_count = arm_count;
    match_node->as.match_stmt.enum_name = NULL;
    return match_node;
}

static AstNode *parse_statement(Parser *p) {
    if (check(p, TOKEN_FN) || check(p, TOKEN_CLASS) || check(p, TOKEN_STRUCT) || check(p, TOKEN_ENUM) || check(p, TOKEN_INTERFACE) || check(p, TOKEN_IMPL) || check(p, TOKEN_IMPORT)) {
        Token tok = peek(p);
        char short_msg[256];
        snprintf(short_msg, sizeof(short_msg), "unexpected top-level declaration '%s' inside a block — missing '}' on the preceding block/function?", tok.lexeme);
        ErrorLocation primary = {get_error_filename(), tok.line, tok.col};
        print_formatted_error(
            short_msg,
            primary,
            "unexpected declaration inside block",
            "top-level declarations ('fn', 'class', 'struct', 'enum', 'interface', 'impl', 'import') cannot be nested inside function or loop blocks — check if a closing brace '}' is missing above",
            NULL,
            NULL,
            NULL
        );
        exit(1);
    }
    if (check(p, TOKEN_LET)) return parse_let_stmt(p);
    if (check(p, TOKEN_IF)) return parse_if_stmt(p);
    if (check(p, TOKEN_WHILE)) return parse_while_stmt(p);
    if (check(p, TOKEN_FOR)) {
        if (p->current + 1 < p->tokens.count && p->tokens.tokens[p->current + 1].type == TOKEN_LPAREN) {
            return parse_for_stmt(p);
        } else {
            return parse_for_each_stmt(p);
        }
    }
    if (check(p, TOKEN_MATCH)) return parse_match_stmt(p);
    if (check(p, TOKEN_RETURN)) return parse_return_stmt(p);
    if (check(p, TOKEN_BREAK)) return parse_break_stmt(p);
    if (check(p, TOKEN_CONTINUE)) return parse_continue_stmt(p);
    if (check(p, TOKEN_PRINT)) return parse_print_stmt(p);
    if (check(p, TOKEN_LBRACE)) return parse_block(p);
    return parse_assign_or_expr_stmt(p);
}

static AstNode *parse_class(Parser *p) {
    Token tok = consume(p, TOKEN_CLASS, "Expected 'class'");
    Token name_tok = consume(p, TOKEN_IDENT, "Expected class name");
    consume(p, TOKEN_LBRACE, "Expected '{' after class name");

    AstNode **fields = NULL;
    int field_count = 0;
    int field_cap = 0;

    AstNode **methods = NULL;
    int method_count = 0;
    int method_cap = 0;

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        if (check(p, TOKEN_FN)) {
            // parse method declaration
            Token m_tok = consume(p, TOKEN_FN, "Expected 'fn'");
            Token m_name = consume(p, TOKEN_IDENT, "Expected method name");
            consume(p, TOKEN_LPAREN, "Expected '(' after method name");

            bool has_self = false;
            char **param_names = NULL;
            Type *param_types = NULL;
            char **param_class_names = NULL;
            bool *param_is_borrowed = NULL;
            int *param_lines = NULL;
            int *param_cols = NULL;
            int param_count = 0;
            int param_cap = 0;

            if (!check(p, TOKEN_RPAREN)) {
                if (check(p, TOKEN_SELF) || (check(p, TOKEN_IDENT) && strcmp(peek(p).lexeme, "self") == 0)) {
                    Token self_tok = advance(p); // consume "self"
                    has_self = true;

                    param_cap = 4;
                    param_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                    param_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                    param_class_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                    param_is_borrowed = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                    bool *param_is_array = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                    param_lines = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                    param_cols = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));

                    param_names[0] = arena_strdup(p->arena, "self");
                    param_types[0] = TY_CLASS;
                    param_class_names[0] = arena_strdup(p->arena, name_tok.lexeme);
                    param_is_borrowed[0] = true; // self is borrowed by default
                    param_is_array[0] = false;
                    param_lines[0] = self_tok.line;
                    param_cols[0] = self_tok.col;
                    param_count = 1;

                    if (match(p, TOKEN_COMMA)) {
                        do {
                            Token p_name = consume(p, TOKEN_IDENT, "Expected parameter name");
                            consume(p, TOKEN_COLON, "Expected ':' after parameter name");
                            char *p_cls = NULL;
                            bool p_borrowed = false;
                            bool p_is_arr = false;
                            Type p_type = parse_type_with_class(p, &p_cls, &p_borrowed, &p_is_arr, NULL, NULL);

                            if (param_count >= param_cap) {
                                param_cap *= 2;
                                char **n_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                                Type *n_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                                char **n_cls = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                                bool *n_bor = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                                bool *n_arr = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                                int *n_lines = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                                int *n_cols = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                                memcpy(n_names, param_names, param_count * sizeof(char *));
                                memcpy(n_types, param_types, param_count * sizeof(Type));
                                memcpy(n_cls, param_class_names, param_count * sizeof(char *));
                                memcpy(n_bor, param_is_borrowed, param_count * sizeof(bool));
                                memcpy(n_arr, param_is_array, param_count * sizeof(bool));
                                memcpy(n_lines, param_lines, param_count * sizeof(int));
                                memcpy(n_cols, param_cols, param_count * sizeof(int));
                                param_names = n_names;
                                param_types = n_types;
                                param_class_names = n_cls;
                                param_is_borrowed = n_bor;
                                param_is_array = n_arr;
                                param_lines = n_lines;
                                param_cols = n_cols;
                            }
                            param_names[param_count] = arena_strdup(p->arena, p_name.lexeme);
                            param_types[param_count] = p_type;
                            param_class_names[param_count] = p_cls;
                            param_is_borrowed[param_count] = p_borrowed;
                            param_is_array[param_count] = p_is_arr;
                            param_lines[param_count] = p_name.line;
                            param_cols[param_count] = p_name.col;
                            param_count++;
                        } while (match(p, TOKEN_COMMA));
                    }
                } else {
                    bool *param_is_array = NULL;
                    do {
                        Token p_name = consume(p, TOKEN_IDENT, "Expected parameter name");
                        consume(p, TOKEN_COLON, "Expected ':' after parameter name");
                        char *p_cls = NULL;
                        bool p_borrowed = false;
                        bool p_is_arr = false;
                        Type p_type = parse_type_with_class(p, &p_cls, &p_borrowed, &p_is_arr, NULL, NULL);

                        if (param_count >= param_cap) {
                            param_cap = param_cap == 0 ? 4 : param_cap * 2;
                            char **n_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                            Type *n_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                            char **n_cls = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                            bool *n_bor = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                            bool *n_arr = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                            int *n_lines = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                            int *n_cols = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                            if (param_names) {
                                memcpy(n_names, param_names, param_count * sizeof(char *));
                                memcpy(n_types, param_types, param_count * sizeof(Type));
                                memcpy(n_cls, param_class_names, param_count * sizeof(char *));
                                memcpy(n_bor, param_is_borrowed, param_count * sizeof(bool));
                                if (param_is_array) memcpy(n_arr, param_is_array, param_count * sizeof(bool));
                                memcpy(n_lines, param_lines, param_count * sizeof(int));
                                memcpy(n_cols, param_cols, param_count * sizeof(int));
                            }
                            param_names = n_names;
                            param_types = n_types;
                            param_class_names = n_cls;
                            param_is_borrowed = n_bor;
                            param_is_array = n_arr;
                            param_lines = n_lines;
                            param_cols = n_cols;
                        }
                        param_names[param_count] = arena_strdup(p->arena, p_name.lexeme);
                        param_types[param_count] = p_type;
                        param_class_names[param_count] = p_cls;
                        param_is_borrowed[param_count] = p_borrowed;
                        param_is_array[param_count] = p_is_arr;
                        param_lines[param_count] = p_name.line;
                        param_cols[param_count] = p_name.col;
                        param_count++;
                    } while (match(p, TOKEN_COMMA));
                }
            }

            consume(p, TOKEN_RPAREN, "Expected ')' after method parameters");
            consume(p, TOKEN_ARROW, "Expected '->' after method signature");
            char *ret_cls = NULL;
            bool ret_is_arr = false;
            bool ret_is_map = false;
            Type ret_key_t = TY_INT;
            Type ret_type = parse_type_with_class(p, &ret_cls, NULL, &ret_is_arr, &ret_is_map, &ret_key_t);
            AstNode *body = parse_block(p);

            AstNode *m_node = arena_alloc_node(p->arena, NODE_METHOD, m_tok.line, m_tok.col);
            m_node->as.method.name = arena_strdup(p->arena, m_name.lexeme);
            m_node->as.method.has_self = has_self;
            m_node->as.method.param_names = param_names;
            m_node->as.method.param_types = param_types;
            m_node->as.method.param_class_names = param_class_names;
            m_node->as.method.param_is_borrowed = param_is_borrowed;
            m_node->as.method.param_lines = param_lines;
            m_node->as.method.param_cols = param_cols;
            m_node->as.method.param_count = param_count;
            m_node->as.method.return_type = ret_type;
            m_node->as.method.return_is_array = ret_is_arr;
            m_node->as.method.return_is_map = ret_is_map;
            m_node->as.method.return_key_type = ret_key_t;
            m_node->as.method.return_class_name = ret_cls;
            m_node->as.method.body = body;

            if (method_count >= method_cap) {
                method_cap = method_cap == 0 ? 4 : method_cap * 2;
                AstNode **new_m = (AstNode **)arena_alloc_array(p->arena, method_cap, sizeof(AstNode *));
                if (methods) memcpy(new_m, methods, method_count * sizeof(AstNode *));
                methods = new_m;
            }
            methods[method_count++] = m_node;
        } else {
            // parse field declaration: IDENT : type ;
            Token f_name = consume(p, TOKEN_IDENT, "Expected field or method declaration in class");
            consume(p, TOKEN_COLON, "Expected ':' after field name");
            char *f_cls = NULL;
            bool f_is_arr = false;
            bool f_is_map = false;
            Type f_key_type = TY_INT;
            Type f_type = parse_type_with_class(p, &f_cls, NULL, &f_is_arr, &f_is_map, &f_key_type);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after field declaration");

            AstNode *f_node = arena_alloc_node(p->arena, NODE_FIELD, f_name.line, f_name.col);
            f_node->as.field.name = arena_strdup(p->arena, f_name.lexeme);
            f_node->as.field.type = f_type;
            f_node->as.field.is_array = f_is_arr;
            f_node->as.field.is_map = f_is_map;
            f_node->as.field.key_type = f_key_type;
            f_node->as.field.class_name = f_cls;

            if (field_count >= field_cap) {
                field_cap = field_cap == 0 ? 4 : field_cap * 2;
                AstNode **new_f = (AstNode **)arena_alloc_array(p->arena, field_cap, sizeof(AstNode *));
                if (fields) memcpy(new_f, fields, field_count * sizeof(AstNode *));
                fields = new_f;
            }
            fields[field_count++] = f_node;
        }
    }

    consume(p, TOKEN_RBRACE, "Expected '}' at end of class declaration");

    AstNode *cls_node = arena_alloc_node(p->arena, NODE_CLASS, tok.line, tok.col);
    cls_node->as.class_decl.name = arena_strdup(p->arena, name_tok.lexeme);
    cls_node->as.class_decl.fields = fields;
    cls_node->as.class_decl.field_count = field_count;
    cls_node->as.class_decl.methods = methods;
    cls_node->as.class_decl.method_count = method_count;
    return cls_node;
}

static AstNode *parse_struct(Parser *p) {
    Token tok = consume(p, TOKEN_STRUCT, "Expected 'struct'");
    Token name_tok = consume(p, TOKEN_IDENT, "Expected struct name");
    consume(p, TOKEN_LBRACE, "Expected '{' after struct name");

    AstNode **fields = NULL;
    int field_count = 0;
    int field_cap = 0;

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        if (check(p, TOKEN_FN)) {
            fatal_parser_error(peek(p).line, peek(p).col, peek(p).lexeme, "structs cannot have methods — did you mean to declare this as a 'class' instead?");
        }

        Token f_name = consume(p, TOKEN_IDENT, "Expected field declaration in struct");
        consume(p, TOKEN_COLON, "Expected ':' after field name");

        Type f_type;
        if (match(p, TOKEN_TYPE_INT)) {
            f_type = TY_INT;
        } else if (match(p, TOKEN_TYPE_FLOAT)) {
            f_type = TY_FLOAT;
        } else if (match(p, TOKEN_TYPE_CHAR)) {
            f_type = TY_CHAR;
        } else if (match(p, TOKEN_TYPE_BOOL)) {
            f_type = TY_BOOL;
        } else {
            Token err_tok = peek(p);
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "struct fields must be a primitive type (int, float, char, bool) — '%s: %s' is not allowed in v7", f_name.lexeme, err_tok.lexeme);
            ErrorLocation loc = {get_error_filename(), err_tok.line, err_tok.col};
            print_formatted_error(
                short_msg,
                loc,
                "not allowed in v7",
                "structs are value types with no ownership tracking; a string or object field would need copy semantics not yet supported — consider using a class instead if you need this field",
                NULL,
                NULL,
                NULL
            );
            exit(1);
        }

        consume(p, TOKEN_SEMICOLON, "Expected ';' after struct field declaration");

        AstNode *f_node = arena_alloc_node(p->arena, NODE_STRUCT_FIELD, f_name.line, f_name.col);
        f_node->as.struct_field_decl.name = arena_strdup(p->arena, f_name.lexeme);
        f_node->as.struct_field_decl.field_type = f_type;

        if (field_count >= field_cap) {
            field_cap = field_cap == 0 ? 4 : field_cap * 2;
            AstNode **new_f = (AstNode **)arena_alloc_array(p->arena, field_cap, sizeof(AstNode *));
            if (fields) memcpy(new_f, fields, field_count * sizeof(AstNode *));
            fields = new_f;
        }
        fields[field_count++] = f_node;
    }

    consume(p, TOKEN_RBRACE, "Expected '}' at end of struct declaration");

    AstNode *s_node = arena_alloc_node(p->arena, NODE_STRUCT, tok.line, tok.col);
    s_node->as.struct_decl.name = arena_strdup(p->arena, name_tok.lexeme);
    s_node->as.struct_decl.fields = fields;
    s_node->as.struct_decl.field_count = field_count;
    return s_node;
}

static AstNode *parse_enum(Parser *p) {
    Token tok = consume(p, TOKEN_ENUM, "Expected 'enum'");
    Token name_tok = consume(p, TOKEN_IDENT, "Expected enum name");
    consume(p, TOKEN_LBRACE, "Expected '{' after enum name");

    AstNode **variants = NULL;
    int variant_count = 0;
    int variant_cap = 0;

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        Token v_name = consume(p, TOKEN_IDENT, "Expected variant name in enum");
        bool is_unit = true;
        AstNode **fields = NULL;
        int field_count = 0;
        int field_cap = 0;

        if (match(p, TOKEN_LBRACE)) {
            is_unit = false;
            while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
                Token f_name = consume(p, TOKEN_IDENT, "Expected field name in variant");
                consume(p, TOKEN_COLON, "Expected ':' after field name");

                char *f_cls = NULL;
                bool f_bor = false;
                bool f_is_arr = false;
                bool f_is_map = false;
                Type f_key_t = TY_INT;
                Token type_tok = peek(p);
                Type f_type = parse_type_with_class(p, &f_cls, &f_bor, &f_is_arr, &f_is_map, &f_key_t);

                if (f_is_arr || f_is_map) {
                    char short_msg[256];
                    snprintf(short_msg, sizeof(short_msg), "enum variant fields cannot be array or map types in v11");
                    ErrorLocation loc = {get_error_filename(), type_tok.line, type_tok.col};
                    print_formatted_error(
                        short_msg,
                        loc,
                        "array/map field not supported in enum variant",
                        "enum variants support primitive types, strings, structs, classes, and recursive enums in v11",
                        NULL,
                        NULL,
                        NULL
                    );
                    exit(1);
                }

                AstNode *f_node = arena_alloc_node(p->arena, NODE_FIELD, f_name.line, f_name.col);
                f_node->as.field.name = arena_strdup(p->arena, f_name.lexeme);
                f_node->as.field.type = f_type;
                f_node->as.field.is_array = false;
                f_node->as.field.is_map = false;
                f_node->as.field.class_name = f_cls ? arena_strdup(p->arena, f_cls) : NULL;

                if (field_count >= field_cap) {
                    field_cap = field_cap == 0 ? 4 : field_cap * 2;
                    AstNode **new_f = (AstNode **)arena_alloc_array(p->arena, field_cap, sizeof(AstNode *));
                    if (fields) memcpy(new_f, fields, field_count * sizeof(AstNode *));
                    fields = new_f;
                }
                fields[field_count++] = f_node;

                if (!check(p, TOKEN_RBRACE)) {
                    consume(p, TOKEN_COMMA, "Expected ',' between variant fields");
                }
            }
            consume(p, TOKEN_RBRACE, "Expected '}' after variant fields");
        }

        AstNode *v_node = arena_alloc_node(p->arena, NODE_VARIANT, v_name.line, v_name.col);
        v_node->as.variant_decl.name = arena_strdup(p->arena, v_name.lexeme);
        v_node->as.variant_decl.enum_name = arena_strdup(p->arena, name_tok.lexeme);
        v_node->as.variant_decl.fields = fields;
        v_node->as.variant_decl.field_count = field_count;
        v_node->as.variant_decl.is_unit = is_unit;

        if (variant_count >= variant_cap) {
            variant_cap = variant_cap == 0 ? 4 : variant_cap * 2;
            AstNode **new_v = (AstNode **)arena_alloc_array(p->arena, variant_cap, sizeof(AstNode *));
            if (variants) memcpy(new_v, variants, variant_count * sizeof(AstNode *));
            variants = new_v;
        }
        variants[variant_count++] = v_node;

        if (!check(p, TOKEN_RBRACE)) {
            consume(p, TOKEN_COMMA, "Expected ',' between enum variants");
        }
    }

    consume(p, TOKEN_RBRACE, "Expected '}' at end of enum declaration");

    AstNode *e_node = arena_alloc_node(p->arena, NODE_ENUM, tok.line, tok.col);
    e_node->as.enum_decl.name = arena_strdup(p->arena, name_tok.lexeme);
    e_node->as.enum_decl.variants = variants;
    e_node->as.enum_decl.variant_count = variant_count;
    return e_node;
}

static AstNode *parse_function(Parser *p) {
    Token tok = consume(p, TOKEN_FN, "Expected 'fn'");
    Token name_tok;
    bool is_operator = false;
    char *op_symbol = NULL;
    char *fn_name = NULL;

    if (match(p, TOKEN_OPERATOR)) {
        is_operator = true;
        Token op_tok;
        if (match(p, TOKEN_PLUS)) {
            op_tok = previous(p);
            op_symbol = "+";
        } else if (match(p, TOKEN_MINUS)) {
            op_tok = previous(p);
            op_symbol = "-";
        } else if (match(p, TOKEN_STAR)) {
            op_tok = previous(p);
            op_symbol = "*";
        } else if (match(p, TOKEN_SLASH)) {
            op_tok = previous(p);
            op_symbol = "/";
        } else if (match(p, TOKEN_EQ)) {
            op_tok = previous(p);
            op_symbol = "==";
        } else if (match(p, TOKEN_NE)) {
            op_tok = previous(p);
            op_symbol = "!=";
        } else {
            fatal_parser_error(peek(p).line, peek(p).col, peek(p).lexeme, "Expected operator symbol (+, -, *, /, ==, !=) after 'operator'");
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "operator%s", op_symbol);
        fn_name = arena_strdup(p->arena, buf);
        name_tok = op_tok;
    } else {
        name_tok = consume(p, TOKEN_IDENT, "Expected function name");
        fn_name = arena_strdup(p->arena, name_tok.lexeme);
    }
    consume(p, TOKEN_LPAREN, "Expected '(' after function name");

    char **param_names = NULL;
    Type *param_types = NULL;
    bool *param_is_array = NULL;
    char **param_class_names = NULL;
    bool *param_is_borrowed = NULL;
    bool *param_is_impl_trait = NULL;
    char **param_impl_trait_names = NULL;
    int *param_lines = NULL;
    int *param_cols = NULL;
    int param_count = 0;
    int param_cap = 0;

    if (!check(p, TOKEN_RPAREN)) {
        do {
            Token p_name = consume(p, TOKEN_IDENT, "Expected parameter name");
            consume(p, TOKEN_COLON, "Expected ':' after parameter name");
            char *p_cls = NULL;
            bool p_borrowed = false;
            bool p_is_arr = false;
            bool p_is_map = false;
            Type p_key_t = TY_INT;
            bool p_is_impl = false;
            Type p_type = parse_type_full(p, &p_cls, &p_borrowed, &p_is_arr, &p_is_map, &p_key_t, &p_is_impl, false, true);

            if (param_count >= param_cap) {
                param_cap = param_cap == 0 ? 4 : param_cap * 2;
                char **new_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                Type *new_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                bool *new_arr = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                char **new_cls = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                bool *new_bor = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                bool *new_impl = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                char **new_impl_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                int *new_lines = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                int *new_cols = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                if (param_names) {
                    memcpy(new_names, param_names, param_count * sizeof(char *));
                    memcpy(new_types, param_types, param_count * sizeof(Type));
                    if (param_is_array) memcpy(new_arr, param_is_array, param_count * sizeof(bool));
                    memcpy(new_cls, param_class_names, param_count * sizeof(char *));
                    memcpy(new_bor, param_is_borrowed, param_count * sizeof(bool));
                    if (param_is_impl_trait) memcpy(new_impl, param_is_impl_trait, param_count * sizeof(bool));
                    if (param_impl_trait_names) memcpy(new_impl_names, param_impl_trait_names, param_count * sizeof(char *));
                    memcpy(new_lines, param_lines, param_count * sizeof(int));
                    memcpy(new_cols, param_cols, param_count * sizeof(int));
                }
                param_names = new_names;
                param_types = new_types;
                param_is_array = new_arr;
                param_class_names = new_cls;
                param_is_borrowed = new_bor;
                param_is_impl_trait = new_impl;
                param_impl_trait_names = new_impl_names;
                param_lines = new_lines;
                param_cols = new_cols;
            }
            param_names[param_count] = arena_strdup(p->arena, p_name.lexeme);
            param_types[param_count] = p_type;
            param_is_array[param_count] = p_is_arr;
            param_class_names[param_count] = p_is_impl ? NULL : p_cls;
            param_is_borrowed[param_count] = p_borrowed;
            param_is_impl_trait[param_count] = p_is_impl;
            param_impl_trait_names[param_count] = p_is_impl ? p_cls : NULL;
            param_lines[param_count] = p_name.line;
            param_cols[param_count] = p_name.col;
            param_count++;
        } while (match(p, TOKEN_COMMA));
    }

    consume(p, TOKEN_RPAREN, "Expected ')' after parameters");
    consume(p, TOKEN_ARROW, "Expected '->' after function signature");
    char *ret_cls = NULL;
    bool ret_is_arr = false;
    bool ret_is_map = false;
    Type ret_key_t = TY_INT;
    Type return_type = parse_type_with_class(p, &ret_cls, NULL, &ret_is_arr, &ret_is_map, &ret_key_t);
    AstNode *body = parse_block(p);

    AstNode *node = arena_alloc_node(p->arena, NODE_FUNCTION, tok.line, tok.col);
    node->as.function.name = fn_name;
    node->as.function.is_operator = is_operator;
    node->as.function.operator_symbol = op_symbol ? arena_strdup(p->arena, op_symbol) : NULL;
    node->as.function.param_names = param_names;
    node->as.function.param_types = param_types;
    node->as.function.param_is_array = param_is_array;
    node->as.function.param_class_names = param_class_names;
    node->as.function.param_is_borrowed = param_is_borrowed;
    node->as.function.param_is_impl_trait = param_is_impl_trait;
    node->as.function.param_impl_trait_names = param_impl_trait_names;
    node->as.function.param_lines = param_lines;
    node->as.function.param_cols = param_cols;
    node->as.function.param_count = param_count;
    node->as.function.return_type = return_type;
    node->as.function.return_is_array = ret_is_arr;
    node->as.function.return_is_map = ret_is_map;
    node->as.function.return_key_type = ret_key_t;
    node->as.function.return_class_name = ret_cls;
    node->as.function.body = body;
    return node;
}

static AstNode *parse_interface(Parser *p) {
    Token tok = consume(p, TOKEN_INTERFACE, "Expected 'interface'");
    Token name_tok = consume(p, TOKEN_IDENT, "Expected interface name");
    consume(p, TOKEN_LBRACE, "Expected '{' after interface name");

    AstNode **methods = NULL;
    int method_count = 0;
    int method_cap = 0;

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        Token fn_tok = consume(p, TOKEN_FN, "Expected 'fn' in interface method signature");
        Token m_name = consume(p, TOKEN_IDENT, "Expected method name in interface");
        consume(p, TOKEN_LPAREN, "Expected '(' after method name");

        // First parameter must be 'self'
        Token self_tok = consume(p, TOKEN_SELF, "Expected 'self' as first parameter in interface method");

        int param_cap = 4;
        char **param_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
        Type *param_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
        bool *param_is_array = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
        char **param_class_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
        bool *param_is_borrowed = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
        int *param_lines = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
        int *param_cols = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));

        param_names[0] = arena_strdup(p->arena, "self");
        param_types[0] = TY_CLASS;
        param_class_names[0] = arena_strdup(p->arena, "Self");
        param_is_borrowed[0] = true;
        param_is_array[0] = false;
        param_lines[0] = self_tok.line;
        param_cols[0] = self_tok.col;
        int param_count = 1;

        if (match(p, TOKEN_COMMA)) {
            do {
                Token p_name = consume(p, TOKEN_IDENT, "Expected parameter name");
                consume(p, TOKEN_COLON, "Expected ':' after parameter name");
                char *p_cls = NULL;
                bool p_borrowed = false;
                bool p_is_arr = false;
                bool p_is_map = false;
                Type p_key_t = TY_INT;
                Type p_type = parse_type_full(p, &p_cls, &p_borrowed, &p_is_arr, &p_is_map, &p_key_t, NULL, true, false);

                if (param_count >= param_cap) {
                    param_cap = param_cap * 2;
                    char **new_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                    Type *new_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                    bool *new_arr = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                    char **new_cls = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                    bool *new_bor = (bool *)arena_alloc_array(p->arena, param_cap, sizeof(bool));
                    int *new_lines = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                    int *new_cols = (int *)arena_alloc_array(p->arena, param_cap, sizeof(int));
                    memcpy(new_names, param_names, param_count * sizeof(char *));
                    memcpy(new_types, param_types, param_count * sizeof(Type));
                    memcpy(new_arr, param_is_array, param_count * sizeof(bool));
                    memcpy(new_cls, param_class_names, param_count * sizeof(char *));
                    memcpy(new_bor, param_is_borrowed, param_count * sizeof(bool));
                    memcpy(new_lines, param_lines, param_count * sizeof(int));
                    memcpy(new_cols, param_cols, param_count * sizeof(int));
                    param_names = new_names;
                    param_types = new_types;
                    param_is_array = new_arr;
                    param_class_names = new_cls;
                    param_is_borrowed = new_bor;
                    param_lines = new_lines;
                    param_cols = new_cols;
                }
                param_names[param_count] = arena_strdup(p->arena, p_name.lexeme);
                param_types[param_count] = p_type;
                param_is_array[param_count] = p_is_arr;
                param_class_names[param_count] = p_cls;
                param_is_borrowed[param_count] = p_borrowed;
                param_lines[param_count] = p_name.line;
                param_cols[param_count] = p_name.col;
                param_count++;
            } while (match(p, TOKEN_COMMA));
        }

        consume(p, TOKEN_RPAREN, "Expected ')' after parameters");
        consume(p, TOKEN_ARROW, "Expected '->' after parameter list");

        char *ret_cls = NULL;
        bool ret_borrowed = false;
        bool ret_is_arr = false;
        bool ret_is_map = false;
        Type ret_key_t = TY_INT;
        Type ret_type = parse_type_full(p, &ret_cls, &ret_borrowed, &ret_is_arr, &ret_is_map, &ret_key_t, NULL, true, false);

        consume(p, TOKEN_SEMICOLON, "Expected ';' after interface method signature");

        AstNode *m_node = arena_alloc_node(p->arena, NODE_INTERFACE_METHOD, fn_tok.line, fn_tok.col);
        m_node->as.interface_method.name = arena_strdup(p->arena, m_name.lexeme);
        m_node->as.interface_method.param_names = param_names;
        m_node->as.interface_method.param_types = param_types;
        m_node->as.interface_method.param_is_array = param_is_array;
        m_node->as.interface_method.param_class_names = param_class_names;
        m_node->as.interface_method.param_is_borrowed = param_is_borrowed;
        m_node->as.interface_method.param_lines = param_lines;
        m_node->as.interface_method.param_cols = param_cols;
        m_node->as.interface_method.param_count = param_count;
        m_node->as.interface_method.return_type = ret_type;
        m_node->as.interface_method.return_is_array = ret_is_arr;
        m_node->as.interface_method.return_is_map = ret_is_map;
        m_node->as.interface_method.return_key_type = ret_key_t;
        m_node->as.interface_method.return_class_name = ret_cls;

        if (method_count >= method_cap) {
            method_cap = method_cap == 0 ? 4 : method_cap * 2;
            AstNode **new_m = (AstNode **)arena_alloc_array(p->arena, method_cap, sizeof(AstNode *));
            if (methods) memcpy(new_m, methods, method_count * sizeof(AstNode *));
            methods = new_m;
        }
        methods[method_count++] = m_node;
    }

    consume(p, TOKEN_RBRACE, "Expected '}' after interface body");

    AstNode *iface_node = arena_alloc_node(p->arena, NODE_INTERFACE, tok.line, tok.col);
    iface_node->as.interface_decl.name = arena_strdup(p->arena, name_tok.lexeme);
    iface_node->as.interface_decl.methods = methods;
    iface_node->as.interface_decl.method_count = method_count;
    return iface_node;
}

static AstNode *parse_impl(Parser *p) {
    Token tok = consume(p, TOKEN_IMPL, "Expected 'impl'");
    Token iface_tok = consume(p, TOKEN_IDENT, "Expected interface name after 'impl'");
    consume(p, TOKEN_FOR, "Expected 'for' after interface name in impl declaration");
    Token cls_tok = consume(p, TOKEN_IDENT, "Expected class name after 'for' in impl declaration");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after impl declaration");

    AstNode *impl_node = arena_alloc_node(p->arena, NODE_IMPL, tok.line, tok.col);
    impl_node->as.impl_decl.interface_name = arena_strdup(p->arena, iface_tok.lexeme);
    impl_node->as.impl_decl.class_name = arena_strdup(p->arena, cls_tok.lexeme);
    return impl_node;
}

static AstNode *parse_import_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_IMPORT, "Expected 'import'");
    Token path_tok = consume(p, TOKEN_STRING_LIT, "Expected string literal path after 'import'");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after import statement");

    AstNode *node = arena_alloc_node(p->arena, NODE_IMPORT, tok.line, tok.col);
    node->as.import_stmt.path = arena_strdup(p->arena, path_tok.lexeme);
    return node;
}

AstNode *parse_program(Parser *p) {
    AstNode **imports = NULL;
    int import_count = 0;
    int import_cap = 0;

    AstNode **classes = NULL;
    int class_count = 0;
    int class_cap = 0;

    AstNode **structs = NULL;
    int struct_count = 0;
    int struct_cap = 0;

    AstNode **enums = NULL;
    int enum_count = 0;
    int enum_cap = 0;

    AstNode **interfaces = NULL;
    int interface_count = 0;
    int interface_cap = 0;

    AstNode **impls = NULL;
    int impl_count = 0;
    int impl_cap = 0;

    AstNode **functions = NULL;
    int fn_count = 0;
    int fn_cap = 0;

    bool seen_decl = false;

    while (!is_at_end(p)) {
        if (check(p, TOKEN_IMPORT)) {
            if (seen_decl) {
                fatal_parser_error(peek(p).line, peek(p).col, peek(p).lexeme, "import statements must appear before any function or class declaration");
            }
            AstNode *imp = parse_import_stmt(p);
            if (import_count >= import_cap) {
                import_cap = import_cap == 0 ? 4 : import_cap * 2;
                AstNode **new_imp = (AstNode **)arena_alloc_array(p->arena, import_cap, sizeof(AstNode *));
                if (imports) memcpy(new_imp, imports, import_count * sizeof(AstNode *));
                imports = new_imp;
            }
            imports[import_count++] = imp;
        } else if (check(p, TOKEN_INTERFACE)) {
            seen_decl = true;
            AstNode *iface = parse_interface(p);
            if (interface_count >= interface_cap) {
                interface_cap = interface_cap == 0 ? 4 : interface_cap * 2;
                AstNode **new_if = (AstNode **)arena_alloc_array(p->arena, interface_cap, sizeof(AstNode *));
                if (interfaces) memcpy(new_if, interfaces, interface_count * sizeof(AstNode *));
                interfaces = new_if;
            }
            interfaces[interface_count++] = iface;
        } else if (check(p, TOKEN_IMPL)) {
            seen_decl = true;
            AstNode *imp = parse_impl(p);
            if (impl_count >= impl_cap) {
                impl_cap = impl_cap == 0 ? 4 : impl_cap * 2;
                AstNode **new_im = (AstNode **)arena_alloc_array(p->arena, impl_cap, sizeof(AstNode *));
                if (impls) memcpy(new_im, impls, impl_count * sizeof(AstNode *));
                impls = new_im;
            }
            impls[impl_count++] = imp;
        } else if (check(p, TOKEN_CLASS)) {
            seen_decl = true;
            AstNode *cls = parse_class(p);
            if (class_count >= class_cap) {
                class_cap = class_cap == 0 ? 4 : class_cap * 2;
                AstNode **new_cls = (AstNode **)arena_alloc_array(p->arena, class_cap, sizeof(AstNode *));
                if (classes) memcpy(new_cls, classes, class_count * sizeof(AstNode *));
                classes = new_cls;
            }
            classes[class_count++] = cls;
        } else if (check(p, TOKEN_STRUCT)) {
            seen_decl = true;
            AstNode *st = parse_struct(p);
            if (struct_count >= struct_cap) {
                struct_cap = struct_cap == 0 ? 4 : struct_cap * 2;
                AstNode **new_st = (AstNode **)arena_alloc_array(p->arena, struct_cap, sizeof(AstNode *));
                if (structs) memcpy(new_st, structs, struct_count * sizeof(AstNode *));
                structs = new_st;
            }
            structs[struct_count++] = st;
        } else if (check(p, TOKEN_ENUM)) {
            seen_decl = true;
            AstNode *en = parse_enum(p);
            if (enum_count >= enum_cap) {
                enum_cap = enum_cap == 0 ? 4 : enum_cap * 2;
                AstNode **new_en = (AstNode **)arena_alloc_array(p->arena, enum_cap, sizeof(AstNode *));
                if (enums) memcpy(new_en, enums, enum_count * sizeof(AstNode *));
                enums = new_en;
            }
            enums[enum_count++] = en;
        } else if (check(p, TOKEN_FN)) {
            seen_decl = true;
            AstNode *fn = parse_function(p);
            if (fn_count >= fn_cap) {
                fn_cap = fn_cap == 0 ? 4 : fn_cap * 2;
                AstNode **new_fns = (AstNode **)arena_alloc_array(p->arena, fn_cap, sizeof(AstNode *));
                if (functions) memcpy(new_fns, functions, fn_count * sizeof(AstNode *));
                functions = new_fns;
            }
            functions[fn_count++] = fn;
        } else {
            fatal_parser_error(peek(p).line, peek(p).col, peek(p).lexeme, "Expected 'import', 'interface', 'impl', 'class', 'struct', 'enum', or 'fn'");
        }
    }

    AstNode *prog = arena_alloc_node(p->arena, NODE_PROGRAM, 1, 1);
    prog->as.program.imports = imports;
    prog->as.program.import_count = import_count;
    prog->as.program.interfaces = interfaces;
    prog->as.program.interface_count = interface_count;
    prog->as.program.impls = impls;
    prog->as.program.impl_count = impl_count;
    prog->as.program.classes = classes;
    prog->as.program.class_count = class_count;
    prog->as.program.structs = structs;
    prog->as.program.struct_count = struct_count;
    prog->as.program.enums = enums;
    prog->as.program.enum_count = enum_count;
    prog->as.program.functions = functions;
    prog->as.program.count = fn_count;

    for (int i = 0; i < prog->as.program.count; i++) {
        AstNode *fn = prog->as.program.functions[i];
        if (!fn->as.function.is_operator) continue;

        const char *op = fn->as.function.operator_symbol;
        int arity = fn->as.function.param_count;

        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "+") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
            if (arity != 2) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "'operator%s' requires exactly 2 parameters", op);
                ErrorLocation loc = {get_error_filename(), fn->line, fn->col};
                print_formatted_error(short_msg, loc, "invalid operator signature", NULL, NULL, NULL, NULL);
                exit(1);
            }
        } else if (strcmp(op, "-") == 0) {
            if (arity < 1 || arity > 2) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "'operator-' requires 1 or 2 parameters");
                ErrorLocation loc = {get_error_filename(), fn->line, fn->col};
                print_formatted_error(short_msg, loc, "invalid operator signature", NULL, NULL, NULL, NULL);
                exit(1);
            }
        }

        if (arity > 0) {
            Type t0 = fn->as.function.param_types[0];
            const char *c0 = fn->as.function.param_class_names[0];

            if (t0 != TY_CLASS || !c0) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "operator overloading is only supported for struct types in this version");
                ErrorLocation loc = {get_error_filename(), fn->as.function.param_lines ? fn->as.function.param_lines[0] : fn->line, fn->as.function.param_cols ? fn->as.function.param_cols[0] : fn->col};
                print_formatted_error(short_msg, loc, "operator overloading requires struct type", NULL, NULL, NULL, NULL);
                exit(1);
            }

            bool is_class = false;
            for (int c = 0; c < prog->as.program.class_count; c++) {
                if (strcmp(prog->as.program.classes[c]->as.class_decl.name, c0) == 0) {
                    is_class = true;
                    break;
                }
            }
            if (is_class) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "operator overloading is only supported for struct types in this version — '%s' is a class", c0);
                ErrorLocation loc = {get_error_filename(), fn->as.function.param_lines ? fn->as.function.param_lines[0] : fn->line, fn->as.function.param_cols ? fn->as.function.param_cols[0] : fn->col};
                print_formatted_error(short_msg, loc, "classes not supported for operator overloading in v16", "structs are copied values with no ownership question; class operator overloading is a separate feature for later", NULL, NULL, NULL);
                exit(1);
            }

            bool is_struct = false;
            for (int s = 0; s < prog->as.program.struct_count; s++) {
                if (strcmp(prog->as.program.structs[s]->as.struct_decl.name, c0) == 0) {
                    is_struct = true;
                    break;
                }
            }
            if (!is_struct && prog->as.program.import_count == 0) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "operator overloading is only supported for struct types in this version — '%s' is not a struct", c0);
                ErrorLocation loc = {get_error_filename(), fn->as.function.param_lines ? fn->as.function.param_lines[0] : fn->line, fn->as.function.param_cols ? fn->as.function.param_cols[0] : fn->col};
                print_formatted_error(short_msg, loc, "unknown struct type", NULL, NULL, NULL, NULL);
                exit(1);
            }

            if (arity == 2) {
                Type t1 = fn->as.function.param_types[1];
                const char *c1 = fn->as.function.param_class_names[1];
                if (t1 != TY_CLASS || !c1 || strcmp(c0, c1) != 0) {
                    char short_msg[256];
                    snprintf(short_msg, sizeof(short_msg), "operator function parameters must both be the same struct type");
                    ErrorLocation loc = {get_error_filename(), fn->as.function.param_lines ? fn->as.function.param_lines[1] : fn->line, fn->as.function.param_cols ? fn->as.function.param_cols[1] : fn->col};
                    print_formatted_error(short_msg, loc, "parameter type mismatch in operator function", NULL, NULL, NULL, NULL);
                    exit(1);
                }
            }
        }

        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
            if (fn->as.function.return_type != TY_BOOL) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "'operator%s' must return bool", op);
                ErrorLocation loc = {get_error_filename(), fn->line, fn->col};
                print_formatted_error(short_msg, loc, "comparison operators must return bool", NULL, NULL, NULL, NULL);
                exit(1);
            }
        }

        for (int j = 0; j < i; j++) {
            AstNode *prev = prog->as.program.functions[j];
            if (!prev->as.function.is_operator) continue;
            if (strcmp(prev->as.function.operator_symbol, op) == 0 &&
                prev->as.function.param_count == arity) {
                const char *prev_c = (prev->as.function.param_count > 0) ? prev->as.function.param_class_names[0] : "";
                const char *cur_c = (arity > 0) ? fn->as.function.param_class_names[0] : "";
                if (prev_c && cur_c && strcmp(prev_c, cur_c) == 0) {
                    char short_msg[256];
                    snprintf(short_msg, sizeof(short_msg), "duplicate definition of 'operator%s' for struct '%s'", op, cur_c);
                    ErrorLocation primary = {get_error_filename(), fn->line, fn->col};
                    ErrorLocation note_loc = {get_error_filename(), prev->line, prev->col};
                    print_formatted_error(short_msg, primary, "duplicate operator definition", "first defined here:", &note_loc, "first defined here", NULL);
                    exit(1);
                }
            }
        }
    }

    return prog;
}
