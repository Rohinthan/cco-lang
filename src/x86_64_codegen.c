// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_codegen.h"
#include "ir_profile.h"
#include "x86_64_target.h"
#include "x86_64_regalloc.h"
#include "x86_64_instr.h"
#include "x86_64_encode.h"
#include "x86_64_elf.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <elf.h>

/* String & Floating-Point Constant Pools                                    */

typedef struct StrConst {
    int id;
    char *text;
    struct StrConst *next;
} StrConst;

static StrConst *g_strings = NULL;
static int g_str_count = 0;

static void clear_strings(void) {
    StrConst *curr = g_strings;
    while (curr) {
        StrConst *next = curr->next;
        free(curr->text);
        free(curr);
        curr = next;
    }
    g_strings = NULL;
    g_str_count = 0;
}

static int get_or_add_string(const char *text) {
    for (StrConst *s = g_strings; s; s = s->next) {
        if (strcmp(s->text, text) == 0) return s->id;
    }
    StrConst *ns = (StrConst *)malloc(sizeof(StrConst));
    ns->id = g_str_count++;
    ns->text = strdup(text);
    ns->next = g_strings;
    g_strings = ns;
    return ns->id;
}

typedef struct FloatConst {
    int id;
    uint64_t bits;
    double val;
    struct FloatConst *next;
} FloatConst;

static FloatConst *g_floats = NULL;
static int g_float_count = 0;

static void clear_floats(void) {
    FloatConst *curr = g_floats;
    while (curr) {
        FloatConst *next = curr->next;
        free(curr);
        curr = next;
    }
    g_floats = NULL;
    g_float_count = 0;
}

static int get_or_add_float(double val) {
    uint64_t bits = 0;
    memcpy(&bits, &val, sizeof(bits));

    for (FloatConst *f = g_floats; f; f = f->next) {
        if (f->bits == bits) return f->id;
    }
    FloatConst *nf = (FloatConst *)malloc(sizeof(FloatConst));
    nf->id = g_float_count++;
    nf->bits = bits;
    nf->val = val;
    nf->next = g_floats;
    g_floats = nf;
    return nf->id;
}

static void sanitize_label(const char *in, char *out, size_t max_len) {
    size_t idx = 0;
    while (*in && idx + 1 < max_len) {
        if (isalnum((unsigned char)*in) || *in == '_') {
            out[idx++] = *in;
        } else {
            out[idx++] = '_';
        }
        in++;
    }
    out[idx] = '\0';
}

/* System V AMD64 ABI Physical Register Tables                              */

static const X86Reg INT_ARG_REGS[6] = {
    X86_REG_RDI, X86_REG_RSI, X86_REG_RDX, X86_REG_RCX, X86_REG_R8, X86_REG_R9
};

static const X86Reg FLOAT_ARG_REGS[8] = {
    X86_REG_XMM0, X86_REG_XMM1, X86_REG_XMM2, X86_REG_XMM3,
    X86_REG_XMM4, X86_REG_XMM5, X86_REG_XMM6, X86_REG_XMM7
};

static bool is_val_64(IrValue *val) {
    if (!val || !val->type) return false;
    return (val->type->kind == IR_TYPE_PTR || val->type->kind == IR_TYPE_I64);
}

static bool is_val_float(IrValue *val) {
    if (!val || !val->type) return false;
    return (val->type->kind == IR_TYPE_F64);
}

static X86Reg get_val_phys_reg(X86FunctionFrame *frame, IrValue *val) {
    if (!frame || !frame->regalloc || !val || val->kind != IR_VAL_REG) return X86_REG_NONE;
    return regalloc_get_phys_reg(frame->regalloc, val->id);
}

/* Value Load and Store Lowering                                             */

static void emit_load_val(X86InstrList *list, X86FunctionFrame *frame, IrValue *val, X86Reg target_reg) {
    if (!val) {
        x86_emit_mov_imm_reg(list, 4, 0, target_reg);
        return;
    }

    if (is_val_float(val)) {
        switch (val->kind) {
            case IR_VAL_CONST_FLOAT: {
                int fid = get_or_add_float(val->const_val.float_val);
                char sym[64];
                snprintf(sym, sizeof(sym), ".LC_float_%d", fid);
                x86_emit_movsd_rip_reg(list, sym, target_reg);
                break;
            }
            case IR_VAL_REG: {
                X86Reg src_reg = get_val_phys_reg(frame, val);
                if (src_reg != X86_REG_NONE) {
                    if (src_reg != target_reg) {
                        x86_emit_movsd_reg_reg(list, src_reg, target_reg);
                    }
                } else {
                    int offset = 0, slot_size = 8;
                    if (x86_frame_get_offset(frame, val, &offset, &slot_size)) {
                        x86_emit_movsd_mem_reg(list, X86_REG_RBP, offset, target_reg);
                    } else {
                        x86_emit_xorpd(list, target_reg, target_reg);
                    }
                }
                break;
            }
            case IR_VAL_PARAM:
            case IR_VAL_VAR: {
                int offset = 0, slot_size = 8;
                if (x86_frame_get_offset(frame, val, &offset, &slot_size)) {
                    x86_emit_movsd_mem_reg(list, X86_REG_RBP, offset, target_reg);
                } else {
                    x86_emit_xorpd(list, target_reg, target_reg);
                }
                break;
            }
            default:
                x86_emit_xorpd(list, target_reg, target_reg);
                break;
        }
        return;
    }

    bool is_64 = is_val_64(val);
    int size = is_64 ? 8 : 4;

    switch (val->kind) {
        case IR_VAL_CONST_INT:
            x86_emit_mov_imm_reg(list, size, (long)val->const_val.int_val, target_reg);
            break;
        case IR_VAL_CONST_BOOL:
            x86_emit_mov_imm_reg(list, 4, val->const_val.bool_val ? 1 : 0, target_reg);
            break;
        case IR_VAL_CONST_CHAR:
            x86_emit_mov_imm_reg(list, 4, (int)val->const_val.char_val, target_reg);
            break;
        case IR_VAL_CONST_STRING: {
            int sid = get_or_add_string(val->const_val.str_val ? val->const_val.str_val : "");
            char sym[64];
            snprintf(sym, sizeof(sym), ".LC_str_%d", sid);
            x86_emit_lea_rip(list, sym, target_reg);
            break;
        }
        case IR_VAL_VAR: {
            int offset = 0, slot_size = 8;
            if (x86_frame_get_offset(frame, val, &offset, &slot_size)) {
                x86_emit_lea_mem(list, X86_REG_RBP, offset, target_reg);
            } else {
                x86_emit_mov_imm_reg(list, 8, 0, target_reg);
            }
            break;
        }
        case IR_VAL_REG: {
            X86Reg src_reg = get_val_phys_reg(frame, val);
            if (src_reg != X86_REG_NONE) {
                if (src_reg != target_reg) {
                    x86_emit_mov_reg_reg(list, size, src_reg, target_reg);
                }
            } else {
                int offset = 0, slot_size = 8;
                if (x86_frame_get_offset(frame, val, &offset, &slot_size)) {
                    x86_emit_mov_mem_reg(list, size, X86_REG_RBP, offset, target_reg);
                } else {
                    x86_emit_mov_imm_reg(list, size, 0, target_reg);
                }
            }
            break;
        }
        case IR_VAL_PARAM: {
            int offset = 0, slot_size = 8;
            if (x86_frame_get_offset(frame, val, &offset, &slot_size)) {
                x86_emit_mov_mem_reg(list, size, X86_REG_RBP, offset, target_reg);
            } else {
                x86_emit_mov_imm_reg(list, size, 0, target_reg);
            }
            break;
        }
        case IR_VAL_GLOBAL:
            x86_emit_mov_rip_reg(list, val->name ? val->name : "global", target_reg);
            break;
        default:
            x86_emit_mov_imm_reg(list, size, 0, target_reg);
            break;
    }
}

