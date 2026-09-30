// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static void set_error(char **out_err, const char *fn_name, const char *block_name, const char *inst_name, const char *fmt, ...) {
    if (!out_err) return;
    char detail[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);

    char full_msg[1024];
    snprintf(full_msg, sizeof(full_msg),
             "Cco IR verification failed:\n"
             "  function: %s\n"
             "  block: %s\n"
             "  instruction: %s\n"
             "  error: %s\n",
             fn_name ? fn_name : "<unknown>",
             block_name ? block_name : "<unknown>",
             inst_name ? inst_name : "<none>",
             detail);

    *out_err = strdup(full_msg);
}

static bool block_exists_in_fn(IrFunction *fn, IrBasicBlock *target) {
    if (!fn || !target) return false;
    for (IrBasicBlock *b = fn->first_block; b; b = b->next) {
        if (b == target) return true;
    }
    return false;
}

static bool is_numeric_type(IrTypeKind kind) {
    return (kind == IR_TYPE_I32 || kind == IR_TYPE_I64 || kind == IR_TYPE_F64);
}

static bool verify_instruction(IrFunction *fn, IrBasicBlock *block, IrInstruction *inst, char **out_error_msg) {
    const char *fn_name = fn->name;
    const char *bb_name = block->name;
    const char *op_name = ir_opcode_to_string(inst->op);

    switch (inst->op) {
        case IR_OP_ALLOCA:
            if (!inst->result || inst->result->type->kind != IR_TYPE_PTR) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "alloca result must be a pointer type");
                return false;
            }
            break;

        case IR_OP_LOAD:
            if (!inst->lhs) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "load requires pointer operand");
                return false;
            }
            if (inst->lhs->type->kind != IR_TYPE_PTR) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "load source is not a pointer type (got %s)",
                          ir_type_to_string(inst->lhs->type));
                return false;
            }
            if (!inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "load requires a destination result register");
                return false;
            }
            if (inst->lhs->type->elem_type && !ir_type_equals(inst->result->type, inst->lhs->type->elem_type)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "load result type (%s) does not match pointer element type (%s)",
                          ir_type_to_string(inst->result->type), ir_type_to_string(inst->lhs->type->elem_type));
                return false;
            }
            break;

        case IR_OP_STORE:
            if (!inst->lhs || !inst->rhs) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "store requires value and destination pointer operands");
                return false;
            }
            if (inst->rhs->type->kind != IR_TYPE_PTR) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "store destination is not a pointer (got %s)",
                          ir_type_to_string(inst->rhs->type));
                return false;
            }
            if (inst->rhs->type->elem_type && !ir_type_equals(inst->lhs->type, inst->rhs->type->elem_type)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "stored value type (%s) does not match destination pointer element type (%s)",
                          ir_type_to_string(inst->lhs->type), ir_type_to_string(inst->rhs->type->elem_type));
                return false;
            }
            break;

        case IR_OP_CONST:
            if (!inst->lhs || !inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "const requires literal value and result register");
                return false;
            }
            if (!ir_type_equals(inst->lhs->type, inst->result->type)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "const value type does not match result type");
                return false;
            }
            break;

        case IR_OP_PHI:
            if (!inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "phi requires result register");
                return false;
            }
            if (inst->phi_count < 1) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "phi must have at least one incoming value");
                return false;
            }
            for (int i = 0; i < inst->phi_count; i++) {
                if (!inst->phi_blocks[i]) {
                    set_error(out_error_msg, fn_name, bb_name, op_name, "phi operand %d has null block", i);
                    return false;
                }
                if (!inst->phi_values[i]) {
                    set_error(out_error_msg, fn_name, bb_name, op_name, "phi operand %d has null value", i);
                    return false;
                }
                if (!ir_type_equals(inst->result->type, inst->phi_values[i]->type)) {
                    set_error(out_error_msg, fn_name, bb_name, op_name,
                              "phi incoming value type (%s) does not match phi result type (%s)",
                              ir_type_to_string(inst->phi_values[i]->type), ir_type_to_string(inst->result->type));
                    return false;
                }
            }
            break;

        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD:
            if (!inst->lhs || !inst->rhs || !inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "binary arithmetic requires lhs, rhs, and result");
                return false;
            }
            if (!ir_type_equals(inst->lhs->type, inst->rhs->type)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "operand types do not match (%s vs %s)",
                          ir_type_to_string(inst->lhs->type), ir_type_to_string(inst->rhs->type));
                return false;
            }
            if (!is_numeric_type(inst->lhs->type->kind)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "arithmetic operands must be numeric (got %s)",
                          ir_type_to_string(inst->lhs->type));
                return false;
            }
            if (!ir_type_equals(inst->result->type, inst->lhs->type)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "result type (%s) does not match operand type (%s)",
                          ir_type_to_string(inst->result->type), ir_type_to_string(inst->lhs->type));
                return false;
            }
            break;

        case IR_OP_NEG:
            if (!inst->lhs || !inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "negation requires operand and result");
                return false;
            }
            if (!is_numeric_type(inst->lhs->type->kind)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "negation operand must be numeric");
                return false;
            }
            if (!ir_type_equals(inst->result->type, inst->lhs->type)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "result type does not match negation operand type");
                return false;
            }
            break;

        case IR_OP_EQ:
        case IR_OP_NE:
        case IR_OP_LT:
        case IR_OP_LE:
        case IR_OP_GT:
        case IR_OP_GE:
            if (!inst->lhs || !inst->rhs || !inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "comparison requires lhs, rhs, and result");
                return false;
            }
            if (!ir_type_equals(inst->lhs->type, inst->rhs->type)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "comparison operand types do not match (%s vs %s)",
                          ir_type_to_string(inst->lhs->type), ir_type_to_string(inst->rhs->type));
                return false;
            }
            if (inst->result->type->kind != IR_TYPE_BOOL) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "comparison result type must be bool");
                return false;
            }
            break;

        case IR_OP_AND:
        case IR_OP_OR:
            if (!inst->lhs || !inst->rhs || !inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "logical op requires lhs, rhs, and result");
                return false;
            }
            if (inst->lhs->type->kind != IR_TYPE_BOOL || inst->rhs->type->kind != IR_TYPE_BOOL) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "logical op operands must be bool");
                return false;
            }
            if (inst->result->type->kind != IR_TYPE_BOOL) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "logical op result must be bool");
                return false;
            }
            break;

        case IR_OP_NOT:
            if (!inst->lhs || !inst->result) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "not requires operand and result");
                return false;
            }
            if (inst->lhs->type->kind != IR_TYPE_BOOL || inst->result->type->kind != IR_TYPE_BOOL) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "not operand and result must be bool");
                return false;
            }
            break;

        case IR_OP_CALL:
            if (!inst->callee_name) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "call missing callee name");
                return false;
            }
            /* If callee exists in module, verify signature */
            if (fn->module) {
                IrFunction *target_fn = ir_module_find_function(fn->module, inst->callee_name);
                if (target_fn) {
                    if (target_fn->param_count != inst->arg_count) {
                        set_error(out_error_msg, fn_name, bb_name, op_name,
                                  "callee @%s expects %d arguments, got %d",
                                  inst->callee_name, target_fn->param_count, inst->arg_count);
                        return false;
                    }
                    for (int a = 0; a < inst->arg_count; a++) {
                        if (!ir_type_equals(inst->args[a]->type, target_fn->param_types[a])) {
                            set_error(out_error_msg, fn_name, bb_name, op_name,
                                      "argument %d type (%s) does not match parameter type (%s) in call to @%s",
                                      a, ir_type_to_string(inst->args[a]->type),
                                      ir_type_to_string(target_fn->param_types[a]), inst->callee_name);
                            return false;
                        }
                    }
                    if (!ir_type_equals(inst->type, target_fn->return_type)) {
                        set_error(out_error_msg, fn_name, bb_name, op_name,
                                  "call return type (%s) does not match function @%s return type (%s)",
                                  ir_type_to_string(inst->type), inst->callee_name,
                                  ir_type_to_string(target_fn->return_type));
                        return false;
                    }
                }
            }
            break;

        case IR_OP_BR:
            if (!inst->target_true) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "unconditional branch missing target block");
                return false;
            }
            if (!block_exists_in_fn(fn, inst->target_true)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "branch target block '%s' does not exist in function",
                          inst->target_true->name ? inst->target_true->name : "<unknown>");
                return false;
            }
            break;

        case IR_OP_CONDBR:
            if (!inst->lhs) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "conditional branch missing condition operand");
                return false;
            }
            if (inst->lhs->type->kind != IR_TYPE_BOOL) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "conditional branch condition must be bool (got %s)",
                          ir_type_to_string(inst->lhs->type));
                return false;
            }
            if (!inst->target_true || !inst->target_false) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "conditional branch missing true or false target block");
                return false;
            }
            if (!block_exists_in_fn(fn, inst->target_true) || !block_exists_in_fn(fn, inst->target_false)) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "conditional branch target block does not exist in function");
                return false;
            }
            break;

        case IR_OP_RET:
            if (fn->return_type->kind == IR_TYPE_VOID) {
                if (inst->lhs && inst->lhs->type->kind != IR_TYPE_VOID) {
                    set_error(out_error_msg, fn_name, bb_name, op_name,
                              "void function cannot return a value");
                    return false;
                }
            } else {
                if (!inst->lhs) {
                    set_error(out_error_msg, fn_name, bb_name, op_name,
                              "non-void function must return a value (expected %s)",
                              ir_type_to_string(fn->return_type));
                    return false;
                }
                if (!ir_type_equals(inst->lhs->type, fn->return_type)) {
                    set_error(out_error_msg, fn_name, bb_name, op_name,
                              "returned value type (%s) does not match function return type (%s)",
                              ir_type_to_string(inst->lhs->type), ir_type_to_string(fn->return_type));
                    return false;
                }
            }
            break;

        case IR_OP_PRINT:
            if (!inst->lhs) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "print instruction missing value operand");
                return false;
            }
            break;

        case IR_OP_FREE:
        case IR_OP_RELEASE:
            if (!inst->lhs || inst->lhs->type->kind != IR_TYPE_PTR) {
                set_error(out_error_msg, fn_name, bb_name, op_name, "deallocation requires pointer operand");
                return false;
            }
            break;

        default:
            break;
    }
    return true;
}

