// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "x86_64_link.h"
#include "runtime_start_data.h"
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/* Internal Constants & Helpers                                              */

#define PAGE_SIZE       0x1000ULL
#define BASE_VADDR      0x400000ULL
#define TEXT_VADDR_BASE 0x401000ULL

static inline uint64_t align_to(uint64_t val, uint64_t align) {
    if (align <= 1) return val;
    return (val + align - 1) & ~(align - 1);
}

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} ByteBuffer;

static ByteBuffer *byte_buf_create(void) {
    ByteBuffer *b = (ByteBuffer *)malloc(sizeof(ByteBuffer));
    if (!b) return NULL;
    b->capacity = 256;
    b->size = 0;
    b->data = (uint8_t *)malloc(b->capacity);
    return b;
}

static void byte_buf_free(ByteBuffer *b) {
    if (!b) return;
    if (b->data) free(b->data);
    free(b);
}

static void byte_buf_append(ByteBuffer *b, const void *src, size_t len) {
    if (!b || !src || len == 0) return;
    if (b->size + len > b->capacity) {
        while (b->size + len > b->capacity) b->capacity *= 2;
        b->data = (uint8_t *)realloc(b->data, b->capacity);
    }
    memcpy(b->data + b->size, src, len);
    b->size += len;
}

static void byte_buf_pad_to(ByteBuffer *b, size_t target_size, uint8_t pad_byte) {
    if (!b || b->size >= target_size) return;
    size_t diff = target_size - b->size;
    if (b->size + diff > b->capacity) {
        while (b->size + diff > b->capacity) b->capacity *= 2;
        b->data = (uint8_t *)realloc(b->data, b->capacity);
    }
    memset(b->data + b->size, pad_byte, diff);
    b->size = target_size;
}

/* ELF64 Relocatable Object Reader & Validator                               */

