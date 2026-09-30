// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_encode.h"
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Byte Buffer Implementation                                                */

ByteBuffer *byte_buf_create(void) {
    ByteBuffer *b = (ByteBuffer *)malloc(sizeof(ByteBuffer));
    if (!b) return NULL;
    b->capacity = 4096;
    b->size = 0;
    b->data = (uint8_t *)malloc(b->capacity);
    return b;
}

void byte_buf_free(ByteBuffer *b) {
    if (!b) return;
    if (b->data) free(b->data);
    free(b);
}

static void byte_buf_ensure_space(ByteBuffer *b, size_t needed) {
    if (b->size + needed > b->capacity) {
        while (b->size + needed > b->capacity) {
            b->capacity *= 2;
        }
        b->data = (uint8_t *)realloc(b->data, b->capacity);
    }
}

void byte_buf_append_u8(ByteBuffer *b, uint8_t byte) {
    if (!b) return;
    byte_buf_ensure_space(b, 1);
    b->data[b->size++] = byte;
}

void byte_buf_append_u16(ByteBuffer *b, uint16_t val) {
    if (!b) return;
    byte_buf_ensure_space(b, 2);
    b->data[b->size++] = (uint8_t)(val & 0xFF);
    b->data[b->size++] = (uint8_t)((val >> 8) & 0xFF);
}

void byte_buf_append_u32(ByteBuffer *b, uint32_t val) {
    if (!b) return;
    byte_buf_ensure_space(b, 4);
    b->data[b->size++] = (uint8_t)(val & 0xFF);
    b->data[b->size++] = (uint8_t)((val >> 8) & 0xFF);
    b->data[b->size++] = (uint8_t)((val >> 16) & 0xFF);
    b->data[b->size++] = (uint8_t)((val >> 24) & 0xFF);
}

void byte_buf_append_u64(ByteBuffer *b, uint64_t val) {
    if (!b) return;
    byte_buf_ensure_space(b, 8);
    for (int i = 0; i < 8; i++) {
        b->data[b->size++] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }
}

void byte_buf_append_bytes(ByteBuffer *b, const void *bytes, size_t len) {
    if (!b || !bytes || len == 0) return;
    byte_buf_ensure_space(b, len);
    memcpy(b->data + b->size, bytes, len);
    b->size += len;
}

/* Relocation List Implementation                                            */

X86RelocList *x86_reloc_list_create(void) {
    X86RelocList *list = (X86RelocList *)malloc(sizeof(X86RelocList));
    if (!list) return NULL;
    list->capacity = 64;
    list->count = 0;
    list->items = (X86Reloc *)malloc(sizeof(X86Reloc) * list->capacity);
    return list;
}

void x86_reloc_list_free(X86RelocList *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].symbol) free(list->items[i].symbol);
    }
    if (list->items) free(list->items);
    free(list);
}

void x86_reloc_list_append(X86RelocList *list, uint64_t offset, const char *symbol, uint32_t type, int64_t addend) {
    if (!list) return;
    if (list->count + 1 > list->capacity) {
        list->capacity *= 2;
        list->items = (X86Reloc *)realloc(list->items, sizeof(X86Reloc) * list->capacity);
    }
    X86Reloc *r = &list->items[list->count++];
    r->offset = offset;
    r->symbol = symbol ? strdup(symbol) : NULL;
    r->type = type;
    r->addend = addend;
}

/* Label Map for 2-Pass Resolution                                           */

typedef struct {
    char *name;
    uint64_t offset;
} LabelEntry;

typedef struct {
    LabelEntry *entries;
    size_t count;
    size_t capacity;
} LabelMap;

static LabelMap *label_map_create(void) {
    LabelMap *m = (LabelMap *)malloc(sizeof(LabelMap));
    m->capacity = 64;
    m->count = 0;
    m->entries = (LabelEntry *)malloc(sizeof(LabelEntry) * m->capacity);
    return m;
}

static void label_map_free(LabelMap *m) {
    if (!m) return;
    for (size_t i = 0; i < m->count; i++) {
        if (m->entries[i].name) free(m->entries[i].name);
    }
    if (m->entries) free(m->entries);
    free(m);
}

