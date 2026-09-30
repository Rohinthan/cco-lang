// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Memory Arena for IR                                                       */

typedef struct IrArenaChunk {
    void *memory;
    size_t used;
    size_t capacity;
    struct IrArenaChunk *next;
} IrArenaChunk;

struct IrArena {
    IrArenaChunk *head;
};

static IrArenaChunk *create_chunk(size_t capacity) {
    IrArenaChunk *chunk = (IrArenaChunk *)malloc(sizeof(IrArenaChunk));
    if (!chunk) return NULL;
    chunk->memory = malloc(capacity);
    if (!chunk->memory) {
        free(chunk);
        return NULL;
    }
    chunk->used = 0;
    chunk->capacity = capacity;
    chunk->next = NULL;
    return chunk;
}

IrArena *ir_arena_create(void) {
    IrArena *arena = (IrArena *)malloc(sizeof(IrArena));
    if (!arena) return NULL;
    arena->head = create_chunk(65536); /* 64 KB initial chunk */
    return arena;
}

void ir_arena_free(IrArena *arena) {
    if (!arena) return;
    IrArenaChunk *curr = arena->head;
    while (curr) {
        IrArenaChunk *next = curr->next;
        free(curr->memory);
        free(curr);
        curr = next;
    }
    free(arena);
}

void *ir_arena_alloc(IrArena *arena, size_t size) {
    if (!arena || size == 0) return NULL;
    /* 8-byte alignment */
    size = (size + 7) & ~((size_t)7);
    IrArenaChunk *curr = arena->head;
    if (!curr || (curr->used + size > curr->capacity)) {
        size_t new_cap = curr ? (curr->capacity * 2) : 65536;
        if (new_cap < size) new_cap = size * 2;
        IrArenaChunk *new_c = create_chunk(new_cap);
        new_c->next = arena->head;
        arena->head = new_c;
        curr = new_c;
    }
    void *ptr = (char *)curr->memory + curr->used;
    curr->used += size;
    memset(ptr, 0, size);
    return ptr;
}

char *ir_arena_strdup(IrArena *arena, const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *copy = (char *)ir_arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

/* IR Types Implementation                                                   */

IrType *ir_type_void(IrArena *arena) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_VOID;
    return t;
}

IrType *ir_type_i32(IrArena *arena) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_I32;
    return t;
}

IrType *ir_type_i64(IrArena *arena) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_I64;
    return t;
}

IrType *ir_type_f64(IrArena *arena) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_F64;
    return t;
}

IrType *ir_type_bool(IrArena *arena) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_BOOL;
    return t;
}

IrType *ir_type_char(IrArena *arena) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_CHAR;
    return t;
}

IrType *ir_type_ptr(IrArena *arena, IrType *elem_type) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_PTR;
    t->elem_type = elem_type;
    return t;
}

IrType *ir_type_struct(IrArena *arena, const char *name) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_STRUCT;
    t->name = ir_arena_strdup(arena, name);
    return t;
}

IrType *ir_type_array(IrArena *arena, IrType *elem_type, int size) {
    IrType *t = (IrType *)ir_arena_alloc(arena, sizeof(IrType));
    t->kind = IR_TYPE_ARRAY;
    t->elem_type = elem_type;
    t->array_size = size;
    return t;
}

bool ir_type_equals(const IrType *a, const IrType *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    switch (a->kind) {
        case IR_TYPE_VOID:
        case IR_TYPE_I32:
        case IR_TYPE_I64:
        case IR_TYPE_F64:
        case IR_TYPE_BOOL:
        case IR_TYPE_CHAR:
            return true;
        case IR_TYPE_PTR:
            if (!a->elem_type || !b->elem_type) return true; /* Opaque pointers match */
            return ir_type_equals(a->elem_type, b->elem_type);
        case IR_TYPE_STRUCT:
            if (!a->name || !b->name) return false;
            return strcmp(a->name, b->name) == 0;
        case IR_TYPE_ARRAY:
            if (a->array_size != b->array_size) return false;
            return ir_type_equals(a->elem_type, b->elem_type);
        default:
            return false;
    }
}

