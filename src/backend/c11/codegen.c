// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include "class_decl.h"
#include "stdlib_prelude.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>

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

static void sb_append_len(StringBuffer *sb, const char *str, size_t len) {
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
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len < 0) { va_end(args_copy); return; }
    char *buf = (char *)malloc(len + 1);
    if (!buf) { va_end(args_copy); return; }
    vsnprintf(buf, len + 1, fmt, args_copy);
    va_end(args_copy);
    sb_append(sb, buf);
    free(buf);
}

typedef struct {
    StringBuffer sb;
    int indent_level;
    AstNode *program;
    AstNode *current_function;
    ClassTable *ct;
    char emitted_helpers[256][64];
    int emitted_helper_count;
} CodegenCtx;

static bool helper_already_emitted(CodegenCtx *ctx, const char *name) {
    if (!ctx || !name) return false;
    int count = ctx->emitted_helper_count;
    if (count < 0) return false;
    if (count > 256) count = 256;
    for (int i = 0; i < count; i++) {
        if (strcmp(ctx->emitted_helpers[i], name) == 0) return true;
    }
    return false;
}

static void mark_helper_emitted(CodegenCtx *ctx, const char *name) {
    if (!ctx || !name) return;
    if (ctx->emitted_helper_count < 256) {
        strncpy(ctx->emitted_helpers[ctx->emitted_helper_count], name, 63);
        ctx->emitted_helpers[ctx->emitted_helper_count][63] = '\0';
        ctx->emitted_helper_count++;
    }
}

static bool is_struct_name(CodegenCtx *ctx, const char *name) {
    if (!ctx || !ctx->ct || !name) return false;
    return resolve_type_name(ctx->ct, name) == TYPE_KIND_STRUCT;
}

