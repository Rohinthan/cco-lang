#define _POSIX_C_SOURCE 200809L
#include "scope_analysis.h"
#include "class_decl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AstNode *block_node;
    char **owned_vars;
    bool *transferred;
    int count;
    int capacity;
    bool is_loop_scope;
} Scope;

typedef struct {
    Scope scopes[128];
    int depth;
    AstArena *arena;
    AstNode *program;
    AstNode *current_function;
} ScopeStack;

static void push_scope(ScopeStack *stack, AstNode *block_node, bool is_loop) {
    if (stack->depth >= 128) {
        fprintf(stderr, "Scope Analysis Error: scope stack overflow\n");
        exit(1);
    }
    Scope *s = &stack->scopes[stack->depth++];
    s->block_node = block_node;
    s->owned_vars = NULL;
    s->transferred = NULL;
    s->count = 0;
    s->capacity = 0;
    s->is_loop_scope = is_loop;
}

static void pop_scope(ScopeStack *stack) {
    if (stack->depth > 0) {
        stack->depth--;
    }
}

static Scope *current_scope(ScopeStack *stack) {
    if (stack->depth == 0) return NULL;
    return &stack->scopes[stack->depth - 1];
}

static void scope_add_owned(ScopeStack *stack, Scope *s, const char *name) {
    if (s->count >= s->capacity) {
        s->capacity = s->capacity == 0 ? 4 : s->capacity * 2;
        char **new_vars = (char **)arena_alloc_array(stack->arena, s->capacity, sizeof(char *));
        bool *new_trans = (bool *)arena_alloc_array(stack->arena, s->capacity, sizeof(bool));
        if (s->owned_vars) {
            memcpy(new_vars, s->owned_vars, s->count * sizeof(char *));
            memcpy(new_trans, s->transferred, s->count * sizeof(bool));
        }
        s->owned_vars = new_vars;
        s->transferred = new_trans;
    }
    s->owned_vars[s->count] = arena_strdup(stack->arena, name);
    s->transferred[s->count] = false;
    s->count++;
}

static int find_owned_in_stack(ScopeStack *stack, const char *name, int *out_scope_idx) {
    for (int i = stack->depth - 1; i >= 0; i--) {
        Scope *s = &stack->scopes[i];
        for (int j = 0; j < s->count; j++) {
            if (strcmp(s->owned_vars[j], name) == 0) {
                if (out_scope_idx) *out_scope_idx = i;
                return j;
            }
        }
    }
    return -1;
}

static void add_free_to_node(ScopeStack *stack, AstNode *node, const char *var_name) {
    for (int i = 0; i < node->frees_count; i++) {
        if (strcmp(node->frees_to_emit[i], var_name) == 0) return;
    }

    int cap = node->frees_count + 1;
    char **new_frees = (char **)arena_alloc_array(stack->arena, cap, sizeof(char *));
    if (node->frees_to_emit) {
        memcpy(new_frees, node->frees_to_emit, node->frees_count * sizeof(char *));
    }
    new_frees[node->frees_count] = arena_strdup(stack->arena, var_name);
    node->frees_to_emit = new_frees;
    node->frees_count++;
}

static AstNode *find_function(AstNode *program, const char *name) {
    if (!program || program->type != NODE_PROGRAM) return NULL;
    for (int i = 0; i < program->as.program.count; i++) {
        AstNode *fn = program->as.program.functions[i];
        if (strcmp(fn->as.function.name, name) == 0) return fn;
    }
    return NULL;
}

static void analyze_node(ScopeStack *stack, AstNode *node);

