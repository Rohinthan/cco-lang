// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IR_H
#define IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Forward declarations */
typedef struct IrArena IrArena;
typedef struct IrType IrType;
typedef struct IrValue IrValue;
typedef struct IrInstruction IrInstruction;
typedef struct IrBasicBlock IrBasicBlock;
typedef struct IrFunction IrFunction;
typedef struct IrStructDecl IrStructDecl;
typedef struct IrModule IrModule;

/* Memory Arena for IR                                                       */

IrArena *ir_arena_create(void);
void ir_arena_free(IrArena *arena);
void *ir_arena_alloc(IrArena *arena, size_t size);
char *ir_arena_strdup(IrArena *arena, const char *str);

/* IR Type System                                                            */

typedef enum {
    IR_TYPE_VOID,
    IR_TYPE_I32,
    IR_TYPE_I64,
    IR_TYPE_F64,
    IR_TYPE_BOOL,
    IR_TYPE_CHAR,
    IR_TYPE_PTR,
    IR_TYPE_STRUCT,
    IR_TYPE_ARRAY
} IrTypeKind;

struct IrType {
    IrTypeKind kind;
    IrType *elem_type;    /* For PTR and ARRAY */
    char *name;           /* For named STRUCT */
    int array_size;       /* For ARRAY (-1 if dynamic) */
};

/* Type constructors */
IrType *ir_type_void(IrArena *arena);
IrType *ir_type_i32(IrArena *arena);
IrType *ir_type_i64(IrArena *arena);
IrType *ir_type_f64(IrArena *arena);
IrType *ir_type_bool(IrArena *arena);
IrType *ir_type_char(IrArena *arena);
IrType *ir_type_ptr(IrArena *arena, IrType *elem_type);
IrType *ir_type_struct(IrArena *arena, const char *name);
IrType *ir_type_array(IrArena *arena, IrType *elem_type, int size);

bool ir_type_equals(const IrType *a, const IrType *b);
const char *ir_type_to_string(const IrType *type);

/* IR Values                                                                 */

typedef enum {
    IR_VAL_CONST_INT,
    IR_VAL_CONST_FLOAT,
    IR_VAL_CONST_BOOL,
    IR_VAL_CONST_CHAR,
    IR_VAL_CONST_STRING,
    IR_VAL_REG,       /* Virtual register, e.g. %0 */
    IR_VAL_VAR,       /* Alloca stack slot pointer, e.g. %x.addr */
    IR_VAL_PARAM,     /* Function parameter, e.g. %arg.x */
    IR_VAL_GLOBAL     /* Global symbol reference */
} IrValueKind;

struct IrValue {
    IrValueKind kind;
    IrType *type;
    int id;           /* Virtual register number or parameter index */
    char *name;       /* Symbolic name */
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        char char_val;
        char *str_val;
    } const_val;
};

/* Value constructors */
IrValue *ir_val_const_int(IrArena *arena, IrType *type, int64_t v);
IrValue *ir_val_const_float(IrArena *arena, double v);
IrValue *ir_val_const_bool(IrArena *arena, bool v);
IrValue *ir_val_const_char(IrArena *arena, char v);
IrValue *ir_val_const_string(IrArena *arena, const char *s);
IrValue *ir_val_reg(IrArena *arena, IrType *type, int id);
IrValue *ir_val_var(IrArena *arena, IrType *ptr_type, const char *name);
IrValue *ir_val_param(IrArena *arena, IrType *type, int id, const char *name);
IrValue *ir_val_global(IrArena *arena, IrType *type, const char *name);

/* IR Instructions and Opcodes                                               */

typedef enum {
    /* Stack & Memory */
    IR_OP_ALLOCA,
    IR_OP_LOAD,
    IR_OP_STORE,

    /* Literals / Constants */
    IR_OP_CONST,

    /* Arithmetic */
    IR_OP_ADD,
    IR_OP_SUB,
    IR_OP_MUL,
    IR_OP_DIV,
    IR_OP_MOD,
    IR_OP_NEG,

    /* Comparison */
    IR_OP_EQ,
    IR_OP_NE,
    IR_OP_LT,
    IR_OP_LE,
    IR_OP_GT,
    IR_OP_GE,

    /* Logical */
    IR_OP_AND,
    IR_OP_OR,
    IR_OP_NOT,

    /* Struct & Array Access */
    IR_OP_GET_FIELD,
    IR_OP_SET_FIELD,
    IR_OP_GET_INDEX,
    IR_OP_SET_INDEX,

    /* Subroutines & Runtime */
    IR_OP_CALL,
    IR_OP_PRINT,
    IR_OP_ALLOC,
    IR_OP_FREE,
    IR_OP_RELEASE,

    /* SSA */
    IR_OP_PHI,

    /* Terminators */
    IR_OP_BR,
    IR_OP_CONDBR,
    IR_OP_RET
} IrOpcode;

const char *ir_opcode_to_string(IrOpcode op);
bool ir_is_terminator(IrOpcode op);

struct IrInstruction {
    IrOpcode op;
    IrType *type;             /* Result type or operation type */
    IrValue *result;          /* Destination register/slot (NULL if statement) */

    IrValue *lhs;             /* Operand 1 (or condition, ptr for load/store, ret val) */
    IrValue *rhs;             /* Operand 2 (or value to store) */

    /* For calls */
    char *callee_name;
    IrValue **args;
    int arg_count;

    /* For branches */
    IrBasicBlock *target_true;
    IrBasicBlock *target_false;

