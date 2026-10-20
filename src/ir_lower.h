// SPDX-License-Identifier: Apache-2.0

#ifndef IR_LOWER_H
#define IR_LOWER_H

#include "ast.h"
#include "ir.h"

/* Lowers a semantically validated AST program into a Cco IR module.
 * Takes the analyzed program AST and a module name.
 * Returns the generated IrModule.
 */
IrModule *ir_lower_ast(AstNode *program, const char *module_name);

#endif /* IR_LOWER_H */
