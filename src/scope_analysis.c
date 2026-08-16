#define _POSIX_C_SOURCE 200809L
#include "scope_analysis.h"
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
    // Avoid duplicates in frees_to_emit list for a single node
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

static void analyze_node(ScopeStack *stack, AstNode *node);

static void analyze_block(ScopeStack *stack, AstNode *block_node, bool is_loop) {
    push_scope(stack, block_node, is_loop);
    Scope *s = current_scope(stack);

    for (int i = 0; i < block_node->as.block.count; i++) {
        AstNode *stmt = block_node->as.block.stmts[i];

        if (stmt->type == NODE_LET) {
            analyze_node(stack, stmt->as.let.value);
            if (stmt->as.let.value->type == NODE_ALLOC) {
                stmt->is_heap_owner = true;
                scope_add_owned(stack, s, stmt->as.let.name);
            }
        } else if (stmt->type == NODE_ASSIGN) {
            analyze_node(stack, stmt->as.assign.value);

            /*
             * EDGE CASE 3 HANDLING:
             * Reassigning an owned pointer variable (e.g. p = alloc(...))
             * If p already holds an allocation in the current or outer scope,
             * we must emit free(p) BEFORE assigning the new allocation so the
             * previous memory block is not leaked.
             */
            int scope_idx = -1;
            int owned_idx = find_owned_in_stack(stack, stmt->as.assign.name, &scope_idx);
            if (owned_idx != -1 && stmt->as.assign.value->type == NODE_ALLOC) {
                stmt->free_old_on_reassign = true;
                add_free_to_node(stack, stmt, stmt->as.assign.name);
            }

            /*
             * OWNERSHIP TRANSFER (stmt = outer_var = inner_owned_var):
             * If assigning an owned pointer to an outer-scope variable or param,
             * transfer ownership so the inner scope doesn't double-free it.
             */
            if (stmt->as.assign.value->type == NODE_IDENT) {
                const char *rhs_name = stmt->as.assign.value->as.ident.name;
                int rhs_scope_idx = -1;
                int rhs_owned_idx = find_owned_in_stack(stack, rhs_name, &rhs_scope_idx);
                if (rhs_owned_idx != -1) {
                    // Mark RHS as transferred in its scope
                    stack->scopes[rhs_scope_idx].transferred[rhs_owned_idx] = true;
                    // If target LHS is in an outer scope, LHS becomes the new owner
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

    /*
     * BLOCK FALLTHROUGH EXIT:
     * Emit free() for all non-transferred variables owned by THIS block.
     * EDGE CASE 2 HANDLING:
     * Allocations inside a loop body are bound to the loop body's block scope
     * and freed here at the end of each iteration.
     */
    for (int i = 0; i < s->count; i++) {
        if (!s->transferred[i]) {
            add_free_to_node(stack, block_node, s->owned_vars[i]);
        }
    }

    // Attach owned vars summary to AST node
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

        case NODE_FUNCTION:
            analyze_block(stack, node->as.function.body, false);
            break;

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
            // For loop introduces a scope for init let statement + body
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
            // Free any variables owned directly by for loop header scope
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

            /*
             * EDGE CASE 1 & 4 HANDLING:
             * Early return inside nested scopes/loops/ifs:
             * Collect all non-transferred owned pointers across ALL enclosing scopes
             * of the current function.
             * If returning a variable (e.g. return x;), skip freeing x (ownership transfer to caller),
             * but free all other owned pointers!
             */
            const char *ret_var = NULL;
            if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_IDENT) {
                ret_var = node->as.return_stmt.value->as.ident.name;
                int r_scope_idx = -1;
                int r_owned_idx = find_owned_in_stack(stack, ret_var, &r_scope_idx);
                if (r_owned_idx != -1) {
                    stack->scopes[r_scope_idx].transferred[r_owned_idx] = true;
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
            /*
             * JUMP OUT OF LOOP (break / continue):
             * Gather all non-transferred owned pointers from current scope up to
             * the nearest enclosing loop scope and emit free() for them.
             */
            for (int s_idx = stack->depth - 1; s_idx >= 0; s_idx--) {
                Scope *s = &stack->scopes[s_idx];
                for (int i = 0; i < s->count; i++) {
                    if (!s->transferred[i]) {
                        add_free_to_node(stack, node, s->owned_vars[i]);
                    }
                }
                if (s->is_loop_scope) break; // Reached loop boundary
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

void analyze_scopes(AstNode *program, AstArena *arena) {
    ScopeStack stack;
    stack.depth = 0;
    stack.arena = arena;
    analyze_node(&stack, program);
}
