// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X86_64_CODEGEN_H
#define X86_64_CODEGEN_H

#include "ir.h"
#include "x86_64_instr.h"
#include <stdbool.h>

/* Generates standard GNU/AT&T x86-64 Linux assembly text from a verified Cco IR module.
 * Returns a newly allocated string containing the assembly code.
 * The caller is responsible for calling free() on the returned string.
 * If an unsupported construct is encountered, returns NULL and sets *out_error.
 */
char *x86_64_generate_assembly(IrModule *module, char **out_error);

struct CcoPassTimings;

/* Directly encodes x86-64 machine instructions and writes a Linux ELF64 relocatable
 * object file (.o) to out_path. Does not invoke GNU as.
 * Returns true on success, false on failure (with error message in *out_error).
 */
bool x86_64_emit_object_file(IrModule *module, const char *out_path, char **out_error);
bool x86_64_emit_object_file_timed(IrModule *module, const char *out_path, struct CcoPassTimings *timings, char **out_error);

/* Phase 8: Profile-Guided Block Layout Reordering and Function Lowering */
struct CcoProfile;
void ir_cfg_reorder_blocks_for_layout(IrFunction *fn, const struct CcoProfile *prof);
bool gen_function_instructions(IrFunction *fn, X86InstrList **out_list, char **out_error);
bool gen_function_instructions_timed(IrFunction *fn, X86InstrList **out_list, struct CcoPassTimings *timings, char **out_error);

#endif /* X86_64_CODEGEN_H */
