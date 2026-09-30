// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X86_64_ELF_H
#define X86_64_ELF_H

#include "x86_64_encode.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ELF Symbol Description                                                    */

typedef struct {
    char *name;
    uint64_t text_offset;
    uint64_t size;
    bool is_global;
    bool is_defined;       /* false for external undefined symbols like printf */
} ElfSymbolDesc;

typedef struct {
    ElfSymbolDesc *items;
    size_t count;
    size_t capacity;
} ElfSymbolList;

ElfSymbolList *elf_symbol_list_create(void);
void elf_symbol_list_free(ElfSymbolList *list);
void elf_symbol_list_add(ElfSymbolList *list, const char *name, uint64_t offset, uint64_t size, bool is_global, bool is_defined);

/* ELF64 Object Writer                                                       */

/* Writes a complete Linux ELF64 relocatable object file (ET_REL) to out_path */
bool elf64_write_object_file(const char *out_path,
                             const char *source_filename,
                             ByteBuffer *text_sec,
                             X86RelocList *text_relocs,
                             ByteBuffer *rodata_sec,
                             ElfSymbolList *symbols,
                             char **out_error);

#endif /* X86_64_ELF_H */