static void label_map_put(LabelMap *m, const char *name, uint64_t offset) {
    if (!m || !name) return;
    if (m->count + 1 > m->capacity) {
        m->capacity *= 2;
        m->entries = (LabelEntry *)realloc(m->entries, sizeof(LabelEntry) * m->capacity);
    }
    m->entries[m->count].name = strdup(name);
    m->entries[m->count].offset = offset;
    m->count++;
}

static bool label_map_get(LabelMap *m, const char *name, uint64_t *out_offset) {
    if (!m || !name) return false;
    for (size_t i = 0; i < m->count; i++) {
        if (strcmp(m->entries[i].name, name) == 0) {
            if (out_offset) *out_offset = m->entries[i].offset;
            return true;
        }
    }
    return false;
}

/* Low-Level x86-64 Encoding Helpers                                         */

static inline uint8_t reg_hw_id(X86Reg r) {
    if (r >= X86_REG_XMM0 && r <= X86_REG_XMM15) {
        return (uint8_t)(r - X86_REG_XMM0);
    }
    if (r >= X86_REG_RAX && r <= X86_REG_R15) {
        return (uint8_t)r;
    }
    return 0;
}

static void emit_rex_prefix(ByteBuffer *b, bool w, bool r, bool x, bool b_bit, size_t *out_len) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b_bit) rex |= 0x01;
    if (rex != 0x40 || w) {
        if (b) byte_buf_append_u8(b, rex);
        if (out_len) (*out_len)++;
    }
}

static void emit_modrm_sib_disp(ByteBuffer *b, uint8_t reg_3bit, X86Reg base_reg, int64_t disp, size_t *out_len) {
    uint8_t base_hw = reg_hw_id(base_reg);
    uint8_t rm_3bit = base_hw & 7;

    if (rm_3bit == 4) { /* RSP or R12 requires SIB byte */
        if (disp >= -128 && disp <= 127) {
            uint8_t modrm = (1 << 6) | ((reg_3bit & 7) << 3) | 4;
            if (b) {
                byte_buf_append_u8(b, modrm);
                byte_buf_append_u8(b, 0x24); /* SIB */
                byte_buf_append_u8(b, (uint8_t)(int8_t)disp);
            }
            if (out_len) (*out_len) += 3;
        } else {
            uint8_t modrm = (2 << 6) | ((reg_3bit & 7) << 3) | 4;
            if (b) {
                byte_buf_append_u8(b, modrm);
                byte_buf_append_u8(b, 0x24); /* SIB */
                byte_buf_append_u32(b, (uint32_t)(int32_t)disp);
            }
            if (out_len) (*out_len) += 6;
        }
    } else if (rm_3bit == 5) { /* RBP or R13 */
        if (disp >= -128 && disp <= 127) {
            uint8_t modrm = (1 << 6) | ((reg_3bit & 7) << 3) | 5;
            if (b) {
                byte_buf_append_u8(b, modrm);
                byte_buf_append_u8(b, (uint8_t)(int8_t)disp);
            }
            if (out_len) (*out_len) += 2;
        } else {
            uint8_t modrm = (2 << 6) | ((reg_3bit & 7) << 3) | 5;
            if (b) {
                byte_buf_append_u8(b, modrm);
                byte_buf_append_u32(b, (uint32_t)(int32_t)disp);
            }
            if (out_len) (*out_len) += 5;
        }
    } else { /* Standard GPR base (RAX, RCX, RDX, RBX, RSI, RDI, etc.) */
        if (disp == 0) {
            uint8_t modrm = (0 << 6) | ((reg_3bit & 7) << 3) | rm_3bit;
            if (b) byte_buf_append_u8(b, modrm);
            if (out_len) (*out_len) += 1;
        } else if (disp >= -128 && disp <= 127) {
            uint8_t modrm = (1 << 6) | ((reg_3bit & 7) << 3) | rm_3bit;
            if (b) {
                byte_buf_append_u8(b, modrm);
                byte_buf_append_u8(b, (uint8_t)(int8_t)disp);
            }
            if (out_len) (*out_len) += 2;
        } else {
            uint8_t modrm = (2 << 6) | ((reg_3bit & 7) << 3) | rm_3bit;
            if (b) {
                byte_buf_append_u8(b, modrm);
                byte_buf_append_u32(b, (uint32_t)(int32_t)disp);
            }
            if (out_len) (*out_len) += 5;
        }
    }
}