static void emit_store_result(X86InstrList *list, X86FunctionFrame *frame, IrValue *dst, X86Reg src_reg) {
    if (!dst) return;

    if (is_val_float(dst)) {
        if (dst->kind == IR_VAL_REG) {
            X86Reg dst_reg = get_val_phys_reg(frame, dst);
            if (dst_reg != X86_REG_NONE) {
                if (src_reg != dst_reg) {
                    x86_emit_movsd_reg_reg(list, src_reg, dst_reg);
                }
                if (frame->regalloc && dst->id < frame->regalloc->total_regs) {
                    LiveInterval *inv = &frame->regalloc->intervals[dst->id];
                    if (inv->spill_offset != 0) {
                        x86_emit_movsd_reg_mem(list, dst_reg, X86_REG_RBP, inv->spill_offset);
                    }
                }
            } else {
                int offset = 0, slot_size = 8;
                if (x86_frame_get_offset(frame, dst, &offset, &slot_size)) {
                    x86_emit_movsd_reg_mem(list, src_reg, X86_REG_RBP, offset);
                }
            }
        } else if (dst->kind == IR_VAL_VAR) {
            int offset = 0, slot_size = 8;
            if (x86_frame_get_offset(frame, dst, &offset, &slot_size)) {
                x86_emit_movsd_reg_mem(list, src_reg, X86_REG_RBP, offset);
            }
        }
        return;
    }

    bool is_64 = is_val_64(dst);
    int size = is_64 ? 8 : 4;

    if (dst->kind == IR_VAL_REG) {
        X86Reg dst_reg = get_val_phys_reg(frame, dst);
        if (dst_reg != X86_REG_NONE) {
            if (src_reg != dst_reg) {
                x86_emit_mov_reg_reg(list, size, src_reg, dst_reg);
            }
            if (frame->regalloc && dst->id < frame->regalloc->total_regs) {
                LiveInterval *inv = &frame->regalloc->intervals[dst->id];
                if (inv->spill_offset != 0) {
                    x86_emit_mov_reg_mem(list, size, dst_reg, X86_REG_RBP, inv->spill_offset);
                }
            }
        } else {
            int offset = 0, slot_size = 8;
            if (x86_frame_get_offset(frame, dst, &offset, &slot_size)) {
                x86_emit_mov_reg_mem(list, size, src_reg, X86_REG_RBP, offset);
            }
        }
    } else if (dst->kind == IR_VAL_VAR) {
        int offset = 0, slot_size = 8;
        if (x86_frame_get_offset(frame, dst, &offset, &slot_size)) {
            x86_emit_mov_reg_mem(list, size, src_reg, X86_REG_RBP, offset);
        }
    }
}

static void reload_caller_saved_after_call(X86InstrList *list, X86FunctionFrame *frame, int call_inst_idx) {
    if (!frame || !frame->regalloc) return;
    for (int v = 0; v < frame->regalloc->total_regs; v++) {
        LiveInterval *inv = &frame->regalloc->intervals[v];
        if (inv->phys_reg != X86_REG_NONE && x86_reg_is_caller_saved(inv->phys_reg)) {
            if (inv->start < call_inst_idx && inv->end > call_inst_idx && inv->spill_offset != 0) {
                if (inv->reg_class == REG_CLASS_FLOAT) {
                    x86_emit_movsd_mem_reg(list, X86_REG_RBP, inv->spill_offset, inv->phys_reg);
                } else {
                    bool is_64 = (inv->type && (inv->type->kind == IR_TYPE_PTR || inv->type->kind == IR_TYPE_I64));
                    x86_emit_mov_mem_reg(list, is_64 ? 8 : 4, X86_REG_RBP, inv->spill_offset, inv->phys_reg);
                }
            }
        }
    }
}

/* Instruction Selection Lowering                                            */