const char *ir_type_to_string(const IrType *type) {
    if (!type) return "unknown";
    switch (type->kind) {
        case IR_TYPE_VOID: return "void";
        case IR_TYPE_I32:  return "i32";
        case IR_TYPE_I64:  return "i64";
        case IR_TYPE_F64:  return "f64";
        case IR_TYPE_BOOL: return "bool";
        case IR_TYPE_CHAR: return "char";
        case IR_TYPE_PTR:  return "ptr";
        case IR_TYPE_STRUCT: return type->name ? type->name : "struct";
        case IR_TYPE_ARRAY:  return "array";
        default:           return "unknown";
    }
}

/* IR Values Implementation                                                  */

IrValue *ir_val_const_int(IrArena *arena, IrType *type, int64_t v) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_CONST_INT;
    val->type = type ? type : ir_type_i32(arena);
    val->const_val.int_val = v;
    return val;
}

IrValue *ir_val_const_float(IrArena *arena, double v) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_CONST_FLOAT;
    val->type = ir_type_f64(arena);
    val->const_val.float_val = v;
    return val;
}

IrValue *ir_val_const_bool(IrArena *arena, bool v) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_CONST_BOOL;
    val->type = ir_type_bool(arena);
    val->const_val.bool_val = v;
    return val;
}

IrValue *ir_val_const_char(IrArena *arena, char v) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_CONST_CHAR;
    val->type = ir_type_char(arena);
    val->const_val.char_val = v;
    return val;
}

IrValue *ir_val_const_string(IrArena *arena, const char *s) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_CONST_STRING;
    val->type = ir_type_ptr(arena, ir_type_char(arena));
    val->const_val.str_val = ir_arena_strdup(arena, s);
    return val;
}

IrValue *ir_val_reg(IrArena *arena, IrType *type, int id) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_REG;
    val->type = type;
    val->id = id;
    return val;
}

IrValue *ir_val_var(IrArena *arena, IrType *ptr_type, const char *name) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_VAR;
    val->type = ptr_type;
    val->name = ir_arena_strdup(arena, name);
    return val;
}

IrValue *ir_val_param(IrArena *arena, IrType *type, int id, const char *name) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_PARAM;
    val->type = type;
    val->id = id;
    val->name = ir_arena_strdup(arena, name);
    return val;
}

IrValue *ir_val_global(IrArena *arena, IrType *type, const char *name) {
    IrValue *val = (IrValue *)ir_arena_alloc(arena, sizeof(IrValue));
    val->kind = IR_VAL_GLOBAL;
    val->type = type;
    val->name = ir_arena_strdup(arena, name);
    return val;
}

/* IR Instructions and Opcodes                                               */

const char *ir_opcode_to_string(IrOpcode op) {
    switch (op) {
        case IR_OP_ALLOCA:    return "alloca";
        case IR_OP_LOAD:      return "load";
        case IR_OP_STORE:     return "store";
        case IR_OP_CONST:     return "const";
        case IR_OP_ADD:       return "add";
        case IR_OP_SUB:       return "sub";
        case IR_OP_MUL:       return "mul";
        case IR_OP_DIV:       return "div";
        case IR_OP_MOD:       return "mod";
        case IR_OP_NEG:       return "neg";
        case IR_OP_EQ:        return "eq";
        case IR_OP_NE:        return "ne";
        case IR_OP_LT:        return "lt";
        case IR_OP_LE:        return "le";
        case IR_OP_GT:        return "gt";
        case IR_OP_GE:        return "ge";
        case IR_OP_AND:       return "and";
        case IR_OP_OR:        return "or";
        case IR_OP_NOT:       return "not";
        case IR_OP_GET_FIELD: return "get_field";
        case IR_OP_SET_FIELD: return "set_field";
        case IR_OP_GET_INDEX: return "get_index";
        case IR_OP_SET_INDEX: return "set_index";
        case IR_OP_CALL:      return "call";
        case IR_OP_PRINT:     return "print";
        case IR_OP_ALLOC:     return "alloc";
        case IR_OP_FREE:      return "free";
        case IR_OP_RELEASE:   return "release";
        case IR_OP_PHI:       return "phi";
        case IR_OP_BR:        return "br";
        case IR_OP_CONDBR:    return "condbr";
        case IR_OP_RET:       return "ret";
        default:              return "unknown_op";
    }
}

bool ir_is_terminator(IrOpcode op) {
    return (op == IR_OP_BR || op == IR_OP_CONDBR || op == IR_OP_RET);
}

/* Module, Structs, Functions, and Blocks                                    */

