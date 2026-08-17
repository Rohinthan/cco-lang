#ifndef CLASS_DECL_H
#define CLASS_DECL_H

#include "ast.h"

typedef enum {
    TYPE_KIND_UNKNOWN,
    TYPE_KIND_CLASS,
    TYPE_KIND_STRUCT
} TypeKind;

typedef struct FieldInfo {
    char *name;
    Type type;
    char *class_name; // Non-NULL if type == TY_CLASS
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

typedef struct ClassTable {
    ClassDef *classes;
    int count;
    int capacity;
    StructDef *structs;
    int struct_count;
    int struct_capacity;
} ClassTable;

ClassTable *build_class_table(AstNode *program, AstArena *arena);
ClassDef *find_class(ClassTable *ct, const char *name);
StructDef *find_struct(ClassTable *ct, const char *name);
TypeKind resolve_type_name(ClassTable *ct, const char *name);
FieldInfo *find_field(ClassDef *cls, const char *field_name);
FieldInfo *find_struct_field(StructDef *sdef, const char *field_name);
MethodInfo *find_method(ClassDef *cls, const char *method_name);

#endif // CLASS_DECL_H
