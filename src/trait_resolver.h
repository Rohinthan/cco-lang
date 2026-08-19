#ifndef TRAIT_RESOLVER_H
#define TRAIT_RESOLVER_H

#include "ast.h"

void resolve_and_monomorphize_traits(AstNode *program, AstArena *arena);

#endif // TRAIT_RESOLVER_H
