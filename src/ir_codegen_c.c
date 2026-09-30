// SPDX-License-Identifier: GPL-3.0-or-later

#define _POSIX_C_SOURCE 200809L
#include "ir_codegen_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} Buffer;

static Buffer create_buf(void) {
    Buffer b;
    b.capacity = 4096;
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
    char tmp[2048];
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    buf_append(b, tmp);
}

static void sanitize_name(const char *in, char *out, size_t max_len) {
    size_t idx = 0;
    while (*in && idx + 1 < max_len) {
        if (isalnum((unsigned char)*in) || *in == '_') {
            out[idx++] = *in;
        } else {
            out[idx++] = '_';
        }
        in++;
    }
    out[idx] = '\0';
}

static const char *c_type_str(const IrType *type) {
    if (!type) return "void";
    switch (type->kind) {
        case IR_TYPE_VOID: return "void";
        case IR_TYPE_I32:  return "int32_t";
        case IR_TYPE_I64:  return "int64_t";
        case IR_TYPE_F64:  return "double";
        case IR_TYPE_BOOL: return "bool";
        case IR_TYPE_CHAR: return "char";
        case IR_TYPE_PTR:
            if (!type->elem_type || type->elem_type->kind == IR_TYPE_VOID) return "void *";
            if (type->elem_type->kind == IR_TYPE_CHAR) return "char *";
            if (type->elem_type->kind == IR_TYPE_I32) return "int32_t *";
            if (type->elem_type->kind == IR_TYPE_I64) return "int64_t *";
            if (type->elem_type->kind == IR_TYPE_F64) return "double *";
            if (type->elem_type->kind == IR_TYPE_BOOL) return "bool *";
            if (type->elem_type->kind == IR_TYPE_STRUCT) {
                static char sbuf[128];
                snprintf(sbuf, sizeof(sbuf), "struct %s *", type->elem_type->name ? type->elem_type->name : "Anon");
                return sbuf;
            }
            return "void *";
        case IR_TYPE_STRUCT: {
            static char sbuf[128];
            snprintf(sbuf, sizeof(sbuf), "struct %s", type->name ? type->name : "Anon");
            return sbuf;
        }
        case IR_TYPE_ARRAY: return "void *";
        default: return "int32_t";
    }
}

static void print_c_val(Buffer *b, IrValue *v) {
    if (!v) {
        buf_append(b, "0");
        return;
    }
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            buf_appendf(b, "%ld", (long)v->const_val.int_val);
            break;
        case IR_VAL_CONST_FLOAT: {
            char fbuf[64];
            snprintf(fbuf, sizeof(fbuf), "%.17g", v->const_val.float_val);
            if (!strchr(fbuf, '.') && !strchr(fbuf, 'e') && !strchr(fbuf, 'E')) {
                buf_appendf(b, "%s.0", fbuf);
            } else {
                buf_append(b, fbuf);
            }
            break;
        }
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
            buf_appendf(b, "_r%d", v->id);
            break;
        case IR_VAL_VAR: {
            char clean[128];
            sanitize_name(v->name ? v->name : "var", clean, sizeof(clean));
            buf_appendf(b, "_slot_%s", clean);
            break;
        }
        case IR_VAL_PARAM: {
            char clean[128];
            sanitize_name(v->name ? v->name : "param", clean, sizeof(clean));
            buf_appendf(b, "__param_%s", clean);
            break;
        }
        case IR_VAL_GLOBAL:
            buf_appendf(b, "%s", v->name ? v->name : "global");
            break;
    }
}

/* Helper to collect all virtual registers in a function */
typedef struct RegEntry {
    int id;
    IrType *type;
    struct RegEntry *next;
} RegEntry;

