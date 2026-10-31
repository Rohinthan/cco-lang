#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco FFI-3B Explicit C String Conversion and Interoperability Test Suite
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR="$(mktemp -d /tmp/cco_test_ffi_3b_XXXXXX)"

trap 'rm -rf "$TMP_DIR"' EXIT

passed_tests=0
failed_tests=0
current_test=""

pass_test() {
    passed_tests=$((passed_tests + 1))
    printf "PASSED\n"
}

fail_test() {
    local reason="$1"
    failed_tests=$((failed_tests + 1))
    printf "FAILED (%s)\n" "$reason"
}

run_test() {
    local name="$1"
    current_test="$name"
    printf "[%2d] %s ... " $((passed_tests + failed_tests + 1)) "$name"
}

if [ ! -x "$CCO" ]; then
    echo "Compiler binary '$CCO' not found. Building with make..."
    (cd "$REPO_ROOT" && make cco)
fi

echo "=============================================================="
echo "Cco FFI-3B Explicit C String Conversion Test Suite"
echo "=============================================================="

# Build a small custom C library (both static archive and shared lib) for test cases
cat << 'EOF' > "$TMP_DIR/libtest_str.c"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char g_static_buf[] = "STATIC_C_BUFFER_12345";
static char g_mutable_buf[64] = "ORIGINAL_MUTABLE_VALUE";

char *get_static_c_string(void) {
    return g_static_buf;
}

char *allocate_dynamic_c_string(char *prefix) {
    if (!prefix) prefix = "";
    size_t len = strlen(prefix) + 32;
    char *res = (char *)malloc(len);
    if (!res) return NULL;
    snprintf(res, len, "%s_DYNAMIC_ALLOCATED", prefix);
    return res;
}

void free_dynamic_c_string(char *p) {
    if (p) free(p);
}

char *get_mutable_buffer(void) {
    return g_mutable_buf;
}

void mutate_buffer(char *new_val) {
    if (!new_val) return;
    strncpy(g_mutable_buf, new_val, sizeof(g_mutable_buf) - 1);
    g_mutable_buf[sizeof(g_mutable_buf) - 1] = '\0';
}

