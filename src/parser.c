#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void error_at(Token token, const char *message) {
    fprintf(stderr, "Parser Error at line %d, col %d", token.line, token.col);
    if (token.type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token.lexeme) {
        fprintf(stderr, " at '%s'", token.lexeme);
    }
    fprintf(stderr, ": %s\n", message);
    exit(1);
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
static Type parse_type_with_class(Parser *p, char **out_class_name) {
    if (out_class_name) *out_class_name = NULL;
    if (match(p, TOKEN_TYPE_INT)) return TY_INT;
    if (match(p, TOKEN_TYPE_FLOAT)) return TY_FLOAT;
    if (match(p, TOKEN_TYPE_CHAR)) return TY_CHAR;
    if (match(p, TOKEN_TYPE_BOOL)) return TY_BOOL;
    if (match(p, TOKEN_TYPE_STRING)) return TY_STRING;
    if (match(p, TOKEN_TYPE_VOID)) return TY_VOID;
    if (match(p, TOKEN_IDENT)) {
        Token tok = previous(p);
        if (out_class_name) {
            *out_class_name = arena_strdup(p->arena, tok.lexeme);
        }
        return TY_CLASS;
    }
    error_at(peek(p), "Expected type specification (int, float, char, bool, string, void, or class identifier)");
    return TY_VOID;
}

static AstNode *parse_primary(Parser *p) {
    Token tok = peek(p);

    if (match(p, TOKEN_INT_LIT)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line);
        node->as.literal.lit_type = TY_INT;
        node->as.literal.val.i = atol(tok.lexeme);
        return node;
    }

    if (match(p, TOKEN_FLOAT_LIT)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line);
        node->as.literal.lit_type = TY_FLOAT;
        node->as.literal.val.f = atof(tok.lexeme);
        return node;
    }

    if (match(p, TOKEN_STRING_LIT)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line);
        node->as.literal.lit_type = TY_STRING;
        node->as.literal.val.s = arena_strdup(p->arena, tok.lexeme);
        return node;
    }

    if (match(p, TOKEN_TRUE)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line);
        node->as.literal.lit_type = TY_BOOL;
        node->as.literal.val.b = true;
        return node;
    }

    if (match(p, TOKEN_FALSE)) {
        AstNode *node = arena_alloc_node(p->arena, NODE_LITERAL, tok.line);
        node->as.literal.lit_type = TY_BOOL;
        node->as.literal.val.b = false;
        return node;
    }

    if (match(p, TOKEN_ALLOC)) {
        consume(p, TOKEN_LPAREN, "Expected '(' after 'alloc'");
        char *alloc_cls = NULL;
        Type elem_type = parse_type_with_class(p, &alloc_cls);
        consume(p, TOKEN_COMMA, "Expected ',' after alloc type");
        AstNode *count_expr = parse_expr(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after alloc count expression");

        AstNode *node = arena_alloc_node(p->arena, NODE_ALLOC, tok.line);
        node->as.alloc.elem_type = elem_type;
        node->as.alloc.class_name = alloc_cls;
        node->as.alloc.count_expr = count_expr;
        return node;
    }

    if (match(p, TOKEN_IDENT) || match(p, TOKEN_SELF)) {
        Token id_tok = previous(p);
        if (match(p, TOKEN_LBRACE)) {
            // New expression: ClassName { field1: expr1, field2: expr2 }
            char **field_names = NULL;
            AstNode **field_values = NULL;
            int field_count = 0;
            int field_cap = 0;

            if (!check(p, TOKEN_RBRACE)) {
                do {
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

            AstNode *node = arena_alloc_node(p->arena, NODE_NEW, id_tok.line);
            node->as.new_expr.class_name = arena_strdup(p->arena, id_tok.lexeme);
            node->as.new_expr.field_names = field_names;
            node->as.new_expr.field_values = field_values;
            node->as.new_expr.field_count = field_count;
            return node;
        }

        AstNode *node = arena_alloc_node(p->arena, NODE_IDENT, id_tok.line);
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

                AstNode *mc_node = arena_alloc_node(p->arena, NODE_METHOD_CALL, m_tok.line);
                mc_node->as.method_call.object = expr;
                mc_node->as.method_call.method_name = arena_strdup(p->arena, m_tok.lexeme);
                mc_node->as.method_call.args = args;
                mc_node->as.method_call.arg_count = arg_count;
                expr = mc_node;
            } else {
                // Member access: expr.member
                AstNode *m_node = arena_alloc_node(p->arena, NODE_MEMBER, m_tok.line);
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

            AstNode *call_node = arena_alloc_node(p->arena, NODE_CALL, expr->line);
            call_node->as.call.callee = callee;
            call_node->as.call.args = args;
            call_node->as.call.arg_count = arg_count;
            expr = call_node;
        } else if (match(p, TOKEN_LBRACKET)) {
            // Array indexing
            if (expr->type != NODE_IDENT) {
                error_at(previous(p), "Expected identifier before '[' in array index");
            }
            char *arr_name = arena_strdup(p->arena, expr->as.ident.name);
            AstNode *index_expr = parse_expr(p);
            consume(p, TOKEN_RBRACKET, "Expected ']' after array index expression");

            AstNode *idx_node = arena_alloc_node(p->arena, NODE_INDEX, expr->line);
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
    if (match(p, TOKEN_NOT) || match(p, TOKEN_MINUS)) {
        Token op = previous(p);
        AstNode *operand = parse_unary(p);
        AstNode *node = arena_alloc_node(p->arena, NODE_UNARY, op.line);
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
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line);
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
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line);
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
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line);
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
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line);
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
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line);
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
        AstNode *node = arena_alloc_node(p->arena, NODE_BINARY, op.line);
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
    if (match(p, TOKEN_COLON)) {
        var_type = parse_type_with_class(p, &class_name);
    }

    consume(p, TOKEN_ASSIGN, "Expected '=' in variable declaration");
    AstNode *value = parse_expr(p);
    consume(p, TOKEN_SEMICOLON, "Expected ';' after let statement");

    AstNode *node = arena_alloc_node(p->arena, NODE_LET, tok.line);
    node->as.let.name = arena_strdup(p->arena, name_tok.lexeme);
    node->as.let.var_type = var_type;
    node->as.let.class_name = class_name;
    node->as.let.value = value;
    return node;
}

