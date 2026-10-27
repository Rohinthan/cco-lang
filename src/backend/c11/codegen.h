// SPDX-License-Identifier: Apache-2.0

#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <stdbool.h>

char *generate_c_code(AstNode *program, AstArena *arena);
bool cco_codegen_needs_libm(void);

#endif // CODEGEN_H