static void analyze_block(ScopeStack *stack, AstNode *block_node, bool is_loop) {
    push_scope(stack, block_node, is_loop);
    Scope *s = current_scope(stack);

    for (int i = 0; i < block_node->as.block.count; i++) {
        AstNode *stmt = block_node->as.block.stmts[i];

        if (stmt->type == NODE_LET) {
            analyze_node(stack, stmt->as.let.value);
            bool is_alloc = (stmt->as.let.value->type == NODE_ALLOC);
            if (!is_alloc && stmt->as.let.value->type == NODE_CALL) {
                const char *callee = stmt->as.let.value->as.call.callee;
                AstNode *fn = find_function(stack->program, callee);
                if (fn && fn->as.function.returns_heap_pointer) {
                    is_alloc = true;
                }
            }

            if (is_alloc) {
                stmt->is_heap_owner = true;
                scope_add_owned(stack, s, stmt->as.let.name);
            }
        } else if (stmt->type == NODE_ASSIGN) {
            analyze_node(stack, stmt->as.assign.value);

            int scope_idx = -1;
            int owned_idx = find_owned_in_stack(stack, stmt->as.assign.name, &scope_idx);
            if (owned_idx != -1 && stmt->as.assign.value->type == NODE_ALLOC) {
                stmt->free_old_on_reassign = true;
                add_free_to_node(stack, stmt, stmt->as.assign.name);
            }

            if (stmt->as.assign.value->type == NODE_IDENT) {
                const char *rhs_name = stmt->as.assign.value->as.ident.name;
                int rhs_scope_idx = -1;
                int rhs_owned_idx = find_owned_in_stack(stack, rhs_name, &rhs_scope_idx);
                if (rhs_owned_idx != -1) {
                    stack->scopes[rhs_scope_idx].transferred[rhs_owned_idx] = true;
                    int lhs_scope_idx = -1;
                    int lhs_owned_idx = find_owned_in_stack(stack, stmt->as.assign.name, &lhs_scope_idx);
                    if (lhs_scope_idx != -1 && lhs_scope_idx < rhs_scope_idx) {
                        stack->scopes[lhs_scope_idx].transferred[lhs_owned_idx] = false;
                    }
                }
            }
        } else {
            analyze_node(stack, stmt);
        }
    }

    for (int i = 0; i < s->count; i++) {
        if (!s->transferred[i]) {
            add_free_to_node(stack, block_node, s->owned_vars[i]);
        }
    }

    if (s->count > 0) {
        block_node->as.block.owned_vars = (char **)arena_alloc_array(stack->arena, s->count, sizeof(char *));
        for (int i = 0; i < s->count; i++) {
            block_node->as.block.owned_vars[i] = arena_strdup(stack->arena, s->owned_vars[i]);
        }
        block_node->as.block.owned_count = s->count;
    }

    pop_scope(stack);
}

