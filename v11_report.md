# Cco v11 Engineering Report: Tagged Unions (`enum`) & Pattern Matching (`match`)

**Version**: v11.0  
**Status**: Completed & Verified  
**Compiler Standard**: Strict C11 (`-Wall -Wextra -Werror -pedantic-errors -std=c11`)  
**Test Suite**: 61/61 integration & unit tests passing with **0 Valgrind memory leaks**  

---

## 1. Executive Summary & Goals

Cco v11 represents the third foundational milestone toward full compiler self-hosting (v9 growable arrays $\to$ v10 hash maps $\to$ v11 tagged unions $\to$ v12 self-hosted lexer & parser proof-of-concept).

Before v11, representing heterogeneous trees (like an Abstract Syntax Tree) or sum types in Cco required flat classes with null pointers or manual integer tags. v11 introduces first-class **heap-allocated tagged unions (`enum`)** and **compile-time verified pattern matching (`match`)** with borrow-only field bindings and automated recursive cleanup cascades.

---

## 2. Language Grammar & Syntax Additions

### 2.1 Enum Declarations
Enums support both unit variants (no payload) and payload variants with named fields:

```cco
enum NodeKind {
    Number { value: int },
    Add { left: NodeKind, right: NodeKind },
    Mul { left: NodeKind, right: NodeKind },
    Eof,
}
```

- Variant fields can be primitives (`int`, `float`, `bool`, `char`), heap strings (`string`), heap classes, or enum types.
- Variant fields cannot be array or map types in v11.

### 2.2 Construction Syntax
- **Payload Variant**: `<Enum>.<Variant> { field: value, ... }`
  ```cco
  let leaf: NodeKind = NodeKind.Number { value: 42 };
  ```
- **Unit Variant**: `<Enum>.<Variant>`
  ```cco
  let token: NodeKind = NodeKind.Eof;
  ```

### 2.3 Pattern Matching (`match`)
```cco
fn eval(n: &NodeKind) -> int {
    match n {
        NodeKind.Number { value } => {
            return value;
        }
        NodeKind.Add { left, right } => {
            return eval(left) + eval(right);
        }
        NodeKind.Mul { left, right } => {
            return eval(left) * eval(right);
        }
        _ => {
            return 0;
        }
    }
}
```

- **Borrow-Only Semantics**: Scrutinees (`n`) are borrowed references. Pattern bindings (`value`, `left`, `right`) are registered as borrowed aliases directly into the scrutinee's union payload and are **never freed** when leaving the match arm.
- **Strict Binding Names**: Bound local variables must match declared variant field names. Renaming (`left: l`) triggers a compile-time error.
- **Exhaustiveness**: A match statement without a wildcard `_ => { ... }` arm must cover 100% of declared variants.

---

## 3. C Runtime Representation & Codegen Strategy

### 3.1 Tagged Union Structs
Every Cco `enum` declaration generates:
1. An anonymous tag enum `__cco_<Enum>_tag` with entries `<ENUM>_TAG_<Variant>`.
2. A typed C struct containing the tag and a nested anonymous union `as`:

```c
typedef enum {
    EXPR_TAG_Num,
    EXPR_TAG_Add,
    EXPR_TAG_Mul,
    EXPR_TAG_Eof
} __cco_Expr_tag;

typedef struct Expr Expr;

struct Expr {
    __cco_Expr_tag tag;
    union {
        struct { int value; } Num;
        struct { Expr * left; Expr * right; } Add;
        struct { Expr * left; Expr * right; } Mul;
    } as;
};
```

### 3.2 Constructors & Recursive Free Cascades
- **Constructors**: Each variant generates `<Enum>_new_<Variant>(...)` allocating `sizeof(Enum)` on the heap and populating the tag and fields.
- **Destructors**: `<Enum>_free(p)` recursively switches on `p->tag`, frees any child classes/enums or strings, and calls `free(p)`:

```c
static inline void Expr_free(Expr *p) {
    if (!p) return;
    switch (p->tag) {
        case EXPR_TAG_Num:
            break;
        case EXPR_TAG_Add:
            Expr_free(p->as.Add.left);
            Expr_free(p->as.Add.right);
            break;
        case EXPR_TAG_Mul:
            Expr_free(p->as.Mul.left);
            Expr_free(p->as.Mul.right);
            break;
        case EXPR_TAG_Eof:
            break;
    }
    free(p);
}
```

### 3.3 Match Desugaring
```c
Expr *__match_scrut_1 = ast;
switch (__match_scrut_1->tag) {
    case EXPR_TAG_Num: {
        int value = __match_scrut_1->as.Num.value;
        (void)value;
        return value;
        break;
    }
    ...
}
```

---

## 4. Compile-Time Diagnostics & Safety Rules

| Feature / Invariant | Diagnostic Behavior | Verified By |
|---|---|---|
| **Non-Exhaustive Match** | Reports missing variant names and suggests adding them or a `_` arm | Test 59 (`59_enum_nonexhaustive_ERROR.cco`) |
| **Duplicate Match Arm** | Emits primary error at redundant arm and secondary note with caret at first handled location | Test 60 (`60_enum_duplicate_arm_ERROR.cco`) |
| **Field Renaming Rejection** | Rejects renamed field patterns (`field: f`) | Test 61 (`61_enum_field_rename_ERROR.cco`) |
| **Single Ownership / Move** | Moving enum transfers ownership; use-after-move is rejected | Test 57 (`57_enum_ownership_move.cco`) |
| **Borrow-Only Pattern Arms** | Match bindings do not take ownership or double-free | Test 58 (`58_enum_borrowed_match.cco`) |

---

## 5. Integration Test Suite Matrix (01–61)

