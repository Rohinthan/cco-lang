#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct RefRelease {
    char *var_name;
    char *class_name;
} RefRelease;

typedef enum {
    TY_INT,
    TY_FLOAT,
    TY_CHAR,
    TY_BOOL,
    TY_STRING,
    TY_VOID,
    TY_CLASS
} Type;

typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION,
    NODE_BLOCK,
    NODE_LET,
    NODE_ASSIGN,
    NODE_INDEX_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_PRINT,
    NODE_EXPR_STMT,
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_INDEX,
    NODE_LITERAL,
    NODE_IDENT,
    NODE_ALLOC,
    // v2 Node Types
    NODE_CLASS,
    NODE_FIELD,
    NODE_METHOD,
    NODE_NEW,
    NODE_MEMBER,
    NODE_METHOD_CALL,
    NODE_MEMBER_ASSIGN
} NodeType;

typedef struct AstNode AstNode;

struct AstNode {
    NodeType type;
    int line;
    int col;

    // Scope Analysis annotations (alloc based)
    bool is_heap_owner;           // True if let statement binds an alloc() call
    bool is_transferred;          // True if ownership moved to caller/outer variable
    char **frees_to_emit;         // Variable names to emit free() for before exit/jump
    int frees_count;
    bool free_old_on_reassign;    // True if assign stmt overwrites an existing owned pointer

    // Scope Analysis annotations (ownership based)
    RefRelease *releases_to_emit; // Class vars to emit _free() for
    int releases_count;
    bool is_moved_from;           // True if variable was moved out
    int move_line;                // Line number where move occurred

    union {
        struct {
            AstNode **classes;
            int class_count;
            AstNode **functions;
            int count;
        } program;

        struct {
            char *name;
            AstNode **fields;
            int field_count;
            AstNode **methods;
            int method_count;
        } class_decl;

        struct {
            char *name;
            Type type;
            char *class_name;
        } field;

        struct {
            char *name;
            bool has_self;
            char **param_names;
            Type *param_types;
            char **param_class_names;
            bool *param_is_borrowed;
            int *param_lines;
            int *param_cols;
            int param_count;
            Type return_type;
            char *return_class_name;
            bool returns_heap_pointer;
            AstNode *body;
        } method;

        struct {
            char *name;
            char **param_names;
            Type *param_types;
            char **param_class_names;
            bool *param_is_borrowed;
            int *param_lines;
            int *param_cols;
            int param_count;
            Type return_type;
            char *return_class_name;
            bool returns_heap_pointer;
            AstNode *body;
        } function;

        struct {
            AstNode **stmts;
            int count;
            // Owned pointers registered directly in this block's scope
            char **owned_vars;
            int owned_count;
        } block;

        struct {
            char *name;
            Type var_type;
            char *class_name;
            AstNode *value;
            bool retain_rhs;
        } let;

        struct {
            char *name;
            AstNode *value;
            char *class_name;
            bool retain_rhs;
            bool release_old;
        } assign;

        struct {
            AstNode *object;
            char *member_name;
            AstNode *value;
            char *field_class_name;
            bool retain_rhs;
            bool release_old;
        } member_assign;

        struct {
            char *array_name;
            AstNode *index;
            AstNode *value;
        } index_assign;

        struct {
            AstNode *cond;
            AstNode *then_b;
            AstNode *else_b;
        } if_stmt;

        struct {
            AstNode *cond;
            AstNode *body;
        } while_stmt;

        struct {
            AstNode *init;
            AstNode *cond;
            AstNode *step;
            AstNode *body;
        } for_stmt;

        struct {
            AstNode *value; // NULL if return void
        } return_stmt;

        struct {
            AstNode *value;
        } print_stmt;

        struct {
            AstNode *expr;
        } expr_stmt;

        struct {
            char op[3];
            AstNode *left;
            AstNode *right;
        } binary;

        struct {
            char op[3];
            AstNode *operand;
        } unary;

        struct {
            char *callee;
            AstNode **args;
            int arg_count;
        } call;

        struct {
            char *class_name;
            char **field_names;
            AstNode **field_values;
            int field_count;
        } new_expr;

        struct {
            AstNode *object;
            char *member_name;
            char *field_class_name;
        } member;

        struct {
            AstNode *object;
            char *method_name;
            AstNode **args;
            int arg_count;
            char *target_class_name;
        } method_call;

        struct {
            char *array_name;
            AstNode *index;
        } index;

        struct {
            Type lit_type;
            union {
                long i;
                double f;
                char c;
                char *s;
                bool b;
            } val;
        } literal;

        struct {
            char *name;
        } ident;

        struct {
            Type elem_type;
            char *class_name;
            AstNode *count_expr;
        } alloc;
    } as;
};

// Arena allocator interface
typedef struct AstArena AstArena;

AstArena *create_ast_arena(void);
void free_ast_arena(AstArena *arena);
AstNode *arena_alloc_node(AstArena *arena, NodeType type, int line, int col);
char *arena_strdup(AstArena *arena, const char *str);
void **arena_alloc_array(AstArena *arena, int count, size_t elem_size);

const char *type_to_string(Type type);

#endif // AST_H
