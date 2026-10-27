#!/usr/bin/env bash
# ==============================================================
# Cco Compiler — Milestone 2: Loop Ownership Safety Test Suite
# Tests rejection of unsafe moves of ancestor-scope variables
# inside loops (while, for, foreach, nested loops) and confirms
# valid iteration-local moves compile and execute cleanly.
# ==============================================================

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

CCO="./cco"
if [ ! -f "$CCO" ]; then
    echo "Building cco compiler..."
    make cco
fi

echo "=============================================================="
echo "Cco Compiler: Ownership Analysis Loop Safety Test Suite"
echo "=============================================================="

TMP_DIR="build/test_own_loops"
mkdir -p "$TMP_DIR"
TOTAL=0
PASSED=0

run_test() {
    local name="$1"
    TOTAL=$((TOTAL + 1))
    echo -n "[$TOTAL] $name ... "
}

pass_test() {
    echo "PASSED"
    PASSED=$((PASSED + 1))
}

fail_test() {
    local reason="$1"
    echo "FAILED ($reason)"
    exit 1
}

# -------------------------------------------------------------
# Test 1: Move an outer-scope owned variable inside a while loop
# -------------------------------------------------------------
run_test "Move an outer-scope owned variable inside a while loop"
cat << 'EOF' > "$TMP_DIR/test1_while_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let i: int = 0;
    while (i < 3) {
        consume(b);
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test1_while_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving outer variable in while loop, but compilation succeeded"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop' error, got: $err_out"
fi
if ! echo "$err_out" | grep -q "cannot move out of ancestor scope in loop"; then
    fail_test "Expected ancestor scope caret label, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 2: Move an outer-scope owned variable inside a for loop
# -------------------------------------------------------------
run_test "Move an outer-scope owned variable inside a for loop"
cat << 'EOF' > "$TMP_DIR/test2_for_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    for (let i: int = 0; i < 3; i = i + 1) {
        consume(b);
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test2_for_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving outer variable in for loop, but compilation succeeded"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 3: Move an outer-scope owned variable inside a for-each loop
# -------------------------------------------------------------
run_test "Move an outer-scope owned variable inside a for-each loop"
cat << 'EOF' > "$TMP_DIR/test3_foreach_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let nums: int[] = alloc(int, 3);
    nums[0] = 1; nums[1] = 2; nums[2] = 3;
    for n in nums {
        consume(b);
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test3_foreach_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving outer variable in foreach loop, but compilation succeeded"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 4: Use the moved variable later in the same iteration
# -------------------------------------------------------------
run_test "Use the moved variable later in the same iteration"
cat << 'EOF' > "$TMP_DIR/test4_use_same_iter.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let i: int = 0;
    while (i < 3) {
        consume(b);
        print(b.val);
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test4_use_same_iter.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure on invalid move in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 5: Use the moved variable after the loop
# -------------------------------------------------------------
run_test "Use the moved variable after the loop"
cat << 'EOF' > "$TMP_DIR/test5_use_after_loop.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let i: int = 0;
    while (i < 3) {
        consume(b);
        i = i + 1;
    }
    print(b.val);
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test5_use_after_loop.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure on invalid move in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 6: Attempt another invalid move on a subsequent iteration
# -------------------------------------------------------------
run_test "Attempt another invalid move on a subsequent iteration"
cat << 'EOF' > "$TMP_DIR/test6_subsequent_iter.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let i: int = 0;
    while (i < 2) {
        consume(b);
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test6_subsequent_iter.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure on invalid move in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 7: Move a variable from an enclosing block scope inside a nested loop
# -------------------------------------------------------------
run_test "Move a variable from an enclosing block scope inside a nested loop"
cat << 'EOF' > "$TMP_DIR/test7_block_loop.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    {
        let b: Box = Box { val: 42 };
        let i: int = 0;
        while (i < 3) {
            consume(b);
            i = i + 1;
        }
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test7_block_loop.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving enclosing block variable in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 8: Attempt an invalid move from an inner loop affecting outer loop variable
# -------------------------------------------------------------
run_test "Attempt an invalid move from an inner loop affecting outer loop variable"
cat << 'EOF' > "$TMP_DIR/test8_inner_loop_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let i: int = 0;
    while (i < 2) {
        let b: Box = Box { val: i };
        let j: int = 0;
        while (j < 2) {
            consume(b);
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test8_inner_loop_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving outer-loop variable inside inner loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 9: Nested loops involving a function-scope owned variable
# -------------------------------------------------------------
run_test "Nested loops involving a function-scope owned variable"
cat << 'EOF' > "$TMP_DIR/test9_nested_fn_scope.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let i: int = 0;
    while (i < 2) {
        let j: int = 0;
        while (j < 2) {
            consume(b);
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test9_nested_fn_scope.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving function-scope variable in deeply nested loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 10: Move outer-scope variable via let-assignment inside a loop
# -------------------------------------------------------------
run_test "Move outer-scope variable via let-assignment inside a loop"
cat << 'EOF' > "$TMP_DIR/test10_let_assign_loop.cco"
class Box { val: int; }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let i: int = 0;
    while (i < 3) {
        let x: Box = b;
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test10_let_assign_loop.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving outer variable via let in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 11: Move outer-scope variable via reassignment inside a loop
# -------------------------------------------------------------
run_test "Move outer-scope variable via reassignment inside a loop"
cat << 'EOF' > "$TMP_DIR/test11_reassign_loop.cco"
class Box { val: int; }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let target: Box = Box { val: 0 };
    let i: int = 0;
    while (i < 3) {
        target = b;
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test11_reassign_loop.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving outer variable via reassignment in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 12: Move outer-scope variable via array index assignment inside a loop
# -------------------------------------------------------------
run_test "Move outer-scope variable via array index assignment inside a loop"
cat << 'EOF' > "$TMP_DIR/test12_index_assign_loop.cco"
class Box { val: int; }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let arr: Box[] = alloc(Box, 3);
    let i: int = 0;
    while (i < 3) {
        arr[i] = b;
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test12_index_assign_loop.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving outer variable into array in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 13: Move outer-scope variable via push() inside a loop
# -------------------------------------------------------------
run_test "Move outer-scope variable via push() inside a loop"
cat << 'EOF' > "$TMP_DIR/test13_push_loop.cco"
class Box { val: int; }
fn main() -> int {
    let b: Box = Box { val: 42 };
    let list: Box[] = list_new(Box);
    let i: int = 0;
    while (i < 3) {
        list = push(list, b);
        i = i + 1;
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test13_push_loop.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure pushing outer variable in loop, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 14: Move a variable declared in for-loop init inside body
# -------------------------------------------------------------
run_test "Move a variable declared in for-loop init inside body"
cat << 'EOF' > "$TMP_DIR/test14_for_init_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    for (let b: Box = Box { val: 42 }; true; false) {
        consume(b);
    }
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test14_for_init_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving for-init variable in body, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "cannot move 'b' inside a loop"; then
    fail_test "Expected 'cannot move b inside a loop', got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 15: Positive — Move an iteration-local variable in a while loop
# -------------------------------------------------------------
run_test "Positive: Move an iteration-local variable in a while loop"
cat << 'EOF' > "$TMP_DIR/test15_while_local_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let i: int = 0;
    while (i < 3) {
        let b: Box = Box { val: i };
        consume(b);
        i = i + 1;
    }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test15_while_local_move.cco" -o "$TMP_DIR/test15_while_local_move"; then
    fail_test "Failed compiling valid iteration-local move in while loop"
fi
if ! "$TMP_DIR/test15_while_local_move"; then
    fail_test "Failed executing valid iteration-local move in while loop"
fi
pass_test

# -------------------------------------------------------------
# Test 16: Positive — Move an owned value into a consumer outside a loop
# -------------------------------------------------------------
run_test "Positive: Move an owned value outside a loop"
cat << 'EOF' > "$TMP_DIR/test16_outside_loop_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let b: Box = Box { val: 42 };
    consume(b);
    let i: int = 0;
    while (i < 3) {
        i = i + 1;
    }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test16_outside_loop_move.cco" -o "$TMP_DIR/test16_outside_loop_move"; then
    fail_test "Failed compiling valid move outside loop"
fi
if ! "$TMP_DIR/test16_outside_loop_move"; then
    fail_test "Failed executing valid move outside loop"
fi
pass_test

# -------------------------------------------------------------
# Test 17: Positive — Use outer-scope owned variable in a loop without moving
# -------------------------------------------------------------
run_test "Positive: Borrow outer-scope variable in a loop without moving"
cat << 'EOF' > "$TMP_DIR/test17_borrow_in_loop.cco"
class Box { val: int; }
fn inspect(b: &Box) -> int {
    return b.val;
}
fn main() -> int {
    let b: Box = Box { val: 100 };
    let sum: int = 0;
    let i: int = 0;
    while (i < 3) {
        sum = sum + inspect(&b);
        i = i + 1;
    }
    if (sum != 300) { return 1; }
    if (b.val != 100) { return 2; }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test17_borrow_in_loop.cco" -o "$TMP_DIR/test17_borrow_in_loop"; then
    fail_test "Failed compiling valid borrow in loop"
fi
if ! "$TMP_DIR/test17_borrow_in_loop"; then
    fail_test "Failed executing valid borrow in loop"
fi
pass_test

# -------------------------------------------------------------
# Test 18: Positive — Move distinct loop-local value while keeping outer valid
# -------------------------------------------------------------
run_test "Positive: Move loop-local value while keeping outer valid"
cat << 'EOF' > "$TMP_DIR/test18_local_move_outer_valid.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let outer: Box = Box { val: 50 };
    let i: int = 0;
    while (i < 3) {
        let inner: Box = Box { val: i * 10 };
        consume(inner);
        i = i + 1;
    }
    if (outer.val != 50) { return 1; }
    consume(outer);
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test18_local_move_outer_valid.cco" -o "$TMP_DIR/test18_local_move_outer_valid"; then
    fail_test "Failed compiling local move with valid outer variable"
fi
if ! "$TMP_DIR/test18_local_move_outer_valid"; then
    fail_test "Failed executing local move with valid outer variable"
fi
pass_test

# -------------------------------------------------------------
# Test 19: Positive — Nested loops where no invalid move occurs
# -------------------------------------------------------------
run_test "Positive: Nested loops with iteration-local moves"
cat << 'EOF' > "$TMP_DIR/test19_nested_local_moves.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let i: int = 0;
    while (i < 2) {
        let j: int = 0;
        while (j < 2) {
            let local: Box = Box { val: i * 10 + j };
            consume(local);
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test19_nested_local_moves.cco" -o "$TMP_DIR/test19_nested_local_moves"; then
    fail_test "Failed compiling valid nested loop local moves"
fi
if ! "$TMP_DIR/test19_nested_local_moves"; then
    fail_test "Failed executing valid nested loop local moves"
fi
pass_test

# -------------------------------------------------------------
# Test 20: Positive — Move an iteration-local variable in a counted for loop
# -------------------------------------------------------------
run_test "Positive: Move an iteration-local variable in a counted for loop"
cat << 'EOF' > "$TMP_DIR/test20_for_local_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    for (let i: int = 0; i < 3; i = i + 1) {
        let b: Box = Box { val: i * 100 };
        consume(b);
    }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test20_for_local_move.cco" -o "$TMP_DIR/test20_for_local_move"; then
    fail_test "Failed compiling valid iteration-local move in counted for loop"
fi
if ! "$TMP_DIR/test20_for_local_move"; then
    fail_test "Failed executing valid iteration-local move in counted for loop"
fi
pass_test

# -------------------------------------------------------------
# Test 21: Positive — Move an iteration-local variable in a for-each loop
# -------------------------------------------------------------
run_test "Positive: Move an iteration-local variable in a for-each loop"
cat << 'EOF' > "$TMP_DIR/test21_foreach_local_move.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    let nums: int[] = alloc(int, 3);
    nums[0] = 10; nums[1] = 20; nums[2] = 30;
    for n in nums {
        let b: Box = Box { val: n };
        consume(b);
    }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test21_foreach_local_move.cco" -o "$TMP_DIR/test21_foreach_local_move"; then
    fail_test "Failed compiling valid iteration-local move in foreach loop"
fi
if ! "$TMP_DIR/test21_foreach_local_move"; then
    fail_test "Failed executing valid iteration-local move in foreach loop"
fi
pass_test

# -------------------------------------------------------------
# Test 22: Valgrind memory leak verification on compiled binary
# -------------------------------------------------------------
run_test "Valgrind memory verification on valid loop programs"
if ! valgrind --error-exitcode=1 --leak-check=full "$TMP_DIR/test15_while_local_move" > /dev/null 2>&1; then
    fail_test "Valgrind detected memory corruption in test15"
fi
if ! valgrind --error-exitcode=1 --leak-check=full "$TMP_DIR/test18_local_move_outer_valid" > /dev/null 2>&1; then
    fail_test "Valgrind detected memory corruption in test18"
fi
if ! valgrind --error-exitcode=1 --leak-check=full "$TMP_DIR/test19_nested_local_moves" > /dev/null 2>&1; then
    fail_test "Valgrind detected memory corruption in test19"
fi
pass_test

echo "=============================================================="
echo "Loop Ownership Safety Suite: $PASSED / $TOTAL Passed (100% Success)"
echo "=============================================================="

rm -rf "$TMP_DIR"
exit 0