static AstNode *parse_assign_or_expr_stmt(Parser *p) {
    Token tok = peek(p);

    if (tok.type == TOKEN_IDENT) {
        // Check for member assignment: ident . ident =
        if (p->current + 3 < p->tokens.count &&
            p->tokens.tokens[p->current + 1].type == TOKEN_DOT &&
            p->tokens.tokens[p->current + 2].type == TOKEN_IDENT &&
            p->tokens.tokens[p->current + 3].type == TOKEN_ASSIGN) {

            Token name_tok = advance(p); // consume obj name
            advance(p); // consume '.'
            Token m_tok = advance(p); // consume member name
            advance(p); // consume '='

            AstNode *value = parse_expr(p);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after member assignment");

            AstNode *obj_node = arena_alloc_node(p->arena, NODE_IDENT, name_tok.line);
            obj_node->as.ident.name = arena_strdup(p->arena, name_tok.lexeme);

            AstNode *node = arena_alloc_node(p->arena, NODE_MEMBER_ASSIGN, name_tok.line);
            node->as.member_assign.object = obj_node;
            node->as.member_assign.member_name = arena_strdup(p->arena, m_tok.lexeme);
            node->as.member_assign.value = value;
            return node;
        }

        Token name_tok = advance(p);
        if (match(p, TOKEN_ASSIGN)) {
            // Simple variable assignment: ident = expr;
            AstNode *value = parse_expr(p);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after assignment");
            AstNode *node = arena_alloc_node(p->arena, NODE_ASSIGN, name_tok.line);
            node->as.assign.name = arena_strdup(p->arena, name_tok.lexeme);
            node->as.assign.value = value;
            return node;
        } else if (match(p, TOKEN_LBRACKET)) {
            // Array element assignment: ident[idx] = expr;
            AstNode *index_expr = parse_expr(p);
            consume(p, TOKEN_RBRACKET, "Expected ']' after array index");
            consume(p, TOKEN_ASSIGN, "Expected '=' after array element indexing");
            AstNode *value = parse_expr(p);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after array element assignment");

            AstNode *node = arena_alloc_node(p->arena, NODE_INDEX_ASSIGN, name_tok.line);
            node->as.index_assign.array_name = arena_strdup(p->arena, name_tok.lexeme);
            node->as.index_assign.index = index_expr;
            node->as.index_assign.value = value;
            return node;
        } else {
            // Backtrack token pointer and parse as normal expression statement
            p->current--;
        }
    }

    AstNode *expr = parse_expr(p);
    consume(p, TOKEN_SEMICOLON, "Expected ';' after expression statement");
    AstNode *node = arena_alloc_node(p->arena, NODE_EXPR_STMT, expr->line);
    node->as.expr_stmt.expr = expr;
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

    AstNode *node = arena_alloc_node(p->arena, NODE_IF, tok.line);
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

    AstNode *node = arena_alloc_node(p->arena, NODE_WHILE, tok.line);
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
            step = arena_alloc_node(p->arena, NODE_ASSIGN, step_tok.line);
            step->as.assign.name = arena_strdup(p->arena, name_tok.lexeme);
            step->as.assign.value = val;
        } else if (match(p, TOKEN_LBRACKET)) {
            AstNode *idx = parse_expr(p);
            consume(p, TOKEN_RBRACKET, "Expected ']'");
            consume(p, TOKEN_ASSIGN, "Expected '='");
            AstNode *val = parse_expr(p);
            step = arena_alloc_node(p->arena, NODE_INDEX_ASSIGN, step_tok.line);
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

    AstNode *node = arena_alloc_node(p->arena, NODE_FOR, tok.line);
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

    AstNode *node = arena_alloc_node(p->arena, NODE_RETURN, tok.line);
    node->as.return_stmt.value = val;
    return node;
}