static bool gen_instruction(X86InstrList *list, X86FunctionFrame *frame, IrInstruction *inst, int inst_idx, char **out_error) {
    switch (inst->op) {
        case IR_OP_ALLOCA: {
            if (inst->result && inst->result->kind == IR_VAL_REG) {
                int offset = 0, size = 8;
                if (x86_frame_get_offset(frame, inst->result, &offset, &size)) {
                    x86_emit_lea_mem(list, X86_REG_RBP, offset, X86_REG_RAX);
                    emit_store_result(list, frame, inst->result, X86_REG_RAX);
                }
            }
            break;
        }

        case IR_OP_LOAD: {
            int lhs_offset = 0, lhs_size = 8;
            if (inst->type && inst->type->kind == IR_TYPE_F64) {
                X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
                X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_XMM0;
                if (inst->lhs->kind == IR_VAL_VAR && x86_frame_get_offset(frame, inst->lhs, &lhs_offset, &lhs_size)) {
                    x86_emit_movsd_mem_reg(list, X86_REG_RBP, lhs_offset, target);
                } else {
                    emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                    x86_emit_movsd_mem_reg(list, X86_REG_RAX, 0, target);
                }
                if (dst_reg == X86_REG_NONE) {
                    emit_store_result(list, frame, inst->result, target);
                } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                    x86_emit_movsd_reg_mem(list, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
                }
                break;
            }

            bool is_64 = (inst->type && (inst->type->kind == IR_TYPE_PTR || inst->type->kind == IR_TYPE_I64));
            int sz = is_64 ? 8 : 4;
            X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
            X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_RAX;

            if (inst->lhs->kind == IR_VAL_VAR && x86_frame_get_offset(frame, inst->lhs, &lhs_offset, &lhs_size)) {
                x86_emit_mov_mem_reg(list, sz, X86_REG_RBP, lhs_offset, target);
                if (dst_reg == X86_REG_NONE) {
                    emit_store_result(list, frame, inst->result, target);
                } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                    x86_emit_mov_reg_mem(list, sz, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
                }
            } else {
                emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                x86_emit_mov_mem_reg(list, sz, X86_REG_RAX, 0, target);
                if (dst_reg == X86_REG_NONE) {
                    emit_store_result(list, frame, inst->result, target);
                } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                    x86_emit_mov_reg_mem(list, sz, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
                }
            }
            break;
        }

        case IR_OP_STORE: {
            int rhs_offset = 0, rhs_size = 8;
            if (is_val_float(inst->lhs)) {
                X86Reg src_reg = get_val_phys_reg(frame, inst->lhs);
                if (src_reg == X86_REG_NONE) {
                    emit_load_val(list, frame, inst->lhs, X86_REG_XMM15);
                    src_reg = X86_REG_XMM15;
                }
                if (inst->rhs->kind == IR_VAL_VAR && x86_frame_get_offset(frame, inst->rhs, &rhs_offset, &rhs_size)) {
                    x86_emit_movsd_reg_mem(list, src_reg, X86_REG_RBP, rhs_offset);
                } else {
                    emit_load_val(list, frame, inst->rhs, X86_REG_RAX);
                    x86_emit_movsd_reg_mem(list, src_reg, X86_REG_RAX, 0);
                }
                break;
            }

            bool is_64 = is_val_64(inst->lhs);
            int sz = is_64 ? 8 : 4;
            X86Reg src_reg = get_val_phys_reg(frame, inst->lhs);

            if (inst->rhs->kind == IR_VAL_VAR && x86_frame_get_offset(frame, inst->rhs, &rhs_offset, &rhs_size)) {
                if (src_reg != X86_REG_NONE) {
                    x86_emit_mov_reg_mem(list, sz, src_reg, X86_REG_RBP, rhs_offset);
                } else {
                    emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                    x86_emit_mov_reg_mem(list, sz, X86_REG_RAX, X86_REG_RBP, rhs_offset);
                }
            } else {
                emit_load_val(list, frame, inst->rhs, X86_REG_RAX);
                if (src_reg != X86_REG_NONE) {
                    x86_emit_mov_reg_mem(list, sz, src_reg, X86_REG_RAX, 0);
                } else {
                    emit_load_val(list, frame, inst->lhs, X86_REG_RDX);
                    x86_emit_mov_reg_mem(list, sz, X86_REG_RDX, X86_REG_RAX, 0);
                }
            }
            break;
        }

        case IR_OP_CONST: {
            if (inst->type && inst->type->kind == IR_TYPE_F64) {
                X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
                X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_XMM0;
                emit_load_val(list, frame, inst->lhs, target);
                if (dst_reg == X86_REG_NONE) {
                    emit_store_result(list, frame, inst->result, target);
                } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                    x86_emit_movsd_reg_mem(list, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
                }
                break;
            }

            bool is_64 = (inst->type && (inst->type->kind == IR_TYPE_I64 || inst->type->kind == IR_TYPE_PTR));
            int sz = is_64 ? 8 : 4;
            X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
            X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_RAX;

            if (inst->lhs->kind == IR_VAL_CONST_STRING) {
                int sid = get_or_add_string(inst->lhs->const_val.str_val ? inst->lhs->const_val.str_val : "");
                char sym[64];
                snprintf(sym, sizeof(sym), ".LC_str_%d", sid);
                x86_emit_lea_rip(list, sym, target);
            } else {
                emit_load_val(list, frame, inst->lhs, target);
            }
            if (dst_reg == X86_REG_NONE) {
                emit_store_result(list, frame, inst->result, target);
            } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                x86_emit_mov_reg_mem(list, sz, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
            }
            break;
        }

        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV: {
            if (inst->type && inst->type->kind == IR_TYPE_F64) {
                X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
                X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_XMM0;

                X86Reg rhs_reg = get_val_phys_reg(frame, inst->rhs);
                if (rhs_reg == X86_REG_NONE || rhs_reg == target) {
                    emit_load_val(list, frame, inst->rhs, X86_REG_XMM15);
                    rhs_reg = X86_REG_XMM15;
                }

                emit_load_val(list, frame, inst->lhs, target);

                if (inst->op == IR_OP_ADD) x86_emit_addsd(list, rhs_reg, target);
                else if (inst->op == IR_OP_SUB) x86_emit_subsd(list, rhs_reg, target);
                else if (inst->op == IR_OP_MUL) x86_emit_mulsd(list, rhs_reg, target);
                else if (inst->op == IR_OP_DIV) x86_emit_divsd(list, rhs_reg, target);

                if (dst_reg == X86_REG_NONE) {
                    emit_store_result(list, frame, inst->result, target);
                } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                    x86_emit_movsd_reg_mem(list, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
                }
                break;
            }

            if (inst->op == IR_OP_DIV) {
                emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                x86_emit_cltd(list);
                emit_load_val(list, frame, inst->rhs, X86_REG_RCX);
                x86_emit_idiv(list, X86_REG_RCX);
                emit_store_result(list, frame, inst->result, X86_REG_RAX);
                break;
            }

            X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
            X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_RAX;

            X86Reg rhs_reg = get_val_phys_reg(frame, inst->rhs);
            if (rhs_reg == X86_REG_NONE || rhs_reg == target) {
                emit_load_val(list, frame, inst->rhs, X86_REG_RDX);
                rhs_reg = X86_REG_RDX;
            }

            emit_load_val(list, frame, inst->lhs, target);

            if (inst->op == IR_OP_ADD) x86_emit_add(list, rhs_reg, target);
            else if (inst->op == IR_OP_SUB) x86_emit_sub(list, rhs_reg, target);
            else if (inst->op == IR_OP_MUL) x86_emit_imul(list, rhs_reg, target);

            if (dst_reg == X86_REG_NONE) {
                emit_store_result(list, frame, inst->result, target);
            } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                x86_emit_mov_reg_mem(list, 4, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
            }
            break;
        }

        case IR_OP_MOD: {
            emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
            x86_emit_cltd(list);
            emit_load_val(list, frame, inst->rhs, X86_REG_RCX);
            x86_emit_idiv(list, X86_REG_RCX);
            emit_store_result(list, frame, inst->result, X86_REG_RDX);
            break;
        }

        case IR_OP_AND:
        case IR_OP_OR: {
            X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
            X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_RAX;
            X86Reg rhs_reg = get_val_phys_reg(frame, inst->rhs);
            if (rhs_reg == X86_REG_NONE || rhs_reg == target) {
                emit_load_val(list, frame, inst->rhs, X86_REG_RDX);
                rhs_reg = X86_REG_RDX;
            }
            emit_load_val(list, frame, inst->lhs, target);
            if (inst->op == IR_OP_AND) x86_emit_and(list, rhs_reg, target);
            else x86_emit_or(list, rhs_reg, target);

            if (dst_reg == X86_REG_NONE) {
                emit_store_result(list, frame, inst->result, target);
            } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                x86_emit_mov_reg_mem(list, 4, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
            }
            break;
        }

        case IR_OP_NEG: {
            if (inst->type && inst->type->kind == IR_TYPE_F64) {
                X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
                X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_XMM0;
                emit_load_val(list, frame, inst->lhs, target);
                x86_emit_movsd_rip_reg(list, ".LC_neg_float_mask", X86_REG_XMM15);
                x86_emit_xorpd(list, X86_REG_XMM15, target);
                if (dst_reg == X86_REG_NONE) {
                    emit_store_result(list, frame, inst->result, target);
                } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                    x86_emit_movsd_reg_mem(list, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
                }
                break;
            }

            X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
            X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_RAX;
            emit_load_val(list, frame, inst->lhs, target);
            x86_emit_neg(list, target);
            if (dst_reg == X86_REG_NONE) {
                emit_store_result(list, frame, inst->result, target);
            } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                x86_emit_mov_reg_mem(list, 4, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
            }
            break;
        }

        case IR_OP_EQ:
        case IR_OP_NE:
        case IR_OP_LT:
        case IR_OP_LE:
        case IR_OP_GT:
        case IR_OP_GE: {
            X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
            X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_RAX;

            if (is_val_float(inst->lhs)) {
                emit_load_val(list, frame, inst->lhs, X86_REG_XMM14);
                emit_load_val(list, frame, inst->rhs, X86_REG_XMM15);
                x86_emit_ucomisd(list, X86_REG_XMM15, X86_REG_XMM14);

                switch (inst->op) {
                    case IR_OP_EQ:
                        x86_emit_setcc(list, X86_CC_NP, X86_REG_RAX);
                        x86_emit_setcc(list, X86_CC_E, X86_REG_RDX);
                        x86_emit_andb(list, X86_REG_RDX, X86_REG_RAX);
                        x86_emit_movzbl(list, X86_REG_RAX, target);
                        break;
                    case IR_OP_NE:
                        x86_emit_setcc(list, X86_CC_P, X86_REG_RAX);
                        x86_emit_setcc(list, X86_CC_NE, X86_REG_RDX);
                        x86_emit_orb(list, X86_REG_RDX, X86_REG_RAX);
                        x86_emit_movzbl(list, X86_REG_RAX, target);
                        break;
                    case IR_OP_LT:
                        x86_emit_setcc(list, X86_CC_B, X86_REG_RAX);
                        x86_emit_setcc(list, X86_CC_NP, X86_REG_RDX);
                        x86_emit_andb(list, X86_REG_RDX, X86_REG_RAX);
                        x86_emit_movzbl(list, X86_REG_RAX, target);
                        break;
                    case IR_OP_LE:
                        x86_emit_setcc(list, X86_CC_BE, X86_REG_RAX);
                        x86_emit_setcc(list, X86_CC_NP, X86_REG_RDX);
                        x86_emit_andb(list, X86_REG_RDX, X86_REG_RAX);
                        x86_emit_movzbl(list, X86_REG_RAX, target);
                        break;
                    case IR_OP_GT:
                        x86_emit_setcc(list, X86_CC_A, X86_REG_RAX);
                        x86_emit_movzbl(list, X86_REG_RAX, target);
                        break;
                    case IR_OP_GE:
                        x86_emit_setcc(list, X86_CC_AE, X86_REG_RAX);
                        x86_emit_setcc(list, X86_CC_NP, X86_REG_RDX);
                        x86_emit_andb(list, X86_REG_RDX, X86_REG_RAX);
                        x86_emit_movzbl(list, X86_REG_RAX, target);
                        break;
                    default:
                        break;
                }

                if (dst_reg == X86_REG_NONE) {
                    emit_store_result(list, frame, inst->result, target);
                } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                    x86_emit_mov_reg_mem(list, 4, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
                }
                break;
            }

            emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
            emit_load_val(list, frame, inst->rhs, X86_REG_RDX);
            x86_emit_cmp(list, X86_REG_RDX, X86_REG_RAX);

            X86CondCode cc = X86_CC_E;
            if (inst->op == IR_OP_NE) cc = X86_CC_NE;
            else if (inst->op == IR_OP_LT) cc = X86_CC_L;
            else if (inst->op == IR_OP_LE) cc = X86_CC_LE;
            else if (inst->op == IR_OP_GT) cc = X86_CC_G;
            else if (inst->op == IR_OP_GE) cc = X86_CC_GE;

            x86_emit_setcc(list, cc, X86_REG_RAX);
            x86_emit_movzbl(list, X86_REG_RAX, target);

            if (dst_reg == X86_REG_NONE) {
                emit_store_result(list, frame, inst->result, target);
            } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                x86_emit_mov_reg_mem(list, 4, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
            }
            break;
        }

        case IR_OP_NOT: {
            X86Reg dst_reg = get_val_phys_reg(frame, inst->result);
            X86Reg target = (dst_reg != X86_REG_NONE) ? dst_reg : X86_REG_RAX;
            emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
            x86_emit_cmp_imm(list, 0, X86_REG_RAX);
            x86_emit_setcc(list, X86_CC_E, X86_REG_RAX);
            x86_emit_movzbl(list, X86_REG_RAX, target);
            if (dst_reg == X86_REG_NONE) {
                emit_store_result(list, frame, inst->result, target);
            } else if (frame->regalloc && inst->result->id < frame->regalloc->total_regs && frame->regalloc->intervals[inst->result->id].spill_offset != 0) {
                x86_emit_mov_reg_mem(list, 4, dst_reg, X86_REG_RBP, frame->regalloc->intervals[inst->result->id].spill_offset);
            }
            break;
        }

        case IR_OP_BR: {
            char tgt[128], full_lbl[256];
            sanitize_label(inst->target_true ? inst->target_true->name : "block", tgt, sizeof(tgt));
            snprintf(full_lbl, sizeof(full_lbl), ".L_%s_%s", frame->fn->name, tgt);
            x86_emit_jmp(list, full_lbl);
            break;
        }

        case IR_OP_CONDBR: {
            emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
            x86_emit_cmp_imm(list, 0, X86_REG_RAX);
            char true_tgt[128], false_tgt[128], full_true[256], full_false[256];
            sanitize_label(inst->target_true ? inst->target_true->name : "true_bb", true_tgt, sizeof(true_tgt));
            sanitize_label(inst->target_false ? inst->target_false->name : "false_bb", false_tgt, sizeof(false_tgt));
            snprintf(full_true, sizeof(full_true), ".L_%s_%s", frame->fn->name, true_tgt);
            snprintf(full_false, sizeof(full_false), ".L_%s_%s", frame->fn->name, false_tgt);
            x86_emit_jne(list, full_true);
            x86_emit_jmp(list, full_false);
            break;
        }

        case IR_OP_RET: {
            if (inst->lhs) {
                if (is_val_float(inst->lhs)) {
                    emit_load_val(list, frame, inst->lhs, X86_REG_XMM0);
                } else if (is_val_64(inst->lhs)) {
                    emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                } else if (inst->lhs->type && (inst->lhs->type->kind == IR_TYPE_BOOL || inst->lhs->type->kind == IR_TYPE_CHAR)) {
                    emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                    x86_emit_movzbl(list, X86_REG_RAX, X86_REG_RAX);
                } else {
                    emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                }
            } else {
                x86_emit_mov_imm_reg(list, 4, 0, X86_REG_RAX);
            }
            char epilogue_lbl[256];
            snprintf(epilogue_lbl, sizeof(epilogue_lbl), ".L_%s_epilogue", frame->fn->name);
            x86_emit_jmp(list, epilogue_lbl);
            break;
        }

        case IR_OP_CALL: {
            int reg_int_count = 0;
            int reg_float_count = 0;
            int stack_arg_count = 0;

            for (int a = 0; a < inst->arg_count; a++) {
                IrValue *arg = inst->args[a];
                if (is_val_float(arg)) {
                    if (reg_float_count < X86_64_MAX_FLOAT_ARG_REGS) reg_float_count++;
                    else stack_arg_count++;
                } else {
                    if (reg_int_count < X86_64_MAX_ARG_REGS) reg_int_count++;
                    else stack_arg_count++;
                }
            }

            int outgoing_stack_bytes = (stack_arg_count > 0) ? ((stack_arg_count * 8 + 15) & ~15) : 0;
            int cur_reg_int = 0;
            int cur_reg_flt = 0;
            int cur_stack = 0;

            if (inst->arg_count > 0) {
                int total_push = 128 + outgoing_stack_bytes;
                x86_emit_sub_imm_rsp(list, total_push);

                for (int a = 0; a < inst->arg_count; a++) {
                    IrValue *arg = inst->args[a];
                    if (is_val_float(arg)) {
                        if (cur_reg_flt < X86_64_MAX_FLOAT_ARG_REGS) {
                            emit_load_val(list, frame, arg, X86_REG_XMM15);
                            x86_emit_movsd_reg_mem(list, X86_REG_XMM15, X86_REG_RSP, cur_reg_flt * 8);
                            cur_reg_flt++;
                        } else {
                            emit_load_val(list, frame, arg, X86_REG_XMM15);
                            x86_emit_movsd_reg_mem(list, X86_REG_XMM15, X86_REG_RSP, 128 + cur_stack * 8);
                            cur_stack++;
                        }
                    } else {
                        if (cur_reg_int < X86_64_MAX_ARG_REGS) {
                            if (is_val_64(arg)) {
                                emit_load_val(list, frame, arg, X86_REG_RAX);
                                x86_emit_mov_reg_mem(list, 8, X86_REG_RAX, X86_REG_RSP, 64 + cur_reg_int * 8);
                            } else {
                                emit_load_val(list, frame, arg, X86_REG_RAX);
                                x86_emit_movslq(list, X86_REG_RAX, X86_REG_RAX);
                                x86_emit_mov_reg_mem(list, 8, X86_REG_RAX, X86_REG_RSP, 64 + cur_reg_int * 8);
                            }
                            cur_reg_int++;
                        } else {
                            if (is_val_64(arg)) {
                                emit_load_val(list, frame, arg, X86_REG_RAX);
                                x86_emit_mov_reg_mem(list, 8, X86_REG_RAX, X86_REG_RSP, 128 + cur_stack * 8);
                            } else {
                                emit_load_val(list, frame, arg, X86_REG_RAX);
                                x86_emit_movslq(list, X86_REG_RAX, X86_REG_RAX);
                                x86_emit_mov_reg_mem(list, 8, X86_REG_RAX, X86_REG_RSP, 128 + cur_stack * 8);
                            }
                            cur_stack++;
                        }
                    }
                }

                for (int i = 0; i < cur_reg_flt; i++) {
                    x86_emit_movsd_mem_reg(list, X86_REG_RSP, i * 8, FLOAT_ARG_REGS[i]);
                }
                for (int i = 0; i < cur_reg_int; i++) {
                    x86_emit_mov_mem_reg(list, 8, X86_REG_RSP, 64 + i * 8, INT_ARG_REGS[i]);
                }
                x86_emit_add_imm_rsp(list, 128);
                x86_emit_movb_imm(list, (uint8_t)cur_reg_flt, X86_REG_RAX);
            } else {
                x86_emit_xor(list, X86_REG_RAX, X86_REG_RAX);
            }

            x86_emit_call(list, inst->callee_name ? inst->callee_name : "fn");

            if (inst->result) {
                if (is_val_float(inst->result)) {
                    emit_store_result(list, frame, inst->result, X86_REG_XMM0);
                } else if (is_val_64(inst->result)) {
                    emit_store_result(list, frame, inst->result, X86_REG_RAX);
                } else {
                    emit_store_result(list, frame, inst->result, X86_REG_RAX);
                }
            }

            if (outgoing_stack_bytes > 0) {
                x86_emit_add_imm_rsp(list, outgoing_stack_bytes);
            }

            reload_caller_saved_after_call(list, frame, inst_idx);
            break;
        }

        case IR_OP_PRINT: {
            if (is_val_float(inst->lhs)) {
                emit_load_val(list, frame, inst->lhs, X86_REG_XMM0);
                x86_emit_lea_rip(list, ".LC_fmt_float", X86_REG_RDI);
                x86_emit_movb_imm(list, 1, X86_REG_RAX);
                x86_emit_call(list, "printf");
            } else if (inst->lhs->type && inst->lhs->type->kind == IR_TYPE_BOOL) {
                emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                x86_emit_cmp_imm(list, 0, X86_REG_RAX);
                x86_emit_lea_rip(list, ".LC_str_true", X86_REG_RAX);
                x86_emit_lea_rip(list, ".LC_str_false", X86_REG_RDX);
                x86_emit_cmove(list, X86_REG_RDX, X86_REG_RAX);
                x86_emit_mov_reg_reg(list, 8, X86_REG_RAX, X86_REG_RSI);
                x86_emit_lea_rip(list, ".LC_fmt_str", X86_REG_RDI);
                x86_emit_xor(list, X86_REG_RAX, X86_REG_RAX);
                x86_emit_call(list, "printf");
            } else if (inst->lhs->type && (inst->lhs->type->kind == IR_TYPE_PTR && inst->lhs->type->elem_type && inst->lhs->type->elem_type->kind == IR_TYPE_CHAR)) {
                emit_load_val(list, frame, inst->lhs, X86_REG_RSI);
                x86_emit_lea_rip(list, ".LC_fmt_str", X86_REG_RDI);
                x86_emit_xor(list, X86_REG_RAX, X86_REG_RAX);
                x86_emit_call(list, "printf");
            } else {
                emit_load_val(list, frame, inst->lhs, X86_REG_RAX);
                x86_emit_movslq(list, X86_REG_RAX, X86_REG_RSI);
                x86_emit_lea_rip(list, ".LC_fmt_int", X86_REG_RDI);
                x86_emit_xor(list, X86_REG_RAX, X86_REG_RAX);
                x86_emit_call(list, "printf");
            }
            reload_caller_saved_after_call(list, frame, inst_idx);
            break;
        }

        case IR_OP_FREE:
        case IR_OP_RELEASE: {
            emit_load_val(list, frame, inst->lhs, X86_REG_RDI);
            x86_emit_call(list, "free");
            reload_caller_saved_after_call(list, frame, inst_idx);
            break;
        }

        default:
            if (out_error) {
                char err_buf[256];
                snprintf(err_buf, sizeof(err_buf), "x86-64 backend error: unsupported IR instruction '%s' in function '%s'",
                         ir_opcode_to_string(inst->op), frame->fn->name);
                *out_error = strdup(err_buf);
            }
            return false;
    }
    return true;
}