static void gen_function_c(Buffer *b, IrFunction *fn) {
    /* Function prototype / signature */
    buf_appendf(b, "%s %s(", c_type_str(fn->return_type), fn->name ? fn->name : "unnamed");
    if (fn->param_count == 0) {
        buf_append(b, "void");
    } else {
        for (int p = 0; p < fn->param_count; p++) {
            if (p > 0) buf_append(b, ", ");
            char clean[128];
            sanitize_name(fn->param_names[p], clean, sizeof(clean));
            buf_appendf(b, "%s __param_%s", c_type_str(fn->param_types[p]), clean);
        }
    }
    buf_append(b, ") {\n");

    /* Collect all virtual registers and alloca slots */
    RegEntry *regs = NULL;
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            if (inst->result && inst->result->kind == IR_VAL_REG) {
                /* Insert if not present */
                bool found = false;
                for (RegEntry *r = regs; r; r = r->next) {
                    if (r->id == inst->result->id) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    RegEntry *nr = (RegEntry *)malloc(sizeof(RegEntry));
                    nr->id = inst->result->id;
                    nr->type = inst->result->type;
                    nr->next = regs;
                    regs = nr;
                }
            } else if (inst->op == IR_OP_ALLOCA && inst->result && inst->result->kind == IR_VAL_VAR) {
                char clean[128];
                sanitize_name(inst->result->name, clean, sizeof(clean));
                IrType *elem = inst->result->type->elem_type ? inst->result->type->elem_type : inst->result->type;
                buf_appendf(b, "    %s _val_%s;\n", c_type_str(elem), clean);
                buf_appendf(b, "    %s _slot_%s = &_val_%s;\n", c_type_str(inst->result->type), clean, clean);
            }
        }
    }

    /* Declare all virtual registers */
    for (RegEntry *r = regs; r; r = r->next) {
        buf_appendf(b, "    %s _r%d;\n", c_type_str(r->type), r->id);
    }

    /* Free temp reg entries */
    while (regs) {
        RegEntry *next = regs->next;
        free(regs);
        regs = next;
    }

    buf_append(b, "\n");

    /* Emit blocks and instructions */
    for (IrBasicBlock *bb = fn->first_block; bb; bb = bb->next) {
        char bb_clean[128];
        sanitize_name(bb->name ? bb->name : "block", bb_clean, sizeof(bb_clean));
        buf_appendf(b, "__bb_%s:;\n", bb_clean);

        for (IrInstruction *inst = bb->first_inst; inst; inst = inst->next) {
            switch (inst->op) {
                case IR_OP_ALLOCA:
                    /* Already declared at top of function */
                    break;

                case IR_OP_LOAD:
                    buf_append(b, "    ");
                    print_c_val(b, inst->result);
                    buf_append(b, " = *(");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ");\n");
                    break;

                case IR_OP_STORE:
                    buf_append(b, "    *(");
                    print_c_val(b, inst->rhs);
                    buf_append(b, ") = ");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ";\n");
                    break;

                case IR_OP_CONST:
                    buf_append(b, "    ");
                    print_c_val(b, inst->result);
                    buf_append(b, " = ");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ";\n");
                    break;

                case IR_OP_ADD:
                case IR_OP_SUB:
                case IR_OP_MUL:
                case IR_OP_DIV:
                case IR_OP_MOD: {
                    const char *sym = "+";
                    if (inst->op == IR_OP_SUB) sym = "-";
                    else if (inst->op == IR_OP_MUL) sym = "*";
                    else if (inst->op == IR_OP_DIV) sym = "/";
                    else if (inst->op == IR_OP_MOD) sym = "%";

                    buf_append(b, "    ");
                    print_c_val(b, inst->result);
                    buf_append(b, " = ");
                    print_c_val(b, inst->lhs);
                    buf_appendf(b, " %s ", sym);
                    print_c_val(b, inst->rhs);
                    buf_append(b, ";\n");
                    break;
                }

                case IR_OP_NEG:
                    buf_append(b, "    ");
                    print_c_val(b, inst->result);
                    buf_append(b, " = -");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ";\n");
                    break;

                case IR_OP_EQ:
                case IR_OP_NE:
                case IR_OP_LT:
                case IR_OP_LE:
                case IR_OP_GT:
                case IR_OP_GE:
                case IR_OP_AND:
                case IR_OP_OR: {
                    const char *sym = "==";
                    if (inst->op == IR_OP_NE) sym = "!=";
                    else if (inst->op == IR_OP_LT) sym = "<";
                    else if (inst->op == IR_OP_LE) sym = "<=";
                    else if (inst->op == IR_OP_GT) sym = ">";
                    else if (inst->op == IR_OP_GE) sym = ">=";
                    else if (inst->op == IR_OP_AND) sym = "&&";
                    else if (inst->op == IR_OP_OR) sym = "||";

                    buf_append(b, "    ");
                    print_c_val(b, inst->result);
                    buf_append(b, " = (");
                    print_c_val(b, inst->lhs);
                    buf_appendf(b, " %s ", sym);
                    print_c_val(b, inst->rhs);
                    buf_append(b, ");\n");
                    break;
                }

                case IR_OP_NOT:
                    buf_append(b, "    ");
                    print_c_val(b, inst->result);
                    buf_append(b, " = !(");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ");\n");
                    break;

                case IR_OP_CALL:
                    buf_append(b, "    ");
                    if (inst->result) {
                        print_c_val(b, inst->result);
                        buf_append(b, " = ");
                    }
                    buf_appendf(b, "%s(", inst->callee_name ? inst->callee_name : "fn");
                    for (int a = 0; a < inst->arg_count; a++) {
                        if (a > 0) buf_append(b, ", ");
                        print_c_val(b, inst->args[a]);
                    }
                    buf_append(b, ");\n");
                    break;

                case IR_OP_PRINT: {
                    buf_append(b, "    ");
                    if (inst->lhs->type->kind == IR_TYPE_BOOL) {
                        buf_append(b, "printf(\"%s\\n\", (");
                        print_c_val(b, inst->lhs);
                        buf_append(b, ") ? \"true\" : \"false\");\n");
                    } else if (inst->lhs->type->kind == IR_TYPE_I32 || inst->lhs->type->kind == IR_TYPE_I64) {
                        buf_append(b, "printf(\"%ld\\n\", (long)(");
                        print_c_val(b, inst->lhs);
                        buf_append(b, "));\n");
                    } else if (inst->lhs->type->kind == IR_TYPE_F64) {
                        buf_append(b, "printf(\"%g\\n\", (double)(");
                        print_c_val(b, inst->lhs);
                        buf_append(b, "));\n");
                    } else if (inst->lhs->type->kind == IR_TYPE_CHAR) {
                        buf_append(b, "printf(\"%c\\n\", (char)(");
                        print_c_val(b, inst->lhs);
                        buf_append(b, "));\n");
                    } else if (inst->lhs->type->kind == IR_TYPE_PTR && inst->lhs->type->elem_type && inst->lhs->type->elem_type->kind == IR_TYPE_CHAR) {
                        buf_append(b, "printf(\"%s\\n\", (char *)(");
                        print_c_val(b, inst->lhs);
                        buf_append(b, "));\n");
                    } else {
                        buf_append(b, "printf(\"%p\\n\", (void *)(");
                        print_c_val(b, inst->lhs);
                        buf_append(b, "));\n");
                    }
                    break;
                }

                case IR_OP_FREE:
                    buf_append(b, "    if (");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ") { free((void *)(");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ")); }\n");
                    break;

                case IR_OP_RELEASE:
                    buf_append(b, "    if (");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ") { free((void *)(");
                    print_c_val(b, inst->lhs);
                    buf_append(b, ")); }\n");
                    break;

                case IR_OP_BR: {
                    char target_clean[128];
                    sanitize_name(inst->target_true ? inst->target_true->name : "block", target_clean, sizeof(target_clean));
                    buf_appendf(b, "    goto __bb_%s;\n", target_clean);
                    break;
                }

                case IR_OP_CONDBR: {
                    char true_clean[128];
                    char false_clean[128];
                    sanitize_name(inst->target_true ? inst->target_true->name : "block", true_clean, sizeof(true_clean));
                    sanitize_name(inst->target_false ? inst->target_false->name : "block", false_clean, sizeof(false_clean));
                    buf_append(b, "    if (");
                    print_c_val(b, inst->lhs);
                    buf_appendf(b, ") { goto __bb_%s; } else { goto __bb_%s; }\n", true_clean, false_clean);
                    break;
                }

                case IR_OP_RET:
                    if (inst->lhs) {
                        buf_append(b, "    return ");
                        print_c_val(b, inst->lhs);
                        buf_append(b, ";\n");
                    } else {
                        buf_append(b, "    return;\n");
                    }
                    break;

                default:
                    break;
            }
        }
    }

    buf_append(b, "}\n\n");
}

