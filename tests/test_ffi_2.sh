#!/usr/bin/env bash
# ==============================================================================
# tests/test_ffi_2.sh
# Cco FFI-2: Custom C Library Linking (-l and -L) Test Suite
# Positive & Negative Verification, Parity, Ordering, and Memory Safety Tests
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_ffi2_XXXXXX)

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
echo "Cco FFI-2 Custom C Library Linking (-l, -L) Test Suite"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Setup Test Libraries
# ------------------------------------------------------------------------------
mkdir -p "$TMP_DIR/lib1" "$TMP_DIR/lib2" "$TMP_DIR/deps" "$TMP_DIR/dir with space"

# 1. libmathcustom.a in lib1
cat << 'EOF' > "$TMP_DIR/math_custom.c"
int math_mult(int a, int b) {
    return a * b;
}
EOF
gcc -c -O2 "$TMP_DIR/math_custom.c" -o "$TMP_DIR/math_custom.o"
ar rcs "$TMP_DIR/lib1/libmathcustom.a" "$TMP_DIR/math_custom.o"

# 2. libstringcustom.a in lib2
cat << 'EOF' > "$TMP_DIR/str_custom.c"
#include <string.h>
int str_len_c(const char *s) {
    return (int)strlen(s);
}
EOF
gcc -c -O2 "$TMP_DIR/str_custom.c" -o "$TMP_DIR/str_custom.o"
ar rcs "$TMP_DIR/lib2/libstringcustom.a" "$TMP_DIR/str_custom.o"

# 3. Interdependent libraries in deps: depa depends on depb
cat << 'EOF' > "$TMP_DIR/dep_b.c"
int b_calc(int x) {
    return x * 3;
}
EOF
gcc -c -O2 "$TMP_DIR/dep_b.c" -o "$TMP_DIR/dep_b.o"
ar rcs "$TMP_DIR/deps/libdepb.a" "$TMP_DIR/dep_b.o"

cat << 'EOF' > "$TMP_DIR/dep_a.c"
extern int b_calc(int x);
int a_calc(int x) {
    return b_calc(x) + 5;
}
EOF
gcc -c -O2 "$TMP_DIR/dep_a.c" -o "$TMP_DIR/dep_a.o"
ar rcs "$TMP_DIR/deps/libdepa.a" "$TMP_DIR/dep_a.o"

# 4. Library in directory with space
cat << 'EOF' > "$TMP_DIR/dir with space/space_custom.c"
int space_calc(int x) {
    return x + 100;
}
EOF
gcc -c -O2 "$TMP_DIR/dir with space/space_custom.c" -o "$TMP_DIR/dir with space/space_custom.o"
ar rcs "$TMP_DIR/dir with space/libspacelib.a" "$TMP_DIR/dir with space/space_custom.o"

# ------------------------------------------------------------------------------
# Positive Test 1: Existing FFI-1 unflagged program compiles and runs
# ------------------------------------------------------------------------------
run_test "Positive 1: Existing FFI-1 program without flags (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos1.cco"
extern "C" {
    fn puts(s: string) -> int
}

fn main() -> int {
    puts("FFI-1 unflagged OK")
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos1.cco" -o "$TMP_DIR/pos1_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos1_c11")
"$CCO" "$TMP_DIR/pos1.cco" --use-native -o "$TMP_DIR/pos1_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos1_native")

if [ "$out_c11" = "FFI-1 unflagged OK" ] && [ "$out_nat" = "FFI-1 unflagged OK" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 2: Compact -L<dir> and -l<lib> with custom library
# ------------------------------------------------------------------------------
run_test "Positive 2: Compact -L<dir> -l<lib> syntax (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos2.cco"
extern "C" {
    fn math_mult(a: int, b: int) -> int
}

fn main() -> int {
    print(math_mult(6, 7))
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos2.cco" -L"$TMP_DIR/lib1" -lmathcustom -o "$TMP_DIR/pos2_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos2_c11")
"$CCO" "$TMP_DIR/pos2.cco" --use-native -L"$TMP_DIR/lib1" -lmathcustom -o "$TMP_DIR/pos2_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos2_native")

if [ "$out_c11" = "42" ] && [ "$out_nat" = "42" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 3: Separated -L <dir> and -l <lib> syntax
# ------------------------------------------------------------------------------
run_test "Positive 3: Separated -L <dir> -l <lib> syntax (C11 + native)"
"$CCO" "$TMP_DIR/pos2.cco" -L "$TMP_DIR/lib1" -l mathcustom -o "$TMP_DIR/pos3_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos3_c11")
"$CCO" "$TMP_DIR/pos2.cco" --use-native -L "$TMP_DIR/lib1" -l mathcustom -o "$TMP_DIR/pos3_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos3_native")