/* Function Lowering Pipeline & Profile-Guided Block Layout                  */

void ir_cfg_reorder_blocks_for_layout(IrFunction *fn, const CcoProfile *prof) {
    if (!fn || !fn->first_block || !prof) return;

    /* Count basic blocks */
    int count = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) count++;
    if (count <= 2) return;

    IrBasicBlock **orig = (IrBasicBlock **)malloc(count * sizeof(IrBasicBlock *));
    bool *placed = (bool *)calloc(count, sizeof(bool));
    IrBasicBlock **layout = (IrBasicBlock **)malloc(count * sizeof(IrBasicBlock *));

    int idx = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        orig[idx++] = bb;
    }

    int layout_count = 0;
    /* Always begin layout with the entry block (orig[0]) */
    layout[layout_count++] = orig[0];
    placed[0] = true;

    IrBasicBlock *curr = orig[0];
    while (layout_count < count) {
        IrBasicBlock *next_bb = NULL;

        /* Look at terminator of curr */
        if (curr && curr->last_inst) {
            if (curr->last_inst->op == IR_OP_CONDBR) {
                IrBasicBlock *t = curr->last_inst->target_true;
                IrBasicBlock *f = curr->last_inst->target_false;

                uint64_t t_cnt = 0, f_cnt = 0;
                cco_profile_get_branch_counts(prof, fn->name, curr->name, &t_cnt, &f_cnt);

                IrBasicBlock *hot_target = (t_cnt >= f_cnt) ? t : f;
                IrBasicBlock *cold_target = (t_cnt >= f_cnt) ? f : t;

                /* Prefer placing the hot target next if not yet placed */
                int hot_idx = -1, cold_idx = -1;
                for (int i = 0; i < count; i++) {
                    if (orig[i] == hot_target) hot_idx = i;
                    if (orig[i] == cold_target) cold_idx = i;
                }

                if (hot_idx >= 0 && !placed[hot_idx]) {
                    next_bb = hot_target;
                    placed[hot_idx] = true;
                } else if (cold_idx >= 0 && !placed[cold_idx]) {
                    next_bb = cold_target;
                    placed[cold_idx] = true;
                }
            } else if (curr->last_inst->op == IR_OP_BR) {
                IrBasicBlock *tgt = curr->last_inst->target_true;
                int tgt_idx = -1;
                for (int i = 0; i < count; i++) {
                    if (orig[i] == tgt) { tgt_idx = i; break; }
                }
                if (tgt_idx >= 0 && !placed[tgt_idx]) {
                    next_bb = tgt;
                    placed[tgt_idx] = true;
                }
            }
        }

        if (!next_bb) {
            /* Pick first unplaced block in original order */
            for (int i = 0; i < count; i++) {
                if (!placed[i]) {
                    next_bb = orig[i];
                    placed[i] = true;
                    break;
                }
            }
        }

        if (next_bb) {
            layout[layout_count++] = next_bb;
            curr = next_bb;
        } else {
            break;
        }
    }

    /* Re-link fn->first_block in layout order */
    fn->first_block = layout[0];
    for (int i = 0; i < layout_count - 1; i++) {
        layout[i]->next = layout[i + 1];
    }
    layout[layout_count - 1]->next = NULL;
    fn->last_block = layout[layout_count - 1];

    free(orig);
    free(placed);
    free(layout);
}

