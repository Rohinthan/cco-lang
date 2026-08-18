#ifndef CLASS_DECL_H
#define CLASS_DECL_H

#include "ast.h"

typedef enum {
    TYPE_KIND_UNKNOWN,
    TYPE_KIND_CLASS,
    TYPE_KIND_STRUCT,
    TYPE_KIND_ENUM
} TypeKind;

typedef struct FieldInfo {
    char *name;
    Type type;
    char *class_name; // Non-NULL if type == TY_CLASS or TY_ENUM
} FieldInfo;

typedef struct MethodInfo {
    char *name;
    AstNode *method_node; // NODE_METHOD
} MethodInfo;

typedef struct ClassDef {
    char *name;
    FieldInfo *fields;
    int field_count;
    MethodInfo *methods;
    int method_count;
} ClassDef;

typedef struct StructDef {
    char *name;
    FieldInfo *fields;
    int field_count;
} StructDef;

typedef struct EnumVariantDef {
    char *name;
    FieldInfo *fields;
    int field_count;
    bool is_unit;
} EnumVariantDef;

typedef struct EnumDef {
    char *name;
    EnumVariantDef *variants;
    int variant_count;
    AstNode *enum_node;
} EnumDef;

typedef struct ClassTable {
    ClassDef *classes;
    int count;
    int capacity;
    StructDef *structs;
    int struct_count;
    int struct_capacity;
    EnumDef *enums;
    int enum_count;
    int enum_capacity;
} ClassTable;

ClassTable *build_class_table(AstNode *program, AstArena *arena);
ClassDef *find_class(ClassTable *ct, const char *name);
StructDef *find_struct(ClassTable *ct, const char *name);
EnumDef *find_enum(ClassTable *ct, const char *name);
EnumVariantDef *find_enum_variant(EnumDef *edef, const char *variant_name);
FieldInfo *find_variant_field(EnumVariantDef *vdef, const char *field_name);
TypeKind resolve_type_name(ClassTable *ct, const char *name);
FieldInfo *find_field(ClassDef *cls, const char *field_name);
FieldInfo *find_struct_field(StructDef *sdef, const char *field_name);
MethodInfo *find_method(ClassDef *cls, const char *method_name);

#endif // CLASS_DECL_H
