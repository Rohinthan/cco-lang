// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_lower.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarBinding {
    char *name;
    IrValue *slot;       /* Pointer from alloca */
    IrType *val_type;    /* Value type */
    struct VarBinding *next;
} VarBinding;

typedef struct LoopScope {
    IrBasicBlock *cond_block;
    IrBasicBlock *step_block;
    IrBasicBlock *exit_block;
    struct LoopScope *parent;
} LoopScope;

typedef struct {
    IrModule *module;
    IrBuilder builder;
    AstNode *current_fn_ast;
    IrFunction *current_fn;
    VarBinding *sym_table;
    LoopScope *current_loop;
} LowerCtx;

static IrType *map_ast_type(IrArena *arena, Type ast_type, bool is_array, const char *class_name) {
    if (is_array) {
        IrType *elem = map_ast_type(arena, ast_type, false, class_name);
        return ir_type_ptr(arena, elem);
    }
    switch (ast_type) {
        case TY_INT:    return ir_type_i32(arena);
        case TY_FLOAT:  return ir_type_f64(arena);
        case TY_BOOL:   return ir_type_bool(arena);
        case TY_CHAR:   return ir_type_char(arena);
        case TY_STRING: return ir_type_ptr(arena, ir_type_char(arena));
        case TY_VOID:   return ir_type_void(arena);
        case TY_CLASS:  return ir_type_ptr(arena, ir_type_struct(arena, class_name ? class_name : "Class"));
        case TY_MAP:    return ir_type_ptr(arena, ir_type_void(arena));
        default:        return ir_type_i32(arena);
    }
}

static void sym_push(LowerCtx *ctx, const char *name, IrValue *slot, IrType *val_type) {
    VarBinding *b = (VarBinding *)ir_arena_alloc(ctx->module->arena, sizeof(VarBinding));
    b->name = ir_arena_strdup(ctx->module->arena, name);
    b->slot = slot;
    b->val_type = val_type;
    b->next = ctx->sym_table;
    ctx->sym_table = b;
}

static VarBinding *sym_lookup(LowerCtx *ctx, const char *name) {
    for (VarBinding *b = ctx->sym_table; b; b = b->next) {
        if (strcmp(b->name, name) == 0) return b;
    }
    return NULL;
}

static bool is_current_block_terminated(LowerCtx *ctx) {
    if (!ctx->builder.block) return true;
    return (ctx->builder.block->last_inst && ir_is_terminator(ctx->builder.block->last_inst->op));
}

static void emit_cleanups(LowerCtx *ctx, AstNode *node) {
    if (!node || is_current_block_terminated(ctx)) return;

    /* Emit raw frees */
    for (int i = 0; i < node->frees_count; i++) {
        const char *vname = node->frees_to_emit[i].var_name;
        VarBinding *b = sym_lookup(ctx, vname);
        if (b && b->slot) {
            IrInstruction *load = ir_emit_load(&ctx->builder, b->val_type, b->slot);
            ir_emit_free(&ctx->builder, load->result, node->frees_to_emit[i].is_array);
        }
    }

    /* Emit releases */
    for (int i = 0; i < node->releases_count; i++) {
        const char *vname = node->releases_to_emit[i].var_name;
        VarBinding *b = sym_lookup(ctx, vname);
        if (b && b->slot) {
            IrInstruction *load = ir_emit_load(&ctx->builder, b->val_type, b->slot);
            ir_emit_release(&ctx->builder, load->result, node->releases_to_emit[i].class_name,
                            node->releases_to_emit[i].is_array);
        }
    }
}

/* Forward declarations */
static IrValue *lower_expr(LowerCtx *ctx, AstNode *expr);
static void lower_stmt(LowerCtx *ctx, AstNode *stmt);

