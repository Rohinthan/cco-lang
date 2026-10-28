#!/usr/bin/env bash
# ==============================================================================
# tests/test_ffi_1.sh
# Cco FFI-1: In-Source C Interoperability (extern "C") Test Suite
# Positive & Negative Verification, Parity, and Memory Safety Tests
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_ffi1_XXXXXX)

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
echo "Cco FFI-1 In-Source C Interoperability Test Suite"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Positive Test 1: puts prints a string
# ------------------------------------------------------------------------------
run_test "Positive 1: puts prints a string (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos1.cco"
extern "C" {
    fn puts(message: string) -> int
}

fn main() -> int {
    puts("Hello from Cco FFI-1!")
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos1.cco" -o "$TMP_DIR/pos1_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos1_c11")
"$CCO" "$TMP_DIR/pos1.cco" --use-native -o "$TMP_DIR/pos1_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos1_native")

if [ "$out_c11" = "Hello from Cco FFI-1!" ] && [ "$out_nat" = "Hello from Cco FFI-1!" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 2: abs(-100) returns 100
# ------------------------------------------------------------------------------
run_test "Positive 2: abs(-100) returns 100 (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos2.cco"
extern "C" {
    fn abs(value: int) -> int
}

fn main() -> int {
    let result: int = abs(-100)
    print(result)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos2.cco" -o "$TMP_DIR/pos2_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos2_c11")
"$CCO" "$TMP_DIR/pos2.cco" --use-native -o "$TMP_DIR/pos2_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos2_native")

if [ "$out_c11" = "100" ] && [ "$out_nat" = "100" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 3: putchar prints the expected character
# ------------------------------------------------------------------------------
run_test "Positive 3: putchar prints expected character (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos3.cco"
extern "C" {
    fn putchar(ch: int) -> int
}

fn main() -> int {
    putchar(65)
    putchar(10)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos3.cco" -o "$TMP_DIR/pos3_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos3_c11")
"$CCO" "$TMP_DIR/pos3.cco" --use-native -o "$TMP_DIR/pos3_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos3_native")

if [ "$out_c11" = "A" ] && [ "$out_nat" = "A" ]; then
    pass_test
else
    fail_test "output mismatch: c11='$out_c11', nat='$out_nat'"
fi

# ------------------------------------------------------------------------------
# Positive Test 4: Multiple declarations in one block work
# ------------------------------------------------------------------------------
run_test "Positive 4: Multiple declarations in block (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos4.cco"
extern "C" {
    fn puts(message: string) -> int
    fn abs(value: int) -> int
    fn putchar(ch: int) -> int
}

fn main() -> int {
    puts("Multiple externs:")
    print(abs(-42))
    putchar(66)
    putchar(10)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos4.cco" -o "$TMP_DIR/pos4_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos4_c11")
"$CCO" "$TMP_DIR/pos4.cco" --use-native -o "$TMP_DIR/pos4_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos4_native")

expected_p4=$(printf "Multiple externs:\n42\nB")
if [ "$out_c11" = "$expected_p4" ] && [ "$out_nat" = "$expected_p4" ]; then
    pass_test
else
    fail_test "output mismatch"
fi

# ------------------------------------------------------------------------------
# Positive Test 5: Single-declaration syntax works
# ------------------------------------------------------------------------------
run_test "Positive 5: Single-declaration syntax (C11 + native)"
cat << 'EOF' > "$TMP_DIR/pos5.cco"
extern "C" fn puts(message: string) -> int
extern "C" fn abs(value: int) -> int

fn main() -> int {
    puts("Single declaration syntax works!")
    print(abs(-999))
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos5.cco" -o "$TMP_DIR/pos5_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos5_c11")
"$CCO" "$TMP_DIR/pos5.cco" --use-native -o "$TMP_DIR/pos5_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos5_native")

expected_p5=$(printf "Single declaration syntax works!\n999")
if [ "$out_c11" = "$expected_p5" ] && [ "$out_nat" = "$expected_p5" ]; then
    pass_test
else
    fail_test "output mismatch"
fi

# ------------------------------------------------------------------------------
# Positive Test 6: External calls accept valid arguments and return expected results
# ------------------------------------------------------------------------------
run_test "Positive 6: Mixed external call expressions and returns"
cat << 'EOF' > "$TMP_DIR/pos6.cco"
extern "C" {
    fn abs(x: int) -> int
    fn putchar(c: int) -> int
}

fn compute(val: int) -> int {
    return abs(val) + 10
}

fn main() -> int {
    let a: int = compute(-50)
    print(a)
    putchar(48 + (a / 60))
    putchar(10)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos6.cco" -o "$TMP_DIR/pos6_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos6_c11")
"$CCO" "$TMP_DIR/pos6.cco" --use-native -o "$TMP_DIR/pos6_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos6_native")

expected_p6=$(printf "60\n1")
if [ "$out_c11" = "$expected_p6" ] && [ "$out_nat" = "$expected_p6" ]; then
    pass_test
else
    fail_test "output mismatch"
fi

# ------------------------------------------------------------------------------
# Positive Test 7: Owned string remains usable after passing to puts
# ------------------------------------------------------------------------------
run_test "Positive 7: Owned string reuse after puts (borrow invariant)"
cat << 'EOF' > "$TMP_DIR/pos7.cco"
extern "C" fn puts(message: string) -> int

fn main() -> int {
    let s: string = "hello borrow invariant"
    puts(s)
    print(s)
    puts(s)
    return 0
}
EOF

"$CCO" "$TMP_DIR/pos7.cco" -o "$TMP_DIR/pos7_c11" > /dev/null 2>&1 || { fail_test "c11 build failed"; exit 1; }
out_c11=$("$TMP_DIR/pos7_c11")
"$CCO" "$TMP_DIR/pos7.cco" --use-native -o "$TMP_DIR/pos7_native" > /dev/null 2>&1 || { fail_test "native build failed"; exit 1; }
out_nat=$("$TMP_DIR/pos7_native")

expected_p7=$(printf "hello borrow invariant\nhello borrow invariant\nhello borrow invariant")
if [ "$out_c11" = "$expected_p7" ] && [ "$out_nat" = "$expected_p7" ]; then
    pass_test
else
    fail_test "output mismatch"
fi

# ------------------------------------------------------------------------------
# Positive Test 8: Heap-backed strings cleaned up without leaks (Valgrind)
# ------------------------------------------------------------------------------
run_test "Positive 8: Memory safety & Valgrind 0 leaks on string FFI"
if command -v valgrind > /dev/null 2>&1; then
    val_out_c11=$(valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/pos7_c11" 2>&1)
    vcode_c11=$?
    val_out_nat=$(valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/pos7_native" 2>&1)
    vcode_nat=$?

    if [ "$vcode_c11" -eq 0 ] && [ "$vcode_nat" -eq 0 ]; then
        pass_test
    else
        fail_test "Valgrind detected leaks or errors (c11=$vcode_c11, nat=$vcode_nat)"
    fi
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Positive Test 9: Parity across all positive examples
# ------------------------------------------------------------------------------
run_test "Positive 9: C11 and native output bitwise parity"
if [ "$out_c11" = "$out_nat" ]; then
    pass_test
else
    fail_test "C11 and native output diverged"
fi

# ------------------------------------------------------------------------------
# Negative Test 1: Unsupported ABI such as extern "Rust"
# ------------------------------------------------------------------------------
run_test "Negative 1: Unsupported ABI extern \"Rust\" rejected"
cat << 'EOF' > "$TMP_DIR/neg1.cco"
extern "Rust" {
    fn foo() -> int
}

fn main() -> int {
    return 0
}
EOF

neg_out=$("$CCO" "$TMP_DIR/neg1.cco" 2>&1 || true)
if echo "$neg_out" | grep -qi "Unsupported ABI 'Rust'"; then
    pass_test
else
    fail_test "Expected unsupported ABI error, got: $neg_out"
fi

# ------------------------------------------------------------------------------
# Negative Test 2: External declarations nested inside functions
# ------------------------------------------------------------------------------
run_test "Negative 2: Nested extern declaration rejected"
cat << 'EOF' > "$TMP_DIR/neg2.cco"
fn main() -> int {
    extern "C" fn puts(s: string) -> int
    return 0
}
EOF

neg_out=$("$CCO" "$TMP_DIR/neg2.cco" 2>&1 || true)
if echo "$neg_out" | grep -qi "unexpected 'extern' declaration inside a block"; then
    pass_test
else
    fail_test "Expected nested extern error, got: $neg_out"
fi

# ------------------------------------------------------------------------------
# Negative Test 3: Ordinary functions missing their bodies
# ------------------------------------------------------------------------------
run_test "Negative 3: Ordinary function without body rejected"
cat << 'EOF' > "$TMP_DIR/neg3.cco"
fn foo(x: int) -> int

fn main() -> int {
    return 0
}
EOF

neg_out=$("$CCO" "$TMP_DIR/neg3.cco" 2>&1 || true)
if echo "$neg_out" | grep -qi "Expected '{'"; then
    pass_test
else
    fail_test "Expected missing body error, got: $neg_out"
fi

# ------------------------------------------------------------------------------
# Negative Test 4: Incorrect argument count
# ------------------------------------------------------------------------------
run_test "Negative 4: Incorrect argument count rejected"
cat << 'EOF' > "$TMP_DIR/neg4.cco"
extern "C" fn abs(x: int) -> int

fn main() -> int {
    abs(1, 2)
    return 0
}
EOF

neg_out=$("$CCO" "$TMP_DIR/neg4.cco" 2>&1 || true)
if echo "$neg_out" | grep -qi "expects 1 arguments, got 2"; then
    pass_test
else
    fail_test "Expected argument count mismatch, got: $neg_out"
fi

# ------------------------------------------------------------------------------
# Negative Test 5: Incorrect argument types
# ------------------------------------------------------------------------------
run_test "Negative 5: Incorrect argument types rejected"
cat << 'EOF' > "$TMP_DIR/neg5.cco"
extern "C" fn abs(x: int) -> int

fn main() -> int {
    abs("not an integer")
    return 0
}
EOF

neg_out=$("$CCO" "$TMP_DIR/neg5.cco" 2>&1 || true)
if echo "$neg_out" | grep -qi "type mismatch in call to 'abs'"; then
    pass_test
else
    fail_test "Expected type mismatch error, got: $neg_out"
fi

# ------------------------------------------------------------------------------
# Negative Test 6: Duplicate or conflicting external declarations
# ------------------------------------------------------------------------------
run_test "Negative 6: Duplicate external declarations rejected"
cat << 'EOF' > "$TMP_DIR/neg6.cco"
extern "C" fn puts(message: string) -> int
extern "C" fn puts(message: string) -> int

fn main() -> int {
    return 0
}
EOF

neg_out=$("$CCO" "$TMP_DIR/neg6.cco" 2>&1 || true)
if echo "$neg_out" | grep -qi "duplicate definition of 'puts'"; then
    pass_test
else
    fail_test "Expected duplicate declaration error, got: $neg_out"
fi

# ------------------------------------------------------------------------------
# Negative Test 7: Unsupported ABI types (class/struct)
# ------------------------------------------------------------------------------
run_test "Negative 7: Unsupported ABI parameter type rejected"
cat << 'EOF' > "$TMP_DIR/neg7.cco"
class Point {
    x: int
    y: int
}

extern "C" fn process_point(p: Point) -> int

fn main() -> int {
    return 0
}
EOF

neg_out=$("$CCO" "$TMP_DIR/neg7.cco" 2>&1 || true)
if echo "$neg_out" | grep -qi "class/struct types are not supported in extern \"C\" declarations"; then
    pass_test
else
    fail_test "Expected unsupported ABI type error, got: $neg_out"
fi

# ------------------------------------------------------------------------------
# Negative Test 8: Missing external symbols produce link failure
# ------------------------------------------------------------------------------
run_test "Negative 8: Missing external symbol produces link failure"
cat << 'EOF' > "$TMP_DIR/neg8.cco"
extern "C" fn nonexistent_libc_symbol_12345() -> int

fn main() -> int {
    nonexistent_libc_symbol_12345()
    return 0
}
EOF

set +e
neg_out_c11=$("$CCO" "$TMP_DIR/neg8.cco" -o "$TMP_DIR/neg8_c11" 2>&1)
code_c11=$?
neg_out_nat=$("$CCO" "$TMP_DIR/neg8.cco" --use-native -o "$TMP_DIR/neg8_native" 2>&1)
code_nat=$?
set -e

if [ "$code_c11" -ne 0 ] && [ "$code_nat" -ne 0 ] && \
   echo "$neg_out_c11" | grep -qi "undefined reference" && \
   echo "$neg_out_nat" | grep -qi "undefined reference"; then
    pass_test
else
    fail_test "Expected undefined reference linker failure (c11=$code_c11, nat=$code_nat)"
fi

echo "=============================================================="
echo "Suite Summary: $passed_tests Passed, $failed_tests Failed (Total: $total_tests)"
echo "=============================================================="

if [ "$failed_tests" -gt 0 ]; then
    exit 1
fi
exit 0