bool gen_function_instructions_timed(IrFunction *fn, X86InstrList **out_list, CcoPassTimings *timings, char **out_error) {
    /* If profile data is available, reorder blocks to maximize hot fall-through */
    if (fn->module && fn->module->profile) {
        double t_pgo = timings ? cco_get_time_ms() : 0.0;
        ir_cfg_reorder_blocks_for_layout(fn, (const CcoProfile *)fn->module->profile);
        if (timings) timings->pgo_ms += (cco_get_time_ms() - t_pgo);
    }

    double t_ra = timings ? cco_get_time_ms() : 0.0;
    RegAllocResult *regalloc = regalloc_run(fn);
    if (timings) timings->regalloc_ms += (cco_get_time_ms() - t_ra);

    double t_cg = timings ? cco_get_time_ms() : 0.0;
    X86FunctionFrame *frame = x86_frame_build(fn, regalloc);
    X86InstrList *list = x86_instr_list_create();

    /* Function Entry Label */
    x86_emit_label(list, fn->name);

    /* Prologue */
    x86_emit_push(list, X86_REG_RBP);
    x86_emit_mov_reg_reg(list, 8, X86_REG_RSP, X86_REG_RBP);

    for (int i = 0; i < frame->callee_saved_count; i++) {
        x86_emit_push(list, frame->callee_saved_regs[i]);
    }

    if (frame->total_stack_size > 0) {
        x86_emit_sub_imm_rsp(list, frame->total_stack_size);
    }

    /* Spill incoming argument registers into parameter home slots */
    int int_arg_idx = 0;
    int float_arg_idx = 0;

    for (int p = 0; p < fn->param_count; p++) {
        int offset = 0, size = 4;
        if (x86_frame_get_offset(frame, fn->param_values[p], &offset, &size)) {
            if (fn->param_types[p] && fn->param_types[p]->kind == IR_TYPE_F64) {
                if (float_arg_idx < X86_64_MAX_FLOAT_ARG_REGS) {
                    x86_emit_movsd_reg_mem(list, FLOAT_ARG_REGS[float_arg_idx++], X86_REG_RBP, offset);
                }
            } else {
                if (int_arg_idx < X86_64_MAX_ARG_REGS) {
                    if (size == 8 || (fn->param_types[p] && (fn->param_types[p]->kind == IR_TYPE_PTR || fn->param_types[p]->kind == IR_TYPE_I64))) {
                        x86_emit_mov_reg_mem(list, 8, INT_ARG_REGS[int_arg_idx++], X86_REG_RBP, offset);
                    } else {
                        x86_emit_mov_reg_mem(list, 4, INT_ARG_REGS[int_arg_idx++], X86_REG_RBP, offset);
                    }
                }
            }
        }
    }

    /* Emit basic blocks and instructions */
    int inst_counter = 0;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        char bb_clean[128], full_lbl[256];
        sanitize_label(bb->name ? bb->name : "block", bb_clean, sizeof(bb_clean));
        snprintf(full_lbl, sizeof(full_lbl), ".L_%s_%s", fn->name, bb_clean);
        x86_emit_label(list, full_lbl);

        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next, inst_counter++) {
            if (!gen_instruction(list, frame, inst, inst_counter, out_error)) {
                x86_instr_list_free(list);
                x86_frame_free(frame);
                regalloc_free(regalloc);
                return false;
            }
        }
    }

    /* Epilogue */
    char epilogue_lbl[256];
    snprintf(epilogue_lbl, sizeof(epilogue_lbl), ".L_%s_epilogue", fn->name);
    x86_emit_label(list, epilogue_lbl);

    if (frame->callee_saved_count > 0) {
        x86_emit_lea_mem(list, X86_REG_RBP, -frame->callee_saved_count * 8, X86_REG_RSP);
        for (int i = frame->callee_saved_count - 1; i >= 0; i--) {
            x86_emit_pop(list, frame->callee_saved_regs[i]);
        }
    } else if (frame->total_stack_size > 0) {
        x86_emit_mov_reg_reg(list, 8, X86_REG_RBP, X86_REG_RSP);
    }
    x86_emit_pop(list, X86_REG_RBP);
    x86_emit_ret(list);

    x86_frame_free(frame);
    regalloc_free(regalloc);

    /* Run Peephole Optimization pass on instructions */
    x86_instr_list_optimize(list);
    if (timings) timings->codegen_ms += (cco_get_time_ms() - t_cg);

    *out_list = list;
    return true;
}