static IrValue *lower_expr(LowerCtx *ctx, AstNode *expr) {
    if (!expr) return NULL;
    IrArena *arena = ctx->module->arena;

    switch (expr->type) {
        case NODE_LITERAL: {
            switch (expr->as.literal.lit_type) {
                case TY_INT: {
                    IrInstruction *inst = ir_emit_const_int(&ctx->builder, ir_type_i32(arena), expr->as.literal.val.i);
                    return inst->result;
                }
                case TY_FLOAT: {
                    IrInstruction *inst = ir_emit_const_float(&ctx->builder, expr->as.literal.val.f);
                    return inst->result;
                }
                case TY_BOOL: {
                    IrInstruction *inst = ir_emit_const_bool(&ctx->builder, expr->as.literal.val.b);
                    return inst->result;
                }
                case TY_CHAR: {
                    IrInstruction *inst = ir_emit_const_char(&ctx->builder, expr->as.literal.val.c);
                    return inst->result;
                }
                case TY_STRING: {
                    IrInstruction *inst = ir_emit_const_string(&ctx->builder, expr->as.literal.val.s);
                    return inst->result;
                }
                default: {
                    IrInstruction *inst = ir_emit_const_int(&ctx->builder, ir_type_i32(arena), 0);
                    return inst->result;
                }
            }
        }

        case NODE_IDENT: {
            VarBinding *b = sym_lookup(ctx, expr->as.ident.name);
            if (b && b->slot) {
                IrInstruction *inst = ir_emit_load(&ctx->builder, b->val_type, b->slot);
                return inst->result;
            }
            /* Check if parameter */
            for (int p = 0; p < ctx->current_fn->param_count; p++) {
                if (strcmp(ctx->current_fn->param_names[p], expr->as.ident.name) == 0) {
                    return ctx->current_fn->param_values[p];
                }
            }
            /* Fallback default 0 */
            IrInstruction *inst = ir_emit_const_int(&ctx->builder, ir_type_i32(arena), 0);
            return inst->result;
        }

        case NODE_BINARY: {
            IrValue *lhs = lower_expr(ctx, expr->as.binary.left);
            IrValue *rhs = lower_expr(ctx, expr->as.binary.right);
            if (!lhs || !rhs) return NULL;

            const char *op = expr->as.binary.op;
            IrOpcode ir_op = IR_OP_ADD;
            IrType *res_type = lhs->type;

            if (strcmp(op, "+") == 0) ir_op = IR_OP_ADD;
            else if (strcmp(op, "-") == 0) ir_op = IR_OP_SUB;
            else if (strcmp(op, "*") == 0) ir_op = IR_OP_MUL;
            else if (strcmp(op, "/") == 0) ir_op = IR_OP_DIV;
            else if (strcmp(op, "%") == 0) ir_op = IR_OP_MOD;
            else if (strcmp(op, "==") == 0) { ir_op = IR_OP_EQ; res_type = ir_type_bool(arena); }
            else if (strcmp(op, "!=") == 0) { ir_op = IR_OP_NE; res_type = ir_type_bool(arena); }
            else if (strcmp(op, "<") == 0)  { ir_op = IR_OP_LT; res_type = ir_type_bool(arena); }
            else if (strcmp(op, "<=") == 0) { ir_op = IR_OP_LE; res_type = ir_type_bool(arena); }
            else if (strcmp(op, ">") == 0)  { ir_op = IR_OP_GT; res_type = ir_type_bool(arena); }
            else if (strcmp(op, ">=") == 0) { ir_op = IR_OP_GE; res_type = ir_type_bool(arena); }
            else if (strcmp(op, "&&") == 0) { ir_op = IR_OP_AND; res_type = ir_type_bool(arena); }
            else if (strcmp(op, "||") == 0) { ir_op = IR_OP_OR; res_type = ir_type_bool(arena); }

            IrInstruction *inst = ir_emit_binary(&ctx->builder, ir_op, res_type, lhs, rhs);
            return inst->result;
        }

        case NODE_UNARY: {
            IrValue *operand = lower_expr(ctx, expr->as.unary.operand);
            if (!operand) return NULL;
            const char *op = expr->as.unary.op;
            if (strcmp(op, "-") == 0) {
                IrInstruction *inst = ir_emit_unary(&ctx->builder, IR_OP_NEG, operand->type, operand);
                return inst->result;
            } else if (strcmp(op, "!") == 0) {
                IrInstruction *inst = ir_emit_unary(&ctx->builder, IR_OP_NOT, ir_type_bool(arena), operand);
                return inst->result;
            }
            return operand;
        }

        case NODE_CALL: {
            int argc = expr->as.call.arg_count;
            IrValue **args = NULL;
            if (argc > 0) {
                args = (IrValue **)ir_arena_alloc(arena, argc * sizeof(IrValue *));
                for (int i = 0; i < argc; i++) {
                    args[i] = lower_expr(ctx, expr->as.call.args[i]);
                }
            }

            IrType *ret_type = ir_type_void(arena);
            IrFunction *callee_fn = ir_module_find_function(ctx->module, expr->as.call.callee);
            if (callee_fn) {
                ret_type = callee_fn->return_type;
            } else {
                /* Infer default i32 for non-void C functions, or void */
                ret_type = ir_type_i32(arena);
            }

            IrInstruction *inst = ir_emit_call(&ctx->builder, expr->as.call.callee, ret_type, args, argc);
            return inst->result;
        }

        default:
            return NULL;
    }
}