static AstNode *parse_break_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_BREAK, "Expected 'break'");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after break");
    return arena_alloc_node(p->arena, NODE_BREAK, tok.line);
}

static AstNode *parse_continue_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_CONTINUE, "Expected 'continue'");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after continue");
    return arena_alloc_node(p->arena, NODE_CONTINUE, tok.line);
}

static AstNode *parse_print_stmt(Parser *p) {
    Token tok = consume(p, TOKEN_PRINT, "Expected 'print'");
    consume(p, TOKEN_LPAREN, "Expected '(' after 'print'");
    AstNode *val = parse_expr(p);
    consume(p, TOKEN_RPAREN, "Expected ')' after print argument");
    consume(p, TOKEN_SEMICOLON, "Expected ';' after print statement");

    AstNode *node = arena_alloc_node(p->arena, NODE_PRINT, tok.line);
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

    AstNode *node = arena_alloc_node(p->arena, NODE_BLOCK, tok.line);
    node->as.block.stmts = stmts;
    node->as.block.count = count;
    return node;
}

static AstNode *parse_statement(Parser *p) {
    if (check(p, TOKEN_LET)) return parse_let_stmt(p);
    if (check(p, TOKEN_IF)) return parse_if_stmt(p);
    if (check(p, TOKEN_WHILE)) return parse_while_stmt(p);
    if (check(p, TOKEN_FOR)) return parse_for_stmt(p);
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
            int param_count = 0;
            int param_cap = 0;

            if (!check(p, TOKEN_RPAREN)) {
                if (check(p, TOKEN_SELF) || (check(p, TOKEN_IDENT) && strcmp(peek(p).lexeme, "self") == 0)) {
                    advance(p); // consume "self"
                    has_self = true;

                    param_cap = 4;
                    param_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                    param_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                    param_class_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));

                    param_names[0] = arena_strdup(p->arena, "self");
                    param_types[0] = TY_CLASS;
                    param_class_names[0] = arena_strdup(p->arena, name_tok.lexeme);
                    param_count = 1;

                    if (match(p, TOKEN_COMMA)) {
                        do {
                            Token p_name = consume(p, TOKEN_IDENT, "Expected parameter name");
                            consume(p, TOKEN_COLON, "Expected ':' after parameter name");
                            char *p_cls = NULL;
                            Type p_type = parse_type_with_class(p, &p_cls);

                            if (param_count >= param_cap) {
                                param_cap *= 2;
                                char **n_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                                Type *n_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                                char **n_cls = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                                memcpy(n_names, param_names, param_count * sizeof(char *));
                                memcpy(n_types, param_types, param_count * sizeof(Type));
                                memcpy(n_cls, param_class_names, param_count * sizeof(char *));
                                param_names = n_names;
                                param_types = n_types;
                                param_class_names = n_cls;
                            }
                            param_names[param_count] = arena_strdup(p->arena, p_name.lexeme);
                            param_types[param_count] = p_type;
                            param_class_names[param_count] = p_cls;
                            param_count++;
                        } while (match(p, TOKEN_COMMA));
                    }
                } else {
                    do {
                        Token p_name = consume(p, TOKEN_IDENT, "Expected parameter name");
                        consume(p, TOKEN_COLON, "Expected ':' after parameter name");
                        char *p_cls = NULL;
                        Type p_type = parse_type_with_class(p, &p_cls);

                        if (param_count >= param_cap) {
                            param_cap = param_cap == 0 ? 4 : param_cap * 2;
                            char **n_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                            Type *n_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                            char **n_cls = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                            if (param_names) {
                                memcpy(n_names, param_names, param_count * sizeof(char *));
                                memcpy(n_types, param_types, param_count * sizeof(Type));
                                memcpy(n_cls, param_class_names, param_count * sizeof(char *));
                            }
                            param_names = n_names;
                            param_types = n_types;
                            param_class_names = n_cls;
                        }
                        param_names[param_count] = arena_strdup(p->arena, p_name.lexeme);
                        param_types[param_count] = p_type;
                        param_class_names[param_count] = p_cls;
                        param_count++;
                    } while (match(p, TOKEN_COMMA));
                }
            }

            consume(p, TOKEN_RPAREN, "Expected ')' after method parameters");
            consume(p, TOKEN_ARROW, "Expected '->' after method signature");
            char *ret_cls = NULL;
            Type ret_type = parse_type_with_class(p, &ret_cls);
            AstNode *body = parse_block(p);

            AstNode *m_node = arena_alloc_node(p->arena, NODE_METHOD, m_tok.line);
            m_node->as.method.name = arena_strdup(p->arena, m_name.lexeme);
            m_node->as.method.has_self = has_self;
            m_node->as.method.param_names = param_names;
            m_node->as.method.param_types = param_types;
            m_node->as.method.param_class_names = param_class_names;
            m_node->as.method.param_count = param_count;
            m_node->as.method.return_type = ret_type;
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
            Type f_type = parse_type_with_class(p, &f_cls);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after field declaration");

            AstNode *f_node = arena_alloc_node(p->arena, NODE_FIELD, f_name.line);
            f_node->as.field.name = arena_strdup(p->arena, f_name.lexeme);
            f_node->as.field.type = f_type;
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

    AstNode *cls_node = arena_alloc_node(p->arena, NODE_CLASS, tok.line);
    cls_node->as.class_decl.name = arena_strdup(p->arena, name_tok.lexeme);
    cls_node->as.class_decl.fields = fields;
    cls_node->as.class_decl.field_count = field_count;
    cls_node->as.class_decl.methods = methods;
    cls_node->as.class_decl.method_count = method_count;
    return cls_node;
}

