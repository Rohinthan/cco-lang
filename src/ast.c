#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ArenaChunk {
    void *memory;
    size_t used;
    size_t capacity;
    struct ArenaChunk *next;
} ArenaChunk;

struct AstArena {
    ArenaChunk *head;
};

static ArenaChunk *create_chunk(size_t capacity) {
    ArenaChunk *chunk = malloc(sizeof(ArenaChunk));
    chunk->memory = malloc(capacity);
    chunk->used = 0;
    chunk->capacity = capacity;
    chunk->next = NULL;
    return chunk;
}

AstArena *create_ast_arena(void) {
    AstArena *arena = malloc(sizeof(AstArena));
    arena->head = create_chunk(65536); // 64 KB initial chunk
    return arena;
}

void free_ast_arena(AstArena *arena) {
    if (!arena) return;
    ArenaChunk *curr = arena->head;
    while (curr) {
        ArenaChunk *next = curr->next;
        free(curr->memory);
        free(curr);
        curr = next;
    }
    free(arena);
}

static void *arena_malloc(AstArena *arena, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    ArenaChunk *curr = arena->head;
    if (curr->used + size > curr->capacity) {
        size_t new_cap = curr->capacity * 2;
        if (new_cap < size) new_cap = size * 2;
        ArenaChunk *new_c = create_chunk(new_cap);
        new_c->next = arena->head;
        arena->head = new_c;
        curr = new_c;
    }
    void *ptr = (char *)curr->memory + curr->used;
    curr->used += size;
    memset(ptr, 0, size);
    return ptr;
}

AstNode *arena_alloc_node(AstArena *arena, NodeType type, int line, int col) {
    AstNode *node = (AstNode *)arena_malloc(arena, sizeof(AstNode));
    node->type = type;
    node->line = line;
    node->col = col;
    return node;
}

char *arena_strdup(AstArena *arena, const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = (char *)arena_malloc(arena, len);
    memcpy(copy, str, len);
    return copy;
}

void **arena_alloc_array(AstArena *arena, int count, size_t elem_size) {
    if (count <= 0) return NULL;
    return (void **)arena_malloc(arena, count * elem_size);
}

const char *type_to_string(Type type) {
    switch (type) {
        case TY_INT: return "int";
        case TY_FLOAT: return "float";
        case TY_CHAR: return "char";
        case TY_BOOL: return "bool";
        case TY_STRING: return "string";
        case TY_VOID: return "void";
        case TY_CLASS: return "class";
        case TY_MAP: return "map";
        default: return "unknown";
    }
}
