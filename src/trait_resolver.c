#define _POSIX_C_SOURCE 200809L
#include "trait_resolver.h"
#include "class_decl.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    char *interface_name;
    char *class_name;
} ConfirmedImpl;

typedef struct {
    ConfirmedImpl *items;
    int count;
    int capacity;
} ImplTable;

static void add_confirmed_impl(ImplTable *table, const char *iface, const char *cls) {
    if (table->count >= table->capacity) {
        table->capacity = table->capacity == 0 ? 8 : table->capacity * 2;
        table->items = realloc(table->items, table->capacity * sizeof(ConfirmedImpl));
    }
    table->items[table->count].interface_name = strdup(iface);
    table->items[table->count].class_name = strdup(cls);
    table->count++;
}

static bool has_confirmed_impl(ImplTable *table, const char *iface, const char *cls) {
    if (!table || !iface || !cls) return false;
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].interface_name, iface) == 0 &&
            strcmp(table->items[i].class_name, cls) == 0) {
            return true;
        }
    }
    return false;
}

static const char *type_to_str(Type t) {
    switch (t) {
        case TY_INT: return "int";
        case TY_FLOAT: return "float";
        case TY_CHAR: return "char";
        case TY_BOOL: return "bool";
        case TY_STRING: return "string";
        case TY_VOID: return "void";
        case TY_CLASS: return "class";
        case TY_MAP: return "map";
        case TY_ENUM: return "enum";
    }
    return "unknown";
}

static char *format_sig(const char *name, int param_count, char **param_names, Type *param_types, char **param_cls, bool *param_borrowed, bool *param_is_arr, Type ret_type, char *ret_cls, bool ret_is_arr, bool ret_is_map, Type ret_key_t) {
    char buf[512];
    int offset = snprintf(buf, sizeof(buf), "fn %s(self", name);
    for (int i = 1; i < param_count; i++) {
        const char *pname = (param_names && param_names[i]) ? param_names[i] : "arg";
        const char *bor = (param_borrowed && param_borrowed[i]) ? "&" : "";
        const char *tname = "";
        if (param_types && param_types[i] == TY_CLASS && param_cls && param_cls[i]) {
            tname = param_cls[i];
        } else {
            tname = type_to_str(param_types ? param_types[i] : TY_VOID);
        }
        const char *arr = (param_is_arr && param_is_arr[i]) ? "[]" : "";
        offset += snprintf(buf + offset, sizeof(buf) - offset, ", %s: %s%s%s", pname, bor, tname, arr);
    }
    const char *ret_name = "";
    char rbuf[128];
    if (ret_is_map) {
        const char *val_t = (ret_type == TY_CLASS && ret_cls) ? ret_cls : type_to_str(ret_type);
        snprintf(rbuf, sizeof(rbuf), "map[%s]%s", type_to_str(ret_key_t), val_t);
        ret_name = rbuf;
    } else if (ret_type == TY_CLASS && ret_cls) {
        ret_name = ret_cls;
    } else {
        ret_name = type_to_str(ret_type);
    }
    const char *rarr = ret_is_arr ? "[]" : "";
    snprintf(buf + offset, sizeof(buf) - offset, ") -> %s%s", ret_name, rarr);
    return strdup(buf);
}