static Elf64ParsedObject *elf64_read_object_memory(const uint8_t *data, size_t size, const char *name, bool owns_data, char **out_error) {
    if (!data || size < sizeof(Elf64_Ehdr)) {
        if (out_error) *out_error = strdup("file is too small to be a valid ELF64 header");
        return NULL;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

    /* 1. Magic check */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        if (out_error) *out_error = strdup("invalid ELF magic bytes (not an ELF file)");
        return NULL;
    }

    /* 2. Class check */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        if (out_error) *out_error = strdup("unsupported ELF class (only 64-bit ELFCLASS64 supported)");
        return NULL;
    }

    /* 3. Data encoding */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        if (out_error) *out_error = strdup("unsupported ELF data encoding (only 2's complement little-endian supported)");
        return NULL;
    }

    /* 4. Type & Machine */
    if (ehdr->e_type != ET_REL) {
        if (out_error) *out_error = strdup("unsupported ELF type (expected relocatable object ET_REL)");
        return NULL;
    }

    if (ehdr->e_machine != EM_X86_64) {
        if (out_error) *out_error = strdup("unsupported architecture (expected x86-64 EM_X86_64)");
        return NULL;
    }

    /* 5. Section Header Table Bounds */
    if (ehdr->e_shentsize < sizeof(Elf64_Shdr)) {
        if (out_error) *out_error = strdup("invalid section header entry size");
        return NULL;
    }

    uint64_t sh_table_end = (uint64_t)ehdr->e_shoff + (uint64_t)ehdr->e_shnum * (uint64_t)ehdr->e_shentsize;
    if (sh_table_end > size) {
        if (out_error) *out_error = strdup("section header table extends past end of file");
        return NULL;
    }

    const Elf64_Shdr *shdrs = (const Elf64_Shdr *)(data + ehdr->e_shoff);

    /* 6. Section Header String Table (.shstrtab) */
    if (ehdr->e_shstrndx >= ehdr->e_shnum) {
        if (out_error) *out_error = strdup("invalid section header string table index");
        return NULL;
    }

    const Elf64_Shdr *shstr_shdr = &shdrs[ehdr->e_shstrndx];
    if (shstr_shdr->sh_offset + shstr_shdr->sh_size > size) {
        if (out_error) *out_error = strdup("section header string table extends past end of file");
        return NULL;
    }
    const char *shstrtab = (const char *)(data + shstr_shdr->sh_offset);
    size_t shstrtab_size = shstr_shdr->sh_size;

    /* Allocate parsed object representation */
    Elf64ParsedObject *obj = (Elf64ParsedObject *)calloc(1, sizeof(Elf64ParsedObject));
    obj->filename = strdup(name ? name : "<unnamed>");
    obj->raw_data = (uint8_t *)data;
    obj->raw_size = size;
    obj->ehdr = (Elf64_Ehdr *)ehdr;
    obj->shdrs = (Elf64_Shdr *)shdrs;
    obj->shdr_count = ehdr->e_shnum;
    obj->shstrtab = shstrtab;
    obj->shstrtab_size = shstrtab_size;

    /* 7. Locate and validate sections */
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        const Elf64_Shdr *sh = &shdrs[i];
        if (sh->sh_name >= shstrtab_size) continue;
        const char *sname = shstrtab + sh->sh_name;

        if (sh->sh_type != SHT_NOBITS && (sh->sh_offset + sh->sh_size > size)) {
            if (out_error) {
                char msg[256];
                snprintf(msg, sizeof(msg), "section '%s' extends past end of file", sname);
                *out_error = strdup(msg);
            }
            elf64_free_object_file(obj);
            return NULL;
        }

        if (strcmp(sname, ".text") == 0) {
            obj->text_shdr = (Elf64_Shdr *)sh;
            obj->text_data = data + sh->sh_offset;
            obj->text_size = sh->sh_size;
            obj->text_shndx = i;
        } else if (strcmp(sname, ".rodata") == 0) {
            obj->rodata_shdr = (Elf64_Shdr *)sh;
            obj->rodata_data = data + sh->sh_offset;
            obj->rodata_size = sh->sh_size;
            obj->rodata_shndx = i;
        } else if (strcmp(sname, ".data") == 0) {
            obj->data_shdr = (Elf64_Shdr *)sh;
            obj->data_data = data + sh->sh_offset;
            obj->data_size = sh->sh_size;
            obj->data_shndx = i;
        } else if (strcmp(sname, ".bss") == 0) {
            obj->bss_shdr = (Elf64_Shdr *)sh;
            obj->bss_size = sh->sh_size;
            obj->bss_shndx = i;
        } else if (sh->sh_type == SHT_SYMTAB) {
            obj->symtab_shdr = (Elf64_Shdr *)sh;
            obj->symtab = (const Elf64_Sym *)(data + sh->sh_offset);
            obj->sym_count = sh->sh_size / sizeof(Elf64_Sym);

            /* Associated string table for symbol names */
            if (sh->sh_link < ehdr->e_shnum) {
                const Elf64_Shdr *strtab_sh = &shdrs[sh->sh_link];
                if (strtab_sh->sh_offset + strtab_sh->sh_size <= size) {
                    obj->strtab = (const char *)(data + strtab_sh->sh_offset);
                    obj->strtab_size = strtab_sh->sh_size;
                }
            }
        } else if (sh->sh_type == SHT_RELA && strcmp(sname, ".rela.text") == 0) {
            obj->relatext_shdr = (Elf64_Shdr *)sh;
            obj->relas = (const Elf64_Rela *)(data + sh->sh_offset);
            obj->rela_count = sh->sh_size / sizeof(Elf64_Rela);
        }
    }

    /* 8. Validate relocations */
    if (obj->relas && obj->rela_count > 0) {
        for (size_t r = 0; r < obj->rela_count; r++) {
            const Elf64_Rela *rel = &obj->relas[r];
            if (rel->r_offset >= obj->text_size) {
                if (out_error) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "relocation offset 0x%lx exceeds .text size in '%s'", (unsigned long)rel->r_offset, obj->filename);
                    *out_error = strdup(msg);
                }
                elf64_free_object_file(obj);
                return NULL;
            }
            uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
            if (sym_idx >= obj->sym_count) {
                if (out_error) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "relocation references out-of-bounds symbol index %u in '%s'", sym_idx, obj->filename);
                    *out_error = strdup(msg);
                }
                elf64_free_object_file(obj);
                return NULL;
            }
        }
    }

    (void)owns_data;
    return obj;
}

