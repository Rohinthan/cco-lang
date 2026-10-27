#!/usr/bin/env bash
# ==============================================================================
# tests/test_ownership_conditional_moves.sh
# Cco Compiler Ownership Analysis: Milestone 3 Conditional Move & Drop Soundness
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_cond_moves_XXXXXX)

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
    echo "Compiler binary '$CCO' not found. Building with make cco..."
    (cd "$REPO_ROOT" && make cco)
fi

echo "=============================================================="
echo "Cco Compiler: Ownership Analysis Conditional Move Test Suite"
echo "=============================================================="

# -------------------------------------------------------------
# Test 1: if (cond) { consume(r); } - True branch (Valgrind clean)
# -------------------------------------------------------------
run_test "if-only move: True branch executed (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t01_if_true.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c: bool) -> void {
    let r = Resource { id: 101 };
    if (c) {
        consume(r);
    }
}
fn main() -> int {
    test(true);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t01_if_true.cco" -o "$TMP_DIR/t01_if_true_bin" 2>"$TMP_DIR/t01.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t01.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t01_if_true_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 2: if (cond) { consume(r); } - False branch (Valgrind clean)
# -------------------------------------------------------------
run_test "if-only move: False branch executed (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t02_if_false.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c: bool) -> void {
    let r = Resource { id: 102 };
    if (c) {
        consume(r);
    }
}
fn main() -> int {
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t02_if_false.cco" -o "$TMP_DIR/t02_if_false_bin" 2>"$TMP_DIR/t02.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t02.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t02_if_false_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 3: if (cond) { let b = r; } - True branch (Valgrind clean)
# -------------------------------------------------------------
run_test "let-move inside if: True branch (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t03_let_true.cco"
class Resource { id: int; }
fn test(c: bool) -> void {
    let r = Resource { id: 103 };
    if (c) {
        let b = r;
    }
}
fn main() -> int {
    test(true);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t03_let_true.cco" -o "$TMP_DIR/t03_let_true_bin" 2>"$TMP_DIR/t03.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t03.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t03_let_true_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 4: if (cond) { let b = r; } - False branch (Valgrind clean)
# -------------------------------------------------------------
run_test "let-move inside if: False branch (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t04_let_false.cco"
class Resource { id: int; }
fn test(c: bool) -> void {
    let r = Resource { id: 104 };
    if (c) {
        let b = r;
    }
}
fn main() -> int {
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t04_let_false.cco" -o "$TMP_DIR/t04_let_false_bin" 2>"$TMP_DIR/t04.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t04.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t04_let_false_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 5: if/else with move in 'then' only (both branches clean)
# -------------------------------------------------------------
run_test "if/else move in then only: Both branches clean"
cat << 'EOF' > "$TMP_DIR/t05_then_only.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c: bool) -> void {
    let r = Resource { id: 105 };
    if (c) {
        consume(r);
    } else {
        let dummy = 0;
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t05_then_only.cco" -o "$TMP_DIR/t05_then_only_bin" 2>"$TMP_DIR/t05.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t05.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t05_then_only_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 6: if/else with move in 'else' only (both branches clean)
# -------------------------------------------------------------
run_test "if/else move in else only: Both branches clean"
cat << 'EOF' > "$TMP_DIR/t06_else_only.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c: bool) -> void {
    let r = Resource { id: 106 };
    if (c) {
        let dummy = 1;
    } else {
        consume(r);
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t06_else_only.cco" -o "$TMP_DIR/t06_else_only_bin" 2>"$TMP_DIR/t06.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t06.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t06_else_only_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 7: if/else with move in both branches (both branches clean)
# -------------------------------------------------------------
run_test "if/else move in both branches: Both branches clean"
cat << 'EOF' > "$TMP_DIR/t07_both_branches.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c: bool) -> void {
    let r = Resource { id: 107 };
    if (c) {
        consume(r);
    } else {
        consume(r);
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t07_both_branches.cco" -o "$TMP_DIR/t07_both_branches_bin" 2>"$TMP_DIR/t07.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t07.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t07_both_branches_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 8: Reassignment after conditional move (both branches clean)
# -------------------------------------------------------------
run_test "Reassignment after conditional move (both branches clean)"
cat << 'EOF' > "$TMP_DIR/t08_reassign_after_cond.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c: bool) -> void {
    let r = Resource { id: 108 };
    if (c) {
        consume(r);
    }
    r = Resource { id: 999 };
    print(r.id);
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t08_reassign_after_cond.cco" -o "$TMP_DIR/t08_reassign_after_cond_bin" 2>"$TMP_DIR/t08.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t08.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t08_reassign_after_cond_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 9: Nested if statements (all 4 combinations clean)
# -------------------------------------------------------------
run_test "Nested conditionals (4 execution paths clean)"
cat << 'EOF' > "$TMP_DIR/t09_nested_if.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c1: bool, c2: bool) -> void {
    let r = Resource { id: 109 };
    if (c1) {
        if (c2) {
            consume(r);
        }
    }
}
fn main() -> int {
    test(true, true);
    test(true, false);
    test(false, true);
    test(false, false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t09_nested_if.cco" -o "$TMP_DIR/t09_nested_if_bin" 2>"$TMP_DIR/t09.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t09.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t09_nested_if_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 10: Early return inside conditional move branch
# -------------------------------------------------------------
run_test "Early return in conditional move branch (both paths clean)"
cat << 'EOF' > "$TMP_DIR/t10_early_ret.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(c: bool) -> void {
    let r = Resource { id: 110 };
    if (c) {
        consume(r);
        return;
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t10_early_ret.cco" -o "$TMP_DIR/t10_early_ret_bin" 2>"$TMP_DIR/t10.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t10.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t10_early_ret_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 11: Match construct with single-arm move (both paths clean)
# -------------------------------------------------------------
run_test "Match statement single-arm move (both variants clean)"
cat << 'EOF' > "$TMP_DIR/t11_match_single.cco"
enum Mode {
    Fast,
    Slow
}
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(m: Mode) -> void {
    let r = Resource { id: 111 };
    match m {
        Mode.Fast => { consume(r); }
        Mode.Slow => { let d = 0; }
    }
}
fn main() -> int {
    test(Mode.Fast);
    test(Mode.Slow);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t11_match_single.cco" -o "$TMP_DIR/t11_match_single_bin" 2>"$TMP_DIR/t11.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t11.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t11_match_single_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 12: Match construct with all arms moving (all variants clean)
# -------------------------------------------------------------
run_test "Match statement all arms moving (all variants clean)"
cat << 'EOF' > "$TMP_DIR/t12_match_all.cco"
enum Priority {
    High,
    Low
}
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(p: Priority) -> void {
    let r = Resource { id: 112 };
    match p {
        Priority.High => { consume(r); }
        Priority.Low  => { consume(r); }
    }
}
fn main() -> int {
    test(Priority.High);
    test(Priority.Low);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t12_match_all.cco" -o "$TMP_DIR/t12_match_all_bin" 2>"$TMP_DIR/t12.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t12.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t12_match_all_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 13: Loop-local conditional move (clean across multiple iterations)
# -------------------------------------------------------------
run_test "Loop-local conditional move across iterations (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t13_loop_local.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void { print(r.id); }
fn test(flag: bool) -> void {
    let i = 0;
    while (i < 4) {
        let r = Resource { id: i };
        if (flag) {
            consume(r);
        }
        i = i + 1;
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t13_loop_local.cco" -o "$TMP_DIR/t13_loop_local_bin" 2>"$TMP_DIR/t13.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t13.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t13_loop_local_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 14: Heap array conditional move (both branches clean)
# -------------------------------------------------------------
run_test "Heap array conditional move (both branches clean)"
cat << 'EOF' > "$TMP_DIR/t14_arr_cond.cco"
class Item { v: int; }
fn consume_arr(a: Item[]) -> void {
    print(a[0].v);
}
fn test(c: bool) -> void {
    let arr: Item[] = alloc(Item, 1);
    arr[0] = Item { v: 114 };
    if (c) {
        consume_arr(arr);
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t14_arr_cond.cco" -o "$TMP_DIR/t14_arr_cond_bin" 2>"$TMP_DIR/t14.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t14.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t14_arr_cond_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 15: Map conditional move (both branches clean)
# -------------------------------------------------------------
run_test "Map conditional move (both branches clean)"
cat << 'EOF' > "$TMP_DIR/t15_map_cond.cco"
fn consume_map(m: map[string]int) -> void {
    print(get(m, "key"));
}
fn test(c: bool) -> void {
    let m: map[string]int = map_new(string, int);
    m = put(m, "key", 115);
    if (c) {
        consume_map(m);
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! CC="gcc -O0" "$CCO" "$TMP_DIR/t15_map_cond.cco" -o "$TMP_DIR/t15_map_cond_bin" 2>"$TMP_DIR/t15.err"; then
    fail_test "Compilation failed: $(cat "$TMP_DIR/t15.err")"
else
    val_out=$(valgrind --error-exitcode=99 --leak-check=full "$TMP_DIR/t15_map_cond_bin" 2>&1)
    vg_status=$?
    if [ $vg_status -ne 0 ] || echo "$val_out" | grep -q "Invalid free"; then
        fail_test "Valgrind detected memory error: $val_out"
    else
        pass_test
    fi
fi

# -------------------------------------------------------------
# Test 16: Negative: Use after if-only move rejected
# -------------------------------------------------------------
run_test "Negative: Use after if-only move rejected with diagnostic"
cat << 'EOF' > "$TMP_DIR/t16_neg_if.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void {}
fn main() -> int {
    let r = Resource { id: 1 };
    if (true) {
        consume(r);
    }
    print(r.id);
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t16_neg_if.cco" -o "$TMP_DIR/t16_bin" 2>&1)
exit_code=$?
set -e
if [ $exit_code -eq 0 ] || [ -f "$TMP_DIR/t16_bin" ]; then
    fail_test "Expected compilation failure, but succeeded"
elif ! echo "$err_out" | grep -q "'r' may be moved depending on which branch executes"; then
    fail_test "Missing expected diagnostic, got: $err_out"
elif ! echo "$err_out" | grep -q "moved here, in one branch:"; then
    fail_test "Missing expected note, got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 17: Negative: Use after if/else single branch move rejected
# -------------------------------------------------------------
run_test "Negative: Use after if/else single branch move rejected"
cat << 'EOF' > "$TMP_DIR/t17_neg_ifelse.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void {}
fn main() -> int {
    let r = Resource { id: 1 };
    if (false) {
        consume(r);
    } else {
        let x = 0;
    }
    print(r.id);
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t17_neg_ifelse.cco" -o "$TMP_DIR/t17_bin" 2>&1)
exit_code=$?
set -e
if [ $exit_code -eq 0 ] || [ -f "$TMP_DIR/t17_bin" ]; then
    fail_test "Expected compilation failure, but succeeded"
elif ! echo "$err_out" | grep -q "'r' may be moved depending on which branch executes"; then
    fail_test "Missing expected diagnostic, got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 18: Negative: Use after else-only move rejected
# -------------------------------------------------------------
run_test "Negative: Use after else-only move rejected"
cat << 'EOF' > "$TMP_DIR/t18_neg_else.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void {}
fn main() -> int {
    let r = Resource { id: 1 };
    if (true) {
        let x = 0;
    } else {
        consume(r);
    }
    print(r.id);
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t18_neg_else.cco" -o "$TMP_DIR/t18_bin" 2>&1)
exit_code=$?
set -e
if [ $exit_code -eq 0 ] || [ -f "$TMP_DIR/t18_bin" ]; then
    fail_test "Expected compilation failure, but succeeded"
elif ! echo "$err_out" | grep -q "'r' may be moved depending on which branch executes"; then
    fail_test "Missing expected diagnostic, got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 19: Negative: Use after match single-arm move rejected
# -------------------------------------------------------------
run_test "Negative: Use after match single-arm move rejected"
cat << 'EOF' > "$TMP_DIR/t19_neg_match.cco"
enum Mode {
    Fast,
    Slow
}
class Resource { id: int; }
fn consume(r: Resource) -> void {}
fn main() -> int {
    let r = Resource { id: 1 };
    let m = Mode.Fast;
    match m {
        Mode.Fast => { consume(r); }
        Mode.Slow => { let d = 0; }
    }
    print(r.id);
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t19_neg_match.cco" -o "$TMP_DIR/t19_bin" 2>&1)
exit_code=$?
set -e
if [ $exit_code -eq 0 ] || [ -f "$TMP_DIR/t19_bin" ]; then
    fail_test "Expected compilation failure, but succeeded"
elif ! echo "$err_out" | grep -q "'r' may be moved depending on which branch executes"; then
    fail_test "Missing expected diagnostic, got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 20: Negative: Second move after conditional move rejected
# -------------------------------------------------------------
run_test "Negative: Second move after conditional move rejected"
cat << 'EOF' > "$TMP_DIR/t20_neg_second_move.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void {}
fn main() -> int {
    let r = Resource { id: 1 };
    if (true) {
        consume(r);
    }
    consume(r);
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t20_neg_second_move.cco" -o "$TMP_DIR/t20_bin" 2>&1)
exit_code=$?
set -e
if [ $exit_code -eq 0 ] || [ -f "$TMP_DIR/t20_bin" ]; then
    fail_test "Expected compilation failure, but succeeded"
elif ! echo "$err_out" | grep -q "'r' may be moved depending on which branch executes"; then
    fail_test "Missing expected diagnostic, got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 21: Negative: Nested conditional use rejected
# -------------------------------------------------------------
run_test "Negative: Nested conditional move use rejected"
cat << 'EOF' > "$TMP_DIR/t21_neg_nested.cco"
class Resource { id: int; }
fn consume(r: Resource) -> void {}
fn main() -> int {
    let r = Resource { id: 1 };
    if (true) {
        if (false) {
            consume(r);
        }
    }
    print(r.id);
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t21_neg_nested.cco" -o "$TMP_DIR/t21_bin" 2>&1)
exit_code=$?
set -e
if [ $exit_code -eq 0 ] || [ -f "$TMP_DIR/t21_bin" ]; then
    fail_test "Expected compilation failure, but succeeded"
elif ! echo "$err_out" | grep -q "'r' may be moved depending on which branch executes"; then
    fail_test "Missing expected diagnostic, got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 22: Negative: Use after all arms moving reports use-after-move
# -------------------------------------------------------------
run_test "Negative: Use after all arms moving reports unconditional use-after-move"
cat << 'EOF' > "$TMP_DIR/t22_neg_all_moved.cco"
enum Mode {
    Fast,
    Slow
}
class Resource { id: int; }
fn consume(r: Resource) -> void {}
fn main() -> int {
    let r = Resource { id: 1 };
    let m = Mode.Fast;
    match m {
        Mode.Fast => { consume(r); }
        Mode.Slow => { consume(r); }
    }
    print(r.id);
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t22_neg_all_moved.cco" -o "$TMP_DIR/t22_bin" 2>&1)
exit_code=$?
set -e
if [ $exit_code -eq 0 ] || [ -f "$TMP_DIR/t22_bin" ]; then
    fail_test "Expected compilation failure, but succeeded"
elif ! echo "$err_out" | grep -q "use of moved value 'r'"; then
    fail_test "Expected 'use of moved value r', got: $err_out"
else
    pass_test
fi

echo "=============================================================="
echo "Conditional Move Suite: $passed_tests / $total_tests Passed ($(( passed_tests * 100 / total_tests ))% Success)"
echo "=============================================================="

if [ "$failed_tests" -gt 0 ]; then
    exit 1
fi
exit 0
