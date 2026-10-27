#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Compiler — Optional Semicolons & Newline-Aware Statement Termination
# Comprehensive Regression Test Suite (Covers Requirements 4A–4I & Section 6)
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_optional_semicolons_tests_$$"

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
echo "Cco Compiler: Optional Semicolons & Statement Termination Suite"
echo "=============================================================="

# ------------------------------------------------------------------------------
# 1. Declarations without semicolons
# ------------------------------------------------------------------------------
echo "Test 1: Declarations without semicolons..."
cat << 'EOF' > "$TMP_DIR/t1_decl.cco"
let a = 10
let b: int = 20
c: int = 30
print(a + b + c)
EOF

"$CCO" "$TMP_DIR/t1_decl.cco" -o "$TMP_DIR/t1_decl_bin"
OUT1=$("$TMP_DIR/t1_decl_bin")
if [[ "$OUT1" == "60" ]]; then
    pass "Variable declarations without semicolons compile and execute correctly (got 60)"
else
    fail "Test 1" "Expected 60, got '$OUT1'"
fi

# ------------------------------------------------------------------------------
# 2. Assignments without semicolons
# ------------------------------------------------------------------------------
echo "Test 2: Assignments without semicolons..."
cat << 'EOF' > "$TMP_DIR/t2_assign.cco"
let x = 10
x = 25
x += 5
x++
x--
print(x)
EOF

"$CCO" "$TMP_DIR/t2_assign.cco" -o "$TMP_DIR/t2_assign_bin"
OUT2=$("$TMP_DIR/t2_assign_bin")
if [[ "$OUT2" == "30" ]]; then
    pass "Reassignments and compound assignments without semicolons execute correctly (got 30)"
else
    fail "Test 2" "Expected 30, got '$OUT2'"
fi

# ------------------------------------------------------------------------------
# 3. Expression statements without semicolons
# ------------------------------------------------------------------------------
echo "Test 3: Expression statements without semicolons..."
cat << 'EOF' > "$TMP_DIR/t3_expr_stmt.cco"
fn side_effect() -> int {
    print("side effect executed")
    return 42
}

side_effect()
print("done")
EOF

"$CCO" "$TMP_DIR/t3_expr_stmt.cco" -o "$TMP_DIR/t3_expr_stmt_bin"
OUT3=$("$TMP_DIR/t3_expr_stmt_bin")
EXPECTED_3="side effect executed
done"
if [[ "$OUT3" == "$EXPECTED_3" ]]; then
    pass "Expression statement without semicolon evaluates correctly"
else
    fail "Test 3" "Expected '$EXPECTED_3', got '$OUT3'"
fi

# ------------------------------------------------------------------------------
# 4. Function calls without semicolons
# ------------------------------------------------------------------------------
echo "Test 4: Function calls without semicolons..."
cat << 'EOF' > "$TMP_DIR/t4_calls.cco"
fn add(a: int, b: int) -> int {
    return a + b
}

let sum = add(15, 35)
print(sum)
EOF

"$CCO" "$TMP_DIR/t4_calls.cco" -o "$TMP_DIR/t4_calls_bin"
OUT4=$("$TMP_DIR/t4_calls_bin")
if [[ "$OUT4" == "50" ]]; then
    pass "Function calls without semicolons execute and return expected values (got 50)"
else
    fail "Test 4" "Expected 50, got '$OUT4'"
fi

# ------------------------------------------------------------------------------
# 5. Existing semicolon-terminated statements (Requirement 4.A)
# ------------------------------------------------------------------------------
echo "Test 5: Existing semicolon-terminated statements..."
cat << 'EOF' > "$TMP_DIR/t5_semicolons.cco"
let a = 10;
let b = 20;
let result = a + b;
print(result);
EOF

"$CCO" "$TMP_DIR/t5_semicolons.cco" -o "$TMP_DIR/t5_semicolons_bin"
OUT5=$("$TMP_DIR/t5_semicolons_bin")
if [[ "$OUT5" == "30" ]]; then
    pass "Existing semicolon syntax continues working completely intact (got 30)"
else
    fail "Test 5" "Expected 30, got '$OUT5'"
fi

# ------------------------------------------------------------------------------
# 6. Mixed semicolon and newline termination (Requirement 4.C)
# ------------------------------------------------------------------------------
echo "Test 6: Mixed semicolon and newline termination..."
cat << 'EOF' > "$TMP_DIR/t6_mixed.cco"
let a = 10
let b = 20;
let result = a + b
print(result);
EOF