static uint8_t cond_to_opcode_byte(X86CondCode cond) {
    switch (cond) {
        case X86_CC_E:   return 0x94;
        case X86_CC_NE:  return 0x95;
        case X86_CC_L:   return 0x9C;
        case X86_CC_GE:  return 0x9D;
        case X86_CC_LE:  return 0x9E;
        case X86_CC_G:   return 0x9F;
        case X86_CC_B:   return 0x92;
        case X86_CC_AE:  return 0x93;
        case X86_CC_BE:  return 0x96;
        case X86_CC_A:   return 0x97;
        case X86_CC_P:   return 0x9A;
        case X86_CC_NP:  return 0x9B;
    }
    return 0x94;
}

/* Core Instruction Encoder                                                  */

static size_t encode_single_instruction_internal(X86Instr *inst,
                                                 ByteBuffer *b,
                                                 X86RelocList *relocs,
                                                 LabelMap *map,
                                                 uint64_t current_pc) {
    size_t len = 0;

    switch (inst->op) {
        case X86_OP_LABEL:
            return 0;

        case X86_OP_PUSH: {
            uint8_t hw = reg_hw_id(inst->dst.reg);
            if (hw >= 8) {
                if (b) {
                    byte_buf_append_u8(b, 0x41);
                    byte_buf_append_u8(b, 0x50 + (hw & 7));
                }
                len += 2;
            } else {
                if (b) byte_buf_append_u8(b, 0x50 + hw);
                len += 1;
            }
            break;
        }

        case X86_OP_POP: {
            uint8_t hw = reg_hw_id(inst->dst.reg);
            if (hw >= 8) {
                if (b) {
                    byte_buf_append_u8(b, 0x41);
                    byte_buf_append_u8(b, 0x58 + (hw & 7));
                }
                len += 2;
            } else {
                if (b) byte_buf_append_u8(b, 0x58 + hw);
                len += 1;
            }
            break;
        }

        case X86_OP_RET:
            if (b) byte_buf_append_u8(b, 0xC3);
            len += 1;
            break;

        case X86_OP_SUB_IMM: {
            int64_t imm = inst->src.imm;
            if (imm >= -128 && imm <= 127) {
                if (b) {
                    byte_buf_append_u8(b, 0x48);
                    byte_buf_append_u8(b, 0x83);
                    byte_buf_append_u8(b, 0xEC);
                    byte_buf_append_u8(b, (uint8_t)(int8_t)imm);
                }
                len += 4;
            } else {
                if (b) {
                    byte_buf_append_u8(b, 0x48);
                    byte_buf_append_u8(b, 0x81);
                    byte_buf_append_u8(b, 0xEC);
                    byte_buf_append_u32(b, (uint32_t)(int32_t)imm);
                }
                len += 7;
            }
            break;
        }

        case X86_OP_ADD_IMM: {
            int64_t imm = inst->src.imm;
            if (imm >= -128 && imm <= 127) {
                if (b) {
                    byte_buf_append_u8(b, 0x48);
                    byte_buf_append_u8(b, 0x83);
                    byte_buf_append_u8(b, 0xC4);
                    byte_buf_append_u8(b, (uint8_t)(int8_t)imm);
                }
                len += 4;
            } else {
                if (b) {
                    byte_buf_append_u8(b, 0x48);
                    byte_buf_append_u8(b, 0x81);
                    byte_buf_append_u8(b, 0xC4);
                    byte_buf_append_u32(b, (uint32_t)(int32_t)imm);
                }
                len += 7;
            }
            break;
        }

        case X86_OP_LEA: {
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            if (inst->src.kind == X86_OPERAND_MEM_BASE) {
                uint8_t base_hw = reg_hw_id(inst->src.reg);
                emit_rex_prefix(b, true, dst_hw >= 8, false, base_hw >= 8, &len);
                if (b) byte_buf_append_u8(b, 0x8D);
                len += 1;
                emit_modrm_sib_disp(b, dst_hw & 7, inst->src.reg, inst->src.imm, &len);
            } else if (inst->src.kind == X86_OPERAND_MEM_RIP) {
                emit_rex_prefix(b, true, dst_hw >= 8, false, false, &len);
                if (b) {
                    byte_buf_append_u8(b, 0x8D);
                    byte_buf_append_u8(b, (0 << 6) | ((dst_hw & 7) << 3) | 5);
                    if (relocs) {
                        x86_reloc_list_append(relocs, current_pc + len + 2, inst->src.symbol, R_X86_64_PC32, -4);
                    }
                    byte_buf_append_u32(b, 0);
                }
                len += 6;
            }
            break;
        }

        case X86_OP_MOV: {
            bool is_64 = (inst->dst.size == 8 || inst->src.size == 8);

            if (inst->src.kind == X86_OPERAND_IMM) {
                uint8_t dst_hw = reg_hw_id(inst->dst.reg);
                int64_t imm = inst->src.imm;

                if (!is_64) {
                    if (dst_hw >= 8) {
                        if (b) byte_buf_append_u8(b, 0x41);
                        len += 1;
                    }
                    if (b) {
                        byte_buf_append_u8(b, 0xB8 + (dst_hw & 7));
                        byte_buf_append_u32(b, (uint32_t)imm);
                    }
                    len += 5;
                } else {
                    if (imm >= -2147483648LL && imm <= 2147483647LL) {
                        /* 48 C7 /0 imm32 */
                        emit_rex_prefix(b, true, false, false, dst_hw >= 8, &len);
                        if (b) {
                            byte_buf_append_u8(b, 0xC7);
                            byte_buf_append_u8(b, 0xC0 | (dst_hw & 7));
                            byte_buf_append_u32(b, (uint32_t)(int32_t)imm);
                        }
                        len += 6;
                    } else {
                        /* 64-bit movabs */
                        emit_rex_prefix(b, true, false, false, dst_hw >= 8, &len);
                        if (b) {
                            byte_buf_append_u8(b, 0xB8 + (dst_hw & 7));
                            byte_buf_append_u64(b, (uint64_t)imm);
                        }
                        len += 9;
                    }
                }
            } else if (inst->src.kind == X86_OPERAND_REG && inst->dst.kind == X86_OPERAND_REG) {
                uint8_t src_hw = reg_hw_id(inst->src.reg);
                uint8_t dst_hw = reg_hw_id(inst->dst.reg);
                emit_rex_prefix(b, is_64, src_hw >= 8, false, dst_hw >= 8, &len);
                if (b) {
                    byte_buf_append_u8(b, 0x89);
                    byte_buf_append_u8(b, 0xC0 | ((src_hw & 7) << 3) | (dst_hw & 7));
                }
                len += 2;
            } else if (inst->src.kind == X86_OPERAND_MEM_BASE) {
                uint8_t dst_hw = reg_hw_id(inst->dst.reg);
                uint8_t base_hw = reg_hw_id(inst->src.reg);
                emit_rex_prefix(b, is_64, dst_hw >= 8, false, base_hw >= 8, &len);
                if (b) byte_buf_append_u8(b, 0x8B);
                len += 1;
                emit_modrm_sib_disp(b, dst_hw & 7, inst->src.reg, inst->src.imm, &len);
            } else if (inst->dst.kind == X86_OPERAND_MEM_BASE) {
                uint8_t src_hw = reg_hw_id(inst->src.reg);
                uint8_t base_hw = reg_hw_id(inst->dst.reg);
                emit_rex_prefix(b, is_64, src_hw >= 8, false, base_hw >= 8, &len);
                if (b) byte_buf_append_u8(b, 0x89);
                len += 1;
                emit_modrm_sib_disp(b, src_hw & 7, inst->dst.reg, inst->dst.imm, &len);
            } else if (inst->src.kind == X86_OPERAND_MEM_RIP) {
                uint8_t dst_hw = reg_hw_id(inst->dst.reg);
                emit_rex_prefix(b, true, dst_hw >= 8, false, false, &len);
                if (b) {
                    byte_buf_append_u8(b, 0x8B);
                    byte_buf_append_u8(b, (0 << 6) | ((dst_hw & 7) << 3) | 5);
                    if (relocs) {
                        x86_reloc_list_append(relocs, current_pc + len + 2, inst->src.symbol, R_X86_64_PC32, -4);
                    }
                    byte_buf_append_u32(b, 0);
                }
                len += 6;
            }
            break;
        }

        case X86_OP_MOVB_IMM: {
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            if (dst_hw >= 8) {
                if (b) byte_buf_append_u8(b, 0x41);
                len += 1;
            }
            if (b) {
                byte_buf_append_u8(b, 0xB0 + (dst_hw & 7));
                byte_buf_append_u8(b, (uint8_t)inst->src.imm);
            }
            len += 2;
            break;
        }

        case X86_OP_MOVSLQ: {
            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, true, dst_hw >= 8, false, src_hw >= 8, &len);
            if (b) {
                byte_buf_append_u8(b, 0x63);
                byte_buf_append_u8(b, 0xC0 | ((dst_hw & 7) << 3) | (src_hw & 7));
            }
            len += 2;
            break;
        }

        case X86_OP_MOVZBL: {
            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, dst_hw >= 8, false, src_hw >= 8, &len);
            if (b) {
                byte_buf_append_u8(b, 0x0F);
                byte_buf_append_u8(b, 0xB6);
                byte_buf_append_u8(b, 0xC0 | ((dst_hw & 7) << 3) | (src_hw & 7));
            }
            len += 3;
            break;
        }

        case X86_OP_CMOVE: {
            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, true, dst_hw >= 8, false, src_hw >= 8, &len);
            if (b) {
                byte_buf_append_u8(b, 0x0F);
                byte_buf_append_u8(b, 0x44);
                byte_buf_append_u8(b, 0xC0 | ((dst_hw & 7) << 3) | (src_hw & 7));
            }
            len += 3;
            break;
        }

        case X86_OP_ADD:
        case X86_OP_SUB:
        case X86_OP_AND:
        case X86_OP_OR:
        case X86_OP_XOR:
        case X86_OP_CMP: {
            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, src_hw >= 8, false, dst_hw >= 8, &len);

            uint8_t opc = 0x01;
            if (inst->op == X86_OP_SUB) opc = 0x29;
            else if (inst->op == X86_OP_AND) opc = 0x21;
            else if (inst->op == X86_OP_OR)  opc = 0x09;
            else if (inst->op == X86_OP_XOR) opc = 0x31;
            else if (inst->op == X86_OP_CMP) opc = 0x39;

            if (b) {
                byte_buf_append_u8(b, opc);
                byte_buf_append_u8(b, 0xC0 | ((src_hw & 7) << 3) | (dst_hw & 7));
            }
            len += 2;
            break;
        }

        case X86_OP_IMUL: {
            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, dst_hw >= 8, false, src_hw >= 8, &len);
            if (b) {
                byte_buf_append_u8(b, 0x0F);
                byte_buf_append_u8(b, 0xAF);
                byte_buf_append_u8(b, 0xC0 | ((dst_hw & 7) << 3) | (src_hw & 7));
            }
            len += 3;
            break;
        }

        case X86_OP_IDIV: {
            uint8_t src_hw = reg_hw_id(inst->src.reg);
            emit_rex_prefix(b, false, false, false, src_hw >= 8, &len);
            if (b) {
                byte_buf_append_u8(b, 0xF7);
                byte_buf_append_u8(b, 0xC0 | (7 << 3) | (src_hw & 7));
            }
            len += 2;
            break;
        }

        case X86_OP_CLTD:
            if (b) byte_buf_append_u8(b, 0x99);
            len += 1;
            break;

        case X86_OP_NEG: {
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, false, false, dst_hw >= 8, &len);
            if (b) {
                byte_buf_append_u8(b, 0xF7);
                byte_buf_append_u8(b, 0xC0 | (3 << 3) | (dst_hw & 7));
            }
            len += 2;
            break;
        }

        case X86_OP_CMP_IMM: {
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, false, false, dst_hw >= 8, &len);
            int64_t imm = inst->src.imm;

            if (imm >= -128 && imm <= 127) {
                if (b) {
                    byte_buf_append_u8(b, 0x83);
                    byte_buf_append_u8(b, 0xC0 | (7 << 3) | (dst_hw & 7));
                    byte_buf_append_u8(b, (uint8_t)(int8_t)imm);
                }
                len += 3;
            } else {
                if (b) {
                    byte_buf_append_u8(b, 0x81);
                    byte_buf_append_u8(b, 0xC0 | (7 << 3) | (dst_hw & 7));
                    byte_buf_append_u32(b, (uint32_t)(int32_t)imm);
                }
                len += 6;
            }
            break;
        }

        case X86_OP_SETCC: {
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, false, false, dst_hw >= 8, &len);
            if (b) {
                byte_buf_append_u8(b, 0x0F);
                byte_buf_append_u8(b, cond_to_opcode_byte(inst->cond));
                byte_buf_append_u8(b, 0xC0 | (0 << 3) | (dst_hw & 7));
            }
            len += 3;
            break;
        }

        case X86_OP_ANDB:
        case X86_OP_ORB: {
            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, src_hw >= 8, false, dst_hw >= 8, &len);
            uint8_t opc = (inst->op == X86_OP_ANDB) ? 0x20 : 0x08;
            if (b) {
                byte_buf_append_u8(b, opc);
                byte_buf_append_u8(b, 0xC0 | ((src_hw & 7) << 3) | (dst_hw & 7));
            }
            len += 2;
            break;
        }

        case X86_OP_JMP: {
            if (b) {
                byte_buf_append_u8(b, 0xE9);
                uint64_t target_pc = 0;
                int32_t disp = 0;
                if (map && label_map_get(map, inst->src.symbol, &target_pc)) {
                    disp = (int32_t)(target_pc - (current_pc + 5));
                }
                byte_buf_append_u32(b, (uint32_t)disp);
            }
            len += 5;
            break;
        }

        case X86_OP_JNE:
        case X86_OP_JE: {
            if (b) {
                byte_buf_append_u8(b, 0x0F);
                byte_buf_append_u8(b, (inst->op == X86_OP_JNE) ? 0x85 : 0x84);
                uint64_t target_pc = 0;
                int32_t disp = 0;
                if (map && label_map_get(map, inst->src.symbol, &target_pc)) {
                    disp = (int32_t)(target_pc - (current_pc + 6));
                }
                byte_buf_append_u32(b, (uint32_t)disp);
            }
            len += 6;
            break;
        }

        case X86_OP_CALL: {
            if (b) {
                byte_buf_append_u8(b, 0xE8);
                if (relocs) {
                    x86_reloc_list_append(relocs, current_pc + 1, inst->src.symbol, R_X86_64_PLT32, -4);
                }
                byte_buf_append_u32(b, 0);
            }
            len += 5;
            break;
        }

        /* SSE2 Instructions */
        case X86_OP_MOVSD: {
            if (b) byte_buf_append_u8(b, 0xF2);
            len += 1;

            if (inst->src.kind == X86_OPERAND_REG && inst->dst.kind == X86_OPERAND_REG) {
                uint8_t src_hw = reg_hw_id(inst->src.reg);
                uint8_t dst_hw = reg_hw_id(inst->dst.reg);
                emit_rex_prefix(b, false, dst_hw >= 8, false, src_hw >= 8, &len);
                if (b) {
                    byte_buf_append_u8(b, 0x0F);
                    byte_buf_append_u8(b, 0x10);
                    byte_buf_append_u8(b, 0xC0 | ((dst_hw & 7) << 3) | (src_hw & 7));
                }
                len += 3;
            } else if (inst->src.kind == X86_OPERAND_MEM_BASE) {
                uint8_t dst_hw = reg_hw_id(inst->dst.reg);
                uint8_t base_hw = reg_hw_id(inst->src.reg);
                emit_rex_prefix(b, false, dst_hw >= 8, false, base_hw >= 8, &len);
                if (b) {
                    byte_buf_append_u8(b, 0x0F);
                    byte_buf_append_u8(b, 0x10);
                }
                len += 2;
                emit_modrm_sib_disp(b, dst_hw & 7, inst->src.reg, inst->src.imm, &len);
            } else if (inst->dst.kind == X86_OPERAND_MEM_BASE) {
                uint8_t src_hw = reg_hw_id(inst->src.reg);
                uint8_t base_hw = reg_hw_id(inst->dst.reg);
                emit_rex_prefix(b, false, src_hw >= 8, false, base_hw >= 8, &len);
                if (b) {
                    byte_buf_append_u8(b, 0x0F);
                    byte_buf_append_u8(b, 0x11);
                }
                len += 2;
                emit_modrm_sib_disp(b, src_hw & 7, inst->dst.reg, inst->dst.imm, &len);
            } else if (inst->src.kind == X86_OPERAND_MEM_RIP) {
                uint8_t dst_hw = reg_hw_id(inst->dst.reg);
                emit_rex_prefix(b, false, dst_hw >= 8, false, false, &len);
                if (b) {
                    byte_buf_append_u8(b, 0x0F);
                    byte_buf_append_u8(b, 0x10);
                    byte_buf_append_u8(b, (0 << 6) | ((dst_hw & 7) << 3) | 5);
                    if (relocs) {
                        x86_reloc_list_append(relocs, current_pc + len + 3, inst->src.symbol, R_X86_64_PC32, -4);
                    }
                    byte_buf_append_u32(b, 0);
                }
                len += 7;
            }
            break;
        }

        case X86_OP_ADDSD:
        case X86_OP_SUBSD:
        case X86_OP_MULSD:
        case X86_OP_DIVSD: {
            if (b) byte_buf_append_u8(b, 0xF2);
            len += 1;

            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, dst_hw >= 8, false, src_hw >= 8, &len);

            uint8_t opc = 0x58;
            if (inst->op == X86_OP_SUBSD) opc = 0x5C;
            else if (inst->op == X86_OP_MULSD) opc = 0x59;
            else if (inst->op == X86_OP_DIVSD) opc = 0x5E;

            if (b) {
                byte_buf_append_u8(b, 0x0F);
                byte_buf_append_u8(b, opc);
                byte_buf_append_u8(b, 0xC0 | ((dst_hw & 7) << 3) | (src_hw & 7));
            }
            len += 3;
            break;
        }

        case X86_OP_XORPD:
        case X86_OP_UCOMISD: {
            if (b) byte_buf_append_u8(b, 0x66);
            len += 1;

            uint8_t src_hw = reg_hw_id(inst->src.reg);
            uint8_t dst_hw = reg_hw_id(inst->dst.reg);
            emit_rex_prefix(b, false, dst_hw >= 8, false, src_hw >= 8, &len);

            uint8_t opc = (inst->op == X86_OP_XORPD) ? 0x57 : 0x2E;

            if (b) {
                byte_buf_append_u8(b, 0x0F);
                byte_buf_append_u8(b, opc);
                byte_buf_append_u8(b, 0xC0 | ((dst_hw & 7) << 3) | (src_hw & 7));
            }
            len += 3;
            break;
        }
    }

    return len;
}

