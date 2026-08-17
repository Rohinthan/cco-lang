#define _POSIX_C_SOURCE 200809L
#include "class_decl.h"
#include <stdlib.h>
#include <string.h>

ClassTable *build_class_table(AstNode *program, AstArena *arena) {
    if (!program || program->type != NODE_PROGRAM) return NULL;

    ClassTable *ct = (ClassTable *)arena_alloc_array(arena, 1, sizeof(ClassTable));
    ct->count = program->as.program.class_count;
    ct->capacity = ct->count;
    if (ct->count > 0) {
        ct->classes = (ClassDef *)arena_alloc_array(arena, ct->count, sizeof(ClassDef));
    } else {
        ct->classes = NULL;
    }

    for (int i = 0; i < program->as.program.class_count; i++) {
        AstNode *cls_node = program->as.program.classes[i];
        ClassDef *cdef = &ct->classes[i];
        cdef->name = arena_strdup(arena, cls_node->as.class_decl.name);
        
        cdef->field_count = cls_node->as.class_decl.field_count;
        if (cdef->field_count > 0) {
            cdef->fields = (FieldInfo *)arena_alloc_array(arena, cdef->field_count, sizeof(FieldInfo));
            for (int f = 0; f < cdef->field_count; f++) {
                AstNode *f_node = cls_node->as.class_decl.fields[f];
                cdef->fields[f].name = arena_strdup(arena, f_node->as.field.name);
                cdef->fields[f].type = f_node->as.field.type;
                cdef->fields[f].class_name = f_node->as.field.class_name ? arena_strdup(arena, f_node->as.field.class_name) : NULL;
            }
        } else {
            cdef->fields = NULL;
        }

        cdef->method_count = cls_node->as.class_decl.method_count;
        if (cdef->method_count > 0) {
            cdef->methods = (MethodInfo *)arena_alloc_array(arena, cdef->method_count, sizeof(MethodInfo));
            for (int m = 0; m < cdef->method_count; m++) {
                AstNode *m_node = cls_node->as.class_decl.methods[m];
                cdef->methods[m].name = arena_strdup(arena, m_node->as.method.name);
                cdef->methods[m].method_node = m_node;
            }
        } else {
            cdef->methods = NULL;
        }
    }

    ct->struct_count = program->as.program.struct_count;
    ct->struct_capacity = ct->struct_count;
    if (ct->struct_count > 0) {
        ct->structs = (StructDef *)arena_alloc_array(arena, ct->struct_count, sizeof(StructDef));
    } else {
        ct->structs = NULL;
    }

    for (int i = 0; i < program->as.program.struct_count; i++) {
        AstNode *s_node = program->as.program.structs[i];
        StructDef *sdef = &ct->structs[i];
        sdef->name = arena_strdup(arena, s_node->as.struct_decl.name);
        sdef->field_count = s_node->as.struct_decl.field_count;
        if (sdef->field_count > 0) {
            sdef->fields = (FieldInfo *)arena_alloc_array(arena, sdef->field_count, sizeof(FieldInfo));
            for (int f = 0; f < sdef->field_count; f++) {
                AstNode *f_node = s_node->as.struct_decl.fields[f];
                sdef->fields[f].name = arena_strdup(arena, f_node->as.struct_field_decl.name);
                sdef->fields[f].type = f_node->as.struct_field_decl.field_type;
                sdef->fields[f].class_name = NULL;
            }
        } else {
            sdef->fields = NULL;
        }
    }

    return ct;
}

ClassDef *find_class(ClassTable *ct, const char *name) {
    if (!ct || !name) return NULL;
    for (int i = 0; i < ct->count; i++) {
        if (strcmp(ct->classes[i].name, name) == 0) {
            return &ct->classes[i];
        }
    }
    return NULL;
}

StructDef *find_struct(ClassTable *ct, const char *name) {
    if (!ct || !name) return NULL;
    for (int i = 0; i < ct->struct_count; i++) {
        if (strcmp(ct->structs[i].name, name) == 0) {
            return &ct->structs[i];
        }
    }
    return NULL;
}

TypeKind resolve_type_name(ClassTable *ct, const char *name) {
    if (!ct || !name) return TYPE_KIND_UNKNOWN;
    if (find_class(ct, name) != NULL) return TYPE_KIND_CLASS;
    if (find_struct(ct, name) != NULL) return TYPE_KIND_STRUCT;
    return TYPE_KIND_UNKNOWN;
}

FieldInfo *find_field(ClassDef *cls, const char *field_name) {
    if (!cls || !field_name) return NULL;
    for (int i = 0; i < cls->field_count; i++) {
        if (strcmp(cls->fields[i].name, field_name) == 0) {
            return &cls->fields[i];
        }
    }
    return NULL;
}

FieldInfo *find_struct_field(StructDef *sdef, const char *field_name) {
    if (!sdef || !field_name) return NULL;
    for (int i = 0; i < sdef->field_count; i++) {
        if (strcmp(sdef->fields[i].name, field_name) == 0) {
            return &sdef->fields[i];
        }
    }
    return NULL;
}

MethodInfo *find_method(ClassDef *cls, const char *method_name) {
    if (!cls || !method_name) return NULL;
    for (int i = 0; i < cls->method_count; i++) {
        if (strcmp(cls->methods[i].name, method_name) == 0) {
            return &cls->methods[i];
        }
    }
    return NULL;
}
