// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X86_64_ENCODE_H
#define X86_64_ENCODE_H

#include "x86_64_instr.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Byte Buffer                                                               */

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} ByteBuffer;

ByteBuffer *byte_buf_create(void);
void byte_buf_free(ByteBuffer *b);
void byte_buf_append_u8(ByteBuffer *b, uint8_t byte);
void byte_buf_append_u16(ByteBuffer *b, uint16_t val);
void byte_buf_append_u32(ByteBuffer *b, uint32_t val);
void byte_buf_append_u64(ByteBuffer *b, uint64_t val);
void byte_buf_append_bytes(ByteBuffer *b, const void *bytes, size_t len);

/* Relocation Record and List                                                */

typedef struct {
    uint64_t offset;       /* Offset in .text where the 32-bit field begins */
    char *symbol;          /* Symbol name or section name */
    uint32_t type;         /* R_X86_64_PLT32 or R_X86_64_PC32 */
    int64_t addend;        /* Relocation addend (e.g. -4) */
} X86Reloc;

typedef struct {
    X86Reloc *items;
    size_t count;
    size_t capacity;
} X86RelocList;

X86RelocList *x86_reloc_list_create(void);
void x86_reloc_list_free(X86RelocList *list);
void x86_reloc_list_append(X86RelocList *list, uint64_t offset, const char *symbol, uint32_t type, int64_t addend);

/* Instruction and Function Encoders                                         */

/* Encodes a single instruction into a buffer without branch resolution (used for unit testing).
 * Returns the number of bytes encoded, or 0 on failure.
 */
size_t x86_encode_single_instruction(X86Instr *inst, uint8_t *out_buf, size_t max_len, X86Reloc *out_reloc);

/* Encodes a full function instruction list, performing 2-pass label resolution.
 * Appends machine code bytes to out_code and relocations to out_relocs.
 */
bool x86_encode_function(X86InstrList *list, ByteBuffer *out_code, X86RelocList *out_relocs, char **out_error);

#endif /* X86_64_ENCODE_H */
