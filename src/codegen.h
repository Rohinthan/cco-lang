// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

char *generate_c_code(AstNode *program, AstArena *arena);

#endif // CODEGEN_H
