// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_PRINT_H
#define IR_PRINT_H

#include "ir.h"
#include <stdio.h>

/* Returns a newly allocated string containing the textual IR of the module.
 * The caller is responsible for calling free() on the returned string.
 */
char *ir_print_module(IrModule *module);

/* Prints the textual IR of the module to the given stream. */
void ir_dump_module(FILE *out, IrModule *module);

/* Prints the textual IR of a single function to the given stream. */
void ir_dump_function(FILE *out, IrFunction *fn);

#endif /* IR_PRINT_H */
