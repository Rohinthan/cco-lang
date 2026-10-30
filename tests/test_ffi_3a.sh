#!/usr/bin/env bash
# ==============================================================================
# tests/test_ffi_3a.sh
# Cco FFI-3A: Core Raw Pointer Support Test Suite
# Positive & Negative Verification, Parity, and Memory Safety Tests
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_ffi3a_XXXXXX)

trap 'rm -rf "$TMP_DIR"' EXIT

passed_tests=0
failed_tests=0
total_tests=0

run_test() {
    local test_name="$1"
    total_tests=$((total_tests + 1))
    printf "[%2d] %s ... " "$total_tests" "$test_name"
}

pass_test() {
    passed_tests=$((passed_tests + 1))
    printf "PASSED\n"
}

fail_test() {
    local reason="$1"
    failed_tests=$((failed_tests + 1))
    printf "FAILED (%s)\n" "$reason"
}

if [ ! -x "$CCO" ]; then
    echo "Compiler binary '$CCO' not found. Building with make..."
    (cd "$REPO_ROOT" && make cco)
fi

echo "=============================================================="
echo "Cco FFI-3A Core Raw Pointer Test Suite"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Positive Test 1: Null literal initialization and comparison against null
# ------------------------------------------------------------------------------
run_test "Positive 1: null literal initialization and comparison (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos1.cco"
fn main() -> int {
    let p: *void = null
    if p == null {
        if p != null {
            return 1
        }
        print("PASS_NULL")
        return 0
    }
    return 2
}
EOF

"$CCO" "$TMP_DIR/pos1.cco" -o "$TMP_DIR/pos1_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos1_c11")
"$CCO" "$TMP_DIR/pos1.cco" --use-native -o "$TMP_DIR/pos1_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos1_native")