static const char *get_operator_fn_c_name(const char *struct_name, const char *op_symbol, int arity) {
    static char buf[256];
    const char *op_name = "unknown";
    if (strcmp(op_symbol, "+") == 0) op_name = "add";
    else if (strcmp(op_symbol, "-") == 0) op_name = (arity == 1) ? "neg" : "sub";
    else if (strcmp(op_symbol, "*") == 0) op_name = "mul";
    else if (strcmp(op_symbol, "/") == 0) op_name = "div";
    else if (strcmp(op_symbol, "==") == 0) op_name = "eq";
    else if (strcmp(op_symbol, "!=") == 0) op_name = "ne";
    else if (strcmp(op_symbol, "<") == 0) op_name = "lt";
    else if (strcmp(op_symbol, ">") == 0) op_name = "gt";
    else if (strcmp(op_symbol, "<=") == 0) op_name = "le";
    else if (strcmp(op_symbol, ">=") == 0) op_name = "ge";

    snprintf(buf, sizeof(buf), "__cco_operator_%s_%s", op_name, struct_name ? struct_name : "unknown");
    return buf;
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

static const char *c_type_str_decl_full(CodegenCtx *ctx, Type t, const char *class_name, bool is_array, bool is_map, bool is_heap_owner) {
    if (is_map) {
        return "__cco_map *";
    }
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

static const char *c_type_str_decl(CodegenCtx *ctx, Type t, const char *class_name, bool is_array, bool is_heap_owner) {
    return c_type_str_decl_full(ctx, t, class_name, is_array, false, is_heap_owner);
}

static const char *c_type_str_desc(CodegenCtx *ctx, const TypeDesc *td) {
    if (!td) return "void *";
    static char ring_buf[8][256];
    static int ring_idx = 0;
    char *buf = ring_buf[ring_idx++ % 8];
    switch (td->kind) {
        case TYPEDESC_PRIMITIVE:
            switch (td->as.primitive_desc.primitive_kind) {
                case TY_INT: return "int";
                case TY_FLOAT: return "double";
                case TY_CHAR: return "char";
                case TY_BOOL: return "bool";
                case TY_STRING: return "char *";
                case TY_VOID: return "void";
                default: return "int";
            }
        case TYPEDESC_CLASS:
        case TYPEDESC_ENUM:
        case TYPEDESC_NAMED: {
            const char *name = td->as.named_desc.name ? td->as.named_desc.name : "void";
            if (is_struct_name(ctx, name)) {
                return name;
            }
            snprintf(buf, 256, "%s *", name);
            return buf;
        }
        case TYPEDESC_MAP:
            return "__cco_map *";
        case TYPEDESC_ARRAY: {
            const char *elem_s = c_type_str_desc(ctx, td->as.array_desc.elem_type);
            snprintf(buf, 256, "%s *", elem_s);
            return buf;
        }
        default:
            return "void *";
    }
}

static const char *get_expr_class_name(AstNode *program, AstNode *fn, AstNode *expr);
static const char *find_ident_class_in_node(AstNode *program, AstNode *node, const char *var_name);
static TypeDesc *get_expr_type_desc(CodegenCtx *ctx, AstNode *expr);

static bool is_expr_pointer(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return true;

    if (expr->type == NODE_INDEX) {
        const char *cname = get_expr_class_name(ctx->program, ctx->current_function, expr);
        if (cname && is_struct_name(ctx, cname)) {
            return false;
        }
        return true;
    }

    if (expr->type == NODE_MEMBER) {
        const char *cname = expr->as.member.field_class_name;
        if (!cname) {
            cname = get_expr_class_name(ctx->program, ctx->current_function, expr);
        }
        if (cname && is_struct_name(ctx, cname)) {
            return false;
        }
        return true;
    }

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
            const char *found_cname = find_ident_class_in_node(ctx->program, body, name);
            if (found_cname && is_struct_name(ctx, found_cname)) {
                return false;
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

static Type infer_expr_type(AstNode *program, AstNode *fn, AstNode *expr);

static const char *find_ident_class_in_node(AstNode *program, AstNode *node, const char *var_name) {
    if (!node) return NULL;

    if (node->type == NODE_PROGRAM) {
        for (int i = 0; i < node->as.program.count; i++) {
            const char *c = find_ident_class_in_node(program, node->as.program.functions[i], var_name);
            if (c) return c;
        }
        for (int i = 0; i < node->as.program.class_count; i++) {
            AstNode *cls = node->as.program.classes[i];
            for (int m = 0; m < cls->as.class_decl.method_count; m++) {
                const char *c = find_ident_class_in_node(program, cls->as.class_decl.methods[m], var_name);
                if (c) return c;
            }
        }
        return NULL;
    }

    if (node->type == NODE_FUNCTION) {
        for (int p = 0; p < node->as.function.param_count; p++) {
            if (strcmp(node->as.function.param_names[p], var_name) == 0) {
                if (node->as.function.param_class_names && node->as.function.param_class_names[p]) {
                    return node->as.function.param_class_names[p];
                }
            }
        }
        return find_ident_class_in_node(program, node->as.function.body, var_name);
    }

    if (node->type == NODE_METHOD) {
        for (int p = 0; p < node->as.method.param_count; p++) {
            if (strcmp(node->as.method.param_names[p], var_name) == 0) {
                if (node->as.method.param_class_names && node->as.method.param_class_names[p]) {
                    return node->as.method.param_class_names[p];
                }
            }
        }
        return find_ident_class_in_node(program, node->as.method.body, var_name);
    }

    if (node->type == NODE_BLOCK) {
        for (int i = 0; i < node->as.block.count; i++) {
            const char *c = find_ident_class_in_node(program, node->as.block.stmts[i], var_name);
            if (c) return c;
        }
        return NULL;
    }

    if (node->type == NODE_LET) {
        if (strcmp(node->as.let.name, var_name) == 0) {
            if (node->as.let.class_name) return node->as.let.class_name;
            if (node->as.let.value && node->as.let.value->type == NODE_ALLOC) {
                return node->as.let.value->as.alloc.class_name;
            }
        }
        return NULL;
    }

    if (node->type == NODE_FOR_EACH) {
        if (strcmp(node->as.for_each.loop_var_name, var_name) == 0) {
            if (node->as.for_each.collection_expr && node->as.for_each.collection_expr->type == NODE_IDENT) {
                const char *arr_name = node->as.for_each.collection_expr->as.ident.name;
                const char *arr_cls = find_ident_class_in_node(program, program, arr_name);
                if (arr_cls) return arr_cls;
            }
        }
        return find_ident_class_in_node(program, node->as.for_each.body, var_name);
    }

    if (node->type == NODE_IF) {
        const char *c1 = find_ident_class_in_node(program, node->as.if_stmt.then_b, var_name);
        if (c1) return c1;
        if (node->as.if_stmt.else_b) {
            return find_ident_class_in_node(program, node->as.if_stmt.else_b, var_name);
        }
        return NULL;
    }

    if (node->type == NODE_WHILE) {
        return find_ident_class_in_node(program, node->as.while_stmt.body, var_name);
    }

    if (node->type == NODE_FOR) {
        const char *c_init = find_ident_class_in_node(program, node->as.for_stmt.init, var_name);
        if (c_init) return c_init;
        return find_ident_class_in_node(program, node->as.for_stmt.body, var_name);
    }

    if (node->type == NODE_MATCH) {
        for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
            AstNode *arm = node->as.match_stmt.arms[a];
            if (!arm->as.match_arm.is_wildcard && program && program->type == NODE_PROGRAM) {
                const char *ename = arm->as.match_arm.enum_name;
                const char *vname = arm->as.match_arm.variant_name;
                for (int e = 0; e < program->as.program.enum_count; e++) {
                    AstNode *en = program->as.program.enums[e];
                    if (strcmp(en->as.enum_decl.name, ename) == 0) {
                        for (int v = 0; v < en->as.enum_decl.variant_count; v++) {
                            AstNode *vn = en->as.enum_decl.variants[v];
                            if (strcmp(vn->as.variant_decl.name, vname) == 0) {
                                for (int b = 0; b < arm->as.match_arm.bind_count; b++) {
                                    if (strcmp(arm->as.match_arm.bind_names[b], var_name) == 0) {
                                        for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                                            if (strcmp(vn->as.variant_decl.fields[f]->as.field.name, var_name) == 0) {
                                                return vn->as.variant_decl.fields[f]->as.field.class_name;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            const char *c_arm = find_ident_class_in_node(program, arm->as.match_arm.body, var_name);
            if (c_arm) return c_arm;
        }
        return NULL;
    }

    return NULL;
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
        const char *found_cls = find_ident_class_in_node(ctx->program, body, var_name);
        if (found_cls) return found_cls;
    }
    if (expr->type == NODE_CALL && expr->as.call.arg_count > 0) {
        const char *name = expr->as.call.callee;
        if (strcmp(name, "pop") == 0 || strcmp(name, "push") == 0) {
            return get_expr_elem_class_name(ctx, expr->as.call.args[0]);
        }
    }
    if (expr->type == NODE_CALL && ctx->program && ctx->program->type == NODE_PROGRAM) {
        const char *name = expr->as.call.callee;
        if (name) {
            for (int f = 0; f < ctx->program->as.program.count; f++) {
                AstNode *target_fn = ctx->program->as.program.functions[f];
                if (strcmp(target_fn->as.function.name, name) == 0) {
                    return target_fn->as.function.return_class_name;
                }
            }
        }
    }
    if (expr->type == NODE_NEW) {
        return expr->as.new_expr.class_name;
    }
    if (expr->type == NODE_BINARY) {
        return get_expr_elem_class_name(ctx, expr->as.binary.left);
    }
    if (expr->type == NODE_UNARY) {
        return get_expr_elem_class_name(ctx, expr->as.unary.operand);
    }
    return NULL;
}

static void gen_expr(CodegenCtx *ctx, AstNode *expr);

static Type find_ident_type_in_node(AstNode *program, AstNode *node, const char *var_name) {
    if (!node) return (Type)-1;

    if (node->type == NODE_PROGRAM) {
        for (int i = 0; i < node->as.program.count; i++) {
            Type t = find_ident_type_in_node(program, node->as.program.functions[i], var_name);
            if (t != (Type)-1) return t;
        }
        for (int i = 0; i < node->as.program.class_count; i++) {
            AstNode *cls = node->as.program.classes[i];
            for (int m = 0; m < cls->as.class_decl.method_count; m++) {
                Type t = find_ident_type_in_node(program, cls->as.class_decl.methods[m], var_name);
                if (t != (Type)-1) return t;
            }
        }
        return (Type)-1;
    }

    if (node->type == NODE_FUNCTION) {
        return find_ident_type_in_node(program, node->as.function.body, var_name);
    }

    if (node->type == NODE_METHOD) {
        return find_ident_type_in_node(program, node->as.method.body, var_name);
    }

    if (node->type == NODE_BLOCK) {
        for (int i = 0; i < node->as.block.count; i++) {
            Type t = find_ident_type_in_node(program, node->as.block.stmts[i], var_name);
            if (t != (Type)-1) return t;
        }
        return (Type)-1;
    }

    if (node->type == NODE_LET) {
        if (strcmp(node->as.let.name, var_name) == 0) {
            return node->as.let.var_type;
        }
        return (Type)-1;
    }

    if (node->type == NODE_FOR_EACH) {
        if (strcmp(node->as.for_each.loop_var_name, var_name) == 0) {
            return infer_expr_type(program, NULL, node->as.for_each.collection_expr);
        }
        return find_ident_type_in_node(program, node->as.for_each.body, var_name);
    }

    if (node->type == NODE_IF) {
        Type t1 = find_ident_type_in_node(program, node->as.if_stmt.then_b, var_name);
        if (t1 != (Type)-1) return t1;
        if (node->as.if_stmt.else_b) {
            return find_ident_type_in_node(program, node->as.if_stmt.else_b, var_name);
        }
        return (Type)-1;
    }

    if (node->type == NODE_WHILE) {
        return find_ident_type_in_node(program, node->as.while_stmt.body, var_name);
    }

    if (node->type == NODE_FOR) {
        Type t_init = find_ident_type_in_node(program, node->as.for_stmt.init, var_name);
        if (t_init != (Type)-1) return t_init;
        return find_ident_type_in_node(program, node->as.for_stmt.body, var_name);
    }

    if (node->type == NODE_MATCH) {
        for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
            AstNode *arm = node->as.match_stmt.arms[a];
            if (!arm->as.match_arm.is_wildcard && program && program->type == NODE_PROGRAM) {
                const char *ename = arm->as.match_arm.enum_name;
                const char *vname = arm->as.match_arm.variant_name;
                for (int e = 0; e < program->as.program.enum_count; e++) {
                    AstNode *en = program->as.program.enums[e];
                    if (strcmp(en->as.enum_decl.name, ename) == 0) {
                        for (int v = 0; v < en->as.enum_decl.variant_count; v++) {
                            AstNode *vn = en->as.enum_decl.variants[v];
                            if (strcmp(vn->as.variant_decl.name, vname) == 0) {
                                for (int b = 0; b < arm->as.match_arm.bind_count; b++) {
                                    if (strcmp(arm->as.match_arm.bind_names[b], var_name) == 0) {
                                        for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                                            if (strcmp(vn->as.variant_decl.fields[f]->as.field.name, var_name) == 0) {
                                                return vn->as.variant_decl.fields[f]->as.field.type;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Type t_arm = find_ident_type_in_node(program, arm->as.match_arm.body, var_name);
            if (t_arm != (Type)-1) return t_arm;
        }
        return (Type)-1;
    }

    return (Type)-1;
}

static const char *get_expr_class_name(AstNode *program, AstNode *fn, AstNode *expr) {
    if (!expr) return NULL;
    if (expr->type == NODE_INDEX) {
        if (expr->as.index.array_expr) {
            return get_expr_class_name(program, fn, expr->as.index.array_expr);
        } else if (expr->as.index.array_name) {
            AstNode dummy;
            memset(&dummy, 0, sizeof(dummy));
            dummy.type = NODE_IDENT;
            dummy.as.ident.name = expr->as.index.array_name;
            return get_expr_class_name(program, fn, &dummy);
        }
    }
    if (expr->type == NODE_IDENT && fn) {
        const char *obj_name = expr->as.ident.name;
        if (fn->type == NODE_METHOD) {
            if (strcmp(obj_name, "self") == 0) {
                return fn->as.method.param_class_names ? fn->as.method.param_class_names[0] : NULL;
            }
            for (int p = 0; p < fn->as.method.param_count; p++) {
                if (strcmp(fn->as.method.param_names[p], obj_name) == 0) {
                    return fn->as.method.param_class_names ? fn->as.method.param_class_names[p] : NULL;
                }
            }
        } else if (fn->type == NODE_FUNCTION) {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (strcmp(fn->as.function.param_names[p], obj_name) == 0) {
                    return fn->as.function.param_class_names ? fn->as.function.param_class_names[p] : NULL;
                }
            }
        }
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : fn->as.method.body;
        return find_ident_class_in_node(program, body, obj_name);
    }
    if (expr->type == NODE_MEMBER && program && program->type == NODE_PROGRAM) {
        if (expr->as.member.field_class_name) return expr->as.member.field_class_name;
        const char *obj_cls = get_expr_class_name(program, fn, expr->as.member.object);
        if (obj_cls) {
            for (int c = 0; c < program->as.program.class_count; c++) {
                AstNode *cls = program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, obj_cls) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        if (strcmp(cls->as.class_decl.fields[f]->as.field.name, expr->as.member.member_name) == 0) {
                            return cls->as.class_decl.fields[f]->as.field.class_name;
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

static Type infer_expr_type(AstNode *program, AstNode *fn, AstNode *expr) {
    if (!expr) return TY_INT;

    if (expr->type == NODE_LITERAL) {
        return expr->as.literal.lit_type;
    }
    if (expr->type == NODE_FSTRING || expr->type == NODE_FSTRING_TEXT) {
        return TY_STRING;
    }
    if (expr->type == NODE_ALLOC) {
        return expr->as.alloc.elem_type;
    }
    if (expr->type == NODE_INDEX) {
        if (expr->as.index.array_expr) {
            return infer_expr_type(program, fn, expr->as.index.array_expr);
        } else if (expr->as.index.array_name) {
            AstNode dummy;
            memset(&dummy, 0, sizeof(dummy));
            dummy.type = NODE_IDENT;
            dummy.as.ident.name = expr->as.index.array_name;
            return infer_expr_type(program, fn, &dummy);
        }
    }
    if (expr->type == NODE_CALL) {
        const char *name = expr->as.call.callee;
        if (strcmp(name, "pop") == 0 && expr->as.call.arg_count > 0) {
            return infer_expr_type(program, fn, expr->as.call.args[0]);
        }
        if (strcmp(name, "push") == 0 && expr->as.call.arg_count > 0) {
            return infer_expr_type(program, fn, expr->as.call.args[0]);
        }
        if (strcmp(name, "len") == 0 || strcmp(name, "abs_int") == 0 || strcmp(name, "min_int") == 0 || strcmp(name, "max_int") == 0 || strcmp(name, "arg_count") == 0 || strcmp(name, "to_int") == 0 || strcmp(name, "random_int") == 0 || strcmp(name, "net_listen") == 0 || strcmp(name, "net_listen_on") == 0 || strcmp(name, "net_connect") == 0 || strcmp(name, "net_accept") == 0 || strcmp(name, "net_send") == 0 || strcmp(name, "net_send_bytes") == 0 || strcmp(name, "net_recv_bytes") == 0 || strcmp(name, "net_shutdown") == 0 || strcmp(name, "net_set_timeouts") == 0 || strcmp(name, "execute_cmd") == 0 || strcmp(name, "system") == 0) return TY_INT;
        if (strcmp(name, "sqrt") == 0 || strcmp(name, "exp") == 0 || strcmp(name, "log") == 0 || strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 || strcmp(name, "tan") == 0 || strcmp(name, "pow") == 0 || strcmp(name, "abs_float") == 0 || strcmp(name, "floor") == 0 || strcmp(name, "ceil") == 0 || strcmp(name, "min_float") == 0 || strcmp(name, "max_float") == 0 || strcmp(name, "to_float") == 0) return TY_FLOAT;
        if (strcmp(name, "concat") == 0 || strcmp(name, "substring") == 0 || strcmp(name, "char_to_string") == 0 || strcmp(name, "to_string") == 0 || strcmp(name, "read_file") == 0 || strcmp(name, "read_file_checked") == 0 || strcmp(name, "io_last_error") == 0 || strcmp(name, "net_last_error") == 0 || strcmp(name, "program_name") == 0 || strcmp(name, "args") == 0 || strcmp(name, "read_line") == 0 || strcmp(name, "input") == 0 || strcmp(name, "net_recv") == 0 || strcmp(name, "net_recv_raw") == 0 || strcmp(name, "net_recv_http") == 0) return TY_STRING;
        if (strcmp(name, "equals") == 0 || strcmp(name, "write_file") == 0 || strcmp(name, "is_int") == 0 || strcmp(name, "is_float") == 0 || strcmp(name, "file_exists") == 0 || strcmp(name, "write_bytes") == 0) return TY_BOOL;
        if (strcmp(name, "char_at") == 0) return TY_CHAR;
        if (strcmp(name, "random_seed") == 0 || strcmp(name, "net_close") == 0 || strcmp(name, "sleep_ms") == 0 || strcmp(name, "exit") == 0) return TY_VOID;

        if (program && program->type == NODE_PROGRAM) {
            for (int f = 0; f < program->as.program.count; f++) {
                AstNode *target_fn = program->as.program.functions[f];
                if (strcmp(target_fn->as.function.name, name) == 0) {
                    return target_fn->as.function.return_type;
                }
            }
        }
    }
    if (expr->type == NODE_METHOD_CALL && program && program->type == NODE_PROGRAM) {
        const char *mname = expr->as.method_call.method_name;
        for (int c = 0; c < program->as.program.class_count; c++) {
            AstNode *cls = program->as.program.classes[c];
            for (int m = 0; m < cls->as.class_decl.method_count; m++) {
                AstNode *mn = cls->as.class_decl.methods[m];
                if (strcmp(mn->as.method.name, mname) == 0) {
                    return mn->as.method.return_type;
                }
            }
        }
        for (int t = 0; t < program->as.program.interface_count; t++) {
            AstNode *tr = program->as.program.interfaces[t];
            if (tr && tr->type == NODE_INTERFACE) {
                for (int m = 0; m < tr->as.interface_decl.method_count; m++) {
                    AstNode *mn = tr->as.interface_decl.methods[m];
                    if (strcmp(mn->as.interface_method.name, mname) == 0) {
                        return mn->as.interface_method.return_type;
                    }
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && program && program->type == NODE_PROGRAM) {
        const char *mname = expr->as.member.member_name;
        const char *cname = expr->as.member.field_class_name;
        if (!cname) {
            cname = get_expr_class_name(program, fn, expr->as.member.object);
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
            for (int s = 0; s < program->as.program.struct_count; s++) {
                AstNode *st = program->as.program.structs[s];
                if (strcmp(st->as.struct_decl.name, cname) == 0) {
                    for (int f = 0; f < st->as.struct_decl.field_count; f++) {
                        if (strcmp(st->as.struct_decl.fields[f]->as.struct_field_decl.name, mname) == 0) {
                            return st->as.struct_decl.fields[f]->as.struct_field_decl.field_type;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_IDENT) {
        const char *var_name = expr->as.ident.name;
        if (fn) {
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
            Type found_t = find_ident_type_in_node(program, body, var_name);
            if (found_t != (Type)-1) return found_t;
        } else if (program) {
            Type found_t = find_ident_type_in_node(program, program, var_name);
            if (found_t != (Type)-1) return found_t;
        }
    }
    if (expr->type == NODE_NEW) {
        return TY_CLASS;
    }
    if (expr->type == NODE_BINARY) {
        if (strcmp(expr->as.binary.op, "==") == 0 || strcmp(expr->as.binary.op, "!=") == 0 ||
            strcmp(expr->as.binary.op, "<") == 0 || strcmp(expr->as.binary.op, ">") == 0 ||
            strcmp(expr->as.binary.op, "<=") == 0 || strcmp(expr->as.binary.op, ">=") == 0 ||
            strcmp(expr->as.binary.op, "&&") == 0 || strcmp(expr->as.binary.op, "||") == 0) {
            return TY_BOOL;
        }
        Type lt = infer_expr_type(program, fn, expr->as.binary.left);
        if (lt == TY_CLASS) return TY_CLASS;
        if (lt == TY_FLOAT || infer_expr_type(program, fn, expr->as.binary.right) == TY_FLOAT) return TY_FLOAT;
        return TY_INT;
    }
    if (expr->type == NODE_UNARY) {
        if (strcmp(expr->as.unary.op, "!") == 0) return TY_BOOL;
        return infer_expr_type(program, fn, expr->as.unary.operand);
    }

    return TY_INT;
}

static bool find_ident_is_array_in_node(AstNode *program, AstNode *node, const char *var_name) {
    if (!node) return false;
    if (node->type == NODE_BLOCK) {
        for (int i = 0; i < node->as.block.count; i++) {
            if (find_ident_is_array_in_node(program, node->as.block.stmts[i], var_name)) {
                return true;
            }
        }
        return false;
    }
    if (node->type == NODE_LET) {
        if (strcmp(node->as.let.name, var_name) == 0) {
            if (node->as.let.type_desc) {
                return node->as.let.type_desc->kind == TYPEDESC_ARRAY;
            }
            return node->as.let.is_array;
        }
        return false;
    }
    if (node->type == NODE_IF) {
        if (find_ident_is_array_in_node(program, node->as.if_stmt.then_b, var_name)) return true;
        if (node->as.if_stmt.else_b) {
            return find_ident_is_array_in_node(program, node->as.if_stmt.else_b, var_name);
        }
        return false;
    }
    if (node->type == NODE_WHILE) {
        return find_ident_is_array_in_node(program, node->as.while_stmt.body, var_name);
    }
    if (node->type == NODE_FOR) {
        if (find_ident_is_array_in_node(program, node->as.for_stmt.init, var_name)) return true;
        return find_ident_is_array_in_node(program, node->as.for_stmt.body, var_name);
    }
    if (node->type == NODE_FOR_EACH) {
        return find_ident_is_array_in_node(program, node->as.for_each.body, var_name);
    }
    if (node->type == NODE_MATCH) {
        for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
            if (find_ident_is_array_in_node(program, node->as.match_stmt.arms[a]->as.match_arm.body, var_name)) {
                return true;
            }
        }
        return false;
    }
    return false;
}

static bool infer_expr_is_array(AstNode *program, AstNode *fn, AstNode *expr) {
    if (!expr) return false;
    if (expr->type == NODE_ALLOC) return !expr->as.alloc.is_map;

    if (expr->type == NODE_IDENT && fn) {
        const char *var_name = expr->as.ident.name;
        if (fn->type == NODE_FUNCTION) {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (strcmp(fn->as.function.param_names[p], var_name) == 0) {
                    if (fn->as.function.param_type_descs && fn->as.function.param_type_descs[p]) {
                        return fn->as.function.param_type_descs[p]->kind == TYPEDESC_ARRAY;
                    }
                    return fn->as.function.param_is_array ? fn->as.function.param_is_array[p] : false;
                }
            }
        } else if (fn->type == NODE_METHOD) {
            for (int p = 0; p < fn->as.method.param_count; p++) {
                if (strcmp(fn->as.method.param_names[p], var_name) == 0) {
                    if (fn->as.method.param_type_descs && fn->as.method.param_type_descs[p]) {
                        return fn->as.method.param_type_descs[p]->kind == TYPEDESC_ARRAY;
                    }
                    return fn->as.method.param_is_array ? fn->as.method.param_is_array[p] : false;
                }
            }
        }
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : fn->as.method.body;
        return find_ident_is_array_in_node(program, body, var_name);
    }
    if (expr->type == NODE_CALL) {
        const char *name = expr->as.call.callee;
        if (strcmp(name, "push") == 0 || strcmp(name, "keys") == 0 || strcmp(name, "args") == 0) return true;
        if (program && program->type == NODE_PROGRAM) {
            for (int f = 0; f < program->as.program.count; f++) {
                AstNode *target_fn = program->as.program.functions[f];
                if (strcmp(target_fn->as.function.name, name) == 0) {
                    return target_fn->as.function.return_is_array;
                }
            }
        }
    }
    if (expr->type == NODE_METHOD_CALL && program && program->type == NODE_PROGRAM) {
        const char *mname = expr->as.method_call.method_name;
        const char *cname = expr->as.method_call.target_class_name;
        if (cname) {
            for (int c = 0; c < program->as.program.class_count; c++) {
                AstNode *cls = program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, cname) == 0) {
                    for (int m = 0; m < cls->as.class_decl.method_count; m++) {
                        AstNode *mn = cls->as.class_decl.methods[m];
                        if (strcmp(mn->as.method.name, mname) == 0) {
                            return mn->as.method.return_is_array;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && program && program->type == NODE_PROGRAM) {
        const char *mname = expr->as.member.member_name;
        const char *cname = expr->as.member.field_class_name;
        if (!cname) {
            cname = get_expr_class_name(program, fn, expr->as.member.object);
        }
        if (cname) {
            for (int c = 0; c < program->as.program.class_count; c++) {
                AstNode *cls = program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, cname) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        if (strcmp(cls->as.class_decl.fields[f]->as.field.name, mname) == 0) {
                            return cls->as.class_decl.fields[f]->as.field.is_array;
                        }
                    }
                }
            }
            for (int s = 0; s < program->as.program.struct_count; s++) {
                AstNode *st = program->as.program.structs[s];
                if (strcmp(st->as.struct_decl.name, cname) == 0) {
                    for (int f = 0; f < st->as.struct_decl.field_count; f++) {
                        if (strcmp(st->as.struct_decl.fields[f]->as.struct_field_decl.name, mname) == 0) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    return false;
}

static bool infer_expr_is_map(AstNode *program, AstNode *fn, AstNode *expr) {
    if (!expr) return false;
    if (expr->type == NODE_ALLOC && expr->as.alloc.is_map) return true;

    if (expr->type == NODE_IDENT && fn) {
        const char *var_name = expr->as.ident.name;
        if (fn->type == NODE_FUNCTION) {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (strcmp(fn->as.function.param_names[p], var_name) == 0) {
                    if (fn->as.function.param_type_descs && fn->as.function.param_type_descs[p]) {
                        return fn->as.function.param_type_descs[p]->kind == TYPEDESC_MAP;
                    }
                    return fn->as.function.param_is_map ? fn->as.function.param_is_map[p] : false;
                }
            }
        } else if (fn->type == NODE_METHOD) {
            for (int p = 0; p < fn->as.method.param_count; p++) {
                if (strcmp(fn->as.method.param_names[p], var_name) == 0) {
                    if (fn->as.method.param_type_descs && fn->as.method.param_type_descs[p]) {
                        return fn->as.method.param_type_descs[p]->kind == TYPEDESC_MAP;
                    }
                    return fn->as.method.param_is_map ? fn->as.method.param_is_map[p] : false;
                }
            }
        }
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : (fn->type == NODE_METHOD ? fn->as.method.body : NULL);
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    if (stmt->as.let.type_desc) {
                        return stmt->as.let.type_desc->kind == TYPEDESC_MAP;
                    }
                    return stmt->as.let.is_map;
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && program && program->type == NODE_PROGRAM) {
        const char *mname = expr->as.member.member_name;
        const char *cname = expr->as.member.field_class_name;
        if (!cname) {
            cname = get_expr_class_name(program, fn, expr->as.member.object);
        }
        if (cname) {
            for (int c = 0; c < program->as.program.class_count; c++) {
                AstNode *cls = program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, cname) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        if (strcmp(cls->as.class_decl.fields[f]->as.field.name, mname) == 0) {
                            return cls->as.class_decl.fields[f]->as.field.is_map;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_CALL && program && program->type == NODE_PROGRAM) {
        const char *name = expr->as.call.callee;
        if (name && strcmp(name, "put") == 0) return true;
        if (name) {
            for (int f = 0; f < program->as.program.count; f++) {
                AstNode *target_fn = program->as.program.functions[f];
                if (strcmp(target_fn->as.function.name, name) == 0) {
                    return target_fn->as.function.return_is_map;
                }
            }
        }
    }
    return false;
}

static Type get_map_key_type(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return TY_INT;
    TypeDesc *td = get_expr_type_desc(ctx, expr);
    if (td && td->kind == TYPEDESC_MAP && td->as.map_desc.key_type) {
        if (td->as.map_desc.key_type->kind == TYPEDESC_PRIMITIVE && td->as.map_desc.key_type->as.primitive_desc.primitive_kind == TY_STRING) {
            return TY_STRING;
        }
        return TY_INT;
    }
    if (expr->type == NODE_ALLOC && expr->as.alloc.is_map) {
        return expr->as.alloc.key_type;
    }
    if (expr->type == NODE_IDENT && ctx->current_function) {
        const char *var_name = expr->as.ident.name;
        AstNode *fn = ctx->current_function;
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : (fn->type == NODE_METHOD ? fn->as.method.body : NULL);
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.key_type;
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && ctx->program && ctx->program->type == NODE_PROGRAM) {
        const char *obj_cls = get_expr_elem_class_name(ctx, expr->as.member.object);
        if (obj_cls) {
            for (int c = 0; c < ctx->program->as.program.class_count; c++) {
                AstNode *cls = ctx->program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, obj_cls) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        AstNode *fld = cls->as.class_decl.fields[f];
                        if (strcmp(fld->as.field.name, expr->as.member.member_name) == 0) {
                            return fld->as.field.key_type;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_CALL && expr->as.call.callee && strcmp(expr->as.call.callee, "put") == 0) {
        return get_map_key_type(ctx, expr->as.call.args[0]);
    }
    if (expr->type == NODE_CALL && ctx->program && ctx->program->type == NODE_PROGRAM) {
        const char *name = expr->as.call.callee;
        if (name) {
            for (int f = 0; f < ctx->program->as.program.count; f++) {
                AstNode *target_fn = ctx->program->as.program.functions[f];
                if (strcmp(target_fn->as.function.name, name) == 0) {
                    return target_fn->as.function.return_key_type;
                }
            }
        }
    }
    return TY_INT;
}

static Type get_map_val_type(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return TY_INT;
    TypeDesc *td = get_expr_type_desc(ctx, expr);
    if (td && td->kind == TYPEDESC_MAP && td->as.map_desc.val_type) {
        TypeDesc *v = td->as.map_desc.val_type;
        if (v->kind == TYPEDESC_PRIMITIVE) return (Type)v->as.primitive_desc.primitive_kind;
        if (v->kind == TYPEDESC_CLASS) return TY_CLASS;
        if (v->kind == TYPEDESC_ENUM) return TY_ENUM;
        if (v->kind == TYPEDESC_MAP) return TY_MAP;
        if (v->kind == TYPEDESC_ARRAY) return TY_CLASS;
    }
    if (expr->type == NODE_ALLOC && expr->as.alloc.is_map) {
        return expr->as.alloc.elem_type;
    }
    if (expr->type == NODE_IDENT && ctx->current_function) {
        const char *var_name = expr->as.ident.name;
        AstNode *fn = ctx->current_function;
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : (fn->type == NODE_METHOD ? fn->as.method.body : NULL);
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.var_type;
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && ctx->program && ctx->program->type == NODE_PROGRAM) {
        const char *obj_cls = get_expr_elem_class_name(ctx, expr->as.member.object);
        if (obj_cls) {
            for (int c = 0; c < ctx->program->as.program.class_count; c++) {
                AstNode *cls = ctx->program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, obj_cls) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        AstNode *fld = cls->as.class_decl.fields[f];
                        if (strcmp(fld->as.field.name, expr->as.member.member_name) == 0) {
                            return fld->as.field.type;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_CALL && expr->as.call.callee && strcmp(expr->as.call.callee, "put") == 0) {
        return get_map_val_type(ctx, expr->as.call.args[0]);
    }
    return TY_INT;
}

static bool get_map_val_is_array(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return false;
    TypeDesc *td = get_expr_type_desc(ctx, expr);
    if (td && td->kind == TYPEDESC_MAP && td->as.map_desc.val_type) {
        return td->as.map_desc.val_type->kind == TYPEDESC_ARRAY;
    }
    if (expr->type == NODE_ALLOC) {
        return false;
    }
    if (expr->type == NODE_IDENT && ctx->current_function) {
        const char *var_name = expr->as.ident.name;
        AstNode *fn = ctx->current_function;
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : (fn->type == NODE_METHOD ? fn->as.method.body : NULL);
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.is_array;
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && ctx->program && ctx->program->type == NODE_PROGRAM) {
        const char *obj_cls = get_expr_elem_class_name(ctx, expr->as.member.object);
        if (obj_cls) {
            for (int c = 0; c < ctx->program->as.program.class_count; c++) {
                AstNode *cls = ctx->program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, obj_cls) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        AstNode *fld = cls->as.class_decl.fields[f];
                        if (strcmp(fld->as.field.name, expr->as.member.member_name) == 0) {
                            return fld->as.field.is_array;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_CALL && expr->as.call.callee && strcmp(expr->as.call.callee, "put") == 0) {
        return get_map_val_is_array(ctx, expr->as.call.args[0]);
    }
    return false;
}

static const char *get_map_val_class_name(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return NULL;
    TypeDesc *td = get_expr_type_desc(ctx, expr);
    if (td && td->kind == TYPEDESC_MAP && td->as.map_desc.val_type) {
        TypeDesc *v = td->as.map_desc.val_type;
        if (v->kind == TYPEDESC_CLASS || v->kind == TYPEDESC_ENUM || v->kind == TYPEDESC_NAMED) {
            return v->as.named_desc.name;
        }
        if (v->kind == TYPEDESC_ARRAY) {
            TypeDesc *elem = v->as.array_desc.elem_type;
            if (elem && (elem->kind == TYPEDESC_CLASS || elem->kind == TYPEDESC_ENUM || elem->kind == TYPEDESC_NAMED)) {
                return elem->as.named_desc.name;
            }
            if (elem && elem->kind == TYPEDESC_PRIMITIVE && elem->as.primitive_desc.primitive_kind == TY_STRING) {
                return "string";
            }
        }
    }
    if (expr->type == NODE_ALLOC && expr->as.alloc.is_map) {
        return expr->as.alloc.class_name;
    }
    if (expr->type == NODE_IDENT && ctx->current_function) {
        const char *var_name = expr->as.ident.name;
        AstNode *fn = ctx->current_function;
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : (fn->type == NODE_METHOD ? fn->as.method.body : NULL);
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.class_name;
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && ctx->program && ctx->program->type == NODE_PROGRAM) {
        const char *obj_cls = get_expr_elem_class_name(ctx, expr->as.member.object);
        if (obj_cls) {
            for (int c = 0; c < ctx->program->as.program.class_count; c++) {
                AstNode *cls = ctx->program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, obj_cls) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        AstNode *fld = cls->as.class_decl.fields[f];
                        if (strcmp(fld->as.field.name, expr->as.member.member_name) == 0) {
                            return fld->as.field.class_name;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_CALL && expr->as.call.callee && strcmp(expr->as.call.callee, "put") == 0) {
        return get_map_val_class_name(ctx, expr->as.call.args[0]);
    }
    return NULL;
}

static void gen_stmt(CodegenCtx *ctx, AstNode *stmt);
static void gen_block(CodegenCtx *ctx, AstNode *block_node);

static void emit_frees(CodegenCtx *ctx, AstNode *node) {
    if (!node || node->frees_count == 0) return;
    for (int i = 0; i < node->frees_count; i++) {
        emit_indent(ctx);
        if (node->frees_to_emit[i].is_array) {
            sb_appendf(&ctx->sb, "__cco_free_arr(%s);\n", node->frees_to_emit[i].var_name);
        } else {
            sb_appendf(&ctx->sb, "free(%s);\n", node->frees_to_emit[i].var_name);
        }
    }
}

static TypeDesc *get_expr_type_desc(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return NULL;
    if (expr->type == NODE_ALLOC && expr->as.alloc.type_desc) {
        return expr->as.alloc.type_desc;
    }
    if (expr->type == NODE_IDENT && ctx->current_function) {
        const char *var_name = expr->as.ident.name;
        AstNode *fn = ctx->current_function;
        if (fn->type == NODE_FUNCTION) {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (strcmp(fn->as.function.param_names[p], var_name) == 0) {
                    if (fn->as.function.param_type_descs) return fn->as.function.param_type_descs[p];
                    break;
                }
            }
        } else if (fn->type == NODE_METHOD) {
            for (int p = 0; p < fn->as.method.param_count; p++) {
                if (strcmp(fn->as.method.param_names[p], var_name) == 0) {
                    if (fn->as.method.param_type_descs) return fn->as.method.param_type_descs[p];
                    break;
                }
            }
        }
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : (fn->type == NODE_METHOD ? fn->as.method.body : NULL);
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.type_desc;
                }
            }
        }
    }
    if (expr->type == NODE_MEMBER && ctx->program && ctx->program->type == NODE_PROGRAM) {
        TypeDesc *obj_td = get_expr_type_desc(ctx, expr->as.member.object);
        const char *obj_cls = expr->as.member.field_class_name;
        if (!obj_cls && obj_td && (obj_td->kind == TYPEDESC_CLASS || obj_td->kind == TYPEDESC_NAMED || obj_td->kind == TYPEDESC_ENUM)) {
            obj_cls = obj_td->as.named_desc.name;
        }
        if (!obj_cls) {
            obj_cls = get_expr_elem_class_name(ctx, expr->as.member.object);
        }
        if (obj_cls) {
            for (int c = 0; c < ctx->program->as.program.class_count; c++) {
                AstNode *cls = ctx->program->as.program.classes[c];
                if (strcmp(cls->as.class_decl.name, obj_cls) == 0) {
                    for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                        AstNode *fld = cls->as.class_decl.fields[f];
                        if (strcmp(fld->as.field.name, expr->as.member.member_name) == 0) {
                            return fld->as.field.type_desc;
                        }
                    }
                }
            }
        }
    }
    if (expr->type == NODE_CALL) {
        const char *callee = expr->as.call.callee;
        if (callee) {
            if ((strcmp(callee, "put") == 0 || strcmp(callee, "push") == 0) && expr->as.call.arg_count > 0) {
                return get_expr_type_desc(ctx, expr->as.call.args[0]);
            }
            if (strcmp(callee, "get") == 0 && expr->as.call.arg_count == 2) {
                TypeDesc *mtd = get_expr_type_desc(ctx, expr->as.call.args[0]);
                if (mtd && mtd->kind == TYPEDESC_MAP) {
                    return mtd->as.map_desc.val_type;
                }
            }
            if (strcmp(callee, "pop") == 0 && expr->as.call.arg_count == 1) {
                TypeDesc *atd = get_expr_type_desc(ctx, expr->as.call.args[0]);
                if (atd && atd->kind == TYPEDESC_ARRAY) {
                    return atd->as.array_desc.elem_type;
                }
            }
            if (strcmp(callee, "remove") == 0 && expr->as.call.arg_count == 2) {
                TypeDesc *mtd = get_expr_type_desc(ctx, expr->as.call.args[0]);
                if (mtd && mtd->kind == TYPEDESC_MAP) {
                    return mtd->as.map_desc.val_type;
                }
            }
            if (ctx->program && ctx->program->type == NODE_PROGRAM) {
                for (int f = 0; f < ctx->program->as.program.count; f++) {
                    AstNode *target_fn = ctx->program->as.program.functions[f];
                    if (strcmp(target_fn->as.function.name, callee) == 0) {
                        return target_fn->as.function.return_type_desc;
                    }
                }
            }
        }
    }
    if (expr->type == NODE_INDEX) {
        TypeDesc *atd = get_expr_type_desc(ctx, expr->as.index.array_expr ? expr->as.index.array_expr : expr);
        if (atd && atd->kind == TYPEDESC_ARRAY) {
            return atd->as.array_desc.elem_type;
        }
    }
    return NULL;
}

static void emit_typedesc_cleanup(CodegenCtx *ctx, const char *vname, const TypeDesc *td) {
    if (!vname || !td) return;

    if (td->kind == TYPEDESC_MAP) {
        TypeDesc *v = td->as.map_desc.val_type;
        if (!v) {
            sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, NULL);\n", vname, vname);
            return;
        }
        if (v->kind == TYPEDESC_PRIMITIVE) {
            if (v->as.primitive_desc.primitive_kind == TY_STRING) {
                sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, (__cco_val_free_fn)free);\n", vname, vname);
            } else {
                sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, NULL);\n", vname, vname);
            }
        } else if (v->kind == TYPEDESC_CLASS || v->kind == TYPEDESC_ENUM || v->kind == TYPEDESC_NAMED) {
            sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, (__cco_val_free_fn)%s_free);\n", vname, vname, v->as.named_desc.name);
        } else if (v->kind == TYPEDESC_ARRAY) {
            TypeDesc *elem = v->as.array_desc.elem_type;
            if (elem && (elem->kind == TYPEDESC_CLASS || elem->kind == TYPEDESC_ENUM || elem->kind == TYPEDESC_NAMED)) {
                sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, (__cco_val_free_fn)__cco_free_arr_of_%s);\n", vname, vname, elem->as.named_desc.name);
            } else if (elem && elem->kind == TYPEDESC_PRIMITIVE && elem->as.primitive_desc.primitive_kind == TY_STRING) {
                sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, (__cco_val_free_fn)__cco_free_arr_of_string);\n", vname, vname);
            } else if (elem && elem->kind == TYPEDESC_MAP) {
                TypeDesc *ik = elem->as.map_desc.key_type;
                TypeDesc *iv = elem->as.map_desc.val_type;
                const char *ks = (ik && ik->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int";
                const char *vs = (iv && iv->kind != TYPEDESC_PRIMITIVE) ? iv->as.named_desc.name : "int";
                sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, (__cco_val_free_fn)__cco_free_arr_of_map_%s_%s);\n", vname, vname, ks, vs);
            } else {
                sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, (__cco_val_free_fn)__cco_free_arr);\n", vname, vname);
            }
        } else if (v->kind == TYPEDESC_MAP) {
            TypeDesc *ik = v->as.map_desc.key_type;
            TypeDesc *iv = v->as.map_desc.val_type;
            const char *ks = (ik && ik->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int";
            const char *vs = (iv && iv->kind != TYPEDESC_PRIMITIVE) ? iv->as.named_desc.name : "int";
            sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, (__cco_val_free_fn)__cco_free_map_%s_%s);\n", vname, vname, ks, vs);
        } else {
            sb_appendf(&ctx->sb, "if (%s) __cco_map_free(%s, NULL);\n", vname, vname);
        }
        return;
    }

    if (td->kind == TYPEDESC_ARRAY) {
        TypeDesc *elem = td->as.array_desc.elem_type;
        if (!elem) {
            sb_appendf(&ctx->sb, "if (%s) __cco_free_arr(%s);\n", vname, vname);
            return;
        }
        if (elem->kind == TYPEDESC_PRIMITIVE) {
            if (elem->as.primitive_desc.primitive_kind == TY_STRING) {
                sb_appendf(&ctx->sb, "if (%s != NULL) {\n", vname);
                ctx->indent_level++;
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(%s); __i++) {\n", vname);
                ctx->indent_level++;
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) free(%s[__i]);\n", vname, vname);
                ctx->indent_level--;
                emit_indent(ctx);
                sb_append(&ctx->sb, "}\n");
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "__cco_free_arr(%s);\n", vname);
                ctx->indent_level--;
                emit_indent(ctx);
                sb_append(&ctx->sb, "}\n");
            } else {
                sb_appendf(&ctx->sb, "if (%s) __cco_free_arr(%s);\n", vname, vname);
            }
        } else if (elem->kind == TYPEDESC_CLASS || elem->kind == TYPEDESC_ENUM || elem->kind == TYPEDESC_NAMED) {
            const char *cname = elem->as.named_desc.name;
            sb_appendf(&ctx->sb, "if (%s != NULL) {\n", vname);
            ctx->indent_level++;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(%s); __i++) {\n", vname);
            ctx->indent_level++;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "if (%s[__i] != NULL) %s_free(%s[__i]);\n", vname, cname, vname);
            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "__cco_free_arr(%s);\n", vname);
            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");
        } else if (elem->kind == TYPEDESC_MAP) {
            TypeDesc *iv = elem->as.map_desc.val_type;
            const char *val_free = (iv && iv->kind != TYPEDESC_PRIMITIVE) ? iv->as.named_desc.name : NULL;
            sb_appendf(&ctx->sb, "if (%s != NULL) {\n", vname);
            ctx->indent_level++;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(%s); __i++) {\n", vname);
            ctx->indent_level++;
            emit_indent(ctx);
            if (val_free) {
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) __cco_map_free(%s[__i], (__cco_val_free_fn)%s_free);\n", vname, vname, val_free);
            } else if (iv && iv->kind == TYPEDESC_PRIMITIVE && iv->as.primitive_desc.primitive_kind == TY_STRING) {
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) __cco_map_free(%s[__i], (__cco_val_free_fn)free);\n", vname, vname);
            } else {
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) __cco_map_free(%s[__i], NULL);\n", vname, vname);
            }
            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "__cco_free_arr(%s);\n", vname);
            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");
        } else if (elem->kind == TYPEDESC_ARRAY) {
            TypeDesc *sub = elem->as.array_desc.elem_type;
            sb_appendf(&ctx->sb, "if (%s != NULL) {\n", vname);
            ctx->indent_level++;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(%s); __i++) {\n", vname);
            ctx->indent_level++;
            emit_indent(ctx);
            if (sub && (sub->kind == TYPEDESC_CLASS || sub->kind == TYPEDESC_ENUM || sub->kind == TYPEDESC_NAMED)) {
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) __cco_free_arr_of_%s(%s[__i]);\n", vname, sub->as.named_desc.name, vname);
            } else {
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) __cco_free_arr(%s[__i]);\n", vname, vname);
            }
            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "__cco_free_arr(%s);\n", vname);
            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");
        } else {
            sb_appendf(&ctx->sb, "if (%s) __cco_free_arr(%s);\n", vname, vname);
        }
        return;
    }

    if (td->kind == TYPEDESC_CLASS || td->kind == TYPEDESC_ENUM || td->kind == TYPEDESC_NAMED) {
        if (!is_struct_name(ctx, td->as.named_desc.name)) {
            sb_appendf(&ctx->sb, "%s_free(%s);\n", td->as.named_desc.name, vname);
        }
        return;
    }

    if (td->kind == TYPEDESC_PRIMITIVE && td->as.primitive_desc.primitive_kind == TY_STRING) {
        sb_appendf(&ctx->sb, "free(%s);\n", vname);
        return;
    }
}

static void emit_releases(CodegenCtx *ctx, AstNode *node) {
    if (!node || node->releases_count == 0) return;
    for (int i = 0; i < node->releases_count; i++) {
        emit_indent(ctx);
        if (node->releases_to_emit[i].type_desc) {
            emit_typedesc_cleanup(ctx, node->releases_to_emit[i].var_name, node->releases_to_emit[i].type_desc);
            continue;
        }
        if (node->releases_to_emit[i].is_map) {
            const char *mname = node->releases_to_emit[i].var_name;
            const char *cls = node->releases_to_emit[i].class_name;
            if (cls && (find_class(ctx->ct, cls) != NULL || find_enum(ctx->ct, cls) != NULL)) {
                sb_appendf(&ctx->sb, "__cco_map_free(%s, (__cco_val_free_fn)%s_free);\n", mname, cls);
            } else {
                sb_appendf(&ctx->sb, "__cco_map_free(%s, NULL);\n", mname);
            }
        } else if (node->releases_to_emit[i].is_array) {
            const char *arr = node->releases_to_emit[i].var_name;
            const char *cls = node->releases_to_emit[i].class_name;
            sb_appendf(&ctx->sb, "if (%s != NULL) {\n", arr);
            ctx->indent_level++;
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(%s); __i++) {\n", arr);
            ctx->indent_level++;
            emit_indent(ctx);
            if (cls && strcmp(cls, "string") == 0) {
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) { free(%s[__i]); }\n", arr, arr);
            } else if (cls && (find_class(ctx->ct, cls) != NULL || find_enum(ctx->ct, cls) != NULL)) {
                sb_appendf(&ctx->sb, "if (%s[__i] != NULL) { %s_free(%s[__i]); }\n", arr, cls, arr);
            }
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

static void emit_nulls(CodegenCtx *ctx, AstNode *stmt) {
    if (!stmt || stmt->nulls_count == 0) return;
    for (int i = 0; i < stmt->nulls_count; i++) {
        emit_indent(ctx);
        sb_appendf(&ctx->sb, "%s = NULL;\n", stmt->nulls_to_emit[i]);
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

static void sb_append_escaped_string(StringBuffer *sb, const char *s) {
    sb_append(sb, "\"");
    if (s) {
        sb_append(sb, s);
    }
    sb_append(sb, "\"");
}

static void gen_fstring_part(CodegenCtx *ctx, AstNode *part, bool *out_needs_free);
static void gen_expr(CodegenCtx *ctx, AstNode *expr);

static bool is_pure_lvalue(const AstNode *node) {
    if (!node) return false;
    if (node->type == NODE_IDENT) {
        return true;
    }
    if (node->type == NODE_MEMBER) {
        return is_pure_lvalue(node->as.member.object);
    }
    if (node->type == NODE_INDEX) {
        AstNode *arr = node->as.index.array_expr;
        AstNode *idx = node->as.index.index;
        if (!arr || !is_pure_lvalue(arr)) return false;
        if (!idx) return true;
        if (idx->type == NODE_IDENT || idx->type == NODE_LITERAL) return true;
        return false;
    }
    return false;
}

static bool is_owned_temporary_string_expr(const AstNode *expr) {
    if (!expr) return false;
    if (expr->type == NODE_CALL) {
        if (expr->as.call.callee && strcmp(expr->as.call.callee, "get") == 0) {
            return false;
        }
        return true;
    }
    if (expr->type == NODE_METHOD_CALL) {
        return true;
    }
    if (expr->type == NODE_FSTRING) {
        return true;
    }
    return false;
}

static void gen_expr(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return;

    switch (expr->type) {
        case NODE_FSTRING_TEXT:
            sb_append(&ctx->sb, "strdup(");
            sb_append_escaped_string(&ctx->sb, expr->as.fstring_text.text);
            sb_append(&ctx->sb, ")");
            break;

        case NODE_FSTRING: {
            if (expr->as.fstring.part_count == 1) {
                AstNode *p0 = expr->as.fstring.parts[0];
                if (p0->type == NODE_FSTRING_TEXT) {
                    sb_append(&ctx->sb, "strdup(");
                    sb_append_escaped_string(&ctx->sb, p0->as.fstring_text.text);
                    sb_append(&ctx->sb, ")");
                } else {
                    bool needs_free = false;
                    Type t = infer_expr_type(ctx->program, ctx->current_function, p0);
                    if (t == TY_STRING && p0->type != NODE_LITERAL) {
                        sb_append(&ctx->sb, "strdup(");
                        gen_expr(ctx, p0);
                        sb_append(&ctx->sb, ")");
                    } else {
                        gen_fstring_part(ctx, p0, &needs_free);
                    }
                }
            } else {
                for (int i = 0; i < expr->as.fstring.part_count - 1; i++) {
                    sb_append(&ctx->sb, "__cco_concat_free(");
                }
                bool free0 = false;
                gen_fstring_part(ctx, expr->as.fstring.parts[0], &free0);
                sb_append(&ctx->sb, ", ");
                bool free1 = false;
                gen_fstring_part(ctx, expr->as.fstring.parts[1], &free1);
                sb_appendf(&ctx->sb, ", %s, %s)", free0 ? "true" : "false", free1 ? "true" : "false");

                for (int i = 2; i < expr->as.fstring.part_count; i++) {
                    sb_append(&ctx->sb, ", ");
                    bool free_i = false;
                    gen_fstring_part(ctx, expr->as.fstring.parts[i], &free_i);
                    sb_appendf(&ctx->sb, ", true, %s)", free_i ? "true" : "false");
                }
            }
            break;
        }

        case NODE_LITERAL:
            if (expr->as.literal.lit_type == TY_INT) {
                sb_appendf(&ctx->sb, "%ld", expr->as.literal.val.i);
            } else if (expr->as.literal.lit_type == TY_FLOAT) {
                char fbuf[64];
                snprintf(fbuf, sizeof(fbuf), "%g", expr->as.literal.val.f);
                if (!strchr(fbuf, '.') && !strchr(fbuf, 'e') && !strchr(fbuf, 'E')) {
                    sb_appendf(&ctx->sb, "%s.0", fbuf);
                } else {
                    sb_append(&ctx->sb, fbuf);
                }
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
            if (expr->as.alloc.is_map) {
                int kt = (expr->as.alloc.key_type == TY_STRING) ? 1 : 0;
                if (expr->as.alloc.type_desc && expr->as.alloc.type_desc->kind == TYPEDESC_MAP) {
                    TypeDesc *ktd = expr->as.alloc.type_desc->as.map_desc.key_type;
                    if (ktd && ktd->kind == TYPEDESC_PRIMITIVE && ktd->as.primitive_desc.primitive_kind == TY_STRING) {
                        kt = 1;
                    } else {
                        kt = 0;
                    }
                }
                sb_appendf(&ctx->sb, "__cco_map_new(%d)", kt);
            } else if (expr->as.alloc.is_list) {
                if (expr->as.alloc.type_desc && expr->as.alloc.type_desc->kind == TYPEDESC_ARRAY) {
                    TypeDesc *elem_td = expr->as.alloc.type_desc->as.array_desc.elem_type;
                    const char *elem_str = c_type_str_desc(ctx, elem_td);
                    sb_appendf(&ctx->sb, "(%s *)__cco_list_new(sizeof(%s))", elem_str, elem_str);
                } else if (expr->as.alloc.elem_type == TY_CLASS && !is_struct_name(ctx, expr->as.alloc.class_name)) {
                    sb_appendf(&ctx->sb, "(%s **)__cco_list_new(sizeof(%s *))", expr->as.alloc.class_name, expr->as.alloc.class_name);
                } else {
                    const char *elem_str = c_type_str_full(ctx, expr->as.alloc.elem_type, expr->as.alloc.class_name, false, false);
                    sb_appendf(&ctx->sb, "(%s *)__cco_list_new(sizeof(%s))", elem_str, elem_str);
                }
            } else {
                if (expr->as.alloc.type_desc && expr->as.alloc.type_desc->kind == TYPEDESC_ARRAY) {
                    TypeDesc *elem_td = expr->as.alloc.type_desc->as.array_desc.elem_type;
                    const char *elem_str = c_type_str_desc(ctx, elem_td);
                    sb_appendf(&ctx->sb, "(%s *)__cco_alloc_arr(sizeof(%s), ", elem_str, elem_str);
                    gen_expr(ctx, expr->as.alloc.count_expr);
                    sb_append(&ctx->sb, ")");
                } else if (expr->as.alloc.elem_type == TY_CLASS && !is_struct_name(ctx, expr->as.alloc.class_name)) {
                    sb_appendf(&ctx->sb, "(%s **)__cco_alloc_arr(sizeof(%s *), ", expr->as.alloc.class_name, expr->as.alloc.class_name);
                    gen_expr(ctx, expr->as.alloc.count_expr);
                    sb_append(&ctx->sb, ")");
                } else {
                    const char *elem_str = c_type_str_full(ctx, expr->as.alloc.elem_type, expr->as.alloc.class_name, false, false);
                    sb_appendf(&ctx->sb, "(%s *)__cco_alloc_arr(sizeof(%s), ", elem_str, elem_str);
                    gen_expr(ctx, expr->as.alloc.count_expr);
                    sb_append(&ctx->sb, ")");
                }
            }
            break;

        case NODE_NEW: {
            const char *cname = expr->as.new_expr.class_name;
            if (expr->as.new_expr.constructs_enum || find_enum(ctx->ct, cname) != NULL) {
                const char *vname = expr->as.new_expr.variant_name;
                EnumDef *edef = find_enum(ctx->ct, cname);
                EnumVariantDef *vdef = edef ? find_enum_variant(edef, vname) : NULL;
                sb_appendf(&ctx->sb, "%s_new_%s(", cname, vname);
                if (vdef && vdef->field_count > 0) {
                    for (int f = 0; f < vdef->field_count; f++) {
                        if (f > 0) sb_append(&ctx->sb, ", ");
                        const char *target_fname = vdef->fields[f].name;
                        AstNode *fval = NULL;
                        for (int k = 0; k < expr->as.new_expr.field_count; k++) {
                            if (strcmp(expr->as.new_expr.field_names[k], target_fname) == 0) {
                                fval = expr->as.new_expr.field_values[k];
                                break;
                            }
                        }
                        bool is_str = (vdef && vdef->fields[f].type == TY_STRING && !vdef->fields[f].is_array && !vdef->fields[f].is_map);
                        if (fval) {
                            gen_expr(ctx, fval);
                            if (is_str) {
                                sb_appendf(&ctx->sb, ", %s", is_owned_temporary_string_expr(fval) ? "1" : "0");
                            }
                        } else {
                            sb_append(&ctx->sb, "0");
                            if (is_str) {
                                sb_append(&ctx->sb, ", 0");
                            }
                        }
                    }
                }
                sb_append(&ctx->sb, ")");
            } else if (is_struct_name(ctx, cname)) {
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
                        bool is_str = (cls_node->as.class_decl.fields[f]->as.field.type == TY_STRING &&
                                       !cls_node->as.class_decl.fields[f]->as.field.is_array &&
                                       !cls_node->as.class_decl.fields[f]->as.field.is_map);
                        if (fval) {
                            gen_expr(ctx, fval);
                            if (is_str) {
                                sb_appendf(&ctx->sb, ", %s", is_owned_temporary_string_expr(fval) ? "1" : "0");
                            }
                        } else {
                            sb_append(&ctx->sb, "0");
                            if (is_str) {
                                sb_append(&ctx->sb, ", 0");
                            }
                        }
                    }
                }
                sb_append(&ctx->sb, ")");
            }
            break;
        }

        case NODE_MEMBER:
            gen_expr(ctx, expr->as.member.object);
            if (is_expr_pointer(ctx, expr->as.member.object)) {
                sb_append(&ctx->sb, "->");
            } else {
                sb_append(&ctx->sb, ".");
            }
            sb_append(&ctx->sb, expr->as.member.member_name);
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

        case NODE_BINARY: {
            Type lt = infer_expr_type(ctx->program, ctx->current_function, expr->as.binary.left);
            Type rt = infer_expr_type(ctx->program, ctx->current_function, expr->as.binary.right);
            const char *lcls = get_expr_elem_class_name(ctx, expr->as.binary.left);
            const char *rcls = get_expr_elem_class_name(ctx, expr->as.binary.right);

            if (lt == TY_CLASS && rt == TY_CLASS && lcls && rcls && strcmp(lcls, rcls) == 0 && is_struct_name(ctx, lcls)) {
                AstNode *op_fn = NULL;
                if (ctx->program && ctx->program->type == NODE_PROGRAM) {
                    for (int i = 0; i < ctx->program->as.program.count; i++) {
                        AstNode *fn = ctx->program->as.program.functions[i];
                        if (fn->as.function.is_operator &&
                            strcmp(fn->as.function.operator_symbol, expr->as.binary.op) == 0 &&
                            fn->as.function.param_count == 2 &&
                            fn->as.function.param_class_names[0] &&
                            strcmp(fn->as.function.param_class_names[0], lcls) == 0) {
                            op_fn = fn;
                            break;
                        }
                    }
                }
                if (op_fn) {
                    const char *c_fn_name = get_operator_fn_c_name(lcls, expr->as.binary.op, 2);
                    sb_appendf(&ctx->sb, "%s(", c_fn_name);
                    gen_expr(ctx, expr->as.binary.left);
                    sb_append(&ctx->sb, ", ");
                    gen_expr(ctx, expr->as.binary.right);
                    sb_append(&ctx->sb, ")");
                } else {
                    char short_msg[256];
                    snprintf(short_msg, sizeof(short_msg), "no 'operator%s' defined for struct '%s'", expr->as.binary.op, lcls);
                    char note_msg[256];
                    if (strcmp(expr->as.binary.op, "==") == 0 || strcmp(expr->as.binary.op, "!=") == 0 ||
                        strcmp(expr->as.binary.op, "<") == 0 || strcmp(expr->as.binary.op, ">") == 0 ||
                        strcmp(expr->as.binary.op, "<=") == 0 || strcmp(expr->as.binary.op, ">=") == 0) {
                        snprintf(note_msg, sizeof(note_msg), "fn operator%s(a: %s, b: %s) -> bool { ... }", expr->as.binary.op, lcls, lcls);
                    } else {
                        snprintf(note_msg, sizeof(note_msg), "fn operator%s(a: %s, b: %s) -> %s { ... }", expr->as.binary.op, lcls, lcls, lcls);
                    }
                    ErrorLocation loc = {get_error_filename(), expr->line, expr->col};
                    print_formatted_error(short_msg, loc, "missing operator definition", note_msg, NULL, NULL, NULL);
                    exit(1);
                }
            } else {
                sb_append(&ctx->sb, "(");
                gen_expr(ctx, expr->as.binary.left);
                sb_appendf(&ctx->sb, " %s ", expr->as.binary.op);
                gen_expr(ctx, expr->as.binary.right);
                sb_append(&ctx->sb, ")");
            }
            break;
        }

        case NODE_UNARY: {
            if (strcmp(expr->as.unary.op, "&") == 0) {
                Type ot = infer_expr_type(ctx->program, ctx->current_function, expr->as.unary.operand);
                bool is_arr = infer_expr_is_array(ctx->program, ctx->current_function, expr->as.unary.operand);
                bool is_map = infer_expr_is_map(ctx->program, ctx->current_function, expr->as.unary.operand);
                TypeDesc *otd = get_expr_type_desc(ctx, expr->as.unary.operand);
                if (otd && (otd->kind == TYPEDESC_MAP || otd->kind == TYPEDESC_ARRAY)) {
                    is_map = (otd->kind == TYPEDESC_MAP);
                    is_arr = (otd->kind == TYPEDESC_ARRAY);
                }
                const char *ocls = get_expr_elem_class_name(ctx, expr->as.unary.operand);
                if (is_arr || is_map || (ot == TY_CLASS && ocls && !is_struct_name(ctx, ocls))) {
                    gen_expr(ctx, expr->as.unary.operand);
                    break;
                }
            } else if (strcmp(expr->as.unary.op, "-") == 0) {
                Type ot = infer_expr_type(ctx->program, ctx->current_function, expr->as.unary.operand);
                const char *ocls = get_expr_elem_class_name(ctx, expr->as.unary.operand);
                if (ot == TY_CLASS && ocls && is_struct_name(ctx, ocls)) {
                    AstNode *op_fn = NULL;
                    if (ctx->program && ctx->program->type == NODE_PROGRAM) {
                        for (int i = 0; i < ctx->program->as.program.count; i++) {
                            AstNode *fn = ctx->program->as.program.functions[i];
                            if (fn->as.function.is_operator &&
                                strcmp(fn->as.function.operator_symbol, "-") == 0 &&
                                fn->as.function.param_count == 1 &&
                                fn->as.function.param_class_names[0] &&
                                strcmp(fn->as.function.param_class_names[0], ocls) == 0) {
                                op_fn = fn;
                                break;
                            }
                        }
                    }
                    if (op_fn) {
                        const char *c_fn_name = get_operator_fn_c_name(ocls, "-", 1);
                        sb_appendf(&ctx->sb, "%s(", c_fn_name);
                        gen_expr(ctx, expr->as.unary.operand);
                        sb_append(&ctx->sb, ")");
                        break;
                    } else {
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "no 'operator-' defined for struct '%s'", ocls);
                        char note_msg[256];
                        snprintf(note_msg, sizeof(note_msg), "fn operator-(a: %s) -> %s { ... }", ocls, ocls);
                        ErrorLocation loc = {get_error_filename(), expr->line, expr->col};
                        print_formatted_error(short_msg, loc, "missing operator definition", note_msg, NULL, NULL, NULL);
                        exit(1);
                    }
                }
            }
            sb_appendf(&ctx->sb, "(%s", expr->as.unary.op);
            gen_expr(ctx, expr->as.unary.operand);
            sb_append(&ctx->sb, ")");
            break;
        }

        case NODE_CALL: {
            const char *callee = expr->as.call.callee;
            if (strcmp(callee, "len") == 0) {
                AstNode *arg = expr->as.call.args[0];
                if (infer_expr_is_map(ctx->program, ctx->current_function, arg)) {
                    sb_append(&ctx->sb, "((int)(");
                    gen_expr(ctx, arg);
                    sb_append(&ctx->sb, ")->occupied)");
                } else {
                    Type arg_t = infer_expr_type(ctx->program, ctx->current_function, arg);
                    bool is_arr = infer_expr_is_array(ctx->program, ctx->current_function, arg);
                    if (is_arr || arg_t != TY_STRING) {
                        sb_append(&ctx->sb, "((int)__cco_arr_len_raw(");
                        gen_expr(ctx, arg);
                        sb_append(&ctx->sb, "))");
                    } else {
                        sb_append(&ctx->sb, "((int)strlen(");
                        gen_expr(ctx, arg);
                        sb_append(&ctx->sb, "))");
                    }
                }
            } else if (strcmp(callee, "put") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                AstNode *k_arg = expr->as.call.args[1];
                AstNode *v_arg = expr->as.call.args[2];
                TypeDesc *m_td = get_expr_type_desc(ctx, m_arg);

                sb_append(&ctx->sb, "__cco_map_put(");
                gen_expr(ctx, m_arg);
                sb_append(&ctx->sb, ", ");

                bool k_is_str = false;
                if (m_td && m_td->kind == TYPEDESC_MAP && m_td->as.map_desc.key_type) {
                    k_is_str = (m_td->as.map_desc.key_type->kind == TYPEDESC_PRIMITIVE && m_td->as.map_desc.key_type->as.primitive_desc.primitive_kind == TY_STRING);
                } else {
                    k_is_str = (get_map_key_type(ctx, m_arg) == TY_STRING);
                }

                if (k_is_str) {
                    sb_append(&ctx->sb, "(void *)(");
                    gen_expr(ctx, k_arg);
                    sb_append(&ctx->sb, ")");
                } else {
                    sb_append(&ctx->sb, "(void *)(intptr_t)(");
                    gen_expr(ctx, k_arg);
                    sb_append(&ctx->sb, ")");
                }
                sb_append(&ctx->sb, ", ");

                if (m_td && m_td->kind == TYPEDESC_MAP && m_td->as.map_desc.val_type) {
                    TypeDesc *v_td = m_td->as.map_desc.val_type;
                    bool v_is_ptr = (v_td->kind == TYPEDESC_CLASS || v_td->kind == TYPEDESC_ENUM || v_td->kind == TYPEDESC_NAMED || v_td->kind == TYPEDESC_MAP || v_td->kind == TYPEDESC_ARRAY || (v_td->kind == TYPEDESC_PRIMITIVE && v_td->as.primitive_desc.primitive_kind == TY_STRING));
                    if (v_is_ptr) {
                        sb_append(&ctx->sb, "(void *)(");
                        gen_expr(ctx, v_arg);
                        sb_append(&ctx->sb, ")");
                    } else {
                        sb_append(&ctx->sb, "(void *)(intptr_t)(");
                        gen_expr(ctx, v_arg);
                        sb_append(&ctx->sb, ")");
                    }
                    sb_append(&ctx->sb, ", ");
                    if (v_td->kind == TYPEDESC_PRIMITIVE && v_td->as.primitive_desc.primitive_kind == TY_STRING) {
                        sb_append(&ctx->sb, "(__cco_val_free_fn)free)");
                    } else if (v_td->kind == TYPEDESC_CLASS || v_td->kind == TYPEDESC_ENUM || v_td->kind == TYPEDESC_NAMED) {
                        sb_appendf(&ctx->sb, "(__cco_val_free_fn)%s_free)", v_td->as.named_desc.name);
                    } else if (v_td->kind == TYPEDESC_ARRAY) {
                        TypeDesc *elem = v_td->as.array_desc.elem_type;
                        if (elem && (elem->kind == TYPEDESC_CLASS || elem->kind == TYPEDESC_ENUM || elem->kind == TYPEDESC_NAMED)) {
                            sb_appendf(&ctx->sb, "(__cco_val_free_fn)__cco_free_arr_of_%s)", elem->as.named_desc.name);
                        } else if (elem && elem->kind == TYPEDESC_PRIMITIVE && elem->as.primitive_desc.primitive_kind == TY_STRING) {
                            sb_append(&ctx->sb, "(__cco_val_free_fn)__cco_free_arr_of_string)");
                        } else if (elem && elem->kind == TYPEDESC_MAP) {
                            TypeDesc *ik = elem->as.map_desc.key_type;
                            TypeDesc *iv = elem->as.map_desc.val_type;
                            const char *ks = (ik && ik->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int";
                            const char *vs = (iv && iv->kind != TYPEDESC_PRIMITIVE) ? iv->as.named_desc.name : ((iv && iv->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int");
                            sb_appendf(&ctx->sb, "(__cco_val_free_fn)__cco_free_arr_of_map_%s_%s)", ks, vs);
                        } else {
                            sb_append(&ctx->sb, "(__cco_val_free_fn)__cco_free_arr)");
                        }
                    } else if (v_td->kind == TYPEDESC_MAP) {
                        TypeDesc *ik = v_td->as.map_desc.key_type;
                        TypeDesc *iv = v_td->as.map_desc.val_type;
                        const char *ks = (ik && ik->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int";
                        const char *vs = (iv && iv->kind != TYPEDESC_PRIMITIVE) ? iv->as.named_desc.name : ((iv && iv->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int");
                        sb_appendf(&ctx->sb, "(__cco_val_free_fn)__cco_free_map_%s_%s)", ks, vs);
                    } else {
                        sb_append(&ctx->sb, "NULL)");
                    }
                } else {
                    const char *v_cls = get_map_val_class_name(ctx, m_arg);
                    bool val_is_arr = get_map_val_is_array(ctx, m_arg);
                    if (val_is_arr || (v_cls && (find_class(ctx->ct, v_cls) != NULL || find_enum(ctx->ct, v_cls) != NULL))) {
                        sb_append(&ctx->sb, "(void *)(");
                        gen_expr(ctx, v_arg);
                        sb_append(&ctx->sb, ")");
                    } else {
                        Type vt = infer_expr_type(ctx->program, ctx->current_function, v_arg);
                        if (vt == TY_STRING) {
                            sb_append(&ctx->sb, "(void *)(");
                            gen_expr(ctx, v_arg);
                            sb_append(&ctx->sb, ")");
                        } else {
                            sb_append(&ctx->sb, "(void *)(intptr_t)(");
                            gen_expr(ctx, v_arg);
                            sb_append(&ctx->sb, ")");
                        }
                    }
                    sb_append(&ctx->sb, ", ");
                    if (!val_is_arr && v_cls && (find_class(ctx->ct, v_cls) != NULL || find_enum(ctx->ct, v_cls) != NULL)) {
                        sb_appendf(&ctx->sb, "(__cco_val_free_fn)%s_free)", v_cls);
                    } else {
                        sb_append(&ctx->sb, "NULL)");
                    }
                }
            } else if (strcmp(callee, "get") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                AstNode *k_arg = expr->as.call.args[1];
                TypeDesc *m_td = get_expr_type_desc(ctx, m_arg);
                if (m_td && m_td->kind == TYPEDESC_MAP && m_td->as.map_desc.val_type) {
                    TypeDesc *v_td = m_td->as.map_desc.val_type;
                    const char *c_type = c_type_str_desc(ctx, v_td);
                    bool v_is_ptr = (v_td->kind == TYPEDESC_CLASS || v_td->kind == TYPEDESC_ENUM || v_td->kind == TYPEDESC_NAMED || v_td->kind == TYPEDESC_MAP || v_td->kind == TYPEDESC_ARRAY || (v_td->kind == TYPEDESC_PRIMITIVE && v_td->as.primitive_desc.primitive_kind == TY_STRING));
                    if (v_is_ptr) {
                        sb_appendf(&ctx->sb, "(%s)__cco_map_get(", c_type);
                    } else {
                        sb_appendf(&ctx->sb, "(%s)(intptr_t)__cco_map_get(", c_type);
                    }
                    gen_expr(ctx, m_arg);
                    sb_append(&ctx->sb, ", ");
                    bool k_is_str = (m_td->as.map_desc.key_type && m_td->as.map_desc.key_type->kind == TYPEDESC_PRIMITIVE && m_td->as.map_desc.key_type->as.primitive_desc.primitive_kind == TY_STRING);
                    if (k_is_str) {
                        sb_append(&ctx->sb, "(void *)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    } else {
                        sb_append(&ctx->sb, "(void *)(intptr_t)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    }
                } else {
                    Type kt = get_map_key_type(ctx, m_arg);
                    Type vt = get_map_val_type(ctx, m_arg);
                    const char *v_cls = get_map_val_class_name(ctx, m_arg);
                    bool val_is_arr = get_map_val_is_array(ctx, m_arg);
                    const char *c_type = val_is_arr ? c_type_str_decl(ctx, vt, v_cls, true, false) : c_type_str_full(ctx, vt, v_cls, false, false);

                    if (val_is_arr || vt == TY_CLASS || vt == TY_STRING) {
                        sb_appendf(&ctx->sb, "(%s)__cco_map_get(", c_type);
                    } else {
                        sb_appendf(&ctx->sb, "(%s)(intptr_t)__cco_map_get(", c_type);
                    }
                    gen_expr(ctx, m_arg);
                    sb_append(&ctx->sb, ", ");
                    if (kt == TY_STRING) {
                        sb_append(&ctx->sb, "(void *)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    } else {
                        sb_append(&ctx->sb, "(void *)(intptr_t)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    }
                }
            } else if (strcmp(callee, "has") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                AstNode *k_arg = expr->as.call.args[1];
                TypeDesc *m_td = get_expr_type_desc(ctx, m_arg);
                bool k_is_str = false;
                if (m_td && m_td->kind == TYPEDESC_MAP && m_td->as.map_desc.key_type) {
                    k_is_str = (m_td->as.map_desc.key_type->kind == TYPEDESC_PRIMITIVE && m_td->as.map_desc.key_type->as.primitive_desc.primitive_kind == TY_STRING);
                } else {
                    k_is_str = (get_map_key_type(ctx, m_arg) == TY_STRING);
                }

                sb_append(&ctx->sb, "__cco_map_has(");
                gen_expr(ctx, m_arg);
                sb_append(&ctx->sb, ", ");
                if (k_is_str) {
                    sb_append(&ctx->sb, "(void *)(");
                    gen_expr(ctx, k_arg);
                    sb_append(&ctx->sb, "))");
                } else {
                    sb_append(&ctx->sb, "(void *)(intptr_t)(");
                    gen_expr(ctx, k_arg);
                    sb_append(&ctx->sb, "))");
                }
            } else if (strcmp(callee, "remove") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                AstNode *k_arg = expr->as.call.args[1];
                TypeDesc *m_td = get_expr_type_desc(ctx, m_arg);
                if (m_td && m_td->kind == TYPEDESC_MAP && m_td->as.map_desc.val_type) {
                    TypeDesc *v_td = m_td->as.map_desc.val_type;
                    const char *c_type = c_type_str_desc(ctx, v_td);
                    bool v_is_ptr = (v_td->kind == TYPEDESC_CLASS || v_td->kind == TYPEDESC_ENUM || v_td->kind == TYPEDESC_NAMED || v_td->kind == TYPEDESC_MAP || v_td->kind == TYPEDESC_ARRAY || (v_td->kind == TYPEDESC_PRIMITIVE && v_td->as.primitive_desc.primitive_kind == TY_STRING));
                    if (v_is_ptr) {
                        sb_appendf(&ctx->sb, "(%s)__cco_map_remove(", c_type);
                    } else {
                        sb_appendf(&ctx->sb, "(%s)(intptr_t)__cco_map_remove(", c_type);
                    }
                    gen_expr(ctx, m_arg);
                    sb_append(&ctx->sb, ", ");
                    bool k_is_str = (m_td->as.map_desc.key_type && m_td->as.map_desc.key_type->kind == TYPEDESC_PRIMITIVE && m_td->as.map_desc.key_type->as.primitive_desc.primitive_kind == TY_STRING);
                    if (k_is_str) {
                        sb_append(&ctx->sb, "(void *)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    } else {
                        sb_append(&ctx->sb, "(void *)(intptr_t)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    }
                } else {
                    Type kt = get_map_key_type(ctx, m_arg);
                    Type vt = get_map_val_type(ctx, m_arg);
                    const char *v_cls = get_map_val_class_name(ctx, m_arg);
                    const char *c_type = c_type_str_full(ctx, vt, v_cls, false, false);

                    if (vt == TY_CLASS || vt == TY_STRING) {
                        sb_appendf(&ctx->sb, "(%s)__cco_map_remove(", c_type);
                    } else {
                        sb_appendf(&ctx->sb, "(%s)(intptr_t)__cco_map_remove(", c_type);
                    }
                    gen_expr(ctx, m_arg);
                    sb_append(&ctx->sb, ", ");
                    if (kt == TY_STRING) {
                        sb_append(&ctx->sb, "(void *)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    } else {
                        sb_append(&ctx->sb, "(void *)(intptr_t)(");
                        gen_expr(ctx, k_arg);
                        sb_append(&ctx->sb, "))");
                    }
                }
            } else if (strcmp(callee, "keys") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                Type kt = get_map_key_type(ctx, m_arg);
                const char *ret_cast = (kt == TY_STRING) ? "(char **)" : "(int *)";
                sb_appendf(&ctx->sb, "%s__cco_map_keys(", ret_cast);
                gen_expr(ctx, m_arg);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "push") == 0) {
                AstNode *arr_arg = expr->as.call.args[0];
                AstNode *val_arg = expr->as.call.args[1];
                TypeDesc *arr_td = get_expr_type_desc(ctx, arr_arg);
                const char *elem_c_type = NULL;
                const char *arr_ptr_type = NULL;
                Type elem_type = TY_INT;
                if (arr_td && arr_td->kind == TYPEDESC_ARRAY) {
                    elem_c_type = c_type_str_desc(ctx, arr_td->as.array_desc.elem_type);
                    arr_ptr_type = c_type_str_desc(ctx, arr_td);
                    if (arr_td->as.array_desc.elem_type && arr_td->as.array_desc.elem_type->kind == TYPEDESC_PRIMITIVE && arr_td->as.array_desc.elem_type->as.primitive_desc.primitive_kind == TY_STRING) {
                        elem_type = TY_STRING;
                    }
                } else {
                    elem_type = infer_expr_type(ctx->program, ctx->current_function, arr_arg);
                    const char *elem_cls = get_expr_elem_class_name(ctx, arr_arg);
                    elem_c_type = c_type_str_full(ctx, elem_type, elem_cls, false, false);
                    arr_ptr_type = c_type_str_decl(ctx, elem_type, elem_cls, true, false);
                }

                sb_append(&ctx->sb, "(");
                gen_expr(ctx, arr_arg);
                sb_appendf(&ctx->sb, " = (%s)__cco_arr_maybe_grow(", arr_ptr_type);
                gen_expr(ctx, arr_arg);
                sb_appendf(&ctx->sb, ", sizeof(%s)), (", elem_c_type);

                sb_appendf(&ctx->sb, "((%s)", arr_ptr_type);
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, "))[__cco_arr_len_raw(");
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, ")] = ");
                if (elem_type == TY_STRING && val_arg->type == NODE_LITERAL) {
                    sb_appendf(&ctx->sb, "strdup(\"%s\")", val_arg->as.literal.val.s);
                } else {
                    gen_expr(ctx, val_arg);
                }
                sb_append(&ctx->sb, ", __cco_arr_incr_len(");
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, "), ");
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "pop") == 0) {
                AstNode *arr_arg = expr->as.call.args[0];
                TypeDesc *arr_td = get_expr_type_desc(ctx, arr_arg);
                const char *arr_ptr_type = NULL;
                if (arr_td && arr_td->kind == TYPEDESC_ARRAY) {
                    arr_ptr_type = c_type_str_desc(ctx, arr_td);
                } else {
                    Type elem_type = infer_expr_type(ctx->program, ctx->current_function, arr_arg);
                    const char *elem_cls = get_expr_elem_class_name(ctx, arr_arg);
                    arr_ptr_type = c_type_str_decl(ctx, elem_type, elem_cls, true, false);
                }

                sb_append(&ctx->sb, "(__cco_arr_decr_len(");
                gen_expr(ctx, arr_arg);
                sb_appendf(&ctx->sb, "), ((%s)", arr_ptr_type);
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, ")[__cco_arr_len_raw(");
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, ")])");
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
            } else if (strcmp(callee, "exp") == 0) {
                sb_append(&ctx->sb, "((float)exp(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "log") == 0) {
                sb_append(&ctx->sb, "((float)log(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "sin") == 0) {
                sb_append(&ctx->sb, "((float)sin(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "cos") == 0) {
                sb_append(&ctx->sb, "((float)cos(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, "))");
            } else if (strcmp(callee, "tan") == 0) {
                sb_append(&ctx->sb, "((float)tan(");
                gen_expr(ctx, expr->as.call.args[0]);
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
            } else if (strcmp(callee, "read_file_checked") == 0) {
                sb_append(&ctx->sb, "__cco_read_file_checked(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "io_last_error") == 0) {
                sb_append(&ctx->sb, "__cco_io_last_error()");
            } else if (strcmp(callee, "char_to_string") == 0) {
                sb_append(&ctx->sb, "__cco_char_to_string(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "to_string") == 0) {
                sb_append(&ctx->sb, "__cco_to_string(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "exit") == 0) {
                sb_append(&ctx->sb, "__cco_exit(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "file_exists") == 0) {
                sb_append(&ctx->sb, "__cco_file_exists(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "write_file") == 0) {
                sb_append(&ctx->sb, "__cco_write_file(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "write_bytes") == 0) {
                sb_append(&ctx->sb, "__cco_write_bytes(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "execute_cmd") == 0 || strcmp(callee, "system") == 0) {
                sb_append(&ctx->sb, "__cco_execute_cmd(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "args") == 0) {
                sb_append(&ctx->sb, "__cco_get_args()");
            } else if (strcmp(callee, "arg_count") == 0) {
                sb_append(&ctx->sb, "__cco_get_arg_count()");
            } else if (strcmp(callee, "program_name") == 0) {
                sb_append(&ctx->sb, "__cco_get_program_name()");
            } else if (strcmp(callee, "random_seed") == 0) {
                sb_append(&ctx->sb, "__cco_random_seed(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "random_int") == 0) {
                sb_append(&ctx->sb, "__cco_random_int(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "to_int") == 0) {
                sb_append(&ctx->sb, "__cco_to_int(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "is_int") == 0) {
                sb_append(&ctx->sb, "__cco_is_int(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "to_float") == 0) {
                sb_append(&ctx->sb, "__cco_to_float(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "is_float") == 0) {
                sb_append(&ctx->sb, "__cco_is_float(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "read_line") == 0) {
                sb_append(&ctx->sb, "__cco_read_line()");
            } else if (strcmp(callee, "input") == 0) {
                if (expr->as.call.arg_count == 0) {
                    sb_append(&ctx->sb, "__cco_input(NULL)");
                } else {
                    sb_append(&ctx->sb, "__cco_input(");
                    gen_expr(ctx, expr->as.call.args[0]);
                    sb_append(&ctx->sb, ")");
                }
            } else if (strcmp(callee, "net_listen") == 0) {
                sb_append(&ctx->sb, "__cco_net_listen(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_listen_on") == 0) {
                sb_append(&ctx->sb, "__cco_net_listen_on(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_connect") == 0) {
                sb_append(&ctx->sb, "__cco_net_connect(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_last_error") == 0) {
                sb_append(&ctx->sb, "__cco_net_last_error()");
            } else if (strcmp(callee, "net_accept") == 0) {
                sb_append(&ctx->sb, "__cco_net_accept(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_recv") == 0) {
                sb_append(&ctx->sb, "__cco_net_recv(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_recv_raw") == 0) {
                sb_append(&ctx->sb, "__cco_net_recv_raw(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_recv_http") == 0) {
                sb_append(&ctx->sb, "__cco_net_recv_http(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_send") == 0) {
                sb_append(&ctx->sb, "__cco_net_send(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_send_bytes") == 0) {
                sb_append(&ctx->sb, "__cco_net_send_bytes(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[2]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_recv_bytes") == 0) {
                sb_append(&ctx->sb, "__cco_net_recv_bytes(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[2]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_close") == 0) {
                sb_append(&ctx->sb, "__cco_net_close(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_shutdown") == 0) {
                sb_append(&ctx->sb, "__cco_net_shutdown(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "net_set_timeouts") == 0) {
                sb_append(&ctx->sb, "__cco_net_set_timeouts(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[1]);
                sb_append(&ctx->sb, ", ");
                gen_expr(ctx, expr->as.call.args[2]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "sleep_ms") == 0) {
                sb_append(&ctx->sb, "__cco_sleep_ms(");
                gen_expr(ctx, expr->as.call.args[0]);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "free") == 0 && expr->as.call.arg_count == 1) {
                AstNode *arg = expr->as.call.args[0];
                if (is_pure_lvalue(arg)) {
                    sb_append(&ctx->sb, "(free(");
                    gen_expr(ctx, arg);
                    sb_append(&ctx->sb, "), ");
                    gen_expr(ctx, arg);
                    sb_append(&ctx->sb, " = NULL)");
                } else {
                    sb_append(&ctx->sb, "free(");
                    gen_expr(ctx, arg);
                    sb_append(&ctx->sb, ")");
                }
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

static void gen_fstring_part(CodegenCtx *ctx, AstNode *part, bool *out_needs_free) {
    if (part->type == NODE_FSTRING_TEXT) {
        *out_needs_free = false;
        sb_append_escaped_string(&ctx->sb, part->as.fstring_text.text);
        return;
    }

    Type t = infer_expr_type(ctx->program, ctx->current_function, part);
    if (t == TY_INT) {
        *out_needs_free = true;
        sb_append(&ctx->sb, "__cco_int_to_str(");
        gen_expr(ctx, part);
        sb_append(&ctx->sb, ")");
    } else if (t == TY_FLOAT) {
        *out_needs_free = true;
        sb_append(&ctx->sb, "__cco_float_to_str(");
        gen_expr(ctx, part);
        sb_append(&ctx->sb, ")");
    } else if (t == TY_BOOL) {
        *out_needs_free = true;
        sb_append(&ctx->sb, "__cco_bool_to_str(");
        gen_expr(ctx, part);
        sb_append(&ctx->sb, ")");
    } else if (t == TY_CHAR) {
        *out_needs_free = true;
        sb_append(&ctx->sb, "__cco_char_to_str(");
        gen_expr(ctx, part);
        sb_append(&ctx->sb, ")");
    } else if (t == TY_STRING) {
        if (part->type == NODE_LITERAL) {
            *out_needs_free = false;
            sb_append_escaped_string(&ctx->sb, part->as.literal.val.s);
        } else if (part->type == NODE_CALL || part->type == NODE_METHOD_CALL || part->type == NODE_FSTRING) {
            *out_needs_free = true;
            gen_expr(ctx, part);
        } else {
            *out_needs_free = false;
            gen_expr(ctx, part);
        }
    } else {
        *out_needs_free = false;
        gen_expr(ctx, part);
    }
}

static inline bool is_heap_string_returning_call(const char *name) {
    if (!name) return false;
    return (strcmp(name, "concat") == 0 ||
            strcmp(name, "substring") == 0 ||
            strcmp(name, "read_file") == 0 ||
            strcmp(name, "read_file_checked") == 0 ||
            strcmp(name, "io_last_error") == 0 ||
            strcmp(name, "net_last_error") == 0 ||
            strcmp(name, "read_line") == 0 ||
            strcmp(name, "input") == 0 ||
            strcmp(name, "net_recv") == 0 ||
            strcmp(name, "net_recv_raw") == 0 ||
            strcmp(name, "net_recv_http") == 0 ||
            strcmp(name, "program_name") == 0);
}

static void gen_stmt(CodegenCtx *ctx, AstNode *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case NODE_LET: {
            emit_indent(ctx);
            const char *let_type_str = stmt->as.let.type_desc ?
                c_type_str_desc(ctx, stmt->as.let.type_desc) :
                c_type_str_decl_full(ctx, stmt->as.let.var_type, stmt->as.let.class_name, stmt->as.let.is_array, stmt->as.let.is_map, stmt->is_heap_owner);
            sb_appendf(&ctx->sb, "%s %s = ", let_type_str, stmt->as.let.name);
            if (stmt->as.let.var_type == TY_STRING && stmt->as.let.value->type == NODE_LITERAL) {
                sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.let.value->as.literal.val.s);
            } else {
                gen_expr(ctx, stmt->as.let.value);
            }
            sb_append(&ctx->sb, ";\n");
            emit_nulls(ctx, stmt);
            break;
        }

        case NODE_ASSIGN: {
            bool has_cleanup = (stmt->frees_count > 0) || (stmt->as.assign.release_old && (stmt->as.assign.class_name || stmt->as.assign.type_desc));
            if (has_cleanup) {
                static int assign_tmp_counter = 0;
                int cur_assign_tmp = ++assign_tmp_counter;
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "void *__assign_tmp_%d = (void *)(", cur_assign_tmp);
                if (stmt->as.assign.value->type == NODE_LITERAL && stmt->as.assign.value->as.literal.lit_type == TY_STRING) {
                    sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.assign.value->as.literal.val.s);
                } else {
                    gen_expr(ctx, stmt->as.assign.value);
                }
                sb_append(&ctx->sb, ");\n");
                for (int i = 0; i < stmt->frees_count; i++) {
                    if (stmt->as.assign.release_old && (stmt->as.assign.class_name || stmt->as.assign.type_desc) &&
                        strcmp(stmt->frees_to_emit[i].var_name, stmt->as.assign.name) == 0) {
                        continue;
                    }
                    emit_indent(ctx);
                    if (stmt->frees_to_emit[i].is_array) {
                        sb_appendf(&ctx->sb, "if (%s != NULL && (void *)%s != __assign_tmp_%d) __cco_free_arr(%s);\n", stmt->frees_to_emit[i].var_name, stmt->frees_to_emit[i].var_name, cur_assign_tmp, stmt->frees_to_emit[i].var_name);
                    } else {
                        sb_appendf(&ctx->sb, "if (%s != NULL && (void *)%s != __assign_tmp_%d) free(%s);\n", stmt->frees_to_emit[i].var_name, stmt->frees_to_emit[i].var_name, cur_assign_tmp, stmt->frees_to_emit[i].var_name);
                    }
                }
                if (stmt->as.assign.release_old) {
                    if (stmt->as.assign.type_desc) {
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "if (%s != NULL && (void *)%s != __assign_tmp_%d) {\n", stmt->as.assign.name, stmt->as.assign.name, cur_assign_tmp);
                        ctx->indent_level++;
                        emit_typedesc_cleanup(ctx, stmt->as.assign.name, stmt->as.assign.type_desc);
                        ctx->indent_level--;
                        emit_indent(ctx);
                        sb_append(&ctx->sb, "}\n");
                    } else if (stmt->as.assign.class_name) {
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "if (%s != NULL && (void *)%s != __assign_tmp_%d) %s_free(%s);\n", stmt->as.assign.name, stmt->as.assign.name, cur_assign_tmp, stmt->as.assign.class_name, stmt->as.assign.name);
                    }
                }
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "%s = __assign_tmp_%d;\n", stmt->as.assign.name, cur_assign_tmp);
            } else {
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "%s = ", stmt->as.assign.name);
                if (stmt->as.assign.value->type == NODE_LITERAL && stmt->as.assign.value->as.literal.lit_type == TY_STRING) {
                    sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.assign.value->as.literal.val.s);
                } else {
                    gen_expr(ctx, stmt->as.assign.value);
                }
                sb_append(&ctx->sb, ";\n");
            }
            emit_nulls(ctx, stmt);
            break;
        }

        case NODE_MEMBER_ASSIGN: {
            if (stmt->as.member_assign.release_old) {
                static int member_assign_tmp_counter = 0;
                int cur_tmp = ++member_assign_tmp_counter;
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "void *__m_tmp_%d = (void *)(", cur_tmp);
                gen_expr(ctx, stmt->as.member_assign.value);
                sb_append(&ctx->sb, ");\n");

                const char *arrow = is_expr_pointer(ctx, stmt->as.member_assign.object) ? "->" : ".";
                if (stmt->as.member_assign.field_is_map) {
                    emit_indent(ctx);
                    if (stmt->as.member_assign.field_class_name && (find_class(ctx->ct, stmt->as.member_assign.field_class_name) != NULL || find_enum(ctx->ct, stmt->as.member_assign.field_class_name) != NULL)) {
                        sb_appendf(&ctx->sb, "if (");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != NULL && ", arrow, stmt->as.member_assign.member_name);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != __m_tmp_%d) __cco_map_free(", arrow, stmt->as.member_assign.member_name, cur_tmp);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s, (__cco_val_free_fn)%s_free);\n", arrow, stmt->as.member_assign.member_name, stmt->as.member_assign.field_class_name);
                    } else {
                        sb_appendf(&ctx->sb, "if (");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != NULL && ", arrow, stmt->as.member_assign.member_name);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != __m_tmp_%d) __cco_map_free(", arrow, stmt->as.member_assign.member_name, cur_tmp);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s, NULL);\n", arrow, stmt->as.member_assign.member_name);
                    }
                } else if (stmt->as.member_assign.field_is_array) {
                    emit_indent(ctx);
                    const char *fcls = stmt->as.member_assign.field_class_name;
                    if (fcls && (find_class(ctx->ct, fcls) != NULL || find_enum(ctx->ct, fcls) != NULL)) {
                        sb_appendf(&ctx->sb, "if (");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != NULL && ", arrow, stmt->as.member_assign.member_name);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != __m_tmp_%d) {\n", arrow, stmt->as.member_assign.member_name, cur_tmp);
                        ctx->indent_level++;
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "for (int __i = 0; __i < __cco_arr_len(");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s); __i++) {\n", arrow, stmt->as.member_assign.member_name);
                        ctx->indent_level++;
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "if (");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s[__i] != NULL) { %s_free(", arrow, stmt->as.member_assign.member_name, fcls);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s[__i]); }\n", arrow, stmt->as.member_assign.member_name);
                        ctx->indent_level--;
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "}\n");
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "__cco_free_arr(");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s);\n", arrow, stmt->as.member_assign.member_name);
                        ctx->indent_level--;
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "}\n");
                    } else {
                        sb_appendf(&ctx->sb, "if (");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != NULL && ", arrow, stmt->as.member_assign.member_name);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != __m_tmp_%d) { __cco_free_arr(", arrow, stmt->as.member_assign.member_name, cur_tmp);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s); }\n", arrow, stmt->as.member_assign.member_name);
                    }
                } else if (stmt->as.member_assign.field_class_name) {
                    const char *fcls = stmt->as.member_assign.field_class_name;
                    emit_indent(ctx);
                    if (strcmp(fcls, "string") == 0) {
                        sb_appendf(&ctx->sb, "if (");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != NULL && ", arrow, stmt->as.member_assign.member_name);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != __m_tmp_%d) free(", arrow, stmt->as.member_assign.member_name, cur_tmp);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s);\n", arrow, stmt->as.member_assign.member_name);
                    } else {
                        sb_appendf(&ctx->sb, "if (");
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != NULL && ", arrow, stmt->as.member_assign.member_name);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s != __m_tmp_%d) %s_free(", arrow, stmt->as.member_assign.member_name, cur_tmp, fcls);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s);\n", arrow, stmt->as.member_assign.member_name);
                    }
                }
                emit_indent(ctx);
                gen_expr(ctx, stmt->as.member_assign.object);
                const char *arrow_assign = is_expr_pointer(ctx, stmt->as.member_assign.object) ? "->" : ".";
                if (stmt->as.member_assign.field_class_name && strcmp(stmt->as.member_assign.field_class_name, "string") == 0) {
                    if (is_owned_temporary_string_expr(stmt->as.member_assign.value)) {
                        sb_appendf(&ctx->sb, "%s%s = (char *)__m_tmp_%d;\n", arrow_assign, stmt->as.member_assign.member_name, cur_tmp);
                    } else {
                        sb_appendf(&ctx->sb, "%s%s = (", arrow_assign, stmt->as.member_assign.member_name);
                        gen_expr(ctx, stmt->as.member_assign.object);
                        sb_appendf(&ctx->sb, "%s%s == __m_tmp_%d) ? (char *)__m_tmp_%d : (__m_tmp_%d ? strdup((char *)__m_tmp_%d) : NULL);\n", arrow_assign, stmt->as.member_assign.member_name, cur_tmp, cur_tmp, cur_tmp, cur_tmp);
                    }
                } else {
                    sb_appendf(&ctx->sb, "%s%s = __m_tmp_%d;\n", arrow_assign, stmt->as.member_assign.member_name, cur_tmp);
                }
            } else {
                emit_indent(ctx);
                gen_expr(ctx, stmt->as.member_assign.object);
                if (is_expr_pointer(ctx, stmt->as.member_assign.object)) {
                    sb_appendf(&ctx->sb, "->%s = ", stmt->as.member_assign.member_name);
                } else {
                    sb_appendf(&ctx->sb, ".%s = ", stmt->as.member_assign.member_name);
                }
                gen_expr(ctx, stmt->as.member_assign.value);
                sb_append(&ctx->sb, ";\n");
            }
            emit_nulls(ctx, stmt);
            break;
        }

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
            emit_nulls(ctx, stmt);
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
            Type elem_ty = infer_expr_type(ctx->program, ctx->current_function, stmt->as.for_each.collection_expr);
            emit_indent(ctx);
            if (elem_cls) {
                sb_appendf(&ctx->sb, "%s *%s = ", elem_cls, stmt->as.for_each.loop_var_name);
            } else if (elem_ty == TY_STRING) {
                sb_appendf(&ctx->sb, "char *%s = ", stmt->as.for_each.loop_var_name);
            } else if (elem_ty == TY_FLOAT) {
                sb_appendf(&ctx->sb, "double %s = ", stmt->as.for_each.loop_var_name);
            } else if (elem_ty == TY_BOOL) {
                sb_appendf(&ctx->sb, "bool %s = ", stmt->as.for_each.loop_var_name);
            } else {
                sb_appendf(&ctx->sb, "int %s = ", stmt->as.for_each.loop_var_name);
            }
            if (stmt->as.for_each.collection_expr->type == NODE_IDENT) {
                sb_append(&ctx->sb, stmt->as.for_each.collection_expr->as.ident.name);
            } else {
                gen_expr(ctx, stmt->as.for_each.collection_expr);
            }
            sb_append(&ctx->sb, "[__i];\n");

            if (elem_cls) {
                emit_indent(ctx);
                sb_appendf(&ctx->sb, "if (%s == NULL) continue;\n", stmt->as.for_each.loop_var_name);
            }

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

        case NODE_FOR: {
            bool has_owned_init = (stmt->releases_count > 0 || stmt->frees_count > 0);
            if (has_owned_init) {
                emit_indent(ctx);
                sb_append(&ctx->sb, "{\n");
                ctx->indent_level++;
                emit_indent(ctx);
                if (stmt->as.for_stmt.init) {
                    if (stmt->as.for_stmt.init->type == NODE_LET) {
                        sb_appendf(&ctx->sb, "%s %s = ", c_type_str_full(ctx, stmt->as.for_stmt.init->as.let.var_type, stmt->as.for_stmt.init->as.let.class_name, false, stmt->as.for_stmt.init->is_heap_owner), stmt->as.for_stmt.init->as.let.name);
                        gen_expr(ctx, stmt->as.for_stmt.init->as.let.value);
                    } else if (stmt->as.for_stmt.init->type == NODE_ASSIGN) {
                        sb_appendf(&ctx->sb, "%s = ", stmt->as.for_stmt.init->as.assign.name);
                        gen_expr(ctx, stmt->as.for_stmt.init->as.assign.value);
                    }
                }
                sb_append(&ctx->sb, ";\n");
                emit_indent(ctx);
                sb_append(&ctx->sb, "for (; ");
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
                emit_frees(ctx, stmt);
                emit_releases(ctx, stmt);
                ctx->indent_level--;
                emit_indent(ctx);
                sb_append(&ctx->sb, "}\n");
            } else {
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
            }
            break;
        }

        case NODE_RETURN:
            if (stmt->as.return_stmt.value) {
                Type ret_type = TY_INT;
                const char *ret_class = NULL;
                bool ret_is_array = false;
                bool ret_heap = false;
                if (ctx->current_function) {
                    if (ctx->current_function->type == NODE_FUNCTION) {
                        ret_type = ctx->current_function->as.function.return_type;
                        ret_class = ctx->current_function->as.function.return_class_name;
                        ret_is_array = ctx->current_function->as.function.return_is_array;
                        ret_heap = ctx->current_function->as.function.returns_heap_pointer;
                    } else if (ctx->current_function->type == NODE_METHOD) {
                        ret_type = ctx->current_function->as.method.return_type;
                        ret_class = ctx->current_function->as.method.return_class_name;
                        ret_is_array = ctx->current_function->as.method.return_is_array;
                        ret_heap = ctx->current_function->as.method.returns_heap_pointer;
                    }
                }

                if (stmt->frees_count > 0 || stmt->releases_count > 0) {
                    emit_indent(ctx);
                    const char *ret_type_str = c_type_str_decl(ctx, ret_type, ret_class, ret_is_array, ret_heap);
                    sb_appendf(&ctx->sb, "%s __cco_ret_val = ", ret_type_str);
                    if (ret_type == TY_STRING && !ret_is_array && stmt->as.return_stmt.value->type == NODE_LITERAL) {
                        sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.return_stmt.value->as.literal.val.s);
                    } else {
                        gen_expr(ctx, stmt->as.return_stmt.value);
                    }
                    sb_append(&ctx->sb, ";\n");

                    emit_frees(ctx, stmt);
                    emit_releases(ctx, stmt);

                    emit_indent(ctx);
                    sb_append(&ctx->sb, "return __cco_ret_val;\n");
                } else {
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "return ");
                    if (ret_type == TY_STRING && !ret_is_array && stmt->as.return_stmt.value->type == NODE_LITERAL) {
                        sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.return_stmt.value->as.literal.val.s);
                    } else {
                        gen_expr(ctx, stmt->as.return_stmt.value);
                    }
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
            int total = stmt->as.print_stmt.value_count;
            if (total == 0 && !stmt->as.print_stmt.value) {
                emit_indent(ctx);
                sb_append(&ctx->sb, "printf(\"\\n\");\n");
                break;
            }
            if (total <= 1) {
                emit_indent(ctx);
                AstNode *val = stmt->as.print_stmt.value ? stmt->as.print_stmt.value : stmt->as.print_stmt.values[0];
                Type t = infer_expr_type(ctx->program, ctx->current_function, val);
                if (t == TY_STRING) {
                    if (val->type == NODE_FSTRING || (val->type == NODE_CALL && is_heap_string_returning_call(val->as.call.callee))) {
                        static int print_tmp_counter = 0;
                        int cur_tmp = ++print_tmp_counter;
                        sb_appendf(&ctx->sb, "{\n");
                        ctx->indent_level++;
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "char *__print_tmp_%d = ", cur_tmp);
                        gen_expr(ctx, val);
                        sb_append(&ctx->sb, ";\n");
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "printf(\"%%s\\n\", __print_tmp_%d ? __print_tmp_%d : \"\");\n", cur_tmp, cur_tmp);
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "free(__print_tmp_%d);\n", cur_tmp);
                        ctx->indent_level--;
                        emit_indent(ctx);
                        sb_append(&ctx->sb, "}\n");
                    } else {
                        sb_append(&ctx->sb, "printf(\"%s\\n\", ");
                        gen_expr(ctx, val);
                        sb_append(&ctx->sb, ");\n");
                    }
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
                emit_nulls(ctx, stmt);
                break;
            }

            for (int i = 0; i < total; i++) {
                emit_indent(ctx);
                AstNode *val = stmt->as.print_stmt.values[i];
                Type t = infer_expr_type(ctx->program, ctx->current_function, val);
                if (t == TY_STRING) {
                    if (val->type == NODE_FSTRING || (val->type == NODE_CALL && is_heap_string_returning_call(val->as.call.callee))) {
                        static int print_tmp_counter = 0;
                        int cur_tmp = ++print_tmp_counter;
                        sb_appendf(&ctx->sb, "{\n");
                        ctx->indent_level++;
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "char *__print_tmp_%d = ", cur_tmp);
                        gen_expr(ctx, val);
                        sb_append(&ctx->sb, ";\n");
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "printf(\"%%s\", __print_tmp_%d ? __print_tmp_%d : \"\");\n", cur_tmp, cur_tmp);
                        emit_indent(ctx);
                        sb_appendf(&ctx->sb, "free(__print_tmp_%d);\n", cur_tmp);
                        ctx->indent_level--;
                        emit_indent(ctx);
                        sb_append(&ctx->sb, "}\n");
                    } else {
                        sb_append(&ctx->sb, "printf(\"%s\", ");
                        gen_expr(ctx, val);
                        sb_append(&ctx->sb, ");\n");
                    }
                } else if (t == TY_FLOAT) {
                    sb_append(&ctx->sb, "printf(\"%g\", (double)(");
                    gen_expr(ctx, val);
                    sb_append(&ctx->sb, "));\n");
                } else if (t == TY_BOOL) {
                    sb_append(&ctx->sb, "printf(\"%s\", (");
                    gen_expr(ctx, val);
                    sb_append(&ctx->sb, ") ? \"true\" : \"false\");\n");
                } else if (t == TY_CHAR) {
                    sb_append(&ctx->sb, "printf(\"%c\", ");
                    gen_expr(ctx, val);
                    sb_append(&ctx->sb, ");\n");
                } else {
                    sb_append(&ctx->sb, "printf(\"%ld\", (long)(");
                    gen_expr(ctx, val);
                    sb_append(&ctx->sb, "));\n");
                }
                if (i < total - 1) {
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "printf(\" \");\n");
                }
            }
            emit_indent(ctx);
            sb_append(&ctx->sb, "printf(\"\\n\");\n");
            emit_nulls(ctx, stmt);
            break;
        }

        case NODE_MATCH: {
            AstNode *scrut = stmt->as.match_stmt.expr;
            const char *ename = stmt->as.match_stmt.enum_name;
            if (!ename && scrut->type == NODE_IDENT) {
                ename = get_expr_elem_class_name(ctx, scrut);
            }
            if (!ename) {
                for (int a = 0; a < stmt->as.match_stmt.arm_count; a++) {
                    if (!stmt->as.match_stmt.arms[a]->as.match_arm.is_wildcard) {
                        ename = stmt->as.match_stmt.arms[a]->as.match_arm.enum_name;
                        break;
                    }
                }
            }

            char *upper_ename = NULL;
            if (ename) {
                upper_ename = strdup(ename);
                for (int c = 0; upper_ename[c]; c++) {
                    upper_ename[c] = (char)toupper((unsigned char)upper_ename[c]);
                }
            }

            static int match_tmp_counter = 0;
            int cur_tmp = ++match_tmp_counter;

            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s *__match_scrut_%d = ", ename ? ename : "void", cur_tmp);
            gen_expr(ctx, scrut);
            sb_append(&ctx->sb, ";\n");

            emit_indent(ctx);
            sb_appendf(&ctx->sb, "switch (__match_scrut_%d->tag) {\n", cur_tmp);
            ctx->indent_level++;

            EnumDef *edef = ename ? find_enum(ctx->ct, ename) : NULL;

            for (int a = 0; a < stmt->as.match_stmt.arm_count; a++) {
                AstNode *arm = stmt->as.match_stmt.arms[a];
                emit_indent(ctx);
                if (arm->as.match_arm.is_wildcard) {
                    sb_append(&ctx->sb, "default: {\n");
                } else {
                    sb_appendf(&ctx->sb, "case %s_TAG_%s: {\n", upper_ename ? upper_ename : "ENUM", arm->as.match_arm.variant_name);
                }
                ctx->indent_level++;

                if (!arm->as.match_arm.is_wildcard && edef) {
                    EnumVariantDef *vdef = find_enum_variant(edef, arm->as.match_arm.variant_name);
                    if (vdef && vdef->field_count > 0) {
                        for (int b = 0; b < arm->as.match_arm.bind_count; b++) {
                            const char *bname = arm->as.match_arm.bind_names[b];
                            FieldInfo *fi = find_variant_field(vdef, bname);
                            if (fi) {
                                emit_indent(ctx);
                                const char *ftype_str = c_type_str_full(ctx, fi->type, fi->class_name, false, false);
                                sb_appendf(&ctx->sb, "%s %s = __match_scrut_%d->as.%s.%s;\n",
                                           ftype_str, bname, cur_tmp, arm->as.match_arm.variant_name, bname);
                                emit_indent(ctx);
                                sb_appendf(&ctx->sb, "(void)%s;\n", bname);
                            }
                        }
                    }
                }

                AstNode *arm_body = arm->as.match_arm.body;
                if (arm_body && arm_body->type == NODE_BLOCK) {
                    for (int s = 0; s < arm_body->as.block.count; s++) {
                        gen_stmt(ctx, arm_body->as.block.stmts[s]);
                    }
                    emit_frees(ctx, arm_body);
                    emit_releases(ctx, arm_body);
                }

                emit_indent(ctx);
                sb_append(&ctx->sb, "break;\n");
                ctx->indent_level--;
                emit_indent(ctx);
                sb_append(&ctx->sb, "}\n");
            }

            ctx->indent_level--;
            emit_indent(ctx);
            sb_append(&ctx->sb, "}\n");

            if (upper_ename) free(upper_ename);
            break;
        }

        case NODE_EXPR_STMT:
            emit_indent(ctx);
            gen_expr(ctx, stmt->as.expr_stmt.expr);
            sb_append(&ctx->sb, ";\n");
            emit_nulls(ctx, stmt);
            break;

        case NODE_BLOCK:
            gen_block(ctx, stmt);
            break;

        default:
            break;
    }
}

static void gen_block(CodegenCtx *ctx, AstNode *block_node) {
    if (!block_node) return;
    sb_append(&ctx->sb, "{\n");
    ctx->indent_level++;

    if (ctx->current_function && ctx->current_function->type == NODE_FUNCTION &&
        strcmp(ctx->current_function->as.function.name, "main") == 0 &&
        ctx->current_function->as.function.body == block_node) {
        emit_indent(ctx);
        sb_append(&ctx->sb, "__cco_argc = __cco_main_argc;\n");
        emit_indent(ctx);
        sb_append(&ctx->sb, "__cco_argv = __cco_main_argv;\n");
    }

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
        if (ctx->current_function) {
            bool is_fn = (ctx->current_function->type == NODE_FUNCTION && ctx->current_function->as.function.body == block_node);
            bool is_meth = (ctx->current_function->type == NODE_METHOD && ctx->current_function->as.method.body == block_node);
            if (is_fn) {
                if (strcmp(ctx->current_function->as.function.name, "main") == 0) {
                    emit_indent(ctx);
                    sb_append(&ctx->sb, "return 0;\n");
                } else if (ctx->current_function->as.function.return_type != TY_VOID) {
                    emit_indent(ctx);
                    Type rt = ctx->current_function->as.function.return_type;
                    if (rt == TY_INT || rt == TY_BOOL || rt == TY_CHAR) {
                        sb_append(&ctx->sb, "return 0;\n");
                    } else if (rt == TY_FLOAT) {
                        sb_append(&ctx->sb, "return 0.0;\n");
                    } else if (rt == TY_CLASS && is_struct_name(ctx, ctx->current_function->as.function.return_class_name)) {
                        sb_appendf(&ctx->sb, "return (%s){0};\n", ctx->current_function->as.function.return_class_name);
                    } else {
                        sb_append(&ctx->sb, "return NULL;\n");
                    }
                }
            } else if (is_meth && ctx->current_function->as.method.return_type != TY_VOID) {
                emit_indent(ctx);
                Type rt = ctx->current_function->as.method.return_type;
                if (rt == TY_INT || rt == TY_BOOL || rt == TY_CHAR) {
                    sb_append(&ctx->sb, "return 0;\n");
                } else if (rt == TY_FLOAT) {
                    sb_append(&ctx->sb, "return 0.0;\n");
                } else if (rt == TY_CLASS && is_struct_name(ctx, ctx->current_function->as.method.return_class_name)) {
                    sb_appendf(&ctx->sb, "return (%s){0};\n", ctx->current_function->as.method.return_class_name);
                } else {
                    sb_append(&ctx->sb, "return NULL;\n");
                }
            }
        }
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

static void gen_enum_helpers(CodegenCtx *ctx, AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) return;

    // 1. Tag enums and forward typedefs
    for (int i = 0; i < program->as.program.enum_count; i++) {
        AstNode *en = program->as.program.enums[i];
        const char *ename = en->as.enum_decl.name;
        char *upper_ename = strdup(ename);
        for (int c = 0; upper_ename[c]; c++) {
            upper_ename[c] = (char)toupper((unsigned char)upper_ename[c]);
        }

        sb_appendf(&ctx->sb, "typedef enum {\n");
        for (int v = 0; v < en->as.enum_decl.variant_count; v++) {
            AstNode *vn = en->as.enum_decl.variants[v];
            sb_appendf(&ctx->sb, "    %s_TAG_%s%s\n", upper_ename, vn->as.variant_decl.name, (v + 1 < en->as.enum_decl.variant_count) ? "," : "");
        }
        sb_appendf(&ctx->sb, "} __cco_%s_tag;\n\n", ename);
        sb_appendf(&ctx->sb, "typedef struct %s %s;\n\n", ename, ename);
        free(upper_ename);
    }

    // 2. Struct definitions with unions
    for (int i = 0; i < program->as.program.enum_count; i++) {
        AstNode *en = program->as.program.enums[i];
        const char *ename = en->as.enum_decl.name;

        sb_appendf(&ctx->sb, "struct %s {\n", ename);
        sb_appendf(&ctx->sb, "    __cco_%s_tag tag;\n", ename);
        sb_append(&ctx->sb, "    union {\n");

        bool has_payload = false;
        for (int v = 0; v < en->as.enum_decl.variant_count; v++) {
            AstNode *vn = en->as.enum_decl.variants[v];
            if (!vn->as.variant_decl.is_unit && vn->as.variant_decl.field_count > 0) {
                has_payload = true;
                sb_appendf(&ctx->sb, "        struct {\n");
                for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                    AstNode *fn = vn->as.variant_decl.fields[f];
                    sb_appendf(&ctx->sb, "            %s %s;\n",
                               c_type_str_full(ctx, fn->as.field.type, fn->as.field.class_name, false, false),
                               fn->as.field.name);
                }
                sb_appendf(&ctx->sb, "        } %s;\n", vn->as.variant_decl.name);
            }
        }
        if (!has_payload) {
            sb_append(&ctx->sb, "        int _dummy;\n");
        }
        sb_append(&ctx->sb, "    } as;\n");
        sb_append(&ctx->sb, "};\n\n");
    }

    // 3. Destructors
    for (int i = 0; i < program->as.program.enum_count; i++) {
        AstNode *en = program->as.program.enums[i];
        const char *ename = en->as.enum_decl.name;
        char *upper_ename = strdup(ename);
        for (int c = 0; upper_ename[c]; c++) {
            upper_ename[c] = (char)toupper((unsigned char)upper_ename[c]);
        }

        sb_appendf(&ctx->sb, "static inline void %s_free(%s *p) {\n", ename, ename);
        sb_append(&ctx->sb, "    if (!p) return;\n");
        sb_append(&ctx->sb, "    switch (p->tag) {\n");
        for (int v = 0; v < en->as.enum_decl.variant_count; v++) {
            AstNode *vn = en->as.enum_decl.variants[v];
            sb_appendf(&ctx->sb, "        case %s_TAG_%s:\n", upper_ename, vn->as.variant_decl.name);
            if (!vn->as.variant_decl.is_unit && vn->as.variant_decl.field_count > 0) {
                for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                    AstNode *fn = vn->as.variant_decl.fields[f];
                    if (fn->as.field.type == TY_CLASS && fn->as.field.class_name) {
                        if (is_struct_name(ctx, fn->as.field.class_name)) {
                            // Value struct: no free needed
                        } else {
                            // Class or Enum pointer
                            sb_appendf(&ctx->sb, "            %s_free(p->as.%s.%s);\n", fn->as.field.class_name, vn->as.variant_decl.name, fn->as.field.name);
                        }
                    } else if (fn->as.field.type == TY_STRING) {
                        sb_appendf(&ctx->sb, "            if (p->as.%s.%s) free(p->as.%s.%s);\n", vn->as.variant_decl.name, fn->as.field.name, vn->as.variant_decl.name, fn->as.field.name);
                    }
                }
            }
            sb_append(&ctx->sb, "            break;\n");
        }
        sb_append(&ctx->sb, "    }\n");
        sb_append(&ctx->sb, "    free(p);\n");
        sb_append(&ctx->sb, "}\n\n");
        free(upper_ename);
    }

    // 4. Variant Constructor helper functions
    for (int i = 0; i < program->as.program.enum_count; i++) {
        AstNode *en = program->as.program.enums[i];
        const char *ename = en->as.enum_decl.name;
        char *upper_ename = strdup(ename);
        for (int c = 0; upper_ename[c]; c++) {
            upper_ename[c] = (char)toupper((unsigned char)upper_ename[c]);
        }

        for (int v = 0; v < en->as.enum_decl.variant_count; v++) {
            AstNode *vn = en->as.enum_decl.variants[v];
            const char *vname = vn->as.variant_decl.name;

            sb_appendf(&ctx->sb, "static inline %s *%s_new_%s(", ename, ename, vname);
            if (vn->as.variant_decl.is_unit || vn->as.variant_decl.field_count == 0) {
                sb_append(&ctx->sb, "void");
            } else {
                for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                    if (f > 0) sb_append(&ctx->sb, ", ");
                    AstNode *fn = vn->as.variant_decl.fields[f];
                    sb_appendf(&ctx->sb, "%s %s",
                               c_type_str_full(ctx, fn->as.field.type, fn->as.field.class_name, false, false),
                               fn->as.field.name);
                    if (fn->as.field.type == TY_STRING && !fn->as.field.is_array && !fn->as.field.is_map) {
                        sb_appendf(&ctx->sb, ", int %s_is_owned", fn->as.field.name);
                    }
                }
            }
            sb_append(&ctx->sb, ") {\n");
            sb_appendf(&ctx->sb, "    %s *p = (%s *)calloc(1, sizeof(%s));\n", ename, ename, ename);
            sb_appendf(&ctx->sb, "    if (!p) {\n");
            sb_appendf(&ctx->sb, "        fprintf(stderr, \"Memory allocation failed in %s_new_%s()\\n\");\n", ename, vname);
            sb_append(&ctx->sb, "        exit(1);\n");
            sb_append(&ctx->sb, "    }\n");
            sb_appendf(&ctx->sb, "    p->tag = %s_TAG_%s;\n", upper_ename, vname);
            if (!vn->as.variant_decl.is_unit && vn->as.variant_decl.field_count > 0) {
                for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                    AstNode *fn = vn->as.variant_decl.fields[f];
                    const char *fname = fn->as.field.name;
                    if (fn->as.field.type == TY_STRING && !fn->as.field.is_array && !fn->as.field.is_map) {
                        sb_appendf(&ctx->sb, "    if (%s) {\n", fname);
                        sb_appendf(&ctx->sb, "        if (%s_is_owned) {\n", fname);
                        sb_appendf(&ctx->sb, "            p->as.%s.%s = %s;\n", vname, fname, fname);
                        sb_appendf(&ctx->sb, "        } else {\n");
                        sb_appendf(&ctx->sb, "            p->as.%s.%s = strdup(%s);\n", vname, fname, fname);
                        sb_appendf(&ctx->sb, "            if (!p->as.%s.%s) {\n", vname, fname);
                        sb_appendf(&ctx->sb, "                %s_free(p);\n", ename);
                        sb_appendf(&ctx->sb, "                fprintf(stderr, \"Memory allocation failed in strdup() for %s.%s.%s\\n\");\n", ename, vname, fname);
                        sb_append(&ctx->sb, "                exit(1);\n");
                        sb_append(&ctx->sb, "            }\n");
                        sb_append(&ctx->sb, "        }\n");
                        sb_append(&ctx->sb, "    }\n");
                    } else {
                        sb_appendf(&ctx->sb, "    p->as.%s.%s = %s;\n", vname, fname, fname);
                    }
                }
            }
            sb_append(&ctx->sb, "    return p;\n");
            sb_append(&ctx->sb, "}\n\n");
        }
        free(upper_ename);
    }
}

static bool has_map_array_field_for_class(AstNode *program, const char *class_name) {
    if (!program || program->type != NODE_PROGRAM || !class_name) return false;
    for (int c = 0; c < program->as.program.class_count; c++) {
        AstNode *cls = program->as.program.classes[c];
        for (int f = 0; f < cls->as.class_decl.field_count; f++) {
            AstNode *fld = cls->as.class_decl.fields[f];
            if (fld->as.field.is_map && fld->as.field.is_array && fld->as.field.class_name && strcmp(fld->as.field.class_name, class_name) == 0) {
                return true;
            }
        }
    }
    return false;
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
            const char *fld_type = f_node->as.field.type_desc ?
                c_type_str_desc(ctx, f_node->as.field.type_desc) :
                c_type_str_decl_full(ctx, f_node->as.field.type, f_node->as.field.class_name, f_node->as.field.is_array, f_node->as.field.is_map, f_node->is_heap_owner);
            sb_appendf(&ctx->sb, "    %s %s;\n", fld_type, f_node->as.field.name);
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
            if (f_node->as.field.type_desc) {
                char fld_name[256];
                snprintf(fld_name, sizeof(fld_name), "p->%s", f_node->as.field.name);
                sb_append(&ctx->sb, "        ");
                emit_typedesc_cleanup(ctx, fld_name, f_node->as.field.type_desc);
                continue;
            }
            if (f_node->as.field.is_map) {
                if (f_node->as.field.is_array && f_node->as.field.type == TY_CLASS && f_node->as.field.class_name) {
                    sb_appendf(&ctx->sb, "        if (p->%s) __cco_map_free(p->%s, (__cco_val_free_fn)__cco_free_arr_of_%s);\n", f_node->as.field.name, f_node->as.field.name, f_node->as.field.class_name);
                } else if ((f_node->as.field.type == TY_CLASS || f_node->as.field.type == TY_ENUM) && f_node->as.field.class_name && !f_node->as.field.is_array) {
                    sb_appendf(&ctx->sb, "        if (p->%s) __cco_map_free(p->%s, (__cco_val_free_fn)%s_free);\n", f_node->as.field.name, f_node->as.field.name, f_node->as.field.class_name);
                } else if (f_node->as.field.is_array) {
                    sb_appendf(&ctx->sb, "        if (p->%s) __cco_map_free(p->%s, (__cco_val_free_fn)__cco_free_arr);\n", f_node->as.field.name, f_node->as.field.name);
                } else {
                    sb_appendf(&ctx->sb, "        if (p->%s) __cco_map_free(p->%s, NULL);\n", f_node->as.field.name, f_node->as.field.name);
                }
            } else if (f_node->as.field.type == TY_CLASS && f_node->as.field.class_name && !f_node->as.field.is_array) {
                sb_appendf(&ctx->sb, "        %s_free(p->%s);\n", f_node->as.field.class_name, f_node->as.field.name);
            } else if (f_node->as.field.is_array) {
                if ((f_node->as.field.type == TY_CLASS || f_node->as.field.type == TY_ENUM) && f_node->as.field.class_name) {
                    sb_appendf(&ctx->sb, "        if (p->%s != NULL) {\n", f_node->as.field.name);
                    sb_appendf(&ctx->sb, "            for (int __i = 0; __i < __cco_arr_len(p->%s); __i++) {\n", f_node->as.field.name);
                    sb_appendf(&ctx->sb, "                if (p->%s[__i] != NULL) { %s_free(p->%s[__i]); }\n", f_node->as.field.name, f_node->as.field.class_name, f_node->as.field.name);
                    sb_appendf(&ctx->sb, "            }\n");
                    sb_appendf(&ctx->sb, "            __cco_free_arr(p->%s);\n", f_node->as.field.name);
                    sb_appendf(&ctx->sb, "        }\n");
                } else if (f_node->as.field.type == TY_STRING) {
                    sb_appendf(&ctx->sb, "        if (p->%s != NULL) {\n", f_node->as.field.name);
                    sb_appendf(&ctx->sb, "            for (int __i = 0; __i < __cco_arr_len(p->%s); __i++) {\n", f_node->as.field.name);
                    sb_appendf(&ctx->sb, "                if (p->%s[__i] != NULL) { free(p->%s[__i]); }\n", f_node->as.field.name, f_node->as.field.name);
                    sb_appendf(&ctx->sb, "            }\n");
                    sb_appendf(&ctx->sb, "            __cco_free_arr(p->%s);\n", f_node->as.field.name);
                    sb_appendf(&ctx->sb, "        }\n");
                } else {
                    sb_appendf(&ctx->sb, "        if (p->%s) __cco_free_arr(p->%s);\n", f_node->as.field.name, f_node->as.field.name);
                }
            } else if (f_node->is_heap_owner || f_node->as.field.type == TY_STRING) {
                sb_appendf(&ctx->sb, "        if (p->%s) free(p->%s);\n", f_node->as.field.name, f_node->as.field.name);
            }
        }
        sb_append(&ctx->sb, "        free(p);\n");
        sb_append(&ctx->sb, "    }\n");
        sb_append(&ctx->sb, "}\n\n");

        if (has_map_array_field_for_class(program, cname)) {
            sb_appendf(&ctx->sb, "static inline void __cco_free_arr_of_%s(void *arr) {\n", cname);
            sb_append(&ctx->sb, "    if (arr) {\n");
            sb_appendf(&ctx->sb, "        for (int __i = 0; __i < __cco_arr_len(arr); __i++) {\n");
            sb_appendf(&ctx->sb, "            %s *__item = ((%s **)arr)[__i];\n", cname, cname);
            sb_appendf(&ctx->sb, "            if (__item) %s_free(__item);\n", cname);
            sb_append(&ctx->sb, "        }\n");
            sb_append(&ctx->sb, "        __cco_free_arr(arr);\n");
            sb_append(&ctx->sb, "    }\n");
            sb_append(&ctx->sb, "}\n\n");
        }

        sb_appendf(&ctx->sb, "static inline %s *%s_new(", cname, cname);
        if (cls->as.class_decl.field_count == 0) {
            sb_append(&ctx->sb, "void");
        } else {
            for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                if (f > 0) sb_append(&ctx->sb, ", ");
                AstNode *f_node = cls->as.class_decl.fields[f];
                const char *fld_type = f_node->as.field.type_desc ?
                    c_type_str_desc(ctx, f_node->as.field.type_desc) :
                    c_type_str_decl_full(ctx, f_node->as.field.type, f_node->as.field.class_name, f_node->as.field.is_array, f_node->as.field.is_map, f_node->is_heap_owner);
                sb_appendf(&ctx->sb, "%s %s", fld_type, f_node->as.field.name);
                if (f_node->as.field.type == TY_STRING && !f_node->as.field.is_array && !f_node->as.field.is_map) {
                    sb_appendf(&ctx->sb, ", int %s_is_owned", f_node->as.field.name);
                }
            }
        }
        sb_append(&ctx->sb, ") {\n");
        sb_appendf(&ctx->sb, "    %s *__obj = (%s *)calloc(1, sizeof(%s));\n", cname, cname, cname);
        sb_appendf(&ctx->sb, "    if (!__obj) {\n");
        sb_appendf(&ctx->sb, "        fprintf(stderr, \"Memory allocation failed in %s_new()\\n\");\n", cname);
        sb_append(&ctx->sb, "        exit(1);\n");
        sb_append(&ctx->sb, "    }\n");
        for (int f = 0; f < cls->as.class_decl.field_count; f++) {
            AstNode *f_node = cls->as.class_decl.fields[f];
            const char *fname = f_node->as.field.name;
            if (f_node->as.field.type == TY_STRING && !f_node->as.field.is_array && !f_node->as.field.is_map) {
                sb_appendf(&ctx->sb, "    if (%s) {\n", fname);
                sb_appendf(&ctx->sb, "        if (%s_is_owned) {\n", fname);
                sb_appendf(&ctx->sb, "            __obj->%s = %s;\n", fname, fname);
                sb_appendf(&ctx->sb, "        } else {\n");
                sb_appendf(&ctx->sb, "            __obj->%s = strdup(%s);\n", fname, fname);
                sb_appendf(&ctx->sb, "            if (!__obj->%s) {\n", fname);
                sb_appendf(&ctx->sb, "                %s_free(__obj);\n", cname);
                sb_appendf(&ctx->sb, "                fprintf(stderr, \"Memory allocation failed in strdup() for %s.%s\\n\");\n", cname, fname);
                sb_append(&ctx->sb, "                exit(1);\n");
                sb_append(&ctx->sb, "            }\n");
                sb_append(&ctx->sb, "        }\n");
                sb_append(&ctx->sb, "    }\n");
            } else {
                sb_appendf(&ctx->sb, "    __obj->%s = %s;\n", fname, fname);
            }
        }
        sb_append(&ctx->sb, "    return __obj;\n");
        sb_append(&ctx->sb, "}\n\n");
    }
}

static void collect_and_emit_helpers_for_typedesc(CodegenCtx *ctx, const TypeDesc *td) {
    if (!td) return;
    if (td->kind == TYPEDESC_MAP) {
        TypeDesc *v = td->as.map_desc.val_type;
        if (v && v->kind == TYPEDESC_MAP) {
            collect_and_emit_helpers_for_typedesc(ctx, v);
            TypeDesc *ik = v->as.map_desc.key_type;
            TypeDesc *iv = v->as.map_desc.val_type;
            const char *iks = (ik && ik->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int";
            const char *ivs = (iv && iv->kind != TYPEDESC_PRIMITIVE) ? iv->as.named_desc.name : ((iv && iv->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int");
            char helper_name[128];
            snprintf(helper_name, sizeof(helper_name), "__cco_free_map_%s_%s", iks, ivs);
            if (!helper_already_emitted(ctx, helper_name)) {
                mark_helper_emitted(ctx, helper_name);
                sb_appendf(&ctx->sb, "static inline void %s(void *m) {\n", helper_name);
                if (iv && iv->kind != TYPEDESC_PRIMITIVE) {
                    sb_appendf(&ctx->sb, "    if (m) __cco_map_free((__cco_map *)m, (__cco_val_free_fn)%s_free);\n", iv->as.named_desc.name);
                } else if (iv && iv->as.primitive_desc.primitive_kind == TY_STRING) {
                    sb_appendf(&ctx->sb, "    if (m) __cco_map_free((__cco_map *)m, (__cco_val_free_fn)free);\n");
                } else {
                    sb_appendf(&ctx->sb, "    if (m) __cco_map_free((__cco_map *)m, NULL);\n");
                }
                sb_append(&ctx->sb, "}\n\n");
            }
        } else if (v && v->kind == TYPEDESC_ARRAY) {
            collect_and_emit_helpers_for_typedesc(ctx, v);
            TypeDesc *elem = v->as.array_desc.elem_type;
            if (elem && elem->kind == TYPEDESC_MAP) {
                TypeDesc *ik = elem->as.map_desc.key_type;
                TypeDesc *iv = elem->as.map_desc.val_type;
                const char *iks = (ik && ik->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int";
                const char *ivs = (iv && iv->kind != TYPEDESC_PRIMITIVE) ? iv->as.named_desc.name : ((iv && iv->as.primitive_desc.primitive_kind == TY_STRING) ? "string" : "int");
                char helper_name[128];
                snprintf(helper_name, sizeof(helper_name), "__cco_free_arr_of_map_%s_%s", iks, ivs);
                if (!helper_already_emitted(ctx, helper_name)) {
                    mark_helper_emitted(ctx, helper_name);
                    sb_appendf(&ctx->sb, "static inline void %s(void *arr) {\n", helper_name);
                    sb_append(&ctx->sb, "    if (arr) {\n");
                    sb_append(&ctx->sb, "        __cco_map **__marr = (__cco_map **)arr;\n");
                    sb_append(&ctx->sb, "        for (int __i = 0; __i < __cco_arr_len(__marr); __i++) {\n");
                    if (iv && iv->kind != TYPEDESC_PRIMITIVE) {
                        sb_appendf(&ctx->sb, "            if (__marr[__i]) __cco_map_free(__marr[__i], (__cco_val_free_fn)%s_free);\n", iv->as.named_desc.name);
                    } else if (iv && iv->as.primitive_desc.primitive_kind == TY_STRING) {
                        sb_appendf(&ctx->sb, "            if (__marr[__i]) __cco_map_free(__marr[__i], (__cco_val_free_fn)free);\n");
                    } else {
                        sb_appendf(&ctx->sb, "            if (__marr[__i]) __cco_map_free(__marr[__i], NULL);\n");
                    }
                    sb_append(&ctx->sb, "        }\n");
                    sb_append(&ctx->sb, "        __cco_free_arr(arr);\n");
                    sb_append(&ctx->sb, "    }\n");
                    sb_append(&ctx->sb, "}\n\n");
                }
            } else if (elem && (elem->kind == TYPEDESC_CLASS || elem->kind == TYPEDESC_ENUM || elem->kind == TYPEDESC_NAMED)) {
                const char *cname = elem->as.named_desc.name;
                char helper_name[128];
                snprintf(helper_name, sizeof(helper_name), "__cco_free_arr_of_%s", cname);
                if (!helper_already_emitted(ctx, helper_name)) {
                    mark_helper_emitted(ctx, helper_name);
                    sb_appendf(&ctx->sb, "static inline void %s(void *arr) {\n", helper_name);
                    sb_append(&ctx->sb, "    if (arr) {\n");
                    sb_appendf(&ctx->sb, "        %s **__items = (%s **)arr;\n", cname, cname);
                    sb_append(&ctx->sb, "        for (int __i = 0; __i < __cco_arr_len(__items); __i++) {\n");
                    sb_appendf(&ctx->sb, "            if (__items[__i]) %s_free(__items[__i]);\n", cname);
                    sb_append(&ctx->sb, "        }\n");
                    sb_append(&ctx->sb, "        __cco_free_arr(arr);\n");
                    sb_append(&ctx->sb, "    }\n");
                    sb_append(&ctx->sb, "}\n\n");
                }
            } else if (elem && elem->kind == TYPEDESC_PRIMITIVE && elem->as.primitive_desc.primitive_kind == TY_STRING) {
                char helper_name[128];
                snprintf(helper_name, sizeof(helper_name), "__cco_free_arr_of_string");
                if (!helper_already_emitted(ctx, helper_name)) {
                    mark_helper_emitted(ctx, helper_name);
                    sb_append(&ctx->sb, "static inline void __cco_free_arr_of_string(void *arr) {\n");
                    sb_append(&ctx->sb, "    if (arr) {\n");
                    sb_append(&ctx->sb, "        char **__sarr = (char **)arr;\n");
                    sb_append(&ctx->sb, "        for (int __i = 0; __i < __cco_arr_len(__sarr); __i++) {\n");
                    sb_append(&ctx->sb, "            if (__sarr[__i]) free(__sarr[__i]);\n");
                    sb_append(&ctx->sb, "        }\n");
                    sb_append(&ctx->sb, "        __cco_free_arr(arr);\n");
                    sb_append(&ctx->sb, "    }\n");
                    sb_append(&ctx->sb, "}\n\n");
                }
            }
        }
    } else if (td->kind == TYPEDESC_ARRAY) {
        collect_and_emit_helpers_for_typedesc(ctx, td->as.array_desc.elem_type);
    }
}

static void scan_all_typedescs(CodegenCtx *ctx, AstNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.class_count; i++) scan_all_typedescs(ctx, node->as.program.classes[i]);
            for (int i = 0; i < node->as.program.enum_count; i++) scan_all_typedescs(ctx, node->as.program.enums[i]);
            for (int i = 0; i < node->as.program.count; i++) scan_all_typedescs(ctx, node->as.program.functions[i]);
            break;
        case NODE_CLASS:
            for (int i = 0; i < node->as.class_decl.field_count; i++) {
                if (node->as.class_decl.fields[i]->as.field.type_desc) {
                    collect_and_emit_helpers_for_typedesc(ctx, node->as.class_decl.fields[i]->as.field.type_desc);
                }
            }
            for (int i = 0; i < node->as.class_decl.method_count; i++) scan_all_typedescs(ctx, node->as.class_decl.methods[i]);
            break;
        case NODE_ENUM:
            break;
        case NODE_FUNCTION:
            if (node->as.function.return_type_desc) collect_and_emit_helpers_for_typedesc(ctx, node->as.function.return_type_desc);
            if (node->as.function.param_type_descs) {
                for (int i = 0; i < node->as.function.param_count; i++) {
                    if (node->as.function.param_type_descs[i]) collect_and_emit_helpers_for_typedesc(ctx, node->as.function.param_type_descs[i]);
                }
            }
            scan_all_typedescs(ctx, node->as.function.body);
            break;
        case NODE_METHOD:
            if (node->as.method.return_type_desc) collect_and_emit_helpers_for_typedesc(ctx, node->as.method.return_type_desc);
            if (node->as.method.param_type_descs) {
                for (int i = 0; i < node->as.method.param_count; i++) {
                    if (node->as.method.param_type_descs[i]) collect_and_emit_helpers_for_typedesc(ctx, node->as.method.param_type_descs[i]);
                }
            }
            scan_all_typedescs(ctx, node->as.method.body);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) scan_all_typedescs(ctx, node->as.block.stmts[i]);
            break;
        case NODE_LET:
            if (node->as.let.type_desc) collect_and_emit_helpers_for_typedesc(ctx, node->as.let.type_desc);
            scan_all_typedescs(ctx, node->as.let.value);
            break;
        case NODE_ASSIGN:
            if (node->as.assign.type_desc) collect_and_emit_helpers_for_typedesc(ctx, node->as.assign.type_desc);
            scan_all_typedescs(ctx, node->as.assign.value);
            break;
        case NODE_ALLOC:
            if (node->as.alloc.type_desc) collect_and_emit_helpers_for_typedesc(ctx, node->as.alloc.type_desc);
            break;
        case NODE_IF:
            scan_all_typedescs(ctx, node->as.if_stmt.cond);
            scan_all_typedescs(ctx, node->as.if_stmt.then_b);
            scan_all_typedescs(ctx, node->as.if_stmt.else_b);
            break;
        case NODE_WHILE:
            scan_all_typedescs(ctx, node->as.while_stmt.cond);
            scan_all_typedescs(ctx, node->as.while_stmt.body);
            break;
        case NODE_FOR:
            scan_all_typedescs(ctx, node->as.for_stmt.init);
            scan_all_typedescs(ctx, node->as.for_stmt.cond);
            scan_all_typedescs(ctx, node->as.for_stmt.step);
            scan_all_typedescs(ctx, node->as.for_stmt.body);
            break;
        case NODE_MATCH:
            scan_all_typedescs(ctx, node->as.match_stmt.expr);
            for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
                scan_all_typedescs(ctx, node->as.match_stmt.arms[i]->as.match_arm.body);
            }
            break;
        case NODE_CALL:
            for (int i = 0; i < node->as.call.arg_count; i++) scan_all_typedescs(ctx, node->as.call.args[i]);
            break;
        default:
            break;
    }
}

static void gen_recursive_cleanup_helpers(CodegenCtx *ctx, AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) return;
    scan_all_typedescs(ctx, program);
}

static void gen_method(CodegenCtx *ctx, const char *class_name, AstNode *m_node) {
    ctx->current_function = m_node;
    const char *ret_type_str = m_node->as.method.return_type_desc ?
        c_type_str_desc(ctx, m_node->as.method.return_type_desc) :
        c_type_str_decl_full(ctx, m_node->as.method.return_type, m_node->as.method.return_class_name, m_node->as.method.return_is_array, m_node->as.method.return_is_map, m_node->as.method.returns_heap_pointer);
    sb_appendf(&ctx->sb, "%s %s_%s(",
               ret_type_str,
               class_name, m_node->as.method.name);

    if (m_node->as.method.param_count == 0) {
        sb_append(&ctx->sb, "void");
    } else {
        for (int i = 0; i < m_node->as.method.param_count; i++) {
            if (i > 0) sb_append(&ctx->sb, ", ");
            bool is_bor = m_node->as.method.param_is_borrowed ? m_node->as.method.param_is_borrowed[i] : false;
            bool is_arr = m_node->as.method.param_is_array ? m_node->as.method.param_is_array[i] : false;
            bool is_map = m_node->as.method.param_is_map ? m_node->as.method.param_is_map[i] : false;
            const char *p_type_str;
            if (is_bor && m_node->as.method.param_class_names && m_node->as.method.param_class_names[i] && is_struct_name(ctx, m_node->as.method.param_class_names[i])) {
                p_type_str = c_type_str_full(ctx, m_node->as.method.param_types[i], m_node->as.method.param_class_names[i], true, false);
            } else if (m_node->as.method.param_type_descs && m_node->as.method.param_type_descs[i]) {
                p_type_str = c_type_str_desc(ctx, m_node->as.method.param_type_descs[i]);
            } else if (is_arr || is_map) {
                p_type_str = c_type_str_decl_full(ctx, m_node->as.method.param_types[i], m_node->as.method.param_class_names[i], is_arr, is_map, false);
            } else {
                p_type_str = c_type_str_full(ctx, m_node->as.method.param_types[i], m_node->as.method.param_class_names[i], is_bor, false);
            }
            sb_appendf(&ctx->sb, "%s %s", p_type_str, m_node->as.method.param_names[i]);
        }
    }
    sb_append(&ctx->sb, ") ");

    gen_block(ctx, m_node->as.method.body);
    sb_append(&ctx->sb, "\n");
}

static void gen_function(CodegenCtx *ctx, AstNode *fn) {
    ctx->current_function = fn;
    bool is_main = (strcmp(fn->as.function.name, "main") == 0);
    if (is_main) {
        sb_append(&ctx->sb, "int main(int __cco_main_argc, char **__cco_main_argv) ");
    } else {
        const char *ret_type_str = fn->as.function.return_type_desc ?
            c_type_str_desc(ctx, fn->as.function.return_type_desc) :
            c_type_str_decl_full(ctx, fn->as.function.return_type, fn->as.function.return_class_name, fn->as.function.return_is_array, fn->as.function.return_is_map, fn->as.function.returns_heap_pointer);
        const char *c_fn_name = fn->as.function.name;
        if (fn->as.function.is_operator) {
            c_fn_name = get_operator_fn_c_name(fn->as.function.param_class_names[0], fn->as.function.operator_symbol, fn->as.function.param_count);
        }
        sb_appendf(&ctx->sb, "%s %s(", ret_type_str, c_fn_name);

        if (fn->as.function.param_count == 0) {
            sb_append(&ctx->sb, "void");
        } else {
            for (int i = 0; i < fn->as.function.param_count; i++) {
                if (i > 0) sb_append(&ctx->sb, ", ");
                bool is_bor = fn->as.function.param_is_borrowed ? fn->as.function.param_is_borrowed[i] : false;
                bool is_arr = fn->as.function.param_is_array ? fn->as.function.param_is_array[i] : false;
                bool is_map = fn->as.function.param_is_map ? fn->as.function.param_is_map[i] : false;
                const char *p_type_str;
                if (is_bor && fn->as.function.param_class_names && fn->as.function.param_class_names[i] && is_struct_name(ctx, fn->as.function.param_class_names[i])) {
                    p_type_str = c_type_str_full(ctx, fn->as.function.param_types[i], fn->as.function.param_class_names[i], true, false);
                } else if (fn->as.function.param_type_descs && fn->as.function.param_type_descs[i]) {
                    p_type_str = c_type_str_desc(ctx, fn->as.function.param_type_descs[i]);
                } else if (is_arr || is_map) {
                    p_type_str = c_type_str_decl_full(ctx, fn->as.function.param_types[i], fn->as.function.param_class_names[i], is_arr, is_map, false);
                } else {
                    p_type_str = c_type_str_full(ctx, fn->as.function.param_types[i], fn->as.function.param_class_names[i], is_bor, false);
                }
                sb_appendf(&ctx->sb, "%s %s", p_type_str, fn->as.function.param_names[i]);
            }
        }
        sb_append(&ctx->sb, ") ");
    }

    gen_block(ctx, fn->as.function.body);
    sb_append(&ctx->sb, "\n");
}

typedef enum {
    HEADER_NONE        = 0,
    HEADER_STDIO       = 1 << 0,  // <stdio.h>
    HEADER_STDLIB      = 1 << 1,  // <stdlib.h>
    HEADER_STDBOOL     = 1 << 2,  // <stdbool.h>
    HEADER_STDINT      = 1 << 3,  // <stdint.h>
    HEADER_STRING      = 1 << 4,  // <string.h>
    HEADER_MATH        = 1 << 5,  // <math.h> (also triggers linker -lm)
    HEADER_TIME        = 1 << 6,  // <time.h>
    HEADER_ERRNO       = 1 << 7,  // <errno.h>
    HEADER_UNISTD      = 1 << 8,  // <unistd.h>
    HEADER_SYS_TYPES   = 1 << 9,  // <sys/types.h>
    HEADER_SYS_SOCKET  = 1 << 10, // <sys/socket.h>
    HEADER_NETINET_IN  = 1 << 11, // <netinet/in.h>
    HEADER_ARPA_INET   = 1 << 12, // <arpa/inet.h>
    HEADER_STRINGS     = 1 << 13, // <strings.h>
    HEADER_POLL        = 1 << 14, // <poll.h>
    HEADER_POSIX       = 1 << 15, // _POSIX_C_SOURCE 200809L & _DEFAULT_SOURCE
} HeaderFlags;

static bool s_last_codegen_needs_libm = false;

bool cco_codegen_needs_libm(void) {
    return s_last_codegen_needs_libm;
}

static bool chunk_needs_posix(const char *name) {
    if (!name) return false;
    return (strcmp(name, "map_put") == 0 ||
            strcmp(name, "map_keys") == 0 ||
            strcmp(name, "get_args") == 0 ||
            strcmp(name, "get_program_name") == 0 ||
            strcmp(name, "int_to_str") == 0 ||
            strcmp(name, "float_to_str") == 0 ||
            strcmp(name, "bool_to_str") == 0 ||
            strcmp(name, "char_to_str") == 0 ||
            strcmp(name, "to_string") == 0 ||
            strcmp(name, "sleep_ms") == 0 ||
            strcmp(name, "net_listen") == 0 ||
            strcmp(name, "net_listen_on") == 0 ||
            strcmp(name, "net_connect") == 0 ||
            strcmp(name, "net_last_error") == 0 ||
            strcmp(name, "net_accept") == 0 ||
            strcmp(name, "net_recv") == 0 ||
            strcmp(name, "net_recv_raw") == 0 ||
            strcmp(name, "net_recv_http") == 0 ||
            strcmp(name, "net_send") == 0 ||
            strcmp(name, "net_send_bytes") == 0 ||
            strcmp(name, "net_recv_bytes") == 0 ||
            strcmp(name, "net_shutdown") == 0 ||
            strcmp(name, "net_set_timeouts") == 0 ||
            strcmp(name, "net_close") == 0);
}

static uint32_t get_chunk_header_flags(const char *name) {
    if (!name) return HEADER_NONE;
    if (strcmp(name, "concat") == 0 ||
        strcmp(name, "char_at") == 0 ||
        strcmp(name, "substring") == 0 ||
        strcmp(name, "char_to_string") == 0 ||
        strcmp(name, "to_string") == 0) {
        return HEADER_STDIO | HEADER_STDLIB | HEADER_STRING;
    }
    if (strcmp(name, "concat_free") == 0) {
        return HEADER_STDIO | HEADER_STDLIB | HEADER_STRING | HEADER_STDBOOL;
    }
    if (strcmp(name, "int_to_str") == 0 ||
        strcmp(name, "float_to_str") == 0) {
        return HEADER_STDIO | HEADER_STRING;
    }
    if (strcmp(name, "bool_to_str") == 0) {
        return HEADER_STDBOOL | HEADER_STRING;
    }
    if (strcmp(name, "char_to_str") == 0 ||
        strcmp(name, "get_args") == 0 ||
        strcmp(name, "get_program_name") == 0 ||
        strcmp(name, "map_keys") == 0) {
        return HEADER_STRING;
    }
    if (strcmp(name, "read_file") == 0 ||
        strcmp(name, "read_file_checked") == 0 ||
        strcmp(name, "io_last_error") == 0) {
        return HEADER_STDIO | HEADER_STDLIB | HEADER_STRING | HEADER_ERRNO;
    }
    if (strcmp(name, "write_file") == 0) {
        return HEADER_STDIO | HEADER_STDLIB | HEADER_STDBOOL | HEADER_STRING;
    }
    if (strcmp(name, "write_bytes") == 0) {
        return HEADER_STDIO | HEADER_STDBOOL;
    }
    if (strcmp(name, "read_line") == 0 ||
        strcmp(name, "input") == 0) {
        return HEADER_STDIO | HEADER_STDLIB;
    }
    if (strcmp(name, "alloc_arr") == 0 ||
        strcmp(name, "list_new") == 0 ||
        strcmp(name, "arr_maybe_grow") == 0 ||
        strcmp(name, "arr_decr_len") == 0 ||
        strcmp(name, "bounds_check") == 0) {
        return HEADER_STDIO | HEADER_STDLIB;
    }
    if (strcmp(name, "free_arr") == 0 ||
        strcmp(name, "map_free") == 0 ||
        strcmp(name, "execute_cmd") == 0 ||
        strcmp(name, "cco_exit") == 0) {
        return HEADER_STDLIB;
    }
    if (strcmp(name, "map_hash") == 0) {
        return HEADER_STRING | HEADER_STDBOOL | HEADER_STDINT;
    }
    if (strcmp(name, "map_new") == 0 ||
        strcmp(name, "map_rehash") == 0 ||
        strcmp(name, "map_get") == 0 ||
        strcmp(name, "map_remove") == 0) {
        return HEADER_STDIO | HEADER_STDLIB | HEADER_STDINT;
    }
    if (strcmp(name, "map_put") == 0) {
        return HEADER_STDIO | HEADER_STDLIB | HEADER_STRING | HEADER_STDINT;
    }
    if (strcmp(name, "map_has") == 0 ||
        strcmp(name, "is_int") == 0 ||
        strcmp(name, "is_float") == 0) {
        return HEADER_STDBOOL;
    }
    if (strcmp(name, "file_exists") == 0) {
        return HEADER_STDIO | HEADER_STDBOOL;
    }
    if (strcmp(name, "to_int") == 0 ||
        strcmp(name, "to_float") == 0) {
        return HEADER_STDIO | HEADER_STDLIB;
    }
    if (strcmp(name, "try_parse_int") == 0 ||
        strcmp(name, "try_parse_float") == 0) {
        return HEADER_ERRNO | HEADER_STDBOOL | HEADER_STDLIB;
    }
    if (strcmp(name, "random_seed") == 0 ||
        strcmp(name, "random_int") == 0) {
        return HEADER_STDLIB | HEADER_TIME;
    }
    if (strcmp(name, "sleep_ms") == 0) {
        return HEADER_TIME;
    }
    if (strcmp(name, "net_last_error") == 0) {
        return HEADER_STDLIB | HEADER_STRING;
    }
    if (strcmp(name, "net_listen") == 0) {
        return HEADER_SYS_SOCKET | HEADER_NETINET_IN | HEADER_UNISTD | HEADER_STRING | HEADER_STDINT | HEADER_ARPA_INET | HEADER_ERRNO | HEADER_STDIO;
    }
    if (strcmp(name, "net_listen_on") == 0) {
        return HEADER_SYS_SOCKET | HEADER_NETINET_IN | HEADER_ARPA_INET | HEADER_UNISTD | HEADER_STRING | HEADER_STDINT | HEADER_ERRNO | HEADER_STDIO;
    }
    if (strcmp(name, "net_connect") == 0) {
        return HEADER_SYS_SOCKET | HEADER_NETINET_IN | HEADER_ARPA_INET | HEADER_UNISTD | HEADER_STRING | HEADER_STDINT | HEADER_ERRNO | HEADER_STDIO | HEADER_POLL;
    }
    if (strcmp(name, "net_accept") == 0) {
        return HEADER_SYS_SOCKET | HEADER_NETINET_IN;
    }
    if (strcmp(name, "net_recv") == 0 ||
        strcmp(name, "net_recv_raw") == 0 ||
        strcmp(name, "net_recv_http") == 0) {
        return HEADER_SYS_SOCKET | HEADER_SYS_TYPES | HEADER_STRINGS | HEADER_ERRNO | HEADER_STDLIB | HEADER_STRING;
    }
    if (strcmp(name, "net_send") == 0 ||
        strcmp(name, "net_send_bytes") == 0 ||
        strcmp(name, "net_recv_bytes") == 0) {
        return HEADER_SYS_SOCKET | HEADER_SYS_TYPES | HEADER_ERRNO | HEADER_STRING | HEADER_STDLIB;
    }
    if (strcmp(name, "net_close") == 0) {
        return HEADER_UNISTD;
    }
    if (strcmp(name, "net_shutdown") == 0) {
        return HEADER_SYS_SOCKET | HEADER_ERRNO | HEADER_STDIO;
    }
    if (strcmp(name, "net_set_timeouts") == 0) {
        return HEADER_SYS_SOCKET | HEADER_SYS_TYPES | HEADER_TIME | HEADER_ERRNO | HEADER_STDIO;
    }
    return HEADER_NONE;
}

static void mark_chunk_used(bool *used_chunks, const char *name) {
    if (!name) return;
    for (size_t i = 0; i < PRELUDE_CHUNK_COUNT; i++) {
        if (strcmp(PRELUDE_CHUNKS[i].name, name) == 0) {
            used_chunks[i] = true;
            return;
        }
    }
}

static void scan_typedesc_usage(const TypeDesc *td, bool *used_chunks, uint32_t *header_flags) {
    if (!td) return;
    if (td->kind == TYPEDESC_PRIMITIVE && td->as.primitive_desc.primitive_kind == TY_STRING) {
        *header_flags |= HEADER_STRING | HEADER_POSIX | HEADER_STDLIB;
    } else if (td->kind == TYPEDESC_ARRAY) {
        mark_chunk_used(used_chunks, "free_arr");
        mark_chunk_used(used_chunks, "arr_len");
        *header_flags |= HEADER_STDLIB;
        scan_typedesc_usage(td->as.array_desc.elem_type, used_chunks, header_flags);
    } else if (td->kind == TYPEDESC_MAP) {
        mark_chunk_used(used_chunks, "map_free");
        *header_flags |= HEADER_STDINT | HEADER_STDLIB;
        scan_typedesc_usage(td->as.map_desc.key_type, used_chunks, header_flags);
        scan_typedesc_usage(td->as.map_desc.val_type, used_chunks, header_flags);
    } else if (td->kind == TYPEDESC_CLASS || td->kind == TYPEDESC_ENUM) {
        *header_flags |= HEADER_STDLIB;
    }
}

static void check_releases_usage(AstNode *node, bool *used_chunks, uint32_t *header_flags) {
    if (!node) return;
    for (int i = 0; i < node->releases_count; i++) {
        *header_flags |= HEADER_STDLIB;
        if (node->releases_to_emit && node->releases_to_emit[i].type_desc) {
            scan_typedesc_usage(node->releases_to_emit[i].type_desc, used_chunks, header_flags);
        } else if (node->releases_to_emit && node->releases_to_emit[i].is_map) {
            mark_chunk_used(used_chunks, "map_free");
            *header_flags |= HEADER_STDINT;
        } else if (node->releases_to_emit && node->releases_to_emit[i].is_array) {
            mark_chunk_used(used_chunks, "free_arr");
            mark_chunk_used(used_chunks, "arr_len");
        }
    }
    if (node->frees_count > 0) {
        mark_chunk_used(used_chunks, "free_arr");
        *header_flags |= HEADER_STDLIB;
    }
}

static void scan_node_usage(AstNode *node, bool *used_chunks, uint32_t *header_flags, bool *needs_libm) {
    if (!node) return;

    check_releases_usage(node, used_chunks, header_flags);
    if (node->frees_count > 0) {
        mark_chunk_used(used_chunks, "free_arr");
        *header_flags |= HEADER_STDLIB;
    }

    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.class_count; i++) {
                scan_node_usage(node->as.program.classes[i], used_chunks, header_flags, needs_libm);
            }
            for (int i = 0; i < node->as.program.struct_count; i++) {
                scan_node_usage(node->as.program.structs[i], used_chunks, header_flags, needs_libm);
            }
            for (int i = 0; i < node->as.program.count; i++) {
                scan_node_usage(node->as.program.functions[i], used_chunks, header_flags, needs_libm);
            }
            break;

        case NODE_CLASS:
            *header_flags |= HEADER_STDLIB;
            for (int f = 0; f < node->as.class_decl.field_count; f++) {
                AstNode *fld = node->as.class_decl.fields[f];
                if (fld->as.field.type == TY_BOOL) {
                    *header_flags |= HEADER_STDBOOL;
                }
                if (fld->as.field.is_map) {
                    mark_chunk_used(used_chunks, "map_free");
                    *header_flags |= HEADER_STDINT | HEADER_STDLIB;
                    if (fld->as.field.is_array) {
                        mark_chunk_used(used_chunks, "arr_len");
                        mark_chunk_used(used_chunks, "free_arr");
                    }
                }
                if (fld->as.field.is_array) {
                    mark_chunk_used(used_chunks, "free_arr");
                    mark_chunk_used(used_chunks, "arr_len");
                    *header_flags |= HEADER_STDLIB;
                }
            }
            for (int m = 0; m < node->as.class_decl.method_count; m++) {
                scan_node_usage(node->as.class_decl.methods[m], used_chunks, header_flags, needs_libm);
            }
            break;

        case NODE_STRUCT:
            for (int f = 0; f < node->as.struct_decl.field_count; f++) {
                AstNode *fld = node->as.struct_decl.fields[f];
                if (fld->as.struct_field_decl.field_type == TY_BOOL) {
                    *header_flags |= HEADER_STDBOOL;
                }
            }
            break;

        case NODE_ENUM:
            *header_flags |= HEADER_STDLIB;
            for (int v = 0; v < node->as.enum_decl.variant_count; v++) {
                AstNode *vn = node->as.enum_decl.variants[v];
                for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                    AstNode *fld = vn->as.variant_decl.fields[f];
                    if (fld->as.field.type == TY_BOOL) {
                        *header_flags |= HEADER_STDBOOL;
                    }
                }
            }
            break;

        case NODE_FUNCTION:
            if (node->as.function.return_type == TY_BOOL) {
                *header_flags |= HEADER_STDBOOL;
            }
            for (int p = 0; p < node->as.function.param_count; p++) {
                if (node->as.function.param_types[p] == TY_BOOL) {
                    *header_flags |= HEADER_STDBOOL;
                }
                if (node->as.function.param_is_map && node->as.function.param_is_map[p]) {
                    *header_flags |= HEADER_STDINT;
                }
            }
            scan_node_usage(node->as.function.body, used_chunks, header_flags, needs_libm);
            break;

        case NODE_METHOD:
            if (node->as.method.return_type == TY_BOOL) {
                *header_flags |= HEADER_STDBOOL;
            }
            for (int p = 0; p < node->as.method.param_count; p++) {
                if (node->as.method.param_types[p] == TY_BOOL) {
                    *header_flags |= HEADER_STDBOOL;
                }
                if (node->as.method.param_is_map && node->as.method.param_is_map[p]) {
                    *header_flags |= HEADER_STDINT;
                }
            }
            scan_node_usage(node->as.method.body, used_chunks, header_flags, needs_libm);
            break;

        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                scan_node_usage(node->as.block.stmts[i], used_chunks, header_flags, needs_libm);
            }
            break;

        case NODE_LET:
            if (node->as.let.var_type == TY_BOOL) {
                *header_flags |= HEADER_STDBOOL;
            }
            if (node->as.let.var_type == TY_STRING) {
                *header_flags |= HEADER_STRING | HEADER_POSIX | HEADER_STDLIB;
            }
            if (node->as.let.is_map) {
                *header_flags |= HEADER_STDINT | HEADER_STDLIB;
            }
            if (node->as.let.is_array) {
                mark_chunk_used(used_chunks, "free_arr");
                *header_flags |= HEADER_STDLIB;
            }
            scan_node_usage(node->as.let.value, used_chunks, header_flags, needs_libm);
            break;

        case NODE_LITERAL:
            if (node->as.literal.lit_type == TY_BOOL) {
                *header_flags |= HEADER_STDBOOL;
            }
            break;

        case NODE_ASSIGN:
            if (node->as.assign.value && node->as.assign.value->type == NODE_LITERAL &&
                node->as.assign.value->as.literal.lit_type == TY_STRING) {
                *header_flags |= HEADER_STRING | HEADER_POSIX | HEADER_STDLIB;
            }
            scan_node_usage(node->as.assign.value, used_chunks, header_flags, needs_libm);
            break;

        case NODE_MEMBER_ASSIGN:
            scan_node_usage(node->as.member_assign.object, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.member_assign.value, used_chunks, header_flags, needs_libm);
            break;

        case NODE_INDEX_ASSIGN:
            mark_chunk_used(used_chunks, "bounds_check");
            *header_flags |= HEADER_STDIO | HEADER_STDLIB;
            scan_node_usage(node->as.index_assign.array_expr, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.index_assign.index, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.index_assign.value, used_chunks, header_flags, needs_libm);
            break;

        case NODE_IF:
            scan_node_usage(node->as.if_stmt.cond, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.if_stmt.then_b, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.if_stmt.else_b, used_chunks, header_flags, needs_libm);
            break;

        case NODE_WHILE:
            scan_node_usage(node->as.while_stmt.cond, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.while_stmt.body, used_chunks, header_flags, needs_libm);
            break;

        case NODE_FOR:
            scan_node_usage(node->as.for_stmt.init, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.for_stmt.cond, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.for_stmt.step, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.for_stmt.body, used_chunks, header_flags, needs_libm);
            break;

        case NODE_FOR_EACH:
            mark_chunk_used(used_chunks, "arr_len");
            scan_node_usage(node->as.for_each.collection_expr, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.for_each.body, used_chunks, header_flags, needs_libm);
            break;

        case NODE_MATCH:
            scan_node_usage(node->as.match_stmt.expr, used_chunks, header_flags, needs_libm);
            for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
                scan_node_usage(node->as.match_stmt.arms[a]->as.match_arm.body, used_chunks, header_flags, needs_libm);
            }
            break;

        case NODE_RETURN:
            if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_LITERAL &&
                node->as.return_stmt.value->as.literal.lit_type == TY_STRING) {
                *header_flags |= HEADER_STRING | HEADER_POSIX | HEADER_STDLIB;
            }
            scan_node_usage(node->as.return_stmt.value, used_chunks, header_flags, needs_libm);
            break;

        case NODE_PRINT:
            *header_flags |= HEADER_STDIO;
            if (node->as.print_stmt.values) {
                for (int i = 0; i < node->as.print_stmt.value_count; i++) {
                    scan_node_usage(node->as.print_stmt.values[i], used_chunks, header_flags, needs_libm);
                }
            } else if (node->as.print_stmt.value) {
                scan_node_usage(node->as.print_stmt.value, used_chunks, header_flags, needs_libm);
            }
            break;

        case NODE_EXPR_STMT:
            scan_node_usage(node->as.expr_stmt.expr, used_chunks, header_flags, needs_libm);
            break;

        case NODE_BINARY:
            if (strcmp(node->as.binary.op, "==") == 0 ||
                strcmp(node->as.binary.op, "!=") == 0 ||
                strcmp(node->as.binary.op, "<") == 0 ||
                strcmp(node->as.binary.op, "<=") == 0 ||
                strcmp(node->as.binary.op, ">") == 0 ||
                strcmp(node->as.binary.op, ">=") == 0 ||
                strcmp(node->as.binary.op, "&&") == 0 ||
                strcmp(node->as.binary.op, "||") == 0) {
                *header_flags |= HEADER_STDBOOL;
            }
            scan_node_usage(node->as.binary.left, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.binary.right, used_chunks, header_flags, needs_libm);
            break;

        case NODE_UNARY:
            if (strcmp(node->as.unary.op, "!") == 0) {
                *header_flags |= HEADER_STDBOOL;
            }
            scan_node_usage(node->as.unary.operand, used_chunks, header_flags, needs_libm);
            break;

        case NODE_INDEX:
            mark_chunk_used(used_chunks, "bounds_check");
            *header_flags |= HEADER_STDIO | HEADER_STDLIB;
            scan_node_usage(node->as.index.array_expr, used_chunks, header_flags, needs_libm);
            scan_node_usage(node->as.index.index, used_chunks, header_flags, needs_libm);
            break;

        case NODE_ALLOC:
            *header_flags |= HEADER_STDIO | HEADER_STDLIB;
            if (node->as.alloc.is_map) {
                mark_chunk_used(used_chunks, "map_new");
                *header_flags |= HEADER_STDINT;
            } else if (node->as.alloc.is_list) {
                mark_chunk_used(used_chunks, "list_new");
            } else {
                mark_chunk_used(used_chunks, "alloc_arr");
            }
            if (node->as.alloc.elem_type == TY_CLASS) {
                mark_chunk_used(used_chunks, "free_arr");
                mark_chunk_used(used_chunks, "arr_len");
            }
            scan_node_usage(node->as.alloc.count_expr, used_chunks, header_flags, needs_libm);
            break;

        case NODE_CALL: {
            const char *callee = node->as.call.callee;
            if (callee) {
                if (strcmp(callee, "equals") == 0) {
                    *header_flags |= HEADER_STRING | HEADER_STDBOOL;
                } else if (strcmp(callee, "sqrt") == 0 ||
                           strcmp(callee, "pow") == 0 ||
                           strcmp(callee, "exp") == 0 ||
                           strcmp(callee, "log") == 0 ||
                           strcmp(callee, "sin") == 0 ||
                           strcmp(callee, "cos") == 0 ||
                           strcmp(callee, "tan") == 0 ||
                           strcmp(callee, "abs_float") == 0 ||
                           strcmp(callee, "floor") == 0 ||
                           strcmp(callee, "ceil") == 0) {
                    *header_flags |= HEADER_MATH;
                    *needs_libm = true;
                } else if (strcmp(callee, "concat") == 0) mark_chunk_used(used_chunks, "concat");
                else if (strcmp(callee, "char_at") == 0) mark_chunk_used(used_chunks, "char_at");
                else if (strcmp(callee, "substring") == 0) mark_chunk_used(used_chunks, "substring");
                else if (strcmp(callee, "abs_int") == 0) mark_chunk_used(used_chunks, "abs_int");
                else if (strcmp(callee, "min_int") == 0) mark_chunk_used(used_chunks, "min_int");
                else if (strcmp(callee, "max_int") == 0) mark_chunk_used(used_chunks, "max_int");
                else if (strcmp(callee, "min_float") == 0) mark_chunk_used(used_chunks, "min_float");
                else if (strcmp(callee, "max_float") == 0) mark_chunk_used(used_chunks, "max_float");
                else if (strcmp(callee, "read_file") == 0) mark_chunk_used(used_chunks, "read_file");
                else if (strcmp(callee, "read_file_checked") == 0) mark_chunk_used(used_chunks, "read_file_checked");
                else if (strcmp(callee, "io_last_error") == 0) mark_chunk_used(used_chunks, "io_last_error");
                else if (strcmp(callee, "char_to_string") == 0) mark_chunk_used(used_chunks, "char_to_string");
                else if (strcmp(callee, "to_string") == 0) mark_chunk_used(used_chunks, "to_string");
                else if (strcmp(callee, "exit") == 0) mark_chunk_used(used_chunks, "cco_exit");
                else if (strcmp(callee, "file_exists") == 0) mark_chunk_used(used_chunks, "file_exists");
                else if (strcmp(callee, "write_file") == 0) mark_chunk_used(used_chunks, "write_file");
                else if (strcmp(callee, "write_bytes") == 0) mark_chunk_used(used_chunks, "write_bytes");
                else if (strcmp(callee, "execute_cmd") == 0 || strcmp(callee, "system") == 0) mark_chunk_used(used_chunks, "execute_cmd");
                else if (strcmp(callee, "put") == 0) mark_chunk_used(used_chunks, "map_put");
                else if (strcmp(callee, "get") == 0) mark_chunk_used(used_chunks, "map_get");
                else if (strcmp(callee, "has") == 0) mark_chunk_used(used_chunks, "map_has");
                else if (strcmp(callee, "remove") == 0) mark_chunk_used(used_chunks, "map_remove");
                else if (strcmp(callee, "keys") == 0) mark_chunk_used(used_chunks, "map_keys");
                else if (strcmp(callee, "push") == 0) {
                    mark_chunk_used(used_chunks, "arr_maybe_grow");
                    mark_chunk_used(used_chunks, "arr_incr_len");
                    mark_chunk_used(used_chunks, "arr_len_raw");
                }
                else if (strcmp(callee, "pop") == 0) {
                    mark_chunk_used(used_chunks, "arr_decr_len");
                    mark_chunk_used(used_chunks, "arr_len_raw");
                }
                else if (strcmp(callee, "len") == 0) {
                    mark_chunk_used(used_chunks, "arr_len");
                    mark_chunk_used(used_chunks, "map_bucket");
                    *header_flags |= HEADER_STRING;
                }
                else if (strcmp(callee, "args") == 0) {
                    mark_chunk_used(used_chunks, "get_args");
                }
                else if (strcmp(callee, "arg_count") == 0) {
                    mark_chunk_used(used_chunks, "get_arg_count");
                }
                else if (strcmp(callee, "program_name") == 0) {
                    mark_chunk_used(used_chunks, "get_program_name");
                }
                else if (strcmp(callee, "random_seed") == 0) {
                    mark_chunk_used(used_chunks, "random_seed");
                }
                else if (strcmp(callee, "random_int") == 0) {
                    mark_chunk_used(used_chunks, "random_int");
                }
                else if (strcmp(callee, "to_int") == 0) {
                    mark_chunk_used(used_chunks, "to_int");
                }
                else if (strcmp(callee, "is_int") == 0) {
                    mark_chunk_used(used_chunks, "is_int");
                }
                else if (strcmp(callee, "to_float") == 0) {
                    mark_chunk_used(used_chunks, "to_float");
                }
                else if (strcmp(callee, "is_float") == 0) {
                    mark_chunk_used(used_chunks, "is_float");
                }
                else if (strcmp(callee, "read_line") == 0) {
                    mark_chunk_used(used_chunks, "read_line");
                }
                else if (strcmp(callee, "input") == 0) {
                    mark_chunk_used(used_chunks, "input");
                }
                else if (strcmp(callee, "net_listen") == 0) {
                    mark_chunk_used(used_chunks, "net_listen");
                }
                else if (strcmp(callee, "net_listen_on") == 0) {
                    mark_chunk_used(used_chunks, "net_listen_on");
                }
                else if (strcmp(callee, "net_connect") == 0) {
                    mark_chunk_used(used_chunks, "net_connect");
                }
                else if (strcmp(callee, "net_last_error") == 0) {
                    mark_chunk_used(used_chunks, "net_last_error");
                }
                else if (strcmp(callee, "net_accept") == 0) {
                    mark_chunk_used(used_chunks, "net_accept");
                }
                else if (strcmp(callee, "net_recv") == 0) {
                    mark_chunk_used(used_chunks, "net_recv");
                }
                else if (strcmp(callee, "net_recv_raw") == 0) {
                    mark_chunk_used(used_chunks, "net_recv_raw");
                }
                else if (strcmp(callee, "net_recv_http") == 0) {
                    mark_chunk_used(used_chunks, "net_recv_http");
                }
                else if (strcmp(callee, "net_send") == 0) {
                    mark_chunk_used(used_chunks, "net_send");
                }
                else if (strcmp(callee, "net_send_bytes") == 0) {
                    mark_chunk_used(used_chunks, "net_send_bytes");
                }
                else if (strcmp(callee, "net_recv_bytes") == 0) {
                    mark_chunk_used(used_chunks, "net_recv_bytes");
                }
                else if (strcmp(callee, "net_close") == 0) {
                    mark_chunk_used(used_chunks, "net_close");
                }
                else if (strcmp(callee, "net_shutdown") == 0) {
                    mark_chunk_used(used_chunks, "net_shutdown");
                }
                else if (strcmp(callee, "net_set_timeouts") == 0) {
                    mark_chunk_used(used_chunks, "net_set_timeouts");
                }
                else if (strcmp(callee, "sleep_ms") == 0) {
                    mark_chunk_used(used_chunks, "sleep_ms");
                }
            }
            for (int i = 0; i < node->as.call.arg_count; i++) {
                scan_node_usage(node->as.call.args[i], used_chunks, header_flags, needs_libm);
            }
            break;
        }

        case NODE_METHOD_CALL:
            if (node->as.method_call.method_name && strcmp(node->as.method_call.method_name, "push") == 0) {
                mark_chunk_used(used_chunks, "arr_maybe_grow");
                if (node->as.method_call.arg_count > 0) {
                    AstNode *arg0 = node->as.method_call.args[0];
                    if (arg0 && arg0->type == NODE_LITERAL && arg0->as.literal.lit_type == TY_STRING) {
                        *header_flags |= HEADER_STRING | HEADER_POSIX | HEADER_STDLIB;
                    }
                }
            }
            scan_node_usage(node->as.method_call.object, used_chunks, header_flags, needs_libm);
            for (int i = 0; i < node->as.method_call.arg_count; i++) {
                scan_node_usage(node->as.method_call.args[i], used_chunks, header_flags, needs_libm);
            }
            break;

        case NODE_NEW:
            *header_flags |= HEADER_STDLIB;
            for (int i = 0; i < node->as.new_expr.field_count; i++) {
                AstNode *fv = node->as.new_expr.field_values[i];
                if (fv && fv->type == NODE_LITERAL && fv->as.literal.lit_type == TY_STRING) {
                    *header_flags |= HEADER_STRING | HEADER_POSIX | HEADER_STDLIB;
                }
                scan_node_usage(fv, used_chunks, header_flags, needs_libm);
            }
            break;

        case NODE_MEMBER:
            scan_node_usage(node->as.member.object, used_chunks, header_flags, needs_libm);
            break;

        case NODE_FSTRING_TEXT:
            *header_flags |= HEADER_STRING | HEADER_POSIX | HEADER_STDLIB;
            break;

        case NODE_FSTRING:
            *header_flags |= HEADER_STDIO | HEADER_STDLIB | HEADER_STRING | HEADER_POSIX;
            mark_chunk_used(used_chunks, "concat_free");
            mark_chunk_used(used_chunks, "int_to_str");
            mark_chunk_used(used_chunks, "float_to_str");
            mark_chunk_used(used_chunks, "bool_to_str");
            mark_chunk_used(used_chunks, "char_to_str");
            for (int i = 0; i < node->as.fstring.part_count; i++) {
                AstNode *part = node->as.fstring.parts[i];
                if (part->type != NODE_FSTRING_TEXT) {
                    scan_node_usage(part, used_chunks, header_flags, needs_libm);
                }
            }
            break;

        default:
            break;
    }
}