"$CCO" "$TMP_DIR/t6_mixed.cco" -o "$TMP_DIR/t6_mixed_bin"
OUT6=$("$TMP_DIR/t6_mixed_bin")
if [[ "$OUT6" == "30" ]]; then
    pass "Mixed semicolon and newline termination works seamlessly (got 30)"
else
    fail "Test 6" "Expected 30, got '$OUT6'"
fi

# ------------------------------------------------------------------------------
# 7. Multiple consecutive statements (Requirement 4.D)
# ------------------------------------------------------------------------------
echo "Test 7: Consecutive statements remaining distinct..."
cat << 'EOF' > "$TMP_DIR/t7_consecutive.cco"
let first = 10
let second = 20
let third = 30

print(first)
print(second)
print(third)
EOF

"$CCO" "$TMP_DIR/t7_consecutive.cco" -o "$TMP_DIR/t7_consecutive_bin"
OUT7=$("$TMP_DIR/t7_consecutive_bin")
EXPECTED_7="10
20
30"
if [[ "$OUT7" == "$EXPECTED_7" ]]; then
    pass "Consecutive statements remain strictly distinct (got 10, 20, 30)"
else
    fail "Test 7" "Expected '$EXPECTED_7', got '$OUT7'"
fi

# ------------------------------------------------------------------------------
# 8. Blank lines between statements
# ------------------------------------------------------------------------------
echo "Test 8: Blank lines between statements..."
cat << 'EOF' > "$TMP_DIR/t8_blank_lines.cco"
let a = 100



let b = 200


print(a + b)
EOF

"$CCO" "$TMP_DIR/t8_blank_lines.cco" -o "$TMP_DIR/t8_blank_lines_bin"
OUT8=$("$TMP_DIR/t8_blank_lines_bin")
if [[ "$OUT8" == "300" ]]; then
    pass "Multiple blank lines between statements do not affect parsing (got 300)"
else
    fail "Test 8" "Expected 300, got '$OUT8'"
fi

# ------------------------------------------------------------------------------
# 9. Comments between statements (Requirement 4.F)
# ------------------------------------------------------------------------------
echo "Test 9: Comments and blank lines between statements..."
cat << 'EOF' > "$TMP_DIR/t9_comments.cco"
let a = 10

# Hash comment between statements
// Slash comment between statements
/* Block comment */
let b = 20

print(a + b)
EOF

"$CCO" "$TMP_DIR/t9_comments.cco" -o "$TMP_DIR/t9_comments_bin"
OUT9=$("$TMP_DIR/t9_comments_bin")
if [[ "$OUT9" == "30" ]]; then
    pass "Comments (#, //, /* */) between statements preserve separation (got 30)"
else
    fail "Test 9" "Expected 30, got '$OUT9'"
fi

# ------------------------------------------------------------------------------
# 10. Multiline binary expressions (Requirement 4.E)
# ------------------------------------------------------------------------------
echo "Test 10: Multiline binary expressions..."
cat << 'EOF' > "$TMP_DIR/t10_multiline_expr.cco"
let result = 10 +
    20 +
    30

print(result)
EOF

"$CCO" "$TMP_DIR/t10_multiline_expr.cco" -o "$TMP_DIR/t10_multiline_expr_bin"
OUT10=$("$TMP_DIR/t10_multiline_expr_bin")
if [[ "$OUT10" == "60" ]]; then
    pass "Multiline binary expressions continue across newlines after operators (got 60)"
else
    fail "Test 10" "Expected 60, got '$OUT10'"
fi

# ------------------------------------------------------------------------------
# 11. Multiline expressions in parentheses
# ------------------------------------------------------------------------------
echo "Test 11: Multiline expressions in parentheses..."
cat << 'EOF' > "$TMP_DIR/t11_paren_expr.cco"
let result = (
    100 -
    40 +
    10
)

print(result)
EOF

"$CCO" "$TMP_DIR/t11_paren_expr.cco" -o "$TMP_DIR/t11_paren_expr_bin"
OUT11=$("$TMP_DIR/t11_paren_expr_bin")
if [[ "$OUT11" == "70" ]]; then
    pass "Parenthesized multiline expressions evaluated properly without premature termination (got 70)"
else
    fail "Test 11" "Expected 70, got '$OUT11'"
fi

# ------------------------------------------------------------------------------
# 12. Nested function calls and argument lists (Requirement 4.H)
# ------------------------------------------------------------------------------
echo "Test 12: Nested function calls and multiline argument lists..."
cat << 'EOF' > "$TMP_DIR/t12_nested_calls.cco"
fn multiply(x: int, y: int) -> int {
    return x * y
}

fn add(x: int, y: int) -> int {
    return x + y
}