int c_streq(char *a, char *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

int c_strlen(char *s) {
    return s ? (int)strlen(s) : 0;
}

void c_puts(char *s) {
    if (s) {
        puts(s);
    }
}
EOF

gcc -c -O2 "$TMP_DIR/libtest_str.c" -o "$TMP_DIR/libtest_str.o"
ar rcs "$TMP_DIR/libteststr.a" "$TMP_DIR/libtest_str.o"

# ------------------------------------------------------------------------------
# Positive Test 1: str_to_cstr passing string to external c_puts(*char)
# ------------------------------------------------------------------------------
run_test "Positive 1: str_to_cstr passing string to c_puts(*char) (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos1.cco"
extern "C" {
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let msg: string = "HELLO_C_STRING"
    let p: *char = str_to_cstr(msg)
    c_puts(p)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos1.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos1_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos1_c11")
"$CCO" "$TMP_DIR/pos1.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos1_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos1_nat")

if [ "$out_c11" = "HELLO_C_STRING" ] && [ "$out_nat" = "HELLO_C_STRING" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 2: string_to_cstr alias passing string to c_puts(*char)
# ------------------------------------------------------------------------------
run_test "Positive 2: string_to_cstr alias passing string to c_puts(*char)"
cat << 'EOF' > "$TMP_DIR/pos2.cco"
extern "C" {
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let msg: string = "ALIAS_C_STRING"
    let p: *char = string_to_cstr(msg)
    c_puts(p)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos2.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos2_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos2_c11")
"$CCO" "$TMP_DIR/pos2.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos2_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos2_nat")

if [ "$out_c11" = "ALIAS_C_STRING" ] && [ "$out_nat" = "ALIAS_C_STRING" ]; then
    pass_test
else
    fail_test "output mismatch"
fi

# ------------------------------------------------------------------------------
# Positive Test 3: str_from_cstr reading foreign C string into string
# ------------------------------------------------------------------------------
run_test "Positive 3: str_from_cstr reading foreign C string into string"
cat << 'EOF' > "$TMP_DIR/pos3.cco"
extern "C" {
    fn get_static_c_string() -> *char
    fn c_strlen(s: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let c_p: *char = get_static_c_string()
    if c_p != null {
        let s: string = str_from_cstr(c_p)
        let n: int = c_strlen(str_to_cstr(s))
        if n > 0 {
            let ok: string = "STATIC_READ_OK"
            c_puts(str_to_cstr(ok))
            return 0
        }
    }
    let fail: string = "STATIC_READ_FAIL"
    c_puts(str_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos3.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos3_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos3_c11")
"$CCO" "$TMP_DIR/pos3.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos3_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos3_nat")

if [ "$out_c11" = "STATIC_READ_OK" ] && [ "$out_nat" = "STATIC_READ_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 4: string_from_cstr alias reading foreign C string
# ------------------------------------------------------------------------------
run_test "Positive 4: string_from_cstr alias reading foreign C string"
cat << 'EOF' > "$TMP_DIR/pos4.cco"
extern "C" {
    fn get_static_c_string() -> *char
    fn c_strlen(s: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let c_p: *char = get_static_c_string()
    if c_p != null {
        let s: string = string_from_cstr(c_p)
        let n: int = c_strlen(string_to_cstr(s))
        if n > 0 {
            let ok: string = "ALIAS_STATIC_READ_OK"
            c_puts(string_to_cstr(ok))
            return 0
        }
    }
    let fail: string = "ALIAS_STATIC_READ_FAIL"
    c_puts(string_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos4.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos4_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos4_c11")
"$CCO" "$TMP_DIR/pos4.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos4_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos4_nat")

if [ "$out_c11" = "ALIAS_STATIC_READ_OK" ] && [ "$out_nat" = "ALIAS_STATIC_READ_OK" ]; then
    pass_test
else
    fail_test "output mismatch"
fi

# ------------------------------------------------------------------------------
# Positive Test 5: Round-trip identity check str_from_cstr(str_to_cstr(s)) == s
# ------------------------------------------------------------------------------
run_test "Positive 5: Round-trip identity conversion (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos5.cco"
extern "C" {
    fn c_streq(a: *char, b: *char) -> int
    fn c_strlen(s: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let s1: string = "roundtrip testing 12345"
    let p1: *char = str_to_cstr(s1)
    let s2: string = str_from_cstr(p1)

    let empty: string = ""
    let p_empty: *char = str_to_cstr(empty)
    let s_empty: string = str_from_cstr(p_empty)

    let match1: int = c_streq(p1, str_to_cstr(s2))
    let match2: int = c_streq(p_empty, str_to_cstr(s_empty))
    let len2: int = c_strlen(str_to_cstr(s2))
    let len_empty: int = c_strlen(str_to_cstr(s_empty))

    if match1 == 1 && match2 == 1 && len2 == 23 && len_empty == 0 {
        let ok: string = "ROUNDTRIP_OK"
        c_puts(str_to_cstr(ok))
        return 0
    }
    let fail: string = "ROUNDTRIP_FAIL"
    c_puts(str_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos5.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos5_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos5_c11")
"$CCO" "$TMP_DIR/pos5.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos5_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos5_nat")

if [ "$out_c11" = "ROUNDTRIP_OK" ] && [ "$out_nat" = "ROUNDTRIP_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 6: Null conversion safety with literal null
# ------------------------------------------------------------------------------
run_test "Positive 6: str_from_cstr(null) produces owned empty string"
cat << 'EOF' > "$TMP_DIR/pos6.cco"
extern "C" {
    fn c_streq(a: *char, b: *char) -> int
    fn c_strlen(s: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let s: string = str_from_cstr(null)
    let empty_cstr: *char = str_to_cstr("")
    let is_eq: int = c_streq(str_to_cstr(s), empty_cstr)
    let n: int = c_strlen(str_to_cstr(s))

    if is_eq == 1 && n == 0 {
        let ok: string = "NULL_LITERAL_OK"
        c_puts(str_to_cstr(ok))
        return 0
    }
    let fail: string = "NULL_LITERAL_FAIL"
    c_puts(str_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos6.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos6_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos6_c11")
"$CCO" "$TMP_DIR/pos6.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos6_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos6_nat")

if [ "$out_c11" = "NULL_LITERAL_OK" ] && [ "$out_nat" = "NULL_LITERAL_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 7: Null conversion safety with typed *char variable
# ------------------------------------------------------------------------------
run_test "Positive 7: str_from_cstr(p) where p is null variable"
cat << 'EOF' > "$TMP_DIR/pos7.cco"
extern "C" {
    fn c_streq(a: *char, b: *char) -> int
    fn c_strlen(s: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let p: *char = null
    let s: string = str_from_cstr(p)
    let empty_cstr: *char = str_to_cstr("")
    let is_eq: int = c_streq(str_to_cstr(s), empty_cstr)
    let n: int = c_strlen(str_to_cstr(s))

    if is_eq == 1 && n == 0 {
        let ok: string = "TYPED_NULL_OK"
        c_puts(str_to_cstr(ok))
        return 0
    }
    let fail: string = "TYPED_NULL_FAIL"
    c_puts(str_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos7.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos7_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos7_c11")
"$CCO" "$TMP_DIR/pos7.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos7_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos7_nat")

if [ "$out_c11" = "TYPED_NULL_OK" ] && [ "$out_nat" = "TYPED_NULL_OK" ]; then
    pass_test
else
    fail_test "output mismatch"
fi

# ------------------------------------------------------------------------------
# Positive Test 8: Static foreign C string lifetime
# ------------------------------------------------------------------------------
run_test "Positive 8: Static foreign C string lifetime & independence"
cat << 'EOF' > "$TMP_DIR/pos8.cco"
extern "C" {
    fn get_static_c_string() -> *char
    fn c_streq(a: *char, b: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let p: *char = get_static_c_string()
    let s: string = str_from_cstr(p)
    let expected: *char = str_to_cstr("STATIC_C_BUFFER_12345")
    if c_streq(str_to_cstr(s), expected) == 1 {
        let ok: string = "STATIC_LIFETIME_OK"
        c_puts(str_to_cstr(ok))
        return 0
    }
    let fail: string = "STATIC_LIFETIME_FAIL"
    c_puts(str_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos8.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos8_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos8_c11")
"$CCO" "$TMP_DIR/pos8.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos8_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos8_nat")

if [ "$out_c11" = "STATIC_LIFETIME_OK" ] && [ "$out_nat" = "STATIC_LIFETIME_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 9: Dynamically allocated foreign C string lifetime
# ------------------------------------------------------------------------------
run_test "Positive 9: Dynamically allocated foreign C string lifecycle"
cat << 'EOF' > "$TMP_DIR/pos9.cco"
extern "C" {
    fn allocate_dynamic_c_string(prefix: *char) -> *char
    fn free_dynamic_c_string(p: *char) -> void
    fn c_streq(a: *char, b: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let pref: *char = str_to_cstr("TEST")
    let c_buf: *char = allocate_dynamic_c_string(pref)
    if c_buf == null {
        return 1
    }

    # Copy into Cco owned string
    let cco_str: string = str_from_cstr(c_buf)

    # C library frees its original buffer
    free_dynamic_c_string(c_buf)

    # Cco string must still be fully valid and readable!
    let expected: *char = str_to_cstr("TEST_DYNAMIC_ALLOCATED")
    if c_streq(str_to_cstr(cco_str), expected) == 1 {
        let ok: string = "DYNAMIC_LIFETIME_OK"
        c_puts(str_to_cstr(ok))
        return 0
    }
    let fail: string = "DYNAMIC_LIFETIME_FAIL"
    c_puts(str_to_cstr(fail))
    return 2
}
EOF

"$CCO" "$TMP_DIR/pos9.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos9_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos9_c11")
"$CCO" "$TMP_DIR/pos9.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos9_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos9_nat")

if [ "$out_c11" = "DYNAMIC_LIFETIME_OK" ] && [ "$out_nat" = "DYNAMIC_LIFETIME_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 10: Mutation hazard isolation
# ------------------------------------------------------------------------------
run_test "Positive 10: Foreign C mutation isolation check"
cat << 'EOF' > "$TMP_DIR/pos10.cco"
extern "C" {
    fn get_mutable_buffer() -> *char
    fn mutate_buffer(new_val: *char) -> void
    fn c_streq(a: *char, b: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let p: *char = get_mutable_buffer()
    let s_snapshot: string = str_from_cstr(p)

    # Mutate C buffer externally
    let new_p: *char = str_to_cstr("MODIFIED_BY_C_EXTERNAL")
    mutate_buffer(new_p)

    # s_snapshot must NOT be affected!
    let expected: *char = str_to_cstr("ORIGINAL_MUTABLE_VALUE")
    if c_streq(str_to_cstr(s_snapshot), expected) == 1 {
        let ok: string = "MUTATION_ISOLATED_OK"
        c_puts(str_to_cstr(ok))
        return 0
    }
    let fail: string = "MUTATION_ISOLATED_FAIL"
    c_puts(str_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos10.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos10_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos10_c11")
"$CCO" "$TMP_DIR/pos10.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos10_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos10_nat")

if [ "$out_c11" = "MUTATION_ISOLATED_OK" ] && [ "$out_nat" = "MUTATION_ISOLATED_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 11: Reassignment and scope cleanup of converted string
# ------------------------------------------------------------------------------
run_test "Positive 11: String variable reassignment with str_from_cstr"
cat << 'EOF' > "$TMP_DIR/pos11.cco"
extern "C" {
    fn c_streq(a: *char, b: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let s: string = "initial"
    let p1: *char = str_to_cstr("reassigned_first")
    s = str_from_cstr(p1)
    let p2: *char = str_to_cstr("reassigned_second")
    s = str_from_cstr(p2)

    let expected: *char = str_to_cstr("reassigned_second")
    if c_streq(str_to_cstr(s), expected) == 1 {
        let ok: string = "REASSIGN_OK"
        c_puts(str_to_cstr(ok))
        return 0
    }
    let fail: string = "REASSIGN_FAIL"
    c_puts(str_to_cstr(fail))
    return 1
}
EOF

"$CCO" "$TMP_DIR/pos11.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos11_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos11_c11")
"$CCO" "$TMP_DIR/pos11.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos11_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos11_nat")

if [ "$out_c11" = "REASSIGN_OK" ] && [ "$out_nat" = "REASSIGN_OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 12: High-iteration allocation loop under Valgrind
# ------------------------------------------------------------------------------
run_test "Positive 12: 5,000-iteration stress test under Valgrind (0 leaks)"
cat << 'EOF' > "$TMP_DIR/pos12.cco"
extern "C" {
    fn c_strlen(s: *char) -> int
    fn c_puts(s: *char) -> void
}

fn main() -> int {
    let i: int = 0
    while i < 5000 {
        let msg: string = "iteration_data"
        let p: *char = str_to_cstr(msg)
        let copy: string = str_from_cstr(p)
        if c_strlen(str_to_cstr(copy)) != 14 {
            return 1
        }
        i = i + 1
    }
    let ok: string = "LOOP_STRESS_OK"
    c_puts(str_to_cstr(ok))
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos12.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos12_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
"$CCO" "$TMP_DIR/pos12.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/pos12_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }

valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 "$TMP_DIR/pos12_c11" > /dev/null 2>&1 || {
    fail_test "c11 loop failed valgrind"
    exit 1
}

valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 "$TMP_DIR/pos12_nat" > /dev/null 2>&1 || {
    fail_test "native loop failed valgrind"
    exit 1
}

pass_test

# ------------------------------------------------------------------------------
# Positive Test 13: Valgrind memory cleanliness on all positive test binaries
# ------------------------------------------------------------------------------
run_test "Positive 13: Full Valgrind suite across positive tests 1-11"
for b in "$TMP_DIR"/pos{1,2,3,4,5,6,7,8,9,10,11}_c11 "$TMP_DIR"/pos{1,2,3,4,5,6,7,8,9,10,11}_nat; do
    if [ -x "$b" ]; then
        valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 "$b" > /dev/null 2>&1 || {
            fail_test "valgrind reported leaks on $(basename "$b")"
            exit 1
        }
    fi
done
pass_test

# ------------------------------------------------------------------------------
# Positive Test 14: IR verification across test programs
# ------------------------------------------------------------------------------
run_test "Positive 14: IR verification on string conversion programs"
"$CCO" "$TMP_DIR/pos1.cco" --emit-ir > /dev/null 2>&1 || { fail_test "emit-ir failed on pos1"; exit 1; }
"$CCO" "$TMP_DIR/pos5.cco" --emit-ir > /dev/null 2>&1 || { fail_test "emit-ir failed on pos5"; exit 1; }
"$CCO" "$TMP_DIR/pos6.cco" --emit-ir > /dev/null 2>&1 || { fail_test "emit-ir failed on pos6"; exit 1; }
pass_test

# ==============================================================================
# Negative Test Cases
# ==============================================================================

# ------------------------------------------------------------------------------
# Negative Test 1: Arbitrary *void variable passed to str_from_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 1: Arbitrary *void variable to str_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg1.cco"
extern "C" {
    fn fopen(path: *char, mode: *char) -> *void
}

fn main() -> int {
    let mode: *char = str_to_cstr("r")
    let path: *char = str_to_cstr("/dev/null")
    let h: *void = fopen(path, mode)
    # MUST BE REJECTED: opaque handle cannot be passed to str_from_cstr
    let s: string = str_from_cstr(h)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg1.cco" -o "$TMP_DIR/neg1_bin" > "$TMP_DIR/neg1.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg1.log" && grep -q "*void" "$TMP_DIR/neg1.log"; then
        pass_test
    else
        fail_test "unexpected error message: $(cat "$TMP_DIR/neg1.log")"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 2: Arbitrary *void variable to string_from_cstr alias rejected
# ------------------------------------------------------------------------------
run_test "Negative 2: Arbitrary *void variable to string_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg2.cco"
fn main() -> int {
    let h: *void = null
    let s: string = string_from_cstr(h)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg2.cco" -o "$TMP_DIR/neg2_bin" > "$TMP_DIR/neg2.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg2.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 3: Passing int to str_to_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 3: Passing int to str_to_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg3.cco"
fn main() -> int {
    let p: *char = str_to_cstr(42)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg3.cco" -o "$TMP_DIR/neg3_bin" > "$TMP_DIR/neg3.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg3.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 4: Passing *char to str_to_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 4: Passing *char to str_to_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg4.cco"
fn main() -> int {
    let p: *char = null
    let p2: *char = str_to_cstr(p)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg4.cco" -o "$TMP_DIR/neg4_bin" > "$TMP_DIR/neg4.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg4.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 5: Passing null to str_to_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 5: Passing null literal to str_to_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg5.cco"
fn main() -> int {
    let p: *char = str_to_cstr(null)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg5.cco" -o "$TMP_DIR/neg5_bin" > "$TMP_DIR/neg5.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg5.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 6: Passing string to str_from_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 6: Passing string to str_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg6.cco"
fn main() -> int {
    let s: string = "hello"
    let copy: string = str_from_cstr(s)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg6.cco" -o "$TMP_DIR/neg6_bin" > "$TMP_DIR/neg6.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg6.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 7: Passing int to str_from_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 7: Passing int to str_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg7.cco"
fn main() -> int {
    let s: string = str_from_cstr(100)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg7.cco" -o "$TMP_DIR/neg7_bin" > "$TMP_DIR/neg7.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg7.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 8: Passing *int to str_from_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 8: Passing *int pointer to str_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg8.cco"
fn main() -> int {
    let pi: *int = null
    let s: string = str_from_cstr(pi)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg8.cco" -o "$TMP_DIR/neg8_bin" > "$TMP_DIR/neg8.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg8.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 9: Passing **char pointer to str_from_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 9: Passing **char nested pointer to str_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg9.cco"
fn main() -> int {
    let pp: **char = null
    let s: string = str_from_cstr(pp)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg9.cco" -o "$TMP_DIR/neg9_bin" > "$TMP_DIR/neg9.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg9.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 10: Zero arguments to str_to_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 10: Zero arguments to str_to_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg10.cco"
fn main() -> int {
    let p: *char = str_to_cstr()
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg10.cco" -o "$TMP_DIR/neg10_bin" > "$TMP_DIR/neg10.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "wrong argument count" "$TMP_DIR/neg10.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 11: Multiple arguments to str_to_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 11: Multiple arguments to str_to_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg11.cco"
fn main() -> int {
    let p: *char = str_to_cstr("a", "b")
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg11.cco" -o "$TMP_DIR/neg11_bin" > "$TMP_DIR/neg11.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "wrong argument count" "$TMP_DIR/neg11.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 12: Zero arguments to str_from_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 12: Zero arguments to str_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg12.cco"
fn main() -> int {
    let s: string = str_from_cstr()
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg12.cco" -o "$TMP_DIR/neg12_bin" > "$TMP_DIR/neg12.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "wrong argument count" "$TMP_DIR/neg12.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 13: Multiple arguments to str_from_cstr rejected
# ------------------------------------------------------------------------------
run_test "Negative 13: Multiple arguments to str_from_cstr rejected"
cat << 'EOF' > "$TMP_DIR/neg13.cco"
fn main() -> int {
    let p: *char = null
    let s: string = str_from_cstr(p, p)
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg13.cco" -o "$TMP_DIR/neg13_bin" > "$TMP_DIR/neg13.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "wrong argument count" "$TMP_DIR/neg13.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 14: Implicit assignment from string to *char rejected
# ------------------------------------------------------------------------------
run_test "Negative 14: Implicit conversion from string to *char rejected"
cat << 'EOF' > "$TMP_DIR/neg14.cco"
fn main() -> int {
    let s: string = "hello"
    let p: *char = s
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg14.cco" -o "$TMP_DIR/neg14_bin" > "$TMP_DIR/neg14.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg14.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 15: Implicit assignment from *char to string rejected
# ------------------------------------------------------------------------------
run_test "Negative 15: Implicit conversion from *char to string rejected"
cat << 'EOF' > "$TMP_DIR/neg15.cco"
fn main() -> int {
    let p: *char = null
    let s: string = p
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg15.cco" -o "$TMP_DIR/neg15_bin" > "$TMP_DIR/neg15.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg15.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ------------------------------------------------------------------------------
# Negative Test 16: Assigning null literal directly to string rejected
# ------------------------------------------------------------------------------
run_test "Negative 16: Assigning null literal to string variable rejected"
cat << 'EOF' > "$TMP_DIR/neg16.cco"
fn main() -> int {
    let s: string = null
    return 0
}
EOF

if "$CCO" "$TMP_DIR/neg16.cco" -o "$TMP_DIR/neg16_bin" > "$TMP_DIR/neg16.log" 2>&1; then
    fail_test "compiled unexpectedly"
else
    if grep -q "type mismatch" "$TMP_DIR/neg16.log"; then
        pass_test
    else
        fail_test "unexpected error message"
    fi
fi

# ==============================================================================
# Adversarial Fault-Injection Tests: Allocation Failure Handling (AC-5 / AC-8)
# ==============================================================================

# Compile fault-injection shims for strdup and malloc
cat << 'EOF' > "$TMP_DIR/fault_strdup.c"
#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>

static char *(*real_strdup)(const char *) = NULL;

char *strdup(const char *s) {
    if (!real_strdup) {
        real_strdup = (char *(*)(const char *))dlsym(RTLD_NEXT, "strdup");
    }
    const char *fail_mode = getenv("FAULT_STRDUP_FAIL");
    if (fail_mode) {
        if (strcmp(fail_mode, "ALL") == 0) {
            return NULL;
        }
        if (strcmp(fail_mode, "NULL_PATH") == 0 && s != NULL && s[0] == '\0') {
            return NULL;
        }
        if (strcmp(fail_mode, "NON_NULL") == 0 && s != NULL && s[0] != '\0') {
            return NULL;
        }
    }
    return real_strdup(s);
}
EOF
gcc -shared -fPIC -o "$TMP_DIR/libfault_strdup.so" "$TMP_DIR/fault_strdup.c" -ldl

cat << 'EOF' > "$TMP_DIR/fault_malloc.c"
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *(*real_malloc)(size_t) = NULL;

void *malloc(size_t size) {
    if (!real_malloc) {
        real_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    }
    const char *fail = getenv("FAULT_MALLOC_FAIL");
    if (fail && strcmp(fail, "1") == 0) {
        return NULL;
    }
    return real_malloc(size);
}
EOF
gcc -shared -fPIC -o "$TMP_DIR/libfault_malloc.so" "$TMP_DIR/fault_malloc.c" -ldl

# ------------------------------------------------------------------------------
# Adversarial Test 1: Native strdup() NULL failure on null-input path exits with code 1
# ------------------------------------------------------------------------------
run_test "Adversarial 1: Native strdup() NULL failure on null-input path exits 1"
cat << 'EOF' > "$TMP_DIR/adv1.cco"
extern "C" {
    fn c_puts(s: *char) -> void
}
fn main() -> int {
    let s: string = str_from_cstr(null)
    c_puts(str_to_cstr(s))
    return 0
}
EOF

"$CCO" "$TMP_DIR/adv1.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/adv1_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
set +e
FAULT_STRDUP_FAIL=NULL_PATH LD_PRELOAD="$TMP_DIR/libfault_strdup.so" "$TMP_DIR/adv1_nat" > "$TMP_DIR/adv1.out" 2> "$TMP_DIR/adv1.err"
adv1_ec=$?
set -e
if [ "$adv1_ec" -eq 1 ]; then
    pass_test
else
    fail_test "expected exit code 1, got $adv1_ec"
fi

# ------------------------------------------------------------------------------
# Adversarial Test 2: Native strdup() NULL failure on non-null path exits with code 1
# ------------------------------------------------------------------------------
run_test "Adversarial 2: Native strdup() NULL failure on non-null path exits 1"
cat << 'EOF' > "$TMP_DIR/adv2.cco"
extern "C" {
    fn get_static_c_string() -> *char
    fn c_puts(s: *char) -> void
}
fn main() -> int {
    let p: *char = get_static_c_string()
    let s: string = str_from_cstr(p)
    c_puts(str_to_cstr(s))
    return 0
}
EOF

"$CCO" "$TMP_DIR/adv2.cco" --use-native -L "$TMP_DIR" -l teststr -o "$TMP_DIR/adv2_nat" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
set +e
FAULT_STRDUP_FAIL=NON_NULL LD_PRELOAD="$TMP_DIR/libfault_strdup.so" "$TMP_DIR/adv2_nat" > "$TMP_DIR/adv2.out" 2> "$TMP_DIR/adv2.err"
adv2_ec=$?
set -e
if [ "$adv2_ec" -eq 1 ]; then
    pass_test
else
    fail_test "expected exit code 1, got $adv2_ec"
fi

# ------------------------------------------------------------------------------
# Adversarial Test 3: C11 malloc() failure on null-input path exits with code 1
# ------------------------------------------------------------------------------
run_test "Adversarial 3: C11 malloc() failure on null-input path exits 1"
cat << 'EOF' > "$TMP_DIR/adv3.cco"
extern "C" {
    fn c_puts(s: *char) -> void
}
fn main() -> int {
    let s: string = str_from_cstr(null)
    c_puts(str_to_cstr(s))
    return 0
}
EOF

"$CCO" "$TMP_DIR/adv3.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/adv3_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
set +e
FAULT_MALLOC_FAIL=1 LD_PRELOAD="$TMP_DIR/libfault_malloc.so" "$TMP_DIR/adv3_c11" > "$TMP_DIR/adv3.out" 2> "$TMP_DIR/adv3.err"
adv3_ec=$?
set -e
if [ "$adv3_ec" -eq 1 ]; then
    pass_test
else
    fail_test "expected exit code 1, got $adv3_ec"
fi

# ------------------------------------------------------------------------------
# Adversarial Test 4: C11 malloc() failure on non-null path exits with code 1
# ------------------------------------------------------------------------------
run_test "Adversarial 4: C11 malloc() failure on non-null path exits 1"
cat << 'EOF' > "$TMP_DIR/adv4.cco"
extern "C" {
    fn get_static_c_string() -> *char
    fn c_puts(s: *char) -> void
}
fn main() -> int {
    let p: *char = get_static_c_string()
    let s: string = str_from_cstr(p)
    c_puts(str_to_cstr(s))
    return 0
}
EOF

"$CCO" "$TMP_DIR/adv4.cco" -L "$TMP_DIR" -l teststr -o "$TMP_DIR/adv4_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
set +e
FAULT_MALLOC_FAIL=1 LD_PRELOAD="$TMP_DIR/libfault_malloc.so" "$TMP_DIR/adv4_c11" > "$TMP_DIR/adv4.out" 2> "$TMP_DIR/adv4.err"
adv4_ec=$?
set -e
if [ "$adv4_ec" -eq 1 ]; then
    pass_test
else
    fail_test "expected exit code 1, got $adv4_ec"
fi

# ------------------------------------------------------------------------------
# Adversarial Test 5: C11 stderr diagnostic match
# ------------------------------------------------------------------------------
run_test "Adversarial 5: C11 stderr diagnostic match"
if grep -q "Fatal Error: Out of memory in str_from_cstr()" "$TMP_DIR/adv3.err" && \
   grep -q "Fatal Error: Out of memory in str_from_cstr()" "$TMP_DIR/adv4.err"; then
    pass_test
else
    fail_test "diagnostic message missing in stderr"
fi

# ------------------------------------------------------------------------------
# Adversarial Test 6: Backend parity on normal run (both backends exit code 0)
# ------------------------------------------------------------------------------
run_test "Adversarial 6: Backend parity on normal run (both backends exit 0)"
set +e
"$TMP_DIR/adv2_nat" > "$TMP_DIR/adv2_nat_norm.out" 2>&1
ec_nat=$?
"$TMP_DIR/adv4_c11" > "$TMP_DIR/adv4_c11_norm.out" 2>&1
ec_c11=$?
set -e
if [ "$ec_nat" -eq 0 ] && [ "$ec_c11" -eq 0 ]; then
    pass_test
else
    fail_test "expected exit code 0, got nat=$ec_nat c11=$ec_c11"
fi

# ------------------------------------------------------------------------------
# Adversarial Test 7: Backend parity under fault injection (both backends exit code 1)
# ------------------------------------------------------------------------------
run_test "Adversarial 7: Backend parity under fault injection (both backends exit 1)"
if [ "$adv1_ec" -eq 1 ] && [ "$adv2_ec" -eq 1 ] && [ "$adv3_ec" -eq 1 ] && [ "$adv4_ec" -eq 1 ]; then
    pass_test
else
    fail_test "inconsistent exit codes: nat_null=$adv1_ec nat_nonnull=$adv2_ec c11_null=$adv3_ec c11_nonnull=$adv4_ec"
fi

# ------------------------------------------------------------------------------
# Adversarial Test 8: Valgrind memory leak check on successful conversion paths
# ------------------------------------------------------------------------------
run_test "Adversarial 8: Valgrind memory leak check on conversions (0 leaks)"
valgrind --leak-check=full --error-exitcode=99 "$TMP_DIR/adv2_nat" > /dev/null 2>&1
valg_nat=$?
valgrind --leak-check=full --error-exitcode=99 "$TMP_DIR/adv4_c11" > /dev/null 2>&1
valg_c11=$?
if [ "$valg_nat" -eq 0 ] && [ "$valg_c11" -eq 0 ]; then
    pass_test
else
    fail_test "valgrind reported leaks or errors: nat=$valg_nat c11=$valg_c11"
fi

# ==============================================================================
echo "=============================================================="
printf "FFI-3B Test Results: %d passed, %d failed (Total: %d)\n" \
    "$passed_tests" "$failed_tests" "$((passed_tests + failed_tests))"
echo "=============================================================="

if [ "$failed_tests" -ne 0 ]; then
    exit 1
fi
exit 0
