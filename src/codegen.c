#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} StringBuffer;

static StringBuffer create_buffer(void) {
    StringBuffer sb;
    sb.capacity = 1024;
    sb.length = 0;
    sb.buffer = malloc(sb.capacity);
    sb.buffer[0] = '\0';
    return sb;
}

static void sb_append(StringBuffer *sb, const char *str) {
    size_t len = strlen(str);
    if (sb->length + len + 1 > sb->capacity) {
        while (sb->length + len + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->buffer = realloc(sb->buffer, sb->capacity);
    }
    memcpy(sb->buffer + sb->length, str, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0';
}

static void sb_appendf(StringBuffer *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sb_append(sb, buf);
}

typedef struct {
    StringBuffer sb;
    int indent_level;
} CodegenCtx;

static void emit_indent(CodegenCtx *ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        sb_append(&ctx->sb, "    ");
    }
}

static const char *c_type_str(Type t, bool is_heap_owner) {
    switch (t) {
        case TY_INT: return is_heap_owner ? "int *" : "int";
        case TY_FLOAT: return is_heap_owner ? "double *" : "double";
        case TY_CHAR: return is_heap_owner ? "char *" : "char";
        case TY_BOOL: return is_heap_owner ? "bool *" : "bool";
        case TY_STRING: return "char *";
        case TY_VOID: return "void";
        default: return "int";
    }
}

static void gen_expr(CodegenCtx *ctx, AstNode *expr);
static void gen_stmt(CodegenCtx *ctx, AstNode *stmt);
static void gen_block(CodegenCtx *ctx, AstNode *block_node);

static void emit_frees(CodegenCtx *ctx, AstNode *node) {
    if (!node || node->frees_count == 0) return;
    for (int i = 0; i < node->frees_count; i++) {
        emit_indent(ctx);
        sb_appendf(&ctx->sb, "free(%s);\n", node->frees_to_emit[i]);
    }
}

static void gen_expr(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return;

    switch (expr->type) {
        case NODE_LITERAL:
            if (expr->as.literal.lit_type == TY_INT) {
                sb_appendf(&ctx->sb, "%ld", expr->as.literal.val.i);
            } else if (expr->as.literal.lit_type == TY_FLOAT) {
                sb_appendf(&ctx->sb, "%g", expr->as.literal.val.f);
            } else if (expr->as.literal.lit_type == TY_STRING) {
                sb_appendf(&ctx->sb, "\"%s\"", expr->as.literal.val.s);
            } else if (expr->as.literal.lit_type == TY_BOOL) {
                sb_append(&ctx->sb, expr->as.literal.val.b ? "true" : "false");
            } else if (expr->as.literal.lit_type == TY_CHAR) {
                sb_appendf(&ctx->sb, "'%c'", expr->as.literal.val.c);
            }
            break;

        case NODE_IDENT:
            sb_append(&ctx->sb, expr->as.ident.name);
            break;

        case NODE_ALLOC:
            sb_appendf(&ctx->sb, "(%s *)malloc((", c_type_str(expr->as.alloc.elem_type, false));
            gen_expr(ctx, expr->as.alloc.count_expr);
            sb_appendf(&ctx->sb, ") * sizeof(%s))", c_type_str(expr->as.alloc.elem_type, false));
            break;

        case NODE_INDEX:
            sb_appendf(&ctx->sb, "%s[", expr->as.index.array_name);
            gen_expr(ctx, expr->as.index.index);
            sb_append(&ctx->sb, "]");
            break;

        case NODE_BINARY:
            sb_append(&ctx->sb, "(");
            gen_expr(ctx, expr->as.binary.left);
            sb_appendf(&ctx->sb, " %s ", expr->as.binary.op);
            gen_expr(ctx, expr->as.binary.right);
            sb_append(&ctx->sb, ")");
            break;

        case NODE_UNARY:
            sb_appendf(&ctx->sb, "(%s", expr->as.unary.op);
            gen_expr(ctx, expr->as.unary.operand);
            sb_append(&ctx->sb, ")");
            break;

        case NODE_CALL:
            sb_appendf(&ctx->sb, "%s(", expr->as.call.callee);
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                if (i > 0) sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[i]);
            }
            sb_append(&ctx->sb, ")");
            break;

        default:
            break;
    }
}