let result = add(
    multiply(2, 5),
    multiply(
        3,
        4
    )
)

print(result)
EOF

"$CCO" "$TMP_DIR/t12_nested_calls.cco" -o "$TMP_DIR/t12_nested_calls_bin"
OUT12=$("$TMP_DIR/t12_nested_calls_bin")
if [[ "$OUT12" == "22" ]]; then
    pass "Nested calls and multiline argument lists work accurately (got 22)"
else
    fail "Test 12" "Expected 22, got '$OUT12'"
fi

# ------------------------------------------------------------------------------
# 13. Nested blocks and scope preservation (Requirement 4.G)
# ------------------------------------------------------------------------------
echo "Test 13: Nested blocks and scope preservation..."
cat << 'EOF' > "$TMP_DIR/t13_blocks.cco"
let a = 10
{
    let b = 20
    let a = b + 5
    print(a)
}
print(a)
EOF

"$CCO" "$TMP_DIR/t13_blocks.cco" -o "$TMP_DIR/t13_blocks_bin"
OUT13=$("$TMP_DIR/t13_blocks_bin")
EXPECTED_13="25
10"
if [[ "$OUT13" == "$EXPECTED_13" ]]; then
    pass "Nested blocks and lexical variable shadowing behave correctly without semicolons"
else
    fail "Test 13" "Expected '$EXPECTED_13', got '$OUT13'"
fi

# ------------------------------------------------------------------------------
# 14. Statements immediately before closing braces
# ------------------------------------------------------------------------------
echo "Test 14: Statements immediately before closing braces..."
cat << 'EOF' > "$TMP_DIR/t14_closing_brace.cco"
fn same_line_brace() -> int { return 42 }
fn newline_brace() -> int {
    return 99
}

print(same_line_brace())
print(newline_brace())
EOF

"$CCO" "$TMP_DIR/t14_closing_brace.cco" -o "$TMP_DIR/t14_closing_brace_bin"
OUT14=$("$TMP_DIR/t14_closing_brace_bin")
EXPECTED_14="42
99"
if [[ "$OUT14" == "$EXPECTED_14" ]]; then
    pass "Statements immediately followed by '}' terminate cleanly on same line and newline"
else
    fail "Test 14" "Expected '$EXPECTED_14', got '$OUT14'"
fi

# ------------------------------------------------------------------------------
# 15. Empty or comment-only file diagnostics
# ------------------------------------------------------------------------------
echo "Test 15: Empty or comment-only file rejection..."
cat << 'EOF' > "$TMP_DIR/t15_empty.cco"
# Only comments in this file
EOF

set +e
"$CCO" "$TMP_DIR/t15_empty.cco" -o "$TMP_DIR/t15_empty_bin" > "$TMP_DIR/t15_empty.out" 2> "$TMP_DIR/t15_empty.err"
EXIT15=$?
set -e

if [[ $EXIT15 -ne 0 ]] && grep -q "source file contains no declarations or statements" "$TMP_DIR/t15_empty.err"; then
    pass "Empty/comment-only file safely rejected with non-zero exit and diagnostic E1005"
else
    fail "Test 15" "Expected non-zero exit with E1005 error, got exit $EXIT15"
fi

# ------------------------------------------------------------------------------
# 16. Invalid incomplete declarations (Requirement 4.I)
# ------------------------------------------------------------------------------
echo "Test 16: Invalid incomplete declarations..."
cat << 'EOF' > "$TMP_DIR/t16_inc_decl.cco"
let value =
EOF

set +e
"$CCO" "$TMP_DIR/t16_inc_decl.cco" -o /dev/null > "$TMP_DIR/t16_inc_decl.out" 2> "$TMP_DIR/t16_inc_decl.err"
EXIT16A=$?
set -e

cat << 'EOF' > "$TMP_DIR/t16_no_name.cco"
let = 10
EOF

set +e
"$CCO" "$TMP_DIR/t16_no_name.cco" -o /dev/null > "$TMP_DIR/t16_no_name.out" 2> "$TMP_DIR/t16_no_name.err"
EXIT16B=$?
set -e

if [[ $EXIT16A -ne 0 ]] && grep -q "Expected expression" "$TMP_DIR/t16_inc_decl.err" && \
   [[ $EXIT16B -ne 0 ]] && grep -q "Expected variable name after 'let'" "$TMP_DIR/t16_no_name.err"; then
    pass "Incomplete declarations ('let value =', 'let = 10') consistently rejected with diagnostics"
else
    fail "Test 16" "Expected syntax errors on incomplete declarations"
fi