// AST Deep Cloner
static AstNode *clone_node(AstNode *node, AstArena *arena) {
    if (!node) return NULL;
    AstNode *c = arena_alloc_node(arena, node->type, node->line, node->col);
    c->source_file = node->source_file;
    c->is_heap_owner = node->is_heap_owner;
    c->is_transferred = node->is_transferred;
    c->free_old_on_reassign = node->free_old_on_reassign;
    c->is_moved_from = node->is_moved_from;
    c->move_line = node->move_line;

    switch (node->type) {
        case NODE_PROGRAM:
            break;
        case NODE_IMPORT:
            c->as.import_stmt.path = arena_strdup(arena, node->as.import_stmt.path);
            break;
        case NODE_CLASS:
            c->as.class_decl.name = arena_strdup(arena, node->as.class_decl.name);
            c->as.class_decl.field_count = node->as.class_decl.field_count;
            if (c->as.class_decl.field_count > 0) {
                c->as.class_decl.fields = (AstNode **)arena_alloc_array(arena, c->as.class_decl.field_count, sizeof(AstNode *));
                for (int i = 0; i < c->as.class_decl.field_count; i++) {
                    c->as.class_decl.fields[i] = clone_node(node->as.class_decl.fields[i], arena);
                }
            }
            c->as.class_decl.method_count = node->as.class_decl.method_count;
            if (c->as.class_decl.method_count > 0) {
                c->as.class_decl.methods = (AstNode **)arena_alloc_array(arena, c->as.class_decl.method_count, sizeof(AstNode *));
                for (int i = 0; i < c->as.class_decl.method_count; i++) {
                    c->as.class_decl.methods[i] = clone_node(node->as.class_decl.methods[i], arena);
                }
            }
            break;
        case NODE_FIELD:
            c->as.field.name = arena_strdup(arena, node->as.field.name);
            c->as.field.type = node->as.field.type;
            c->as.field.is_array = node->as.field.is_array;
            c->as.field.is_map = node->as.field.is_map;
            c->as.field.key_type = node->as.field.key_type;
            c->as.field.class_name = node->as.field.class_name ? arena_strdup(arena, node->as.field.class_name) : NULL;
            break;
        case NODE_METHOD:
            c->as.method.name = arena_strdup(arena, node->as.method.name);
            c->as.method.has_self = node->as.method.has_self;
            c->as.method.param_count = node->as.method.param_count;
            if (c->as.method.param_count > 0) {
                c->as.method.param_names = (char **)arena_alloc_array(arena, c->as.method.param_count, sizeof(char *));
                c->as.method.param_types = (Type *)arena_alloc_array(arena, c->as.method.param_count, sizeof(Type));
                c->as.method.param_is_array = (bool *)arena_alloc_array(arena, c->as.method.param_count, sizeof(bool));
                c->as.method.param_class_names = (char **)arena_alloc_array(arena, c->as.method.param_count, sizeof(char *));
                c->as.method.param_is_borrowed = (bool *)arena_alloc_array(arena, c->as.method.param_count, sizeof(bool));
                c->as.method.param_lines = (int *)arena_alloc_array(arena, c->as.method.param_count, sizeof(int));
                c->as.method.param_cols = (int *)arena_alloc_array(arena, c->as.method.param_count, sizeof(int));
                for (int i = 0; i < c->as.method.param_count; i++) {
                    c->as.method.param_names[i] = arena_strdup(arena, node->as.method.param_names[i]);
                    c->as.method.param_types[i] = node->as.method.param_types[i];
                    if (node->as.method.param_is_array) c->as.method.param_is_array[i] = node->as.method.param_is_array[i];
                    if (node->as.method.param_class_names && node->as.method.param_class_names[i]) {
                        c->as.method.param_class_names[i] = arena_strdup(arena, node->as.method.param_class_names[i]);
                    }
                    if (node->as.method.param_is_borrowed) c->as.method.param_is_borrowed[i] = node->as.method.param_is_borrowed[i];
                    if (node->as.method.param_lines) c->as.method.param_lines[i] = node->as.method.param_lines[i];
                    if (node->as.method.param_cols) c->as.method.param_cols[i] = node->as.method.param_cols[i];
                }
            }
            c->as.method.return_type = node->as.method.return_type;
            c->as.method.return_is_array = node->as.method.return_is_array;
            c->as.method.return_is_map = node->as.method.return_is_map;
            c->as.method.return_key_type = node->as.method.return_key_type;
            c->as.method.return_class_name = node->as.method.return_class_name ? arena_strdup(arena, node->as.method.return_class_name) : NULL;
            c->as.method.returns_heap_pointer = node->as.method.returns_heap_pointer;
            c->as.method.body = clone_node(node->as.method.body, arena);
            break;
        case NODE_FUNCTION:
            c->as.function.name = arena_strdup(arena, node->as.function.name);
            c->as.function.is_operator = node->as.function.is_operator;
            c->as.function.operator_symbol = node->as.function.operator_symbol ? arena_strdup(arena, node->as.function.operator_symbol) : NULL;
            c->as.function.param_count = node->as.function.param_count;
            if (c->as.function.param_count > 0) {
                c->as.function.param_names = (char **)arena_alloc_array(arena, c->as.function.param_count, sizeof(char *));
                c->as.function.param_types = (Type *)arena_alloc_array(arena, c->as.function.param_count, sizeof(Type));
                c->as.function.param_is_array = (bool *)arena_alloc_array(arena, c->as.function.param_count, sizeof(bool));
                c->as.function.param_class_names = (char **)arena_alloc_array(arena, c->as.function.param_count, sizeof(char *));
                c->as.function.param_is_borrowed = (bool *)arena_alloc_array(arena, c->as.function.param_count, sizeof(bool));
                c->as.function.param_is_impl_trait = (bool *)arena_alloc_array(arena, c->as.function.param_count, sizeof(bool));
                c->as.function.param_impl_trait_names = (char **)arena_alloc_array(arena, c->as.function.param_count, sizeof(char *));
                c->as.function.param_lines = (int *)arena_alloc_array(arena, c->as.function.param_count, sizeof(int));
                c->as.function.param_cols = (int *)arena_alloc_array(arena, c->as.function.param_count, sizeof(int));
                for (int i = 0; i < c->as.function.param_count; i++) {
                    c->as.function.param_names[i] = arena_strdup(arena, node->as.function.param_names[i]);
                    c->as.function.param_types[i] = node->as.function.param_types[i];
                    if (node->as.function.param_is_array) c->as.function.param_is_array[i] = node->as.function.param_is_array[i];
                    if (node->as.function.param_class_names && node->as.function.param_class_names[i]) {
                        c->as.function.param_class_names[i] = arena_strdup(arena, node->as.function.param_class_names[i]);
                    }
                    if (node->as.function.param_is_borrowed) c->as.function.param_is_borrowed[i] = node->as.function.param_is_borrowed[i];
                    if (node->as.function.param_is_impl_trait) c->as.function.param_is_impl_trait[i] = node->as.function.param_is_impl_trait[i];
                    if (node->as.function.param_impl_trait_names && node->as.function.param_impl_trait_names[i]) {
                        c->as.function.param_impl_trait_names[i] = arena_strdup(arena, node->as.function.param_impl_trait_names[i]);
                    }
                    if (node->as.function.param_lines) c->as.function.param_lines[i] = node->as.function.param_lines[i];
                    if (node->as.function.param_cols) c->as.function.param_cols[i] = node->as.function.param_cols[i];
                }
            }
            c->as.function.return_type = node->as.function.return_type;
            c->as.function.return_is_array = node->as.function.return_is_array;
            c->as.function.return_is_map = node->as.function.return_is_map;
            c->as.function.return_key_type = node->as.function.return_key_type;
            c->as.function.return_class_name = node->as.function.return_class_name ? arena_strdup(arena, node->as.function.return_class_name) : NULL;
            c->as.function.returns_heap_pointer = node->as.function.returns_heap_pointer;
            c->as.function.body = clone_node(node->as.function.body, arena);
            break;
        case NODE_BLOCK:
            c->as.block.count = node->as.block.count;
            if (c->as.block.count > 0) {
                c->as.block.stmts = (AstNode **)arena_alloc_array(arena, c->as.block.count, sizeof(AstNode *));
                for (int i = 0; i < c->as.block.count; i++) {
                    c->as.block.stmts[i] = clone_node(node->as.block.stmts[i], arena);
                }
            }
            break;
        case NODE_LET:
            c->as.let.name = arena_strdup(arena, node->as.let.name);
            c->as.let.var_type = node->as.let.var_type;
            c->as.let.is_array = node->as.let.is_array;
            c->as.let.is_map = node->as.let.is_map;
            c->as.let.key_type = node->as.let.key_type;
            c->as.let.class_name = node->as.let.class_name ? arena_strdup(arena, node->as.let.class_name) : NULL;
            c->as.let.value = clone_node(node->as.let.value, arena);
            c->as.let.retain_rhs = node->as.let.retain_rhs;
            break;
        case NODE_ASSIGN:
            c->as.assign.name = arena_strdup(arena, node->as.assign.name);
            c->as.assign.value = clone_node(node->as.assign.value, arena);
            c->as.assign.class_name = node->as.assign.class_name ? arena_strdup(arena, node->as.assign.class_name) : NULL;
            c->as.assign.retain_rhs = node->as.assign.retain_rhs;
            c->as.assign.release_old = node->as.assign.release_old;
            break;
        case NODE_MEMBER_ASSIGN:
            c->as.member_assign.object = clone_node(node->as.member_assign.object, arena);
            c->as.member_assign.member_name = arena_strdup(arena, node->as.member_assign.member_name);
            c->as.member_assign.value = clone_node(node->as.member_assign.value, arena);
            c->as.member_assign.field_class_name = node->as.member_assign.field_class_name ? arena_strdup(arena, node->as.member_assign.field_class_name) : NULL;
            c->as.member_assign.retain_rhs = node->as.member_assign.retain_rhs;
            c->as.member_assign.release_old = node->as.member_assign.release_old;
            break;
        case NODE_INDEX_ASSIGN:
            c->as.index_assign.array_expr = clone_node(node->as.index_assign.array_expr, arena);
            c->as.index_assign.array_name = node->as.index_assign.array_name ? arena_strdup(arena, node->as.index_assign.array_name) : NULL;
            c->as.index_assign.index = clone_node(node->as.index_assign.index, arena);
            c->as.index_assign.value = clone_node(node->as.index_assign.value, arena);
            break;
        case NODE_IF:
            c->as.if_stmt.cond = clone_node(node->as.if_stmt.cond, arena);
            c->as.if_stmt.then_b = clone_node(node->as.if_stmt.then_b, arena);
            c->as.if_stmt.else_b = clone_node(node->as.if_stmt.else_b, arena);
            break;
        case NODE_WHILE:
            c->as.while_stmt.cond = clone_node(node->as.while_stmt.cond, arena);
            c->as.while_stmt.body = clone_node(node->as.while_stmt.body, arena);
            break;
        case NODE_FOR:
            c->as.for_stmt.init = clone_node(node->as.for_stmt.init, arena);
            c->as.for_stmt.cond = clone_node(node->as.for_stmt.cond, arena);
            c->as.for_stmt.step = clone_node(node->as.for_stmt.step, arena);
            c->as.for_stmt.body = clone_node(node->as.for_stmt.body, arena);
            break;
        case NODE_FOR_EACH:
            c->as.for_each.loop_var_name = arena_strdup(arena, node->as.for_each.loop_var_name);
            c->as.for_each.collection_expr = clone_node(node->as.for_each.collection_expr, arena);
            c->as.for_each.body = clone_node(node->as.for_each.body, arena);
            break;
        case NODE_RETURN:
            c->as.return_stmt.value = clone_node(node->as.return_stmt.value, arena);
            break;
        case NODE_BREAK:
        case NODE_CONTINUE:
            break;
        case NODE_PRINT:
            c->as.print_stmt.value = clone_node(node->as.print_stmt.value, arena);
            break;
        case NODE_EXPR_STMT:
            c->as.expr_stmt.expr = clone_node(node->as.expr_stmt.expr, arena);
            break;
        case NODE_BINARY:
            c->as.binary.left = clone_node(node->as.binary.left, arena);
            c->as.binary.right = clone_node(node->as.binary.right, arena);
            strncpy(c->as.binary.op, node->as.binary.op, sizeof(c->as.binary.op) - 1);
            c->as.binary.op[sizeof(c->as.binary.op) - 1] = '\0';
            break;
        case NODE_UNARY:
            c->as.unary.operand = clone_node(node->as.unary.operand, arena);
            strncpy(c->as.unary.op, node->as.unary.op, sizeof(c->as.unary.op) - 1);
            c->as.unary.op[sizeof(c->as.unary.op) - 1] = '\0';
            break;
        case NODE_LITERAL:
            c->as.literal.lit_type = node->as.literal.lit_type;
            c->as.literal.val = node->as.literal.val;
            if (node->as.literal.lit_type == TY_STRING && node->as.literal.val.s) {
                c->as.literal.val.s = arena_strdup(arena, node->as.literal.val.s);
            }
            break;
        case NODE_IDENT:
            c->as.ident.name = arena_strdup(arena, node->as.ident.name);
            break;
        case NODE_CALL:
            c->as.call.callee = arena_strdup(arena, node->as.call.callee);
            c->as.call.arg_count = node->as.call.arg_count;
            if (c->as.call.arg_count > 0) {
                c->as.call.args = (AstNode **)arena_alloc_array(arena, c->as.call.arg_count, sizeof(AstNode *));
                for (int i = 0; i < c->as.call.arg_count; i++) {
                    c->as.call.args[i] = clone_node(node->as.call.args[i], arena);
                }
            }
            break;
        case NODE_METHOD_CALL:
            c->as.method_call.object = clone_node(node->as.method_call.object, arena);
            c->as.method_call.method_name = arena_strdup(arena, node->as.method_call.method_name);
            c->as.method_call.target_class_name = node->as.method_call.target_class_name ? arena_strdup(arena, node->as.method_call.target_class_name) : NULL;
            c->as.method_call.arg_count = node->as.method_call.arg_count;
            if (c->as.method_call.arg_count > 0) {
                c->as.method_call.args = (AstNode **)arena_alloc_array(arena, c->as.method_call.arg_count, sizeof(AstNode *));
                for (int i = 0; i < c->as.method_call.arg_count; i++) {
                    c->as.method_call.args[i] = clone_node(node->as.method_call.args[i], arena);
                }
            }
            break;
        case NODE_MEMBER:
            c->as.member.object = clone_node(node->as.member.object, arena);
            c->as.member.member_name = arena_strdup(arena, node->as.member.member_name);
            c->as.member.field_class_name = node->as.member.field_class_name ? arena_strdup(arena, node->as.member.field_class_name) : NULL;
            break;
        case NODE_INDEX:
            c->as.index.array_name = node->as.index.array_name ? arena_strdup(arena, node->as.index.array_name) : NULL;
            c->as.index.array_expr = clone_node(node->as.index.array_expr, arena);
            c->as.index.index = clone_node(node->as.index.index, arena);
            break;
        case NODE_ALLOC:
            c->as.alloc.elem_type = node->as.alloc.elem_type;
            c->as.alloc.class_name = node->as.alloc.class_name ? arena_strdup(arena, node->as.alloc.class_name) : NULL;
            c->as.alloc.count_expr = clone_node(node->as.alloc.count_expr, arena);
            c->as.alloc.is_list = node->as.alloc.is_list;
            c->as.alloc.is_map = node->as.alloc.is_map;
            c->as.alloc.key_type = node->as.alloc.key_type;
            break;
        case NODE_NEW:
            c->as.new_expr.class_name = node->as.new_expr.class_name ? arena_strdup(arena, node->as.new_expr.class_name) : NULL;
            c->as.new_expr.variant_name = node->as.new_expr.variant_name ? arena_strdup(arena, node->as.new_expr.variant_name) : NULL;
            c->as.new_expr.constructs_struct = node->as.new_expr.constructs_struct;
            c->as.new_expr.constructs_enum = node->as.new_expr.constructs_enum;
            c->as.new_expr.field_count = node->as.new_expr.field_count;
            if (c->as.new_expr.field_count > 0) {
                c->as.new_expr.field_names = (char **)arena_alloc_array(arena, c->as.new_expr.field_count, sizeof(char *));
                c->as.new_expr.field_values = (AstNode **)arena_alloc_array(arena, c->as.new_expr.field_count, sizeof(AstNode *));
                for (int i = 0; i < c->as.new_expr.field_count; i++) {
                    c->as.new_expr.field_names[i] = arena_strdup(arena, node->as.new_expr.field_names[i]);
                    c->as.new_expr.field_values[i] = clone_node(node->as.new_expr.field_values[i], arena);
                }
            }
            break;
        case NODE_MATCH:
            c->as.match_stmt.expr = clone_node(node->as.match_stmt.expr, arena);
            c->as.match_stmt.enum_name = node->as.match_stmt.enum_name ? arena_strdup(arena, node->as.match_stmt.enum_name) : NULL;
            c->as.match_stmt.arm_count = node->as.match_stmt.arm_count;
            if (c->as.match_stmt.arm_count > 0) {
                c->as.match_stmt.arms = (AstNode **)arena_alloc_array(arena, c->as.match_stmt.arm_count, sizeof(AstNode *));
                for (int i = 0; i < c->as.match_stmt.arm_count; i++) {
                    c->as.match_stmt.arms[i] = clone_node(node->as.match_stmt.arms[i], arena);
                }
            }
            break;
        case NODE_MATCH_ARM:
            c->as.match_arm.enum_name = node->as.match_arm.enum_name ? arena_strdup(arena, node->as.match_arm.enum_name) : NULL;
            c->as.match_arm.variant_name = node->as.match_arm.variant_name ? arena_strdup(arena, node->as.match_arm.variant_name) : NULL;
            c->as.match_arm.is_wildcard = node->as.match_arm.is_wildcard;
            c->as.match_arm.bind_count = node->as.match_arm.bind_count;
            if (c->as.match_arm.bind_count > 0) {
                c->as.match_arm.bind_names = (char **)arena_alloc_array(arena, c->as.match_arm.bind_count, sizeof(char *));
                for (int i = 0; i < c->as.match_arm.bind_count; i++) {
                    c->as.match_arm.bind_names[i] = arena_strdup(arena, node->as.match_arm.bind_names[i]);
                }
            }
            c->as.match_arm.body = clone_node(node->as.match_arm.body, arena);
            c->as.match_arm.arm_line = node->as.match_arm.arm_line;
            c->as.match_arm.arm_col = node->as.match_arm.arm_col;
            break;
        case NODE_FSTRING:
            c->as.fstring.part_count = node->as.fstring.part_count;
            if (c->as.fstring.part_count > 0) {
                c->as.fstring.parts = (AstNode **)arena_alloc_array(arena, c->as.fstring.part_count, sizeof(AstNode *));
                for (int i = 0; i < c->as.fstring.part_count; i++) {
                    c->as.fstring.parts[i] = clone_node(node->as.fstring.parts[i], arena);
                }
            }
            break;
        case NODE_FSTRING_TEXT:
            c->as.fstring_text.text = arena_strdup(arena, node->as.fstring_text.text);
            break;
        default:
            break;
    }
    return c;
}

