// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_instr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* String Buffer Helper                                                      */

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} Buffer;

static Buffer create_buf(void) {
    Buffer b;
    b.capacity = 4096;
    b.length = 0;
    b.buffer = (char *)malloc(b.capacity);
    if (b.buffer) b.buffer[0] = '\0';
    return b;
}

static void buf_append(Buffer *b, const char *str) {
    if (!str) return;
    size_t len = strlen(str);
    if (b->length + len + 1 > b->capacity) {
        while (b->length + len + 1 > b->capacity) {
            b->capacity *= 2;
        }
        b->buffer = (char *)realloc(b->buffer, b->capacity);
    }
    memcpy(b->buffer + b->length, str, len);
    b->length += len;
    b->buffer[b->length] = '\0';
}

static void buf_appendf(Buffer *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[2048];
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    buf_append(b, tmp);
}

/* Allocation and Management                                                 */

X86InstrList *x86_instr_list_create(void) {
    X86InstrList *list = (X86InstrList *)malloc(sizeof(X86InstrList));
    if (!list) return NULL;
    list->first = NULL;
    list->last = NULL;
    list->count = 0;
    return list;
}

X86Instr *x86_instr_create(X86Op op) {
    X86Instr *inst = (X86Instr *)calloc(1, sizeof(X86Instr));
    if (!inst) return NULL;
    inst->op = op;
    inst->dst.kind = X86_OPERAND_NONE;
    inst->src.kind = X86_OPERAND_NONE;
    return inst;
}

void x86_instr_free(X86Instr *inst) {
    if (!inst) return;
    if (inst->dst.symbol) free(inst->dst.symbol);
    if (inst->src.symbol) free(inst->src.symbol);
    free(inst);
}

void x86_instr_list_free(X86InstrList *list) {
    if (!list) return;
    X86Instr *curr = list->first;
    while (curr) {
        X86Instr *next = curr->next;
        x86_instr_free(curr);
        curr = next;
    }
    free(list);
}

void x86_instr_list_append(X86InstrList *list, X86Instr *inst) {
    if (!list || !inst) return;
    inst->prev = list->last;
    inst->next = NULL;
    if (list->last) {
        list->last->next = inst;
    } else {
        list->first = inst;
    }
    list->last = inst;
    list->count++;
}

/* Emitter Functions                                                         */

void x86_emit_label(X86InstrList *list, const char *name) {
    X86Instr *inst = x86_instr_create(X86_OP_LABEL);
    inst->dst.kind = X86_OPERAND_LABEL;
    inst->dst.symbol = strdup(name);
    x86_instr_list_append(list, inst);
}

void x86_emit_push(X86InstrList *list, X86Reg reg) {
    X86Instr *inst = x86_instr_create(X86_OP_PUSH);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = reg;
    inst->dst.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_pop(X86InstrList *list, X86Reg reg) {
    X86Instr *inst = x86_instr_create(X86_OP_POP);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = reg;
    inst->dst.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_ret(X86InstrList *list) {
    X86Instr *inst = x86_instr_create(X86_OP_RET);
    x86_instr_list_append(list, inst);
}

void x86_emit_sub_imm_rsp(X86InstrList *list, int32_t imm) {
    X86Instr *inst = x86_instr_create(X86_OP_SUB_IMM);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = X86_REG_RSP;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_IMM;
    inst->src.imm = imm;
    x86_instr_list_append(list, inst);
}

void x86_emit_add_imm_rsp(X86InstrList *list, int32_t imm) {
    X86Instr *inst = x86_instr_create(X86_OP_ADD_IMM);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = X86_REG_RSP;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_IMM;
    inst->src.imm = imm;
    x86_instr_list_append(list, inst);
}

void x86_emit_lea_mem(X86InstrList *list, X86Reg base_reg, int32_t offset, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_LEA);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_MEM_BASE;
    inst->src.reg = base_reg;
    inst->src.imm = offset;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_lea_rip(X86InstrList *list, const char *symbol, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_LEA);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_MEM_RIP;
    inst->src.symbol = strdup(symbol);
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_mov_reg_reg(X86InstrList *list, int size, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOV);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = size;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = size;
    x86_instr_list_append(list, inst);
}

void x86_emit_mov_imm_reg(X86InstrList *list, int size, int64_t imm, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOV);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = size;
    inst->src.kind = X86_OPERAND_IMM;
    inst->src.imm = imm;
    inst->src.size = size;
    x86_instr_list_append(list, inst);
}

