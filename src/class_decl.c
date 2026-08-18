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

    ct->enum_count = program->as.program.enum_count;
    ct->enum_capacity = ct->enum_count;
    if (ct->enum_count > 0) {
        ct->enums = (EnumDef *)arena_alloc_array(arena, ct->enum_count, sizeof(EnumDef));
    } else {
        ct->enums = NULL;
    }

    for (int i = 0; i < program->as.program.enum_count; i++) {
        AstNode *e_node = program->as.program.enums[i];
        EnumDef *edef = &ct->enums[i];
        edef->name = arena_strdup(arena, e_node->as.enum_decl.name);
        edef->variant_count = e_node->as.enum_decl.variant_count;
        edef->enum_node = e_node;
        if (edef->variant_count > 0) {
            edef->variants = (EnumVariantDef *)arena_alloc_array(arena, edef->variant_count, sizeof(EnumVariantDef));
            for (int v = 0; v < edef->variant_count; v++) {
                AstNode *v_node = e_node->as.enum_decl.variants[v];
                EnumVariantDef *vdef = &edef->variants[v];
                vdef->name = arena_strdup(arena, v_node->as.variant_decl.name);
                vdef->is_unit = v_node->as.variant_decl.is_unit;
                vdef->field_count = v_node->as.variant_decl.field_count;
                if (vdef->field_count > 0) {
                    vdef->fields = (FieldInfo *)arena_alloc_array(arena, vdef->field_count, sizeof(FieldInfo));
                    for (int f = 0; f < vdef->field_count; f++) {
                        AstNode *f_node = v_node->as.variant_decl.fields[f];
                        vdef->fields[f].name = arena_strdup(arena, f_node->as.field.name);
                        vdef->fields[f].type = f_node->as.field.type;
                        vdef->fields[f].class_name = f_node->as.field.class_name ? arena_strdup(arena, f_node->as.field.class_name) : NULL;
                    }
                } else {
                    vdef->fields = NULL;
                }
            }
        } else {
            edef->variants = NULL;
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

EnumDef *find_enum(ClassTable *ct, const char *name) {
    if (!ct || !name) return NULL;
    for (int i = 0; i < ct->enum_count; i++) {
        if (strcmp(ct->enums[i].name, name) == 0) {
            return &ct->enums[i];
        }
    }
    return NULL;
}

EnumVariantDef *find_enum_variant(EnumDef *edef, const char *variant_name) {
    if (!edef || !variant_name) return NULL;
    for (int i = 0; i < edef->variant_count; i++) {
        if (strcmp(edef->variants[i].name, variant_name) == 0) {
            return &edef->variants[i];
        }
    }
    return NULL;
}

FieldInfo *find_variant_field(EnumVariantDef *vdef, const char *field_name) {
    if (!vdef || !field_name) return NULL;
    for (int i = 0; i < vdef->field_count; i++) {
        if (strcmp(vdef->fields[i].name, field_name) == 0) {
            return &vdef->fields[i];
        }
    }
    return NULL;
}

TypeKind resolve_type_name(ClassTable *ct, const char *name) {
    if (!ct || !name) return TYPE_KIND_UNKNOWN;
    if (find_class(ct, name) != NULL) return TYPE_KIND_CLASS;
    if (find_struct(ct, name) != NULL) return TYPE_KIND_STRUCT;
    if (find_enum(ct, name) != NULL) return TYPE_KIND_ENUM;
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
