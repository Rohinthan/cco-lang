#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TY_INT,
    TY_FLOAT,
    TY_CHAR,
    TY_BOOL,
    TY_STRING,
    TY_VOID
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
    NODE_ALLOC
} NodeType;

typedef struct AstNode AstNode;

struct AstNode {
    NodeType type;
    int line;

    // Scope Analysis annotations
    bool is_heap_owner;           // True if let statement binds an alloc() call
    bool is_transferred;          // True if ownership moved to caller/outer variable
    char **frees_to_emit;         // Variable names to emit free() for before exit/jump
    int frees_count;
    bool free_old_on_reassign;    // True if assign stmt overwrites an existing owned pointer

    union {
        struct {
            AstNode **functions;
            int count;
        } program;

        struct {
            char *name;
            char **param_names;
            Type *param_types;
            int param_count;
            Type return_type;
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
            AstNode *value;
        } let;

        struct {
            char *name;
            AstNode *value;
        } assign;

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
            AstNode *count_expr;
        } alloc;
    } as;
};

// Arena allocator interface
typedef struct AstArena AstArena;

AstArena *create_ast_arena(void);
void free_ast_arena(AstArena *arena);
AstNode *arena_alloc_node(AstArena *arena, NodeType type, int line);
char *arena_strdup(AstArena *arena, const char *str);
void **arena_alloc_array(AstArena *arena, int count, size_t elem_size);

const char *type_to_string(Type type);

#endif // AST_H
