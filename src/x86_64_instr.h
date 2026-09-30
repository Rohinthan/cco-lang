// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X86_64_INSTR_H
#define X86_64_INSTR_H

#include "x86_64_regalloc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Instruction Opcodes                                                       */

typedef enum {
    X86_OP_LABEL,       /* Symbolic label */
    X86_OP_PUSH,        /* pushq reg */
    X86_OP_POP,         /* popq reg */
    X86_OP_RET,         /* ret */
    X86_OP_SUB_IMM,     /* subq $imm, %rsp */
    X86_OP_ADD_IMM,     /* addq $imm, %rsp */
    X86_OP_LEA,         /* leaq disp(%rbp), %reg  or  leaq symbol(%rip), %reg */
    X86_OP_MOV,         /* movl / movq: reg->reg, imm->reg, mem->reg, reg->mem */
    X86_OP_MOVB_IMM,    /* movb $imm, %al */
    X86_OP_MOVSLQ,      /* movslq %reg32, %reg64 */
    X86_OP_MOVZBL,      /* movzbl %reg8, %reg32 */
    X86_OP_CMOVE,       /* cmoveq %reg64, %reg64 */
    X86_OP_ADD,         /* addl %reg, %reg */
    X86_OP_SUB,         /* subl %reg, %reg */
    X86_OP_IMUL,        /* imull %reg, %reg */
    X86_OP_IDIV,        /* idivl %reg */
    X86_OP_CLTD,        /* cltd / cdq */
    X86_OP_NEG,         /* negl %reg */
    X86_OP_AND,         /* andl %reg, %reg */
    X86_OP_OR,          /* orl %reg, %reg */
    X86_OP_XOR,         /* xorl %reg, %reg */
    X86_OP_CMP,         /* cmpl %reg, %reg */
    X86_OP_CMP_IMM,     /* cmpl $imm, %reg */
    X86_OP_SETCC,       /* sete, setne, setl, setle, setg, setge, setb, setae, setbe, seta, setp, setnp */
    X86_OP_ANDB,        /* andb %reg8, %reg8 */
    X86_OP_ORB,         /* orb %reg8, %reg8 */
    X86_OP_JMP,         /* jmp label */
    X86_OP_JNE,         /* jne label */
    X86_OP_JE,          /* je label */
    X86_OP_CALL,        /* call symbol */

    /* SSE2 Floating-Point Opcodes */
    X86_OP_MOVSD,       /* movsd: xmm->xmm, mem->xmm, xmm->mem, rip->xmm */
    X86_OP_ADDSD,       /* addsd %xmm, %xmm */
    X86_OP_SUBSD,       /* subsd %xmm, %xmm */
    X86_OP_MULSD,       /* mulsd %xmm, %xmm */
    X86_OP_DIVSD,       /* divsd %xmm, %xmm */
    X86_OP_XORPD,       /* xorpd %xmm, %xmm */
    X86_OP_UCOMISD      /* ucomisd %xmm, %xmm */
} X86Op;

/* Condition Codes                                                           */

typedef enum {
    X86_CC_E,   /* == (equal / zero) */
    X86_CC_NE,  /* != (not equal / not zero) */
    X86_CC_L,   /* < (less signed) */
    X86_CC_LE,  /* <= (less equal signed) */
    X86_CC_G,   /* > (greater signed) */
    X86_CC_GE,  /* >= (greater equal signed) */
    X86_CC_B,   /* < (below unsigned) */
    X86_CC_BE,  /* <= (below equal unsigned) */
    X86_CC_A,   /* > (above unsigned) */
    X86_CC_AE,  /* >= (above equal unsigned) */
    X86_CC_P,   /* parity */
    X86_CC_NP   /* not parity */
} X86CondCode;

/* Operands                                                                  */

typedef enum {
    X86_OPERAND_NONE,
    X86_OPERAND_REG,       /* Physical register (X86Reg) with size (1, 4, 8) */
    X86_OPERAND_IMM,       /* Immediate integer value (imm64/imm32/imm8) */
    X86_OPERAND_MEM_BASE,  /* Memory: disp(%base_reg) */
    X86_OPERAND_MEM_RIP,   /* Memory: symbol(%rip) */
    X86_OPERAND_LABEL,     /* Label name for jmp/jcc */
    X86_OPERAND_SYMBOL     /* Symbol name for call */
} X86OperandKind;

