#ifndef MODULE_RESOLVER_H
#define MODULE_RESOLVER_H

#include "ast.h"

AstNode *resolve_program(const char *entry_path, AstArena *arena);

#endif // MODULE_RESOLVER_H
