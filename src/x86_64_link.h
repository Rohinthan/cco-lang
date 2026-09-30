// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X86_64_LINK_H
#define X86_64_LINK_H

#include <elf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Parsed Relocatable Object (ET_REL) Representation                         */

typedef struct {
    char *filename;
    uint8_t *raw_data;
    size_t raw_size;

    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdrs;
    size_t shdr_count;
    const char *shstrtab;
    size_t shstrtab_size;

    /* Section pointers */
    Elf64_Shdr *text_shdr;
    const uint8_t *text_data;
    size_t text_size;
    size_t text_shndx;

    Elf64_Shdr *rodata_shdr;
    const uint8_t *rodata_data;
    size_t rodata_size;
    size_t rodata_shndx;

    Elf64_Shdr *data_shdr;
    const uint8_t *data_data;
    size_t data_size;
    size_t data_shndx;

    Elf64_Shdr *bss_shdr;
    size_t bss_size;
    size_t bss_shndx;

    Elf64_Shdr *symtab_shdr;
    const Elf64_Sym *symtab;
    size_t sym_count;
    const char *strtab;
    size_t strtab_size;

    Elf64_Shdr *relatext_shdr;
    const Elf64_Rela *relas;
    size_t rela_count;
} Elf64ParsedObject;

/* Reads and defensively validates an ELF64 relocatable object file */
Elf64ParsedObject *elf64_read_object_file(const char *path, char **out_error);
void elf64_free_object_file(Elf64ParsedObject *obj);

/* Linker Configuration and Invocation                                       */

typedef struct {
    const char *out_path;
    const char **input_files;
    size_t input_count;
    bool standalone_runtime;   /* If true, synthesizes _start when main is present */
    bool verbose;
} LinkerOptions;

/* Core ELF64 static linker: links multiple ET_REL objects into an ET_EXEC executable */
bool elf64_link_objects(const LinkerOptions *opts, char **out_error);

/* Convenience wrapper for linking multiple object files to an executable */
bool elf64_link_files(const char **input_paths, size_t input_count, const char *out_path, char **out_error);

/* Convenience wrapper for linking a single object file to an executable */
bool elf64_link_single_file(const char *input_path, const char *out_path, char **out_error);

#endif /* X86_64_LINK_H */