bool gen_function_instructions(IrFunction *fn, X86InstrList **out_list, char **out_error) {
    return gen_function_instructions_timed(fn, out_list, NULL, out_error);
}

/* Public API 1: Assembly Code Generation (Reference Mode A)                 */

char *x86_64_generate_assembly(IrModule *module, char **out_error) {
    if (!module) {
        if (out_error) *out_error = strdup("x86-64 backend error: null IR module");
        return NULL;
    }

    clear_strings();
    clear_floats();

    size_t fn_count = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) fn_count++;

    X86InstrList **fn_lists = (X86InstrList **)calloc(fn_count > 0 ? fn_count : 1, sizeof(X86InstrList *));
    size_t fidx = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next, fidx++) {
        if (!gen_function_instructions(fn, &fn_lists[fidx], out_error)) {
            for (size_t k = 0; k < fidx; k++) x86_instr_list_free(fn_lists[k]);
            free(fn_lists);
            clear_strings();
            clear_floats();
            return NULL;
        }
    }

    size_t cap = 8192;
    size_t len = 0;
    char *asm_out = (char *)malloc(cap);
    asm_out[0] = '\0';

    #define APPEND(s) do { \
        size_t slen = strlen(s); \
        if (len + slen + 1 > cap) { \
            while (len + slen + 1 > cap) cap *= 2; \
            asm_out = (char *)realloc(asm_out, cap); \
        } \
        memcpy(asm_out + len, s, slen); \
        len += slen; \
        asm_out[len] = '\0'; \
    } while (0)

    #define APPENDF(...) do { \
        char tmp[1024]; \
        snprintf(tmp, sizeof(tmp), __VA_ARGS__); \
        APPEND(tmp); \
    } while (0)

    APPENDF("    .file \"%s\"\n", module->name ? module->name : "program.cco");
    APPEND("    .section .rodata\n");
    APPEND(".LC_fmt_int:\n    .string \"%ld\\n\"\n");
    APPEND(".LC_fmt_float:\n    .string \"%g\\n\"\n");
    APPEND(".LC_fmt_str:\n    .string \"%s\\n\"\n");
    APPEND(".LC_str_true:\n    .string \"true\"\n");
    APPEND(".LC_str_false:\n    .string \"false\"\n");
    APPEND("    .align 16\n.LC_neg_float_mask:\n    .quad 0x8000000000000000\n    .quad 0\n");

    for (StrConst *s = g_strings; s; s = s->next) {
        APPENDF(".LC_str_%d:\n    .string \"%s\"\n", s->id, s->text);
    }

    for (FloatConst *f = g_floats; f; f = f->next) {
        APPENDF(".LC_float_%d:\n    .quad 0x%016lx\n", f->id, (unsigned long)f->bits);
    }

    APPEND("    .text\n");

    fidx = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next, fidx++) {
        X86InstrList *list = fn_lists[fidx];
        APPENDF("\n    .globl %s\n", fn->name);
        APPENDF("    .type %s, @function\n", fn->name);

        char *fn_asm = x86_instr_list_to_asm(list);
        if (fn_asm) {
            APPEND(fn_asm);
            free(fn_asm);
        }

        APPENDF("    .size %s, .-%s\n", fn->name, fn->name);
        x86_instr_list_free(list);
    }
    free(fn_lists);

    APPEND("\n    .section .note.GNU-stack,\"\",@progbits\n");

    clear_strings();
    clear_floats();

    #undef APPEND
    #undef APPENDF

    return asm_out;
}

