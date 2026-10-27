#!/usr/bin/env bash
# ==============================================================
# Cco Compiler — Phase 17A Diagnostic Test Suite Runner
# Validates source diagnostics, error codes, locations & carets
# ==============================================================

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

CCO="./cco"
if [ ! -f "$CCO" ]; then
    echo "Building cco compiler..."
    make cco
fi

echo "=============================================================="
echo "Cco Compiler — Phase 17A Source Diagnostics Test Suite"
echo "=============================================================="

TOTAL=0
PASSED=0
FAILED=0

run_diag_test() {
    local test_file="$1"
    local exp_code="$2"
    local exp_line="$3"
    local exp_col="$4"
    local exp_msg="$5"
    local exp_help="$6"
    local test_name
    test_name=$(basename "$test_file" .cco)
    TOTAL=$((TOTAL + 1))

    echo -n "[$TOTAL] Diagnostic test $test_name ... "

    local err_file="build/diag_${test_name}.err"
    mkdir -p build

    set +e
    $CCO --codes "$test_file" -o "build/diag_${test_name}.c" 2> "$err_file" > /dev/null
    local exit_code=$?
    set -e

    if [ "$exit_code" -eq 0 ]; then
        echo "FAILED (Expected exit code 1, got 0)"
        FAILED=$((FAILED + 1))
        return
    fi

    # 1. Verify Error Code
    if ! grep -q "$exp_code" "$err_file"; then
        echo "FAILED (Expected error code $exp_code not found in stderr)"
        cat "$err_file"
        FAILED=$((FAILED + 1))
        return
    fi

    # 2. Verify Line:Column location
    if ! grep -q ":${exp_line}:${exp_col}" "$err_file"; then
        echo "FAILED (Expected location :${exp_line}:${exp_col} not found in stderr)"
        cat "$err_file"
        FAILED=$((FAILED + 1))
        return
    fi

    # 3. Verify message content
    if [ -n "$exp_msg" ] && ! grep -qi "$exp_msg" "$err_file"; then
        echo "FAILED (Expected message substring '$exp_msg' not found in stderr)"
        cat "$err_file"
        FAILED=$((FAILED + 1))
        return
    fi

    # 4. Verify caret presence
    if ! grep -q "\^" "$err_file"; then
        echo "FAILED (Caret '^' not found in stderr)"
        cat "$err_file"
        FAILED=$((FAILED + 1))
        return
    fi

    # 5. Verify help note if expected
    if [ -n "$exp_help" ] && ! grep -q "help: $exp_help" "$err_file"; then
        echo "FAILED (Expected help '$exp_help' not found in stderr)"
        cat "$err_file"
        FAILED=$((FAILED + 1))
        return
    fi

    echo "PASSED ($exp_code at ${exp_line}:${exp_col})"
    PASSED=$((PASSED + 1))
}

echo "--- 1. Missing Semicolon Location Tests (Primary Bug) ---"
run_diag_test "tests/diagnostics/missing_semicolon_assignment.cco" "E1001" "2" "9" "expected ';' after assignment" "add ';' after \`21\`"
run_diag_test "tests/diagnostics/missing_semicolon_print.cco" "E1001" "2" "18" "expected ';' after print statement" "add ';' after \`)\`"
run_diag_test "tests/diagnostics/missing_semicolon_eof.cco" "E1001" "2" "22" "expected ';' after print statement" "add ';' after \`)\`"
run_diag_test "tests/diagnostics/missing_semicolon_adjacent.cco" "E1001" "2" "7" "expected ';' after assignment" "add ';' after \`10\`"

echo "--- 2. Syntax & Delimiter Tests ---"
run_diag_test "tests/diagnostics/unterminated_string.cco" "E1004" "3" "21" "unterminated string literal" ""
run_diag_test "tests/diagnostics/unclosed_parenthesis.cco" "E1003" "3" "26" "expected ')'" "add ')' here"
run_diag_test "tests/diagnostics/unclosed_brace.cco" "E1003" "5" "2" "expected '}'" "add '}' here"
run_diag_test "tests/diagnostics/malformed_expression.cco" "E1005" "3" "23" "Expected expression" ""