static void analyze_node(ScopeStack *stack, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.count; i++) {
                analyze_node(stack, node->as.program.functions[i]);
            }
            break;

        case NODE_FUNCTION: {
            AstNode *prev_fn = stack->current_function;
            stack->current_function = node;
            analyze_block(stack, node->as.function.body, false);
            stack->current_function = prev_fn;
            break;
        }

        case NODE_BLOCK:
            analyze_block(stack, node, false);
            break;

        case NODE_IF:
            analyze_node(stack, node->as.if_stmt.cond);
            if (node->as.if_stmt.then_b) {
                if (node->as.if_stmt.then_b->type == NODE_BLOCK) {
                    analyze_block(stack, node->as.if_stmt.then_b, false);
                } else {
                    analyze_node(stack, node->as.if_stmt.then_b);
                }
            }
            if (node->as.if_stmt.else_b) {
                if (node->as.if_stmt.else_b->type == NODE_BLOCK) {
                    analyze_block(stack, node->as.if_stmt.else_b, false);
                } else {
                    analyze_node(stack, node->as.if_stmt.else_b);
                }
            }
            break;

        case NODE_WHILE:
            analyze_node(stack, node->as.while_stmt.cond);
            analyze_block(stack, node->as.while_stmt.body, true);
            break;

        case NODE_FOR:
            push_scope(stack, node, true);
            if (node->as.for_stmt.init) analyze_node(stack, node->as.for_stmt.init);
            if (node->as.for_stmt.cond) analyze_node(stack, node->as.for_stmt.cond);
            if (node->as.for_stmt.step) analyze_node(stack, node->as.for_stmt.step);
            if (node->as.for_stmt.body) {
                if (node->as.for_stmt.body->type == NODE_BLOCK) {
                    analyze_block(stack, node->as.for_stmt.body, false);
                } else {
                    analyze_node(stack, node->as.for_stmt.body);
                }
            }
            Scope *fs = current_scope(stack);
            for (int i = 0; i < fs->count; i++) {
                if (!fs->transferred[i]) {
                    add_free_to_node(stack, node, fs->owned_vars[i]);
                }
            }
            pop_scope(stack);
            break;

        case NODE_RETURN: {
            if (node->as.return_stmt.value) {
                analyze_node(stack, node->as.return_stmt.value);
            }

            const char *ret_var = NULL;
            if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_IDENT) {
                ret_var = node->as.return_stmt.value->as.ident.name;
                int r_scope_idx = -1;
                int r_owned_idx = find_owned_in_stack(stack, ret_var, &r_scope_idx);
                if (r_owned_idx != -1) {
                    stack->scopes[r_scope_idx].transferred[r_owned_idx] = true;
                    if (stack->current_function) {
                        stack->current_function->as.function.returns_heap_pointer = true;
                    }
                }
            } else if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_ALLOC) {
                if (stack->current_function) {
                    stack->current_function->as.function.returns_heap_pointer = true;
                }
            }

            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                Scope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!s->transferred[i]) {
                        if (!ret_var || strcmp(s->owned_vars[i], ret_var) != 0) {
                            add_free_to_node(stack, node, s->owned_vars[i]);
                        }
                    }
                }
            }
            break;
        }

        case NODE_BREAK:
        case NODE_CONTINUE: {
            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                Scope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!s->transferred[i]) {
                        add_free_to_node(stack, node, s->owned_vars[i]);
                    }
                }
                if (s->is_loop_scope) break;
            }
            break;
        }

        case NODE_EXPR_STMT:
            analyze_node(stack, node->as.expr_stmt.expr);
            break;
        case NODE_PRINT:
            analyze_node(stack, node->as.print_stmt.value);
            break;
        case NODE_INDEX_ASSIGN:
            analyze_node(stack, node->as.index_assign.index);
            analyze_node(stack, node->as.index_assign.value);
            break;
        case NODE_BINARY:
            analyze_node(stack, node->as.binary.left);
            analyze_node(stack, node->as.binary.right);
            break;
        case NODE_UNARY:
            analyze_node(stack, node->as.unary.operand);
            break;
        case NODE_CALL:
            for (int i = 0; i < node->as.call.arg_count; i++) {
                analyze_node(stack, node->as.call.args[i]);
            }
            break;
        case NODE_INDEX:
            analyze_node(stack, node->as.index.index);
            break;
        default:
            break;
    }
}

/* ========================================================================= */
/*  REFCOUNTING SCOPE ANALYSIS PASS (CMM v2)                                */
/* ========================================================================= */

typedef struct RefVar {
    char *name;
    char *class_name;
    bool transferred;
} RefVar;

typedef struct RefScope {
    AstNode *block_node;
    RefVar *vars;
    int count;
    int capacity;
    bool is_loop_scope;
} RefScope;

typedef struct RefScopeStack {
    RefScope scopes[128];
    int depth;
    AstArena *arena;
    AstNode *program;
    ClassTable *ct;
    AstNode *current_function;
    char *current_class_name;
} RefScopeStack;

static void push_ref_scope(RefScopeStack *stack, AstNode *block_node, bool is_loop) {
    if (stack->depth >= 128) {
        fprintf(stderr, "Refcount Scope Analysis Error: scope stack overflow\n");
        exit(1);
    }
    RefScope *s = &stack->scopes[stack->depth++];
    s->block_node = block_node;
    s->vars = NULL;
    s->count = 0;
    s->capacity = 0;
    s->is_loop_scope = is_loop;
}

