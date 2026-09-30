// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_elf.h"
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Symbol List Implementation                                                */

ElfSymbolList *elf_symbol_list_create(void) {
    ElfSymbolList *list = (ElfSymbolList *)malloc(sizeof(ElfSymbolList));
    if (!list) return NULL;
    list->capacity = 64;
    list->count = 0;
    list->items = (ElfSymbolDesc *)malloc(sizeof(ElfSymbolDesc) * list->capacity);
    return list;
}

void elf_symbol_list_free(ElfSymbolList *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].name) free(list->items[i].name);
    }
    if (list->items) free(list->items);
    free(list);
}

void elf_symbol_list_add(ElfSymbolList *list, const char *name, uint64_t offset, uint64_t size, bool is_global, bool is_defined) {
    if (!list || !name) return;
    /* Avoid duplicate undefined symbols */
    if (!is_defined) {
        for (size_t i = 0; i < list->count; i++) {
            if (!list->items[i].is_defined && strcmp(list->items[i].name, name) == 0) {
                return;
            }
        }
    }
    if (list->count + 1 > list->capacity) {
        list->capacity *= 2;
        list->items = (ElfSymbolDesc *)realloc(list->items, sizeof(ElfSymbolDesc) * list->capacity);
    }
    ElfSymbolDesc *desc = &list->items[list->count++];
    desc->name = strdup(name);
    desc->text_offset = offset;
    desc->size = size;
    desc->is_global = is_global;
    desc->is_defined = is_defined;
}

/* String Table Builder Helper                                               */

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} StrTable;

static StrTable *str_table_create(void) {
    StrTable *t = (StrTable *)malloc(sizeof(StrTable));
    t->capacity = 256;
    t->size = 1; /* Offset 0 is always null byte */
    t->data = (char *)malloc(t->capacity);
    t->data[0] = '\0';
    return t;
}

static void str_table_free(StrTable *t) {
    if (!t) return;
    if (t->data) free(t->data);
    free(t);
}

static uint32_t str_table_add(StrTable *t, const char *str) {
    if (!str || str[0] == '\0') return 0;
    size_t len = strlen(str);
    /* Check if already present */
    for (size_t i = 1; i + len < t->size; i++) {
        if (strcmp(t->data + i, str) == 0) {
            return (uint32_t)i;
        }
    }
    if (t->size + len + 1 > t->capacity) {
        while (t->size + len + 1 > t->capacity) t->capacity *= 2;
        t->data = (char *)realloc(t->data, t->capacity);
    }
    uint32_t offset = (uint32_t)t->size;
    memcpy(t->data + offset, str, len + 1);
    t->size += len + 1;
    return offset;
}

/* Section Layout Helper                                                     */

static inline uint64_t align_offset(uint64_t offset, uint64_t align) {
    if (align <= 1) return offset;
    return (offset + align - 1) & ~(align - 1);
}

/* ELF64 Object Writer                                                       */

