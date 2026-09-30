// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_print.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} Buffer;

static Buffer create_buf(void) {
    Buffer b;
    b.capacity = 1024;
    b.length = 0;
    b.buffer = (char *)malloc(b.capacity);
    if (b.buffer) b.buffer[0] = '\0';
    return b;
}

static void buf_append(Buffer *b, const char *str) {
    if (!str) return;
    size_t len = strlen(str);
    if (b->length + len + 1 > b->capacity) {
        while (b->length + len + 1 > b->capacity) {
            b->capacity *= 2;
        }
        b->buffer = (char *)realloc(b->buffer, b->capacity);
    }
    memcpy(b->buffer + b->length, str, len);
    b->length += len;
    b->buffer[b->length] = '\0';
}

static void buf_appendf(Buffer *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[1024];
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    buf_append(b, tmp);
}

static void print_val(Buffer *b, IrValue *v) {
    if (!v) {
        buf_append(b, "none");
        return;
    }
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            buf_appendf(b, "%ld", (long)v->const_val.int_val);
            break;
        case IR_VAL_CONST_FLOAT:
            buf_appendf(b, "%g", v->const_val.float_val);
            break;
        case IR_VAL_CONST_BOOL:
            buf_append(b, v->const_val.bool_val ? "true" : "false");
            break;
        case IR_VAL_CONST_CHAR:
            buf_appendf(b, "'%c'", v->const_val.char_val);
            break;
        case IR_VAL_CONST_STRING:
            buf_appendf(b, "\"%s\"", v->const_val.str_val ? v->const_val.str_val : "");
            break;
        case IR_VAL_REG:
            buf_appendf(b, "%%%d", v->id);
            break;
        case IR_VAL_VAR:
            buf_appendf(b, "%%%s", v->name ? v->name : "var");
            break;
        case IR_VAL_PARAM:
            buf_appendf(b, "%%arg.%s", v->name ? v->name : "param");
            break;
        case IR_VAL_GLOBAL:
            buf_appendf(b, "@%s", v->name ? v->name : "global");
            break;
    }
}

static void print_instruction(Buffer *b, IrInstruction *inst) {
    buf_append(b, "    ");
    switch (inst->op) {
        case IR_OP_ALLOCA:
            print_val(b, inst->result);
            buf_appendf(b, " = alloca %s\n",
                        ir_type_to_string(inst->result->type->elem_type ? inst->result->type->elem_type : inst->result->type));
            break;

        case IR_OP_LOAD:
            print_val(b, inst->result);
            buf_appendf(b, " = load %s, ", ir_type_to_string(inst->type));
            print_val(b, inst->lhs);
            buf_append(b, "\n");
            break;

        case IR_OP_STORE:
            buf_appendf(b, "store %s ", ir_type_to_string(inst->lhs->type));
            print_val(b, inst->lhs);
            buf_append(b, ", ");
            print_val(b, inst->rhs);
            buf_append(b, "\n");
            break;

        case IR_OP_CONST:
            print_val(b, inst->result);
            buf_appendf(b, " = const %s ", ir_type_to_string(inst->type));
            print_val(b, inst->lhs);
            buf_append(b, "\n");
            break;

        case IR_OP_PHI:
            print_val(b, inst->result);
            buf_appendf(b, " = phi %s ", ir_type_to_string(inst->type));
            for (int i = 0; i < inst->phi_count; i++) {
                if (i > 0) buf_append(b, ", ");
                buf_appendf(b, "[%s: ", inst->phi_blocks[i] ? inst->phi_blocks[i]->name : "null");
                print_val(b, inst->phi_values[i]);
                buf_append(b, "]");
            }
            buf_append(b, "\n");
            break;

        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD:
        case IR_OP_EQ:
        case IR_OP_NE:
        case IR_OP_LT:
        case IR_OP_LE:
        case IR_OP_GT:
        case IR_OP_GE:
        case IR_OP_AND:
        case IR_OP_OR:
            print_val(b, inst->result);
            buf_appendf(b, " = %s %s ", ir_opcode_to_string(inst->op), ir_type_to_string(inst->lhs->type));
            print_val(b, inst->lhs);
            buf_append(b, ", ");
            print_val(b, inst->rhs);
            buf_append(b, "\n");
            break;

        case IR_OP_NEG:
        case IR_OP_NOT:
            print_val(b, inst->result);
            buf_appendf(b, " = %s %s ", ir_opcode_to_string(inst->op), ir_type_to_string(inst->lhs->type));
            print_val(b, inst->lhs);
            buf_append(b, "\n");
            break;

        case IR_OP_CALL:
            if (inst->result) {
                print_val(b, inst->result);
                buf_append(b, " = ");
            }
            buf_appendf(b, "call %s @%s(", ir_type_to_string(inst->type), inst->callee_name ? inst->callee_name : "fn");
            for (int i = 0; i < inst->arg_count; i++) {
                if (i > 0) buf_append(b, ", ");
                buf_appendf(b, "%s ", ir_type_to_string(inst->args[i]->type));
                print_val(b, inst->args[i]);
            }
            buf_append(b, ")\n");
            break;

        case IR_OP_PRINT:
            buf_appendf(b, "print %s ", ir_type_to_string(inst->lhs->type));
            print_val(b, inst->lhs);
            buf_append(b, "\n");
            break;

        case IR_OP_FREE:
            buf_append(b, "free ");
            print_val(b, inst->lhs);
            if (inst->is_array) buf_append(b, " (array)");
            buf_append(b, "\n");
            break;

        case IR_OP_RELEASE:
            buf_appendf(b, "release %s ", inst->class_name ? inst->class_name : "<class>");
            print_val(b, inst->lhs);
            if (inst->is_array) buf_append(b, " (array)");
            buf_append(b, "\n");
            break;

        case IR_OP_BR:
            buf_appendf(b, "br label %%%s\n", inst->target_true ? inst->target_true->name : "<unknown>");
            break;

        case IR_OP_CONDBR:
            buf_append(b, "condbr ");
            print_val(b, inst->lhs);
            buf_appendf(b, ", label %%%s, label %%%s\n",
                        inst->target_true ? inst->target_true->name : "<unknown>",
                        inst->target_false ? inst->target_false->name : "<unknown>");
            break;

        case IR_OP_RET:
            if (inst->lhs) {
                buf_appendf(b, "ret %s ", ir_type_to_string(inst->lhs->type));
                print_val(b, inst->lhs);
                buf_append(b, "\n");
            } else {
                buf_append(b, "ret void\n");
            }
            break;

        default:
            buf_appendf(b, "%s\n", ir_opcode_to_string(inst->op));
            break;
    }
}