char *ir_generate_c(IrModule *module) {
    if (!module) return NULL;
    Buffer b = create_buf();

    /* Standard headers */
    buf_append(&b, "// Generated by Cco Compiler via Intermediate Representation (IR)\n");
    buf_append(&b, "#define _POSIX_C_SOURCE 200809L\n");
    buf_append(&b, "#if defined(__GNUC__) || defined(__clang__)\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wunused-label\"\n");
    buf_append(&b, "#pragma GCC diagnostic ignored \"-Wunused-variable\"\n");
    buf_append(&b, "#endif\n");
    buf_append(&b, "#include <stdio.h>\n");
    buf_append(&b, "#include <stdlib.h>\n");
    buf_append(&b, "#include <stdbool.h>\n");
    buf_append(&b, "#include <stdint.h>\n");
    buf_append(&b, "#include <string.h>\n");
    buf_append(&b, "#include <math.h>\n\n");

    /* Struct definitions */
    for (IrStructDecl *st = module->first_struct; st; st = st->next) {
        buf_appendf(&b, "typedef struct %s %s;\n", st->name, st->name);
    }
    for (IrStructDecl *st = module->first_struct; st; st = st->next) {
        buf_appendf(&b, "struct %s {\n", st->name);
        for (int f = 0; f < st->field_count; f++) {
            buf_appendf(&b, "    %s %s;\n", c_type_str(st->fields[f].type), st->fields[f].name);
        }
        buf_append(&b, "};\n\n");
    }

    /* Function prototypes */
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        if (strcmp(fn->name, "main") == 0) continue;
        buf_appendf(&b, "%s %s(", c_type_str(fn->return_type), fn->name ? fn->name : "unnamed");
        if (fn->param_count == 0) {
            buf_append(&b, "void");
        } else {
            for (int p = 0; p < fn->param_count; p++) {
                if (p > 0) buf_append(&b, ", ");
                char clean[128];
                sanitize_name(fn->param_names[p], clean, sizeof(clean));
                buf_appendf(&b, "%s __param_%s", c_type_str(fn->param_types[p]), clean);
            }
        }
        buf_append(&b, ");\n");
    }
    if (module->first_fn) buf_append(&b, "\n");

    /* Function definitions */
    for (IrFunction *fn = module->first_fn; fn; fn = fn->next) {
        gen_function_c(&b, fn);
    }

    return b.buffer;
}