size_t x86_encode_single_instruction(X86Instr *inst, uint8_t *out_buf, size_t max_len, X86Reloc *out_reloc) {
    if (!inst) return 0;
    ByteBuffer *b = byte_buf_create();
    X86RelocList *relocs = out_reloc ? x86_reloc_list_create() : NULL;

    size_t len = encode_single_instruction_internal(inst, b, relocs, NULL, 0);
    if (len <= max_len && out_buf) {
        memcpy(out_buf, b->data, len);
    }
    if (out_reloc && relocs && relocs->count > 0) {
        out_reloc->offset = relocs->items[0].offset;
        out_reloc->symbol = relocs->items[0].symbol ? strdup(relocs->items[0].symbol) : NULL;
        out_reloc->type = relocs->items[0].type;
        out_reloc->addend = relocs->items[0].addend;
    }
    if (relocs) x86_reloc_list_free(relocs);
    byte_buf_free(b);
    return len;
}

bool x86_encode_function(X86InstrList *list, ByteBuffer *out_code, X86RelocList *out_relocs, char **out_error) {
    if (!list || !out_code) {
        if (out_error) *out_error = strdup("null arguments to x86_encode_function");
        return false;
    }

    LabelMap *map = label_map_create();
    uint64_t pc = out_code->size;

    /* Pass 1: Measure offsets and record label locations */
    for (X86Instr *inst = list->first; inst; inst = inst->next) {
        if (inst->op == X86_OP_LABEL) {
            label_map_put(map, inst->dst.symbol, pc);
        } else {
            size_t ilen = encode_single_instruction_internal(inst, NULL, NULL, NULL, pc);
            pc += ilen;
        }
    }

    /* Pass 2: Encode instructions into output buffer */
    pc = out_code->size;
    for (X86Instr *inst = list->first; inst; inst = inst->next) {
        if (inst->op == X86_OP_LABEL) continue;
        size_t before_sz = out_code->size;
        encode_single_instruction_internal(inst, out_code, out_relocs, map, pc);
        size_t written = out_code->size - before_sz;
        pc += written;
    }

    label_map_free(map);
    return true;
}