// Find concrete class name of an expression in a function/method scope
static const char *determine_expr_class(AstNode *prog, AstNode *fn, AstNode *expr) {
    if (!expr) return NULL;
    if (expr->type == NODE_IDENT) {
        const char *var_name = expr->as.ident.name;
        if (fn) {
            // Check params
            if (fn->type == NODE_FUNCTION) {
                for (int p = 0; p < fn->as.function.param_count; p++) {
                    if (strcmp(fn->as.function.param_names[p], var_name) == 0) {
                        return fn->as.function.param_class_names[p];
                    }
                }
                AstNode *body = fn->as.function.body;
                if (body && body->type == NODE_BLOCK) {
                    for (int i = 0; i < body->as.block.count; i++) {
                        AstNode *stmt = body->as.block.stmts[i];
                        if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                            if (stmt->as.let.class_name) return stmt->as.let.class_name;
                            if (stmt->as.let.value) return determine_expr_class(prog, fn, stmt->as.let.value);
                        }
                    }
                }
            } else if (fn->type == NODE_METHOD) {
                for (int p = 0; p < fn->as.method.param_count; p++) {
                    if (strcmp(fn->as.method.param_names[p], var_name) == 0) {
                        return fn->as.method.param_class_names[p];
                    }
                }
                AstNode *body = fn->as.method.body;
                if (body && body->type == NODE_BLOCK) {
                    for (int i = 0; i < body->as.block.count; i++) {
                        AstNode *stmt = body->as.block.stmts[i];
                        if (stmt->type == NODE_LET && strcmp(stmt->as.let.name, var_name) == 0) {
                            if (stmt->as.let.class_name) return stmt->as.let.class_name;
                            if (stmt->as.let.value) return determine_expr_class(prog, fn, stmt->as.let.value);
                        }
                    }
                }
            }
        }
    } else if (expr->type == NODE_ALLOC) {
        return expr->as.alloc.class_name;
    } else if (expr->type == NODE_NEW) {
        return expr->as.new_expr.class_name;
    } else if (expr->type == NODE_CALL) {
        const char *callee = expr->as.call.callee;
        if (prog && prog->type == NODE_PROGRAM) {
            for (int f = 0; f < prog->as.program.count; f++) {
                AstNode *target_fn = prog->as.program.functions[f];
                if (strcmp(target_fn->as.function.name, callee) == 0) {
                    return target_fn->as.function.return_class_name;
                }
            }
        }
    }
    return NULL;
}