static AstNode *parse_function(Parser *p) {
    Token tok = consume(p, TOKEN_FN, "Expected 'fn'");
    Token name_tok = consume(p, TOKEN_IDENT, "Expected function name");
    consume(p, TOKEN_LPAREN, "Expected '(' after function name");

    char **param_names = NULL;
    Type *param_types = NULL;
    char **param_class_names = NULL;
    int param_count = 0;
    int param_cap = 0;

    if (!check(p, TOKEN_RPAREN)) {
        do {
            Token p_name = consume(p, TOKEN_IDENT, "Expected parameter name");
            consume(p, TOKEN_COLON, "Expected ':' after parameter name");
            char *p_cls = NULL;
            Type p_type = parse_type_with_class(p, &p_cls);

            if (param_count >= param_cap) {
                param_cap = param_cap == 0 ? 4 : param_cap * 2;
                char **new_names = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                Type *new_types = (Type *)arena_alloc_array(p->arena, param_cap, sizeof(Type));
                char **new_cls = (char **)arena_alloc_array(p->arena, param_cap, sizeof(char *));
                if (param_names) {
                    memcpy(new_names, param_names, param_count * sizeof(char *));
                    memcpy(new_types, param_types, param_count * sizeof(Type));
                    memcpy(new_cls, param_class_names, param_count * sizeof(char *));
                }
                param_names = new_names;
                param_types = new_types;
                param_class_names = new_cls;
            }
            param_names[param_count] = arena_strdup(p->arena, p_name.lexeme);
            param_types[param_count] = p_type;
            param_class_names[param_count] = p_cls;
            param_count++;
        } while (match(p, TOKEN_COMMA));
    }

    consume(p, TOKEN_RPAREN, "Expected ')' after parameters");
    consume(p, TOKEN_ARROW, "Expected '->' after function signature");
    char *ret_cls = NULL;
    Type return_type = parse_type_with_class(p, &ret_cls);
    AstNode *body = parse_block(p);

    AstNode *node = arena_alloc_node(p->arena, NODE_FUNCTION, tok.line);
    node->as.function.name = arena_strdup(p->arena, name_tok.lexeme);
    node->as.function.param_names = param_names;
    node->as.function.param_types = param_types;
    node->as.function.param_class_names = param_class_names;
    node->as.function.param_count = param_count;
    node->as.function.return_type = return_type;
    node->as.function.return_class_name = ret_cls;
    node->as.function.body = body;
    return node;
}

