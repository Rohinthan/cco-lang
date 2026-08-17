#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include "class_decl.h"
#include "stdlib_prelude.h"
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
    AstNode *program;
    AstNode *current_function;
    ClassTable *ct;
} CodegenCtx;

static bool is_struct_name(CodegenCtx *ctx, const char *name) {
    if (!ctx || !ctx->ct || !name) return false;
    return resolve_type_name(ctx->ct, name) == TYPE_KIND_STRUCT;
}

static void emit_indent(CodegenCtx *ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        sb_append(&ctx->sb, "    ");
    }
}

static const char *c_type_str_full(CodegenCtx *ctx, Type t, const char *class_name, bool is_borrowed, bool is_heap_owner) {
    if (t == TY_CLASS && class_name) {
        static char buf[128];
        if (is_struct_name(ctx, class_name)) {
            if (is_borrowed) {
                snprintf(buf, sizeof(buf), "%s *", class_name);
            } else {
                snprintf(buf, sizeof(buf), "%s", class_name);
            }
            return buf;
        }
        snprintf(buf, sizeof(buf), "%s *", class_name);
        return buf;
    }
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

static const char *c_type_str_decl(CodegenCtx *ctx, Type t, const char *class_name, bool is_array, bool is_heap_owner) {
    if (is_array) {
        if (t == TY_CLASS && class_name) {
            static char buf[128];
            if (is_struct_name(ctx, class_name)) {
                snprintf(buf, sizeof(buf), "%s *", class_name);
            } else {
                snprintf(buf, sizeof(buf), "%s **", class_name);
            }
            return buf;
        }
        switch (t) {
            case TY_INT: return "int *";
            case TY_FLOAT: return "double *";
            case TY_CHAR: return "char *";
            case TY_BOOL: return "bool *";
            case TY_STRING: return "char **";
            default: return "void *";
        }
    }
    return c_type_str_full(ctx, t, class_name, false, is_heap_owner);
}

static bool is_expr_pointer(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return true;

    if (expr->type == NODE_IDENT) {
        const char *name = expr->as.ident.name;
        if (ctx->current_function) {
            AstNode *fn = ctx->current_function;
            if (fn->type == NODE_FUNCTION) {
                for (int i = 0; i < fn->as.function.param_count; i++) {
                    if (strcmp(fn->as.function.param_names[i], name) == 0) {
                        const char *cname = fn->as.function.param_class_names[i];
                        if (cname && is_struct_name(ctx, cname)) {
                            return fn->as.function.param_is_borrowed[i];
                        }
                        return true;
                    }
                }
            } else if (fn->type == NODE_METHOD) {
                if (strcmp(name, "self") == 0) return true;
                for (int i = 0; i < fn->as.method.param_count; i++) {
                    if (strcmp(fn->as.method.param_names[i], name) == 0) {
                        const char *cname = fn->as.method.param_class_names[i];
                        if (cname && is_struct_name(ctx, cname)) {
                            return fn->as.method.param_is_borrowed[i];
                        }
                        return true;
                    }
                }
            }

            AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : fn->as.method.body;
            if (body && body->type == NODE_BLOCK) {
                for (int i = 0; i < body->as.block.count; i++) {
                    AstNode *stmt = body->as.block.stmts[i];
                    if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, name) == 0) {
                        const char *cname = stmt->as.let.class_name;
                        if (cname && is_struct_name(ctx, cname)) {
                            return false;
                        }
                    }
                }
            }
        }
    }

    if (expr->type == NODE_NEW) {
        const char *cname = expr->as.new_expr.class_name;
        if (cname && is_struct_name(ctx, cname)) {
            return false;
        }
    }

    if (expr->type == NODE_CALL) {
        const char *callee = expr->as.call.callee;
        if (ctx->program && ctx->program->type == NODE_PROGRAM) {
            for (int i = 0; i < ctx->program->as.program.count; i++) {
                AstNode *target_fn = ctx->program->as.program.functions[i];
                if (strcmp(target_fn->as.function.name, callee) == 0) {
                    const char *cname = target_fn->as.function.return_class_name;
                    if (cname && is_struct_name(ctx, cname)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

static const char *get_expr_elem_class_name(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return NULL;
    if (expr->type == NODE_ALLOC && expr->as.alloc.elem_type == TY_CLASS) {
        return expr->as.alloc.class_name;
    }
    if (expr->type == NODE_IDENT && ctx->current_function) {
        const char *var_name = expr->as.ident.name;
        AstNode *fn = ctx->current_function;
        if (fn->type == NODE_FUNCTION) {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (strcmp(fn->as.function.param_names[p], var_name) == 0) {
                    return fn->as.function.param_class_names[p];
                }
            }
        } else if (fn->type == NODE_METHOD) {
            for (int p = 0; p < fn->as.method.param_count; p++) {
                if (strcmp(fn->as.method.param_names[p], var_name) == 0) {
                    return fn->as.method.param_class_names[p];
                }
            }
        }
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : fn->as.method.body;
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.class_name;
                }
            }
        }
    }
    return NULL;
}

static void gen_expr(CodegenCtx *ctx, AstNode *expr);

static Type infer_expr_type(AstNode *program, AstNode *fn, AstNode *expr) {
    if (!expr) return TY_INT;

    if (expr->type == NODE_LITERAL) {
        return expr->as.literal.lit_type;
    }
    if (expr->type == NODE_ALLOC) {
        return expr->as.alloc.elem_type;
    }
    if (expr->type == NODE_CALL) {
        const char *name = expr->as.call.callee;
        if (strcmp(name, "len") == 0 || strcmp(name, "abs_int") == 0 || strcmp(name, "min_int") == 0 || strcmp(name, "max_int") == 0) return TY_INT;
        if (strcmp(name, "sqrt") == 0 || strcmp(name, "pow") == 0 || strcmp(name, "abs_float") == 0 || strcmp(name, "floor") == 0 || strcmp(name, "ceil") == 0 || strcmp(name, "min_float") == 0 || strcmp(name, "max_float") == 0) return TY_FLOAT;
        if (strcmp(name, "concat") == 0 || strcmp(name, "substring") == 0 || strcmp(name, "read_file") == 0) return TY_STRING;
        if (strcmp(name, "equals") == 0 || strcmp(name, "write_file") == 0) return TY_BOOL;
        if (strcmp(name, "char_at") == 0) return TY_CHAR;

        if (program && program->type == NODE_PROGRAM) {
            for (int f = 0; f < program->as.program.count; f++) {
                AstNode *target_fn = program->as.program.functions[f];
                if (strcmp(target_fn->as.function.name, name) == 0) {
                    return target_fn->as.function.return_type;
                }
            }
        }
    }
    if (expr->type == NODE_METHOD_CALL) {
        const char *mname = expr->as.method_call.method_name;
        const char *cname = expr->as.method_call.target_class_name;
        if (cname && program && program->type == NODE_PROGRAM) {
            for (int c = 0; c < program->as.program.class_count; c++) {
                AstNode *cls = program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, cname) == 0) {
                    for (int m = 0; m < cls->as.class_decl.method_count; m++) {
                        AstNode *mn = cls->as.class_decl.methods[m];
                        if (strcmp(mn->as.method.name, mname) == 0) {
                            return mn->as.method.return_type;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && program && program->type == NODE_PROGRAM) {
        const char *mname = expr->as.member.member_name;
        const char *cname = expr->as.member.field_class_name;
        if (!cname && expr->as.member.object->type == NODE_IDENT && fn) {
            const char *obj_name = expr->as.member.object->as.ident.name;
            if (fn->type == NODE_METHOD && strcmp(obj_name, "self") == 0) {
                cname = fn->as.method.param_class_names ? fn->as.method.param_class_names[0] : NULL;
            } else if (fn->type == NODE_FUNCTION) {
                for (int p = 0; p < fn->as.function.param_count; p++) {
                    if (strcmp(fn->as.function.param_names[p], obj_name) == 0) {
                        cname = fn->as.function.param_class_names[p];
                    }
                }
            }
        }
        if (cname) {
            for (int c = 0; c < program->as.program.class_count; c++) {
                AstNode *cls = program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, cname) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        if (strcmp(cls->as.class_decl.fields[f]->as.field.name, mname) == 0) {
                            return cls->as.class_decl.fields[f]->as.field.type;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_IDENT && fn) {
        const char *var_name = expr->as.ident.name;
        if (fn->type == NODE_FUNCTION) {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (strcmp(fn->as.function.param_names[p], var_name) == 0) {
                    return fn->as.function.param_types[p];
                }
            }
        } else if (fn->type == NODE_METHOD) {
            for (int p = 0; p < fn->as.method.param_count; p++) {
                if (strcmp(fn->as.method.param_names[p], var_name) == 0) {
                    return fn->as.method.param_types[p];
                }
            }
        }
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : fn->as.method.body;
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.var_type;
                }
            }
        }
    }

    return TY_INT;
}

static void gen_stmt(CodegenCtx *ctx, AstNode *stmt);
static void gen_block(CodegenCtx *ctx, AstNode *block_node);

static void emit_frees(CodegenCtx *ctx, AstNode *node) {
    if (!node || node->frees_count == 0) return;
    for (int i = 0; i < node->frees_count; i++) {
        emit_indent(ctx);
        sb_appendf(&ctx->sb, "free(%s);\n", node->frees_to_emit[i]);
    }
}

static void emit_releases(CodegenCtx *ctx, AstNode *node) {
    if (!node || node->releases_count == 0) return;
    for (int i = 0; i < node->releases_count; i++) {
        emit_indent(ctx);
        if (node->releases_to_emit[i].is_array) {
            const char *arr = node->releases_to_emit[i].var_name;
            const char *cls = node->releases_to_emit[i].class_name;
            sb_appendf(&ctx->sb, "if (%s != NULL) {\n", arr);
            ctx->indent_level++;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(%s); __i++) {\n", arr);
            ctx->indent_level++;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "if (%s[__i] != NULL) { %s_free(%s[__i]); }\n", arr, cls, arr);
            ctx->indent_level--;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "}\n");
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "__cco_free_arr(%s);\n", arr);
            ctx->indent_level--;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "}\n");
        } else {
            sb_appendf(&ctx->sb, "%s_free(%s);\n", node->releases_to_emit[i].class_name, node->releases_to_emit[i].var_name);
        }
    }
}