void x86_emit_mov_mem_reg(X86InstrList *list, int size, X86Reg base_reg, int32_t offset, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOV);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = size;
    inst->src.kind = X86_OPERAND_MEM_BASE;
    inst->src.reg = base_reg;
    inst->src.imm = offset;
    inst->src.size = size;
    x86_instr_list_append(list, inst);
}

void x86_emit_mov_reg_mem(X86InstrList *list, int size, X86Reg src_reg, X86Reg base_reg, int32_t offset) {
    X86Instr *inst = x86_instr_create(X86_OP_MOV);
    inst->dst.kind = X86_OPERAND_MEM_BASE;
    inst->dst.reg = base_reg;
    inst->dst.imm = offset;
    inst->dst.size = size;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = size;
    x86_instr_list_append(list, inst);
}

void x86_emit_mov_rip_reg(X86InstrList *list, const char *symbol, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOV);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_MEM_RIP;
    inst->src.symbol = strdup(symbol);
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_movb_imm(X86InstrList *list, uint8_t imm, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOVB_IMM);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 1;
    inst->src.kind = X86_OPERAND_IMM;
    inst->src.imm = imm;
    inst->src.size = 1;
    x86_instr_list_append(list, inst);
}

void x86_emit_movslq(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOVSLQ);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_movzbl(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOVZBL);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 1;
    x86_instr_list_append(list, inst);
}

void x86_emit_cmove(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_CMOVE);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_add(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_ADD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_sub(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_SUB);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_imul(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_IMUL);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_idiv(X86InstrList *list, X86Reg src_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_IDIV);
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_cltd(X86InstrList *list) {
    X86Instr *inst = x86_instr_create(X86_OP_CLTD);
    x86_instr_list_append(list, inst);
}

void x86_emit_neg(X86InstrList *list, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_NEG);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_and(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_AND);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_or(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_OR);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_xor(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_XOR);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_cmp(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_CMP);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_cmp_imm(X86InstrList *list, int32_t imm, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_CMP_IMM);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 4;
    inst->src.kind = X86_OPERAND_IMM;
    inst->src.imm = imm;
    inst->src.size = 4;
    x86_instr_list_append(list, inst);
}

void x86_emit_setcc(X86InstrList *list, X86CondCode cond, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_SETCC);
    inst->cond = cond;
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 1;
    x86_instr_list_append(list, inst);
}

void x86_emit_andb(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_ANDB);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 1;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 1;
    x86_instr_list_append(list, inst);
}

void x86_emit_orb(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_ORB);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 1;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 1;
    x86_instr_list_append(list, inst);
}

void x86_emit_jmp(X86InstrList *list, const char *label) {
    X86Instr *inst = x86_instr_create(X86_OP_JMP);
    inst->src.kind = X86_OPERAND_LABEL;
    inst->src.symbol = strdup(label);
    x86_instr_list_append(list, inst);
}

void x86_emit_jne(X86InstrList *list, const char *label) {
    X86Instr *inst = x86_instr_create(X86_OP_JNE);
    inst->src.kind = X86_OPERAND_LABEL;
    inst->src.symbol = strdup(label);
    x86_instr_list_append(list, inst);
}

void x86_emit_je(X86InstrList *list, const char *label) {
    X86Instr *inst = x86_instr_create(X86_OP_JE);
    inst->src.kind = X86_OPERAND_LABEL;
    inst->src.symbol = strdup(label);
    x86_instr_list_append(list, inst);
}