static void lower_stmt(LowerCtx *ctx, AstNode *stmt) {
    if (!stmt || is_current_block_terminated(ctx)) return;
    IrArena *arena = ctx->module->arena;

    switch (stmt->type) {
        case NODE_BLOCK: {
            for (int i = 0; i < stmt->as.block.count; i++) {
                if (is_current_block_terminated(ctx)) break;
                lower_stmt(ctx, stmt->as.block.stmts[i]);
            }
            emit_cleanups(ctx, stmt);
            break;
        }

        case NODE_LET: {
            IrType *vtype = map_ast_type(arena, stmt->as.let.var_type, stmt->as.let.is_array, stmt->as.let.class_name);
            /* Create alloca stack slot in entry block */
            IrBasicBlock *saved_block = ctx->builder.block;
            ctx->builder.block = ctx->current_fn->entry_block;

            char slot_name[128];
            snprintf(slot_name, sizeof(slot_name), "%s.addr", stmt->as.let.name);
            IrInstruction *alloca_inst = ir_emit_alloca(&ctx->builder, vtype, slot_name);

            ctx->builder.block = saved_block;

            sym_push(ctx, stmt->as.let.name, alloca_inst->result, vtype);

            if (stmt->as.let.value) {
                IrValue *val = lower_expr(ctx, stmt->as.let.value);
                if (val) {
                    ir_emit_store(&ctx->builder, val, alloca_inst->result);
                }
            }
            break;
        }

        case NODE_ASSIGN: {
            VarBinding *b = sym_lookup(ctx, stmt->as.assign.name);
            if (!b) break;
            IrValue *val = lower_expr(ctx, stmt->as.assign.value);
            if (val && b->slot) {
                ir_emit_store(&ctx->builder, val, b->slot);
            }
            break;
        }

        case NODE_EXPR_STMT: {
            lower_expr(ctx, stmt->as.expr_stmt.expr);
            break;
        }

        case NODE_PRINT: {
            IrValue *val = lower_expr(ctx, stmt->as.print_stmt.value);
            if (val) {
                ir_emit_print(&ctx->builder, val);
            }
            break;
        }

        case NODE_IF: {
            IrValue *cond = lower_expr(ctx, stmt->as.if_stmt.cond);
            IrBasicBlock *then_bb = ir_block_create(ctx->current_fn, "if.then");
            IrBasicBlock *else_bb = stmt->as.if_stmt.else_b ? ir_block_create(ctx->current_fn, "if.else") : NULL;
            IrBasicBlock *merge_bb = ir_block_create(ctx->current_fn, "if.merge");

            ir_emit_condbr(&ctx->builder, cond, then_bb, else_bb ? else_bb : merge_bb);

            /* Then branch */
            ir_builder_set_insert_block(&ctx->builder, then_bb);
            lower_stmt(ctx, stmt->as.if_stmt.then_b);
            if (!is_current_block_terminated(ctx)) {
                ir_emit_br(&ctx->builder, merge_bb);
            }

            /* Else branch */
            if (else_bb) {
                ir_builder_set_insert_block(&ctx->builder, else_bb);
                lower_stmt(ctx, stmt->as.if_stmt.else_b);
                if (!is_current_block_terminated(ctx)) {
                    ir_emit_br(&ctx->builder, merge_bb);
                }
            }

            ir_builder_set_insert_block(&ctx->builder, merge_bb);
            break;
        }

        case NODE_WHILE: {
            IrBasicBlock *cond_bb = ir_block_create(ctx->current_fn, "while.cond");
            IrBasicBlock *body_bb = ir_block_create(ctx->current_fn, "while.body");
            IrBasicBlock *end_bb = ir_block_create(ctx->current_fn, "while.end");

            ir_emit_br(&ctx->builder, cond_bb);

            /* Condition block */
            ir_builder_set_insert_block(&ctx->builder, cond_bb);
            IrValue *cond = lower_expr(ctx, stmt->as.while_stmt.cond);
            ir_emit_condbr(&ctx->builder, cond, body_bb, end_bb);

            /* Body block */
            LoopScope loop;
            loop.cond_block = cond_bb;
            loop.step_block = cond_bb;
            loop.exit_block = end_bb;
            loop.parent = ctx->current_loop;
            ctx->current_loop = &loop;

            ir_builder_set_insert_block(&ctx->builder, body_bb);
            lower_stmt(ctx, stmt->as.while_stmt.body);
            if (!is_current_block_terminated(ctx)) {
                ir_emit_br(&ctx->builder, cond_bb);
            }

            ctx->current_loop = loop.parent;
            ir_builder_set_insert_block(&ctx->builder, end_bb);
            break;
        }

        case NODE_FOR: {
            if (stmt->as.for_stmt.init) {
                lower_stmt(ctx, stmt->as.for_stmt.init);
            }

            IrBasicBlock *cond_bb = ir_block_create(ctx->current_fn, "for.cond");
            IrBasicBlock *body_bb = ir_block_create(ctx->current_fn, "for.body");
            IrBasicBlock *step_bb = ir_block_create(ctx->current_fn, "for.step");
            IrBasicBlock *end_bb = ir_block_create(ctx->current_fn, "for.end");

            ir_emit_br(&ctx->builder, cond_bb);

            /* Condition block */
            ir_builder_set_insert_block(&ctx->builder, cond_bb);
            IrValue *cond = stmt->as.for_stmt.cond ? lower_expr(ctx, stmt->as.for_stmt.cond)
                                                   : ir_val_const_bool(arena, true);
            ir_emit_condbr(&ctx->builder, cond, body_bb, end_bb);

            /* Body block */
            LoopScope loop;
            loop.cond_block = cond_bb;
            loop.step_block = step_bb;
            loop.exit_block = end_bb;
            loop.parent = ctx->current_loop;
            ctx->current_loop = &loop;

            ir_builder_set_insert_block(&ctx->builder, body_bb);
            lower_stmt(ctx, stmt->as.for_stmt.body);
            if (!is_current_block_terminated(ctx)) {
                ir_emit_br(&ctx->builder, step_bb);
            }

            /* Step block */
            ir_builder_set_insert_block(&ctx->builder, step_bb);
            if (stmt->as.for_stmt.step) {
                lower_stmt(ctx, stmt->as.for_stmt.step);
            }
            if (!is_current_block_terminated(ctx)) {
                ir_emit_br(&ctx->builder, cond_bb);
            }

            ctx->current_loop = loop.parent;
            ir_builder_set_insert_block(&ctx->builder, end_bb);
            break;
        }

        case NODE_BREAK: {
            if (ctx->current_loop) {
                emit_cleanups(ctx, stmt);
                ir_emit_br(&ctx->builder, ctx->current_loop->exit_block);
            }
            break;
        }

        case NODE_CONTINUE: {
            if (ctx->current_loop) {
                emit_cleanups(ctx, stmt);
                ir_emit_br(&ctx->builder, ctx->current_loop->step_block ? ctx->current_loop->step_block : ctx->current_loop->cond_block);
            }
            break;
        }

        case NODE_RETURN: {
            IrValue *ret_val = NULL;
            if (stmt->as.return_stmt.value) {
                ret_val = lower_expr(ctx, stmt->as.return_stmt.value);
            }
            emit_cleanups(ctx, stmt);
            if (ret_val) {
                ir_emit_ret(&ctx->builder, ret_val);
            } else {
                ir_emit_ret_void(&ctx->builder);
            }
            break;
        }

        default:
            break;
    }
}

