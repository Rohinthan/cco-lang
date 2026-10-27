#!/usr/bin/env bash
# ==============================================================
# Cco Compiler — Milestone 1: Ownership Buffer Safety Test Suite
# Tests dynamic moved-variable capacity, branch snapshots,
# capacity exhaustion handling, and use-after-move beyond 256.
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
echo "Cco Compiler: Ownership Analysis Buffer Safety Test Suite"
echo "=============================================================="

TMP_DIR="build/test_own_safety"
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
# Test 1: Branch snapshots below former capacity (10 moved vars)
# -------------------------------------------------------------
run_test "Branch snapshots below capacity (10 moved vars)"
cat << 'EOF' > "$TMP_DIR/test1_below_cap.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    b0 = Box { val: 0 };
    b1 = Box { val: 1 };
    b2 = Box { val: 2 };
    consume(b0);
    consume(b1);
    flag = true;
    if (flag) {
        consume(b2);
    } else {
        print(0);
    }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test1_below_cap.cco" -o "$TMP_DIR/test1_below_cap"; then
    fail_test "Compilation failed"
fi
if ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/test1_below_cap" > /dev/null 2>&1; then
    fail_test "Valgrind error"
fi
pass_test

# -------------------------------------------------------------
# Test 2: Branch snapshots at former 128 boundary (exactly 128 moves)
# -------------------------------------------------------------
run_test "Branch snapshots at former 128 boundary (exactly 128 moves)"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(128):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(128):
    out.append(f"    consume(b{i});")
out.append("    flag = true;")
out.append("    if (flag) {")
out.append("        print(128);")
out.append("    } else {")
out.append("        print(0);")
out.append("    }")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test2_at_128.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
if ! $CCO "$TMP_DIR/test2_at_128.cco" -o "$TMP_DIR/test2_at_128"; then
    fail_test "Compilation failed"
fi
out_val=$("$TMP_DIR/test2_at_128")
if [ "$out_val" != "128" ]; then
    fail_test "Unexpected execution output: $out_val"
fi
pass_test

# -------------------------------------------------------------
# Test 3: Branch snapshots beyond former 128 boundary (135 moves)
# -------------------------------------------------------------
run_test "Branch snapshots beyond former 128 boundary (135 moves)"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(135):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(135):
    out.append(f"    consume(b{i});")
out.append("    flag = true;")
out.append("    if (flag) {")
out.append("        print(135);")
out.append("    } else {")
out.append("        print(0);")
out.append("    }")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test3_beyond_128.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
if ! $CCO "$TMP_DIR/test3_beyond_128.cco" -o "$TMP_DIR/test3_beyond_128"; then
    fail_test "Compilation failed"
fi
out_val=$("$TMP_DIR/test3_beyond_128")
if [ "$out_val" != "135" ]; then
    fail_test "Unexpected output: $out_val"
fi
pass_test

# -------------------------------------------------------------
# Test 4: Branch snapshots at former 256 capacity (256 moves)
# -------------------------------------------------------------
run_test "Branch snapshots at former 256 capacity (256 moves)"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(256):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(256):
    out.append(f"    consume(b{i});")
out.append("    flag = true;")
out.append("    if (flag) {")
out.append("        print(256);")
out.append("    } else {")
out.append("        print(0);")
out.append("    }")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test4_at_256.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
if ! $CCO "$TMP_DIR/test4_at_256.cco" -o "$TMP_DIR/test4_at_256"; then
    fail_test "Compilation failed"
fi
out_val=$("$TMP_DIR/test4_at_256")
if [ "$out_val" != "256" ]; then
    fail_test "Unexpected output: $out_val"
fi
pass_test

# -------------------------------------------------------------
# Test 5: Dynamic capacity expansion beyond 256 moves (300 moves)
# -------------------------------------------------------------
run_test "Dynamic capacity expansion beyond 256 moves (300 moves)"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(300):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(300):
    out.append(f"    consume(b{i});")
out.append("    flag = true;")
out.append("    if (flag) {")
out.append("        print(300);")
out.append("    } else {")
out.append("        print(0);")
out.append("    }")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test5_beyond_256.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
if ! $CCO "$TMP_DIR/test5_beyond_256.cco" -o "$TMP_DIR/test5_beyond_256"; then
    fail_test "Compilation failed"
fi
out_val=$("$TMP_DIR/test5_beyond_256")
if [ "$out_val" != "300" ]; then
    fail_test "Unexpected output: $out_val"
fi
pass_test

# -------------------------------------------------------------
# Test 6: No false success when moves exceed 256 (use-after-move detection)
# -------------------------------------------------------------
run_test "Accurate use-after-move detection beyond 256 moves"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(270):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(270):
    out.append(f"    consume(b{i});")