Elf64ParsedObject *elf64_read_object_file(const char *path, char **out_error) {
    if (!path) {
        if (out_error) *out_error = strdup("null input file path");
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (out_error) {
            char msg[256];
            snprintf(msg, sizeof(msg), "could not open input object '%s': %s", path, strerror(errno));
            *out_error = strdup(msg);
        }
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < (long)sizeof(Elf64_Ehdr)) {
        fclose(f);
        if (out_error) *out_error = strdup("object file too small");
        return NULL;
    }

    uint8_t *buf = (uint8_t *)malloc(fsize);
    if (!buf) {
        fclose(f);
        if (out_error) *out_error = strdup("out of memory reading object file");
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, fsize, f);
    fclose(f);

    if (read_bytes != (size_t)fsize) {
        free(buf);
        if (out_error) *out_error = strdup("short read from object file");
        return NULL;
    }

    Elf64ParsedObject *obj = elf64_read_object_memory(buf, read_bytes, path, true, out_error);
    if (!obj) {
        free(buf);
        return NULL;
    }
    return obj;
}

void elf64_free_object_file(Elf64ParsedObject *obj) {
    if (!obj) return;
    if (obj->filename) free(obj->filename);
    /* Only free raw_data if it does not point to embedded constant data */
    if (obj->raw_data != (const uint8_t *)build_runtime_start_o) {
        free(obj->raw_data);
    }
    free(obj);
}

/* Global Symbol Table for Linker Resolution                                 */

typedef enum {
    SEC_NONE = 0,
    SEC_TEXT,
    SEC_RODATA,
    SEC_DATA,
    SEC_BSS
} SymbolSecKind;

typedef struct {
    char *name;
    uint64_t vaddr;
    uint64_t size;
    int obj_idx;            /* index into input objects */
    uint64_t offset_in_sec; /* offset within object's section */
    SymbolSecKind sec_kind;
    bool is_defined;
} GlobalSym;

typedef struct {
    GlobalSym *items;
    size_t count;
    size_t capacity;
} GlobalSymTable;

static GlobalSymTable *symtab_create(void) {
    GlobalSymTable *t = (GlobalSymTable *)malloc(sizeof(GlobalSymTable));
    t->capacity = 64;
    t->count = 0;
    t->items = (GlobalSym *)malloc(sizeof(GlobalSym) * t->capacity);
    return t;
}

static void symtab_free(GlobalSymTable *t) {
    if (!t) return;
    for (size_t i = 0; i < t->count; i++) {
        if (t->items[i].name) free(t->items[i].name);
    }
    free(t->items);
    free(t);
}

static GlobalSym *symtab_find(GlobalSymTable *t, const char *name) {
    if (!t || !name) return NULL;
    for (size_t i = 0; i < t->count; i++) {
        if (strcmp(t->items[i].name, name) == 0) {
            return &t->items[i];
        }
    }
    return NULL;
}

static bool symtab_add(GlobalSymTable *t, const char *name, int obj_idx, uint64_t offset_in_sec, uint64_t size, SymbolSecKind sec_kind, bool is_defined, char **out_error) {
    if (!t || !name) return false;

    GlobalSym *existing = symtab_find(t, name);
    if (existing) {
        if (existing->is_defined && is_defined) {
            if (out_error) {
                char msg[256];
                snprintf(msg, sizeof(msg), "duplicate strong definition of symbol '%s'", name);
                *out_error = strdup(msg);
            }
            return false;
        }
        if (!existing->is_defined && is_defined) {
            existing->is_defined = true;
            existing->obj_idx = obj_idx;
            existing->offset_in_sec = offset_in_sec;
            existing->size = size;
            existing->sec_kind = sec_kind;
        }
        return true;
    }

    if (t->count + 1 > t->capacity) {
        t->capacity *= 2;
        t->items = (GlobalSym *)realloc(t->items, sizeof(GlobalSym) * t->capacity);
    }

    GlobalSym *sym = &t->items[t->count++];
    sym->name = strdup(name);
    sym->vaddr = 0;
    sym->size = size;
    sym->obj_idx = obj_idx;
    sym->offset_in_sec = offset_in_sec;
    sym->sec_kind = sec_kind;
    sym->is_defined = is_defined;
    return true;
}