// Check template body method calls
static void validate_template_body(AstNode *fn, AstNode *node, AstNode *iface, const char *param_name, const char *iface_name) {
    if (!node) return;
    if (node->type == NODE_METHOD_CALL) {
        if (node->as.method_call.object && node->as.method_call.object->type == NODE_IDENT &&
            strcmp(node->as.method_call.object->as.ident.name, param_name) == 0) {
            const char *mname = node->as.method_call.method_name;
            bool found = false;
            for (int m = 0; m < iface->as.interface_decl.method_count; m++) {
                if (strcmp(iface->as.interface_decl.methods[m]->as.interface_method.name, mname) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "'%s' is not part of interface '%s' — only interface methods can be called on a `impl Trait`-typed parameter", mname, iface_name);
                ErrorLocation loc = {fn->source_file ? fn->source_file : get_error_filename(), node->line, node->col};
                print_formatted_error(short_msg, loc, "not part of interface", NULL, NULL, NULL, NULL);
                exit(1);
            }
        }
    }

    switch (node->type) {
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                validate_template_body(fn, node->as.block.stmts[i], iface, param_name, iface_name);
            }
            break;
        case NODE_IF:
            validate_template_body(fn, node->as.if_stmt.cond, iface, param_name, iface_name);
            validate_template_body(fn, node->as.if_stmt.then_b, iface, param_name, iface_name);
            validate_template_body(fn, node->as.if_stmt.else_b, iface, param_name, iface_name);
            break;
        case NODE_WHILE:
            validate_template_body(fn, node->as.while_stmt.cond, iface, param_name, iface_name);
            validate_template_body(fn, node->as.while_stmt.body, iface, param_name, iface_name);
            break;
        case NODE_FOR:
            validate_template_body(fn, node->as.for_stmt.init, iface, param_name, iface_name);
            validate_template_body(fn, node->as.for_stmt.cond, iface, param_name, iface_name);
            validate_template_body(fn, node->as.for_stmt.step, iface, param_name, iface_name);
            validate_template_body(fn, node->as.for_stmt.body, iface, param_name, iface_name);
            break;
        case NODE_FOR_EACH:
            validate_template_body(fn, node->as.for_each.collection_expr, iface, param_name, iface_name);
            validate_template_body(fn, node->as.for_each.body, iface, param_name, iface_name);
            break;
        case NODE_LET:
            validate_template_body(fn, node->as.let.value, iface, param_name, iface_name);
            break;
        case NODE_ASSIGN:
            validate_template_body(fn, node->as.assign.value, iface, param_name, iface_name);
            break;
        case NODE_MEMBER_ASSIGN:
            validate_template_body(fn, node->as.member_assign.object, iface, param_name, iface_name);
            validate_template_body(fn, node->as.member_assign.value, iface, param_name, iface_name);
            break;
        case NODE_RETURN:
            validate_template_body(fn, node->as.return_stmt.value, iface, param_name, iface_name);
            break;
        case NODE_PRINT:
            validate_template_body(fn, node->as.print_stmt.value, iface, param_name, iface_name);
            break;
        case NODE_EXPR_STMT:
            validate_template_body(fn, node->as.expr_stmt.expr, iface, param_name, iface_name);
            break;
        case NODE_BINARY:
            validate_template_body(fn, node->as.binary.left, iface, param_name, iface_name);
            validate_template_body(fn, node->as.binary.right, iface, param_name, iface_name);
            break;
        case NODE_UNARY:
            validate_template_body(fn, node->as.unary.operand, iface, param_name, iface_name);
            break;
        case NODE_CALL:
            for (int i = 0; i < node->as.call.arg_count; i++) {
                validate_template_body(fn, node->as.call.args[i], iface, param_name, iface_name);
            }
            break;
        case NODE_METHOD_CALL:
            validate_template_body(fn, node->as.method_call.object, iface, param_name, iface_name);
            for (int i = 0; i < node->as.method_call.arg_count; i++) {
                validate_template_body(fn, node->as.method_call.args[i], iface, param_name, iface_name);
            }
            break;
        case NODE_MATCH:
            validate_template_body(fn, node->as.match_stmt.expr, iface, param_name, iface_name);
            for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
                validate_template_body(fn, node->as.match_stmt.arms[i]->as.match_arm.body, iface, param_name, iface_name);
            }
            break;
        default:
            break;
    }
}