if [ "$out_c11" = "42" ] && [ "$out_nat" = "42" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 4: Custom string library through extern "C"
# ------------------------------------------------------------------------------
run_test "Positive 4: Custom library passing string argument (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos4.cco"
extern "C" {
    fn str_len_c(s: string) -> int
}

fn main() -> int {
    let msg: string = "Hello FFI-2!"
    print(str_len_c(msg))
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos4.cco" -L "$TMP_DIR/lib2" -l stringcustom -o "$TMP_DIR/pos4_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos4_c11")
"$CCO" "$TMP_DIR/pos4.cco" --use-native -L "$TMP_DIR/lib2" -l stringcustom -o "$TMP_DIR/pos4_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos4_native")

if [ "$out_c11" = "12" ] && [ "$out_nat" = "12" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 5: Multiple search directories (-L dir1 -L dir2)
# ------------------------------------------------------------------------------
run_test "Positive 5: Multiple search directories (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos5.cco"
extern "C" {
    fn math_mult(a: int, b: int) -> int
    fn str_len_c(s: string) -> int
}

fn main() -> int {
    let m: int = math_mult(3, 4)
    let s: int = str_len_c("Cco")
    print(m + s)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos5.cco" -L "$TMP_DIR/lib1" -L "$TMP_DIR/lib2" -l mathcustom -l stringcustom -o "$TMP_DIR/pos5_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos5_c11")
"$CCO" "$TMP_DIR/pos5.cco" --use-native -L "$TMP_DIR/lib1" -L "$TMP_DIR/lib2" -l mathcustom -l stringcustom -o "$TMP_DIR/pos5_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos5_native")

if [ "$out_c11" = "15" ] && [ "$out_nat" = "15" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 6: Static library dependency ordering (-ldepa -ldepb)
# ------------------------------------------------------------------------------
run_test "Positive 6: Dependency ordering preserved (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos6.cco"
extern "C" {
    fn a_calc(x: int) -> int
}

fn main() -> int {
    print(a_calc(10))
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos6.cco" -L "$TMP_DIR/deps" -l depa -l depb -o "$TMP_DIR/pos6_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos6_c11")
"$CCO" "$TMP_DIR/pos6.cco" --use-native -L "$TMP_DIR/deps" -l depa -l depb -o "$TMP_DIR/pos6_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos6_native")

if [ "$out_c11" = "35" ] && [ "$out_nat" = "35" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 7: Paths containing spaces in -L argument
# ------------------------------------------------------------------------------
run_test "Positive 7: Search path containing spaces (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos7.cco"
extern "C" {
    fn space_calc(x: int) -> int
}

fn main() -> int {
    print(space_calc(50))
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos7.cco" -L "$TMP_DIR/dir with space" -l spacelib -o "$TMP_DIR/pos7_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos7_c11")
"$CCO" "$TMP_DIR/pos7.cco" --use-native -L "$TMP_DIR/dir with space" -l spacelib -o "$TMP_DIR/pos7_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos7_native")

if [ "$out_c11" = "150" ] && [ "$out_nat" = "150" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 8: Execution with --run and library linking
# ------------------------------------------------------------------------------
run_test "Positive 8: Compilation with --run and library linking"
out_run=$("$CCO" "$TMP_DIR/pos2.cco" -L "$TMP_DIR/lib1" -l mathcustom -o "$TMP_DIR/pos8_run" --run 2>&1)
if [ "$out_run" = "42" ]; then
    pass_test
else
    fail_test "Expected output '42', got: '$out_run'"
fi

# ------------------------------------------------------------------------------
# Positive Test 9: Memory safety under Valgrind
# ------------------------------------------------------------------------------
run_test "Positive 9: Valgrind memory safety check on linked program"
valgrind --leak-check=full --error-exitcode=42 "$TMP_DIR/pos6_c11" > /dev/null 2>&1
vg_c11=$?
valgrind --leak-check=full --error-exitcode=42 "$TMP_DIR/pos6_native" > /dev/null 2>&1
vg_nat=$?

if [ "$vg_c11" -eq 0 ] && [ "$vg_nat" -eq 0 ]; then
    pass_test
else
    fail_test "Valgrind detected leak or memory error (c11=$vg_c11, nat=$vg_nat)"
fi

# ------------------------------------------------------------------------------
# Negative Test 1: Bare -L flag rejected
# ------------------------------------------------------------------------------
run_test "Negative 1: Bare -L rejected with clear diagnostic"
set +e
err_bare_L=$("$CCO" "$TMP_DIR/pos1.cco" -L 2>&1)
code_bare_L=$?
set -e

if [ "$code_bare_L" -ne 0 ] && echo "$err_bare_L" | grep -qi "Expected directory argument after '-L'"; then
    pass_test
else
    fail_test "Expected -L error (code=$code_bare_L, err='$err_bare_L')"
fi

# ------------------------------------------------------------------------------
# Negative Test 2: Bare -l flag rejected
# ------------------------------------------------------------------------------
run_test "Negative 2: Bare -l rejected with clear diagnostic"
set +e
err_bare_l=$("$CCO" "$TMP_DIR/pos1.cco" -l 2>&1)
code_bare_l=$?
set -e

if [ "$code_bare_l" -ne 0 ] && echo "$err_bare_l" | grep -qi "Expected library name after '-l'"; then
    pass_test
else
    fail_test "Expected -l error (code=$code_bare_l, err='$err_bare_l')"
fi

# ------------------------------------------------------------------------------
# Negative Test 3: -L followed immediately by another flag
# ------------------------------------------------------------------------------
run_test "Negative 3: -L followed by option rejected"
set +e
err_L_opt=$("$CCO" "$TMP_DIR/pos1.cco" -L -o "$TMP_DIR/bad" 2>&1)
code_L_opt=$?
set -e

if [ "$code_L_opt" -ne 0 ] && echo "$err_L_opt" | grep -qi "Expected directory argument after '-L'"; then
    pass_test
else
    fail_test "Expected -L option error (code=$code_L_opt, err='$err_L_opt')"
fi

# ------------------------------------------------------------------------------
# Negative Test 4: -l followed immediately by another flag
# ------------------------------------------------------------------------------
run_test "Negative 4: -l followed by option rejected"
set +e
err_l_opt=$("$CCO" "$TMP_DIR/pos1.cco" -l -o "$TMP_DIR/bad" 2>&1)
code_l_opt=$?
set -e

if [ "$code_l_opt" -ne 0 ] && echo "$err_l_opt" | grep -qi "Expected library name after '-l'"; then
    pass_test
else
    fail_test "Expected -l option error (code=$code_l_opt, err='$err_l_opt')"
fi

# ------------------------------------------------------------------------------
# Negative Test 5: Empty directory -L "" rejected
# ------------------------------------------------------------------------------
run_test "Negative 5: Empty directory -L \"\" rejected"
set +e
err_empty_L=$("$CCO" "$TMP_DIR/pos1.cco" -L "" 2>&1)
code_empty_L=$?
set -e

if [ "$code_empty_L" -ne 0 ] && echo "$err_empty_L" | grep -qi "cannot be empty"; then
    pass_test
else
    fail_test "Expected empty -L error (code=$code_empty_L, err='$err_empty_L')"
fi

# ------------------------------------------------------------------------------
# Negative Test 6: Empty library -l "" rejected
# ------------------------------------------------------------------------------
run_test "Negative 6: Empty library -l \"\" rejected"
set +e
err_empty_l=$("$CCO" "$TMP_DIR/pos1.cco" -l "" 2>&1)
code_empty_l=$?
set -e

if [ "$code_empty_l" -ne 0 ] && echo "$err_empty_l" | grep -qi "cannot be empty"; then
    pass_test
else
    fail_test "Expected empty -l error (code=$code_empty_l, err='$err_empty_l')"
fi

# ------------------------------------------------------------------------------
# Negative Test 7: Nonexistent library fails during linking
# ------------------------------------------------------------------------------
run_test "Negative 7: Missing library fails with linker error"
set +e
err_miss_lib=$("$CCO" "$TMP_DIR/pos1.cco" -l nonexistent_lib_xyz_9999 -o "$TMP_DIR/bad" 2>&1)
code_miss_lib=$?
set -e

if [ "$code_miss_lib" -ne 0 ] && echo "$err_miss_lib" | grep -qi "cannot find -lnonexistent_lib_xyz_9999"; then
    pass_test
else
    fail_test "Expected missing library linker failure (code=$code_miss_lib, err='$err_miss_lib')"
fi

# ------------------------------------------------------------------------------
# Negative Test 8: Unresolved external symbol fails during linking
# ------------------------------------------------------------------------------
run_test "Negative 8: Unresolved external symbol fails during linking"
cat << 'EOF' > "$TMP_DIR/neg_unresolved.cco"
extern "C" {
    fn missing_symbol_12345() -> int
}

fn main() -> int {
    missing_symbol_12345()
    return 0
}
EOF

set +e
err_unres=$("$CCO" "$TMP_DIR/neg_unresolved.cco" -o "$TMP_DIR/bad" 2>&1)
code_unres=$?
set -e

if [ "$code_unres" -ne 0 ] && echo "$err_unres" | grep -qi "undefined reference"; then
    pass_test
else
    fail_test "Expected undefined reference error (code=$code_unres, err='$err_unres')"
fi

# ------------------------------------------------------------------------------
# Negative Test 9: Incompatible internal-linker usage rejected clearly
# ------------------------------------------------------------------------------
run_test "Negative 9: Incompatible internal linker usage rejected"
set +e
err_internal=$("$CCO" "$TMP_DIR/pos1.cco" --use-internal-linker -lm -o "$TMP_DIR/bad" 2>&1)
code_internal=$?
set -e

if [ "$code_internal" -ne 0 ] && echo "$err_internal" | grep -qi "not supported by the internal freestanding linker"; then
    pass_test
else
    fail_test "Expected internal linker rejection (code=$code_internal, err='$err_internal')"
fi

# ------------------------------------------------------------------------------
# Negative Test 10: Unknown CLI options rejected
# ------------------------------------------------------------------------------
run_test "Negative 10: Unknown CLI option rejected"
set +e
err_unknown=$("$CCO" "$TMP_DIR/pos1.cco" --unknown-flag-1234 -o "$TMP_DIR/bad" 2>&1)
code_unknown=$?
set -e

if [ "$code_unknown" -ne 0 ] && echo "$err_unknown" | grep -qi "Unknown option"; then
    pass_test
else
    fail_test "Expected unknown option error (code=$code_unknown, err='$err_unknown')"
fi

# ------------------------------------------------------------------------------
# Negative Test 11: Reversed library dependency ordering produces linker failure
# ------------------------------------------------------------------------------
run_test "Negative 11: Reversed dependency order causes link failure"
set +e
err_rev_order=$("$CCO" "$TMP_DIR/pos6.cco" -L "$TMP_DIR/deps" -l depb -l depa -o "$TMP_DIR/bad" 2>&1)
code_rev_order=$?
set -e

if [ "$code_rev_order" -ne 0 ] && echo "$err_rev_order" | grep -qi "undefined reference"; then
    pass_test
else
    fail_test "Expected link failure on reversed static library order (code=$code_rev_order, err='$err_rev_order')"
fi

echo "=============================================================="
echo "Suite Summary: $passed_tests Passed, $failed_tests Failed (Total: $total_tests)"
echo "=============================================================="

if [ "$failed_tests" -gt 0 ]; then
    exit 1
fi
exit 0