void x86_emit_call(X86InstrList *list, const char *symbol) {
    X86Instr *inst = x86_instr_create(X86_OP_CALL);
    inst->src.kind = X86_OPERAND_SYMBOL;
    inst->src.symbol = strdup(symbol);
    x86_instr_list_append(list, inst);
}

void x86_emit_movsd_reg_reg(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOVSD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_movsd_mem_reg(X86InstrList *list, X86Reg base_reg, int32_t offset, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOVSD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_MEM_BASE;
    inst->src.reg = base_reg;
    inst->src.imm = offset;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_movsd_reg_mem(X86InstrList *list, X86Reg src_reg, X86Reg base_reg, int32_t offset) {
    X86Instr *inst = x86_instr_create(X86_OP_MOVSD);
    inst->dst.kind = X86_OPERAND_MEM_BASE;
    inst->dst.reg = base_reg;
    inst->dst.imm = offset;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_movsd_rip_reg(X86InstrList *list, const char *symbol, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MOVSD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_MEM_RIP;
    inst->src.symbol = strdup(symbol);
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_addsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_ADDSD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_subsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_SUBSD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_mulsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_MULSD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_divsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_DIVSD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_xorpd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_XORPD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

void x86_emit_ucomisd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg) {
    X86Instr *inst = x86_instr_create(X86_OP_UCOMISD);
    inst->dst.kind = X86_OPERAND_REG;
    inst->dst.reg = dst_reg;
    inst->dst.size = 8;
    inst->src.kind = X86_OPERAND_REG;
    inst->src.reg = src_reg;
    inst->src.size = 8;
    x86_instr_list_append(list, inst);
}

/* Peephole Optimization Pass                                                */

void x86_instr_list_optimize(X86InstrList *list) {
    if (!list) return;

    bool changed = true;
    int iteration = 0;

    while (changed && iteration < 4) {
        changed = false;
        iteration++;

        X86Instr *curr = list->first;
        while (curr) {
            X86Instr *next = curr->next;

            /* 1. Eliminate redundant moves: mov %reg, %reg (GPR & XMM) */
            if ((curr->op == X86_OP_MOV || curr->op == X86_OP_MOVSD) &&
                curr->dst.kind == X86_OPERAND_REG && curr->src.kind == X86_OPERAND_REG &&
                curr->dst.reg == curr->src.reg && curr->dst.size == curr->src.size) {
                if (curr->prev) curr->prev->next = curr->next;
                else list->first = curr->next;
                if (curr->next) curr->next->prev = curr->prev;
                else list->last = curr->prev;
                list->count--;
                x86_instr_free(curr);
                curr = next;
                changed = true;
                continue;
            }

            /* 2. Zero constant optimization: mov $0, %reg -> xorl %reg, %reg */
            if (curr->op == X86_OP_MOV &&
                curr->dst.kind == X86_OPERAND_REG && (curr->dst.size == 4 || curr->dst.size == 8) &&
                curr->src.kind == X86_OPERAND_IMM && curr->src.imm == 0) {
                curr->op = X86_OP_XOR;
                curr->dst.size = 4;
                curr->src.kind = X86_OPERAND_REG;
                curr->src.reg = curr->dst.reg;
                curr->src.size = 4;
                changed = true;
            }

            /* 3. Eliminate redundant store-then-load */
            if (next &&
                (curr->op == X86_OP_MOV || curr->op == X86_OP_MOVSD) &&
                curr->dst.kind == X86_OPERAND_MEM_BASE && curr->src.kind == X86_OPERAND_REG &&
                (next->op == X86_OP_MOV || next->op == X86_OP_MOVSD) &&
                next->dst.kind == X86_OPERAND_REG && next->src.kind == X86_OPERAND_MEM_BASE) {
                if (curr->dst.reg == next->src.reg &&
                    curr->dst.imm == next->src.imm &&
                    curr->src.reg == next->dst.reg &&
                    curr->dst.size == next->src.size) {
                    /* Redundant load can be safely dropped */
                    X86Instr *after_next = next->next;
                    if (next->prev) next->prev->next = after_next;
                    if (after_next) after_next->prev = next->prev;
                    else list->last = next->prev;
                    list->count--;
                    x86_instr_free(next);
                    next = after_next;
                    changed = true;
                }
            }

            /* 4. Eliminate redundant jmp immediately preceding its target label */
            if (curr->op == X86_OP_JMP && next && next->op == X86_OP_LABEL &&
                curr->dst.kind == X86_OPERAND_LABEL && next->dst.kind == X86_OPERAND_LABEL &&
                curr->dst.symbol && next->dst.symbol &&
                strcmp(curr->dst.symbol, next->dst.symbol) == 0) {
                X86Instr *prev = curr->prev;
                if (prev) prev->next = next;
                else list->first = next;
                next->prev = prev;
                list->count--;
                x86_instr_free(curr);
                curr = next;
                changed = true;
                continue;
            }

            /* 5. Inverted conditional branch optimization:
             *    jne L1; jmp L2; L1:  -->  je L2; L1: (L1 falls through)
             *    je L1; jmp L2; L1:   -->  jne L2; L1: (L1 falls through)
             */
            if ((curr->op == X86_OP_JNE || curr->op == X86_OP_JE) &&
                next && next->op == X86_OP_JMP && next->next &&
                next->next->op == X86_OP_LABEL &&
                curr->dst.kind == X86_OPERAND_LABEL && next->dst.kind == X86_OPERAND_LABEL &&
                next->next->dst.kind == X86_OPERAND_LABEL &&
                curr->dst.symbol && next->dst.symbol && next->next->dst.symbol &&
                strcmp(curr->dst.symbol, next->next->dst.symbol) == 0) {
                curr->op = (curr->op == X86_OP_JNE) ? X86_OP_JE : X86_OP_JNE;
                free(curr->dst.symbol);
                curr->dst.symbol = strdup(next->dst.symbol);

                X86Instr *after_next = next->next;
                curr->next = after_next;
                after_next->prev = curr;
                list->count--;
                x86_instr_free(next);
                curr = after_next;
                changed = true;
                continue;
            }

            curr = next;
        }

        /* 6. Jump-to-jump chain collapse */
        typedef struct { char from[128]; char to[128]; } JmpAlias;
        JmpAlias aliases[64];
        int alias_count = 0;

        for (X86Instr *it = list->first; it && alias_count < 64; it = it->next) {
            if (it->op == X86_OP_LABEL && it->next && it->next->op == X86_OP_JMP) {
                if (it->dst.symbol && it->next->dst.symbol &&
                    strcmp(it->dst.symbol, it->next->dst.symbol) != 0) {
                    snprintf(aliases[alias_count].from, sizeof(aliases[alias_count].from), "%s", it->dst.symbol);
                    snprintf(aliases[alias_count].to, sizeof(aliases[alias_count].to), "%s", it->next->dst.symbol);
                    alias_count++;
                }
            }
        }

        if (alias_count > 0) {
            for (X86Instr *it = list->first; it; it = it->next) {
                if ((it->op == X86_OP_JMP || it->op == X86_OP_JE || it->op == X86_OP_JNE) &&
                    it->dst.kind == X86_OPERAND_LABEL && it->dst.symbol) {
                    for (int a = 0; a < alias_count; a++) {
                        if (strcmp(it->dst.symbol, aliases[a].from) == 0) {
                            free(it->dst.symbol);
                            it->dst.symbol = strdup(aliases[a].to);
                            changed = true;
                            break;
                        }
                    }
                }
            }
        }
    }
}

/* Print to GNU/AT&T Assembly String                                         */

static const char *cond_to_str(X86CondCode cond) {
    switch (cond) {
        case X86_CC_E:   return "sete";
        case X86_CC_NE:  return "setne";
        case X86_CC_L:   return "setl";
        case X86_CC_LE:  return "setle";
        case X86_CC_G:   return "setg";
        case X86_CC_GE:  return "setge";
        case X86_CC_B:   return "setb";
        case X86_CC_BE:  return "setbe";
        case X86_CC_A:   return "seta";
        case X86_CC_AE:  return "setae";
        case X86_CC_P:   return "setp";
        case X86_CC_NP:  return "setnp";
    }
    return "sete";
}

char *x86_instr_list_to_asm(X86InstrList *list) {
    if (!list) return NULL;
    Buffer b = create_buf();

    for (X86Instr *inst = list->first; inst; inst = inst->next) {
        switch (inst->op) {
            case X86_OP_LABEL:
                buf_appendf(&b, "%s:\n", inst->dst.symbol);
                break;
            case X86_OP_PUSH:
                buf_appendf(&b, "    pushq %s\n", x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_POP:
                buf_appendf(&b, "    popq %s\n", x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_RET:
                buf_append(&b, "    ret\n");
                break;
            case X86_OP_SUB_IMM:
                buf_appendf(&b, "    subq $%ld, %%rsp\n", (long)inst->src.imm);
                break;
            case X86_OP_ADD_IMM:
                buf_appendf(&b, "    addq $%ld, %%rsp\n", (long)inst->src.imm);
                break;
            case X86_OP_LEA:
                if (inst->src.kind == X86_OPERAND_MEM_BASE) {
                    buf_appendf(&b, "    leaq %ld(%s), %s\n",
                                (long)inst->src.imm,
                                x86_reg_to_str(inst->src.reg, 8),
                                x86_reg_to_str(inst->dst.reg, 8));
                } else if (inst->src.kind == X86_OPERAND_MEM_RIP) {
                    buf_appendf(&b, "    leaq %s(%%rip), %s\n",
                                inst->src.symbol,
                                x86_reg_to_str(inst->dst.reg, 8));
                }
                break;
            case X86_OP_MOV: {
                char sfx = (inst->dst.size == 8 || inst->src.size == 8) ? 'q' : 'l';
                int sz = (sfx == 'q') ? 8 : 4;
                if (inst->src.kind == X86_OPERAND_IMM) {
                    buf_appendf(&b, "    mov%c $%ld, %s\n", sfx, (long)inst->src.imm, x86_reg_to_str(inst->dst.reg, sz));
                } else if (inst->src.kind == X86_OPERAND_REG && inst->dst.kind == X86_OPERAND_REG) {
                    buf_appendf(&b, "    mov%c %s, %s\n", sfx, x86_reg_to_str(inst->src.reg, sz), x86_reg_to_str(inst->dst.reg, sz));
                } else if (inst->src.kind == X86_OPERAND_MEM_BASE) {
                    buf_appendf(&b, "    mov%c %ld(%s), %s\n", sfx, (long)inst->src.imm, x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, sz));
                } else if (inst->dst.kind == X86_OPERAND_MEM_BASE) {
                    buf_appendf(&b, "    mov%c %s, %ld(%s)\n", sfx, x86_reg_to_str(inst->src.reg, sz), (long)inst->dst.imm, x86_reg_to_str(inst->dst.reg, 8));
                } else if (inst->src.kind == X86_OPERAND_MEM_RIP) {
                    buf_appendf(&b, "    movq %s(%%rip), %s\n", inst->src.symbol, x86_reg_to_str(inst->dst.reg, 8));
                }
                break;
            }
            case X86_OP_MOVB_IMM:
                buf_appendf(&b, "    movb $%d, %s\n", (int)(uint8_t)inst->src.imm, x86_reg_to_str(inst->dst.reg, 1));
                break;
            case X86_OP_MOVSLQ:
                buf_appendf(&b, "    movslq %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_MOVZBL:
                buf_appendf(&b, "    movzbl %s, %s\n", x86_reg_to_str(inst->src.reg, 1), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_CMOVE:
                buf_appendf(&b, "    cmoveq %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_ADD:
                buf_appendf(&b, "    addl %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_SUB:
                buf_appendf(&b, "    subl %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_IMUL:
                buf_appendf(&b, "    imull %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_IDIV:
                buf_appendf(&b, "    idivl %s\n", x86_reg_to_str(inst->src.reg, 4));
                break;
            case X86_OP_CLTD:
                buf_append(&b, "    cltd\n");
                break;
            case X86_OP_NEG:
                buf_appendf(&b, "    negl %s\n", x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_AND:
                buf_appendf(&b, "    andl %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_OR:
                buf_appendf(&b, "    orl %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_XOR:
                buf_appendf(&b, "    xorl %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_CMP:
                buf_appendf(&b, "    cmpl %s, %s\n", x86_reg_to_str(inst->src.reg, 4), x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_CMP_IMM:
                buf_appendf(&b, "    cmpl $%ld, %s\n", (long)inst->src.imm, x86_reg_to_str(inst->dst.reg, 4));
                break;
            case X86_OP_SETCC:
                buf_appendf(&b, "    %s %s\n", cond_to_str(inst->cond), x86_reg_to_str(inst->dst.reg, 1));
                break;
            case X86_OP_ANDB:
                buf_appendf(&b, "    andb %s, %s\n", x86_reg_to_str(inst->src.reg, 1), x86_reg_to_str(inst->dst.reg, 1));
                break;
            case X86_OP_ORB:
                buf_appendf(&b, "    orb %s, %s\n", x86_reg_to_str(inst->src.reg, 1), x86_reg_to_str(inst->dst.reg, 1));
                break;
            case X86_OP_JMP:
                buf_appendf(&b, "    jmp %s\n", inst->src.symbol);
                break;
            case X86_OP_JNE:
                buf_appendf(&b, "    jne %s\n", inst->src.symbol);
                break;
            case X86_OP_JE:
                buf_appendf(&b, "    je %s\n", inst->src.symbol);
                break;
            case X86_OP_CALL:
                buf_appendf(&b, "    call %s\n", inst->src.symbol);
                break;

            /* SSE2 Instructions */
            case X86_OP_MOVSD:
                if (inst->src.kind == X86_OPERAND_REG && inst->dst.kind == X86_OPERAND_REG) {
                    buf_appendf(&b, "    movsd %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                } else if (inst->src.kind == X86_OPERAND_MEM_BASE) {
                    buf_appendf(&b, "    movsd %ld(%s), %s\n", (long)inst->src.imm, x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                } else if (inst->dst.kind == X86_OPERAND_MEM_BASE) {
                    buf_appendf(&b, "    movsd %s, %ld(%s)\n", x86_reg_to_str(inst->src.reg, 8), (long)inst->dst.imm, x86_reg_to_str(inst->dst.reg, 8));
                } else if (inst->src.kind == X86_OPERAND_MEM_RIP) {
                    buf_appendf(&b, "    movsd %s(%%rip), %s\n", inst->src.symbol, x86_reg_to_str(inst->dst.reg, 8));
                }
                break;
            case X86_OP_ADDSD:
                buf_appendf(&b, "    addsd %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_SUBSD:
                buf_appendf(&b, "    subsd %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_MULSD:
                buf_appendf(&b, "    mulsd %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_DIVSD:
                buf_appendf(&b, "    divsd %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_XORPD:
                buf_appendf(&b, "    xorpd %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                break;
            case X86_OP_UCOMISD:
                buf_appendf(&b, "    ucomisd %s, %s\n", x86_reg_to_str(inst->src.reg, 8), x86_reg_to_str(inst->dst.reg, 8));
                break;
        }
    }

    return b.buffer;
}