IrModule *ir_module_create(const char *name) {
    IrArena *arena = ir_arena_create();
    if (!arena) return NULL;
    IrModule *mod = (IrModule *)ir_arena_alloc(arena, sizeof(IrModule));
    mod->arena = arena;
    mod->name = ir_arena_strdup(arena, name ? name : "module");
    mod->first_struct = NULL;
    mod->struct_count = 0;
    mod->first_fn = NULL;
    mod->last_fn = NULL;
    mod->fn_count = 0;
    return mod;
}

void ir_module_free(IrModule *module) {
    if (!module) return;
    ir_arena_free(module->arena);
}

IrStructDecl *ir_module_add_struct(IrModule *module, const char *name, bool is_class) {
    IrStructDecl *st = (IrStructDecl *)ir_arena_alloc(module->arena, sizeof(IrStructDecl));
    st->name = ir_arena_strdup(module->arena, name);
    st->fields = NULL;
    st->field_count = 0;
    st->is_class = is_class;
    st->next = module->first_struct;
    module->first_struct = st;
    module->struct_count++;
    return st;
}

void ir_struct_add_field(IrModule *module, IrStructDecl *st, const char *field_name, IrType *type) {
    int new_cnt = st->field_count + 1;
    IrFieldDecl *new_fields = (IrFieldDecl *)ir_arena_alloc(module->arena, new_cnt * sizeof(IrFieldDecl));
    if (st->fields && st->field_count > 0) {
        memcpy(new_fields, st->fields, st->field_count * sizeof(IrFieldDecl));
    }
    new_fields[st->field_count].name = ir_arena_strdup(module->arena, field_name);
    new_fields[st->field_count].type = type;
    st->fields = new_fields;
    st->field_count = new_cnt;
}