typedef struct {
    AstNode *call_node;
    char *specialized_name;
} CallRewrite;

typedef struct {
    char *template_name;
    char **concrete_types;
    int param_count;
    char *specialized_name;
} Specialization;

typedef struct {
    Specialization *specs;
    int spec_count;
    int spec_cap;

    CallRewrite *rewrites;
    int rewrite_count;
    int rewrite_cap;
} MonomorphCtx;

static void record_call_site(MonomorphCtx *mctx, AstNode *prog, AstNode *current_fn, AstNode *call, ImplTable *impl_table) {
    if (!call || call->type != NODE_CALL) return;
    const char *callee = call->as.call.callee;

    // Look for callee in program functions
    AstNode *target_fn = NULL;
    for (int f = 0; f < prog->as.program.count; f++) {
        if (strcmp(prog->as.program.functions[f]->as.function.name, callee) == 0) {
            target_fn = prog->as.program.functions[f];
            break;
        }
    }
    if (!target_fn) return;

    // Check if target_fn has impl Trait params
    bool has_impl_trait = false;
    for (int p = 0; p < target_fn->as.function.param_count; p++) {
        if (target_fn->as.function.param_is_impl_trait && target_fn->as.function.param_is_impl_trait[p]) {
            has_impl_trait = true;
            break;
        }
    }
    if (!has_impl_trait) return;

    // Determine concrete class name for each impl trait parameter
    char spec_name[256];
    int offset = snprintf(spec_name, sizeof(spec_name), "%s", target_fn->as.function.name);

    char **concrete_types = malloc(target_fn->as.function.param_count * sizeof(char *));

    for (int p = 0; p < target_fn->as.function.param_count; p++) {
        if (target_fn->as.function.param_is_impl_trait && target_fn->as.function.param_is_impl_trait[p]) {
            const char *iname = target_fn->as.function.param_impl_trait_names[p];
            AstNode *arg = (p < call->as.call.arg_count) ? call->as.call.args[p] : NULL;
            const char *cname = determine_expr_class(prog, current_fn, arg);

            if (!cname || !has_confirmed_impl(impl_table, iname, cname)) {
                const char *display_name = cname ? cname : "unknown type";
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "'%s' does not implement '%s'", display_name, iname);
                ErrorLocation loc = {current_fn->source_file ? current_fn->source_file : get_error_filename(), arg ? arg->line : call->line, arg ? arg->col : call->col};
                print_formatted_error(short_msg, loc, "does not implement interface", NULL, NULL, NULL, NULL);
                exit(1);
            }

            offset += snprintf(spec_name + offset, sizeof(spec_name) - offset, "__%s", cname);
            concrete_types[p] = strdup(cname);
        } else {
            concrete_types[p] = NULL;
        }
    }

    // Record specialization if not yet present
    bool spec_exists = false;
    for (int s = 0; s < mctx->spec_count; s++) {
        if (strcmp(mctx->specs[s].specialized_name, spec_name) == 0) {
            spec_exists = true;
            break;
        }
    }
    if (!spec_exists) {
        if (mctx->spec_count >= mctx->spec_cap) {
            mctx->spec_cap = mctx->spec_cap == 0 ? 8 : mctx->spec_cap * 2;
            mctx->specs = realloc(mctx->specs, mctx->spec_cap * sizeof(Specialization));
        }
        mctx->specs[mctx->spec_count].template_name = strdup(target_fn->as.function.name);
        mctx->specs[mctx->spec_count].concrete_types = concrete_types;
        mctx->specs[mctx->spec_count].param_count = target_fn->as.function.param_count;
        mctx->specs[mctx->spec_count].specialized_name = strdup(spec_name);
        mctx->spec_count++;
    } else {
        for (int p = 0; p < target_fn->as.function.param_count; p++) {
            if (concrete_types[p]) free(concrete_types[p]);
        }
        free(concrete_types);
    }

    // Record call rewrite
    if (mctx->rewrite_count >= mctx->rewrite_cap) {
        mctx->rewrite_cap = mctx->rewrite_cap == 0 ? 8 : mctx->rewrite_cap * 2;
        mctx->rewrites = realloc(mctx->rewrites, mctx->rewrite_cap * sizeof(CallRewrite));
    }
    mctx->rewrites[mctx->rewrite_count].call_node = call;
    mctx->rewrites[mctx->rewrite_count].specialized_name = strdup(spec_name);
    mctx->rewrite_count++;
}