static void lower_function(LowerCtx *ctx, AstNode *fn_node) {
    IrArena *arena = ctx->module->arena;
    const char *fn_name = fn_node->as.function.name;
    IrType *ret_type = map_ast_type(arena, fn_node->as.function.return_type,
                                    fn_node->as.function.return_is_array,
                                    fn_node->as.function.return_class_name);

    IrFunction *fn = ir_module_find_function(ctx->module, fn_name);
    if (!fn) {
        fn = ir_function_create(ctx->module, fn_name, ret_type);
        for (int p = 0; p < fn_node->as.function.param_count; p++) {
            const char *pname = fn_node->as.function.param_names[p];
            bool is_arr = fn_node->as.function.param_is_array ? fn_node->as.function.param_is_array[p] : false;
            const char *cls = fn_node->as.function.param_class_names ? fn_node->as.function.param_class_names[p] : NULL;
            IrType *ptype = map_ast_type(arena, fn_node->as.function.param_types[p], is_arr, cls);
            ir_function_add_param(fn, pname, ptype);
        }
    }
    ctx->current_fn = fn;
    ctx->current_fn_ast = fn_node;
    ctx->sym_table = NULL;
    ctx->current_loop = NULL;

    /* Create entry block */
    IrBasicBlock *entry_bb = ir_block_create(fn, "entry");
    ir_builder_set_insert_block(&ctx->builder, entry_bb);

    /* Allocate stack slots for parameters and store incoming arguments */
    for (int p = 0; p < fn->param_count; p++) {
        char slot_name[128];
        snprintf(slot_name, sizeof(slot_name), "%s.addr", fn->param_names[p]);
        IrInstruction *alloca_inst = ir_emit_alloca(&ctx->builder, fn->param_types[p], slot_name);
        ir_emit_store(&ctx->builder, fn->param_values[p], alloca_inst->result);
        sym_push(ctx, fn->param_names[p], alloca_inst->result, fn->param_types[p]);
    }

    /* Lower function body */
    if (fn_node->as.function.body) {
        lower_stmt(ctx, fn_node->as.function.body);
    }

    /* Check termination of the last block */
    if (!is_current_block_terminated(ctx)) {
        if (fn->return_type->kind == IR_TYPE_VOID) {
            ir_emit_ret_void(&ctx->builder);
        } else if (strcmp(fn_name, "main") == 0) {
            IrInstruction *c0 = ir_emit_const_int(&ctx->builder, ir_type_i32(arena), 0);
            ir_emit_ret(&ctx->builder, c0->result);
        } else {
            /* Default return 0 or null for non-void if missing return */
            IrInstruction *c0 = ir_emit_const_int(&ctx->builder, fn->return_type, 0);
            ir_emit_ret(&ctx->builder, c0->result);
        }
    }

    ir_recompute_cfg(fn);
}

