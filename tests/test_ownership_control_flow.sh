#!/usr/bin/env bash
# ==============================================================================
# tests/test_ownership_control_flow.sh
# Cco Compiler Ownership Analysis: Milestone 4 Control-Flow Soundness & Completeness
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_ctrl_flow_XXXXXX)

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
echo "Cco Compiler: Ownership Control-Flow Soundness Test Suite"
echo "=============================================================="

# -------------------------------------------------------------
# Test 1: Print-statement move drop-flag: True branch (Valgrind clean)
# -------------------------------------------------------------
run_test "print move drop-flag: True branch (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t01_print_true.cco"
class Resource { id: int; }
fn consume(r: Resource) -> int {
    let id = r.id;
    return id;
}
fn test(c: bool) -> void {
    let r = Resource { id: 101 };
    if (c) {
        print(consume(r));
    }
}
fn main() -> int {
    test(true);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t01_print_true.cco" -o "$TMP_DIR/t01_print_true.c" 2>"$TMP_DIR/t01.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t01.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t01_print_true.c" -o "$TMP_DIR/t01_print_true.bin" 2>"$TMP_DIR/t01_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t01_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t01_print_true.bin" >/dev/null 2>"$TMP_DIR/t01_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t01_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 2: Print-statement move drop-flag: False branch (Valgrind clean)
# -------------------------------------------------------------
run_test "print move drop-flag: False branch (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t02_print_false.cco"
class Resource { id: int; }
fn consume(r: Resource) -> int {
    let id = r.id;
    return id;
}
fn test(c: bool) -> void {
    let r = Resource { id: 102 };
    if (c) {
        print(consume(r));
    }
}
fn main() -> int {
    test(false);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t02_print_false.cco" -o "$TMP_DIR/t02_print_false.c" 2>"$TMP_DIR/t02.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t02.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t02_print_false.c" -o "$TMP_DIR/t02_print_false.bin" 2>"$TMP_DIR/t02_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t02_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t02_print_false.bin" >/dev/null 2>"$TMP_DIR/t02_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t02_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 3: Print multi-value statement with move (Valgrind clean)
# -------------------------------------------------------------
run_test "print multi-value statement with move (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t03_print_multi.cco"
class Resource { id: int; }
fn consume(r: Resource) -> int {
    return r.id;
}
fn test(c: bool) -> void {
    let r = Resource { id: 103 };
    if (c) {
        print(42, consume(r));
    }
}
fn main() -> int {
    test(true);
    test(false);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t03_print_multi.cco" -o "$TMP_DIR/t03_print_multi.c" 2>"$TMP_DIR/t03.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t03.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t03_print_multi.c" -o "$TMP_DIR/t03_print_multi.bin" 2>"$TMP_DIR/t03_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t03_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t03_print_multi.bin" >/dev/null 2>"$TMP_DIR/t03_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t03_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 4: Member assignment of class array field (Valgrind clean)
# -------------------------------------------------------------
run_test "member assign of class array field (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t04_member_array.cco"
class Item { id: int; }
class Container { items: Item[]; }
fn main() -> int {
    let c = Container { items: alloc(Item, 2) };
    let arr = alloc(Item, 3);
    c.items = arr;
    print(len(c.items));
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t04_member_array.cco" -o "$TMP_DIR/t04_member_array.c" 2>"$TMP_DIR/t04.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t04.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t04_member_array.c" -o "$TMP_DIR/t04_member_array.bin" 2>"$TMP_DIR/t04_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t04_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t04_member_array.bin" >/dev/null 2>"$TMP_DIR/t04_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t04_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 5: Negative: Member assign of class array field rejects use-after-move
# -------------------------------------------------------------
run_test "Negative: Member assign of class array rejects use-after-move"
cat << 'EOF' > "$TMP_DIR/t05_member_array_neg.cco"
class Item { id: int; }
class Container { items: Item[]; }
fn main() -> int {
    let c = Container { items: alloc(Item, 2) };
    let arr = alloc(Item, 3);
    c.items = arr;
    print(len(arr));
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t05_member_array_neg.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure for use after member assign move, got 0"
elif ! echo "$err_out" | grep -q "use of moved value 'arr'"; then
    fail_test "Expected 'use of moved value', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 6: Member assignment of primitive array field (Valgrind clean)
# -------------------------------------------------------------
run_test "member assign of primitive array field (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t06_member_prim_arr.cco"
class Numbers { nums: int[]; }
fn main() -> int {
    let c = Numbers { nums: alloc(int, 2) };
    let arr = alloc(int, 5);
    arr[0] = 10;
    arr[1] = 20;
    c.nums = arr;
    print(c.nums[1]);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t06_member_prim_arr.cco" -o "$TMP_DIR/t06_member_prim_arr.c" 2>"$TMP_DIR/t06.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t06.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t06_member_prim_arr.c" -o "$TMP_DIR/t06_member_prim_arr.bin" 2>"$TMP_DIR/t06_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t06_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t06_member_prim_arr.bin" >/dev/null 2>"$TMP_DIR/t06_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t06_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 7: Member assignment of map field (Valgrind clean)
# -------------------------------------------------------------
run_test "member assign of map field (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t07_member_map.cco"
class Dictionary { m: map[string]int; }
fn main() -> int {
    let d = Dictionary { m: map_new(string, int) };
    let fresh = map_new(string, int);
    fresh = put(fresh, "hello", 100);
    d.m = fresh;
    print(get(d.m, "hello"));
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t07_member_map.cco" -o "$TMP_DIR/t07_member_map.c" 2>"$TMP_DIR/t07.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t07.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t07_member_map.c" -o "$TMP_DIR/t07_member_map.bin" 2>"$TMP_DIR/t07_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t07_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t07_member_map.bin" >/dev/null 2>"$TMP_DIR/t07_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t07_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 8: Negative: Member assign of map field rejects use-after-move
# -------------------------------------------------------------
run_test "Negative: Member assign of map rejects use-after-move"
cat << 'EOF' > "$TMP_DIR/t08_member_map_neg.cco"
class Dictionary { m: map[string]int; }
fn main() -> int {
    let d = Dictionary { m: map_new(string, int) };
    let fresh = map_new(string, int);
    fresh = put(fresh, "hello", 100);
    d.m = fresh;
    print(get(fresh, "hello"));
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t08_member_map_neg.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure for use after map member assign, got 0"
elif ! echo "$err_out" | grep -q "use of moved value 'fresh'"; then
    fail_test "Expected 'use of moved value', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 9: for loop init owned variable surviving continue (Valgrind clean)
# -------------------------------------------------------------
run_test "for loop init owned variable surviving continue (0 leaks, 0 use-after-free)"
cat << 'EOF' > "$TMP_DIR/t09_for_continue.cco"
class Res { id: int; }
fn main() -> int {
    let sum = 0;
    for (let r = Res { id: 7 }; sum < 3; sum++) {
        if (sum == 0) {
            continue;
        }
        print(r.id);
    }
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t09_for_continue.cco" -o "$TMP_DIR/t09_for_continue.c" 2>"$TMP_DIR/t09.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t09.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t09_for_continue.c" -o "$TMP_DIR/t09_for_continue.bin" 2>"$TMP_DIR/t09_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t09_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t09_for_continue.bin" >/dev/null 2>"$TMP_DIR/t09_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t09_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 10: for loop init owned variable normal exit cleanup (0 leaks)
# -------------------------------------------------------------
run_test "for loop init owned variable normal exit cleanup (0 leaks)"
cat << 'EOF' > "$TMP_DIR/t10_for_normal_exit.cco"
class Res { id: int; }
fn main() -> int {
    let dummy = 0;
    for (let r = Res { id: 42 }; dummy < 2; dummy++) {
        print(r.id);
    }
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t10_for_normal_exit.cco" -o "$TMP_DIR/t10_for_normal_exit.c" 2>"$TMP_DIR/t10.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t10.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t10_for_normal_exit.c" -o "$TMP_DIR/t10_for_normal_exit.bin" 2>"$TMP_DIR/t10_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t10_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t10_for_normal_exit.bin" >/dev/null 2>"$TMP_DIR/t10_valgrind.err"; then
    fail_test "Valgrind detected memory leak: $(cat "$TMP_DIR/t10_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 11: for loop init owned variable cleanup on break (Valgrind clean)
# -------------------------------------------------------------
run_test "for loop init owned variable cleanup on break (0 leaks, 0 double-frees)"
cat << 'EOF' > "$TMP_DIR/t11_for_break.cco"
class Res { id: int; }
fn main() -> int {
    let dummy = 0;
    for (let r = Res { id: 99 }; dummy < 10; dummy++) {
        if (dummy == 2) {
            break;
        }
    }
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t11_for_break.cco" -o "$TMP_DIR/t11_for_break.c" 2>"$TMP_DIR/t11.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t11.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t11_for_break.c" -o "$TMP_DIR/t11_for_break.bin" 2>"$TMP_DIR/t11_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t11_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t11_for_break.bin" >/dev/null 2>"$TMP_DIR/t11_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t11_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 12: for loop with primitive init continuing unchanged
# -------------------------------------------------------------
run_test "for loop with primitive init continues unchanged (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t12_for_prim.cco"
fn main() -> int {
    let count = 0;
    for (let i = 0; i < 5; i++) {
        count = count + i;
    }
    print(count);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t12_for_prim.cco" -o "$TMP_DIR/t12_for_prim.c" 2>"$TMP_DIR/t12.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t12.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t12_for_prim.c" -o "$TMP_DIR/t12_for_prim.bin" 2>"$TMP_DIR/t12_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t12_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t12_for_prim.bin" >/dev/null 2>"$TMP_DIR/t12_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t12_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 13: Negative: Condition move in if statement rejected
# -------------------------------------------------------------
run_test "Negative: Condition move in if statement rejected"
cat << 'EOF' > "$TMP_DIR/t13_if_cond_move.cco"
class Res { id: int; }
fn consume(r: Res) -> bool { return r.id > 0; }
fn main() -> int {
    let r = Res { id: 1 };
    if (consume(r)) {
        print(1);
    }
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t13_if_cond_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving in if condition, got 0"
elif ! echo "$err_out" | grep -q "cannot move 'r' inside a condition expression"; then
    fail_test "Expected 'cannot move in condition expression', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 14: Negative: Condition move in while statement rejected
# -------------------------------------------------------------
run_test "Negative: Condition move in while statement rejected"
cat << 'EOF' > "$TMP_DIR/t14_while_cond_move.cco"
class Res { id: int; }
fn consume(r: Res) -> bool { return r.id > 0; }
fn main() -> int {
    let r = Res { id: 1 };
    while (consume(r)) {
        print(1);
    }
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t14_while_cond_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving in while condition, got 0"
elif ! echo "$err_out" | grep -q "cannot move 'r' inside a condition expression"; then
    fail_test "Expected 'cannot move in condition expression', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 15: Negative: Condition move in for loop condition rejected
# -------------------------------------------------------------
run_test "Negative: Condition move in for loop condition rejected"
cat << 'EOF' > "$TMP_DIR/t15_for_cond_move.cco"
class Res { id: int; }
fn consume(r: Res) -> bool { return r.id > 0; }
fn main() -> int {
    let r = Res { id: 1 };
    let i = 0;
    for (i = 0; consume(r); i++) {
        print(i);
    }
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t15_for_cond_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving in for condition, got 0"
elif ! echo "$err_out" | grep -q "cannot move 'r' inside a condition expression"; then
    fail_test "Expected 'cannot move in condition expression', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 16: Negative: Move in RHS of && short-circuit operator rejected
# -------------------------------------------------------------
run_test "Negative: Move in RHS of && rejected"
cat << 'EOF' > "$TMP_DIR/t16_and_move.cco"
class Res { id: int; }
fn consume(r: Res) -> bool { return r.id > 0; }
fn main() -> int {
    let r = Res { id: 1 };
    let flag = true;
    if (flag && consume(r)) {
        print(1);
    }
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t16_and_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving in && RHS, got 0"
elif ! echo "$err_out" | grep -q "cannot move 'r' inside a condition expression"; then
    fail_test "Expected 'cannot move in condition expression', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 17: Negative: Move in RHS of || short-circuit operator rejected
# -------------------------------------------------------------
run_test "Negative: Move in RHS of || rejected"
cat << 'EOF' > "$TMP_DIR/t17_or_move.cco"
class Res { id: int; }
fn consume(r: Res) -> bool { return r.id > 0; }
fn main() -> int {
    let r = Res { id: 1 };
    let flag = false;
    if (flag || consume(r)) {
        print(1);
    }
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t17_or_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure moving in || RHS, got 0"
elif ! echo "$err_out" | grep -q "cannot move 'r' inside a condition expression"; then
    fail_test "Expected 'cannot move in condition expression', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 18: Positive: Borrowing in condition expression allowed (Valgrind clean)
# -------------------------------------------------------------
run_test "Positive: Borrowing in condition expression allowed (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t18_borrow_cond.cco"
class Res { id: int; }
fn check(r: &Res) -> bool { return r.id > 0; }
fn main() -> int {
    let r = Res { id: 42 };
    let flag = true;
    if (flag && check(&r)) {
        print(r.id);
    }
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t18_borrow_cond.cco" -o "$TMP_DIR/t18_borrow_cond.c" 2>"$TMP_DIR/t18.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t18.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t18_borrow_cond.c" -o "$TMP_DIR/t18_borrow_cond.bin" 2>"$TMP_DIR/t18_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t18_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t18_borrow_cond.bin" >/dev/null 2>"$TMP_DIR/t18_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t18_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 19: Negative: break statement outside of loop rejected
# -------------------------------------------------------------
run_test "Negative: break statement outside of loop rejected"
cat << 'EOF' > "$TMP_DIR/t19_bare_break.cco"
fn main() -> int {
    break;
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t19_bare_break.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure for bare break outside loop, got 0"
elif ! echo "$err_out" | grep -q "'break' statement outside of loop"; then
    fail_test "Expected 'break statement outside of loop', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 20: Negative: continue statement outside of loop rejected
# -------------------------------------------------------------
run_test "Negative: continue statement outside of loop rejected"
cat << 'EOF' > "$TMP_DIR/t20_bare_continue.cco"
fn main() -> int {
    continue;
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t20_bare_continue.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure for bare continue outside loop, got 0"
elif ! echo "$err_out" | grep -q "'continue' statement outside of loop"; then
    fail_test "Expected 'continue statement outside of loop', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 21: Negative: break in match arm outside of loop rejected
# -------------------------------------------------------------
run_test "Negative: break in match arm outside loop rejected"
cat << 'EOF' > "$TMP_DIR/t21_match_break.cco"
enum Action { Stop, Run }
fn main() -> int {
    let a = Action.Stop;
    match (a) {
        Action.Stop => {
            break;
        }
        Action.Run => {
            print(1);
        }
    }
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t21_match_break.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure for match break outside loop, got 0"
elif ! echo "$err_out" | grep -q "'break' statement outside of loop"; then
    fail_test "Expected 'break statement outside of loop', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 22: Negative: Match non-terminating arm move detected after match
# -------------------------------------------------------------
run_test "Negative: Match non-terminating arm move detected after match"
cat << 'EOF' > "$TMP_DIR/t22_match_arm_move.cco"
enum Action { Stop, Run }
class Res { id: int; }
fn consume(r: Res) -> void { }
fn test(a: Action) -> void {
    let r = Res { id: 1 };
    match (a) {
        Action.Stop => {
            print(0);
        }
        Action.Run => {
            consume(r);
        }
    }
    print(r.id);
}
fn main() -> int {
    return 0;
}
EOF
set +e
err_out=$("$CCO" "$TMP_DIR/t22_match_arm_move.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected failure using conditionally moved value after match, got 0"
elif ! echo "$err_out" | grep -q "may be moved depending on which branch executes"; then
    fail_test "Expected 'may be moved depending on which branch executes', got: $err_out"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 23: Match arm conditional move: both variants clean under Valgrind
# -------------------------------------------------------------
run_test "Match arm conditional move: both variants clean (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t23_match_runtime.cco"
enum Action { Stop, Run }
class Res { id: int; }
fn consume(r: Res) -> void {
    print(r.id);
}
fn test(a: Action) -> void {
    let r = Res { id: 100 };
    match (a) {
        Action.Stop => {
            print(0);
        }
        Action.Run => {
            consume(r);
        }
    }
}
fn main() -> int {
    let s = Action.Stop;
    test(s);
    let r = Action.Run;
    test(r);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t23_match_runtime.cco" -o "$TMP_DIR/t23_match_runtime.c" 2>"$TMP_DIR/t23.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t23.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t23_match_runtime.c" -o "$TMP_DIR/t23_match_runtime.bin" 2>"$TMP_DIR/t23_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t23_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t23_match_runtime.bin" >/dev/null 2>"$TMP_DIR/t23_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t23_valgrind.err")"
else
    pass_test
fi

# -------------------------------------------------------------
# Test 24: Nested control flow with loop, if/else, and continue (Valgrind clean)
# -------------------------------------------------------------
run_test "Nested control flow: for loop + if/else + continue (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t24_nested_ctrl.cco"
class Res { id: int; }
fn main() -> int {
    let total = 0;
    for (let r = Res { id: 10 }; total < 5; total++) {
        let local_res = Res { id: total };
        if (total % 2 == 0) {
            continue;
        }
        print(r.id + local_res.id);
    }
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/t24_nested_ctrl.cco" -o "$TMP_DIR/t24_nested_ctrl.c" 2>"$TMP_DIR/t24.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t24.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/t24_nested_ctrl.c" -o "$TMP_DIR/t24_nested_ctrl.bin" 2>"$TMP_DIR/t24_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/t24_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t24_nested_ctrl.bin" >/dev/null 2>"$TMP_DIR/t24_valgrind.err"; then
    fail_test "Valgrind detected memory error: $(cat "$TMP_DIR/t24_valgrind.err")"
else
    pass_test
fi

echo "=============================================================="
printf "Control-Flow Soundness Suite: %d / %d Passed (%d%% Success)\n" \
    "$passed_tests" "$total_tests" "$(( passed_tests * 100 / (total_tests > 0 ? total_tests : 1) ))"
echo "=============================================================="

if [ "$failed_tests" -gt 0 ]; then
    exit 1
fi
exit 0