static void scan_and_record_calls(MonomorphCtx *mctx, AstNode *prog, AstNode *current_fn, AstNode *node, ImplTable *impl_table) {
    if (!node) return;
    if (node->type == NODE_CALL) {
        record_call_site(mctx, prog, current_fn, node, impl_table);
    }

    switch (node->type) {
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                scan_and_record_calls(mctx, prog, current_fn, node->as.block.stmts[i], impl_table);
            }
            break;
        case NODE_IF:
            scan_and_record_calls(mctx, prog, current_fn, node->as.if_stmt.cond, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.if_stmt.then_b, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.if_stmt.else_b, impl_table);
            break;
        case NODE_WHILE:
            scan_and_record_calls(mctx, prog, current_fn, node->as.while_stmt.cond, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.while_stmt.body, impl_table);
            break;
        case NODE_FOR:
            scan_and_record_calls(mctx, prog, current_fn, node->as.for_stmt.init, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.for_stmt.cond, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.for_stmt.step, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.for_stmt.body, impl_table);
            break;
        case NODE_FOR_EACH:
            scan_and_record_calls(mctx, prog, current_fn, node->as.for_each.collection_expr, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.for_each.body, impl_table);
            break;
        case NODE_LET:
            scan_and_record_calls(mctx, prog, current_fn, node->as.let.value, impl_table);
            break;
        case NODE_ASSIGN:
            scan_and_record_calls(mctx, prog, current_fn, node->as.assign.value, impl_table);
            break;
        case NODE_MEMBER_ASSIGN:
            scan_and_record_calls(mctx, prog, current_fn, node->as.member_assign.object, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.member_assign.value, impl_table);
            break;
        case NODE_RETURN:
            scan_and_record_calls(mctx, prog, current_fn, node->as.return_stmt.value, impl_table);
            break;
        case NODE_PRINT:
            scan_and_record_calls(mctx, prog, current_fn, node->as.print_stmt.value, impl_table);
            break;
        case NODE_EXPR_STMT:
            scan_and_record_calls(mctx, prog, current_fn, node->as.expr_stmt.expr, impl_table);
            break;
        case NODE_BINARY:
            scan_and_record_calls(mctx, prog, current_fn, node->as.binary.left, impl_table);
            scan_and_record_calls(mctx, prog, current_fn, node->as.binary.right, impl_table);
            break;
        case NODE_UNARY:
            scan_and_record_calls(mctx, prog, current_fn, node->as.unary.operand, impl_table);
            break;
        case NODE_CALL:
            for (int i = 0; i < node->as.call.arg_count; i++) {
                scan_and_record_calls(mctx, prog, current_fn, node->as.call.args[i], impl_table);
            }
            break;
        case NODE_METHOD_CALL:
            scan_and_record_calls(mctx, prog, current_fn, node->as.method_call.object, impl_table);
            for (int i = 0; i < node->as.method_call.arg_count; i++) {
                scan_and_record_calls(mctx, prog, current_fn, node->as.method_call.args[i], impl_table);
            }
            break;
        case NODE_MATCH:
            scan_and_record_calls(mctx, prog, current_fn, node->as.match_stmt.expr, impl_table);
            for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
                scan_and_record_calls(mctx, prog, current_fn, node->as.match_stmt.arms[i]->as.match_arm.body, impl_table);
            }
            break;
        default:
            break;
    }
}