AstNode *parse_program(Parser *p) {
    AstNode **classes = NULL;
    int class_count = 0;
    int class_cap = 0;

    AstNode **functions = NULL;
    int fn_count = 0;
    int fn_cap = 0;

    while (!is_at_end(p)) {
        if (check(p, TOKEN_CLASS)) {
            AstNode *cls = parse_class(p);
            if (class_count >= class_cap) {
                class_cap = class_cap == 0 ? 4 : class_cap * 2;
                AstNode **new_cls = (AstNode **)arena_alloc_array(p->arena, class_cap, sizeof(AstNode *));
                if (classes) memcpy(new_cls, classes, class_count * sizeof(AstNode *));
                classes = new_cls;
            }
            classes[class_count++] = cls;
        } else if (check(p, TOKEN_FN)) {
            AstNode *fn = parse_function(p);
            if (fn_count >= fn_cap) {
                fn_cap = fn_cap == 0 ? 4 : fn_cap * 2;
                AstNode **new_fns = (AstNode **)arena_alloc_array(p->arena, fn_cap, sizeof(AstNode *));
                if (functions) memcpy(new_fns, functions, fn_count * sizeof(AstNode *));
                functions = new_fns;
            }
            functions[fn_count++] = fn;
        } else {
            error_at(peek(p), "Expected 'class' or 'fn' at top level");
        }
    }

    AstNode *prog = arena_alloc_node(p->arena, NODE_PROGRAM, 1);
    prog->as.program.classes = classes;
    prog->as.program.class_count = class_count;
    prog->as.program.functions = functions;
    prog->as.program.count = fn_count;
    return prog;
}
