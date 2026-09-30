// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_VERIFY_H
#define IR_VERIFY_H

#include "ir.h"
#include <stdbool.h>

/* Verifies an IR module.
 * Returns true if valid.
 * If invalid, returns false and if out_error_msg != NULL, allocates a formatted
 * error message string describing the verification failure. The caller is responsible
 * for calling free() on *out_error_msg.
 */
bool ir_verify_module(IrModule *module, char **out_error_msg);

/* Verifies a single IR function. */
bool ir_verify_function(IrFunction *fn, char **out_error_msg);

#endif /* IR_VERIFY_H */