# Using b268 after it was moved must be detected!
out.append("    print(b268.val);")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test6_use_after_move_268.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
set +e
err_out=$($CCO "$TMP_DIR/test6_use_after_move_268.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected compilation failure on use-after-move beyond 256, but compilation succeeded"
fi
if ! echo "$err_out" | grep -q "use of moved value 'b268'"; then
    fail_test "Expected 'use of moved value b268' diagnostic, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 7: Branch restoration & conditional move merging across branches
# -------------------------------------------------------------
run_test "Branch restoration and conditional move merging across if branches"
cat << 'EOF' > "$TMP_DIR/test7_branch_merge.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    x = Box { val: 10 };
    y = Box { val: 20 };
    flag = true;
    if (flag) {
        consume(x);
    } else {
        consume(y);
    }
    # Using x here must report that x may be moved depending on branch!
    print(x.val);
    return 0;
}
EOF
set +e
err_out=$($CCO "$TMP_DIR/test7_branch_merge.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected compilation failure on conditional move use, but compilation succeeded"
fi
if ! echo "$err_out" | grep -q "'x' may be moved depending on which branch executes"; then
    fail_test "Expected conditional move diagnostic, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 8: Safe handling of capacity exhaustion via CCO_MAX_MOVED_CAP
# -------------------------------------------------------------
run_test "Safe handling of capacity exhaustion via CCO_MAX_MOVED_CAP"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(70):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(70):
    out.append(f"    consume(b{i});")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test8_exhaustion.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
set +e
err_out=$(CCO_MAX_MOVED_CAP=64 $CCO "$TMP_DIR/test8_exhaustion.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected compilation failure on capacity exhaustion, but got exit code 0"
fi
if ! echo "$err_out" | grep -q "moved variable table capacity limit exceeded (64 variables"; then
    fail_test "Expected capacity limit exceeded diagnostic, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 9: Valgrind memory leak verification on compiler with 200 moves
# -------------------------------------------------------------
run_test "Valgrind verification on compiler compiling 200 moves"
if ! valgrind --error-exitcode=1 "$CCO" "$TMP_DIR/test3_beyond_128.cco" -o /dev/null > /dev/null 2>&1; then
    fail_test "Valgrind detected memory corruption in compiler"
fi
pass_test

# -------------------------------------------------------------
# Test 10: CCO_MAX_MOVED_CAP below initial capacity (5 moves vs 6 moves)
# -------------------------------------------------------------
run_test "CCO_MAX_MOVED_CAP below initial capacity (5 moves ok, 6 moves rejected)"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(5):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(5):
    out.append(f"    consume(b{i});")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test10_cap5_ok.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(6):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(6):
    out.append(f"    consume(b{i});")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test10_cap5_overflow.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
if ! CCO_MAX_MOVED_CAP=5 $CCO "$TMP_DIR/test10_cap5_ok.cco" -o /dev/null; then
    fail_test "Expected 5 moves with cap 5 to succeed"
fi
set +e
err_out=$(CCO_MAX_MOVED_CAP=5 $CCO "$TMP_DIR/test10_cap5_overflow.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected 6 moves with cap 5 to fail"
fi
if ! echo "$err_out" | grep -q "moved variable table capacity limit exceeded (5 variables, requested 6)"; then
    fail_test "Expected limit exceeded diagnostic, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 11: CCO_MAX_MOVED_CAP exactly equal to required capacity (10 moves)
# -------------------------------------------------------------
run_test "CCO_MAX_MOVED_CAP exactly equal to required capacity (10 moves with cap 10)"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(10):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(10):
    out.append(f"    consume(b{i});")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test11_cap10_exact.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
if ! CCO_MAX_MOVED_CAP=10 $CCO "$TMP_DIR/test11_cap10_exact.cco" -o /dev/null; then
    fail_test "Expected 10 moves with cap 10 to succeed exactly"
fi
pass_test

# -------------------------------------------------------------
# Test 12: CCO_MAX_MOVED_CAP just below required capacity (10 moves with cap 9)
# -------------------------------------------------------------
run_test "CCO_MAX_MOVED_CAP just below required capacity (10 moves with cap 9 fails)"
set +e
err_out=$(CCO_MAX_MOVED_CAP=9 $CCO "$TMP_DIR/test11_cap10_exact.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected 10 moves with cap 9 to fail"
fi
if ! echo "$err_out" | grep -q "moved variable table capacity limit exceeded (9 variables, requested 10)"; then
    fail_test "Expected limit exceeded diagnostic for cap 9, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 13: CCO_MAX_MOVED_CAP invalid non-numeric value
# -------------------------------------------------------------
run_test "CCO_MAX_MOVED_CAP invalid non-numeric value rejected"
set +e
err_out=$(CCO_MAX_MOVED_CAP=abc_invalid $CCO "$TMP_DIR/test1_below_cap.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected invalid non-numeric cap to fail"
fi
if ! echo "$err_out" | grep -q "invalid CCO_MAX_MOVED_CAP value 'abc_invalid'"; then
    fail_test "Expected invalid value diagnostic, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 14: CCO_MAX_MOVED_CAP negative value rejected
# -------------------------------------------------------------
run_test "CCO_MAX_MOVED_CAP negative value rejected"
set +e
err_out=$(CCO_MAX_MOVED_CAP=-10 $CCO "$TMP_DIR/test1_below_cap.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected negative cap to fail"
fi
if ! echo "$err_out" | grep -q "invalid CCO_MAX_MOVED_CAP value '-10'"; then
    fail_test "Expected invalid value diagnostic for negative cap, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 15: CCO_MAX_MOVED_CAP zero rejected
# -------------------------------------------------------------
run_test "CCO_MAX_MOVED_CAP zero rejected"
set +e
err_out=$(CCO_MAX_MOVED_CAP=0 $CCO "$TMP_DIR/test1_below_cap.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected zero cap to fail"
fi
if ! echo "$err_out" | grep -q "invalid CCO_MAX_MOVED_CAP value '0'"; then
    fail_test "Expected invalid value diagnostic for zero cap, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 16: CCO_MAX_MOVED_CAP excessively large value rejected
# -------------------------------------------------------------
run_test "CCO_MAX_MOVED_CAP excessively large value rejected (>1,048,576)"
set +e
err_out=$(CCO_MAX_MOVED_CAP=2000000 $CCO "$TMP_DIR/test1_below_cap.cco" -o /dev/null 2>&1)
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "Expected excessively large cap to fail"
fi
if ! echo "$err_out" | grep -q "invalid CCO_MAX_MOVED_CAP value '2000000'"; then
    fail_test "Expected invalid value diagnostic for excessive cap, got: $err_out"
fi
pass_test

# -------------------------------------------------------------
# Test 17: Repeated sequential branch snapshots (25 consecutive if statements)
# -------------------------------------------------------------
run_test "Repeated sequential branch snapshots (25 consecutive if statements)"
python3 -c '
out = ["class Box { val: int; }", "fn consume(b: Box) -> void { }", "fn main() -> int {"]
for i in range(25):
    out.append(f"    b{i} = Box {{ val: {i} }};")
for i in range(25):
    out.append(f"    flag{i} = true;")
    out.append(f"    if (flag{i}) {{")
    out.append(f"        consume(b{i});")
    out.append(f"    }} else {{")
    out.append(f"        print({i});")
    out.append(f"    }}")
out.append("    return 0;")
out.append("}")
with open("build/test_own_safety/test17_repeated_branches.cco", "w") as f:
    f.write("\n".join(out) + "\n")
'
if ! $CCO "$TMP_DIR/test17_repeated_branches.cco" -o "$TMP_DIR/test17_repeated_branches"; then
    fail_test "Compilation failed on repeated branch snapshots"
fi
pass_test

# -------------------------------------------------------------
# Test 18: Deeply nested if constructs (5 levels) with moves at each level
# -------------------------------------------------------------
run_test "Deeply nested if constructs (5 levels) with branch reconciliation"
cat << 'EOF' > "$TMP_DIR/test18_nested_if.cco"
class Box { val: int; }
fn consume(b: Box) -> void { }
fn main() -> int {
    b1 = Box { val: 1 };
    b2 = Box { val: 2 };
    b3 = Box { val: 3 };
    b4 = Box { val: 4 };
    b5 = Box { val: 5 };
    flag = true;
    if (flag) {
        consume(b1);
        if (flag) {
            consume(b2);
            if (flag) {
                consume(b3);
                if (flag) {
                    consume(b4);
                    if (flag) {
                        consume(b5);
                    }
                }
            }
        }
    }
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test18_nested_if.cco" -o "$TMP_DIR/test18_nested_if"; then
    fail_test "Compilation failed on deeply nested if constructs"
fi
pass_test

# -------------------------------------------------------------
# Test 19: Nested match construct with branch snapshot restoration across arms
# -------------------------------------------------------------
run_test "Nested match construct with snapshot restoration across arms"
cat << 'EOF' > "$TMP_DIR/test19_match_arms.cco"
enum Shape {
    Circle { radius: int },
    Rect { w: int, h: int }
}
class Box { val: int; }
fn consume(b: Box) -> void { }
fn test_shape(s: &Shape, b: Box) -> int {
    match s {
        Shape.Circle { radius } => {
            print(radius);
            return radius;
        }
        Shape.Rect { w, h } => {
            print(w);
            return w + h;
        }
    }
}
fn main() -> int {
    s = Shape.Circle { radius: 10 };
    b = Box { val: 42 };
    test_shape(&s, b);
    return 0;
}
EOF
if ! $CCO "$TMP_DIR/test19_match_arms.cco" -o "$TMP_DIR/test19_match_arms"; then
    fail_test "Compilation failed on match snapshot restoration"
fi
pass_test

# -------------------------------------------------------------
# Test 20: Valgrind memory verification on compiler across nested branches
# -------------------------------------------------------------
run_test "Valgrind memory verification on compiler across nested branches"
if ! valgrind --error-exitcode=1 "$CCO" "$TMP_DIR/test18_nested_if.cco" -o /dev/null > /dev/null 2>&1; then
    fail_test "Valgrind detected memory corruption in nested branch compilation"
fi
pass_test

echo "=============================================================="
echo "Ownership Buffer Safety Suite: $PASSED / $TOTAL Passed (100% Success)"
echo "=============================================================="

rm -rf "$TMP_DIR"
exit 0