IrModule *ir_lower_ast(AstNode *program, const char *module_name) {
    if (!program || program->type != NODE_PROGRAM) return NULL;

    IrModule *mod = ir_module_create(module_name ? module_name : "main");
    LowerCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.module = mod;
    ir_builder_init(&ctx.builder, mod);

    /* Lower structs and classes */
    for (int i = 0; i < program->as.program.struct_count; i++) {
        AstNode *st = program->as.program.structs[i];
        IrStructDecl *ir_st = ir_module_add_struct(mod, st->as.struct_decl.name, false);
        for (int f = 0; f < st->as.struct_decl.field_count; f++) {
            AstNode *field = st->as.struct_decl.fields[f];
            IrType *ftype = map_ast_type(mod->arena, field->as.struct_field_decl.field_type, false, NULL);
            ir_struct_add_field(mod, ir_st, field->as.struct_field_decl.name, ftype);
        }
    }

    for (int i = 0; i < program->as.program.class_count; i++) {
        AstNode *cls = program->as.program.classes[i];
        IrStructDecl *ir_cls = ir_module_add_struct(mod, cls->as.class_decl.name, true);
        for (int f = 0; f < cls->as.class_decl.field_count; f++) {
            AstNode *field = cls->as.class_decl.fields[f];
            IrType *ftype = map_ast_type(mod->arena, field->as.field.type, field->as.field.is_array, field->as.field.class_name);
            ir_struct_add_field(mod, ir_cls, field->as.field.name, ftype);
        }
    }

    /* Pre-register all function signatures */
    for (int i = 0; i < program->as.program.count; i++) {
        AstNode *fn_node = program->as.program.functions[i];
        const char *fn_name = fn_node->as.function.name;
        IrType *ret_type = map_ast_type(mod->arena, fn_node->as.function.return_type,
                                        fn_node->as.function.return_is_array,
                                        fn_node->as.function.return_class_name);
        IrFunction *fn = ir_module_find_function(mod, fn_name);
        if (!fn) {
            fn = ir_function_create(mod, fn_name, ret_type);
            for (int p = 0; p < fn_node->as.function.param_count; p++) {
                const char *pname = fn_node->as.function.param_names[p];
                bool is_arr = fn_node->as.function.param_is_array ? fn_node->as.function.param_is_array[p] : false;
                const char *cls = fn_node->as.function.param_class_names ? fn_node->as.function.param_class_names[p] : NULL;
                IrType *ptype = map_ast_type(mod->arena, fn_node->as.function.param_types[p], is_arr, cls);
                ir_function_add_param(fn, pname, ptype);
            }
        }
    }

    /* Lower functions */
    for (int i = 0; i < program->as.program.count; i++) {
        lower_function(&ctx, program->as.program.functions[i]);
    }

    return mod;
}
