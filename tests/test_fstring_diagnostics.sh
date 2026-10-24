#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Compiler — F-String Source Diagnostic Location Regression Test Suite
# Validates exact line and column tracking for embedded f-string expressions
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_fstring_diag_tests_$$"

mkdir -p "$TMP_DIR"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

PASSED=0
FAILED=0

pass() {
    echo -e "  \033[32m[PASS]\033[0m $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo -e "  \033[31m[FAIL]\033[0m $1: $2"
    FAILED=$((FAILED + 1))
}

echo "=============================================================="
echo "Cco Compiler: F-String Diagnostic Location Regression Suite"
echo "=============================================================="

# Test 1: Undefined identifier in a single-line f-string (Confirmed Bug Reproduction)
echo "Test 1: Undefined variable in single-line f-string (q.cco)..."
cat << 'EOF' > "$TMP_DIR/test1_q.cco"
name = "Cco";

print(f"Language: {name}");
print(f"Missing value: {missing}");
EOF

set +e
"$CCO" "$TMP_DIR/test1_q.cco" -o "$TMP_DIR/test1_q" > "$TMP_DIR/test1_q.out" 2> "$TMP_DIR/test1_q.err"
EXIT1=$?
set -e

if [[ $EXIT1 -ne 0 ]] && grep -q "test1_q.cco:4:25" "$TMP_DIR/test1_q.err" && grep -q "undefined variable 'missing'" "$TMP_DIR/test1_q.err"; then
    pass "Reported exact location line 4, col 25 for undefined 'missing' inside f-string"
else
    fail "Test 1 location" "Expected 4:25 with undefined variable 'missing', got:\n$(cat "$TMP_DIR/test1_q.err")"
fi

# Test 2: Ordinary undefined identifier (r.cco)
echo "Test 2: Ordinary undefined identifier (r.cco)..."
cat << 'EOF' > "$TMP_DIR/test2_r.cco"
name = "Cco";
print(missing);
EOF

set +e
"$CCO" "$TMP_DIR/test2_r.cco" -o "$TMP_DIR/test2_r" > "$TMP_DIR/test2_r.out" 2> "$TMP_DIR/test2_r.err"
EXIT2=$?
set -e

if [[ $EXIT2 -ne 0 ]] && grep -q "test2_r.cco:2:7" "$TMP_DIR/test2_r.err" && grep -q "undefined variable 'missing'" "$TMP_DIR/test2_r.err"; then
    pass "Ordinary undefined identifier correctly reports line 2, col 7"
else
    fail "Test 2 location" "Expected 2:7 with undefined variable 'missing', got:\n$(cat "$TMP_DIR/test2_r.err")"
fi

# Test 3: Valid interpolation execution
echo "Test 3: Valid f-string interpolation compiles and executes..."
cat << 'EOF' > "$TMP_DIR/test3_valid.cco"
name = "Cco";
print(f"Language: {name}");
EOF

"$CCO" "$TMP_DIR/test3_valid.cco" -o "$TMP_DIR/test3_valid"
OUT3=$("$TMP_DIR/test3_valid")
if [[ "$OUT3" == "Language: Cco" ]]; then
    pass "Valid f-string compiles and executes correctly ('Language: Cco')"
else
    fail "Test 3 output" "Expected 'Language: Cco', got '$OUT3'"
fi

# Test 4: Undefined identifier after literal text
echo "Test 4: Undefined identifier after prefix text and first interpolation..."
cat << 'EOF' > "$TMP_DIR/test4_after_literal.cco"
a = 10;
print(f"Value: {a}; missing: {missing}");
EOF

set +e
"$CCO" "$TMP_DIR/test4_after_literal.cco" -o "$TMP_DIR/test4_after_literal" > "$TMP_DIR/test4_after_literal.out" 2> "$TMP_DIR/test4_after_literal.err"
EXIT4=$?
set -e

if [[ $EXIT4 -ne 0 ]] && grep -q "test4_after_literal.cco:2:31" "$TMP_DIR/test4_after_literal.err"; then
    pass "Diagnostic points directly to 'missing' at col 31, not f-string start or earlier text"
else
    fail "Test 4 location" "Expected 2:31, got:\n$(cat "$TMP_DIR/test4_after_literal.err")"
fi

# Test 5: Undefined identifier in later interpolation
echo "Test 5: Undefined identifier in second interpolation..."
cat << 'EOF' > "$TMP_DIR/test5_later_interp.cco"
a = 10;
print(f"{a} + {missing}");
EOF

set +e
"$CCO" "$TMP_DIR/test5_later_interp.cco" -o "$TMP_DIR/test5_later_interp" > "$TMP_DIR/test5_later_interp.out" 2> "$TMP_DIR/test5_later_interp.err"
EXIT5=$?
set -e

if [[ $EXIT5 -ne 0 ]] && grep -q "test5_later_interp.cco:2:16" "$TMP_DIR/test5_later_interp.err"; then
    pass "Diagnostic points directly to second interpolation identifier at col 16"
else
    fail "Test 5 location" "Expected 2:16, got:\n$(cat "$TMP_DIR/test5_later_interp.err")"
fi

# Test 6: Multi-line file tracking & multiline f-string boundary
echo "Test 6: Multi-line source file tracking and multiline f-string rejection..."
cat << 'EOF' > "$TMP_DIR/test6_multiline_file.cco"
# Line 1
# Line 2
# Line 3
# Line 4
# Line 5
# Line 6
# Line 7
# Line 8
# Line 9
print(f"Deep in file: {missing}");
EOF

set +e
"$CCO" "$TMP_DIR/test6_multiline_file.cco" -o "$TMP_DIR/test6_multiline_file" > "$TMP_DIR/test6_multiline_file.out" 2> "$TMP_DIR/test6_multiline_file.err"
EXIT6A=$?
set -e

if [[ $EXIT6A -ne 0 ]] && grep -q "test6_multiline_file.cco:10:24" "$TMP_DIR/test6_multiline_file.err"; then
    pass "Diagnostic correctly tracks line 10, col 24 deep in source file"
else
    fail "Test 6A location" "Expected 10:24, got:\n$(cat "$TMP_DIR/test6_multiline_file.err")"
fi

# Raw newline inside f-string must be rejected as unterminated
cat << 'EOF' > "$TMP_DIR/test6_raw_newline.cco"
print(f"hello
{missing}");
EOF

set +e
"$CCO" "$TMP_DIR/test6_raw_newline.cco" -o "$TMP_DIR/test6_raw_newline" > "$TMP_DIR/test6_raw_newline.out" 2> "$TMP_DIR/test6_raw_newline.err"
EXIT6B=$?
set -e

if [[ $EXIT6B -ne 0 ]] && grep -q "unterminated f-string literal" "$TMP_DIR/test6_raw_newline.err"; then
    pass "Raw newline inside f-string cleanly rejected by lexer as unterminated"
else
    fail "Test 6B rejection" "Expected unterminated f-string error, got:\n$(cat "$TMP_DIR/test6_raw_newline.err")"
fi

# Test 7: Expression diagnostics inside f-strings (undefined function & syntax errors)
echo "Test 7: Other expression diagnostics inside f-strings..."
cat << 'EOF' > "$TMP_DIR/test7_fn.cco"
print(f"Calculate: {compute_sum(10, 20)}");
EOF

set +e
"$CCO" "$TMP_DIR/test7_fn.cco" -o "$TMP_DIR/test7_fn" > "$TMP_DIR/test7_fn.out" 2> "$TMP_DIR/test7_fn.err"
EXIT7A=$?
set -e

if [[ $EXIT7A -ne 0 ]] && grep -q "test7_fn.cco:1:21" "$TMP_DIR/test7_fn.err" && grep -q "undefined function 'compute_sum'" "$TMP_DIR/test7_fn.err"; then
    pass "Undefined function inside f-string accurately points to line 1, col 21 ('compute_sum')"
else
    fail "Test 7A function" "Expected 1:21 undefined function 'compute_sum', got:\n$(cat "$TMP_DIR/test7_fn.err")"
fi

cat << 'EOF' > "$TMP_DIR/test7_syntax.cco"
print(f"Value: {1 + }");
EOF

set +e
"$CCO" "$TMP_DIR/test7_syntax.cco" -o "$TMP_DIR/test7_syntax" > "$TMP_DIR/test7_syntax.out" 2> "$TMP_DIR/test7_syntax.err"
EXIT7B=$?
set -e

if [[ $EXIT7B -ne 0 ]] && grep -q "test7_syntax.cco:1:21" "$TMP_DIR/test7_syntax.err"; then
    pass "Syntax error inside f-string accurately points to line 1, col 21 ('}')"
else
    fail "Test 7B syntax" "Expected 1:21 syntax error, got:\n$(cat "$TMP_DIR/test7_syntax.err")"
fi

# Test 8: Valgrind memory leak verification
echo "Test 8: Valgrind memory leak verification..."
if command -v valgrind > /dev/null 2>&1; then
    "$CCO" "$TMP_DIR/test3_valid.cco" -o "$TMP_DIR/valg_bin" > /dev/null 2>&1
    valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/valg_bin" > /dev/null 2>&1
    pass "Valid f-string executable runs 100% clean under Valgrind (0 leaks, 0 errors)"
else
    echo "  (Valgrind not available, skipping)"
fi

# Test 9: Standalone fixture files verification
echo "Test 9: Standalone diagnostic fixture files..."
if [[ -f "$ROOT_DIR/tests/diagnostics/fstring_undefined_variable.cco" ]]; then
    set +e
    "$CCO" "$ROOT_DIR/tests/diagnostics/fstring_undefined_variable.cco" -o /dev/null 2> "$TMP_DIR/fixture1.err" > /dev/null
    EXIT_F1=$?
    set -e
    if [[ $EXIT_F1 -ne 0 ]] && grep -q "4:29" "$TMP_DIR/fixture1.err" && grep -q "undefined variable 'missing'" "$TMP_DIR/fixture1.err"; then
        pass "Fixture fstring_undefined_variable.cco reports exact location 4:29"
    else
        fail "Fixture 1" "Expected 4:29 with undefined variable 'missing', got:\n$(cat "$TMP_DIR/fixture1.err")"
    fi
fi

if [[ -f "$ROOT_DIR/tests/diagnostics/fstring_undefined_later.cco" ]]; then
    set +e
    "$CCO" "$ROOT_DIR/tests/diagnostics/fstring_undefined_later.cco" -o /dev/null 2> "$TMP_DIR/fixture2.err" > /dev/null
    EXIT_F2=$?
    set -e
    if [[ $EXIT_F2 -ne 0 ]] && grep -q "4:20" "$TMP_DIR/fixture2.err" && grep -q "undefined variable 'missing'" "$TMP_DIR/fixture2.err"; then
        pass "Fixture fstring_undefined_later.cco reports exact location 4:20"
    else
        fail "Fixture 2" "Expected 4:20 with undefined variable 'missing', got:\n$(cat "$TMP_DIR/fixture2.err")"
    fi
fi

echo "=============================================================="
echo "F-String Diagnostics Suite Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
