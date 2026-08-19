#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include "class_decl.h"
#include "stdlib_prelude.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

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
        return find_ident_class_in_node(program, node->as.function.body, var_name);
    }

    if (node->type == NODE_METHOD) {
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
            return node->as.let.class_name;
        }
        return NULL;
    }

    if (node->type == NODE_FOR_EACH) {
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
    if (expr->type == NODE_CALL) {
        const char *name = expr->as.call.callee;
        if (strcmp(name, "pop") == 0 && expr->as.call.arg_count > 0) {
            return infer_expr_type(program, fn, expr->as.call.args[0]);
        }
        if (strcmp(name, "push") == 0 && expr->as.call.arg_count > 0) {
            return infer_expr_type(program, fn, expr->as.call.args[0]);
        }
        if (strcmp(name, "len") == 0 || strcmp(name, "abs_int") == 0 || strcmp(name, "min_int") == 0 || strcmp(name, "max_int") == 0 || strcmp(name, "arg_count") == 0 || strcmp(name, "to_int") == 0 || strcmp(name, "random_int") == 0) return TY_INT;
        if (strcmp(name, "sqrt") == 0 || strcmp(name, "pow") == 0 || strcmp(name, "abs_float") == 0 || strcmp(name, "floor") == 0 || strcmp(name, "ceil") == 0 || strcmp(name, "min_float") == 0 || strcmp(name, "max_float") == 0 || strcmp(name, "to_float") == 0) return TY_FLOAT;
        if (strcmp(name, "concat") == 0 || strcmp(name, "substring") == 0 || strcmp(name, "read_file") == 0 || strcmp(name, "program_name") == 0 || strcmp(name, "args") == 0 || strcmp(name, "read_line") == 0) return TY_STRING;
        if (strcmp(name, "equals") == 0 || strcmp(name, "write_file") == 0 || strcmp(name, "is_int") == 0 || strcmp(name, "is_float") == 0) return TY_BOOL;
        if (strcmp(name, "char_at") == 0) return TY_CHAR;
        if (strcmp(name, "random_seed") == 0) return TY_VOID;

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

    return TY_INT;
}

static bool infer_expr_is_array(AstNode *program, AstNode *fn, AstNode *expr) {
    if (!expr) return false;
    if (expr->type == NODE_ALLOC) return true;

    if (expr->type == NODE_IDENT && fn) {
        const char *var_name = expr->as.ident.name;
        if (fn->type == NODE_FUNCTION) {
            for (int p = 0; p < fn->as.function.param_count; p++) {
                if (strcmp(fn->as.function.param_names[p], var_name) == 0) {
                    return fn->as.function.param_is_array ? fn->as.function.param_is_array[p] : false;
                }
            }
        } else if (fn->type == NODE_METHOD) {
            for (int p = 0; p < fn->as.method.param_count; p++) {
                if (strcmp(fn->as.method.param_names[p], var_name) == 0) {
                    return fn->as.method.param_is_array ? fn->as.method.param_is_array[p] : false;
                }
            }
        }
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : fn->as.method.body;
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.is_array;
                }
            }
        }
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
    return false;
}