static void pop_ref_scope(RefScopeStack *stack) {
    if (stack->depth > 0) {
        stack->depth--;
    }
}

static RefScope *current_ref_scope(RefScopeStack *stack) {
    if (stack->depth == 0) return NULL;
    return &stack->scopes[stack->depth - 1];
}

static void ref_scope_add_var(RefScopeStack *stack, RefScope *s, const char *name, const char *class_name, bool transferred) {
    if (!name || !class_name) return;
    if (s->count >= s->capacity) {
        s->capacity = s->capacity == 0 ? 4 : s->capacity * 2;
        RefVar *new_vars = (RefVar *)arena_alloc_array(stack->arena, s->capacity, sizeof(RefVar));
        if (s->vars) {
            memcpy(new_vars, s->vars, s->count * sizeof(RefVar));
        }
        s->vars = new_vars;
    }
    s->vars[s->count].name = arena_strdup(stack->arena, name);
    s->vars[s->count].class_name = arena_strdup(stack->arena, class_name);
    s->vars[s->count].transferred = transferred;
    s->count++;
}

static RefVar *find_ref_var(RefScopeStack *stack, const char *name, int *out_scope_idx, int *out_var_idx) {
    if (!name) return NULL;
    for (int i = stack->depth - 1; i >= 0; i--) {
        RefScope *s = &stack->scopes[i];
        for (int j = 0; j < s->count; j++) {
            if (strcmp(s->vars[j].name, name) == 0) {
                if (out_scope_idx) *out_scope_idx = i;
                if (out_var_idx) *out_var_idx = j;
                return &s->vars[j];
            }
        }
    }
    return NULL;
}

static void add_release_to_node(RefScopeStack *stack, AstNode *node, const char *var_name, const char *class_name) {
    if (!node || !var_name || !class_name) return;
    for (int i = 0; i < node->releases_count; i++) {
        if (strcmp(node->releases_to_emit[i].var_name, var_name) == 0) return;
    }

    int cap = node->releases_count + 1;
    RefRelease *new_rels = (RefRelease *)arena_alloc_array(stack->arena, cap, sizeof(RefRelease));
    if (node->releases_to_emit) {
        memcpy(new_rels, node->releases_to_emit, node->releases_count * sizeof(RefRelease));
    }
    new_rels[node->releases_count].var_name = arena_strdup(stack->arena, var_name);
    new_rels[node->releases_count].class_name = arena_strdup(stack->arena, class_name);
    node->releases_to_emit = new_rels;
    node->releases_count++;
}

static char *get_expr_class_name(RefScopeStack *stack, AstNode *expr) {
    if (!expr) return NULL;

    switch (expr->type) {
        case NODE_IDENT: {
            RefVar *v = find_ref_var(stack, expr->as.ident.name, NULL, NULL);
            if (v) return v->class_name;
            if (stack->current_class_name && strcmp(expr->as.ident.name, "self") == 0) {
                return stack->current_class_name;
            }
            return NULL;
        }

        case NODE_NEW:
            return expr->as.new_expr.class_name;

        case NODE_MEMBER: {
            char *obj_cls = get_expr_class_name(stack, expr->as.member.object);
            if (obj_cls) {
                ClassDef *cd = find_class(stack->ct, obj_cls);
                FieldInfo *fi = find_field(cd, expr->as.member.member_name);
                if (fi && fi->type == TY_CLASS) {
                    expr->as.member.field_class_name = arena_strdup(stack->arena, fi->class_name);
                    return fi->class_name;
                }
            }
            return NULL;
        }

        case NODE_CALL: {
            if (stack->program && stack->program->type == NODE_PROGRAM) {
                for (int i = 0; i < stack->program->as.program.count; i++) {
                    AstNode *fn = stack->program->as.program.functions[i];
                    if (strcmp(fn->as.function.name, expr->as.call.callee) == 0) {
                        if (fn->as.function.return_type == TY_CLASS) {
                            return fn->as.function.return_class_name;
                        }
                    }
                }
            }
            return NULL;
        }

        case NODE_METHOD_CALL: {
            char *obj_cls = get_expr_class_name(stack, expr->as.method_call.object);
            if (obj_cls) {
                expr->as.method_call.target_class_name = arena_strdup(stack->arena, obj_cls);
                ClassDef *cd = find_class(stack->ct, obj_cls);
                MethodInfo *mi = find_method(cd, expr->as.method_call.method_name);
                if (mi && mi->method_node) {
                    AstNode *mn = mi->method_node;
                    if (mn->as.method.return_type == TY_CLASS) {
                        return mn->as.method.return_class_name;
                    }
                }
            }
            return NULL;
        }

        case NODE_ALLOC:
            if (expr->as.alloc.elem_type == TY_CLASS) {
                return expr->as.alloc.class_name;
            }
            return NULL;

        default:
            return NULL;
    }
}