static void gen_stmt(CodegenCtx *ctx, AstNode *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case NODE_LET:
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s %s = ", c_type_str(stmt->as.let.var_type, stmt->is_heap_owner), stmt->as.let.name);
            if (stmt->as.let.var_type == TY_STRING && stmt->as.let.value->type == NODE_LITERAL) {
                sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.let.value->as.literal.val.s);
            } else {
                gen_expr(ctx, stmt->as.let.value);
            }
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_ASSIGN:
            emit_frees(ctx, stmt); // Free old allocation if reassigning
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s = ", stmt->as.assign.name);
            if (stmt->as.assign.value->type == NODE_LITERAL && stmt->as.assign.value->as.literal.lit_type == TY_STRING) {
                sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.assign.value->as.literal.val.s);
            } else {
                gen_expr(ctx, stmt->as.assign.value);
            }
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_INDEX_ASSIGN:
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s[", stmt->as.index_assign.array_name);
            gen_expr(ctx, stmt->as.index_assign.index);
            sb_append(&ctx->sb, "] = ");
            gen_expr(ctx, stmt->as.index_assign.value);
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_IF:
            emit_indent(ctx);
            sb_append(&ctx->sb, "if (");
            gen_expr(ctx, stmt->as.if_stmt.cond);
            sb_append(&ctx->sb, ") ");
            if (stmt->as.if_stmt.then_b->type == NODE_BLOCK) {
                gen_block(ctx, stmt->as.if_stmt.then_b);
            } else {
                sb_append(&ctx->sb, "{\n");
                ctx->indent_level++;
                gen_stmt(ctx, stmt->as.if_stmt.then_b);
                ctx->indent_level--;
                emit_indent(ctx);
                sb_append(&ctx->sb, "}\n");
            }

            if (stmt->as.if_stmt.else_b) {
                emit_indent(ctx);
                sb_append(&ctx->sb, "else ");
                if (stmt->as.if_stmt.else_b->type == NODE_BLOCK || stmt->as.if_stmt.else_b->type == NODE_IF) {
                    gen_stmt(ctx, stmt->as.if_stmt.else_b);
                } else {
                    sb_append(&ctx->sb, "{\n");
                    ctx->indent_level++;
                    gen_stmt(ctx, stmt->as.if_stmt.else_b);
                    ctx->indent_level--;
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "}\n");
                }
            }
            break;

        case NODE_WHILE:
            emit_indent(ctx);
            sb_append(&ctx->sb, "while (");
            gen_expr(ctx, stmt->as.while_stmt.cond);
            sb_append(&ctx->sb, ") ");
            gen_block(ctx, stmt->as.while_stmt.body);
            break;

        case NODE_FOR:
            emit_indent(ctx);
            sb_append(&ctx->sb, "for (");
            if (stmt->as.for_stmt.init) {
                if (stmt->as.for_stmt.init->type == NODE_LET) {
                    sb_appendf(&ctx->sb, "%s %s = ", c_type_str(stmt->as.for_stmt.init->as.let.var_type, stmt->as.for_stmt.init->is_heap_owner), stmt->as.for_stmt.init->as.let.name);
                    gen_expr(ctx, stmt->as.for_stmt.init->as.let.value);
                } else if (stmt->as.for_stmt.init->type == NODE_ASSIGN) {
                    sb_appendf(&ctx->sb, "%s = ", stmt->as.for_stmt.init->as.assign.name);
                    gen_expr(ctx, stmt->as.for_stmt.init->as.assign.value);
                }
            }
            sb_append(&ctx->sb, "; ");
            if (stmt->as.for_stmt.cond) gen_expr(ctx, stmt->as.for_stmt.cond);
            sb_append(&ctx->sb, "; ");
            if (stmt->as.for_stmt.step) {
                if (stmt->as.for_stmt.step->type == NODE_ASSIGN) {
                    sb_appendf(&ctx->sb, "%s = ", stmt->as.for_stmt.step->as.assign.name);
                    gen_expr(ctx, stmt->as.for_stmt.step->as.assign.value);
                } else {
                    gen_expr(ctx, stmt->as.for_stmt.step);
                }
            }
            sb_append(&ctx->sb, ") ");
            gen_block(ctx, stmt->as.for_stmt.body);
            break;

        case NODE_RETURN:
            if (stmt->as.return_stmt.value) {
                if (stmt->frees_count > 0) {
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "__typeof__(");
                    gen_expr(ctx, stmt->as.return_stmt.value);
                    sb_append(&ctx->sb, ") __cmm_ret_val = ");
                    gen_expr(ctx, stmt->as.return_stmt.value);
                    sb_append(&ctx->sb, ";\n");

                    emit_frees(ctx, stmt);

                    emit_indent(ctx);
                    sb_append(&ctx->sb, "return __cmm_ret_val;\n");
                } else {
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "return ");
                    gen_expr(ctx, stmt->as.return_stmt.value);
                    sb_append(&ctx->sb, ";\n");
                }
            } else {
                emit_frees(ctx, stmt);
                emit_indent(ctx);
                sb_append(&ctx->sb, "return;\n");
            }
            break;

        case NODE_BREAK:
            emit_frees(ctx, stmt);
            emit_indent(ctx);
            sb_append(&ctx->sb, "break;\n");
            break;

        case NODE_CONTINUE:
            emit_frees(ctx, stmt);
            emit_indent(ctx);
            sb_append(&ctx->sb, "continue;\n");
            break;

        case NODE_PRINT:
            emit_indent(ctx);
            sb_append(&ctx->sb, "printf(");
            // Format string detection based on print expression
            AstNode *val = stmt->as.print_stmt.value;
            if (val->type == NODE_LITERAL) {
                if (val->as.literal.lit_type == TY_INT) sb_append(&ctx->sb, "\"%ld\\n\", ");
                else if (val->as.literal.lit_type == TY_FLOAT) sb_append(&ctx->sb, "\"%g\\n\", ");
                else if (val->as.literal.lit_type == TY_STRING) sb_append(&ctx->sb, "\"%s\\n\", ");
                else if (val->as.literal.lit_type == TY_BOOL) sb_append(&ctx->sb, "\"%s\\n\", ");
                else if (val->as.literal.lit_type == TY_CHAR) sb_append(&ctx->sb, "\"%c\\n\", ");
            } else {
                sb_append(&ctx->sb, "\"%d\\n\", (int)("); // Default int cast for variables/expressions
            }
            gen_expr(ctx, val);
            if (val->type != NODE_LITERAL) sb_append(&ctx->sb, ")");
            sb_append(&ctx->sb, ");\n");
            break;

        case NODE_EXPR_STMT:
            emit_indent(ctx);
            gen_expr(ctx, stmt->as.expr_stmt.expr);
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_BLOCK:
            gen_block(ctx, stmt);
            break;

        default:
            break;
    }
}