typedef struct {
    X86OperandKind kind;
    int size;              /* 1, 4, 8 bytes */
    X86Reg reg;            /* Register or base register for memory */
    int64_t imm;           /* Immediate or displacement */
    char *symbol;          /* For MEM_RIP, LABEL, SYMBOL */
} X86Operand;

/* Instruction Structure & Instruction List                                  */

typedef struct X86Instr {
    X86Op op;
    X86CondCode cond;      /* For SETCC */
    X86Operand dst;
    X86Operand src;
    struct X86Instr *prev;
    struct X86Instr *next;
} X86Instr;

typedef struct X86InstrList {
    X86Instr *first;
    X86Instr *last;
    int count;
} X86InstrList;

/* Constructor and Emission Helpers                                          */

X86InstrList *x86_instr_list_create(void);
void x86_instr_list_free(X86InstrList *list);
void x86_instr_list_append(X86InstrList *list, X86Instr *inst);

X86Instr *x86_instr_create(X86Op op);
void x86_instr_free(X86Instr *inst);

/* Convenient builders */
void x86_emit_label(X86InstrList *list, const char *name);
void x86_emit_push(X86InstrList *list, X86Reg reg);
void x86_emit_pop(X86InstrList *list, X86Reg reg);
void x86_emit_ret(X86InstrList *list);
void x86_emit_sub_imm_rsp(X86InstrList *list, int32_t imm);
void x86_emit_add_imm_rsp(X86InstrList *list, int32_t imm);
void x86_emit_lea_mem(X86InstrList *list, X86Reg base_reg, int32_t offset, X86Reg dst_reg);
void x86_emit_lea_rip(X86InstrList *list, const char *symbol, X86Reg dst_reg);
void x86_emit_mov_reg_reg(X86InstrList *list, int size, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_mov_imm_reg(X86InstrList *list, int size, int64_t imm, X86Reg dst_reg);
void x86_emit_mov_mem_reg(X86InstrList *list, int size, X86Reg base_reg, int32_t offset, X86Reg dst_reg);
void x86_emit_mov_reg_mem(X86InstrList *list, int size, X86Reg src_reg, X86Reg base_reg, int32_t offset);
void x86_emit_mov_rip_reg(X86InstrList *list, const char *symbol, X86Reg dst_reg);
void x86_emit_movb_imm(X86InstrList *list, uint8_t imm, X86Reg dst_reg);
void x86_emit_movslq(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_movzbl(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_cmove(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_add(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_sub(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_imul(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_idiv(X86InstrList *list, X86Reg src_reg);
void x86_emit_cltd(X86InstrList *list);
void x86_emit_neg(X86InstrList *list, X86Reg dst_reg);
void x86_emit_and(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_or(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_xor(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_cmp(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_cmp_imm(X86InstrList *list, int32_t imm, X86Reg dst_reg);
void x86_emit_setcc(X86InstrList *list, X86CondCode cond, X86Reg dst_reg);
void x86_emit_andb(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_orb(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_jmp(X86InstrList *list, const char *label);
void x86_emit_jne(X86InstrList *list, const char *label);
void x86_emit_je(X86InstrList *list, const char *label);
void x86_emit_call(X86InstrList *list, const char *symbol);

/* SSE2 Floating-point emission helpers */
void x86_emit_movsd_reg_reg(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_movsd_mem_reg(X86InstrList *list, X86Reg base_reg, int32_t offset, X86Reg dst_reg);
void x86_emit_movsd_reg_mem(X86InstrList *list, X86Reg src_reg, X86Reg base_reg, int32_t offset);
void x86_emit_movsd_rip_reg(X86InstrList *list, const char *symbol, X86Reg dst_reg);
void x86_emit_addsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_subsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_mulsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_divsd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_xorpd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);
void x86_emit_ucomisd(X86InstrList *list, X86Reg src_reg, X86Reg dst_reg);

/* Optimization & Serialization */
void x86_instr_list_optimize(X86InstrList *list);
char *x86_instr_list_to_asm(X86InstrList *list);

#endif /* X86_64_INSTR_H */
