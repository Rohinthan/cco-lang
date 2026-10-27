#!/usr/bin/env bash
# ==============================================================================
# tests/test_ownership_member_moves.sh
# Cco Compiler Ownership Analysis: Milestone 5 Member-Move Soundness & Hardening
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_member_moves_XXXXXX)

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
    (cd "$REPO_ROOT" && make)
fi

echo "=============================================================="
echo "Cco Compiler: Ownership Member-Move Soundness Test Suite (M5)"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Test 1: Negative: let b = c.r rejected
# ------------------------------------------------------------------------------
run_test "Negative: Member extraction into owned local (let b = c.r) rejected"
cat << 'EOF' > "$TMP_DIR/t1.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 1 } };
    let b = c.r;
    return 0;
}
EOF
rm -f "$TMP_DIR/t1.bin"
if "$CCO" "$TMP_DIR/t1.cco" -o "$TMP_DIR/t1.bin" 2>"$TMP_DIR/t1.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t1.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.r'" "$TMP_DIR/t1.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t1.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 2: Negative: c2.r = c1.r rejected
# ------------------------------------------------------------------------------
run_test "Negative: Member-to-member assignment (c2.r = c1.r) rejected"
cat << 'EOF' > "$TMP_DIR/t2.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c1 = Container { r: Res { id: 1 } };
    let c2 = Container { r: Res { id: 2 } };
    c2.r = c1.r;
    return 0;
}
EOF
rm -f "$TMP_DIR/t2.bin"
if "$CCO" "$TMP_DIR/t2.cco" -o "$TMP_DIR/t2.bin" 2>"$TMP_DIR/t2.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t2.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c1.r'" "$TMP_DIR/t2.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t2.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 3: Positive: c.r = a accepted (ownership transfer into member)
# ------------------------------------------------------------------------------
run_test "Positive: Assignment of local owner into member (c.r = a) (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t3.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let a = Res { id: 42 };
    let c = Container { r: Res { id: 0 } };
    c.r = a;
    print(c.r.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t3.bin"
if ! "$CCO" "$TMP_DIR/t3.cco" -o "$TMP_DIR/t3.bin" 2>"$TMP_DIR/t3.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t3.err")"
elif ! out=$("$TMP_DIR/t3.bin"); then
    fail_test "execution failed"