void resolve_and_monomorphize_traits(AstNode *program, AstArena *arena) {
    if (!program || program->type != NODE_PROGRAM) return;

    ImplTable impl_table = {NULL, 0, 0};

    // 1. SECTION 6: IMPL-CONFORMANCE CHECKING
    for (int i = 0; i < program->as.program.impl_count; i++) {
        AstNode *imp = program->as.program.impls[i];
        const char *iname = imp->as.impl_decl.interface_name;
        const char *cname = imp->as.impl_decl.class_name;

        // Check if interface exists
        AstNode *iface_node = NULL;
        for (int if_idx = 0; if_idx < program->as.program.interface_count; if_idx++) {
            if (strcmp(program->as.program.interfaces[if_idx]->as.interface_decl.name, iname) == 0) {
                iface_node = program->as.program.interfaces[if_idx];
                break;
            }
        }
        if (!iface_node) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "unknown interface '%s'", iname);
            ErrorLocation loc = {imp->source_file ? imp->source_file : get_error_filename(), imp->line, imp->col};
            print_formatted_error(short_msg, loc, "unknown interface", NULL, NULL, NULL, NULL);
            exit(1);
        }

        // Check if target is a struct (REJECT: case 3)
        for (int s = 0; s < program->as.program.struct_count; s++) {
            if (strcmp(program->as.program.structs[s]->as.struct_decl.name, cname) == 0) {
                char short_msg[256];
                snprintf(short_msg, sizeof(short_msg), "interfaces can only be implemented by classes — '%s' is a struct, which cannot have methods", cname);
                ErrorLocation loc = {imp->source_file ? imp->source_file : get_error_filename(), imp->line, imp->col};
                print_formatted_error(short_msg, loc, "structs cannot implement interfaces", "structs are lightweight value types without methods in Cco", NULL, NULL, NULL);
                exit(1);
            }
        }

        // Check if class exists
        AstNode *cls_node = NULL;
        for (int c = 0; c < program->as.program.class_count; c++) {
            if (strcmp(program->as.program.classes[c]->as.class_decl.name, cname) == 0) {
                cls_node = program->as.program.classes[c];
                break;
            }
        }
        if (!cls_node) {
            char short_msg[256];
            snprintf(short_msg, sizeof(short_msg), "unknown class '%s'", cname);
            ErrorLocation loc = {imp->source_file ? imp->source_file : get_error_filename(), imp->line, imp->col};
            print_formatted_error(short_msg, loc, "unknown class", NULL, NULL, NULL, NULL);
            exit(1);
        }

        // Validate each required interface method
        for (int m = 0; m < iface_node->as.interface_decl.method_count; m++) {
            AstNode *req_m = iface_node->as.interface_decl.methods[m];
            const char *req_name = req_m->as.interface_method.name;

            AstNode *actual_m = NULL;
            for (int cm = 0; cm < cls_node->as.class_decl.method_count; cm++) {
                if (strcmp(cls_node->as.class_decl.methods[cm]->as.method.name, req_name) == 0) {
                    actual_m = cls_node->as.class_decl.methods[cm];
                    break;
                }
            }

            char *expected_sig = format_sig(req_name, req_m->as.interface_method.param_count,
                                            req_m->as.interface_method.param_names,
                                            req_m->as.interface_method.param_types,
                                            req_m->as.interface_method.param_class_names,
                                            req_m->as.interface_method.param_is_borrowed,
                                            req_m->as.interface_method.param_is_array,
                                            req_m->as.interface_method.return_type,
                                            req_m->as.interface_method.return_class_name,
                                            req_m->as.interface_method.return_is_array,
                                            req_m->as.interface_method.return_is_map,
                                            req_m->as.interface_method.return_key_type);

            if (!actual_m) {
                // Missing method (case 1)
                char short_msg[512];
                snprintf(short_msg, sizeof(short_msg), "'%s' does not implement '%s' — missing method '%s'", cname, iname, expected_sig + 3);
                ErrorLocation loc = {imp->source_file ? imp->source_file : get_error_filename(), imp->line, imp->col};
                print_formatted_error(short_msg, loc, "missing required interface method", NULL, NULL, NULL, NULL);
                exit(1);
            }

            // Check signature compatibility
            bool match = true;
            if (actual_m->as.method.param_count != req_m->as.interface_method.param_count) {
                match = false;
            } else {
                for (int p = 0; p < req_m->as.interface_method.param_count; p++) {
                    Type req_t = req_m->as.interface_method.param_types[p];
                    const char *req_c = req_m->as.interface_method.param_class_names ? req_m->as.interface_method.param_class_names[p] : NULL;
                    bool req_bor = req_m->as.interface_method.param_is_borrowed ? req_m->as.interface_method.param_is_borrowed[p] : false;
                    bool req_arr = req_m->as.interface_method.param_is_array ? req_m->as.interface_method.param_is_array[p] : false;

                    Type act_t = actual_m->as.method.param_types[p];
                    const char *act_c = actual_m->as.method.param_class_names ? actual_m->as.method.param_class_names[p] : NULL;
                    bool act_bor = actual_m->as.method.param_is_borrowed ? actual_m->as.method.param_is_borrowed[p] : false;
                    bool act_arr = actual_m->as.method.param_is_array ? actual_m->as.method.param_is_array[p] : false;

                    if (req_bor != act_bor || req_arr != act_arr) {
                        match = false;
                        break;
                    }
                    if (req_t == TY_CLASS && req_c && strcmp(req_c, "Self") == 0) {
                        if (act_t != TY_CLASS || !act_c || strcmp(act_c, cname) != 0) {
                            match = false;
                            break;
                        }
                    } else {
                        if (req_t != act_t) {
                            match = false;
                            break;
                        }
                        if (req_t == TY_CLASS) {
                            if (!req_c || !act_c || strcmp(req_c, act_c) != 0) {
                                match = false;
                                break;
                            }
                        }
                    }
                }
                if (match) {
                    Type req_ret = req_m->as.interface_method.return_type;
                    const char *req_rc = req_m->as.interface_method.return_class_name;
                    Type act_ret = actual_m->as.method.return_type;
                    const char *act_rc = actual_m->as.method.return_class_name;

                    if (req_ret == TY_CLASS && req_rc && strcmp(req_rc, "Self") == 0) {
                        if (act_ret != TY_CLASS || !act_rc || strcmp(act_rc, cname) != 0) {
                            match = false;
                        }
                    } else {
                        if (req_ret != act_ret) {
                            match = false;
                        } else if (req_ret == TY_CLASS) {
                            if (!req_rc || !act_rc || strcmp(req_rc, act_rc) != 0) {
                                match = false;
                            }
                        }
                    }
                    if (req_m->as.interface_method.return_is_array != actual_m->as.method.return_is_array ||
                        req_m->as.interface_method.return_is_map != actual_m->as.method.return_is_map) {
                        match = false;
                    }
                }
            }

            if (!match) {
                // Signature mismatch (case 2)
                char *actual_sig = format_sig(req_name, actual_m->as.method.param_count,
                                              actual_m->as.method.param_names,
                                              actual_m->as.method.param_types,
                                              actual_m->as.method.param_class_names,
                                              actual_m->as.method.param_is_borrowed,
                                              actual_m->as.method.param_is_array,
                                              actual_m->as.method.return_type,
                                              actual_m->as.method.return_class_name,
                                              actual_m->as.method.return_is_array,
                                              actual_m->as.method.return_is_map,
                                              actual_m->as.method.return_key_type);
                char short_msg[512];
                snprintf(short_msg, sizeof(short_msg), "'%s.%s' does not match '%s.%s' — expected `%s`, found `%s`", cname, req_name, iname, req_name, expected_sig, actual_sig);
                ErrorLocation loc = {imp->source_file ? imp->source_file : get_error_filename(), imp->line, imp->col};
                ErrorLocation note_loc = {actual_m->source_file ? actual_m->source_file : get_error_filename(), actual_m->line, actual_m->col};
                print_formatted_error(short_msg, loc, "interface signature mismatch", "actual method defined here:", &note_loc, "declared signature", NULL);
                exit(1);
            }

            free(expected_sig);
        }

        // Register valid implementation
        add_confirmed_impl(&impl_table, iname, cname);
    }

    // 2. SECTION 7: TYPE-CHECK TEMPLATE BODIES & MONOMORPHIZE
    for (int f = 0; f < program->as.program.count; f++) {
        AstNode *fn = program->as.program.functions[f];
        for (int p = 0; p < fn->as.function.param_count; p++) {
            if (fn->as.function.param_is_impl_trait && fn->as.function.param_is_impl_trait[p]) {
                const char *pname = fn->as.function.param_names[p];
                const char *iname = fn->as.function.param_impl_trait_names[p];

                // Find interface node
                AstNode *iface_node = NULL;
                for (int if_idx = 0; if_idx < program->as.program.interface_count; if_idx++) {
                    if (strcmp(program->as.program.interfaces[if_idx]->as.interface_decl.name, iname) == 0) {
                        iface_node = program->as.program.interfaces[if_idx];
                        break;
                    }
                }
                if (iface_node) {
                    validate_template_body(fn, fn->as.function.body, iface_node, pname, iname);
                }
            }
        }
    }

    // 3. SCAN ALL CALL SITES ACROSS PROGRAM FUNCTIONS AND CLASS METHODS
    MonomorphCtx mctx;
    memset(&mctx, 0, sizeof(mctx));

    for (int f = 0; f < program->as.program.count; f++) {
        AstNode *fn = program->as.program.functions[f];
        scan_and_record_calls(&mctx, program, fn, fn->as.function.body, &impl_table);
    }
    for (int c = 0; c < program->as.program.class_count; c++) {
        AstNode *cls = program->as.program.classes[c];
        for (int m = 0; m < cls->as.class_decl.method_count; m++) {
            AstNode *method = cls->as.class_decl.methods[m];
            scan_and_record_calls(&mctx, program, method, method->as.method.body, &impl_table);
        }
    }

    // 4. APPLY CALL REWRITES
    for (int r = 0; r < mctx.rewrite_count; r++) {
        mctx.rewrites[r].call_node->as.call.callee = arena_strdup(arena, mctx.rewrites[r].specialized_name);
    }

    // 5. GENERATE SPECIALIZED FUNCTIONS AND DROP UNUSED TEMPLATES
    AstNode **new_functions = NULL;
    int new_fn_count = 0;
    int new_fn_cap = 0;

    for (int f = 0; f < program->as.program.count; f++) {
        AstNode *fn = program->as.program.functions[f];
        bool is_template = false;
        for (int p = 0; p < fn->as.function.param_count; p++) {
            if (fn->as.function.param_is_impl_trait && fn->as.function.param_is_impl_trait[p]) {
                is_template = true;
                break;
            }
        }

        if (!is_template) {
            // Keep normal function
            if (new_fn_count >= new_fn_cap) {
                new_fn_cap = new_fn_cap == 0 ? 8 : new_fn_cap * 2;
                new_functions = realloc(new_functions, new_fn_cap * sizeof(AstNode *));
            }
            new_functions[new_fn_count++] = fn;
        } else {
            // Check if template was specialized
            for (int s = 0; s < mctx.spec_count; s++) {
                if (strcmp(mctx.specs[s].template_name, fn->as.function.name) == 0) {
                    AstNode *spec_fn = clone_node(fn, arena);
                    spec_fn->as.function.name = arena_strdup(arena, mctx.specs[s].specialized_name);

                    // Update impl Trait params to concrete class type
                    for (int p = 0; p < spec_fn->as.function.param_count; p++) {
                        if (spec_fn->as.function.param_is_impl_trait && spec_fn->as.function.param_is_impl_trait[p]) {
                            spec_fn->as.function.param_types[p] = TY_CLASS;
                            spec_fn->as.function.param_class_names[p] = arena_strdup(arena, mctx.specs[s].concrete_types[p]);
                            spec_fn->as.function.param_is_impl_trait[p] = false;
                            spec_fn->as.function.param_impl_trait_names[p] = NULL;
                        }
                    }

                    if (new_fn_count >= new_fn_cap) {
                        new_fn_cap = new_fn_cap == 0 ? 8 : new_fn_cap * 2;
                        new_functions = realloc(new_functions, new_fn_cap * sizeof(AstNode *));
                    }
                    new_functions[new_fn_count++] = spec_fn;
                }
            }
            // Original template function is dropped!
        }
    }

    // Replace functions array in program
    program->as.program.functions = (AstNode **)arena_alloc_array(arena, new_fn_count, sizeof(AstNode *));
    if (new_fn_count > 0 && new_functions) {
        memcpy(program->as.program.functions, new_functions, new_fn_count * sizeof(AstNode *));
    }
    program->as.program.count = new_fn_count;
    if (new_functions) free(new_functions);

    // Free local tables
    for (int i = 0; i < impl_table.count; i++) {
        free(impl_table.items[i].interface_name);
        free(impl_table.items[i].class_name);
    }
    if (impl_table.items) free(impl_table.items);

    for (int s = 0; s < mctx.spec_count; s++) {
        free(mctx.specs[s].template_name);
        for (int p = 0; p < mctx.specs[s].param_count; p++) {
            if (mctx.specs[s].concrete_types[p]) free(mctx.specs[s].concrete_types[p]);
        }
        free(mctx.specs[s].concrete_types);
        free(mctx.specs[s].specialized_name);
    }
    if (mctx.specs) free(mctx.specs);

    for (int r = 0; r < mctx.rewrite_count; r++) {
        free(mctx.rewrites[r].specialized_name);
    }
    if (mctx.rewrites) free(mctx.rewrites);
}