bool ir_verify_function(IrFunction *fn, char **out_error_msg) {
    if (!fn) {
        set_error(out_error_msg, NULL, NULL, NULL, "function pointer is NULL");
        return false;
    }

    if (!fn->first_block) {
        set_error(out_error_msg, fn->name, NULL, NULL, "function has no basic blocks");
        return false;
    }

    if (fn->entry_block != fn->first_block) {
        set_error(out_error_msg, fn->name, NULL, NULL, "entry block is not the first block");
        return false;
    }

    /* Iterate through blocks */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (!bb->first_inst || !bb->last_inst) {
            set_error(out_error_msg, fn->name, bb->name, NULL, "basic block has no instructions");
            return false;
        }

        /* Check that only the last instruction is a terminator and phis precede non-phis */
        bool seen_non_phi = false;
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->op == IR_OP_PHI) {
                if (seen_non_phi) {
                    set_error(out_error_msg, fn->name, bb->name, "phi",
                              "phi instruction appears after non-phi instruction in basic block");
                    return false;
                }
            } else {
                seen_non_phi = true;
            }

            if (ir_is_terminator(inst->op) && inst != bb->last_inst) {
                set_error(out_error_msg, fn->name, bb->name, ir_opcode_to_string(inst->op),
                          "terminator instruction must be the last instruction in basic block");
                return false;
            }

            if (!verify_instruction(fn, bb, inst, out_error_msg)) {
                return false;
            }
        }

        /* Block must be terminated */
        if (!ir_is_terminator(bb->last_inst->op)) {
            set_error(out_error_msg, fn->name, bb->name, ir_opcode_to_string(bb->last_inst->op),
                      "basic block does not end with a terminator instruction");
            return false;
        }
    }

    return true;
}

bool ir_verify_module(IrModule *module, char **out_error_msg) {
    if (!module) {
        set_error(out_error_msg, NULL, NULL, NULL, "module pointer is NULL");
        return false;
    }

    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (!ir_verify_function(fn, out_error_msg)) {
            return false;
        }
    }

    return true;
}