static void analyze_ref_node(RefScopeStack *stack, AstNode *node);

static void analyze_ref_block(RefScopeStack *stack, AstNode *block_node, bool is_loop, bool is_fn_body) {
    push_ref_scope(stack, block_node, is_loop);
    RefScope *s = current_ref_scope(stack);

    if (is_fn_body && stack->current_function) {
        if (stack->current_function->type == NODE_METHOD) {
            AstNode *m = stack->current_function;
            for (int p = 0; p < m->as.method.param_count; p++) {
                if (m->as.method.param_types[p] == TY_CLASS && m->as.method.param_class_names[p]) {
                    ref_scope_add_var(stack, s, m->as.method.param_names[p], m->as.method.param_class_names[p], true);
                }
            }
        } else if (stack->current_function->type == NODE_FUNCTION) {
            AstNode *f = stack->current_function;
            for (int p = 0; p < f->as.function.param_count; p++) {
                if (f->as.function.param_types[p] == TY_CLASS && f->as.function.param_class_names[p]) {
                    ref_scope_add_var(stack, s, f->as.function.param_names[p], f->as.function.param_class_names[p], true);
                }
            }
        }
    }

    for (int i = 0; i < block_node->as.block.count; i++) {
        AstNode *stmt = block_node->as.block.stmts[i];

        if (stmt->type == NODE_LET) {
            analyze_ref_node(stack, stmt->as.let.value);
            char *cls_name = stmt->as.let.class_name;
            if (!cls_name && stmt->as.let.var_type == TY_CLASS) {
                cls_name = stmt->as.let.class_name;
            }
            if (!cls_name) {
                cls_name = get_expr_class_name(stack, stmt->as.let.value);
            }

            if (cls_name) {
                ref_scope_add_var(stack, s, stmt->as.let.name, cls_name, false);
                stmt->as.let.class_name = arena_strdup(stack->arena, cls_name);
                stmt->as.let.var_type = TY_CLASS;

                AstNode *val = stmt->as.let.value;
                if (val->type == NODE_NEW || val->type == NODE_CALL || val->type == NODE_METHOD_CALL) {
                    stmt->as.let.retain_rhs = false;
                } else {
                    stmt->as.let.retain_rhs = (get_expr_class_name(stack, val) != NULL);
                }
            }
        } else if (stmt->type == NODE_ASSIGN) {
            analyze_ref_node(stack, stmt->as.assign.value);
            RefVar *v = find_ref_var(stack, stmt->as.assign.name, NULL, NULL);
            if (v) {
                stmt->as.assign.class_name = arena_strdup(stack->arena, v->class_name);
                stmt->as.assign.release_old = true;

                AstNode *val = stmt->as.assign.value;
                if (val->type == NODE_NEW || val->type == NODE_CALL || val->type == NODE_METHOD_CALL) {
                    stmt->as.assign.retain_rhs = false;
                } else {
                    stmt->as.assign.retain_rhs = (get_expr_class_name(stack, val) != NULL);
                }
            }
        } else if (stmt->type == NODE_MEMBER_ASSIGN) {
            analyze_ref_node(stack, stmt->as.member_assign.object);
            analyze_ref_node(stack, stmt->as.member_assign.value);

            char *obj_cls = get_expr_class_name(stack, stmt->as.member_assign.object);
            if (obj_cls) {
                ClassDef *cd = find_class(stack->ct, obj_cls);
                FieldInfo *fi = find_field(cd, stmt->as.member_assign.member_name);
                if (fi && fi->type == TY_CLASS) {
                    stmt->as.member_assign.field_class_name = arena_strdup(stack->arena, fi->class_name);
                    stmt->as.member_assign.release_old = true;

                    AstNode *val = stmt->as.member_assign.value;
                    if (val->type == NODE_NEW || val->type == NODE_CALL || val->type == NODE_METHOD_CALL) {
                        stmt->as.member_assign.retain_rhs = false;
                    } else {
                        stmt->as.member_assign.retain_rhs = (get_expr_class_name(stack, val) != NULL);
                    }
                }
            }
        } else {
            analyze_ref_node(stack, stmt);
        }
    }

    for (int i = 0; i < s->count; i++) {
        if (!s->vars[i].transferred) {
            add_release_to_node(stack, block_node, s->vars[i].name, s->vars[i].class_name);
        }
    }

    pop_ref_scope(stack);
}