static void gen_block(CodegenCtx *ctx, AstNode *block_node) {
    sb_append(&ctx->sb, "{\n");
    ctx->indent_level++;

    for (int i = 0; i < block_node->as.block.count; i++) {
        gen_stmt(ctx, block_node->as.block.stmts[i]);
    }

    // Emit block fallthrough frees before closing brace
    emit_frees(ctx, block_node);

    ctx->indent_level--;
    emit_indent(ctx);
    sb_append(&ctx->sb, "}\n");
}

static void gen_function(CodegenCtx *ctx, AstNode *fn) {
    sb_appendf(&ctx->sb, "%s %s(", c_type_str(fn->as.function.return_type, fn->as.function.returns_heap_pointer), fn->as.function.name);

    if (fn->as.function.param_count == 0) {
        sb_append(&ctx->sb, "void");
    } else {
        for (int i = 0; i < fn->as.function.param_count; i++) {
            if (i > 0) sb_append(&ctx->sb, ", ");
            sb_appendf(&ctx->sb, "%s %s", c_type_str(fn->as.function.param_types[i], false), fn->as.function.param_names[i]);
        }
    }
    sb_append(&ctx->sb, ") ");

    gen_block(ctx, fn->as.function.body);
    sb_append(&ctx->sb, "\n");
}

char *generate_c_code(AstNode *program, AstArena *arena) {
    (void)arena;
    CodegenCtx ctx;
    ctx.sb = create_buffer();
    ctx.indent_level = 0;

    // Fixed C prelude
    sb_append(&ctx.sb, "/* Generated by CMM (C--) Compiler */\n");
    sb_append(&ctx.sb, "#define _POSIX_C_SOURCE 200809L\n");
    sb_append(&ctx.sb, "#include <stdio.h>\n");
    sb_append(&ctx.sb, "#include <stdlib.h>\n");
    sb_append(&ctx.sb, "#include <stdbool.h>\n");
    sb_append(&ctx.sb, "#include <string.h>\n\n");

    for (int i = 0; i < program->as.program.count; i++) {
        gen_function(&ctx, program->as.program.functions[i]);
    }

    return ctx.sb.buffer;
}