IrStructDecl *ir_module_find_struct(IrModule *module, const char *name) {
    for (IrStructDecl *s = module->first_struct; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

IrFunction *ir_function_create(IrModule *module, const char *name, IrType *return_type) {
    IrFunction *fn = (IrFunction *)ir_arena_alloc(module->arena, sizeof(IrFunction));
    fn->name = ir_arena_strdup(module->arena, name);
    fn->return_type = return_type ? return_type : ir_type_void(module->arena);
    fn->param_names = NULL;
    fn->param_types = NULL;
    fn->param_values = NULL;
    fn->param_count = 0;
    fn->entry_block = NULL;
    fn->first_block = NULL;
    fn->last_block = NULL;
    fn->block_count = 0;
    fn->next_reg_id = 0;
    fn->next_block_id = 0;
    fn->module = module;
    fn->next = NULL;

    if (!module->first_fn) {
        module->first_fn = fn;
        module->last_fn = fn;
    } else {
        module->last_fn->next = fn;
        module->last_fn = fn;
    }
    module->fn_count++;
    return fn;
}

void ir_function_add_param(IrFunction *fn, const char *param_name, IrType *type) {
    IrArena *arena = fn->module->arena;
    int idx = fn->param_count++;
    char **new_names = (char **)ir_arena_alloc(arena, fn->param_count * sizeof(char *));
    IrType **new_types = (IrType **)ir_arena_alloc(arena, fn->param_count * sizeof(IrType *));
    IrValue **new_vals = (IrValue **)ir_arena_alloc(arena, fn->param_count * sizeof(IrValue *));

    if (idx > 0) {
        memcpy(new_names, fn->param_names, idx * sizeof(char *));
        memcpy(new_types, fn->param_types, idx * sizeof(IrType *));
        memcpy(new_vals, fn->param_values, idx * sizeof(IrValue *));
    }
    new_names[idx] = ir_arena_strdup(arena, param_name);
    new_types[idx] = type;
    new_vals[idx] = ir_val_param(arena, type, idx, param_name);

    fn->param_names = new_names;
    fn->param_types = new_types;
    fn->param_values = new_vals;
}

IrFunction *ir_module_find_function(IrModule *module, const char *name) {
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (strcmp(fn->name, name) == 0) return fn;
    }
    return NULL;
}

IrBasicBlock *ir_block_create(IrFunction *fn, const char *label_prefix) {
    IrArena *arena = fn->module->arena;
    IrBasicBlock *bb = (IrBasicBlock *)ir_arena_alloc(arena, sizeof(IrBasicBlock));
    bb->id = fn->next_block_id++;
    bb->parent = fn;
    bb->first_inst = NULL;
    bb->last_inst = NULL;
    bb->inst_count = 0;
    bb->predecessors = NULL;
    bb->pred_count = 0;
    bb->pred_cap = 0;
    bb->successors = NULL;
    bb->succ_count = 0;
    bb->succ_cap = 0;
    bb->next = NULL;

    char buf[128];
    if (label_prefix && strlen(label_prefix) > 0) {
        if (strcmp(label_prefix, "entry") == 0 && bb->id == 0) {
            snprintf(buf, sizeof(buf), "entry");
        } else {
            snprintf(buf, sizeof(buf), "%s.%d", label_prefix, bb->id);
        }
    } else {
        snprintf(buf, sizeof(buf), "block.%d", bb->id);
    }
    bb->name = ir_arena_strdup(arena, buf);

    if (!fn->first_block) {
        fn->first_block = bb;
        fn->last_block = bb;
        fn->entry_block = bb;
    } else {
        fn->last_block->next = bb;
        fn->last_block = bb;
    }
    fn->block_count++;
    return bb;
}

void ir_block_add_instruction(IrBasicBlock *block, IrInstruction *inst) {
    if (!block || !inst) return;

    /* If this is an alloca and the block already ends with a terminator,
     * insert the alloca before the terminator to keep the terminator at the end */
    if (inst->op == IR_OP_ALLOCA && block->last_inst && ir_is_terminator(block->last_inst->op)) {
        IrInstruction *term = block->last_inst;
        inst->prev = term->prev;
        inst->next = term;
        if (term->prev) {
            term->prev->next = inst;
        } else {
            block->first_inst = inst;
        }
        term->prev = inst;
        block->inst_count++;
        return;
    }

    inst->prev = block->last_inst;
    inst->next = NULL;
    if (block->last_inst) {
        block->last_inst->next = inst;
        block->last_inst = inst;
    } else {
        block->first_inst = inst;
        block->last_inst = inst;
    }
    block->inst_count++;
}

void ir_block_add_successor(IrBasicBlock *from, IrBasicBlock *to) {
    if (!from || !to) return;
    /* Check if already present */
    for (int i = 0; i < from->succ_count; i++) {
        if (from->successors[i] == to) return;
    }
    IrArena *arena = from->parent->module->arena;
    if (from->succ_count >= from->succ_cap) {
        int new_cap = from->succ_cap == 0 ? 2 : from->succ_cap * 2;
        IrBasicBlock **new_arr = (IrBasicBlock **)ir_arena_alloc(arena, new_cap * sizeof(IrBasicBlock *));
        if (from->successors && from->succ_count > 0) {
            memcpy(new_arr, from->successors, from->succ_count * sizeof(IrBasicBlock *));
        }
        from->successors = new_arr;
        from->succ_cap = new_cap;
    }
    from->successors[from->succ_count++] = to;

    /* Add as predecessor to target block */
    for (int i = 0; i < to->pred_count; i++) {
        if (to->predecessors[i] == from) return;
    }
    if (to->pred_count >= to->pred_cap) {
        int new_cap = to->pred_cap == 0 ? 2 : to->pred_cap * 2;
        IrBasicBlock **new_arr = (IrBasicBlock **)ir_arena_alloc(arena, new_cap * sizeof(IrBasicBlock *));
        if (to->predecessors && to->pred_count > 0) {
            memcpy(new_arr, to->predecessors, to->pred_count * sizeof(IrBasicBlock *));
        }
        to->predecessors = new_arr;
        to->pred_cap = new_cap;
    }
    to->predecessors[to->pred_count++] = from;
}

void ir_recompute_cfg(IrFunction *fn) {
    if (!fn) return;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        bb->succ_count = 0;
        bb->pred_count = 0;
    }
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        if (!bb->last_inst) continue;
        if (bb->last_inst->op == IR_OP_BR) {
            if (bb->last_inst->target_true) {
                ir_block_add_successor(bb, bb->last_inst->target_true);
            }
        } else if (bb->last_inst->op == IR_OP_CONDBR) {
            if (bb->last_inst->target_true) {
                ir_block_add_successor(bb, bb->last_inst->target_true);
            }
            if (bb->last_inst->target_false) {
                ir_block_add_successor(bb, bb->last_inst->target_false);
            }
        }
    }
}

/* Builder Implementation                                                    */