static bool infer_expr_is_map(AstNode *program, AstNode *fn, AstNode *expr) {
    if (!expr) return false;
    if (expr->type == NODE_ALLOC && expr->as.alloc.is_map) return true;

    if (expr->type == NODE_IDENT && fn) {
        const char *var_name = expr->as.ident.name;
        AstNode *body = (fn->type == NODE_FUNCTION) ? fn->as.function.body : (fn->type == NODE_METHOD ? fn->as.method.body : NULL);
        if (body && body->type == NODE_BLOCK) {
            for (int i = 0; i < body->as.block.count; i++) {
                AstNode *stmt = body->as.block.stmts[i];
                if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                    return stmt->as.let.is_map;
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
    if (expr->type == NODE_CALL && expr->as.call.callee && strcmp(expr->as.call.callee, "put") == 0) {
        return get_map_val_type(ctx, expr->as.call.args[0]);
    }
    return TY_INT;
}

static const char *get_map_val_class_name(CodegenCtx *ctx, AstNode *expr) {
    if (!expr) return NULL;
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

static void emit_releases(CodegenCtx *ctx, AstNode *node) {
    if (!node || node->releases_count == 0) return;
    for (int i = 0; i < node->releases_count; i++) {
        emit_indent(ctx);
        if (node->releases_to_emit[i].is_map) {
            const char *mname = node->releases_to_emit[i].var_name;
            const char *cls = node->releases_to_emit[i].class_name;
            if (cls && find_class(ctx->ct, cls) != NULL) {
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
            } else if (cls && find_class(ctx->ct, cls) != NULL) {
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
        while (*s) {
            if (*s == '\n') sb_append(sb, "\\n");
            else if (*s == '\t') sb_append(sb, "\\t");
            else if (*s == '\r') sb_append(sb, "\\r");
            else if (*s == '\\') sb_append(sb, "\\\\");
            else if (*s == '\"') sb_append(sb, "\\\"");
            else {
                char ch[2] = {*s, '\0'};
                sb_append(sb, ch);
            }
            s++;
        }
    }
    sb_append(sb, "\"");
}

static void gen_fstring_part(CodegenCtx *ctx, AstNode *part, bool *out_needs_free);

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
            if (expr->as.alloc.is_map) {
                int kt = (expr->as.alloc.key_type == TY_STRING) ? 1 : 0;
                sb_appendf(&ctx->sb, "__cco_map_new(%d)", kt);
            } else if (expr->as.alloc.is_list) {
                if (expr->as.alloc.elem_type == TY_CLASS && !is_struct_name(ctx, expr->as.alloc.class_name)) {
                    sb_appendf(&ctx->sb, "(%s **)__cco_list_new(sizeof(%s *))", expr->as.alloc.class_name, expr->as.alloc.class_name);
                } else {
                    const char *elem_str = c_type_str_full(ctx, expr->as.alloc.elem_type, expr->as.alloc.class_name, false, false);
                    sb_appendf(&ctx->sb, "(%s *)__cco_list_new(sizeof(%s))", elem_str, elem_str);
                }
            } else {
                if (expr->as.alloc.elem_type == TY_CLASS && !is_struct_name(ctx, expr->as.alloc.class_name)) {
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
                        if (fval) {
                            if (vdef->fields[f].type == TY_STRING && fval->type == NODE_LITERAL) {
                                sb_appendf(&ctx->sb, "strdup(\"%s\")", fval->as.literal.val.s);
                            } else {
                                gen_expr(ctx, fval);
                            }
                        } else {
                            sb_append(&ctx->sb, "0");
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

        case NODE_BINARY:
            sb_append(&ctx->sb, "(");
            gen_expr(ctx, expr->as.binary.left);
            sb_appendf(&ctx->sb, " %s ", expr->as.binary.op);
            gen_expr(ctx, expr->as.binary.right);
            sb_append(&ctx->sb, ")");
            break;

        case NODE_UNARY:
            if (strcmp(expr->as.unary.op, "&") == 0) {
                Type ot = infer_expr_type(ctx->program, ctx->current_function, expr->as.unary.operand);
                const char *ocls = get_expr_elem_class_name(ctx, expr->as.unary.operand);
                if (ot == TY_CLASS && ocls && !is_struct_name(ctx, ocls)) {
                    gen_expr(ctx, expr->as.unary.operand);
                    break;
                }
            }
            sb_appendf(&ctx->sb, "(%s", expr->as.unary.op);
            gen_expr(ctx, expr->as.unary.operand);
            sb_append(&ctx->sb, ")");
            break;

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
                Type kt = get_map_key_type(ctx, m_arg);
                const char *v_cls = get_map_val_class_name(ctx, m_arg);

                sb_append(&ctx->sb, "__cco_map_put(");
                gen_expr(ctx, m_arg);
                sb_append(&ctx->sb, ", ");
                if (kt == TY_STRING) {
                    sb_append(&ctx->sb, "(void *)(");
                    gen_expr(ctx, k_arg);
                    sb_append(&ctx->sb, ")");
                } else {
                    sb_append(&ctx->sb, "(void *)(intptr_t)(");
                    gen_expr(ctx, k_arg);
                    sb_append(&ctx->sb, ")");
                }
                sb_append(&ctx->sb, ", ");
                if (v_cls && find_class(ctx->ct, v_cls) != NULL) {
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
                if (v_cls && find_class(ctx->ct, v_cls) != NULL) {
                    sb_appendf(&ctx->sb, "(__cco_val_free_fn)%s_free)", v_cls);
                } else {
                    sb_append(&ctx->sb, "NULL)");
                }
            } else if (strcmp(callee, "get") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                AstNode *k_arg = expr->as.call.args[1];
                Type kt = get_map_key_type(ctx, m_arg);
                Type vt = get_map_val_type(ctx, m_arg);
                const char *v_cls = get_map_val_class_name(ctx, m_arg);
                const char *c_type = c_type_str_full(ctx, vt, v_cls, false, false);

                if (vt == TY_CLASS || vt == TY_STRING) {
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
            } else if (strcmp(callee, "has") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                AstNode *k_arg = expr->as.call.args[1];
                Type kt = get_map_key_type(ctx, m_arg);

                sb_append(&ctx->sb, "__cco_map_has(");
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
            } else if (strcmp(callee, "remove") == 0) {
                AstNode *m_arg = expr->as.call.args[0];
                AstNode *k_arg = expr->as.call.args[1];
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
                Type elem_type = infer_expr_type(ctx->program, ctx->current_function, arr_arg);
                const char *elem_cls = get_expr_elem_class_name(ctx, arr_arg);
                const char *elem_c_type = c_type_str_full(ctx, elem_type, elem_cls, false, false);
                const char *arr_ptr_type = c_type_str_decl(ctx, elem_type, elem_cls, true, false);

                sb_append(&ctx->sb, "(");
                gen_expr(ctx, arr_arg);
                sb_appendf(&ctx->sb, " = (%s)__cco_arr_maybe_grow(", arr_ptr_type);
                gen_expr(ctx, arr_arg);
                sb_appendf(&ctx->sb, ", sizeof(%s)), (", elem_c_type);

                sb_appendf(&ctx->sb, "(%s)", arr_ptr_type);
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, ")[__cco_arr_len_raw(");
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, ")] = ");
                gen_expr(ctx, val_arg);
                sb_append(&ctx->sb, ", __cco_arr_incr_len(");
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, "), ");
                gen_expr(ctx, arr_arg);
                sb_append(&ctx->sb, ")");
            } else if (strcmp(callee, "pop") == 0) {
                AstNode *arr_arg = expr->as.call.args[0];
                Type elem_type = infer_expr_type(ctx->program, ctx->current_function, arr_arg);
                const char *elem_cls = get_expr_elem_class_name(ctx, arr_arg);
                const char *arr_ptr_type = c_type_str_decl(ctx, elem_type, elem_cls, true, false);

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
            strcmp(name, "read_line") == 0 ||
            strcmp(name, "program_name") == 0);
}

static void gen_stmt(CodegenCtx *ctx, AstNode *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case NODE_LET:
            emit_indent(ctx);
            sb_appendf(&ctx->sb, "%s %s = ", c_type_str_decl_full(ctx, stmt->as.let.var_type, stmt->as.let.class_name, stmt->as.let.is_array, stmt->as.let.is_map, stmt->is_heap_owner), stmt->as.let.name);
            if (stmt->as.let.var_type == TY_STRING && stmt->as.let.value->type == NODE_LITERAL) {
                sb_appendf(&ctx->sb, "strdup(\"%s\")", stmt->as.let.value->as.literal.val.s);
            } else {
                gen_expr(ctx, stmt->as.let.value);
            }
            sb_append(&ctx->sb, ";\n");
            break;

        case NODE_ASSIGN: {
            bool has_cleanup = (stmt->frees_count > 0) || (stmt->as.assign.release_old && stmt->as.assign.class_name);
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
                emit_frees(ctx, stmt);
                if (stmt->as.assign.release_old && stmt->as.assign.class_name) {
                    emit_indent(ctx);
                    sb_appendf(&ctx->sb, "%s_free(%s);\n", stmt->as.assign.class_name, stmt->as.assign.name);
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
            break;
        }

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
                    const char *ret_type_str = c_type_str_decl(ctx, ret_type, ret_class, ret_is_array, ret_heap);
                    sb_appendf(&ctx->sb, "%s __cco_ret_val = ", ret_type_str);
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
                }
            }
            sb_append(&ctx->sb, ") {\n");
            sb_appendf(&ctx->sb, "    %s *p = (%s *)malloc(sizeof(%s));\n", ename, ename, ename);
            sb_appendf(&ctx->sb, "    p->tag = %s_TAG_%s;\n", upper_ename, vname);
            if (!vn->as.variant_decl.is_unit && vn->as.variant_decl.field_count > 0) {
                for (int f = 0; f < vn->as.variant_decl.field_count; f++) {
                    AstNode *fn = vn->as.variant_decl.fields[f];
                    sb_appendf(&ctx->sb, "    p->as.%s.%s = %s;\n", vname, fn->as.field.name, fn->as.field.name);
                }
            }
            sb_append(&ctx->sb, "    return p;\n");
            sb_append(&ctx->sb, "}\n\n");
        }
        free(upper_ename);
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
    const char *ret_type_str = c_type_str_decl_full(ctx, m_node->as.method.return_type, m_node->as.method.return_class_name, m_node->as.method.return_is_array, m_node->as.method.return_is_map, m_node->as.method.returns_heap_pointer);
    sb_appendf(&ctx->sb, "%s %s_%s(",
               ret_type_str,
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
    bool is_main = (strcmp(fn->as.function.name, "main") == 0);
    if (is_main) {
        sb_append(&ctx->sb, "int main(int __cco_main_argc, char **__cco_main_argv) ");
    } else {
        const char *ret_type_str = c_type_str_decl_full(ctx, fn->as.function.return_type, fn->as.function.return_class_name, fn->as.function.return_is_array, fn->as.function.return_is_map, fn->as.function.returns_heap_pointer);
        sb_appendf(&ctx->sb, "%s %s(", ret_type_str, fn->as.function.name);

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
    }

    gen_block(ctx, fn->as.function.body);
    sb_append(&ctx->sb, "\n");
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

static void check_releases_usage(AstNode *node, bool *used_chunks) {
    if (!node) return;
    for (int i = 0; i < node->releases_count; i++) {
        if (node->releases_to_emit && node->releases_to_emit[i].is_map) {
            mark_chunk_used(used_chunks, "map_free");
        } else if (node->releases_to_emit && node->releases_to_emit[i].is_array) {
            mark_chunk_used(used_chunks, "free_arr");
        }
    }
    if (node->frees_count > 0) {
        mark_chunk_used(used_chunks, "free_arr");
    }
}

static void scan_node_usage(AstNode *node, bool *used_chunks) {
    if (!node) return;

    check_releases_usage(node, used_chunks);
    if (node->frees_count > 0) {
        mark_chunk_used(used_chunks, "free_arr");
    }

    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.class_count; i++) {
                scan_node_usage(node->as.program.classes[i], used_chunks);
            }
            for (int i = 0; i < node->as.program.struct_count; i++) {
                scan_node_usage(node->as.program.structs[i], used_chunks);
            }
            for (int i = 0; i < node->as.program.count; i++) {
                scan_node_usage(node->as.program.functions[i], used_chunks);
            }
            break;

        case NODE_CLASS:
            for (int m = 0; m < node->as.class_decl.method_count; m++) {
                scan_node_usage(node->as.class_decl.methods[m], used_chunks);
            }
            break;

        case NODE_FUNCTION:
            scan_node_usage(node->as.function.body, used_chunks);
            break;

        case NODE_METHOD:
            scan_node_usage(node->as.method.body, used_chunks);
            break;

        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                scan_node_usage(node->as.block.stmts[i], used_chunks);
            }
            break;

        case NODE_LET:
            if (node->as.let.is_array) {
                mark_chunk_used(used_chunks, "free_arr");
            }
            scan_node_usage(node->as.let.value, used_chunks);
            break;

        case NODE_ASSIGN:
            scan_node_usage(node->as.assign.value, used_chunks);
            break;

        case NODE_MEMBER_ASSIGN:
            scan_node_usage(node->as.member_assign.object, used_chunks);
            scan_node_usage(node->as.member_assign.value, used_chunks);
            break;

        case NODE_INDEX_ASSIGN:
            mark_chunk_used(used_chunks, "bounds_check");
            scan_node_usage(node->as.index_assign.array_expr, used_chunks);
            scan_node_usage(node->as.index_assign.index, used_chunks);
            scan_node_usage(node->as.index_assign.value, used_chunks);
            break;

        case NODE_IF:
            scan_node_usage(node->as.if_stmt.cond, used_chunks);
            scan_node_usage(node->as.if_stmt.then_b, used_chunks);
            scan_node_usage(node->as.if_stmt.else_b, used_chunks);
            break;

        case NODE_WHILE:
            scan_node_usage(node->as.while_stmt.cond, used_chunks);
            scan_node_usage(node->as.while_stmt.body, used_chunks);
            break;

        case NODE_FOR:
            scan_node_usage(node->as.for_stmt.init, used_chunks);
            scan_node_usage(node->as.for_stmt.cond, used_chunks);
            scan_node_usage(node->as.for_stmt.step, used_chunks);
            scan_node_usage(node->as.for_stmt.body, used_chunks);
            break;

        case NODE_FOR_EACH:
            mark_chunk_used(used_chunks, "arr_len");
            scan_node_usage(node->as.for_each.collection_expr, used_chunks);
            scan_node_usage(node->as.for_each.body, used_chunks);
            break;

        case NODE_MATCH:
            scan_node_usage(node->as.match_stmt.expr, used_chunks);
            for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
                scan_node_usage(node->as.match_stmt.arms[a]->as.match_arm.body, used_chunks);
            }
            break;

        case NODE_RETURN:
            scan_node_usage(node->as.return_stmt.value, used_chunks);
            break;

        case NODE_PRINT:
            scan_node_usage(node->as.print_stmt.value, used_chunks);
            break;

        case NODE_EXPR_STMT:
            scan_node_usage(node->as.expr_stmt.expr, used_chunks);
            break;

        case NODE_BINARY:
            scan_node_usage(node->as.binary.left, used_chunks);
            scan_node_usage(node->as.binary.right, used_chunks);
            break;

        case NODE_UNARY:
            scan_node_usage(node->as.unary.operand, used_chunks);
            break;

        case NODE_INDEX:
            mark_chunk_used(used_chunks, "bounds_check");
            scan_node_usage(node->as.index.array_expr, used_chunks);
            scan_node_usage(node->as.index.index, used_chunks);
            break;

        case NODE_ALLOC:
            if (node->as.alloc.is_map) {
                mark_chunk_used(used_chunks, "map_new");
            } else if (node->as.alloc.is_list) {
                mark_chunk_used(used_chunks, "list_new");
            } else {
                mark_chunk_used(used_chunks, "alloc_arr");
            }
            if (node->as.alloc.elem_type == TY_CLASS) {
                mark_chunk_used(used_chunks, "free_arr");
            }
            scan_node_usage(node->as.alloc.count_expr, used_chunks);
            break;

        case NODE_CALL: {
            const char *callee = node->as.call.callee;
            if (callee) {
                if (strcmp(callee, "concat") == 0) mark_chunk_used(used_chunks, "concat");
                else if (strcmp(callee, "char_at") == 0) mark_chunk_used(used_chunks, "char_at");
                else if (strcmp(callee, "substring") == 0) mark_chunk_used(used_chunks, "substring");
                else if (strcmp(callee, "abs_int") == 0) mark_chunk_used(used_chunks, "abs_int");
                else if (strcmp(callee, "min_int") == 0) mark_chunk_used(used_chunks, "min_int");
                else if (strcmp(callee, "max_int") == 0) mark_chunk_used(used_chunks, "max_int");
                else if (strcmp(callee, "min_float") == 0) mark_chunk_used(used_chunks, "min_float");
                else if (strcmp(callee, "max_float") == 0) mark_chunk_used(used_chunks, "max_float");
                else if (strcmp(callee, "read_file") == 0) mark_chunk_used(used_chunks, "read_file");
                else if (strcmp(callee, "write_file") == 0) mark_chunk_used(used_chunks, "write_file");
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
            }
            for (int i = 0; i < node->as.call.arg_count; i++) {
                scan_node_usage(node->as.call.args[i], used_chunks);
            }
            break;
        }

        case NODE_METHOD_CALL:
            scan_node_usage(node->as.method_call.object, used_chunks);
            for (int i = 0; i < node->as.method_call.arg_count; i++) {
                scan_node_usage(node->as.method_call.args[i], used_chunks);
            }
            break;

        case NODE_NEW:
            for (int i = 0; i < node->as.new_expr.field_count; i++) {
                scan_node_usage(node->as.new_expr.field_values[i], used_chunks);
            }
            break;

        case NODE_MEMBER:
            scan_node_usage(node->as.member.object, used_chunks);
            break;

        case NODE_FSTRING:
            mark_chunk_used(used_chunks, "concat_free");
            mark_chunk_used(used_chunks, "int_to_str");
            mark_chunk_used(used_chunks, "float_to_str");
            mark_chunk_used(used_chunks, "bool_to_str");
            mark_chunk_used(used_chunks, "char_to_str");
            for (int i = 0; i < node->as.fstring.part_count; i++) {
                AstNode *part = node->as.fstring.parts[i];
                if (part->type != NODE_FSTRING_TEXT) {
                    scan_node_usage(part, used_chunks);
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
    sb_append(&ctx.sb, "#include <string.h>\n");
    sb_append(&ctx.sb, "#include <math.h>\n");
    sb_append(&ctx.sb, "#include <time.h>\n");
    sb_append(&ctx.sb, "#include <errno.h>\n\n");
    sb_append(&ctx.sb, "static int __cco_argc;\n");
    sb_append(&ctx.sb, "static char **__cco_argv;\n\n");

    bool used_chunks[PRELUDE_CHUNK_COUNT];
    memset(used_chunks, 0, sizeof(used_chunks));
    scan_node_usage(program, used_chunks);
    resolve_transitive_dependencies(used_chunks);

    for (size_t i = 0; i < PRELUDE_CHUNK_COUNT; i++) {
        if (used_chunks[i]) {
            sb_append(&ctx.sb, PRELUDE_CHUNKS[i].c_source);
        }
    }

    // Struct definitions
    gen_struct_typedefs(&ctx, program);

    // Enum definitions, tag enum, union struct, _free, and _new_<variant> helpers
    gen_enum_helpers(&ctx, program);

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