# ------------------------------------------------------------------------------
# 17. Incomplete expressions (Requirement 4.I)
# ------------------------------------------------------------------------------
echo "Test 17: Incomplete expressions..."
cat << 'EOF' > "$TMP_DIR/t17_inc_expr.cco"
let total = 10 +
EOF

set +e
"$CCO" "$TMP_DIR/t17_inc_expr.cco" -o /dev/null > "$TMP_DIR/t17_inc_expr.out" 2> "$TMP_DIR/t17_inc_expr.err"
EXIT17=$?
set -e

if [[ $EXIT17 -ne 0 ]] && grep -q "Expected expression" "$TMP_DIR/t17_inc_expr.err"; then
    pass "Incomplete binary expression ('10 +') correctly rejected at EOF with non-zero exit"
else
    fail "Test 17" "Expected syntax error on '10 +', got exit $EXIT17"
fi

# ------------------------------------------------------------------------------
# 18. Unexpected tokens following completed statements on same line (Requirement 4.I)
# ------------------------------------------------------------------------------
echo "Test 18: Adjacent statements on the same line without semicolon..."
cat << 'EOF' > "$TMP_DIR/t18_adjacent.cco"
x = 10 y = 20;
EOF

set +e
"$CCO" "$TMP_DIR/t18_adjacent.cco" -o /dev/null > "$TMP_DIR/t18_adjacent.out" 2> "$TMP_DIR/t18_adjacent.err"
EXIT18=$?
set -e

if [[ $EXIT18 -ne 0 ]] && grep -q "expected ';'" "$TMP_DIR/t18_adjacent.err" && grep -q "1:7" "$TMP_DIR/t18_adjacent.err"; then
    pass "Adjacent statements on same line without semicolon accurately rejected with E1001 at col 7"
else
    fail "Test 18" "Expected expected ';' diagnostic at 1:7, got:\n$(cat "$TMP_DIR/t18_adjacent.err")"
fi

# ------------------------------------------------------------------------------
# 19. Accurate diagnostic locations
# ------------------------------------------------------------------------------
echo "Test 19: Accurate diagnostic locations..."
cat << 'EOF' > "$TMP_DIR/t19_diag.cco"
# Line 1
# Line 2
let foo = 10
let bar = missing_identifier
EOF

set +e
"$CCO" "$TMP_DIR/t19_diag.cco" -o /dev/null > "$TMP_DIR/t19_diag.out" 2> "$TMP_DIR/t19_diag.err"
EXIT19=$?
set -e

if [[ $EXIT19 -ne 0 ]] && grep -q "t19_diag.cco:4:11" "$TMP_DIR/t19_diag.err" && grep -q "undefined variable 'missing_identifier'" "$TMP_DIR/t19_diag.err"; then
    pass "Diagnostics report exact source file, line 4, and column 11 with accurate carets"
else
    fail "Test 19" "Expected location 4:11 with undefined variable, got:\n$(cat "$TMP_DIR/t19_diag.err")"
fi

# ------------------------------------------------------------------------------
# 20. Existing f-string diagnostic regression verification
# ------------------------------------------------------------------------------
echo "Test 20: Existing f-string diagnostic regression cases..."
cat << 'EOF' > "$TMP_DIR/t20_fstring.cco"
name = "Cco"

print(f"Language: {name}")
print(f"Missing value: {missing}")
EOF

set +e
"$CCO" "$TMP_DIR/t20_fstring.cco" -o /dev/null > "$TMP_DIR/t20_fstring.out" 2> "$TMP_DIR/t20_fstring.err"
EXIT20=$?
set -e

if [[ $EXIT20 -ne 0 ]] && grep -q "t20_fstring.cco:4:25" "$TMP_DIR/t20_fstring.err" && grep -q "undefined variable 'missing'" "$TMP_DIR/t20_fstring.err"; then
    pass "F-string diagnostic location accuracy preserved (line 4, col 25) under optional semicolons"
else
    fail "Test 20" "Expected 4:25 inside f-string under optional semicolons, got:\n$(cat "$TMP_DIR/t20_fstring.err")"
fi

# ------------------------------------------------------------------------------
# 21. Valgrind memory leak verification
# ------------------------------------------------------------------------------
echo "Test 21: Valgrind memory leak check on semicolon-free program..."
if command -v valgrind > /dev/null 2>&1; then
    valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t6_mixed_bin" > /dev/null 2>&1
    valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t12_nested_calls_bin" > /dev/null 2>&1
    pass "All optional-semicolon binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
else
    echo "  (Valgrind not available, skipping)"
fi

echo "=============================================================="
echo "Optional Semicolons Suite: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