echo "--- 3. Name & Declaration Tests ---"
run_diag_test "tests/diagnostics/undefined_variable.cco" "E2001" "4" "11" "undefined variable 'b'" ""
run_diag_test "tests/diagnostics/undefined_function.cco" "E2002" "3" "5" "undefined function 'compute_something'" ""
run_diag_test "tests/diagnostics/duplicate_declaration.cco" "E2003" "3" "1" "duplicate definition of 'foo'" ""

echo "--- 4. Type & Signature Tests ---"
run_diag_test "tests/diagnostics/type_mismatch.cco" "E3001" "3" "5" "type mismatch in variable declaration" ""
run_diag_test "tests/diagnostics/invalid_reassignment.cco" "E3002" "4" "5" "cannot assign value of type 'string' to variable 'x' of type 'int'" ""
run_diag_test "tests/diagnostics/wrong_argument_count.cco" "E3003" "7" "20" "function 'compute' expects 2 arguments, got 1" ""
run_diag_test "tests/diagnostics/invalid_return_type.cco" "E3004" "3" "5" "return type mismatch — expected int, but returning string" ""

echo "--- 5. Return Statement Semicolon Tests (Phase 17B) ---"
run_diag_test "tests/diagnostics/bare_return_missing_semicolon.cco" "E1001" "3" "11" "expected ';' after return" "add ';' after \`return\`"
run_diag_test "tests/diagnostics/return_expr_missing_semicolon.cco" "E1001" "3" "14" "expected ';' after return" "add ';' after \`42\`"

echo "--- 6. Delimiter & Local Declaration Tests (Phase 17B) ---"
run_diag_test "tests/diagnostics/duplicate_local_declaration.cco" "E2003" "4" "5" "duplicate variable declaration 'a'" ""
run_diag_test "tests/diagnostics/unclosed_paren_same_line.cco" "E1003" "3" "16" "expected ')'" "add ')' here"
run_diag_test "tests/diagnostics/unclosed_bracket.cco" "E1003" "4" "21" "expected ']'" "add ']' here"

echo "--- 7. Empty & Comment-Only File Diagnostics (Phase 17B) ---"
run_diag_test "tests/diagnostics/empty_file.cco" "E1005" "1" "1" "source file contains no declarations or statements" ""
run_diag_test "tests/diagnostics/comment_only.cco" "E1005" "1" "1" "source file contains no declarations or statements" ""

echo "--- 8. Self-Hosted Compiler Exit & Error Verification (Phase 17B) ---"
TOTAL=$((TOTAL + 1))
echo -n "[$TOTAL] Self-hosted compiler exits non-zero on syntax error ... "
set +e
./selfhost/cco tests/diagnostics/missing_semicolon_assignment.cco -o /dev/null > "build/diag_sh.out" 2>&1
sh_exit=$?
set -e
if [ "$sh_exit" -eq 1 ] && ! grep -q "Compilation succeeded." "build/diag_sh.out" && grep -q "Parsing errors encountered." "build/diag_sh.out"; then
    echo "PASSED (Exit code 1, halted before codegen)"
    PASSED=$((PASSED + 1))
else
    echo "FAILED (Expected exit code 1 without 'Compilation succeeded', got exit $sh_exit)"
    cat "build/diag_sh.out"
    FAILED=$((FAILED + 1))
fi

echo "--- 9. F-String Diagnostic Location Tests ---"
run_diag_test "tests/diagnostics/fstring_undefined_variable.cco" "E2001" "4" "29" "undefined variable 'missing'" ""
run_diag_test "tests/diagnostics/fstring_undefined_later.cco" "E2001" "4" "20" "undefined variable 'missing'" ""

echo "=============================================================="
echo "Phase 17 Diagnostic Test Suite Results: $PASSED / $TOTAL Passed ($(( PASSED * 100 / TOTAL ))% Success)"
echo "=============================================================="

if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
exit 0