static void resolve_transitive_dependencies(bool *used_chunks) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < PRELUDE_CHUNK_COUNT; i++) {
            if (used_chunks[i] && PRELUDE_CHUNKS[i].depends_on) {
                for (int d = 0; PRELUDE_CHUNKS[i].depends_on[d] != NULL; d++) {
                    const char *dep_name = PRELUDE_CHUNKS[i].depends_on[d];
                    for (size_t k = 0; k < PRELUDE_CHUNK_COUNT; k++) {
                        if (strcmp(PRELUDE_CHUNKS[k].name, dep_name) == 0) {
                            if (!used_chunks[k]) {
                                used_chunks[k] = true;
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }
}


char *generate_c_code(AstNode *program, AstArena *arena) {
    CodegenCtx ctx;
    memset(&ctx, 0, sizeof(CodegenCtx));
    ctx.sb = create_buffer();
    ctx.indent_level = 0;
    ctx.program = program;
    ctx.current_function = NULL;
    ctx.ct = build_class_table(program, arena);

    bool has_main_fn = false;
    if (program && program->type == NODE_PROGRAM) {
        for (int i = 0; i < program->as.program.count; i++) {
            if (program->as.program.functions[i] &&
                strcmp(program->as.program.functions[i]->as.function.name, "main") == 0) {
                has_main_fn = true;
                break;
            }
        }
    }

    bool used_chunks[PRELUDE_CHUNK_COUNT];
    memset(used_chunks, 0, sizeof(used_chunks));
    uint32_t header_flags = HEADER_NONE;
    bool needs_libm = false;

    if (program && program->type == NODE_PROGRAM) {
        if (program->as.program.class_count > 0 || program->as.program.enum_count > 0) {
            header_flags |= HEADER_STDLIB | HEADER_STDIO;
            for (int i = 0; i < program->as.program.class_count; i++) {
                AstNode *cls = program->as.program.classes[i];
                for (int f = 0; f < cls->as.class_decl.field_count; f++) {
                    if (cls->as.class_decl.fields[f]->as.field.type == TY_STRING) {
                        header_flags |= HEADER_STRING | HEADER_POSIX;
                        break;
                    }
                }
            }
            for (int i = 0; i < program->as.program.enum_count; i++) {
                AstNode *en = program->as.program.enums[i];
                for (int v = 0; v < en->as.enum_decl.variant_count; v++) {
                    AstNode *vn = en->as.enum_decl.variants[v];
                    for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                        if (vn->as.variant_decl.fields[f]->as.field.type == TY_STRING) {
                            header_flags |= HEADER_STRING | HEADER_POSIX;
                            break;
                        }
                    }
                }
            }
        }
    }

    scan_node_usage(program, used_chunks, &header_flags, &needs_libm);
    resolve_transitive_dependencies(used_chunks);

    // Aggregate headers from all activated prelude chunks
    for (size_t i = 0; i < PRELUDE_CHUNK_COUNT; i++) {
        if (used_chunks[i]) {
            header_flags |= get_chunk_header_flags(PRELUDE_CHUNKS[i].name);
        }
    }

    s_last_codegen_needs_libm = needs_libm;

    // Fixed C prelude comment
    sb_append(&ctx.sb, "/* Generated by Cco (C--) Compiler */\n");

    // 1. Feature-test macros (only emitted if POSIX features/headers are required)
    bool needs_posix = (header_flags & (HEADER_POSIX | HEADER_UNISTD | HEADER_SYS_TYPES |
                                         HEADER_SYS_SOCKET | HEADER_NETINET_IN |
                                         HEADER_ARPA_INET | HEADER_POLL | HEADER_STRINGS)) != 0;
    for (size_t i = 0; i < PRELUDE_CHUNK_COUNT; i++) {
        if (used_chunks[i] && chunk_needs_posix(PRELUDE_CHUNKS[i].name)) {
            needs_posix = true;
            break;
        }
    }
    if (needs_posix) {
        sb_append(&ctx.sb, "#define _POSIX_C_SOURCE 200809L\n");
        sb_append(&ctx.sb, "#define _DEFAULT_SOURCE\n");
    }

    // 2. Standard C11 headers (emitted in fixed deterministic order)
    if (header_flags & HEADER_STDIO)      sb_append(&ctx.sb, "#include <stdio.h>\n");
    if (header_flags & HEADER_STDLIB)     sb_append(&ctx.sb, "#include <stdlib.h>\n");
    if (header_flags & HEADER_STDBOOL)    sb_append(&ctx.sb, "#include <stdbool.h>\n");
    if (header_flags & HEADER_STDINT)     sb_append(&ctx.sb, "#include <stdint.h>\n");
    if (header_flags & HEADER_STRING)     sb_append(&ctx.sb, "#include <string.h>\n");
    if (header_flags & HEADER_MATH)       sb_append(&ctx.sb, "#include <math.h>\n");
    if (header_flags & HEADER_TIME)       sb_append(&ctx.sb, "#include <time.h>\n");
    if (header_flags & HEADER_ERRNO)      sb_append(&ctx.sb, "#include <errno.h>\n");

    // 3. POSIX/system headers (emitted in fixed deterministic order)
    if (header_flags & HEADER_UNISTD)     sb_append(&ctx.sb, "#include <unistd.h>\n");
    if (header_flags & HEADER_SYS_TYPES)  sb_append(&ctx.sb, "#include <sys/types.h>\n");
    if (header_flags & HEADER_SYS_SOCKET) sb_append(&ctx.sb, "#include <sys/socket.h>\n");
    if (header_flags & HEADER_NETINET_IN) sb_append(&ctx.sb, "#include <netinet/in.h>\n");
    if (header_flags & HEADER_ARPA_INET)  sb_append(&ctx.sb, "#include <arpa/inet.h>\n");
    if (header_flags & HEADER_POLL)       sb_append(&ctx.sb, "#include <poll.h>\n");
    if (header_flags & HEADER_STRINGS)    sb_append(&ctx.sb, "#include <strings.h>\n");

    if (header_flags != HEADER_NONE || needs_posix) {
        sb_append(&ctx.sb, "\n");
    }

    if (has_main_fn) {
        sb_append(&ctx.sb, "static int __cco_argc;\n");
        sb_append(&ctx.sb, "static char **__cco_argv;\n\n");
    }

    for (size_t i = 0; i < PRELUDE_CHUNK_COUNT; i++) {
        if (used_chunks[i]) {
            if (strcmp(PRELUDE_CHUNKS[i].name, "map_put") == 0) {
                const char *orig = PRELUDE_CHUNKS[i].c_source;
                const char *target = "if (free_fn && m->buckets[idx].value) {";
                const char *pos = strstr(orig, target);
                if (pos) {
                    sb_append_len(&ctx.sb, orig, (size_t)(pos - orig));
                    sb_append(&ctx.sb, "if (free_fn && m->buckets[idx].value && m->buckets[idx].value != val) {");
                    sb_append(&ctx.sb, pos + strlen(target));
                } else {
                    sb_append(&ctx.sb, orig);
                }
            } else {
                sb_append(&ctx.sb, PRELUDE_CHUNKS[i].c_source);
            }
        }
    }


    // Struct definitions
    gen_struct_typedefs(&ctx, program);

    // Enum definitions, tag enum, union struct, _free, and _new_<variant> helpers
    gen_enum_helpers(&ctx, program);

    // Class struct definitions, retain/release/new helpers
    gen_class_helpers(&ctx, program);

    // Recursive container cleanup helpers
    gen_recursive_cleanup_helpers(&ctx, program);

    // Function prototypes
    for (int i = 0; i < program->as.program.count; i++) {
        AstNode *fn = program->as.program.functions[i];
        if (strcmp(fn->as.function.name, "main") == 0) continue;
        const char *ret_type_str = fn->as.function.return_type_desc ?
            c_type_str_desc(&ctx, fn->as.function.return_type_desc) :
            c_type_str_decl_full(&ctx, fn->as.function.return_type, fn->as.function.return_class_name, fn->as.function.return_is_array, fn->as.function.return_is_map, fn->as.function.returns_heap_pointer);
        const char *c_fn_name = fn->as.function.name;
        if (fn->as.function.is_operator) {
            c_fn_name = get_operator_fn_c_name(fn->as.function.param_class_names[0], fn->as.function.operator_symbol, fn->as.function.param_count);
        }
        sb_appendf(&ctx.sb, "%s %s(", ret_type_str, c_fn_name);
        if (fn->as.function.param_count == 0) {
            sb_append(&ctx.sb, "void");
        } else {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (p > 0) sb_append(&ctx.sb, ", ");
                bool is_bor = fn->as.function.param_is_borrowed ? fn->as.function.param_is_borrowed[p] : false;
                bool is_arr = fn->as.function.param_is_array ? fn->as.function.param_is_array[p] : false;
                bool is_map = fn->as.function.param_is_map ? fn->as.function.param_is_map[p] : false;
                const char *p_type_str;
                if (is_bor && fn->as.function.param_class_names && fn->as.function.param_class_names[p] && is_struct_name(&ctx, fn->as.function.param_class_names[p])) {
                    p_type_str = c_type_str_full(&ctx, fn->as.function.param_types[p], fn->as.function.param_class_names[p], true, false);
                } else if (fn->as.function.param_type_descs && fn->as.function.param_type_descs[p]) {
                    p_type_str = c_type_str_desc(&ctx, fn->as.function.param_type_descs[p]);
                } else if (is_arr || is_map) {
                    p_type_str = c_type_str_decl_full(&ctx, fn->as.function.param_types[p], fn->as.function.param_class_names[p], is_arr, is_map, false);
                } else {
                    p_type_str = c_type_str_full(&ctx, fn->as.function.param_types[p], fn->as.function.param_class_names[p], is_bor, false);
                }
                sb_appendf(&ctx.sb, "%s %s", p_type_str, fn->as.function.param_names[p]);
            }
        }
        sb_append(&ctx.sb, ");\n");
    }
    if (program->as.program.count > 0) sb_append(&ctx.sb, "\n");

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
