#define _POSIX_C_SOURCE 200809L
#include "scope_analysis.h"
#include "errors.h"
#include "class_decl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AstNode *block_node;
    char **owned_vars;
    bool *transferred;
    bool *is_array;
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
    s->is_array = NULL;
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

static void scope_add_owned(ScopeStack *stack, Scope *s, const char *name, bool is_array) {
    if (s->count >= s->capacity) {
        s->capacity = s->capacity == 0 ? 4 : s->capacity * 2;
        char **new_vars = (char **)arena_alloc_array(stack->arena, s->capacity, sizeof(char *));
        bool *new_trans = (bool *)arena_alloc_array(stack->arena, s->capacity, sizeof(bool));
        bool *new_arr = (bool *)arena_alloc_array(stack->arena, s->capacity, sizeof(bool));
        if (s->owned_vars) {
            memcpy(new_vars, s->owned_vars, s->count * sizeof(char *));
            memcpy(new_trans, s->transferred, s->count * sizeof(bool));
            memcpy(new_arr, s->is_array, s->count * sizeof(bool));
        }
        s->owned_vars = new_vars;
        s->transferred = new_trans;
        s->is_array = new_arr;
    }
    s->owned_vars[s->count] = arena_strdup(stack->arena, name);
    s->transferred[s->count] = false;
    s->is_array[s->count] = is_array;
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

static void add_free_to_node(ScopeStack *stack, AstNode *node, const char *var_name, bool is_array) {
    for (int i = 0; i < node->frees_count; i++) {
        if (strcmp(node->frees_to_emit[i].var_name, var_name) == 0) return;
    }

    int cap = node->frees_count + 1;
    RawFree *new_frees = (RawFree *)arena_alloc_array(stack->arena, cap, sizeof(RawFree));
    if (node->frees_to_emit) {
        memcpy(new_frees, node->frees_to_emit, node->frees_count * sizeof(RawFree));
    }
    new_frees[node->frees_count].var_name = arena_strdup(stack->arena, var_name);
    new_frees[node->frees_count].is_array = is_array;
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

static bool is_stdlib_heap_fn(const char *name) {
    if (!name) return false;
    return strcmp(name, "concat") == 0 ||
           strcmp(name, "substring") == 0 ||
           strcmp(name, "read_file") == 0 ||
           strcmp(name, "read_line") == 0 ||
           strcmp(name, "program_name") == 0 ||
           strcmp(name, "args") == 0 ||
           strcmp(name, "keys") == 0;
}


static void analyze_block(ScopeStack *stack, AstNode *block_node, bool is_loop) {
    push_scope(stack, block_node, is_loop);
    Scope *s = current_scope(stack);

    for (int i = 0; i < block_node->as.block.count; i++) {
        AstNode *stmt = block_node->as.block.stmts[i];

        if (stmt->type == NODE_LET) {
            analyze_node(stack, stmt->as.let.value);
            bool is_alloc = (stmt->as.let.value->type == NODE_ALLOC && !stmt->as.let.value->as.alloc.is_map && stmt->as.let.value->as.alloc.elem_type != TY_CLASS) || (stmt->as.let.var_type == TY_STRING && !stmt->as.let.is_map);
            if (!is_alloc && stmt->as.let.value->type == NODE_CALL) {
                const char *callee = stmt->as.let.value->as.call.callee;
                if (is_stdlib_heap_fn(callee)) {
                    is_alloc = true;
                } else {
                    AstNode *fn = find_function(stack->program, callee);
                    if (fn && fn->as.function.returns_heap_pointer) {
                        is_alloc = true;
                    }
                }
            }

            if (is_alloc) {
                if (!(stmt->as.let.var_type == TY_STRING && stmt->as.let.is_array)) {
                    stmt->is_heap_owner = true;
                    bool is_str = (stmt->as.let.var_type == TY_STRING && !stmt->as.let.is_array);
                    bool is_arr = !is_str;
                    scope_add_owned(stack, s, stmt->as.let.name, is_arr);
                }
            }
        } else if (stmt->type == NODE_ASSIGN) {
            analyze_node(stack, stmt->as.assign.value);

            int scope_idx = -1;
            int owned_idx = find_owned_in_stack(stack, stmt->as.assign.name, &scope_idx);
            bool val_is_heap = ((stmt->as.assign.value->type == NODE_ALLOC && !stmt->as.assign.value->as.alloc.is_map && stmt->as.assign.value->as.alloc.elem_type != TY_CLASS) || (stmt->as.assign.value->type == NODE_LITERAL && stmt->as.assign.value->as.literal.lit_type == TY_STRING) || (stmt->as.assign.value->type == NODE_CALL && is_stdlib_heap_fn(stmt->as.assign.value->as.call.callee)));
            if (owned_idx != -1 && val_is_heap) {
                stmt->free_old_on_reassign = true;
                bool val_is_str = (stmt->as.assign.value->type == NODE_LITERAL && stmt->as.assign.value->as.literal.lit_type == TY_STRING) ||
                                  (stmt->as.assign.value->type == NODE_CALL && (strcmp(stmt->as.assign.value->as.call.callee, "concat") == 0 || strcmp(stmt->as.assign.value->as.call.callee, "substring") == 0 || strcmp(stmt->as.assign.value->as.call.callee, "read_file") == 0 || strcmp(stmt->as.assign.value->as.call.callee, "read_line") == 0 || strcmp(stmt->as.assign.value->as.call.callee, "program_name") == 0));
                bool val_is_arr = !val_is_str;
                add_free_to_node(stack, stmt, stmt->as.assign.name, val_is_arr);
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
            add_free_to_node(stack, block_node, s->owned_vars[i], s->is_array[i]);
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

        case NODE_MATCH:
            analyze_node(stack, node->as.match_stmt.expr);
            for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
                AstNode *arm = node->as.match_stmt.arms[a];
                if (arm->as.match_arm.body) {
                    if (arm->as.match_arm.body->type == NODE_BLOCK) {
                        analyze_block(stack, arm->as.match_arm.body, false);
                    } else {
                        analyze_node(stack, arm->as.match_arm.body);
                    }
                }
            }
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
                    add_free_to_node(stack, node, fs->owned_vars[i], fs->is_array[i]);
                }
            }
            pop_scope(stack);
            break;

        case NODE_FOR_EACH:
            analyze_node(stack, node->as.for_each.collection_expr);
            if (node->as.for_each.body) {
                if (node->as.for_each.body->type == NODE_BLOCK) {
                    analyze_block(stack, node->as.for_each.body, true);
                } else {
                    analyze_node(stack, node->as.for_each.body);
                }
            }
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
            } else if (node->as.return_stmt.value && ((node->as.return_stmt.value->type == NODE_ALLOC && !node->as.return_stmt.value->as.alloc.is_map) || (node->as.return_stmt.value->type == NODE_CALL && is_stdlib_heap_fn(node->as.return_stmt.value->as.call.callee)))) {
                if (stack->current_function) {
                    stack->current_function->as.function.returns_heap_pointer = true;
                }
            }

            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                Scope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!s->transferred[i]) {
                        if (!ret_var || strcmp(s->owned_vars[i], ret_var) != 0) {
                            add_free_to_node(stack, node, s->owned_vars[i], s->is_array[i]);
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
                        add_free_to_node(stack, node, s->owned_vars[i], s->is_array[i]);
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
        case NODE_NEW: {
            for (int k = 0; k < node->as.new_expr.field_count; k++) {
                AstNode *val = node->as.new_expr.field_values[k];
                analyze_node(stack, val);
                bool val_is_heap = (val->type == NODE_ALLOC && !val->as.alloc.is_map);
                if (!val_is_heap && val->type == NODE_IDENT) {
                    int s_idx = -1;
                    int o_idx = find_owned_in_stack(stack, val->as.ident.name, &s_idx);
                    if (o_idx != -1) {
                        val_is_heap = true;
                        stack->scopes[s_idx].transferred[o_idx] = true;
                    }
                }
                if (val_is_heap && stack->program && stack->program->type == NODE_PROGRAM) {
                    for (int c = 0; c < stack->program->as.program.class_count; c++) {
                        AstNode *cls_ast = stack->program->as.program.classes[c];
                        if (strcmp(cls_ast->as.class_decl.name, node->as.new_expr.class_name) == 0) {
                            for (int f = 0; f < cls_ast->as.class_decl.field_count; f++) {
                                if (strcmp(cls_ast->as.class_decl.fields[f]->as.field.name, node->as.new_expr.field_names[k]) == 0) {
                                    cls_ast->as.class_decl.fields[f]->is_heap_owner = true;
                                }
                            }
                        }
                    }
                }
            }
            break;
        }
        case NODE_INDEX:
            analyze_node(stack, node->as.index.index);
            break;
        default:
            break;
    }
}

/* ========================================================================= */
/*  SINGLE OWNERSHIP & MOVE ANALYSIS PASS (Cco v3)                           */
/* ========================================================================= */

typedef struct OwnVar {
    char *name;
    char *class_name;
    int decl_line;
    int decl_col;
    bool is_param;
    bool is_borrowed_param;
    bool transferred;
    bool is_array;
    bool is_map;
    Type key_type;
    int const_size;
} OwnVar;

typedef struct MovedVar {
    char *name;
    int move_line;
    int move_col;
    char *dest_name;
    bool is_conditional;
} MovedVar;

typedef struct OwnScope {
    AstNode *block_node;
    OwnVar *vars;
    int count;
    int capacity;
    bool is_loop_scope;
} OwnScope;

typedef struct OwnScopeStack {
    OwnScope scopes[128];
    int depth;

    MovedVar moved[256];
    int moved_count;

    AstArena *arena;
    AstNode *program;
    ClassTable *ct;
    AstNode *current_function;
    char *current_class_name;
} OwnScopeStack;


static void push_own_scope(OwnScopeStack *stack, AstNode *block_node, bool is_loop) {
    if (stack->depth >= 128) {
        fprintf(stderr, "Ownership Scope Analysis Error: scope stack overflow\n");
        exit(1);
    }
    OwnScope *s = &stack->scopes[stack->depth++];
    s->block_node = block_node;
    s->vars = NULL;
    s->count = 0;
    s->capacity = 0;
    s->is_loop_scope = is_loop;
}

static void pop_own_scope(OwnScopeStack *stack) {
    if (stack->depth > 0) {
        stack->depth--;
    }
}

static OwnScope *current_own_scope(OwnScopeStack *stack) {
    if (stack->depth == 0) return NULL;
    return &stack->scopes[stack->depth - 1];
}

static void own_scope_add_var_full_map(OwnScopeStack *stack, OwnScope *s, const char *name, const char *class_name, int line, int col, bool is_param, bool is_borrowed_param, bool is_array, bool is_map, Type key_type, int const_size) {
    if (!name) return;
    if (s->count >= s->capacity) {
        s->capacity = s->capacity == 0 ? 4 : s->capacity * 2;
        OwnVar *new_vars = (OwnVar *)arena_alloc_array(stack->arena, s->capacity, sizeof(OwnVar));
        if (s->vars) {
            memcpy(new_vars, s->vars, s->count * sizeof(OwnVar));
        }
        s->vars = new_vars;
    }
    s->vars[s->count].name = arena_strdup(stack->arena, name);
    s->vars[s->count].class_name = class_name ? arena_strdup(stack->arena, class_name) : NULL;
    s->vars[s->count].decl_line = line;
    s->vars[s->count].decl_col = col;
    s->vars[s->count].is_param = is_param;
    s->vars[s->count].is_borrowed_param = is_borrowed_param;
    s->vars[s->count].transferred = is_borrowed_param;
    s->vars[s->count].is_array = is_array;
    s->vars[s->count].is_map = is_map;
    s->vars[s->count].key_type = key_type;
    s->vars[s->count].const_size = const_size;
    s->count++;
}

static void own_scope_add_var_full(OwnScopeStack *stack, OwnScope *s, const char *name, const char *class_name, int line, int col, bool is_param, bool is_borrowed_param, bool is_array, int const_size) {
    own_scope_add_var_full_map(stack, s, name, class_name, line, col, is_param, is_borrowed_param, is_array, false, TY_INT, const_size);
}

static void own_scope_add_var(OwnScopeStack *stack, OwnScope *s, const char *name, const char *class_name, int line, int col, bool is_param, bool is_borrowed_param) {
    own_scope_add_var_full(stack, s, name, class_name, line, col, is_param, is_borrowed_param, false, -1);
}

static OwnVar *find_own_var(OwnScopeStack *stack, const char *name, int *out_scope_idx, int *out_var_idx) {
    if (!name) return NULL;
    for (int i = stack->depth - 1; i >= 0; i--) {
        OwnScope *s = &stack->scopes[i];
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

static MovedVar *find_moved_var(OwnScopeStack *stack, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < stack->moved_count; i++) {
        if (strcmp(stack->moved[i].name, name) == 0) {
            return &stack->moved[i];
        }
    }
    return NULL;
}

static bool is_block_terminator(AstNode *block) {
    if (!block) return false;
    if (block->type == NODE_RETURN || block->type == NODE_BREAK || block->type == NODE_CONTINUE) return true;
    if (block->type == NODE_BLOCK && block->as.block.count > 0) {
        AstNode *last = block->as.block.stmts[block->as.block.count - 1];
        return (last->type == NODE_RETURN || last->type == NODE_BREAK || last->type == NODE_CONTINUE);
    }
    return false;
}

static void mark_moved(OwnScopeStack *stack, const char *name, int move_line, int move_col, const char *dest_name, bool is_conditional) {
    if (!name) return;
    MovedVar *mv = find_moved_var(stack, name);
    if (mv) {
        mv->move_line = move_line;
        mv->move_col = move_col;
        if (dest_name) mv->dest_name = arena_strdup(stack->arena, dest_name);
        if (is_conditional) mv->is_conditional = true;
        return;
    }
    if (stack->moved_count < 256) {
        stack->moved[stack->moved_count].name = arena_strdup(stack->arena, name);
        stack->moved[stack->moved_count].move_line = move_line;
        stack->moved[stack->moved_count].move_col = move_col;
        stack->moved[stack->moved_count].dest_name = dest_name ? arena_strdup(stack->arena, dest_name) : NULL;
        stack->moved[stack->moved_count].is_conditional = is_conditional;
        stack->moved_count++;
    }
}

static void unmark_moved(OwnScopeStack *stack, const char *name) {
    if (!name) return;
    for (int i = 0; i < stack->moved_count; i++) {
        if (strcmp(stack->moved[i].name, name) == 0) {
            stack->moved[i] = stack->moved[--stack->moved_count];
            return;
        }
    }
}

static bool is_var_transferred(OwnScopeStack *stack, OwnVar *v) {
    if (!v) return true;
    if (v->is_borrowed_param) return true;
    MovedVar *mv = find_moved_var(stack, v->name);
    if (mv && !mv->is_conditional) return true;
    return false;
}

static void check_use_var(OwnScopeStack *stack, const char *name, int line, int col) {
    MovedVar *mv = find_moved_var(stack, name);
    if (mv) {
        ErrorLocation primary = {get_error_filename(), line, col};
        ErrorLocation note_loc = {get_error_filename(), mv->move_line, mv->move_col};
        char short_msg[256];
        char note_msg[256];

        if (mv->is_conditional) {
            snprintf(short_msg, sizeof(short_msg), "'%s' may be moved depending on which branch executes", name);
            snprintf(note_msg, sizeof(note_msg), "moved here, in one branch:");
            print_formatted_error(
                short_msg,
                primary,
                "value used here",
                note_msg,
                &note_loc,
                "move occurs here",
                "but not on this path — Cco conservatively treats a variable as moved after ANY branch moves it"
            );
        } else {
            snprintf(short_msg, sizeof(short_msg), "use of moved value '%s'", name);
            if (mv->dest_name) {
                snprintf(note_msg, sizeof(note_msg), "'%s' was moved into '%s' on line %d", name, mv->dest_name, mv->move_line);
            } else {
                snprintf(note_msg, sizeof(note_msg), "'%s' was moved into function call on line %d", name, mv->move_line);
            }
            print_formatted_error(
                short_msg,
                primary,
                "value used here after being moved",
                note_msg,
                &note_loc,
                "move occurs here",
                NULL
            );
        }
    }
}

static void add_free_release_to_node_map(OwnScopeStack *stack, AstNode *node, const char *var_name, const char *class_name, bool is_array, bool is_map, Type key_type) {
    if (!node || !var_name) return;
    if (!is_map && !class_name) return;
    for (int i = 0; i < node->releases_count; i++) {
        if (strcmp(node->releases_to_emit[i].var_name, var_name) == 0) return;
    }

    int cap = node->releases_count + 1;
    RefRelease *new_rels = (RefRelease *)arena_alloc_array(stack->arena, cap, sizeof(RefRelease));
    if (node->releases_to_emit) {
        memcpy(new_rels, node->releases_to_emit, node->releases_count * sizeof(RefRelease));
    }
    new_rels[node->releases_count].var_name = arena_strdup(stack->arena, var_name);
    new_rels[node->releases_count].class_name = class_name ? arena_strdup(stack->arena, class_name) : NULL;
    new_rels[node->releases_count].is_array = is_array;
    new_rels[node->releases_count].is_map = is_map;
    new_rels[node->releases_count].key_type = key_type;
    node->releases_to_emit = new_rels;
    node->releases_count++;
}

static bool is_expr_array_scope(OwnScopeStack *stack, AstNode *expr) {
    if (!expr) return false;
    if (expr->type == NODE_ALLOC && !expr->as.alloc.is_map) return true;
    if (expr->type == NODE_IDENT) {
        OwnVar *v = find_own_var(stack, expr->as.ident.name, NULL, NULL);
        if (v) return v->is_array;
    }
    if (expr->type == NODE_CALL) {
        const char *callee = expr->as.call.callee;
        if (callee) {
            if (strcmp(callee, "push") == 0 || strcmp(callee, "keys") == 0 || strcmp(callee, "args") == 0) return true;
            if (stack->program && stack->program->type == NODE_PROGRAM) {
                for (int i = 0; i < stack->program->as.program.count; i++) {
                    AstNode *fn = stack->program->as.program.functions[i];
                    if (strcmp(fn->as.function.name, callee) == 0) {
                        return fn->as.function.return_is_array;
                    }
                }
            }
        }
    }
    return false;
}

static bool is_expr_map_scope(OwnScopeStack *stack, AstNode *expr) {
    if (!expr) return false;
    if (expr->type == NODE_ALLOC && expr->as.alloc.is_map) return true;
    if (expr->type == NODE_IDENT) {
        OwnVar *v = find_own_var(stack, expr->as.ident.name, NULL, NULL);
        if (v) return v->is_map;
    }
    if (expr->type == NODE_CALL) {
        const char *callee = expr->as.call.callee;
        if (callee) {
            if (strcmp(callee, "put") == 0) return true;
            if (stack->program && stack->program->type == NODE_PROGRAM) {
                for (int i = 0; i < stack->program->as.program.count; i++) {
                    AstNode *fn = stack->program->as.program.functions[i];
                    if (strcmp(fn->as.function.name, callee) == 0) {
                        return fn->as.function.return_is_map;
                    }
                }
            }
        }
    }
    return false;
}

static char *get_expr_class_type(OwnScopeStack *stack, AstNode *expr) {
    if (!expr) return NULL;

    switch (expr->type) {
        case NODE_IDENT: {
            OwnVar *v = find_own_var(stack, expr->as.ident.name, NULL, NULL);
            if (v) return v->class_name;
            if (stack->current_class_name && strcmp(expr->as.ident.name, "self") == 0) {
                return stack->current_class_name;
            }
            return NULL;
        }

        case NODE_INDEX: {
            if (expr->as.index.array_expr) {
                return get_expr_class_type(stack, expr->as.index.array_expr);
            } else if (expr->as.index.array_name) {
                OwnVar *v = find_own_var(stack, expr->as.index.array_name, NULL, NULL);
                if (v) return v->class_name;
            }
            return NULL;
        }

        case NODE_NEW:
            return expr->as.new_expr.class_name;

        case NODE_MEMBER: {
            char *obj_cls = get_expr_class_type(stack, expr->as.member.object);
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
            const char *callee = expr->as.call.callee;
            if (callee) {
                if ((strcmp(callee, "pop") == 0 || strcmp(callee, "push") == 0 || strcmp(callee, "get") == 0 || strcmp(callee, "remove") == 0) && expr->as.call.arg_count > 0) {
                    return get_expr_class_type(stack, expr->as.call.args[0]);
                }
            }
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
            char *obj_cls = get_expr_class_type(stack, expr->as.method_call.object);
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

        case NODE_UNARY:
            if (strcmp(expr->as.unary.op, "&") == 0) {
                return get_expr_class_type(stack, expr->as.unary.operand);
            }
            return NULL;

        case NODE_ALLOC:
            if (expr->as.alloc.elem_type == TY_CLASS) {
                return expr->as.alloc.class_name;
            }
            return NULL;

        default:
            return NULL;
    }
}

static bool is_heap_class_or_enum(ClassTable *ct, const char *name) {
    if (!ct || !name) return false;
    return (find_class(ct, name) != NULL || find_enum(ct, name) != NULL);
}

static void analyze_own_node(OwnScopeStack *stack, AstNode *node);

static void analyze_own_block(OwnScopeStack *stack, AstNode *block_node, bool is_loop, bool is_fn_body) {
    push_own_scope(stack, block_node, is_loop);
    OwnScope *s = current_own_scope(stack);

    if (is_fn_body && stack->current_function) {
        if (stack->current_function->type == NODE_METHOD) {
            AstNode *m = stack->current_function;
            for (int p = 0; p < m->as.method.param_count; p++) {
                if (m->as.method.param_types[p] == TY_CLASS && m->as.method.param_class_names[p] && is_heap_class_or_enum(stack->ct, m->as.method.param_class_names[p])) {
                    bool is_bor = m->as.method.param_is_borrowed ? m->as.method.param_is_borrowed[p] : (p == 0);
                    own_scope_add_var(stack, s, m->as.method.param_names[p], m->as.method.param_class_names[p], m->as.method.param_lines ? m->as.method.param_lines[p] : m->line, m->as.method.param_cols ? m->as.method.param_cols[p] : m->col, true, is_bor);
                }
            }
        } else if (stack->current_function->type == NODE_FUNCTION) {
            AstNode *f = stack->current_function;
            for (int p = 0; p < f->as.function.param_count; p++) {
                if (f->as.function.param_types[p] == TY_CLASS && f->as.function.param_class_names[p] && is_heap_class_or_enum(stack->ct, f->as.function.param_class_names[p])) {
                    bool is_bor = f->as.function.param_is_borrowed ? f->as.function.param_is_borrowed[p] : false;
                    own_scope_add_var(stack, s, f->as.function.param_names[p], f->as.function.param_class_names[p], f->as.function.param_lines ? f->as.function.param_lines[p] : f->line, f->as.function.param_cols ? f->as.function.param_cols[p] : f->col, true, is_bor);
                }
            }
        }
    }

    for (int i = 0; i < block_node->as.block.count; i++) {
        AstNode *stmt = block_node->as.block.stmts[i];

        if (stmt->type == NODE_LET) {
            analyze_own_node(stack, stmt->as.let.value);

            if (!stmt->as.let.is_array && stmt->as.let.value && stmt->as.let.value->type == NODE_INDEX) {
                AstNode *idx_node = stmt->as.let.value;
                char *elem_cls = get_expr_class_type(stack, idx_node);
                if (elem_cls != NULL) {
                    const char *arr_n = idx_node->as.index.array_name;
                    if (!arr_n && idx_node->as.index.array_expr && idx_node->as.index.array_expr->type == NODE_IDENT) {
                        arr_n = idx_node->as.index.array_expr->as.ident.name;
                    }
                    char short_msg[256];
                    snprintf(short_msg, sizeof(short_msg), "cannot move out of array element '%s[0]' — array elements can only be borrowed, not moved, in this version of Cco", arr_n ? arr_n : "arr");
                    ErrorLocation primary = {get_error_filename(), idx_node->line, idx_node->col};
                    print_formatted_error(short_msg, primary, "cannot move out of array element", NULL, NULL, NULL, NULL);
                    exit(1);
                }
            }

            char *cls_name = stmt->as.let.class_name;
            if (!cls_name && stmt->as.let.var_type == TY_CLASS) {
                cls_name = stmt->as.let.class_name;
            }
            if (!cls_name) {
                cls_name = get_expr_class_type(stack, stmt->as.let.value);
            }
            if (!cls_name && stmt->as.let.is_array && stmt->as.let.var_type == TY_STRING) {
                cls_name = "string";
            }

            bool is_get_borrow = false;
            if (stmt->as.let.value && stmt->as.let.value->type == NODE_CALL) {
                const char *callee = stmt->as.let.value->as.call.callee;
                if (callee && strcmp(callee, "get") == 0) {
                    is_get_borrow = true;
                }
            }

            if (stmt->as.let.is_map || stmt->as.let.is_array || (cls_name && is_heap_class_or_enum(stack->ct, cls_name))) {
                if (cls_name && is_heap_class_or_enum(stack->ct, cls_name)) {
                    stmt->as.let.class_name = arena_strdup(stack->arena, cls_name);
                    stmt->as.let.var_type = TY_CLASS;
                }

                AstNode *val = stmt->as.let.value;
                if (val && val->type == NODE_IDENT) {
                    const char *rhs_name = val->as.ident.name;
                    OwnVar *rhs_var = find_own_var(stack, rhs_name, NULL, NULL);
                    if (rhs_var) {
                        check_use_var(stack, rhs_name, val->line, val->col);
                        mark_moved(stack, rhs_name, val->line, val->col, stmt->as.let.name, false);
                    }
                }

                int const_size = -1;
                if (stmt->as.let.value && stmt->as.let.value->type == NODE_ALLOC) {
                    AstNode *cnt = stmt->as.let.value->as.alloc.count_expr;
                    if (cnt && cnt->type == NODE_LITERAL && cnt->as.literal.lit_type == TY_INT) {
                        const_size = (int)cnt->as.literal.val.i;
                    }
                }

                unmark_moved(stack, stmt->as.let.name);
                own_scope_add_var_full_map(stack, s, stmt->as.let.name, cls_name, stmt->line, stmt->col, false, is_get_borrow, stmt->as.let.is_array, stmt->as.let.is_map, stmt->as.let.key_type, const_size);
            }
        } else if (stmt->type == NODE_ASSIGN) {
            analyze_own_node(stack, stmt->as.assign.value);

            AstNode *val = stmt->as.assign.value;
            if (val->type == NODE_IDENT) {
                const char *rhs_name = val->as.ident.name;
                OwnVar *rhs_var = find_own_var(stack, rhs_name, NULL, NULL);
                if (rhs_var) {
                    check_use_var(stack, rhs_name, val->line, val->col);
                    mark_moved(stack, rhs_name, val->line, val->col, stmt->as.assign.name, false);
                }
            }

            const char *lhs_name = stmt->as.assign.name;
            OwnVar *lhs_var = find_own_var(stack, lhs_name, NULL, NULL);
            if (lhs_var) {
                if (!lhs_var->is_array && stmt->as.assign.value && stmt->as.assign.value->type == NODE_INDEX) {
                    AstNode *idx_node = stmt->as.assign.value;
                    char *elem_cls = get_expr_class_type(stack, idx_node);
                    if (elem_cls != NULL) {
                        const char *arr_n = idx_node->as.index.array_name;
                        if (!arr_n && idx_node->as.index.array_expr && idx_node->as.index.array_expr->type == NODE_IDENT) {
                            arr_n = idx_node->as.index.array_expr->as.ident.name;
                        }
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "cannot move out of array element '%s[0]' — array elements can only be borrowed, not moved, in this version of Cco", arr_n ? arr_n : "arr");
                        ErrorLocation primary = {get_error_filename(), idx_node->line, idx_node->col};
                        print_formatted_error(short_msg, primary, "cannot move out of array element", NULL, NULL, NULL, NULL);
                        exit(1);
                    }
                }

                stmt->as.assign.class_name = lhs_var->class_name ? arena_strdup(stack->arena, lhs_var->class_name) : NULL;
                if (!lhs_var->is_array && !lhs_var->is_map && !find_moved_var(stack, lhs_name) && !lhs_var->transferred) {
                    stmt->as.assign.release_old = true;
                }
                unmark_moved(stack, lhs_name);
                lhs_var->transferred = false;
            }
        } else if (stmt->type == NODE_INDEX_ASSIGN) {
            analyze_own_node(stack, stmt->as.index_assign.index);
            analyze_own_node(stack, stmt->as.index_assign.value);

            AstNode *val = stmt->as.index_assign.value;
            if (val->type == NODE_IDENT) {
                const char *rhs_name = val->as.ident.name;
                OwnVar *rhs_var = find_own_var(stack, rhs_name, NULL, NULL);
                if (rhs_var) {
                    check_use_var(stack, rhs_name, val->line, val->col);
                    mark_moved(stack, rhs_name, val->line, val->col, NULL, false);
                }
            }

            const char *arr_name = stmt->as.index_assign.array_name;
            if (!arr_name && stmt->as.index_assign.array_expr && stmt->as.index_assign.array_expr->type == NODE_IDENT) {
                arr_name = stmt->as.index_assign.array_expr->as.ident.name;
            }
            if (arr_name) {
                OwnVar *arr_var = find_own_var(stack, arr_name, NULL, NULL);
                if (arr_var) {
                    if (arr_var->const_size >= 0) {
                        AstNode *idx_n = stmt->as.index_assign.index;
                        if (idx_n && idx_n->type == NODE_LITERAL && idx_n->as.literal.lit_type == TY_INT) {
                            long idx_val = idx_n->as.literal.val.i;
                            if (idx_val < 0 || idx_val >= arr_var->const_size) {
                                ErrorLocation primary = {get_error_filename(), stmt->line, stmt->col};
                                char short_msg[256];
                                snprintf(short_msg, sizeof(short_msg), "index %ld out of bounds for array of length %d", idx_val, arr_var->const_size);
                                print_formatted_error(short_msg, primary, "index out of bounds", NULL, NULL, NULL, NULL);
                                exit(1);
                            }
                        }
                    }

                    if (val && val->type == NODE_LITERAL && val->as.literal.lit_type == TY_STRING && arr_var->is_array) {
                        ErrorLocation primary = {get_error_filename(), stmt->line, stmt->col};
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "cannot assign string to array of type %s[]", arr_var->class_name);
                        print_formatted_error(short_msg, primary, "type mismatch", NULL, NULL, NULL, NULL);
                        exit(1);
                    }
                }
            }
        } else if (stmt->type == NODE_MEMBER_ASSIGN) {
            analyze_own_node(stack, stmt->as.member_assign.object);
            analyze_own_node(stack, stmt->as.member_assign.value);

            char *obj_cls = get_expr_class_type(stack, stmt->as.member_assign.object);
            if (obj_cls) {
                ClassDef *cd = find_class(stack->ct, obj_cls);
                FieldInfo *fi = find_field(cd, stmt->as.member_assign.member_name);
                if (fi && fi->type == TY_CLASS) {
                    stmt->as.member_assign.field_class_name = arena_strdup(stack->arena, fi->class_name);
                    stmt->as.member_assign.release_old = true;

                    AstNode *val = stmt->as.member_assign.value;
                    if (val->type == NODE_IDENT) {
                        const char *rhs_name = val->as.ident.name;
                        OwnVar *rhs_var = find_own_var(stack, rhs_name, NULL, NULL);
                        if (rhs_var) {
                            check_use_var(stack, rhs_name, val->line, val->col);
                            mark_moved(stack, rhs_name, val->line, val->col, NULL, false);
                        }
                    }
                }
            }
        } else {
            analyze_own_node(stack, stmt);
        }
    }

    for (int i = 0; i < s->count; i++) {
        if (!is_var_transferred(stack, &s->vars[i])) {
            add_free_release_to_node_map(stack, block_node, s->vars[i].name, s->vars[i].class_name, s->vars[i].is_array, s->vars[i].is_map, s->vars[i].key_type);
        }
    }

    pop_own_scope(stack);
}

static void analyze_own_node(OwnScopeStack *stack, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            for (int c = 0; c < node->as.program.class_count; c++) {
                AstNode *cls = node->as.program.classes[c];
                stack->current_class_name = cls->as.class_decl.name;
                for (int m = 0; m < cls->as.class_decl.method_count; m++) {
                    AstNode *m_node = cls->as.class_decl.methods[m];
                    stack->current_function = m_node;
                    stack->moved_count = 0;
                    analyze_own_block(stack, m_node->as.method.body, false, true);
                }
            }
            stack->current_class_name = NULL;

            for (int f = 0; f < node->as.program.count; f++) {
                AstNode *fn = node->as.program.functions[f];
                stack->current_function = fn;
                stack->moved_count = 0;
                analyze_own_block(stack, fn->as.function.body, false, true);
            }
            stack->current_function = NULL;
            break;
        }

        case NODE_BLOCK:
            analyze_own_block(stack, node, false, false);
            break;

        case NODE_IDENT: {
            OwnVar *v = find_own_var(stack, node->as.ident.name, NULL, NULL);
            if (v) {
                check_use_var(stack, node->as.ident.name, node->line, node->col);
            }
            break;
        }

        case NODE_IF: {
            analyze_own_node(stack, node->as.if_stmt.cond);

            int orig_moved_count = stack->moved_count;
            MovedVar orig_moved[128];
            memcpy(orig_moved, stack->moved, orig_moved_count * sizeof(MovedVar));

            bool then_term = is_block_terminator(node->as.if_stmt.then_b);
            if (node->as.if_stmt.then_b) {
                if (node->as.if_stmt.then_b->type == NODE_BLOCK) {
                    analyze_own_block(stack, node->as.if_stmt.then_b, false, false);
                } else {
                    analyze_own_node(stack, node->as.if_stmt.then_b);
                }
            }

            int then_moved_count = stack->moved_count;
            MovedVar then_moved[128];
            memcpy(then_moved, stack->moved, then_moved_count * sizeof(MovedVar));

            // Restore moved state before else branch
            stack->moved_count = orig_moved_count;
            memcpy(stack->moved, orig_moved, orig_moved_count * sizeof(MovedVar));

            bool else_term = is_block_terminator(node->as.if_stmt.else_b);
            if (node->as.if_stmt.else_b) {
                if (node->as.if_stmt.else_b->type == NODE_BLOCK) {
                    analyze_own_block(stack, node->as.if_stmt.else_b, false, false);
                } else {
                    analyze_own_node(stack, node->as.if_stmt.else_b);
                }
            }

            int else_moved_count = stack->moved_count;
            MovedVar else_moved[128];
            memcpy(else_moved, stack->moved, else_moved_count * sizeof(MovedVar));

            if (then_term && else_term) {
                stack->moved_count = orig_moved_count;
                memcpy(stack->moved, orig_moved, orig_moved_count * sizeof(MovedVar));
            } else if (then_term) {
                stack->moved_count = else_moved_count;
                memcpy(stack->moved, else_moved, else_moved_count * sizeof(MovedVar));
            } else if (else_term || !node->as.if_stmt.else_b) {
                if (else_term) {
                    stack->moved_count = then_moved_count;
                    memcpy(stack->moved, then_moved, then_moved_count * sizeof(MovedVar));
                } else {
                    stack->moved_count = orig_moved_count;
                    memcpy(stack->moved, orig_moved, orig_moved_count * sizeof(MovedVar));
                    for (int i = orig_moved_count; i < then_moved_count; i++) {
                        mark_moved(stack, then_moved[i].name, then_moved[i].move_line, then_moved[i].move_col, then_moved[i].dest_name, true);
                    }
                }
            } else {
                stack->moved_count = orig_moved_count;
                memcpy(stack->moved, orig_moved, orig_moved_count * sizeof(MovedVar));
                for (int i = orig_moved_count; i < then_moved_count; i++) {
                    bool in_else = false;
                    for (int j = orig_moved_count; j < else_moved_count; j++) {
                        if (strcmp(then_moved[i].name, else_moved[j].name) == 0) {
                            in_else = true;
                            break;
                        }
                    }
                    mark_moved(stack, then_moved[i].name, then_moved[i].move_line, then_moved[i].move_col, then_moved[i].dest_name, !in_else);
                }
                for (int j = orig_moved_count; j < else_moved_count; j++) {
                    bool in_then = false;
                    for (int i = orig_moved_count; i < then_moved_count; i++) {
                        if (strcmp(else_moved[j].name, then_moved[i].name) == 0) {
                            in_then = true;
                            break;
                        }
                    }
                    if (!in_then) {
                        mark_moved(stack, else_moved[j].name, else_moved[j].move_line, else_moved[j].move_col, else_moved[j].dest_name, true);
                    }
                }
            }
            break;
        }

        case NODE_WHILE:
            analyze_own_node(stack, node->as.while_stmt.cond);
            if (node->as.while_stmt.body) {
                if (node->as.while_stmt.body->type == NODE_BLOCK) {
                    analyze_own_block(stack, node->as.while_stmt.body, true, false);
                } else {
                    analyze_own_node(stack, node->as.while_stmt.body);
                }
            }
            break;

        case NODE_FOR: {
            push_own_scope(stack, node, true);
            if (node->as.for_stmt.init) analyze_own_node(stack, node->as.for_stmt.init);
            if (node->as.for_stmt.cond) analyze_own_node(stack, node->as.for_stmt.cond);
            if (node->as.for_stmt.step) analyze_own_node(stack, node->as.for_stmt.step);
            if (node->as.for_stmt.body) {
                if (node->as.for_stmt.body->type == NODE_BLOCK) {
                    analyze_own_block(stack, node->as.for_stmt.body, false, false);
                } else {
                    analyze_own_node(stack, node->as.for_stmt.body);
                }
            }
            OwnScope *fs = current_own_scope(stack);
            for (int i = 0; i < fs->count; i++) {
                if (!is_var_transferred(stack, &fs->vars[i])) {
                    add_free_release_to_node_map(stack, node, fs->vars[i].name, fs->vars[i].class_name, fs->vars[i].is_array, fs->vars[i].is_map, fs->vars[i].key_type);
                }
            }
            pop_own_scope(stack);
            break;
        }

        case NODE_FOR_EACH: {
            analyze_own_node(stack, node->as.for_each.collection_expr);
            push_own_scope(stack, node, true);
            OwnScope *s = current_own_scope(stack);
            char *elem_cls = get_expr_class_type(stack, node->as.for_each.collection_expr);
            if (elem_cls) {
                own_scope_add_var_full(stack, s, node->as.for_each.loop_var_name, elem_cls, node->line, node->col, true, true, false, -1);
            }
            if (node->as.for_each.body) {
                if (node->as.for_each.body->type == NODE_BLOCK) {
                    analyze_own_block(stack, node->as.for_each.body, false, false);
                } else {
                    analyze_own_node(stack, node->as.for_each.body);
                }
            }
            pop_own_scope(stack);
            break;
        }

        case NODE_CALL: {
            const char *callee = node->as.call.callee;
            if (strcmp(callee, "push") == 0) {
                if (node->as.call.arg_count == 2) {
                    AstNode *arr_arg = node->as.call.args[0];
                    AstNode *val_arg = node->as.call.args[1];
                    analyze_own_node(stack, arr_arg);
                    analyze_own_node(stack, val_arg);

                    if (!is_expr_array_scope(stack, arr_arg)) {
                        const char *var_n = (arr_arg->type == NODE_IDENT) ? arr_arg->as.ident.name : "expression";
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "'%s' is not an array", var_n);
                        ErrorLocation primary = {get_error_filename(), arr_arg->line, arr_arg->col};
                        print_formatted_error(short_msg, primary, "type mismatch", NULL, NULL, NULL, NULL);
                        exit(1);
                    }

                    if (val_arg->type == NODE_IDENT) {
                        const char *val_name = val_arg->as.ident.name;
                        OwnVar *val_var = find_own_var(stack, val_name, NULL, NULL);
                        if (val_var) {
                            check_use_var(stack, val_name, val_arg->line, val_arg->col);
                            mark_moved(stack, val_name, val_arg->line, val_arg->col, NULL, false);
                        }
                    }
                }
                break;
            }
            if (strcmp(callee, "pop") == 0) {
                // NOTE (v9): pop() is the one sanctioned exception to v5's "no move out of array" rule.
                // Unlike arbitrary slot indexing (e.g. arr[i]), pop() only ever operates on the LAST
                // occupied slot and immediately decrements length. Therefore, all elements below length
                // remain continuously occupied, maintaining a clean single-ownership invariant.
                if (node->as.call.arg_count == 1) {
                    AstNode *arr_arg = node->as.call.args[0];
                    analyze_own_node(stack, arr_arg);
                    if (!is_expr_array_scope(stack, arr_arg)) {
                        const char *var_n = (arr_arg->type == NODE_IDENT) ? arr_arg->as.ident.name : "expression";
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "'%s' is not an array", var_n);
                        ErrorLocation primary = {get_error_filename(), arr_arg->line, arr_arg->col};
                        print_formatted_error(short_msg, primary, "type mismatch", NULL, NULL, NULL, NULL);
                        exit(1);
                    }
                }
                break;
            }
            if (strcmp(callee, "put") == 0) {
                if (node->as.call.arg_count == 3) {
                    AstNode *map_arg = node->as.call.args[0];
                    AstNode *key_arg = node->as.call.args[1];
                    AstNode *val_arg = node->as.call.args[2];
                    analyze_own_node(stack, map_arg);
                    analyze_own_node(stack, key_arg);
                    analyze_own_node(stack, val_arg);

                    if (!is_expr_map_scope(stack, map_arg)) {
                        const char *var_n = (map_arg->type == NODE_IDENT) ? map_arg->as.ident.name : "expression";
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "'%s' is not a map", var_n);
                        ErrorLocation primary = {get_error_filename(), map_arg->line, map_arg->col};
                        print_formatted_error(short_msg, primary, "type mismatch", NULL, NULL, NULL, NULL);
                        exit(1);
                    }

                    if (val_arg->type == NODE_IDENT) {
                        const char *val_name = val_arg->as.ident.name;
                        OwnVar *val_var = find_own_var(stack, val_name, NULL, NULL);
                        if (val_var && val_var->class_name && !val_var->is_array && !val_var->is_map) {
                            check_use_var(stack, val_name, val_arg->line, val_arg->col);
                            mark_moved(stack, val_name, val_arg->line, val_arg->col, NULL, false);
                        }
                    }
                }
                break;
            }
            if (strcmp(callee, "get") == 0 || strcmp(callee, "has") == 0 || strcmp(callee, "remove") == 0) {
                if (node->as.call.arg_count == 2) {
                    AstNode *map_arg = node->as.call.args[0];
                    AstNode *key_arg = node->as.call.args[1];
                    analyze_own_node(stack, map_arg);
                    analyze_own_node(stack, key_arg);

                    if (!is_expr_map_scope(stack, map_arg)) {
                        const char *var_n = (map_arg->type == NODE_IDENT) ? map_arg->as.ident.name : "expression";
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "'%s' is not a map", var_n);
                        ErrorLocation primary = {get_error_filename(), map_arg->line, map_arg->col};
                        print_formatted_error(short_msg, primary, "type mismatch", NULL, NULL, NULL, NULL);
                        exit(1);
                    }
                }
                break;
            }
            if (strcmp(callee, "keys") == 0) {
                if (node->as.call.arg_count == 1) {
                    AstNode *map_arg = node->as.call.args[0];
                    analyze_own_node(stack, map_arg);

                    if (!is_expr_map_scope(stack, map_arg)) {
                        const char *var_n = (map_arg->type == NODE_IDENT) ? map_arg->as.ident.name : "expression";
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "'%s' is not a map", var_n);
                        ErrorLocation primary = {get_error_filename(), map_arg->line, map_arg->col};
                        print_formatted_error(short_msg, primary, "type mismatch", NULL, NULL, NULL, NULL);
                        exit(1);
                    }
                }
                break;
            }
            if (strcmp(callee, "len") == 0) {
                if (node->as.call.arg_count == 1) {
                    AstNode *arg = node->as.call.args[0];
                    analyze_own_node(stack, arg);
                }
                break;
            }

            AstNode *fn = NULL;
            if (stack->program && stack->program->type == NODE_PROGRAM) {
                for (int f = 0; f < stack->program->as.program.count; f++) {
                    if (strcmp(stack->program->as.program.functions[f]->as.function.name, node->as.call.callee) == 0) {
                        fn = stack->program->as.program.functions[f];
                        break;
                    }
                }
            }

            for (int i = 0; i < node->as.call.arg_count; i++) {
                AstNode *arg = node->as.call.args[i];
                analyze_own_node(stack, arg);

                if (arg->type == NODE_IDENT) {
                    const char *arg_name = arg->as.ident.name;
                    OwnVar *arg_var = find_own_var(stack, arg_name, NULL, NULL);
                    if (arg_var) {
                        check_use_var(stack, arg_name, arg->line, arg->col);
                        bool is_bor = (fn && i < fn->as.function.param_count && fn->as.function.param_is_borrowed) ? fn->as.function.param_is_borrowed[i] : false;
                        if (!is_bor) {
                            mark_moved(stack, arg_name, arg->line, arg->col, NULL, false);
                        }
                    }
                } else if (arg->type == NODE_INDEX) {
                    bool is_bor = (fn && i < fn->as.function.param_count && fn->as.function.param_is_borrowed) ? fn->as.function.param_is_borrowed[i] : false;
                    bool is_class_param = (fn && i < fn->as.function.param_count && fn->as.function.param_types[i] == TY_CLASS && (!fn->as.function.param_is_array || !fn->as.function.param_is_array[i]));
                    if (is_class_param && !is_bor) {
                        char *elem_cls = get_expr_class_type(stack, arg);
                        if (elem_cls != NULL) {
                            const char *arr_n = arg->as.index.array_name;
                            if (!arr_n && arg->as.index.array_expr && arg->as.index.array_expr->type == NODE_IDENT) {
                                arr_n = arg->as.index.array_expr->as.ident.name;
                            }
                            char short_msg[256];
                            snprintf(short_msg, sizeof(short_msg), "cannot move out of array element '%s[0]' — array elements can only be borrowed, not moved, in this version of Cco", arr_n ? arr_n : "arr");
                            ErrorLocation primary = {get_error_filename(), arg->line, arg->col};
                            print_formatted_error(short_msg, primary, "cannot move out of array element", NULL, NULL, NULL, NULL);
                            exit(1);
                        }
                    }
                }
            }
            break;
        }

        case NODE_METHOD_CALL: {
            analyze_own_node(stack, node->as.method_call.object);
            char *obj_cls = get_expr_class_type(stack, node->as.method_call.object);

            MethodInfo *mi = NULL;
            if (obj_cls) {
                node->as.method_call.target_class_name = arena_strdup(stack->arena, obj_cls);
                ClassDef *cd = find_class(stack->ct, obj_cls);
                mi = find_method(cd, node->as.method_call.method_name);
            }

            for (int i = 0; i < node->as.method_call.arg_count; i++) {
                AstNode *arg = node->as.method_call.args[i];
                analyze_own_node(stack, arg);

                if (arg->type == NODE_IDENT) {
                    const char *arg_name = arg->as.ident.name;
                    OwnVar *arg_var = find_own_var(stack, arg_name, NULL, NULL);
                    if (arg_var) {
                        check_use_var(stack, arg_name, arg->line, arg->col);
                        bool is_bor = false;
                        if (mi && mi->method_node) {
                            AstNode *mn = mi->method_node;
                            int param_idx = i + 1; // param 0 is self
                            if (param_idx < mn->as.method.param_count && mn->as.method.param_is_borrowed) {
                                is_bor = mn->as.method.param_is_borrowed[param_idx];
                            }
                        }
                        if (!is_bor) {
                            mark_moved(stack, arg_name, arg->line, arg->col, NULL, false);
                        }
                    }
                } else if (arg->type == NODE_INDEX) {
                    bool is_bor = false;
                    bool is_class_param = false;
                    if (mi && mi->method_node) {
                        AstNode *mn = mi->method_node;
                        int param_idx = i + 1;
                        if (param_idx < mn->as.method.param_count) {
                            if (mn->as.method.param_is_borrowed) is_bor = mn->as.method.param_is_borrowed[param_idx];
                            is_class_param = (mn->as.method.param_types[param_idx] == TY_CLASS && (!mn->as.method.param_is_array || !mn->as.method.param_is_array[param_idx]));
                        }
                    }
                    if (is_class_param && !is_bor) {
                        char *elem_cls = get_expr_class_type(stack, arg);
                        if (elem_cls != NULL) {
                            const char *arr_n = arg->as.index.array_name;
                            if (!arr_n && arg->as.index.array_expr && arg->as.index.array_expr->type == NODE_IDENT) {
                                arr_n = arg->as.index.array_expr->as.ident.name;
                            }
                            char short_msg[256];
                            snprintf(short_msg, sizeof(short_msg), "cannot move out of array element '%s[0]' — array elements can only be borrowed, not moved, in this version of Cco", arr_n ? arr_n : "arr");
                            ErrorLocation primary = {get_error_filename(), arg->line, arg->col};
                            print_formatted_error(short_msg, primary, "cannot move out of array element", NULL, NULL, NULL, NULL);
                            exit(1);
                        }
                    }
                }
            }
            break;
        }

        case NODE_INDEX: {
            if (node->as.index.array_expr) analyze_own_node(stack, node->as.index.array_expr);
            analyze_own_node(stack, node->as.index.index);

            const char *arr_name = node->as.index.array_name;
            if (!arr_name && node->as.index.array_expr && node->as.index.array_expr->type == NODE_IDENT) {
                arr_name = node->as.index.array_expr->as.ident.name;
            }
            if (arr_name) {
                OwnVar *arr_var = find_own_var(stack, arr_name, NULL, NULL);
                if (arr_var && arr_var->const_size >= 0) {
                    AstNode *idx_n = node->as.index.index;
                    if (idx_n && idx_n->type == NODE_LITERAL && idx_n->as.literal.lit_type == TY_INT) {
                        long idx_val = idx_n->as.literal.val.i;
                        if (idx_val < 0 || idx_val >= arr_var->const_size) {
                            ErrorLocation primary = {get_error_filename(), node->line, node->col};
                            char short_msg[256];
                            snprintf(short_msg, sizeof(short_msg), "index %ld out of bounds for array of length %d", idx_val, arr_var->const_size);
                            print_formatted_error(short_msg, primary, "index out of bounds", NULL, NULL, NULL, NULL);
                            exit(1);
                        }
                    }
                }
            }
            break;
        }

        case NODE_RETURN: {
            if (node->as.return_stmt.value) {
                analyze_own_node(stack, node->as.return_stmt.value);
            }

            if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_IDENT) {
                const char *ret_var = node->as.return_stmt.value->as.ident.name;
                int s_idx = -1, v_idx = -1;
                OwnVar *v = find_own_var(stack, ret_var, &s_idx, &v_idx);
                if (v) {
                    check_use_var(stack, ret_var, node->as.return_stmt.value->line, node->as.return_stmt.value->col);
                    if (v->is_borrowed_param) {
                        char short_msg[256];
                        char note_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "cannot return borrowed value '%s'", ret_var);
                        snprintf(note_msg, sizeof(note_msg), "'%s' is a borrowed parameter (&%s) — this function does not own it and cannot transfer ownership to the caller", ret_var, v->class_name ? v->class_name : "");
                        ErrorLocation primary = {get_error_filename(), node->as.return_stmt.value->line, node->as.return_stmt.value->col};
                        ErrorLocation note_loc = {get_error_filename(), v->decl_line, v->decl_col};
                        print_formatted_error(
                            short_msg,
                            primary,
                            "return of borrowed value",
                            note_msg,
                            &note_loc,
                            "parameter declared as borrowed here",
                            NULL
                        );
                    }
                    mark_moved(stack, ret_var, node->as.return_stmt.value->line, node->as.return_stmt.value->col, NULL, false);
                }
            }

            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                OwnScope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!is_var_transferred(stack, &s->vars[i])) {
                        add_free_release_to_node_map(stack, node, s->vars[i].name, s->vars[i].class_name, s->vars[i].is_array, s->vars[i].is_map, s->vars[i].key_type);
                    }
                }
            }
            break;
        }

        case NODE_BREAK:
        case NODE_CONTINUE: {
            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                OwnScope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!is_var_transferred(stack, &s->vars[i])) {
                        add_free_release_to_node_map(stack, node, s->vars[i].name, s->vars[i].class_name, s->vars[i].is_array, s->vars[i].is_map, s->vars[i].key_type);
                    }
                }
                if (s->is_loop_scope) break;
            }
            break;
        }

        case NODE_MATCH: {
            // 1. Scrutinee expression: Borrowed!
            AstNode *scrut = node->as.match_stmt.expr;
            if (scrut->type == NODE_IDENT) {
                check_use_var(stack, scrut->as.ident.name, scrut->line, scrut->col);
            } else if (scrut->type == NODE_UNARY && strcmp(scrut->as.unary.op, "&") == 0 && scrut->as.unary.operand->type == NODE_IDENT) {
                check_use_var(stack, scrut->as.unary.operand->as.ident.name, scrut->as.unary.operand->line, scrut->as.unary.operand->col);
            } else {
                analyze_own_node(stack, scrut);
            }

            // 2. Identify the enum type of scrutinee
            const char *enum_name = node->as.match_stmt.enum_name;
            if (!enum_name) {
                if (scrut->type == NODE_IDENT) {
                    OwnVar *ov = find_own_var(stack, scrut->as.ident.name, NULL, NULL);
                    if (ov) enum_name = ov->class_name;
                } else if (scrut->type == NODE_UNARY && strcmp(scrut->as.unary.op, "&") == 0 && scrut->as.unary.operand->type == NODE_IDENT) {
                    OwnVar *ov = find_own_var(stack, scrut->as.unary.operand->as.ident.name, NULL, NULL);
                    if (ov) enum_name = ov->class_name;
                }
            }
            if (!enum_name) {
                for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
                    if (!node->as.match_stmt.arms[a]->as.match_arm.is_wildcard) {
                        enum_name = node->as.match_stmt.arms[a]->as.match_arm.enum_name;
                        break;
                    }
                }
            }
            if (enum_name) {
                node->as.match_stmt.enum_name = arena_strdup(stack->arena, enum_name);
            }

            EnumDef *edef = enum_name ? find_enum(stack->ct, enum_name) : NULL;

            // 3. Check duplicate arms, unknown variants, bindings, and exhaustiveness
            int total_variants = edef ? edef->variant_count : 0;
            bool *handled = calloc(total_variants > 0 ? total_variants : 1, sizeof(bool));
            ErrorLocation *arm_locs = calloc(total_variants > 0 ? total_variants : 1, sizeof(ErrorLocation));
            bool has_wildcard = false;

            for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
                AstNode *arm = node->as.match_stmt.arms[a];
                if (arm->as.match_arm.is_wildcard) {
                    if (has_wildcard) {
                        free(handled); free(arm_locs);
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "duplicate match arm for wildcard '_'");
                        ErrorLocation primary = {arm->source_file ? arm->source_file : get_error_filename(), arm->as.match_arm.arm_line, arm->as.match_arm.arm_col};
                        print_formatted_error(short_msg, primary, "duplicate wildcard arm", NULL, NULL, NULL, NULL);
                    }
                    has_wildcard = true;
                } else {
                    if (edef && strcmp(arm->as.match_arm.enum_name, edef->name) != 0) {
                        free(handled); free(arm_locs);
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "'%s' has no variant named '%s'", edef->name, arm->as.match_arm.variant_name);
                        ErrorLocation primary = {arm->source_file ? arm->source_file : get_error_filename(), arm->as.match_arm.arm_line, arm->as.match_arm.arm_col};
                        print_formatted_error(short_msg, primary, "mismatched enum variant", NULL, NULL, NULL, NULL);
                    }

                    EnumVariantDef *vdef = edef ? find_enum_variant(edef, arm->as.match_arm.variant_name) : NULL;
                    if (!vdef) {
                        free(handled); free(arm_locs);
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "'%s' has no variant named '%s'", enum_name ? enum_name : "Enum", arm->as.match_arm.variant_name);
                        ErrorLocation primary = {arm->source_file ? arm->source_file : get_error_filename(), arm->as.match_arm.arm_line, arm->as.match_arm.arm_col};
                        print_formatted_error(short_msg, primary, "unknown variant", NULL, NULL, NULL, NULL);
                    }

                    int v_idx = -1;
                    for (int vi = 0; vi < edef->variant_count; vi++) {
                        if (strcmp(edef->variants[vi].name, arm->as.match_arm.variant_name) == 0) {
                            v_idx = vi;
                            break;
                        }
                    }

                    if (v_idx != -1 && handled[v_idx]) {
                        ErrorLocation primary = {arm->source_file ? arm->source_file : get_error_filename(), arm->as.match_arm.arm_line, arm->as.match_arm.arm_col};
                        ErrorLocation note_loc = arm_locs[v_idx];
                        char short_msg[256];
                        snprintf(short_msg, sizeof(short_msg), "duplicate match arm for variant '%s'", arm->as.match_arm.variant_name);
                        free(handled); free(arm_locs);
                        print_formatted_error(short_msg, primary, "duplicate match arm", "first handled here:", &note_loc, "first handled here", NULL);
                    }

                    if (v_idx != -1) {
                        handled[v_idx] = true;
                        arm_locs[v_idx] = (ErrorLocation){arm->source_file ? arm->source_file : get_error_filename(), arm->as.match_arm.arm_line, arm->as.match_arm.arm_col};
                    }
                }
            }

            // Check exhaustiveness
            if (edef && !has_wildcard) {
                int missing_count = 0;
                char missing_buf[256] = {0};
                for (int vi = 0; vi < edef->variant_count; vi++) {
                    if (!handled[vi]) {
                        if (missing_count > 0) strncat(missing_buf, ", ", sizeof(missing_buf) - strlen(missing_buf) - 1);
                        strncat(missing_buf, "'", sizeof(missing_buf) - strlen(missing_buf) - 1);
                        strncat(missing_buf, edef->variants[vi].name, sizeof(missing_buf) - strlen(missing_buf) - 1);
                        strncat(missing_buf, "'", sizeof(missing_buf) - strlen(missing_buf) - 1);
                        missing_count++;
                    }
                }
                if (missing_count > 0) {
                    free(handled); free(arm_locs);
                    char short_msg[512];
                    snprintf(short_msg, sizeof(short_msg), "match is not exhaustive — missing variant(s): %s", missing_buf);
                    ErrorLocation primary = {node->source_file ? node->source_file : get_error_filename(), node->line, node->col};
                    print_formatted_error(short_msg, primary, "non-exhaustive match", "add arms for the missing variants, or a catch-all '_ => { }' arm", NULL, NULL, NULL);
                }
            }
            free(handled);
            free(arm_locs);

            // 4. Analyze each arm body with bound variables registered as borrowed
            int orig_moved_count = stack->moved_count;
            MovedVar orig_moved[128];
            memcpy(orig_moved, stack->moved, orig_moved_count * sizeof(MovedVar));

            for (int a = 0; a < node->as.match_stmt.arm_count; a++) {
                AstNode *arm = node->as.match_stmt.arms[a];
                push_own_scope(stack, arm->as.match_arm.body, false);
                OwnScope *s = current_own_scope(stack);

                if (!arm->as.match_arm.is_wildcard && edef) {
                    EnumVariantDef *vdef = find_enum_variant(edef, arm->as.match_arm.variant_name);
                    if (vdef && vdef->field_count > 0) {
                        for (int b = 0; b < arm->as.match_arm.bind_count; b++) {
                            const char *bname = arm->as.match_arm.bind_names[b];
                            FieldInfo *fi = find_variant_field(vdef, bname);
                            if (fi) {
                                bool is_heap_type = (fi->type == TY_CLASS && fi->class_name && is_heap_class_or_enum(stack->ct, fi->class_name));
                                if (is_heap_type) {
                                    // Registered as borrowed!
                                    own_scope_add_var(stack, s, bname, fi->class_name, arm->as.match_arm.arm_line, arm->as.match_arm.arm_col, false, true);
                                }
                            }
                        }
                    }
                }

                // Analyze statements inside arm body
                if (arm->as.match_arm.body && arm->as.match_arm.body->type == NODE_BLOCK) {
                    for (int st = 0; st < arm->as.match_arm.body->as.block.count; st++) {
                        analyze_own_node(stack, arm->as.match_arm.body->as.block.stmts[st]);
                    }

                    // Emit frees for any owned allocations created inside the arm
                    for (int i = 0; i < s->count; i++) {
                        if (!s->vars[i].is_param && !s->vars[i].is_borrowed_param && !is_var_transferred(stack, &s->vars[i])) {
                            add_free_release_to_node_map(stack, arm->as.match_arm.body, s->vars[i].name, s->vars[i].class_name, s->vars[i].is_array, s->vars[i].is_map, s->vars[i].key_type);
                        }
                    }
                }
                pop_own_scope(stack);

                // Match is borrow-only: restore moved state across arms
                stack->moved_count = orig_moved_count;
                memcpy(stack->moved, orig_moved, orig_moved_count * sizeof(MovedVar));
            }
            break;
        }

        case NODE_EXPR_STMT:
            if (node->as.expr_stmt.expr && node->as.expr_stmt.expr->type == NODE_CALL) {
                const char *callee = node->as.expr_stmt.expr->as.call.callee;
                if (callee && strcmp(callee, "put") == 0) {
                    char short_msg[256];
                    snprintf(short_msg, sizeof(short_msg), "result of put() must be reassigned (e.g. m = put(m, key, value))");
                    ErrorLocation primary = {get_error_filename(), node->line, node->col};
                    print_formatted_error(short_msg, primary, "unassigned put() call", NULL, NULL, NULL, NULL);
                    exit(1);
                }
            }
            analyze_own_node(stack, node->as.expr_stmt.expr);
            break;
        case NODE_PRINT:
            analyze_own_node(stack, node->as.print_stmt.value);
            break;
        case NODE_MEMBER:
            analyze_own_node(stack, node->as.member.object);
            break;
        default:
            break;
    }
}

static void analyze_ownership(AstNode *program, AstArena *arena) {
    OwnScopeStack stack;
    memset(&stack, 0, sizeof(stack));
    stack.arena = arena;
    stack.program = program;
    stack.ct = build_class_table(program, arena);
    analyze_own_node(&stack, program);
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

    // Pass 3: analyze full AST for single-ownership and move tracking (Cco v3)
    analyze_ownership(program, arena);
}
