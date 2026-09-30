// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_CODEGEN_C_H
#define IR_CODEGEN_C_H

#include "ir.h"

/* Generates standard ISO C11 code from a verified Cco IR module.
 * Returns a newly allocated string containing the C code.
 * The caller is responsible for calling free() on the returned string.
 */
char *ir_generate_c(IrModule *module);

#endif /* IR_CODEGEN_C_H */
