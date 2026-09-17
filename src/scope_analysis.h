// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCOPE_ANALYSIS_H
#define SCOPE_ANALYSIS_H

#include "ast.h"

void analyze_scopes(AstNode *program, AstArena *arena);

#endif // SCOPE_ANALYSIS_H