```
==================================================
  Cco (C--) INTEGRATION & VALGRIND TEST SUITE    
==================================================
Testing 01_hello... PASSED (Diff Clean + 0 Leaks)
Testing 02_alloc_basic... PASSED (Diff Clean + 0 Leaks)
Testing 03_early_return... PASSED (Diff Clean + 0 Leaks)
Testing 04_loop_alloc... PASSED (Diff Clean + 0 Leaks)
Testing 05_ownership_transfer... PASSED (Diff Clean + 0 Leaks)
Testing 06_nested_scopes... PASSED (Diff Clean + 0 Leaks)
Testing 07_break_continue... PASSED (Diff Clean + 0 Leaks)
Testing 08_reassign_alloc... PASSED (Diff Clean + 0 Leaks)
Testing 09_basic_class... PASSED (Diff Clean + 0 Leaks)
Testing 10_method_call... PASSED (Diff Clean + 0 Leaks)
Testing 11_aliasing_refcount... PASSED (Diff Clean + 0 Leaks)
Testing 12_reassign_object... PASSED (Diff Clean + 0 Leaks)
Testing 13_object_early_return... PASSED (Diff Clean + 0 Leaks)
Testing 14_basic_move... PASSED (Diff Clean + 0 Leaks)
Testing 15_borrowed_param... PASSED (Diff Clean + 0 Leaks)
Testing 16_use_after_move_ERROR... PASSED (Compilation Failed as Expected)
Testing 17_double_move_ERROR... PASSED (Compilation Failed as Expected)
Testing 18_conditional_move_ERROR... PASSED (Compilation Failed as Expected)
Testing 19_move_via_return... PASSED (Diff Clean + 0 Leaks)
Testing 20_return_borrowed_ERROR... PASSED (Compilation Failed as Expected)
Testing 21_stdlib_string... PASSED (Diff Clean + 0 Leaks)
Testing 22_stdlib_math... PASSED (Diff Clean + 0 Leaks)
Testing 23_stdlib_file_io... PASSED (Diff Clean + 0 Leaks)
Testing 24_object_array_basic... PASSED (Diff Clean + 0 Leaks)
Testing 25_object_array_foreach... PASSED (Diff Clean + 0 Leaks)
Testing 26_object_array_free_cascade... PASSED (Diff Clean + 0 Leaks)
Testing 27_object_array_move_out_ERROR... PASSED (Compilation Failed as Expected)
Testing 28_object_array_bounds_ERROR... PASSED (Compilation Failed as Expected)
Testing 29_import_basic... PASSED (Diff Clean + 0 Leaks)
Testing 30_import_diamond... PASSED (Diff Clean + 0 Leaks)
Testing 31_import_circular_ERROR... PASSED (Compilation Failed as Expected)
Testing 32_import_duplicate_symbol_ERROR... PASSED (Compilation Failed as Expected)
Testing 33_struct_basic... PASSED (Diff Clean + 0 Leaks)
Testing 34_struct_copy_semantics... PASSED (Diff Clean + 0 Leaks)
Testing 35_struct_borrowed_param... PASSED (Diff Clean + 0 Leaks)
Testing 36_struct_nonprimitive_field_ERROR... PASSED (Compilation Failed as Expected)
Testing 37_struct_class_name_collision_ERROR... PASSED (Compilation Failed as Expected)
Testing 38_prelude_minimal... PASSED (Diff Clean + 0 Leaks)
Testing 39_prelude_partial... PASSED (Diff Clean + 0 Leaks)
Testing 40_prelude_transitive... PASSED (Diff Clean + 0 Leaks)
Testing 41_growable_push_primitive... PASSED (Diff Clean + 0 Leaks)
Testing 42_growable_pop_primitive... PASSED (Diff Clean + 0 Leaks)
Testing 43_growable_push_pop_class... PASSED (Diff Clean + 0 Leaks)
Testing 44_growable_realloc_stress... PASSED (Diff Clean + 0 Leaks)
Testing 45_pop_empty_RUNTIME_ERROR... PASSED (Runtime Failed as Expected)
Testing 46_map_basic_primitive... PASSED (Diff Clean + 0 Leaks)
Testing 47_map_string_key... PASSED (Diff Clean + 0 Leaks)
Testing 48_map_class_value_ownership... PASSED (Diff Clean + 0 Leaks)
Testing 49_map_remove_and_tombstones... PASSED (Diff Clean + 0 Leaks)
Testing 50_map_keys_and_len... PASSED (Diff Clean + 0 Leaks)
Testing 51_map_rehash_stress... PASSED (Diff Clean + 0 Leaks)
Testing 52_map_invalid_key_type_ERROR... PASSED (Compilation Failed as Expected)
Testing 53_map_put_reassignment_check_ERROR... PASSED (Compilation Failed as Expected)
Testing 54_enum_unit_variants... PASSED (Diff Clean + 0 Leaks)
Testing 55_enum_payload_variants... PASSED (Diff Clean + 0 Leaks)
Testing 56_enum_recursive_eval... PASSED (Diff Clean + 0 Leaks)
Testing 57_enum_ownership_move... PASSED (Diff Clean + 0 Leaks)
Testing 58_enum_borrowed_match... PASSED (Diff Clean + 0 Leaks)
Testing 59_enum_nonexhaustive_ERROR... PASSED (Compilation Failed as Expected)
Testing 60_enum_duplicate_arm_ERROR... PASSED (Compilation Failed as Expected)
Testing 61_enum_field_rename_ERROR... PASSED (Compilation Failed as Expected)
--------------------------------------------------
Summary: 61 Passed, 0 Failed
==================================================
```