void ir_builder_init(IrBuilder *builder, IrModule *module) {
    builder->module = module;
    builder->arena = module->arena;
    builder->function = NULL;
    builder->block = NULL;
}

void ir_builder_set_insert_block(IrBuilder *builder, IrBasicBlock *block) {
    builder->block = block;
    if (block) {
        builder->function = block->parent;
    }
}

IrValue *ir_builder_next_reg(IrBuilder *builder, IrType *type) {
    int id = builder->function->next_reg_id++;
    return ir_val_reg(builder->arena, type, id);
}

static IrInstruction *create_instruction(IrBuilder *builder, IrOpcode op, IrType *type) {
    IrInstruction *inst = (IrInstruction *)ir_arena_alloc(builder->arena, sizeof(IrInstruction));
    inst->op = op;
    inst->type = type;
    inst->result = NULL;
    inst->lhs = NULL;
    inst->rhs = NULL;
    inst->callee_name = NULL;
    inst->args = NULL;
    inst->arg_count = 0;
    inst->target_true = NULL;
    inst->target_false = NULL;
    inst->phi_blocks = NULL;
    inst->phi_values = NULL;
    inst->phi_count = 0;
    inst->field_name = NULL;
    inst->field_index = -1;
    inst->class_name = NULL;
    inst->is_array = false;
    inst->line = 0;
    inst->col = 0;
    inst->prev = NULL;
    inst->next = NULL;
    return inst;
}

