#ifndef CLASS_DECL_H
#define CLASS_DECL_H

#include "ast.h"

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

typedef struct ClassTable {
    ClassDef *classes;
    int count;
    int capacity;
} ClassTable;

ClassTable *build_class_table(AstNode *program, AstArena *arena);
ClassDef *find_class(ClassTable *ct, const char *name);
FieldInfo *find_field(ClassDef *cls, const char *field_name);
MethodInfo *find_method(ClassDef *cls, const char *method_name);

#endif // CLASS_DECL_H
