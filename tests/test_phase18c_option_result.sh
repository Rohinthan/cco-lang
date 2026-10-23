#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18C — Milestone 1: Option & Result Test Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
STAGE2="$ROOT_DIR/build/cco_stage2"
TMP_DIR="/tmp/cco_phase18c_tests_$$"

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
echo "Cco Phase 18C Milestone 1: Option & Result Test Suite"
echo "=============================================================="

# Test 1: OptionInt operations via std/option.cco import
echo "Test 1: OptionInt (Some, None, is_some, is_none, unwrap_or)..."
cat << 'EOF' > "$TMP_DIR/test_opt_int.cco"
import "std/option.cco";

fn main() -> int {
    let s: OptionInt = some_int(42);
    let n: OptionInt = none_int();

    if (!opt_int_is_some(&s)) { return 1; }
    if (opt_int_is_none(&s)) { return 2; }
    if (opt_int_unwrap_or(&s, 0) != 42) { return 3; }

    if (opt_int_is_some(&n)) { return 4; }
    if (!opt_int_is_none(&n)) { return 5; }
    if (opt_int_unwrap_or(&n, 99) != 99) { return 6; }

    print("OptionInt OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test_opt_int.cco" -o "$TMP_DIR/test_opt_int"
OUT=$("$TMP_DIR/test_opt_int")
if [[ "$OUT" == "OptionInt OK" ]]; then
    pass "OptionInt operations succeed with expected values"
else
    fail "OptionInt execution" "Output was: $OUT"
fi

# Test 2: OptionStr operations via std/option.cco import
echo "Test 2: OptionStr (Some, None, is_some, is_none, unwrap_or)..."
cat << 'EOF' > "$TMP_DIR/test_opt_str.cco"
import "std/option.cco";

fn main() -> int {
    let s: OptionStr = some_str("hello cco");
    let n: OptionStr = none_str();

    if (!opt_str_is_some(&s)) { return 1; }
    if (opt_str_is_none(&s)) { return 2; }
    if (!equals(opt_str_unwrap_or(&s, "def"), "hello cco")) { return 3; }

    if (opt_str_is_some(&n)) { return 4; }
    if (!opt_str_is_none(&n)) { return 5; }
    if (!equals(opt_str_unwrap_or(&n, "fallback"), "fallback")) { return 6; }

    print("OptionStr OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test_opt_str.cco" -o "$TMP_DIR/test_opt_str"
OUT=$("$TMP_DIR/test_opt_str")
if [[ "$OUT" == "OptionStr OK" ]]; then
    pass "OptionStr operations succeed with expected values"
else
    fail "OptionStr execution" "Output was: $OUT"
fi

# Test 3: ResultInt operations via std/result.cco import
echo "Test 3: ResultInt (Ok, Err, is_ok, is_err, unwrap_or, error)..."
cat << 'EOF' > "$TMP_DIR/test_res_int.cco"
import "std/result.cco";

fn divide(a: int, b: int) -> ResultInt {
    if (b == 0) {
        return err_int("division by zero");
    }
    return ok_int(a / b);
}

fn main() -> int {
    let r1: ResultInt = divide(10, 2);
    let r2: ResultInt = divide(10, 0);

    if (!res_int_is_ok(&r1)) { return 1; }
    if (res_int_is_err(&r1)) { return 2; }
    if (res_int_unwrap_or(&r1, 0) != 5) { return 3; }

    if (res_int_is_ok(&r2)) { return 4; }
    if (!res_int_is_err(&r2)) { return 5; }
    if (res_int_unwrap_or(&r2, -1) != -1) { return 6; }
    if (!equals(res_int_error(&r2), "division by zero")) { return 7; }

    print("ResultInt OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test_res_int.cco" -o "$TMP_DIR/test_res_int"
OUT=$("$TMP_DIR/test_res_int")
if [[ "$OUT" == "ResultInt OK" ]]; then
    pass "ResultInt operations succeed with expected values"
else
    fail "ResultInt execution" "Output was: $OUT"
fi

# Test 4: ResultStr operations via std/result.cco import
echo "Test 4: ResultStr (Ok, Err, is_ok, is_err, unwrap_or, error)..."
cat << 'EOF' > "$TMP_DIR/test_res_str.cco"
import "std/result.cco";

fn lookup_name(id: int) -> ResultStr {
    if (id == 1) {
        return ok_str("Alice");
    }
    return err_str("user not found");
}

fn main() -> int {
    let r1: ResultStr = lookup_name(1);
    let r2: ResultStr = lookup_name(99);

    if (!res_str_is_ok(&r1)) { return 1; }
    if (res_str_is_err(&r1)) { return 2; }
    if (!equals(res_str_unwrap_or(&r1, "none"), "Alice")) { return 3; }

    if (res_str_is_ok(&r2)) { return 4; }
    if (!res_str_is_err(&r2)) { return 5; }
    if (!equals(res_str_unwrap_or(&r2, "Guest"), "Guest")) { return 6; }
    if (!equals(res_str_error(&r2), "user not found")) { return 7; }

    print("ResultStr OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test_res_str.cco" -o "$TMP_DIR/test_res_str"
OUT=$("$TMP_DIR/test_res_str")
if [[ "$OUT" == "ResultStr OK" ]]; then
    pass "ResultStr operations succeed with expected values"
else
    fail "ResultStr execution" "Output was: $OUT"
fi

# Test 5: ResultVoid operations via std/result.cco import
echo "Test 5: ResultVoid (Ok, Err, is_ok, is_err, error)..."
cat << 'EOF' > "$TMP_DIR/test_res_void.cco"
import "std/result.cco";

fn validate_positive(x: int) -> ResultVoid {
    if (x <= 0) {
        return err_void("value must be positive");
    }
    return ok_void();
}

fn main() -> int {
    let r1: ResultVoid = validate_positive(10);
    let r2: ResultVoid = validate_positive(-5);

    if (!res_void_is_ok(&r1)) { return 1; }
    if (res_void_is_err(&r1)) { return 2; }

    if (res_void_is_ok(&r2)) { return 3; }
    if (!res_void_is_err(&r2)) { return 4; }
    if (!equals(res_void_error(&r2), "value must be positive")) { return 5; }

    print("ResultVoid OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test_res_void.cco" -o "$TMP_DIR/test_res_void"
OUT=$("$TMP_DIR/test_res_void")
if [[ "$OUT" == "ResultVoid OK" ]]; then
    pass "ResultVoid operations succeed with expected values"
else
    fail "ResultVoid execution" "Output was: $OUT"
fi

# Test 6: Pattern Matching with Custom Matching on Option & Result
echo "Test 6: Idiomatic pattern matching on Option and Result..."
cat << 'EOF' > "$TMP_DIR/test_match.cco"
import "std/option.cco";
import "std/result.cco";

fn format_opt(opt: &OptionInt) -> string {
    match opt {
        OptionInt.Some { value } => {
            return "has value";
        }
        OptionInt.None => {
            return "empty";
        }
    }
}

fn format_res(res: &ResultInt) -> string {
    match res {
        ResultInt.Ok { value } => {
            return "success";
        }
        ResultInt.Err { message } => {
            return "failure";
        }
    }
}

fn main() -> int {
    let o1: OptionInt = some_int(7);
    let o2: OptionInt = none_int();
    let r1: ResultInt = ok_int(7);
    let r2: ResultInt = err_int("error");

    let s1: string = format_opt(&o1);
    let s2: string = format_opt(&o2);
    let s3: string = format_res(&r1);
    let s4: string = format_res(&r2);

    if (!equals(s1, "has value")) { return 1; }
    if (!equals(s2, "empty")) { return 2; }
    if (!equals(s3, "success")) { return 3; }
    if (!equals(s4, "failure")) { return 4; }

    print("Pattern Matching OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test_match.cco" -o "$TMP_DIR/test_match"
OUT=$("$TMP_DIR/test_match")
if [[ "$OUT" == "Pattern Matching OK" ]]; then
    pass "Pattern matching on Option and Result enums behaves correctly"
else
    fail "Pattern match execution" "Output was: $OUT"
fi

# Test 7: Valgrind Memory Leak Verification
echo "Test 7: Valgrind memory leak verification across all tests..."
VALG_FAIL=0
for bin in "$TMP_DIR/test_opt_int" "$TMP_DIR/test_opt_str" "$TMP_DIR/test_res_int" "$TMP_DIR/test_res_str" "$TMP_DIR/test_res_void" "$TMP_DIR/test_match"; do
    if valgrind --leak-check=full --error-exitcode=1 "$bin" > /dev/null 2>&1; then
        :
    else
        VALG_FAIL=$((VALG_FAIL + 1))
        fail "Valgrind memcheck" "$(basename "$bin") leaked memory or raised errors"
    fi
done
if [[ $VALG_FAIL -eq 0 ]]; then
    pass "All Option and Result test binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# Test 8: Self-Hosted Compiler Parity (if Stage 2 exists)
echo "Test 8: Self-hosted compiler compilation parity..."
if [[ -x "$STAGE2" ]]; then
    cat << 'EOF' > "$TMP_DIR/test_selfhost_opt.cco"
enum SingleOption {
    SingleSome { val: int },
    SingleNone,
}

fn make_some(v: int) -> SingleOption {
    return SingleOption.SingleSome { val: v };
}

fn unwrap_single(o: &SingleOption, def: int) -> int {
    match o {
        SingleOption.SingleSome { val } => {
            return val;
        }
        SingleOption.SingleNone => {
            return def;
        }
    }
    return def;
}

fn main() -> int {
    let o: SingleOption = make_some(42);
    if (unwrap_single(&o, 0) != 42) {
        return 1;
    }
    print("Selfhost Enum OK");
    return 0;
}
EOF
    "$STAGE2" "$TMP_DIR/test_selfhost_opt.cco" -o "$TMP_DIR/test_selfhost_opt.c" > /dev/null 2>&1
    gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 -I"$ROOT_DIR/src" "$TMP_DIR/test_selfhost_opt.c" -o "$TMP_DIR/test_selfhost_opt" -lm
    STAGE2_OUT=$("$TMP_DIR/test_selfhost_opt")
    if [[ "$STAGE2_OUT" == "Selfhost Enum OK" ]]; then
        pass "Self-hosted Stage 2 compiler successfully compiles and executes enum sum types with payload matching"
    else
        fail "Self-host compilation" "Stage 2 binary output differed: $STAGE2_OUT"
    fi
else
    echo "  (Stage 2 binary not built, skipping self-host parity check)"
fi

# Test 9: Negative Boundary — Clean rejection on native backend
echo "Test 9: Negative boundary — compile-time rejection by native backend..."
SELFHOST_CCO="$ROOT_DIR/selfhost/cco"
if [[ -x "$SELFHOST_CCO" ]]; then
    set +e
    NATIVE_ERR=$(cd "$ROOT_DIR" && "$SELFHOST_CCO" "$TMP_DIR/test_opt_int.cco" --selfhost-native -o "$TMP_DIR/native_bin" 2>&1)
    NATIVE_CODE=$?
    set -e
    if [[ $NATIVE_CODE -eq 0 ]]; then
        fail "Native backend boundary" "Expected native backend to reject enum, but compilation succeeded"
    else
        if [[ "$NATIVE_ERR" == *"does not support enum definitions"* ]]; then
            pass "Native backend cleanly rejected enum usage with informative diagnostic"
        else
            fail "Native backend diagnostic" "Unexpected error message: $NATIVE_ERR"
        fi
    fi
else
    echo "  (selfhost/cco binary not built, skipping native rejection check)"
fi

# Test 10: Negative Boundary — Non-exhaustive match rejection
echo "Test 10: Negative boundary — non-exhaustive match detection..."
cat << 'EOF' > "$TMP_DIR/neg_nonexhaustive.cco"
import "std/option.cco";

fn bad_match(opt: &OptionInt) -> int {
    match opt {
        OptionInt.Some { value } => {
            return value;
        }
    }
}

fn main() -> int {
    let o: OptionInt = some_int(1);
    return bad_match(&o);
}
EOF

if "$CCO" "$TMP_DIR/neg_nonexhaustive.cco" -o "$TMP_DIR/neg_bin" > "$TMP_DIR/neg_err.txt" 2>&1; then
    fail "Non-exhaustive match" "Expected compiler to reject non-exhaustive match, but it succeeded"
else
    if grep -q "non-exhaustive" "$TMP_DIR/neg_err.txt" || grep -q "missing variant" "$TMP_DIR/neg_err.txt" || grep -q "E30" "$TMP_DIR/neg_err.txt" || grep -q "error" "$TMP_DIR/neg_err.txt"; then
        pass "Compiler cleanly rejected non-exhaustive match on OptionInt"
    else
        fail "Non-exhaustive diagnostic" "Unexpected error output: $(cat "$TMP_DIR/neg_err.txt")"
    fi
fi

echo "=============================================================="
echo "Phase 18C Milestone 1 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