static void analyze_ref_node(RefScopeStack *stack, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            for (int c = 0; c < node->as.program.class_count; c++) {
                AstNode *cls = node->as.program.classes[c];
                stack->current_class_name = cls->as.class_decl.name;
                for (int m = 0; m < cls->as.class_decl.method_count; m++) {
                    AstNode *m_node = cls->as.class_decl.methods[m];
                    stack->current_function = m_node;
                    analyze_ref_block(stack, m_node->as.method.body, false, true);
                }
            }
            stack->current_class_name = NULL;

            for (int f = 0; f < node->as.program.count; f++) {
                AstNode *fn = node->as.program.functions[f];
                stack->current_function = fn;
                analyze_ref_block(stack, fn->as.function.body, false, true);
            }
            stack->current_function = NULL;
            break;
        }

        case NODE_BLOCK:
            analyze_ref_block(stack, node, false, false);
            break;

        case NODE_IF: {
            analyze_ref_node(stack, node->as.if_stmt.cond);

            int var_count = 0;
            for (int d = 0; d < stack->depth; d++) {
                var_count += stack->scopes[d].count;
            }
            bool *saved_trans = NULL;
            if (var_count > 0) {
                saved_trans = (bool *)malloc(var_count * sizeof(bool));
                int idx = 0;
                for (int d = 0; d < stack->depth; d++) {
                    for (int v = 0; v < stack->scopes[d].count; v++) {
                        saved_trans[idx++] = stack->scopes[d].vars[v].transferred;
                    }
                }
            }

            if (node->as.if_stmt.then_b) {
                if (node->as.if_stmt.then_b->type == NODE_BLOCK) {
                    analyze_ref_block(stack, node->as.if_stmt.then_b, false, false);
                } else {
                    analyze_ref_node(stack, node->as.if_stmt.then_b);
                }
            }

            if (saved_trans) {
                int idx = 0;
                for (int d = 0; d < stack->depth; d++) {
                    for (int v = 0; v < stack->scopes[d].count; v++) {
                        stack->scopes[d].vars[v].transferred = saved_trans[idx++];
                    }
                }
            }

            if (node->as.if_stmt.else_b) {
                if (node->as.if_stmt.else_b->type == NODE_BLOCK) {
                    analyze_ref_block(stack, node->as.if_stmt.else_b, false, false);
                } else {
                    analyze_ref_node(stack, node->as.if_stmt.else_b);
                }
            }

            if (saved_trans) {
                int idx = 0;
                for (int d = 0; d < stack->depth; d++) {
                    for (int v = 0; v < stack->scopes[d].count; v++) {
                        stack->scopes[d].vars[v].transferred = saved_trans[idx++];
                    }
                }
                free(saved_trans);
            }
            break;
        }

        case NODE_WHILE:
            analyze_ref_node(stack, node->as.while_stmt.cond);
            if (node->as.while_stmt.body) {
                if (node->as.while_stmt.body->type == NODE_BLOCK) {
                    analyze_ref_block(stack, node->as.while_stmt.body, true, false);
                } else {
                    analyze_ref_node(stack, node->as.while_stmt.body);
                }
            }
            break;

        case NODE_FOR: {
            push_ref_scope(stack, node, true);
            if (node->as.for_stmt.init) analyze_ref_node(stack, node->as.for_stmt.init);
            if (node->as.for_stmt.cond) analyze_ref_node(stack, node->as.for_stmt.cond);
            if (node->as.for_stmt.step) analyze_ref_node(stack, node->as.for_stmt.step);
            if (node->as.for_stmt.body) {
                if (node->as.for_stmt.body->type == NODE_BLOCK) {
                    analyze_ref_block(stack, node->as.for_stmt.body, false, false);
                } else {
                    analyze_ref_node(stack, node->as.for_stmt.body);
                }
            }
            RefScope *fs = current_ref_scope(stack);
            for (int i = 0; i < fs->count; i++) {
                if (!fs->vars[i].transferred) {
                    add_release_to_node(stack, node, fs->vars[i].name, fs->vars[i].class_name);
                }
            }
            pop_ref_scope(stack);
            break;
        }

        case NODE_RETURN: {
            if (node->as.return_stmt.value) {
                analyze_ref_node(stack, node->as.return_stmt.value);
            }

            if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_IDENT) {
                const char *ret_var = node->as.return_stmt.value->as.ident.name;
                int s_idx = -1, v_idx = -1;
                RefVar *v = find_ref_var(stack, ret_var, &s_idx, &v_idx);
                if (v && s_idx != -1 && v_idx != -1) {
                    stack->scopes[s_idx].vars[v_idx].transferred = true;
                }
            }

            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                RefScope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!s->vars[i].transferred) {
                        add_release_to_node(stack, node, s->vars[i].name, s->vars[i].class_name);
                    }
                }
            }
            break;
        }

        case NODE_BREAK:
        case NODE_CONTINUE: {
            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                RefScope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!s->vars[i].transferred) {
                        add_release_to_node(stack, node, s->vars[i].name, s->vars[i].class_name);
                    }
                }
                if (s->is_loop_scope) break;
            }
            break;
        }

        case NODE_EXPR_STMT:
            analyze_ref_node(stack, node->as.expr_stmt.expr);
            break;
        case NODE_PRINT:
            analyze_ref_node(stack, node->as.print_stmt.value);
            break;
        case NODE_MEMBER:
            get_expr_class_name(stack, node);
            break;
        case NODE_METHOD_CALL:
            for (int i = 0; i < node->as.method_call.arg_count; i++) {
                analyze_ref_node(stack, node->as.method_call.args[i]);
            }
            get_expr_class_name(stack, node);
            break;
        default:
            break;
    }
}

static void analyze_refcounts(AstNode *program, AstArena *arena) {
    RefScopeStack stack;
    memset(&stack, 0, sizeof(stack));
    stack.arena = arena;
    stack.program = program;
    stack.ct = build_class_table(program, arena);
    analyze_ref_node(&stack, program);
}

void analyze_scopes(AstNode *program, AstArena *arena) {
    ScopeStack stack;
    stack.depth = 0;
    stack.arena = arena;
    stack.program = program;
    stack.current_function = NULL;

    // Pass 1: analyze functions to compute returns_heap_pointer flags
    analyze_node(&stack, program);

    // Pass 2: analyze full AST with returns_heap_pointer signatures available
    stack.depth = 0;
    stack.current_function = NULL;
    analyze_node(&stack, program);

    // Pass 3: analyze full AST for refcounting (classes)
    analyze_refcounts(program, arena);
}