static AstNode *find_class_ast(AstNode *program, const char *name) {
    if (!program || program->type != NODE_PROGRAM || !name) return NULL;
    for (int i = 0; i < program->as.program.class_count; i++) {
        AstNode *cls = program->as.program.classes[i];
        if (strcmp(cls->as.class_decl.name, name) == 0) return cls;
    }
    return NULL;
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
                char c = expr->as.literal.val.c;
                if (c == '\n') sb_append(&ctx->sb, "'\\n'");
                else if (c == '\t') sb_append(&ctx->sb, "'\\t'");
                else if (c == '\r') sb_append(&ctx->sb, "'\\r'");
                else if (c == '\0') sb_append(&ctx->sb, "'\\0'");
                else if (c == '\\') sb_append(&ctx->sb, "'\\\\'");
                else if (c == '\'') sb_append(&ctx->sb, "'\\''");
                else sb_appendf(&ctx->sb, "'%c'", c);
            }
            break;

        case NODE_IDENT:
            sb_append(&ctx->sb, expr->as.ident.name);
            break;

        case NODE_ALLOC:
            if (expr->as.alloc.elem_type == TY_CLASS) {
                sb_appendf(&ctx->sb, "(%s **)__cco_alloc_arr(sizeof(%s *), ", expr->as.alloc.class_name, expr->as.alloc.class_name);
                gen_expr(ctx, expr->as.alloc.count_expr);
                sb_append(&ctx->sb, ")");
            } else {
                sb_appendf(&ctx->sb, "(%s *)malloc((", c_type_str_full(ctx, expr->as.alloc.elem_type, expr->as.alloc.class_name, false, false));
                gen_expr(ctx, expr->as.alloc.count_expr);
                sb_appendf(&ctx->sb, ") * sizeof(%s))", c_type_str_full(ctx, expr->as.alloc.elem_type, expr->as.alloc.class_name, false, false));
            }
            break;

        case NODE_NEW: {
            const char *cname = expr->as.new_expr.class_name;
            if (is_struct_name(ctx, cname)) {
                sb_appendf(&ctx->sb, "(%s){ ", cname);
                for (int k = 0; k < expr->as.new_expr.field_count; k++) {
                    if (k > 0) sb_append(&ctx->sb, ", ");
                    sb_appendf(&ctx->sb, ".%s = ", expr->as.new_expr.field_names[k]);
                    gen_expr(ctx, expr->as.new_expr.field_values[k]);
                }
                sb_append(&ctx->sb, " }");
            } else {
                AstNode *cls_node = find_class_ast(ctx->program, cname);
                sb_appendf(&ctx->sb, "%s_new(", cname);
                if (cls_node) {
                    for (int f = 0; f < cls_node->as.class_decl.field_count; f++) {
                        if (f > 0) sb_append(&ctx->sb, ", ");
                        const char *target_fname = cls_node->as.class_decl.fields[f]->as.field.name;
                        AstNode *fval = NULL;
                        for (int k = 0; k < expr->as.new_expr.field_count; k++) {
                            if (strcmp(expr->as.new_expr.field_names[k], target_fname) == 0) {
                                fval = expr->as.new_expr.field_values[k];
                                break;
                            }
                        }
                        if (fval) {
                            if (cls_node->as.class_decl.fields[f]->as.field.type == TY_STRING && fval->type == NODE_LITERAL) {
                                sb_appendf(&ctx->sb, "strdup(\"%s\")", fval->as.literal.val.s);
                            } else {
                                gen_expr(ctx, fval);
                            }
                        } else sb_append(&ctx->sb, "0");
                    }
                }
                sb_append(&ctx->sb, ")");
            }
            break;
        }

        case NODE_MEMBER:
            gen_expr(ctx, expr->as.member.object);
            if (is_expr_pointer(ctx, expr->as.member.object)) {
                sb_appendf(&ctx->sb, "->%s", expr->as.member.member_name);
            } else {
                sb_appendf(&ctx->sb, ".%s", expr->as.member.member_name);
            }
            break;

        case NODE_METHOD_CALL:
            sb_appendf(&ctx->sb, "%s_%s(", expr->as.method_call.target_class_name ? expr->as.method_call.target_class_name : "Object", expr->as.method_call.method_name);
            gen_expr(ctx, expr->as.method_call.object);
            for (int i = 0; i < expr->as.method_call.arg_count; i++) {
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.method_call.args[i]);
            }
            sb_append(&ctx->sb, ")");
            break;

        case NODE_INDEX:
            if (expr->as.index.array_expr) {
                gen_expr(ctx, expr->as.index.array_expr);
            } else {
                sb_append(&ctx->sb, expr->as.index.array_name);
            }
            sb_append(&ctx->sb, "[");
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

        case NODE_CALL: {
            const char *callee = expr->as.call.callee;
            if (strcmp(callee, "len") == 0) {
                sb_append(&ctx->sb, "((int)strlen(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "concat") == 0) {
                sb_append(&ctx->sb, "__cco_concat(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "equals") == 0) {
                sb_append(&ctx->sb, "(strcmp(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ") == 0)");
            } else if (strcmp(callee, "char_at") == 0) {
                sb_append(&ctx->sb, "__cco_char_at(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "substring") == 0) {
                sb_append(&ctx->sb, "__cco_substring(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[2]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "sqrt") == 0) {
                sb_append(&ctx->sb, "((float)sqrt(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "pow") == 0) {
                sb_append(&ctx->sb, "((float)pow(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "abs_int") == 0) {
                sb_append(&ctx->sb, "__cco_abs_int(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "abs_float") == 0) {
                sb_append(&ctx->sb, "((float)fabs(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "floor") == 0) {
                sb_append(&ctx->sb, "((float)floor(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "ceil") == 0) {
                sb_append(&ctx->sb, "((float)ceil(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "min_int") == 0) {
                sb_append(&ctx->sb, "__cco_min_int(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "max_int") == 0) {
                sb_append(&ctx->sb, "__cco_max_int(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "min_float") == 0) {
                sb_append(&ctx->sb, "__cco_min_float(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "max_float") == 0) {
                sb_append(&ctx->sb, "__cco_max_float(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "read_file") == 0) {
                sb_append(&ctx->sb, "__cco_read_file(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "write_file") == 0) {
                sb_append(&ctx->sb, "__cco_write_file(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else {
                sb_appendf(&ctx->sb, "%s(", callee);
                for (int i = 0; i < expr->as.call.arg_count; i++) {
                    if (i > 0) sb_append(&ctx->sb, ", ");
                    gen_expr(ctx, expr->as.call.args[i]);
                }
                sb_append(&ctx->sb, ")");
            }
            break;
        }

        default:
            break;
    }
}

static void gen_stmt(CodegenCtx *ctx, AstNode *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case NODE_LET:
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s %s = ", c_type_str_decl(ctx, stmt->as.let.var_type, stmt->as.let.class_name, stmt->as.let.is_array, stmt->is_heap_owner), stmt->as.let.name);
            if (stmt->as.let.var_type == TY_STRING && stmt->as.let.value->type == NODE_LITERAL) {
                sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.let.value->as.literal.val.s);
            } else {
                gen_expr(ctx, stmt->as.let.value);
            }
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_ASSIGN:
            emit_frees(ctx, stmt);
            if (stmt->as.assign.release_old && stmt->as.assign.class_name) {
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "%s_free(%s);\n", stmt->as.assign.class_name, stmt->as.assign.name);
            }
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s = ", stmt->as.assign.name);
            if (stmt->as.assign.value->type == NODE_LITERAL && stmt->as.assign.value->as.literal.lit_type == TY_STRING) {
                sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.assign.value->as.literal.val.s);
            } else {
                gen_expr(ctx, stmt->as.assign.value);
            }
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_MEMBER_ASSIGN:
            if (stmt->as.member_assign.release_old && stmt->as.member_assign.field_class_name) {
                const char *fcls = stmt->as.member_assign.field_class_name;
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "%s_free(", fcls);
                gen_expr(ctx, stmt->as.member_assign.object);
                if (is_expr_pointer(ctx, stmt->as.member_assign.object)) {
                    sb_appendf(&ctx->sb, "->%s);\n", stmt->as.member_assign.member_name);
                } else {
                    sb_appendf(&ctx->sb, ".%s);\n", stmt->as.member_assign.member_name);
                }
            }
            emit_indent(ctx);
            gen_expr(ctx, stmt->as.member_assign.object);
            if (is_expr_pointer(ctx, stmt->as.member_assign.object)) {
                sb_appendf(&ctx->sb, "->%s = ", stmt->as.member_assign.member_name);
            } else {
                sb_appendf(&ctx->sb, ".%s = ", stmt->as.member_assign.member_name);
            }
            gen_expr(ctx, stmt->as.member_assign.value);
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_INDEX_ASSIGN:
            emit_indent(ctx);
            if (stmt->as.index_assign.array_expr) {
                gen_expr(ctx, stmt->as.index_assign.array_expr);
            } else {
                sb_append(&ctx->sb, stmt->as.index_assign.array_name);
            }
            sb_append(&ctx->sb, "[");
            gen_expr(ctx, stmt->as.index_assign.index);
            sb_append(&ctx->sb, "] = ");
            gen_expr(ctx, stmt->as.index_assign.value);
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_FOR_EACH: {
            emit_indent(ctx);
            sb_append(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(");
            if (stmt->as.for_each.collection_expr->type == NODE_IDENT) {
                sb_append(&ctx->sb, stmt->as.for_each.collection_expr->as.ident.name);
            } else {
                gen_expr(ctx, stmt->as.for_each.collection_expr);
            }
            sb_append(&ctx->sb, "); __i++) {\n");
            ctx->indent_level++;

            const char *elem_cls = get_expr_elem_class_name(ctx, stmt->as.for_each.collection_expr);
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s *%s = ", elem_cls ? elem_cls : "void", stmt->as.for_each.loop_var_name);
            if (stmt->as.for_each.collection_expr->type == NODE_IDENT) {
                sb_append(&ctx->sb, stmt->as.for_each.collection_expr->as.ident.name);
            } else {
                gen_expr(ctx, stmt->as.for_each.collection_expr);
            }
            sb_append(&ctx->sb, "[__i];\n");

            emit_indent(ctx);
            sb_appendf(&ctx->sb, "if (%s == NULL) continue;\n", stmt->as.for_each.loop_var_name);

            if (stmt->as.for_each.body->type == NODE_BLOCK) {
                gen_block(ctx, stmt->as.for_each.body);
            } else {
                gen_stmt(ctx, stmt->as.for_each.body);
            }

            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");
            break;
        }

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
                    sb_appendf(&ctx->sb, "%s %s = ", c_type_str_full(ctx, stmt->as.for_stmt.init->as.let.var_type, stmt->as.for_stmt.init->as.let.class_name, false, stmt->as.for_stmt.init->is_heap_owner), stmt->as.for_stmt.init->as.let.name);
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
                if (stmt->frees_count > 0 || stmt->releases_count > 0) {
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "__typeof__(");
                    gen_expr(ctx, stmt->as.return_stmt.value);
                    sb_append(&ctx->sb, ") __cco_ret_val = ");
                    gen_expr(ctx, stmt->as.return_stmt.value);
                    sb_append(&ctx->sb, ";\n");

                    emit_frees(ctx, stmt);
                    emit_releases(ctx, stmt);

                    emit_indent(ctx);
                    sb_append(&ctx->sb, "return __cco_ret_val;\n");
                } else {
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "return ");
                    gen_expr(ctx, stmt->as.return_stmt.value);
                    sb_append(&ctx->sb, ";\n");
                }
            } else {
                emit_frees(ctx, stmt);
                emit_releases(ctx, stmt);
                emit_indent(ctx);
                sb_append(&ctx->sb, "return;\n");
            }
            break;

        case NODE_BREAK:
            emit_frees(ctx, stmt);
            emit_releases(ctx, stmt);
            emit_indent(ctx);
            sb_append(&ctx->sb, "break;\n");
            break;

        case NODE_CONTINUE:
            emit_frees(ctx, stmt);
            emit_releases(ctx, stmt);
            emit_indent(ctx);
            sb_append(&ctx->sb, "continue;\n");
            break;

        case NODE_PRINT: {
            emit_indent(ctx);
            AstNode *val = stmt->as.print_stmt.value;
            Type t = infer_expr_type(ctx->program, ctx->current_function, val);
            if (t == TY_STRING) {
                sb_append(&ctx->sb, "printf(\"%s\\n\", ");
                gen_expr(ctx, val);
                sb_append(&ctx->sb, ");\n");
            } else if (t == TY_FLOAT) {
                sb_append(&ctx->sb, "printf(\"%g\\n\", (double)(");
                gen_expr(ctx, val);
                sb_append(&ctx->sb, "));\n");
            } else if (t == TY_BOOL) {
                sb_append(&ctx->sb, "printf(\"%s\\n\", (");
                gen_expr(ctx, val);
                sb_append(&ctx->sb, ") ? \"true\" : \"false\");\n");
            } else if (t == TY_CHAR) {
                sb_append(&ctx->sb, "printf(\"%c\\n\", ");
                gen_expr(ctx, val);
                sb_append(&ctx->sb, ");\n");
            } else {
                sb_append(&ctx->sb, "printf(\"%ld\\n\", (long)(");
                gen_expr(ctx, val);
                sb_append(&ctx->sb, "));\n");
            }
            break;
        }

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

    bool last_is_jump = false;
    if (block_node->as.block.count > 0) {
        NodeType last_type = block_node->as.block.stmts[block_node->as.block.count - 1]->type;
        if (last_type == NODE_RETURN || last_type == NODE_BREAK || last_type == NODE_CONTINUE) {
            last_is_jump = true;
        }
    }

    if (!last_is_jump) {
        emit_frees(ctx, block_node);
        emit_releases(ctx, block_node);
    }

    ctx->indent_level--;
    emit_indent(ctx);
    sb_append(&ctx->sb, "}\n");
}

static void gen_struct_typedefs(CodegenCtx *ctx, AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) return;

    for (int i = 0; i < program->as.program.struct_count; i++) {
        AstNode *st = program->as.program.structs[i];
        sb_appendf(&ctx->sb, "typedef struct {\n");
        for (int f = 0; f < st->as.struct_decl.field_count; f++) {
            AstNode *f_node = st->as.struct_decl.fields[f];
            sb_appendf(&ctx->sb, "    %s %s;\n",
                       c_type_str_full(ctx, f_node->as.struct_field_decl.field_type, NULL, false, false),
                       f_node->as.struct_field_decl.name);
        }
        sb_appendf(&ctx->sb, "} %s;\n\n", st->as.struct_decl.name);
    }
}

static void gen_class_helpers(CodegenCtx *ctx, AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) return;

    for (int i = 0; i < program->as.program.class_count; i++) {
        AstNode *cls = program->as.program.classes[i];
        sb_appendf(&ctx->sb, "typedef struct %s %s;\n", cls->as.class_decl.name, cls->as.class_decl.name);
    }
    if (program->as.program.class_count > 0) sb_append(&ctx->sb, "\n");

    for (int i = 0; i < program->as.program.class_count; i++) {
        AstNode *cls = program->as.program.classes[i];
        sb_appendf(&ctx->sb, "struct %s {\n", cls->as.class_decl.name);
        for (int f = 0; f < cls->as.class_decl.field_count; f++) {
            AstNode *f_node = cls->as.class_decl.fields[f];
            sb_appendf(&ctx->sb, "    %s %s;\n",
                       c_type_str_full(ctx, f_node->as.field.type, f_node->as.field.class_name, false, f_node->is_heap_owner),
                       f_node->as.field.name);
        }
        sb_append(&ctx->sb, "};\n\n");
    }

    for (int i = 0; i < program->as.program.class_count; i++) {
        AstNode *cls = program->as.program.classes[i];
        const char *cname = cls->as.class_decl.name;

        sb_appendf(&ctx->sb, "static inline void %s_free(%s *p) {\n", cname, cname);
        sb_append(&ctx->sb, "    if (p) {\n");
        for (int f = 0; f < cls->as.class_decl.field_count; f++) {
            AstNode *f_node = cls->as.class_decl.fields[f];
            if (f_node->as.field.type == TY_CLASS && f_node->as.field.class_name) {
                sb_appendf(&ctx->sb, "        %s_free(p->%s);\n", f_node->as.field.class_name, f_node->as.field.name);
            } else if (f_node->is_heap_owner || f_node->as.field.type == TY_STRING) {
                sb_appendf(&ctx->sb, "        if (p->%s) free(p->%s);\n", f_node->as.field.name, f_node->as.field.name);
            }
        }
        sb_append(&ctx->sb, "        free(p);\n");
        sb_append(&ctx->sb, "    }\n");
        sb_append(&ctx->sb, "}\n\n");

        sb_appendf(&ctx->sb, "static inline %s *%s_new(", cname, cname);
        if (cls->as.class_decl.field_count == 0) {
            sb_append(&ctx->sb, "void");
        } else {
            for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                if (f > 0) sb_append(&ctx->sb, ", ");
                AstNode *f_node = cls->as.class_decl.fields[f];
                sb_appendf(&ctx->sb, "%s %s",
                           c_type_str_full(ctx, f_node->as.field.type, f_node->as.field.class_name, false, f_node->is_heap_owner),
                           f_node->as.field.name);
            }
        }
        sb_append(&ctx->sb, ") {\n");
        sb_appendf(&ctx->sb, "    %s *__obj = (%s *)malloc(sizeof(%s));\n", cname, cname, cname);
        for (int f = 0; f < cls->as.class_decl.field_count; f++) {
            AstNode *f_node = cls->as.class_decl.fields[f];
            const char *fname = f_node->as.field.name;
            sb_appendf(&ctx->sb, "    __obj->%s = %s;\n", fname, fname);
        }
        sb_append(&ctx->sb, "    return __obj;\n");
        sb_append(&ctx->sb, "}\n\n");
    }
}

static void gen_method(CodegenCtx *ctx, const char *class_name, AstNode *m_node) {
    ctx->current_function = m_node;
    sb_appendf(&ctx->sb, "%s %s_%s(",
               c_type_str_full(ctx, m_node->as.method.return_type, m_node->as.method.return_class_name, false, m_node->as.method.returns_heap_pointer),
               class_name, m_node->as.method.name);

    if (m_node->as.method.param_count == 0) {
        sb_append(&ctx->sb, "void");
    } else {
        for (int i = 0; i < m_node->as.method.param_count; i++) {
            if (i > 0) sb_append(&ctx->sb, ", ");
            bool is_bor = m_node->as.method.param_is_borrowed ? m_node->as.method.param_is_borrowed[i] : false;
            sb_appendf(&ctx->sb, "%s %s",
                       c_type_str_full(ctx, m_node->as.method.param_types[i], m_node->as.method.param_class_names[i], is_bor, false),
                       m_node->as.method.param_names[i]);
        }
    }
    sb_append(&ctx->sb, ") ");

    gen_block(ctx, m_node->as.method.body);
    sb_append(&ctx->sb, "\n");
}

static void gen_function(CodegenCtx *ctx, AstNode *fn) {
    ctx->current_function = fn;
    sb_appendf(&ctx->sb, "%s %s(", c_type_str_full(ctx, fn->as.function.return_type, fn->as.function.return_class_name, false, fn->as.function.returns_heap_pointer), fn->as.function.name);

    if (fn->as.function.param_count == 0) {
        sb_append(&ctx->sb, "void");
    } else {
        for (int i = 0; i < fn->as.function.param_count; i++) {
            if (i > 0) sb_append(&ctx->sb, ", ");
            bool is_bor = fn->as.function.param_is_borrowed ? fn->as.function.param_is_borrowed[i] : false;
            sb_appendf(&ctx->sb, "%s %s", c_type_str_full(ctx, fn->as.function.param_types[i], fn->as.function.param_class_names[i], is_bor, false), fn->as.function.param_names[i]);
        }
    }
    sb_append(&ctx->sb, ") ");

    gen_block(ctx, fn->as.function.body);
    sb_append(&ctx->sb, "\n");
}

char *generate_c_code(AstNode *program, AstArena *arena) {
    CodegenCtx ctx;
    ctx.sb = create_buffer();
    ctx.indent_level = 0;
    ctx.program = program;
    ctx.current_function = NULL;
    ctx.ct = build_class_table(program, arena);

    // Fixed C prelude
    sb_append(&ctx.sb, "/* Generated by Cco (C--) Compiler */\n");
    sb_append(&ctx.sb, "#define _POSIX_C_SOURCE 200809L\n");
    sb_append(&ctx.sb, "#include <stdio.h>\n");
    sb_append(&ctx.sb, "#include <stdlib.h>\n");
    sb_append(&ctx.sb, "#include <stdbool.h>\n");
    sb_append(&ctx.sb, "#include <string.h>\n\n");
    sb_append(&ctx.sb, STDLIB_PRELUDE_C);
    sb_append(&ctx.sb, "\n");

    // Struct definitions
    gen_struct_typedefs(&ctx, program);

    // Class struct definitions, retain/release/new helpers
    gen_class_helpers(&ctx, program);

    // Method definitions
    for (int i = 0; i < program->as.program.class_count; i++) {
        AstNode *cls = program->as.program.classes[i];
        for (int m = 0; m < cls->as.class_decl.method_count; m++) {
            gen_method(&ctx, cls->as.class_decl.name, cls->as.class_decl.methods[m]);
        }
    }

    // Function definitions
    for (int i = 0; i < program->as.program.count; i++) {
        gen_function(&ctx, program->as.program.functions[i]);
    }

    return ctx.sb.buffer;
}