char *ir_print_module(IrModule *module) {
    if (!module) return NULL;
    Buffer b = create_buf();

    buf_appendf(&b, "; Module: %s\n\n", module->name ? module->name : "unnamed");

    /* Struct declarations */
    for (IrStructDecl *st = module->first_struct; st; st = st->next) {
        buf_appendf(&b, "type @%s = { ", st->name);
        for (int i = 0; i < st->field_count; i++) {
            if (i > 0) buf_append(&b, ", ");
            buf_appendf(&b, "%s: %s", st->fields[i].name, ir_type_to_string(st->fields[i].type));
        }
        buf_append(&b, " }\n");
    }
    if (module->first_struct) buf_append(&b, "\n");

    /* Function declarations & definitions */
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        buf_appendf(&b, "fn @%s(", fn->name ? fn->name : "unnamed");
        for (int p = 0; p < fn->param_count; p++) {
            if (p > 0) buf_append(&b, ", ");
            buf_appendf(&b, "%%arg.%s: %s", fn->param_names[p], ir_type_to_string(fn->param_types[p]));
        }
        buf_appendf(&b, ") -> %s {\n", ir_type_to_string(fn->return_type));

        for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
            buf_appendf(&b, "%s:\n", bb->name ? bb->name : "block");
            for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
                print_instruction(&b, inst);
            }
        }
        buf_append(&b, "}\n\n");
    }

    return b.buffer;
}

void ir_dump_module(FILE *out, IrModule *module) {
    if (!out || !module) return;
    char *text = ir_print_module(module);
    if (text) {
        fputs(text, out);
        free(text);
    }
}

void ir_dump_function(FILE *out, IrFunction *fn) {
    if (!out || !fn) return;
    Buffer b = create_buf();
    buf_appendf(&b, "fn @%s(", fn->name ? fn->name : "unnamed");
    for (int p = 0; p < fn->param_count; p++) {
        if (p > 0) buf_append(&b, ", ");
        buf_appendf(&b, "%%arg.%s: %s", fn->param_names[p], ir_type_to_string(fn->param_types[p]));
    }
    buf_appendf(&b, ") -> %s {\n", ir_type_to_string(fn->return_type));

    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        buf_appendf(&b, "%s:\n", bb->name ? bb->name : "block");
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            print_instruction(&b, inst);
        }
    }
    buf_append(&b, "}\n\n");
    if (b.buffer) {
        fputs(b.buffer, out);
        free(b.buffer);
    }
}
