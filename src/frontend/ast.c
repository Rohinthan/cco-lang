// SPDX-License-Identifier: Apache-2.0

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
        case TY_ENUM: return "enum";
        default: return "unknown";
    }
}

TypeDesc *type_desc_primitive(AstArena *arena, Type primitive) {
    if (!arena) return NULL;
    TypeDesc *td = (TypeDesc *)arena_malloc(arena, sizeof(TypeDesc));
    td->kind = TYPEDESC_PRIMITIVE;
    td->as.primitive_desc.primitive_kind = primitive;
    return td;
}

TypeDesc *type_desc_named(AstArena *arena, const char *name) {
    if (!arena) return NULL;
    TypeDesc *td = (TypeDesc *)arena_malloc(arena, sizeof(TypeDesc));
    td->kind = TYPEDESC_NAMED;
    td->as.named_desc.name = arena_strdup(arena, name);
    return td;
}

TypeDesc *type_desc_array(AstArena *arena, TypeDesc *elem_type) {
    if (!arena) return NULL;
    TypeDesc *td = (TypeDesc *)arena_malloc(arena, sizeof(TypeDesc));
    td->kind = TYPEDESC_ARRAY;
    td->as.array_desc.elem_type = elem_type;
    return td;
}

TypeDesc *type_desc_map(AstArena *arena, TypeDesc *key_type, TypeDesc *val_type) {
    if (!arena) return NULL;
    TypeDesc *td = (TypeDesc *)arena_malloc(arena, sizeof(TypeDesc));
    td->kind = TYPEDESC_MAP;
    td->as.map_desc.key_type = key_type;
    td->as.map_desc.val_type = val_type;
    return td;
}

bool type_desc_equals(const TypeDesc *a, const TypeDesc *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    switch (a->kind) {
        case TYPEDESC_PRIMITIVE:
            return a->as.primitive_desc.primitive_kind == b->as.primitive_desc.primitive_kind;
        case TYPEDESC_CLASS:
        case TYPEDESC_ENUM:
        case TYPEDESC_NAMED:
            if (!a->as.named_desc.name || !b->as.named_desc.name) return false;
            return strcmp(a->as.named_desc.name, b->as.named_desc.name) == 0;
        case TYPEDESC_ARRAY:
            return type_desc_equals(a->as.array_desc.elem_type, b->as.array_desc.elem_type);
        case TYPEDESC_MAP:
            return type_desc_equals(a->as.map_desc.key_type, b->as.map_desc.key_type) &&
                   type_desc_equals(a->as.map_desc.val_type, b->as.map_desc.val_type);
        default:
            return false;
    }
}

bool type_desc_is_compatible(const TypeDesc *expected, const TypeDesc *actual) {
    if (expected == actual) return true;
    if (!expected || !actual) return true;
    if (expected->kind == TYPEDESC_PRIMITIVE && actual->kind == TYPEDESC_PRIMITIVE) {
        Type et = expected->as.primitive_desc.primitive_kind;
        Type at = actual->as.primitive_desc.primitive_kind;
        if (et == at) return true;
        if (et == TY_FLOAT && at == TY_INT) return true;
        if (et == TY_INT && at == TY_FLOAT) return true;
        if (et == TY_CHAR && at == TY_INT) return true;
        if (et == TY_INT && at == TY_CHAR) return true;
        return false;
    }
    if (expected->kind == TYPEDESC_ARRAY && actual->kind == TYPEDESC_ARRAY) {
        return type_desc_is_compatible(expected->as.array_desc.elem_type, actual->as.array_desc.elem_type);
    }
    if (expected->kind == TYPEDESC_MAP && actual->kind == TYPEDESC_MAP) {
        return type_desc_is_compatible(expected->as.map_desc.key_type, actual->as.map_desc.key_type) &&
               type_desc_is_compatible(expected->as.map_desc.val_type, actual->as.map_desc.val_type);
    }
    if ((expected->kind == TYPEDESC_CLASS || expected->kind == TYPEDESC_NAMED || expected->kind == TYPEDESC_ENUM) &&
        (actual->kind == TYPEDESC_CLASS || actual->kind == TYPEDESC_NAMED || actual->kind == TYPEDESC_ENUM)) {
        if (!expected->as.named_desc.name || !actual->as.named_desc.name) return false;
        return strcmp(expected->as.named_desc.name, actual->as.named_desc.name) == 0;
    }
    return false;
}

char *type_desc_to_string(AstArena *arena, const TypeDesc *td) {
    if (!td) return arena_strdup(arena, "unknown");
    char buf[512];
    switch (td->kind) {
        case TYPEDESC_PRIMITIVE:
            return arena_strdup(arena, type_to_string(td->as.primitive_desc.primitive_kind));
        case TYPEDESC_CLASS:
        case TYPEDESC_ENUM:
        case TYPEDESC_NAMED:
            return arena_strdup(arena, td->as.named_desc.name ? td->as.named_desc.name : "unknown");
        case TYPEDESC_ARRAY: {
            char *elem_s = type_desc_to_string(arena, td->as.array_desc.elem_type);
            if (td->as.array_desc.elem_type && td->as.array_desc.elem_type->kind == TYPEDESC_MAP) {
                snprintf(buf, sizeof(buf), "(%s)[]", elem_s);
            } else {
                snprintf(buf, sizeof(buf), "%s[]", elem_s);
            }
            return arena_strdup(arena, buf);
        }
        case TYPEDESC_MAP: {
            char *key_s = type_desc_to_string(arena, td->as.map_desc.key_type);
            char *val_s = type_desc_to_string(arena, td->as.map_desc.val_type);
            snprintf(buf, sizeof(buf), "map[%s]%s", key_s, val_s);
            return arena_strdup(arena, buf);
        }
        default:
            return arena_strdup(arena, "unknown");
    }
}

TypeDesc *type_desc_clone(AstArena *arena, const TypeDesc *td) {
    if (!td || !arena) return NULL;
    switch (td->kind) {
        case TYPEDESC_PRIMITIVE:
            return type_desc_primitive(arena, td->as.primitive_desc.primitive_kind);
        case TYPEDESC_CLASS:
        case TYPEDESC_ENUM:
        case TYPEDESC_NAMED:
            return type_desc_named(arena, td->as.named_desc.name);
        case TYPEDESC_ARRAY:
            return type_desc_array(arena, type_desc_clone(arena, td->as.array_desc.elem_type));
        case TYPEDESC_MAP:
            return type_desc_map(arena, type_desc_clone(arena, td->as.map_desc.key_type), type_desc_clone(arena, td->as.map_desc.val_type));
        default:
            return NULL;
    }
}