IrInstruction *ir_emit_alloca(IrBuilder *builder, IrType *type, const char *var_name) {
    IrType *ptr_type = ir_type_ptr(builder->arena, type);
    IrInstruction *inst = create_instruction(builder, IR_OP_ALLOCA, ptr_type);
    inst->result = ir_val_var(builder->arena, ptr_type, var_name);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_load(IrBuilder *builder, IrType *type, IrValue *ptr) {
    IrInstruction *inst = create_instruction(builder, IR_OP_LOAD, type);
    inst->lhs = ptr;
    inst->result = ir_builder_next_reg(builder, type);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_store(IrBuilder *builder, IrValue *val, IrValue *ptr) {
    IrInstruction *inst = create_instruction(builder, IR_OP_STORE, ir_type_void(builder->arena));
    inst->lhs = val;
    inst->rhs = ptr;
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_const_int(IrBuilder *builder, IrType *type, int64_t v) {
    IrType *t = type ? type : ir_type_i32(builder->arena);
    IrInstruction *inst = create_instruction(builder, IR_OP_CONST, t);
    inst->lhs = ir_val_const_int(builder->arena, t, v);
    inst->result = ir_builder_next_reg(builder, t);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_const_float(IrBuilder *builder, double v) {
    IrType *t = ir_type_f64(builder->arena);
    IrInstruction *inst = create_instruction(builder, IR_OP_CONST, t);
    inst->lhs = ir_val_const_float(builder->arena, v);
    inst->result = ir_builder_next_reg(builder, t);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_const_bool(IrBuilder *builder, bool v) {
    IrType *t = ir_type_bool(builder->arena);
    IrInstruction *inst = create_instruction(builder, IR_OP_CONST, t);
    inst->lhs = ir_val_const_bool(builder->arena, v);
    inst->result = ir_builder_next_reg(builder, t);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_const_char(IrBuilder *builder, char v) {
    IrType *t = ir_type_char(builder->arena);
    IrInstruction *inst = create_instruction(builder, IR_OP_CONST, t);
    inst->lhs = ir_val_const_char(builder->arena, v);
    inst->result = ir_builder_next_reg(builder, t);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_const_string(IrBuilder *builder, const char *s) {
    IrType *t = ir_type_ptr(builder->arena, ir_type_char(builder->arena));
    IrInstruction *inst = create_instruction(builder, IR_OP_CONST, t);
    inst->lhs = ir_val_const_string(builder->arena, s);
    inst->result = ir_builder_next_reg(builder, t);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_binary(IrBuilder *builder, IrOpcode op, IrType *result_type, IrValue *lhs, IrValue *rhs) {
    IrInstruction *inst = create_instruction(builder, op, result_type);
    inst->lhs = lhs;
    inst->rhs = rhs;
    inst->result = ir_builder_next_reg(builder, result_type);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_unary(IrBuilder *builder, IrOpcode op, IrType *result_type, IrValue *operand) {
    IrInstruction *inst = create_instruction(builder, op, result_type);
    inst->lhs = operand;
    inst->result = ir_builder_next_reg(builder, result_type);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_call(IrBuilder *builder, const char *callee, IrType *ret_type, IrValue **args, int arg_count) {
    IrInstruction *inst = create_instruction(builder, IR_OP_CALL, ret_type);
    inst->callee_name = ir_arena_strdup(builder->arena, callee);
    inst->arg_count = arg_count;
    if (arg_count > 0) {
        inst->args = (IrValue **)ir_arena_alloc(builder->arena, arg_count * sizeof(IrValue *));
        memcpy(inst->args, args, arg_count * sizeof(IrValue *));
    }
    if (ret_type->kind != IR_TYPE_VOID) {
        inst->result = ir_builder_next_reg(builder, ret_type);
    }
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_print(IrBuilder *builder, IrValue *val) {
    IrInstruction *inst = create_instruction(builder, IR_OP_PRINT, ir_type_void(builder->arena));
    inst->lhs = val;
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_free(IrBuilder *builder, IrValue *ptr, bool is_array) {
    IrInstruction *inst = create_instruction(builder, IR_OP_FREE, ir_type_void(builder->arena));
    inst->lhs = ptr;
    inst->is_array = is_array;
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_release(IrBuilder *builder, IrValue *ptr, const char *class_name, bool is_array) {
    IrInstruction *inst = create_instruction(builder, IR_OP_RELEASE, ir_type_void(builder->arena));
    inst->lhs = ptr;
    inst->class_name = ir_arena_strdup(builder->arena, class_name);
    inst->is_array = is_array;
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_br(IrBuilder *builder, IrBasicBlock *target) {
    IrInstruction *inst = create_instruction(builder, IR_OP_BR, ir_type_void(builder->arena));
    inst->target_true = target;
    ir_block_add_instruction(builder->block, inst);
    ir_block_add_successor(builder->block, target);
    return inst;
}

IrInstruction *ir_emit_condbr(IrBuilder *builder, IrValue *cond, IrBasicBlock *target_true, IrBasicBlock *target_false) {
    IrInstruction *inst = create_instruction(builder, IR_OP_CONDBR, ir_type_void(builder->arena));
    inst->lhs = cond;
    inst->target_true = target_true;
    inst->target_false = target_false;
    ir_block_add_instruction(builder->block, inst);
    ir_block_add_successor(builder->block, target_true);
    ir_block_add_successor(builder->block, target_false);
    return inst;
}

IrInstruction *ir_emit_ret(IrBuilder *builder, IrValue *val) {
    IrType *t = val ? val->type : ir_type_void(builder->arena);
    IrInstruction *inst = create_instruction(builder, IR_OP_RET, t);
    inst->lhs = val;
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_ret_void(IrBuilder *builder) {
    IrInstruction *inst = create_instruction(builder, IR_OP_RET, ir_type_void(builder->arena));
    inst->lhs = NULL;
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

IrInstruction *ir_emit_phi(IrBuilder *builder, IrType *type) {
    IrInstruction *inst = create_instruction(builder, IR_OP_PHI, type);
    inst->result = ir_builder_next_reg(builder, type);
    ir_block_add_instruction(builder->block, inst);
    return inst;
}

void ir_phi_add_incoming(IrInstruction *phi, IrBasicBlock *block, IrValue *val, IrArena *arena) {
    if (!phi || phi->op != IR_OP_PHI || !arena) return;
    int new_cnt = phi->phi_count + 1;
    IrBasicBlock **new_blocks = (IrBasicBlock **)ir_arena_alloc(arena, new_cnt * sizeof(IrBasicBlock *));
    IrValue **new_vals = (IrValue **)ir_arena_alloc(arena, new_cnt * sizeof(IrValue *));
    if (phi->phi_blocks && phi->phi_count > 0) {
        memcpy(new_blocks, phi->phi_blocks, phi->phi_count * sizeof(IrBasicBlock *));
        memcpy(new_vals, phi->phi_values, phi->phi_count * sizeof(IrValue *));
    }
    new_blocks[phi->phi_count] = block;
    new_vals[phi->phi_count] = val;
    phi->phi_blocks = new_blocks;
    phi->phi_values = new_vals;
    phi->phi_count = new_cnt;
}