if [ "$out_c11" = "PASS_NULL" ] && [ "$out_nat" = "PASS_NULL" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 2: getenv returns *char, pass to puts, no double-free
# ------------------------------------------------------------------------------
run_test "Positive 2: getenv (*char) passed to puts (*char) without free crash"
cat << 'EOF' > "$TMP_DIR/pos2.cco"
extern "C" {
    fn getenv(name: string) -> *char
    fn puts(s: *char) -> int
}

fn main() -> int {
    let p: *char = getenv("PATH")
    if p != null {
        puts(p)
        return 0
    }
    return 1
}
EOF

if "$CCO" "$TMP_DIR/pos2.cco" -o "$TMP_DIR/pos2_c11" > /dev/null 2>&1 && \
   "$CCO" "$TMP_DIR/pos2.cco" --use-native -o "$TMP_DIR/pos2_native" > /dev/null 2>&1; then
    out_c11=$("$TMP_DIR/pos2_c11")
    out_nat=$("$TMP_DIR/pos2_native")
    if [ "$out_c11" = "$out_nat" ] && [ -n "$out_c11" ]; then
        pass_test
    else
        fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
    fi
else
    fail_test "build failed"
fi

# ------------------------------------------------------------------------------
# Positive Test 3: fopen returns *void, passed to fclose(*void)
# ------------------------------------------------------------------------------
run_test "Positive 3: opaque handle fopen (*void) and fclose (*void)"
cat << 'EOF' > "$TMP_DIR/pos3.cco"
extern "C" {
    fn fopen(path: string, mode: string) -> *void
    fn fclose(stream: *void) -> int
}

fn main() -> int {
    let handle: *void = fopen("/dev/null", "r")
    if handle != null {
        let rc: int = fclose(handle)
        return rc
    }
    return 1
}
EOF

if "$CCO" "$TMP_DIR/pos3.cco" -o "$TMP_DIR/pos3_c11" > /dev/null 2>&1 && \
   "$CCO" "$TMP_DIR/pos3.cco" --use-native -o "$TMP_DIR/pos3_native" > /dev/null 2>&1; then
    "$TMP_DIR/pos3_c11"
    rc_c11=$?
    "$TMP_DIR/pos3_native"
    rc_nat=$?
    if [ "$rc_c11" -eq 0 ] && [ "$rc_nat" -eq 0 ]; then
        pass_test
    else
        fail_test "exit code mismatch: c11=$rc_c11, nat=$rc_nat"
    fi
else
    fail_test "build failed"
fi

# ------------------------------------------------------------------------------
# Positive Test 4: Pointer equality: p == p, p != q, null == null
# ------------------------------------------------------------------------------
run_test "Positive 4: pointer equality and distinctness (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos4.cco"
extern "C" {
    fn getenv(name: string) -> *char
}

fn main() -> int {
    let p1: *char = getenv("PATH")
    let p2: *char = getenv("PATH")
    let p_null: *char = null

    if p1 == p2 {
        if p1 != p_null {
            if p_null == null {
                print("EQUALITY_OK")
                return 0
            }
        }
    }
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos4.cco" -o "$TMP_DIR/pos4_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos4_c11")
"$CCO" "$TMP_DIR/pos4.cco" --use-native -o "$TMP_DIR/pos4_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos4_native")

if [ "$out_c11" = "EQUALITY_OK" ] && [ "$out_nat" = "EQUALITY_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 5: Typed pointers and nested pointers (*int, *float, **char)
# ------------------------------------------------------------------------------
run_test "Positive 5: typed pointers and nested pointer declarations (**char)"
cat << 'EOF' > "$TMP_DIR/pos5.cco"
fn main() -> int {
    let pi: *int = null
    let pf: *float = null
    let ppc: **char = null

    if pi == null {
        if pf == null {
            if ppc == null {
                print("TYPES_OK")
                return 0
            }
        }
    }
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos5.cco" -o "$TMP_DIR/pos5_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos5_c11")
"$CCO" "$TMP_DIR/pos5.cco" --use-native -o "$TMP_DIR/pos5_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos5_native")

if [ "$out_c11" = "TYPES_OK" ] && [ "$out_nat" = "TYPES_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 6: Non-owning pointer loop lifecycle under Valgrind
# ------------------------------------------------------------------------------
run_test "Positive 6: non-owning pointer in loop with 0 leaks (Valgrind)"
cat << 'EOF' > "$TMP_DIR/pos6.cco"
extern "C" {
    fn getenv(name: string) -> *char
}

fn check_env() -> int {
    let p: *char = getenv("PATH")
    if p != null {
        return 1
    }
    return 0
}

fn main() -> int {
    let mut_count: int = 0
    for (let i: int = 0; i < 50; i = i + 1) {
        mut_count = mut_count + check_env()
    }
    if mut_count == 50 {
        print("LIFECYCLE_OK")
        return 0
    }
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos6.cco" -o "$TMP_DIR/pos6_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
"$CCO" "$TMP_DIR/pos6.cco" --use-native -o "$TMP_DIR/pos6_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }

valgrind --leak-check=full --error-exitcode=99 "$TMP_DIR/pos6_c11" > /dev/null 2>&1
val_c11=$?
valgrind --leak-check=full --error-exitcode=99 "$TMP_DIR/pos6_native" > /dev/null 2>&1
val_nat=$?

if [ "$val_c11" -eq 0 ] && [ "$val_nat" -eq 0 ]; then
    pass_test
else
    fail_test "valgrind reported leaks/errors: c11=$val_c11, nat=$val_nat"
fi

# ------------------------------------------------------------------------------
# Positive Test 7: Passing *void handle between functions
# ------------------------------------------------------------------------------
run_test "Positive 7: passing *void handle across normal Cco functions"
cat << 'EOF' > "$TMP_DIR/pos7.cco"
extern "C" {
    fn fopen(path: string, mode: string) -> *void
    fn fclose(stream: *void) -> int
}

fn close_file(f: *void) -> int {
    if f != null {
        return fclose(f)
    }
    return -1
}

fn main() -> int {
    let file_handle: *void = fopen("/dev/null", "r")
    let rc: int = close_file(file_handle)
    return rc
}
EOF

if "$CCO" "$TMP_DIR/pos7.cco" -o "$TMP_DIR/pos7_c11" > /dev/null 2>&1 && \
   "$CCO" "$TMP_DIR/pos7.cco" --use-native -o "$TMP_DIR/pos7_native" > /dev/null 2>&1; then
    "$TMP_DIR/pos7_c11"
    rc_c11=$?
    "$TMP_DIR/pos7_native"
    rc_nat=$?
    if [ "$rc_c11" -eq 0 ] && [ "$rc_nat" -eq 0 ]; then
        pass_test
    else
        fail_test "exit code mismatch: c11=$rc_c11, nat=$rc_nat"
    fi
else
    fail_test "build failed"
fi

# ------------------------------------------------------------------------------
# Positive Test 8: Compatibility between *void and *T
# ------------------------------------------------------------------------------
run_test "Positive 8: *void compatibility with any pointer type"
cat << 'EOF' > "$TMP_DIR/pos8.cco"
extern "C" {
    fn getenv(name: string) -> *char
}

fn inspect_handle(h: *void) -> int {
    if h != null {
        return 1
    }
    return 0
}

fn main() -> int {
    let p: *char = getenv("PATH")
    # *char can be passed to parameter of type *void
    let res: int = inspect_handle(p)
    if res == 1 {
        print("GENERIC_OK")
        return 0
    }
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos8.cco" -o "$TMP_DIR/pos8_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos8_c11")
"$CCO" "$TMP_DIR/pos8.cco" --use-native -o "$TMP_DIR/pos8_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos8_native")

if [ "$out_c11" = "GENERIC_OK" ] && [ "$out_nat" = "GENERIC_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 9: Pointer variable reassignment
# ------------------------------------------------------------------------------
run_test "Positive 9: pointer variable reassignment (p = null, p = handle)"
cat << 'EOF' > "$TMP_DIR/pos9.cco"
extern "C" {
    fn getenv(name: string) -> *char
}

fn main() -> int {
    let p: *char = null
    if p != null { return 1 }
    p = getenv("PATH")
    if p == null { return 2 }
    p = null
    if p != null { return 3 }
    print("REASSIGN_OK")
    return 0
}
EOF

if "$CCO" "$TMP_DIR/pos9.cco" -o "$TMP_DIR/pos9_c11" > /dev/null 2>&1 && \
   "$CCO" "$TMP_DIR/pos9.cco" --use-native -o "$TMP_DIR/pos9_native" > /dev/null 2>&1; then
    out_c11=$("$TMP_DIR/pos9_c11")
    out_nat=$("$TMP_DIR/pos9_native")
    if [ "$out_c11" = "REASSIGN_OK" ] && [ "$out_nat" = "REASSIGN_OK" ]; then
        pass_test
    else
        fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
    fi
else
    fail_test "build failed"
fi

# ------------------------------------------------------------------------------
# Positive Test 10: Custom C library with handles (-L and -l)
# ------------------------------------------------------------------------------
run_test "Positive 10: custom native C library handle linking via -L and -l"
mkdir -p "$TMP_DIR/custom_lib"
cat << 'EOF' > "$TMP_DIR/custom_lib/ctx.c"
#include <stdlib.h>
struct Context { int id; };
void *create_context(int id) {
    struct Context *c = (struct Context *)malloc(sizeof(struct Context));
    if (c) c->id = id;
    return c;
}
int get_context_id(void *ctx) {
    if (!ctx) return -1;
    return ((struct Context *)ctx)->id;
}
void free_context(void *ctx) {
    free(ctx);
}
EOF
gcc -c -O2 "$TMP_DIR/custom_lib/ctx.c" -o "$TMP_DIR/custom_lib/ctx.o"
ar rcs "$TMP_DIR/custom_lib/libcustomctx.a" "$TMP_DIR/custom_lib/ctx.o"

cat << 'EOF' > "$TMP_DIR/pos10.cco"
extern "C" {
    fn create_context(id: int) -> *void
    fn get_context_id(ctx: *void) -> int
    fn free_context(ctx: *void) -> void
}

fn main() -> int {
    let ctx: *void = create_context(42)
    if ctx == null { return 1 }
    let id: int = get_context_id(ctx)
    free_context(ctx)
    if id == 42 {
        print("CUSTOM_LIB_HANDLE_OK")
        return 0
    }
    return 2
}
EOF

if "$CCO" "$TMP_DIR/pos10.cco" -L"$TMP_DIR/custom_lib" -lcustomctx -o "$TMP_DIR/pos10_c11" > /dev/null 2>&1 && \
   "$CCO" "$TMP_DIR/pos10.cco" --use-native -L"$TMP_DIR/custom_lib" -lcustomctx -o "$TMP_DIR/pos10_native" > /dev/null 2>&1; then
    out_c11=$("$TMP_DIR/pos10_c11")
    out_nat=$("$TMP_DIR/pos10_native")
    if [ "$out_c11" = "CUSTOM_LIB_HANDLE_OK" ] && [ "$out_nat" = "CUSTOM_LIB_HANDLE_OK" ]; then
        pass_test
    else
        fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
    fi
else
    fail_test "build failed"
fi

# ------------------------------------------------------------------------------
# Positive Test 11: Valgrind memory check on Custom C Library Handles
# ------------------------------------------------------------------------------
run_test "Positive 11: custom C library handle allocation & release (Valgrind)"
valgrind --leak-check=full --error-exitcode=99 "$TMP_DIR/pos10_c11" > /dev/null 2>&1
val_c11=$?
valgrind --leak-check=full --error-exitcode=99 "$TMP_DIR/pos10_native" > /dev/null 2>&1
val_nat=$?

if [ "$val_c11" -eq 0 ] && [ "$val_nat" -eq 0 ]; then
    pass_test
else
    fail_test "valgrind reported leaks/errors: c11=$val_c11, nat=$val_nat"
fi

# ------------------------------------------------------------------------------
# Negative Test 1: Pointer addition (p + 1) rejected
# ------------------------------------------------------------------------------
run_test "Negative 1: pointer addition (p + 1) rejected"
cat << 'EOF' > "$TMP_DIR/neg1.cco"
fn main() -> int {
    let p: *char = null
    let q: *char = p + 1
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg1.cco" -o "$TMP_DIR/neg1_bin" > "$TMP_DIR/neg1.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer arithmetic is not supported" "$TMP_DIR/neg1.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg1.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 2: Pointer subtraction (p - 1) rejected
# ------------------------------------------------------------------------------
run_test "Negative 2: pointer subtraction (p - 1) rejected"
cat << 'EOF' > "$TMP_DIR/neg2.cco"
fn main() -> int {
    let p: *char = null
    let q: *char = p - 1
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg2.cco" -o "$TMP_DIR/neg2_bin" > "$TMP_DIR/neg2.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer arithmetic is not supported" "$TMP_DIR/neg2.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg2.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 3: Pointer dereference (*p) rejected
# ------------------------------------------------------------------------------
run_test "Negative 3: pointer dereference (*p) rejected"
cat << 'EOF' > "$TMP_DIR/neg3.cco"
fn main() -> int {
    let p: *int = null
    let val: int = *p
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg3.cco" -o "$TMP_DIR/neg3_bin" > "$TMP_DIR/neg3.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "raw pointer dereferencing is not supported in safe code" "$TMP_DIR/neg3.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg3.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 4: Implicit conversion from *char to string rejected
# ------------------------------------------------------------------------------
run_test "Negative 4: implicit conversion from *char to string rejected"
cat << 'EOF' > "$TMP_DIR/neg4.cco"
fn main() -> int {
    let p: *char = null
    let s: string = p
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg4.cco" -o "$TMP_DIR/neg4_bin" > "$TMP_DIR/neg4.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg4.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg4.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 5: Implicit conversion from string to *char rejected
# ------------------------------------------------------------------------------
run_test "Negative 5: implicit conversion from string to *char rejected"
cat << 'EOF' > "$TMP_DIR/neg5.cco"
fn main() -> int {
    let s: string = "hello"
    let p: *char = s
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg5.cco" -o "$TMP_DIR/neg5_bin" > "$TMP_DIR/neg5.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg5.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg5.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 6: Assigning null to non-pointer (int) rejected
# ------------------------------------------------------------------------------
run_test "Negative 6: assigning null to integer variable rejected"
cat << 'EOF' > "$TMP_DIR/neg6.cco"
fn main() -> int {
    let x: int = null
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg6.cco" -o "$TMP_DIR/neg6_bin" > "$TMP_DIR/neg6.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg6.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg6.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 7: Comparing pointer to integer (p == 0) rejected
# ------------------------------------------------------------------------------
run_test "Negative 7: comparing pointer with integer (p == 0) rejected"
cat << 'EOF' > "$TMP_DIR/neg7.cco"
fn main() -> int {
    let p: *char = null
    if p == 0 {
        return 1
    }
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg7.cco" -o "$TMP_DIR/neg7_bin" > "$TMP_DIR/neg7.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "cannot compare pointer with non-pointer type" "$TMP_DIR/neg7.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg7.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 8: Comparing distinct pointer types (*char == *int) rejected
# ------------------------------------------------------------------------------
run_test "Negative 8: comparing distinct pointer types (*char == *int) rejected"
cat << 'EOF' > "$TMP_DIR/neg8.cco"
fn main() -> int {
    let pc: *char = null
    let pi: *int = null
    if pc == pi {
        return 1
    }
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg8.cco" -o "$TMP_DIR/neg8_bin" > "$TMP_DIR/neg8.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "cannot compare distinct pointer types" "$TMP_DIR/neg8.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg8.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 9: Pointer indexing (p[0]) rejected
# ------------------------------------------------------------------------------
run_test "Negative 9: pointer indexing (p[0]) rejected"
cat << 'EOF' > "$TMP_DIR/neg9.cco"
fn main() -> int {
    let p: *char = null
    let c: char = p[0]
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg9.cco" -o "$TMP_DIR/neg9_bin" > "$TMP_DIR/neg9.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer indexing is not supported" "$TMP_DIR/neg9.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg9.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 10: Pointer relational comparison (p < q) rejected
# ------------------------------------------------------------------------------
run_test "Negative 10: pointer relational comparison (p < q) rejected"
cat << 'EOF' > "$TMP_DIR/neg10.cco"
fn main() -> int {
    let p: *char = null
    let q: *char = null
    if p < q {
        return 1
    }
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg10.cco" -o "$TMP_DIR/neg10_bin" > "$TMP_DIR/neg10.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer arithmetic is not supported" "$TMP_DIR/neg10.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg10.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 11: Bare void parameter rejected in extern "C"
# ------------------------------------------------------------------------------
run_test "Negative 11: bare void parameter rejected in extern \"C\""
cat << 'EOF' > "$TMP_DIR/neg11.cco"
extern "C" {
    fn bad_fn(x: void) -> int
}

fn main() -> int {
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg11.cco" -o "$TMP_DIR/neg11_bin" > "$TMP_DIR/neg11.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "void is not a valid parameter type" "$TMP_DIR/neg11.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg11.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 12: Invalid pointer syntax (* alone) rejected
# ------------------------------------------------------------------------------
run_test "Negative 12: incomplete pointer syntax (* without type) rejected"
cat << 'EOF' > "$TMP_DIR/neg12.cco"
fn main() -> int {
    let p: * = null
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg12.cco" -o "$TMP_DIR/neg12_bin" > "$TMP_DIR/neg12.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "Expected type specification" "$TMP_DIR/neg12.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg12.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 13: Pointer multiplication (p * 2) rejected
# ------------------------------------------------------------------------------
run_test "Negative 13: pointer multiplication (p * 2) rejected"
cat << 'EOF' > "$TMP_DIR/neg13.cco"
fn main() -> int {
    let p: *char = null
    let q: *char = p * 2
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg13.cco" -o "$TMP_DIR/neg13_bin" > "$TMP_DIR/neg13.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer arithmetic is not supported" "$TMP_DIR/neg13.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg13.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 14: Pointer division (p / 2) rejected
# ------------------------------------------------------------------------------
run_test "Negative 14: pointer division (p / 2) rejected"
cat << 'EOF' > "$TMP_DIR/neg14.cco"
fn main() -> int {
    let p: *char = null
    let q: *char = p / 2
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg14.cco" -o "$TMP_DIR/neg14_bin" > "$TMP_DIR/neg14.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer arithmetic is not supported" "$TMP_DIR/neg14.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg14.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 15: Compound assignment (p += 1) rejected
# ------------------------------------------------------------------------------
run_test "Negative 15: compound assignment with pointer (p += 1) rejected"
cat << 'EOF' > "$TMP_DIR/neg15.cco"
fn main() -> int {
    let p: *char = null
    p += 1
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg15.cco" -o "$TMP_DIR/neg15_bin" > "$TMP_DIR/neg15.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer arithmetic is not supported" "$TMP_DIR/neg15.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg15.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 16: Increment operator (p++) rejected
# ------------------------------------------------------------------------------
run_test "Negative 16: increment operator on pointer (p++) rejected"
cat << 'EOF' > "$TMP_DIR/neg16.cco"
fn main() -> int {
    let p: *char = null
    p++
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg16.cco" -o "$TMP_DIR/neg16_bin" > "$TMP_DIR/neg16.log" 2>&1; then
    fail_test "compilation succeeded unexpectedly"
else
    if grep -q "pointer arithmetic is not supported" "$TMP_DIR/neg16.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg16.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
echo "=============================================================="
echo "Results: $passed_tests passed, $failed_tests failed (Total: $total_tests)"
echo "=============================================================="

if [ "$failed_tests" -ne 0 ]; then
    exit 1
fi

exit 0