/* Public API 2: Direct Machine Code & ELF64 Object Writer (Mode B)          */

typedef struct {
    char *name;
    uint64_t offset;
} RodataMapEntry;

bool x86_64_emit_object_file_timed(IrModule *module, const char *out_path, CcoPassTimings *timings, char **out_error) {
    if (!module || !out_path) {
        if (out_error) *out_error = strdup("null module or out_path in x86_64_emit_object_file");
        return false;
    }

    clear_strings();
    clear_floats();

    size_t fn_count = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) fn_count++;

    X86InstrList **fn_lists = (X86InstrList **)calloc(fn_count > 0 ? fn_count : 1, sizeof(X86InstrList *));
    size_t fidx = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next, fidx++) {
        if (!gen_function_instructions_timed(fn, &fn_lists[fidx], timings, out_error)) {
            for (size_t k = 0; k < fidx; k++) x86_instr_list_free(fn_lists[k]);
            free(fn_lists);
            clear_strings();
            clear_floats();
            return false;
        }
    }

    /* 1. Build .rodata byte buffer and symbol offset map */
    ByteBuffer *rodata_buf = byte_buf_create();
    size_t rodata_map_cap = 64 + g_str_count + g_float_count;
    RodataMapEntry *rodata_map = (RodataMapEntry *)malloc(sizeof(RodataMapEntry) * rodata_map_cap);
    size_t rodata_map_count = 0;

    #define ADD_RODATA_SYM(sym_name, data_ptr, data_len) do { \
        rodata_map[rodata_map_count].name = strdup(sym_name); \
        rodata_map[rodata_map_count].offset = rodata_buf->size; \
        rodata_map_count++; \
        byte_buf_append_bytes(rodata_buf, data_ptr, data_len); \
    } while (0)

    /* .LC_neg_float_mask at offset 0 (16-byte aligned) */
    uint8_t neg_mask[16] = {0, 0, 0, 0, 0, 0, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0};
    ADD_RODATA_SYM(".LC_neg_float_mask", neg_mask, 16);

    /* Format strings */
    ADD_RODATA_SYM(".LC_fmt_int", "%ld\n", 5);
    ADD_RODATA_SYM(".LC_fmt_float", "%g\n", 4);
    ADD_RODATA_SYM(".LC_fmt_str", "%s\n", 4);
    ADD_RODATA_SYM(".LC_str_true", "true", 5);
    ADD_RODATA_SYM(".LC_str_false", "false", 6);

    /* String constants */
    for (StrConst *s = g_strings; s; s = s->next) {
        char sym[64];
        snprintf(sym, sizeof(sym), ".LC_str_%d", s->id);
        ADD_RODATA_SYM(sym, s->text, strlen(s->text) + 1);
    }

    /* Float constants (aligned to 8 bytes) */
    while (rodata_buf->size % 8 != 0) {
        byte_buf_append_u8(rodata_buf, 0);
    }
    for (FloatConst *f = g_floats; f; f = f->next) {
        char sym[64];
        snprintf(sym, sizeof(sym), ".LC_float_%d", f->id);
        rodata_map[rodata_map_count].name = strdup(sym);
        rodata_map[rodata_map_count].offset = rodata_buf->size;
        rodata_map_count++;
        byte_buf_append_u64(rodata_buf, f->bits);
    }

    #undef ADD_RODATA_SYM

    /* 2. Encode all functions into .text */
    double t_encode = timings ? cco_get_time_ms() : 0.0;
    ByteBuffer *text_buf = byte_buf_create();
    X86RelocList *text_relocs = x86_reloc_list_create();
    ElfSymbolList *symbols = elf_symbol_list_create();

    fidx = 0;
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next, fidx++) {
        X86InstrList *list = fn_lists[fidx];
        uint64_t fn_start_pc = text_buf->size;
        if (!x86_encode_function(list, text_buf, text_relocs, out_error)) {
            for (size_t k = fidx; k < fn_count; k++) x86_instr_list_free(fn_lists[k]);
            free(fn_lists);
            byte_buf_free(rodata_buf);
            byte_buf_free(text_buf);
            x86_reloc_list_free(text_relocs);
            elf_symbol_list_free(symbols);
            for (size_t i = 0; i < rodata_map_count; i++) free(rodata_map[i].name);
            free(rodata_map);
            clear_strings();
            clear_floats();
            return false;
        }

        uint64_t fn_size = text_buf->size - fn_start_pc;
        elf_symbol_list_add(symbols, fn->name, fn_start_pc, fn_size, true, true);
        x86_instr_list_free(list);
    }
    free(fn_lists);

    /* 3. Resolve rodata relocations and identify external symbols */
    for (size_t r = 0; r < text_relocs->count; r++) {
        X86Reloc *rec = &text_relocs->items[r];
        bool is_rodata = false;

        for (size_t m = 0; m < rodata_map_count; m++) {
            if (strcmp(rec->symbol, rodata_map[m].name) == 0) {
                free(rec->symbol);
                rec->symbol = strdup(".rodata");
                rec->addend = (int64_t)rodata_map[m].offset - 4;
                is_rodata = true;
                break;
            }
        }

        if (!is_rodata) {
            /* Check if this symbol is already in symbols list (e.g. defined local func) */
            bool found = false;
            for (size_t s = 0; s < symbols->count; s++) {
                if (strcmp(symbols->items[s].name, rec->symbol) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                /* Undefined external symbol (printf, free, etc.) */
                elf_symbol_list_add(symbols, rec->symbol, 0, 0, true, false);
            }
        }
    }
    if (timings) timings->codegen_ms += (cco_get_time_ms() - t_encode);

    /* 4. Write ELF64 relocatable object file */
    double t_elf = timings ? cco_get_time_ms() : 0.0;
    bool ok = elf64_write_object_file(out_path,
                                      module->name ? module->name : "program.cco",
                                      text_buf,
                                      text_relocs,
                                      rodata_buf,
                                      symbols,
                                      out_error);
    if (timings) timings->elf_gen_ms += (cco_get_time_ms() - t_elf);

    /* 5. Cleanup */
    byte_buf_free(rodata_buf);
    byte_buf_free(text_buf);
    x86_reloc_list_free(text_relocs);
    elf_symbol_list_free(symbols);
    for (size_t i = 0; i < rodata_map_count; i++) free(rodata_map[i].name);
    free(rodata_map);
    clear_strings();
    clear_floats();

    return ok;
}

bool x86_64_emit_object_file(IrModule *module, const char *out_path, char **out_error) {
    return x86_64_emit_object_file_timed(module, out_path, NULL, out_error);
}