    /* For phi nodes */
    IrBasicBlock **phi_blocks;
    IrValue **phi_values;
    int phi_count;

    /* For field / indexing */
    char *field_name;
    int field_index;

    /* For memory cleanup */
    char *class_name;
    bool is_array;

    /* Source coordinates */
    int line;
    int col;

    /* Linked list within basic block */
    IrInstruction *prev;
    IrInstruction *next;
};

/* Basic Blocks                                                              */

struct IrBasicBlock {
    char *name;               /* Label name: "entry", "if.then.0", etc. */
    int id;                   /* Deterministic ID */
    IrFunction *parent;

    IrInstruction *first_inst;
    IrInstruction *last_inst;
    int inst_count;

    /* CFG Edges */
    IrBasicBlock **predecessors;
    int pred_count;
    int pred_cap;

    IrBasicBlock **successors;
    int succ_count;
    int succ_cap;

    IrBasicBlock *next;       /* Next block in function */
};

/* Functions                                                                 */

struct IrFunction {
    char *name;
    IrType *return_type;

    char **param_names;
    IrType **param_types;
    IrValue **param_values;
    int param_count;

    IrBasicBlock *entry_block;
    IrBasicBlock *first_block;
    IrBasicBlock *last_block;
    int block_count;

    int next_reg_id;
    int next_block_id;

    IrModule *module;
    IrFunction *next;
};

/* Struct and Type Declarations                                              */

typedef struct IrFieldDecl {
    char *name;
    IrType *type;
} IrFieldDecl;

struct IrStructDecl {
    char *name;
    IrFieldDecl *fields;
    int field_count;
    bool is_class;
    IrStructDecl *next;
};

/* Module                                                                    */

struct IrModule {
    char *name;
    IrArena *arena;

    IrStructDecl *first_struct;
    int struct_count;

    IrFunction *first_fn;
    IrFunction *last_fn;
    int fn_count;

    /* Phase 8 Profile Information */
    const void *profile;
};

/* Builder API                                                               */

typedef struct {
    IrModule *module;
    IrFunction *function;
    IrBasicBlock *block;
    IrArena *arena;
} IrBuilder;

IrModule *ir_module_create(const char *name);
void ir_module_free(IrModule *module);

IrStructDecl *ir_module_add_struct(IrModule *module, const char *name, bool is_class);
void ir_struct_add_field(IrModule *module, IrStructDecl *st, const char *field_name, IrType *type);
IrStructDecl *ir_module_find_struct(IrModule *module, const char *name);

IrFunction *ir_function_create(IrModule *module, const char *name, IrType *return_type);
void ir_function_add_param(IrFunction *fn, const char *param_name, IrType *type);
IrFunction *ir_module_find_function(IrModule *module, const char *name);

IrBasicBlock *ir_block_create(IrFunction *fn, const char *label_prefix);
void ir_block_add_instruction(IrBasicBlock *block, IrInstruction *inst);

void ir_builder_init(IrBuilder *builder, IrModule *module);
void ir_builder_set_insert_block(IrBuilder *builder, IrBasicBlock *block);
IrValue *ir_builder_next_reg(IrBuilder *builder, IrType *type);

/* Emission helpers */
IrInstruction *ir_emit_alloca(IrBuilder *builder, IrType *type, const char *var_name);
IrInstruction *ir_emit_load(IrBuilder *builder, IrType *type, IrValue *ptr);
IrInstruction *ir_emit_store(IrBuilder *builder, IrValue *val, IrValue *ptr);
IrInstruction *ir_emit_const_int(IrBuilder *builder, IrType *type, int64_t v);
IrInstruction *ir_emit_const_float(IrBuilder *builder, double v);
IrInstruction *ir_emit_const_bool(IrBuilder *builder, bool v);
IrInstruction *ir_emit_const_char(IrBuilder *builder, char v);
IrInstruction *ir_emit_const_string(IrBuilder *builder, const char *s);
IrInstruction *ir_emit_binary(IrBuilder *builder, IrOpcode op, IrType *result_type, IrValue *lhs, IrValue *rhs);
IrInstruction *ir_emit_unary(IrBuilder *builder, IrOpcode op, IrType *result_type, IrValue *operand);
IrInstruction *ir_emit_call(IrBuilder *builder, const char *callee, IrType *ret_type, IrValue **args, int arg_count);
IrInstruction *ir_emit_print(IrBuilder *builder, IrValue *val);
IrInstruction *ir_emit_free(IrBuilder *builder, IrValue *ptr, bool is_array);
IrInstruction *ir_emit_release(IrBuilder *builder, IrValue *ptr, const char *class_name, bool is_array);
IrInstruction *ir_emit_br(IrBuilder *builder, IrBasicBlock *target);
IrInstruction *ir_emit_condbr(IrBuilder *builder, IrValue *cond, IrBasicBlock *target_true, IrBasicBlock *target_false);
IrInstruction *ir_emit_ret(IrBuilder *builder, IrValue *val);
IrInstruction *ir_emit_ret_void(IrBuilder *builder);
IrInstruction *ir_emit_phi(IrBuilder *builder, IrType *type);
void ir_phi_add_incoming(IrInstruction *phi, IrBasicBlock *block, IrValue *val, IrArena *arena);

void ir_block_add_successor(IrBasicBlock *from, IrBasicBlock *to);
void ir_recompute_cfg(IrFunction *fn);

#endif /* IR_H */