bool elf64_write_object_file(const char *out_path,
                             const char *source_filename,
                             ByteBuffer *text_sec,
                             X86RelocList *text_relocs,
                             ByteBuffer *rodata_sec,
                             ElfSymbolList *symbols,
                             char **out_error) {
    if (!out_path) {
        if (out_error) *out_error = strdup("null output path for ELF object file");
        return false;
    }

    /* 1. Build string tables */
    StrTable *shstrtab = str_table_create();
    StrTable *strtab = str_table_create();

    uint32_t sh_name_text = str_table_add(shstrtab, ".text");
    uint32_t sh_name_relatext = str_table_add(shstrtab, ".rela.text");
    uint32_t sh_name_rodata = str_table_add(shstrtab, ".rodata");
    uint32_t sh_name_symtab = str_table_add(shstrtab, ".symtab");
    uint32_t sh_name_strtab = str_table_add(shstrtab, ".strtab");
    uint32_t sh_name_shstrtab = str_table_add(shstrtab, ".shstrtab");
    uint32_t sh_name_gnustack = str_table_add(shstrtab, ".note.GNU-stack");

    /* 2. Build .symtab entries */
    /* Symbol layout:
     * Index 0: STN_UNDEF (all zeros)
     * Index 1: STT_FILE (source_filename)
     * Index 2: STT_SECTION for .rodata (section 3)
     * First global symbol index = 3
     * Defined functions (STT_FUNC)
     * Undefined externals (STT_NOTYPE, SHN_UNDEF)
     */
    size_t defined_func_count = 0;
    size_t undef_sym_count = 0;
    if (symbols) {
        for (size_t i = 0; i < symbols->count; i++) {
            if (symbols->items[i].is_defined) defined_func_count++;
            else undef_sym_count++;
        }
    }

    size_t total_syms = 3 + defined_func_count + undef_sym_count;
    Elf64_Sym *symtab_entries = (Elf64_Sym *)calloc(total_syms, sizeof(Elf64_Sym));

    /* Sym 1: FILE */
    uint32_t file_str_idx = str_table_add(strtab, source_filename ? source_filename : "program.cco");
    symtab_entries[1].st_name = file_str_idx;
    symtab_entries[1].st_info = ELF64_ST_INFO(STB_LOCAL, STT_FILE);
    symtab_entries[1].st_other = STV_DEFAULT;
    symtab_entries[1].st_shndx = SHN_ABS;

    /* Sym 2: SECTION .rodata (Section index 3) */
    symtab_entries[2].st_name = 0;
    symtab_entries[2].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
    symtab_entries[2].st_other = STV_DEFAULT;
    symtab_entries[2].st_shndx = 3; /* .rodata section index */

    /* Sym 3..N: Defined functions */
    size_t sym_idx = 3;
    if (symbols) {
        for (size_t i = 0; i < symbols->count; i++) {
            if (symbols->items[i].is_defined) {
                uint32_t sname = str_table_add(strtab, symbols->items[i].name);
                symtab_entries[sym_idx].st_name = sname;
                symtab_entries[sym_idx].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
                symtab_entries[sym_idx].st_other = STV_DEFAULT;
                symtab_entries[sym_idx].st_shndx = 1; /* .text section index */
                symtab_entries[sym_idx].st_value = symbols->items[i].text_offset;
                symtab_entries[sym_idx].st_size = symbols->items[i].size;
                sym_idx++;
            }
        }
    }

    /* Sym N+1..M: Undefined externals */
    if (symbols) {
        for (size_t i = 0; i < symbols->count; i++) {
            if (!symbols->items[i].is_defined) {
                uint32_t sname = str_table_add(strtab, symbols->items[i].name);
                symtab_entries[sym_idx].st_name = sname;
                symtab_entries[sym_idx].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
                symtab_entries[sym_idx].st_other = STV_DEFAULT;
                symtab_entries[sym_idx].st_shndx = SHN_UNDEF;
                symtab_entries[sym_idx].st_value = 0;
                symtab_entries[sym_idx].st_size = 0;
                sym_idx++;
            }
        }
    }

    /* 3. Build .rela.text entries */
    size_t rela_count = text_relocs ? text_relocs->count : 0;
    Elf64_Rela *rela_entries = (Elf64_Rela *)calloc(rela_count > 0 ? rela_count : 1, sizeof(Elf64_Rela));

    for (size_t r = 0; r < rela_count; r++) {
        X86Reloc *rec = &text_relocs->items[r];
        uint32_t target_sym_idx = 0;

        if (strcmp(rec->symbol, ".rodata") == 0) {
            target_sym_idx = 2; /* .rodata section symbol */
        } else {
            /* Find matching symbol index in symtab */
            for (size_t s = 3; s < total_syms; s++) {
                const char *sname = strtab->data + symtab_entries[s].st_name;
                if (strcmp(sname, rec->symbol) == 0) {
                    target_sym_idx = (uint32_t)s;
                    break;
                }
            }
        }

        rela_entries[r].r_offset = rec->offset;
        rela_entries[r].r_info = ELF64_R_INFO(target_sym_idx, rec->type);
        rela_entries[r].r_addend = rec->addend;
    }

    /* 4. Lay out sections in file */
    /* Sections:
     * 0: NULL
     * 1: .text (PROGBITS, ALLOC|EXEC, align 16)
     * 2: .rela.text (RELA, INFO_LINK, link=symtab, info=.text, align 8, entsize 24)
     * 3: .rodata (PROGBITS, ALLOC, align 16)
     * 4: .symtab (SYMTAB, link=strtab, info=3, align 8, entsize 24)
     * 5: .strtab (STRTAB, align 1)
     * 6: .shstrtab (STRTAB, align 1)
     * 7: .note.GNU-stack (PROGBITS, align 1, size 0)
     */
    uint64_t file_offset = sizeof(Elf64_Ehdr); /* 64 bytes */

    /* .text */
    uint64_t off_text = align_offset(file_offset, 16);
    uint64_t sz_text = text_sec ? text_sec->size : 0;
    file_offset = off_text + sz_text;

    /* .rela.text */
    uint64_t off_rela = align_offset(file_offset, 8);
    uint64_t sz_rela = rela_count * sizeof(Elf64_Rela);
    file_offset = off_rela + sz_rela;

    /* .rodata */
    uint64_t off_rodata = align_offset(file_offset, 16);
    uint64_t sz_rodata = rodata_sec ? rodata_sec->size : 0;
    file_offset = off_rodata + sz_rodata;

    /* .symtab */
    uint64_t off_symtab = align_offset(file_offset, 8);
    uint64_t sz_symtab = total_syms * sizeof(Elf64_Sym);
    file_offset = off_symtab + sz_symtab;

    /* .strtab */
    uint64_t off_strtab = file_offset;
    uint64_t sz_strtab = strtab->size;
    file_offset = off_strtab + sz_strtab;

    /* .shstrtab */
    uint64_t off_shstrtab = file_offset;
    uint64_t sz_shstrtab = shstrtab->size;
    file_offset = off_shstrtab + sz_shstrtab;

    /* Section Header Table */
    uint64_t off_shdr = align_offset(file_offset, 8);
    uint16_t num_shdr = 8;
    uint64_t total_file_size = off_shdr + num_shdr * sizeof(Elf64_Shdr);

    /* 5. Build ELF Header */
    Elf64_Ehdr ehdr;
    memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[EI_MAG0] = ELFMAG0;
    ehdr.e_ident[EI_MAG1] = ELFMAG1;
    ehdr.e_ident[EI_MAG2] = ELFMAG2;
    ehdr.e_ident[EI_MAG3] = ELFMAG3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    ehdr.e_ident[EI_ABIVERSION] = 0;
    ehdr.e_type = ET_REL;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = 0;
    ehdr.e_phoff = 0;
    ehdr.e_shoff = off_shdr;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = 0;
    ehdr.e_phnum = 0;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = num_shdr;
    ehdr.e_shstrndx = 6; /* .shstrtab section index */

    /* 6. Build Section Headers */
    Elf64_Shdr shdrs[8];
    memset(shdrs, 0, sizeof(shdrs));

    /* 0: NULL */

    /* 1: .text */
    shdrs[1].sh_name = sh_name_text;
    shdrs[1].sh_type = SHT_PROGBITS;
    shdrs[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdrs[1].sh_offset = off_text;
    shdrs[1].sh_size = sz_text;
    shdrs[1].sh_addralign = 16;

    /* 2: .rela.text */
    shdrs[2].sh_name = sh_name_relatext;
    shdrs[2].sh_type = SHT_RELA;
    shdrs[2].sh_flags = SHF_INFO_LINK;
    shdrs[2].sh_offset = off_rela;
    shdrs[2].sh_size = sz_rela;
    shdrs[2].sh_link = 4; /* .symtab */
    shdrs[2].sh_info = 1; /* .text */
    shdrs[2].sh_addralign = 8;
    shdrs[2].sh_entsize = sizeof(Elf64_Rela);

    /* 3: .rodata */
    shdrs[3].sh_name = sh_name_rodata;
    shdrs[3].sh_type = SHT_PROGBITS;
    shdrs[3].sh_flags = SHF_ALLOC;
    shdrs[3].sh_offset = off_rodata;
    shdrs[3].sh_size = sz_rodata;
    shdrs[3].sh_addralign = 16;

    /* 4: .symtab */
    shdrs[4].sh_name = sh_name_symtab;
    shdrs[4].sh_type = SHT_SYMTAB;
    shdrs[4].sh_flags = 0;
    shdrs[4].sh_offset = off_symtab;
    shdrs[4].sh_size = sz_symtab;
    shdrs[4].sh_link = 5; /* .strtab */
    shdrs[4].sh_info = 3; /* First global symbol index */
    shdrs[4].sh_addralign = 8;
    shdrs[4].sh_entsize = sizeof(Elf64_Sym);

    /* 5: .strtab */
    shdrs[5].sh_name = sh_name_strtab;
    shdrs[5].sh_type = SHT_STRTAB;
    shdrs[5].sh_flags = 0;
    shdrs[5].sh_offset = off_strtab;
    shdrs[5].sh_size = sz_strtab;
    shdrs[5].sh_addralign = 1;

    /* 6: .shstrtab */
    shdrs[6].sh_name = sh_name_shstrtab;
    shdrs[6].sh_type = SHT_STRTAB;
    shdrs[6].sh_flags = 0;
    shdrs[6].sh_offset = off_shstrtab;
    shdrs[6].sh_size = sz_shstrtab;
    shdrs[6].sh_addralign = 1;

    /* 7: .note.GNU-stack */
    shdrs[7].sh_name = sh_name_gnustack;
    shdrs[7].sh_type = SHT_PROGBITS;
    shdrs[7].sh_flags = 0;
    shdrs[7].sh_offset = 0;
    shdrs[7].sh_size = 0;
    shdrs[7].sh_addralign = 1;

    /* 7. Assemble file bytes */
    uint8_t *file_data = (uint8_t *)calloc(1, total_file_size);
    if (!file_data) {
        if (out_error) *out_error = strdup("memory allocation failed for ELF object file");
        free(symtab_entries);
        free(rela_entries);
        str_table_free(strtab);
        str_table_free(shstrtab);
        return false;
    }

    memcpy(file_data, &ehdr, sizeof(Elf64_Ehdr));
    if (sz_text > 0 && text_sec && text_sec->data) {
        memcpy(file_data + off_text, text_sec->data, sz_text);
    }
    if (sz_rela > 0) {
        memcpy(file_data + off_rela, rela_entries, sz_rela);
    }
    if (sz_rodata > 0 && rodata_sec && rodata_sec->data) {
        memcpy(file_data + off_rodata, rodata_sec->data, sz_rodata);
    }
    if (sz_symtab > 0) {
        memcpy(file_data + off_symtab, symtab_entries, sz_symtab);
    }
    if (sz_strtab > 0) {
        memcpy(file_data + off_strtab, strtab->data, sz_strtab);
    }
    if (sz_shstrtab > 0) {
        memcpy(file_data + off_shstrtab, shstrtab->data, sz_shstrtab);
    }
    memcpy(file_data + off_shdr, shdrs, sizeof(shdrs));

    /* 8. Write file to disk */
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        if (out_error) {
            char buf[256];
            snprintf(buf, sizeof(buf), "could not open output file '%s' for writing", out_path);
            *out_error = strdup(buf);
        }
        free(file_data);
        free(symtab_entries);
        free(rela_entries);
        str_table_free(strtab);
        str_table_free(shstrtab);
        return false;
    }

    size_t written = fwrite(file_data, 1, total_file_size, f);
    fclose(f);

    /* 9. Cleanup */
    free(file_data);
    free(symtab_entries);
    free(rela_entries);
    str_table_free(strtab);
    str_table_free(shstrtab);

    return (written == total_file_size);
}