elif [ "$out" != "42" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t3.bin" >/dev/null 2>"$TMP_DIR/t3.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t3.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 4: Positive: c.r = c.r self-assignment accepted
# ------------------------------------------------------------------------------
run_test "Positive: Member self-assignment (c.r = c.r) (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t4.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 99 } };
    c.r = c.r;
    print(c.r.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t4.bin"
if ! "$CCO" "$TMP_DIR/t4.cco" -o "$TMP_DIR/t4.bin" 2>"$TMP_DIR/t4.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t4.err")"
elif ! out=$("$TMP_DIR/t4.bin"); then
    fail_test "execution failed"
elif [ "$out" != "99" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t4.bin" >/dev/null 2>"$TMP_DIR/t4.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t4.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 5: Positive: Replacement with fresh allocation
# ------------------------------------------------------------------------------
run_test "Positive: Replacement with newly allocated resource (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t5.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 10 } };
    c.r = Res { id: 20 };
    print(c.r.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t5.bin"
if ! "$CCO" "$TMP_DIR/t5.cco" -o "$TMP_DIR/t5.bin" 2>"$TMP_DIR/t5.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t5.err")"
elif ! out=$("$TMP_DIR/t5.bin"); then
    fail_test "execution failed"
elif [ "$out" != "20" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t5.bin" >/dev/null 2>"$TMP_DIR/t5.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t5.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 6: Positive: let id = c.id (primitive field copying)
# ------------------------------------------------------------------------------
run_test "Positive: Primitive field read (let id = c.id) (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t6.cco"
class Item { id: int; val: int; }
fn main() -> int {
    let it = Item { id: 7, val: 14 };
    let x = it.id;
    let y = it.val;
    print(x + y);
    return 0;
}
EOF
rm -f "$TMP_DIR/t6.bin"
if ! "$CCO" "$TMP_DIR/t6.cco" -o "$TMP_DIR/t6.bin" 2>"$TMP_DIR/t6.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t6.err")"
elif ! out=$("$TMP_DIR/t6.bin"); then
    fail_test "execution failed"
elif [ "$out" != "21" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t6.bin" >/dev/null 2>"$TMP_DIR/t6.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t6.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 7: Positive: let id = c.r.id (nested primitive field reading)
# ------------------------------------------------------------------------------
run_test "Positive: Nested primitive field read (let id = c.r.id) (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t7.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 55 } };
    let id = c.r.id;
    print(id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t7.bin"
if ! "$CCO" "$TMP_DIR/t7.cco" -o "$TMP_DIR/t7.bin" 2>"$TMP_DIR/t7.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t7.err")"
elif ! out=$("$TMP_DIR/t7.bin"); then
    fail_test "execution failed"
elif [ "$out" != "55" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t7.bin" >/dev/null 2>"$TMP_DIR/t7.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t7.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 8: Positive: Passing c.r to borrowed parameter
# ------------------------------------------------------------------------------
run_test "Positive: Passing member to borrowed parameter (&Res) (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t8.cco"
class Res { id: int; }
class Container { r: Res; }
fn inspect(r: &Res) -> void {
    print(r.id);
}
fn main() -> int {
    let c = Container { r: Res { id: 77 } };
    inspect(c.r);
    return 0;
}
EOF
rm -f "$TMP_DIR/t8.bin"
if ! "$CCO" "$TMP_DIR/t8.cco" -o "$TMP_DIR/t8.bin" 2>"$TMP_DIR/t8.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t8.err")"
elif ! out=$("$TMP_DIR/t8.bin"); then
    fail_test "execution failed"
elif [ "$out" != "77" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t8.bin" >/dev/null 2>"$TMP_DIR/t8.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t8.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 9: Negative: Passing c.r to consuming parameter rejected
# ------------------------------------------------------------------------------
run_test "Negative: Passing member to consuming parameter (Res) rejected"
cat << 'EOF' > "$TMP_DIR/t9.cco"
class Res { id: int; }
class Container { r: Res; }
fn consume(r: Res) -> void {
    print(r.id);
}
fn main() -> int {
    let c = Container { r: Res { id: 1 } };
    consume(c.r);
    return 0;
}
EOF
rm -f "$TMP_DIR/t9.bin"
if "$CCO" "$TMP_DIR/t9.cco" -o "$TMP_DIR/t9.bin" 2>"$TMP_DIR/t9.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t9.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.r'" "$TMP_DIR/t9.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t9.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 10: Negative: Returning c.r from function rejected
# ------------------------------------------------------------------------------
run_test "Negative: Returning member from function (return c.r) rejected"
cat << 'EOF' > "$TMP_DIR/t10.cco"
class Res { id: int; }
class Container { r: Res; }
fn get_r() -> Res {
    let c = Container { r: Res { id: 1 } };
    return c.r;
}
fn main() -> int {
    let r = get_r();
    return 0;
}
EOF
rm -f "$TMP_DIR/t10.bin"
if "$CCO" "$TMP_DIR/t10.cco" -o "$TMP_DIR/t10.bin" 2>"$TMP_DIR/t10.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t10.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.r'" "$TMP_DIR/t10.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t10.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 11: Negative: Nested member extraction rejected
# ------------------------------------------------------------------------------
run_test "Negative: Nested member extraction (let b = o.inner.r) rejected"
cat << 'EOF' > "$TMP_DIR/t11.cco"
class Res { id: int; }
class Inner { r: Res; }
class Outer { inner: Inner; }
fn main() -> int {
    let o = Outer { inner: Inner { r: Res { id: 1 } } };
    let b = o.inner.r;
    return 0;
}
EOF
rm -f "$TMP_DIR/t11.bin"
if "$CCO" "$TMP_DIR/t11.cco" -o "$TMP_DIR/t11.bin" 2>"$TMP_DIR/t11.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t11.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'o.inner.r'" "$TMP_DIR/t11.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t11.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 12: Negative: Conditional member extraction in if rejected
# ------------------------------------------------------------------------------
run_test "Negative: Conditional member extraction in if branch rejected"
cat << 'EOF' > "$TMP_DIR/t12.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 1 } };
    let flag = true;
    if (flag) {
        let b = c.r;
    }
    return 0;
}
EOF
rm -f "$TMP_DIR/t12.bin"
if "$CCO" "$TMP_DIR/t12.cco" -o "$TMP_DIR/t12.bin" 2>"$TMP_DIR/t12.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t12.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.r'" "$TMP_DIR/t12.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t12.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 13: Negative: Member extraction inside loop rejected
# ------------------------------------------------------------------------------
run_test "Negative: Member extraction inside loop rejected"
cat << 'EOF' > "$TMP_DIR/t13.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 1 } };
    let i = 0;
    while (i < 2) {
        let b = c.r;
        i = i + 1;
    }
    return 0;
}
EOF
rm -f "$TMP_DIR/t13.bin"
if "$CCO" "$TMP_DIR/t13.cco" -o "$TMP_DIR/t13.bin" 2>"$TMP_DIR/t13.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t13.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.r'" "$TMP_DIR/t13.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t13.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 14: Negative: Member extraction inside match arm rejected
# ------------------------------------------------------------------------------
run_test "Negative: Member extraction inside match arm rejected"
cat << 'EOF' > "$TMP_DIR/t14.cco"
enum Mode {
    Fast,
    Slow
}
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 1 } };
    let m = Mode.Fast;
    match m {
        Mode.Fast => {
            let b = c.r;
        }
        Mode.Slow => {
            print(0);
        }
    }
    return 0;
}
EOF
rm -f "$TMP_DIR/t14.bin"
if "$CCO" "$TMP_DIR/t14.cco" -o "$TMP_DIR/t14.bin" 2>"$TMP_DIR/t14.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t14.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.r'" "$TMP_DIR/t14.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t14.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 15: Negative: Reassignment of member to existing local rejected
# ------------------------------------------------------------------------------
run_test "Negative: Reassignment of member to local (b = c.r) rejected"
cat << 'EOF' > "$TMP_DIR/t15.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 1 } };
    let b = Res { id: 2 };
    b = c.r;
    return 0;
}
EOF
rm -f "$TMP_DIR/t15.bin"
if "$CCO" "$TMP_DIR/t15.cco" -o "$TMP_DIR/t15.bin" 2>"$TMP_DIR/t15.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t15.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.r'" "$TMP_DIR/t15.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t15.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 16: Negative: Existing array-element move restriction preserved
# ------------------------------------------------------------------------------
run_test "Negative: Existing array-element move restriction preserved"
cat << 'EOF' > "$TMP_DIR/t16.cco"
class Res { id: int; }
fn main() -> int {
    let arr: Res[] = alloc(Res, 2);
    let b = arr[0];
    return 0;
}
EOF
rm -f "$TMP_DIR/t16.bin"
if "$CCO" "$TMP_DIR/t16.cco" -o "$TMP_DIR/t16.bin" 2>"$TMP_DIR/t16.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t16.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of array element 'arr\[0\]'" "$TMP_DIR/t16.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t16.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 17: Negative: Use-after-move of local preserved
# ------------------------------------------------------------------------------
run_test "Negative: Use-after-move of local variable preserved"
cat << 'EOF' > "$TMP_DIR/t17.cco"
class Res { id: int; }
fn main() -> int {
    let a = Res { id: 1 };
    let b = a;
    print(a.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t17.bin"
if "$CCO" "$TMP_DIR/t17.cco" -o "$TMP_DIR/t17.bin" 2>"$TMP_DIR/t17.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t17.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "use of moved value 'a'" "$TMP_DIR/t17.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t17.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 18: Positive: Reinitialization of moved local preserved
# ------------------------------------------------------------------------------
run_test "Positive: Reinitialization of moved local (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t18.cco"
class Res { id: int; }
fn main() -> int {
    let a = Res { id: 1 };
    let b = a;
    a = Res { id: 2 };
    print(a.id + b.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t18.bin"
if ! "$CCO" "$TMP_DIR/t18.cco" -o "$TMP_DIR/t18.bin" 2>"$TMP_DIR/t18.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t18.err")"
elif ! out=$("$TMP_DIR/t18.bin"); then
    fail_test "execution failed"
elif [ "$out" != "3" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t18.bin" >/dev/null 2>"$TMP_DIR/t18.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t18.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 19: Positive: Normal container destruction (exactly 1 free of member)
# ------------------------------------------------------------------------------
run_test "Positive: Normal container destruction lifecycle (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t19.cco"
class Res { id: int; }
class Container { r: Res; }
fn main() -> int {
    let c = Container { r: Res { id: 100 } };
    print(c.r.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t19.bin"
if ! "$CCO" "$TMP_DIR/t19.cco" -o "$TMP_DIR/t19.bin" 2>"$TMP_DIR/t19.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t19.err")"
elif ! out=$("$TMP_DIR/t19.bin"); then
    fail_test "execution failed"
elif [ "$out" != "100" ]; then
    fail_test "unexpected output: $out"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t19.bin" >/dev/null 2>"$TMP_DIR/t19.valg"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t19.valg")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 20: Negative: Diagnostic accuracy and artifact check
# ------------------------------------------------------------------------------
run_test "Negative: Accurate diagnostic source location & zero-artifact guarantee"
cat << 'EOF' > "$TMP_DIR/t20.cco"
class Res { id: int; }
class Box { val: Res; }
fn main() -> int {
    let box = Box { val: Res { id: 42 } };
    let extracted = box.val;
    return 0;
}
EOF
rm -f "$TMP_DIR/t20.bin"
# Place a dummy file to verify stale binary removal/non-creation
touch "$TMP_DIR/t20_dummy.flag"
"$CCO" "$TMP_DIR/t20.cco" -o "$TMP_DIR/t20.bin" 2>"$TMP_DIR/t20.err" || true
if [ -f "$TMP_DIR/t20.bin" ]; then
    fail_test "binary was created despite compilation failure"
elif ! grep -q "t20.cco:5:25" "$TMP_DIR/t20.err" && ! grep -q ":5:25" "$TMP_DIR/t20.err"; then
    fail_test "diagnostic line/col inaccurate: $(cat "$TMP_DIR/t20.err")"
elif ! grep -q "cannot move out of class member 'box.val'" "$TMP_DIR/t20.err"; then
    fail_test "expected error message missing: $(cat "$TMP_DIR/t20.err")"
else
    pass_test
fi

echo "=============================================================="
printf "Member Move Soundness Suite: %d / %d Passed (%d%% Success)\n" \
    "$passed_tests" "$total_tests" "$(( passed_tests * 100 / total_tests ))"
echo "=============================================================="

if [ "$failed_tests" -ne 0 ]; then
    exit 1
fi
exit 0