/* Core ELF64 Static Linker Implementation                                   */

bool elf64_link_objects(const LinkerOptions *opts, char **out_error) {
    if (!opts || !opts->out_path) {
        if (out_error) *out_error = strdup("null linker options or output path");
        return false;
    }

    if (opts->input_count == 0) {
        if (out_error) *out_error = strdup("no input object files specified for linking");
        return false;
    }

    /* 1. Parse all input object files */
    size_t total_objs = opts->input_count;
    Elf64ParsedObject **objs = (Elf64ParsedObject **)calloc(total_objs + 1, sizeof(Elf64ParsedObject *));

    for (size_t i = 0; i < opts->input_count; i++) {
        objs[i] = elf64_read_object_file(opts->input_files[i], out_error);
        if (!objs[i]) {
            for (size_t j = 0; j < i; j++) elf64_free_object_file(objs[j]);
            free(objs);
            return false;
        }
    }

    /* 2. Check if _start is defined by any input object */
    bool has_start = false;
    bool has_main = false;
    for (size_t i = 0; i < total_objs; i++) {
        Elf64ParsedObject *obj = objs[i];
        if (obj->symtab && obj->strtab) {
            for (size_t s = 0; s < obj->sym_count; s++) {
                const Elf64_Sym *sym = &obj->symtab[s];
                if (sym->st_shndx != SHN_UNDEF && sym->st_name < obj->strtab_size) {
                    const char *sname = obj->strtab + sym->st_name;
                    if (strcmp(sname, "_start") == 0) has_start = true;
                    if (strcmp(sname, "main") == 0) has_main = true;
                }
            }
        }
    }

    /* If standalone_runtime is requested and _start is missing, add minimal runtime object */
    if (opts->standalone_runtime && !has_start && has_main) {
        Elf64ParsedObject *rt_obj = elf64_read_object_memory((const uint8_t *)build_runtime_start_o,
                                                             build_runtime_start_o_len,
                                                             "<builtin:runtime_start.o>",
                                                             false,
                                                             out_error);
        if (rt_obj) {
            objs[total_objs] = rt_obj;
            total_objs++;
            has_start = true;
        }
    }

    /* 3. Collect and resolve all symbols across objects */
    GlobalSymTable *symtab = symtab_create();

    for (size_t i = 0; i < total_objs; i++) {
        Elf64ParsedObject *obj = objs[i];
        if (!obj->symtab || !obj->strtab) continue;

        for (size_t s = 0; s < obj->sym_count; s++) {
            const Elf64_Sym *sym = &obj->symtab[s];
            if (sym->st_name == 0 || sym->st_name >= obj->strtab_size) continue;
            const char *sname = obj->strtab + sym->st_name;
            uint8_t bind = ELF64_ST_BIND(sym->st_info);

            if (bind == STB_GLOBAL) {
                if (sym->st_shndx == SHN_UNDEF) {
                    /* Undefined reference */
                    if (!symtab_find(symtab, sname)) {
                        symtab_add(symtab, sname, (int)i, 0, 0, SEC_NONE, false, NULL);
                    }
                } else {
                    /* Defined global symbol */
                    SymbolSecKind skind = SEC_NONE;
                    if (sym->st_shndx == obj->text_shndx) skind = SEC_TEXT;
                    else if (sym->st_shndx == obj->rodata_shndx) skind = SEC_RODATA;
                    else if (sym->st_shndx == obj->data_shndx) skind = SEC_DATA;
                    else if (sym->st_shndx == obj->bss_shndx) skind = SEC_BSS;

                    if (!symtab_add(symtab, sname, (int)i, sym->st_value, sym->st_size, skind, true, out_error)) {
                        for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
                        free(objs);
                        symtab_free(symtab);
                        return false;
                    }
                }
            }
        }
    }

    /* Check for unresolved undefined symbols */
    for (size_t i = 0; i < symtab->count; i++) {
        if (!symtab->items[i].is_defined) {
            if (out_error) {
                char msg[256];
                snprintf(msg, sizeof(msg), "undefined symbol '%s' (no definition found across input objects)", symtab->items[i].name);
                *out_error = strdup(msg);
            }
            for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
            free(objs);
            symtab_free(symtab);
            return false;
        }
    }

    /* 4. Merge compatible sections across objects */
    ByteBuffer *merged_text = byte_buf_create();
    ByteBuffer *merged_rodata = byte_buf_create();

    uint64_t *obj_text_offsets = (uint64_t *)calloc(total_objs, sizeof(uint64_t));
    uint64_t *obj_rodata_offsets = (uint64_t *)calloc(total_objs, sizeof(uint64_t));

    for (size_t i = 0; i < total_objs; i++) {
        Elf64ParsedObject *obj = objs[i];

        /* Merge .text */
        if (obj->text_data && obj->text_size > 0) {
            uint64_t aligned_off = align_to(merged_text->size, 16);
            byte_buf_pad_to(merged_text, aligned_off, 0x90); /* NOP pad */
            obj_text_offsets[i] = merged_text->size;
            byte_buf_append(merged_text, obj->text_data, obj->text_size);
        } else {
            obj_text_offsets[i] = merged_text->size;
        }

        /* Merge .rodata */
        if (obj->rodata_data && obj->rodata_size > 0) {
            uint64_t aligned_off = align_to(merged_rodata->size, 16);
            byte_buf_pad_to(merged_rodata, aligned_off, 0x00);
            obj_rodata_offsets[i] = merged_rodata->size;
            byte_buf_append(merged_rodata, obj->rodata_data, obj->rodata_size);
        } else {
            obj_rodata_offsets[i] = merged_rodata->size;
        }
    }

    /* 5. Memory Layout Calculation (Virtual Addresses & Offsets) */
    uint64_t text_file_offset = PAGE_SIZE; /* 0x1000 */
    uint64_t text_vaddr = TEXT_VADDR_BASE;  /* 0x401000 */
    uint64_t text_filesz = merged_text->size;
    uint64_t text_memsz = text_filesz;

    uint64_t rodata_file_offset = 0;
    uint64_t rodata_vaddr = 0;
    uint64_t rodata_filesz = 0;
    uint64_t rodata_memsz = 0;

    if (merged_rodata->size > 0) {
        uint64_t text_page_span = align_to(text_filesz, PAGE_SIZE);
        if (text_page_span == 0) text_page_span = PAGE_SIZE;

        rodata_file_offset = text_file_offset + text_page_span;
        rodata_vaddr = text_vaddr + text_page_span;
        rodata_filesz = merged_rodata->size;
        rodata_memsz = rodata_filesz;
    }

    /* 6. Compute Virtual Addresses for all Global Symbols */
    for (size_t i = 0; i < symtab->count; i++) {
        GlobalSym *s = &symtab->items[i];
        if (s->sec_kind == SEC_TEXT) {
            s->vaddr = text_vaddr + obj_text_offsets[s->obj_idx] + s->offset_in_sec;
        } else if (s->sec_kind == SEC_RODATA) {
            s->vaddr = rodata_vaddr + obj_rodata_offsets[s->obj_idx] + s->offset_in_sec;
        }
    }

    /* Determine entry point */
    GlobalSym *entry_sym = symtab_find(symtab, "_start");
    if (!entry_sym) {
        entry_sym = symtab_find(symtab, "main");
    }
    if (!entry_sym) {
        if (out_error) *out_error = strdup("no valid entry point (_start or main) found in input objects");
        for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
        free(objs);
        symtab_free(symtab);
        byte_buf_free(merged_text);
        byte_buf_free(merged_rodata);
        free(obj_text_offsets);
        free(obj_rodata_offsets);
        return false;
    }
    uint64_t entry_vaddr = entry_sym->vaddr;

    /* 7. Relocation Engine: Apply all relocations across input objects */
    for (size_t i = 0; i < total_objs; i++) {
        Elf64ParsedObject *obj = objs[i];
        if (!obj->relas || obj->rela_count == 0) continue;

        for (size_t r = 0; r < obj->rela_count; r++) {
            const Elf64_Rela *rel = &obj->relas[r];
            uint64_t patch_offset = obj_text_offsets[i] + rel->r_offset;
            uint64_t P = text_vaddr + patch_offset;

            uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
            uint32_t r_type = ELF64_R_TYPE(rel->r_info);
            const Elf64_Sym *target_sym = &obj->symtab[sym_idx];

            uint64_t S = 0;

            if (ELF64_ST_TYPE(target_sym->st_info) == STT_SECTION) {
                /* Section symbol (e.g. .rodata) */
                if (target_sym->st_shndx == obj->rodata_shndx) {
                    S = rodata_vaddr + obj_rodata_offsets[i] + target_sym->st_value;
                } else if (target_sym->st_shndx == obj->text_shndx) {
                    S = text_vaddr + obj_text_offsets[i] + target_sym->st_value;
                }
            } else if (ELF64_ST_BIND(target_sym->st_info) == STB_LOCAL) {
                /* Local symbol within this object */
                if (target_sym->st_shndx == obj->text_shndx) {
                    S = text_vaddr + obj_text_offsets[i] + target_sym->st_value;
                } else if (target_sym->st_shndx == obj->rodata_shndx) {
                    S = rodata_vaddr + obj_rodata_offsets[i] + target_sym->st_value;
                }
            } else {
                /* Global symbol */
                const char *sname = obj->strtab + target_sym->st_name;
                GlobalSym *gsym = symtab_find(symtab, sname);
                if (!gsym || !gsym->is_defined) {
                    if (out_error) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "unresolved relocation target '%s' in '%s'", sname, obj->filename);
                        *out_error = strdup(msg);
                    }
                    for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
                    free(objs);
                    symtab_free(symtab);
                    byte_buf_free(merged_text);
                    byte_buf_free(merged_rodata);
                    free(obj_text_offsets);
                    free(obj_rodata_offsets);
                    return false;
                }
                S = gsym->vaddr;
            }

            int64_t A = rel->r_addend;

            if (r_type == R_X86_64_PC32 || r_type == R_X86_64_PLT32) {
                int64_t val = (int64_t)S + A - (int64_t)P;
                if (val < -2147483648LL || val > 2147483647LL) {
                    if (out_error) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "PC-relative relocation out of 32-bit range (%ld) in '%s'", (long)val, obj->filename);
                        *out_error = strdup(msg);
                    }
                    for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
                    free(objs);
                    symtab_free(symtab);
                    byte_buf_free(merged_text);
                    byte_buf_free(merged_rodata);
                    free(obj_text_offsets);
                    free(obj_rodata_offsets);
                    return false;
                }
                int32_t val32 = (int32_t)val;
                memcpy(merged_text->data + patch_offset, &val32, 4);
            } else {
                if (out_error) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "unsupported relocation type %u in '%s'", r_type, obj->filename);
                    *out_error = strdup(msg);
                }
                for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
                free(objs);
                symtab_free(symtab);
                byte_buf_free(merged_text);
                byte_buf_free(merged_rodata);
                free(obj_text_offsets);
                free(obj_rodata_offsets);
                return false;
            }
        }
    }

    /* 8. Construct Final ELF64 Executable Header and Program Headers */
    uint16_t phnum = (merged_rodata->size > 0) ? 4 : 3;
    Elf64_Phdr phdrs[4];
    memset(phdrs, 0, sizeof(phdrs));

    /* Segment 0: Headers (PT_LOAD, R--) */
    uint64_t hdr_bytes = sizeof(Elf64_Ehdr) + phnum * sizeof(Elf64_Phdr);
    phdrs[0].p_type = PT_LOAD;
    phdrs[0].p_flags = PF_R;
    phdrs[0].p_offset = 0;
    phdrs[0].p_vaddr = BASE_VADDR;
    phdrs[0].p_paddr = BASE_VADDR;
    phdrs[0].p_filesz = hdr_bytes;
    phdrs[0].p_memsz = hdr_bytes;
    phdrs[0].p_align = PAGE_SIZE;

    /* Segment 1: Executable Code (PT_LOAD, R-X) */
    phdrs[1].p_type = PT_LOAD;
    phdrs[1].p_flags = PF_R | PF_X;
    phdrs[1].p_offset = text_file_offset;
    phdrs[1].p_vaddr = text_vaddr;
    phdrs[1].p_paddr = text_vaddr;
    phdrs[1].p_filesz = text_filesz;
    phdrs[1].p_memsz = text_memsz;
    phdrs[1].p_align = PAGE_SIZE;

    int ph_idx = 2;
    if (merged_rodata->size > 0) {
        /* Segment 2: Read-Only Data (PT_LOAD, R--) */
        phdrs[ph_idx].p_type = PT_LOAD;
        phdrs[ph_idx].p_flags = PF_R;
        phdrs[ph_idx].p_offset = rodata_file_offset;
        phdrs[ph_idx].p_vaddr = rodata_vaddr;
        phdrs[ph_idx].p_paddr = rodata_vaddr;
        phdrs[ph_idx].p_filesz = rodata_filesz;
        phdrs[ph_idx].p_memsz = rodata_memsz;
        phdrs[ph_idx].p_align = PAGE_SIZE;
        ph_idx++;
    }

    /* Stack Non-Executable Note (PT_GNU_STACK, RW-) */
    phdrs[ph_idx].p_type = PT_GNU_STACK;
    phdrs[ph_idx].p_flags = PF_R | PF_W;
    phdrs[ph_idx].p_align = 8;

    /* Construct ELF Header */
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
    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = entry_vaddr;
    ehdr.e_phoff = sizeof(Elf64_Ehdr);
    ehdr.e_shoff = 0; /* Pure loadable executable without mandatory section headers */
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = phnum;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = SHN_UNDEF;

    /* 9. Write Executable File to disk */
    FILE *out = fopen(opts->out_path, "wb");
    if (!out) {
        if (out_error) {
            char msg[256];
            snprintf(msg, sizeof(msg), "failed to open output file '%s' for writing: %s", opts->out_path, strerror(errno));
            *out_error = strdup(msg);
        }
        for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
        free(objs);
        symtab_free(symtab);
        byte_buf_free(merged_text);
        byte_buf_free(merged_rodata);
        free(obj_text_offsets);
        free(obj_rodata_offsets);
        return false;
    }

    /* Write ELF Header & Program Headers */
    fwrite(&ehdr, 1, sizeof(ehdr), out);
    fwrite(phdrs, sizeof(Elf64_Phdr), phnum, out);

    /* Pad up to text_file_offset (0x1000) */
    uint64_t current_file_pos = sizeof(ehdr) + sizeof(Elf64_Phdr) * phnum;
    if (current_file_pos < text_file_offset) {
        size_t pad = text_file_offset - current_file_pos;
        uint8_t *pad_bytes = (uint8_t *)calloc(pad, 1);
        fwrite(pad_bytes, 1, pad, out);
        free(pad_bytes);
    }

    /* Write .text */
    fwrite(merged_text->data, 1, merged_text->size, out);
    current_file_pos = text_file_offset + merged_text->size;

    /* Write .rodata if present */
    if (merged_rodata->size > 0) {
        if (current_file_pos < rodata_file_offset) {
            size_t pad = rodata_file_offset - current_file_pos;
            uint8_t *pad_bytes = (uint8_t *)calloc(pad, 1);
            fwrite(pad_bytes, 1, pad, out);
            free(pad_bytes);
        }
        fwrite(merged_rodata->data, 1, merged_rodata->size, out);
    }

    fclose(out);

    /* Set executable permissions (0755: rwxr-xr-x) */
    chmod(opts->out_path, 0755);

    /* 10. Cleanup */
    for (size_t j = 0; j < total_objs; j++) elf64_free_object_file(objs[j]);
    free(objs);
    symtab_free(symtab);
    byte_buf_free(merged_text);
    byte_buf_free(merged_rodata);
    free(obj_text_offsets);
    free(obj_rodata_offsets);

    return true;
}

bool elf64_link_files(const char **input_paths, size_t input_count, const char *out_path, char **out_error) {
    LinkerOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.input_files = input_paths;
    opts.input_count = input_count;
    opts.out_path = out_path;
    opts.standalone_runtime = true;
    opts.verbose = false;
    return elf64_link_objects(&opts, out_error);
}

bool elf64_link_single_file(const char *input_path, const char *out_path, char **out_error) {
    const char *paths[1] = { input_path };
    return elf64_link_files(paths, 1, out_path, out_error);
}
