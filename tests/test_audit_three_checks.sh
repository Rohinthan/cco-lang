#!/bin/bash
# ==============================================================
# Cco Compiler Engineering: Comprehensive Three Checks Audit Suite
# Covers:
# Check 1: Member Replacement, Aliasing, Array push() Growth & Map Mutations
# Check 2: For-Loop Cleanup across 5 Exit Paths (Lifetime & Count)
# Check 3: Self-Hosting Compiler Program Correctness & Differential Verification
# ==============================================================
set -euo pipefail

CCO="./cco"
TMP_DIR=$(mktemp -d /tmp/cco_three_checks_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

passed_count=0
total_count=0

run_test() {
    local name="$1"
    total_count=$((total_count + 1))
    echo -n "[$total_count] $name ... "
}

pass_test() {
    passed_count=$((passed_count + 1))
    echo "PASSED"
}

fail_test() {
    local reason="$1"
    echo "FAILED"
    echo "  -> Error: $reason"
    exit 1
}

# ==============================================================
# CHECK 1: Member Replacement, Aliasing & Container Mutations
# ==============================================================

# 1.1 Object member replacement: release old value, store new value
run_test "Check 1.1: Object member replacement releases old value, keeps new"
cat << 'EOF' > "$TMP_DIR/c1_replace.cco"
class Resource { id: int; }
class Container { res: Resource; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn main() -> int {
    let c = Container { res: Res_new(10) };
    c.res = Res_new(20);
    print(c.res.id);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c1_replace.cco" -o "$TMP_DIR/c1_replace.c" 2>"$TMP_DIR/c1_replace.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c1_replace.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c1_replace.c" -o "$TMP_DIR/c1_replace.bin" 2>"$TMP_DIR/c1_replace_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c1_replace_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c1_replace.bin" >/dev/null 2>"$TMP_DIR/c1_replace_val.err"; then
    fail_test "Valgrind error in member replacement: $(cat "$TMP_DIR/c1_replace_val.err")"
else
    pass_test
fi

# 1.2 Object member self-assignment: c.res = c.res must not destroy resource
run_test "Check 1.2: Object member self-assignment is safe (no double-free)"
cat << 'EOF' > "$TMP_DIR/c1_self_assign.cco"
class Resource { id: int; }
class Container { res: Resource; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn main() -> int {
    let c = Container { res: Res_new(42) };
    c.res = c.res;
    print(c.res.id);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c1_self_assign.cco" -o "$TMP_DIR/c1_self_assign.c" 2>"$TMP_DIR/c1_self.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c1_self.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c1_self_assign.c" -o "$TMP_DIR/c1_self_assign.bin" 2>"$TMP_DIR/c1_self_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c1_self_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c1_self_assign.bin" >/dev/null 2>"$TMP_DIR/c1_self_val.err"; then
    fail_test "Valgrind error in self-assignment: $(cat "$TMP_DIR/c1_self_val.err")"
else
    pass_test
fi

# 1.3 Object member aliased replacement: RHS reads member before replacement
run_test "Check 1.3: Member replacement where RHS reads member (aliasing)"
cat << 'EOF' > "$TMP_DIR/c1_alias_replace.cco"
class Resource { id: int; }
class Container { res: Resource; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn Res_transform(r: &Resource) -> Resource {
    return Resource { id: r.id + 100 };
}
fn main() -> int {
    let c = Container { res: Res_new(5) };
    c.res = Res_transform(c.res);
    print(c.res.id);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c1_alias_replace.cco" -o "$TMP_DIR/c1_alias_replace.c" 2>"$TMP_DIR/c1_alias.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c1_alias.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c1_alias_replace.c" -o "$TMP_DIR/c1_alias_replace.bin" 2>"$TMP_DIR/c1_alias_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c1_alias_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c1_alias_replace.bin" >/dev/null 2>"$TMP_DIR/c1_alias_val.err"; then
    fail_test "Valgrind error in aliased replacement: $(cat "$TMP_DIR/c1_alias_val.err")"
else
    pass_test
fi

# 1.4 Array growth through push() across multiple reallocations
run_test "Check 1.4: Container array growth via push() across reallocations"
cat << 'EOF' > "$TMP_DIR/c1_arr_push.cco"
class Holder {
    items: int[];
}
fn main() -> int {
    let h = Holder { items: alloc(int, 2) };
    for (let i: int = 0; i < 500; i += 1) {
        h.items = push(h.items, i * 3);
    }
    print(len(h.items));
    print(h.items[499]);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c1_arr_push.cco" -o "$TMP_DIR/c1_arr_push.c" 2>"$TMP_DIR/c1_arr.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c1_arr.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c1_arr_push.c" -o "$TMP_DIR/c1_arr_push.bin" 2>"$TMP_DIR/c1_arr_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c1_arr_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c1_arr_push.bin" >/dev/null 2>"$TMP_DIR/c1_arr_val.err"; then
    fail_test "Valgrind error in array push growth: $(cat "$TMP_DIR/c1_arr_val.err")"
else
    pass_test
fi

# 1.5 Map mutations on object member: insertions, updates, query
run_test "Check 1.5: Object map member mutations, growth, and updates"
cat << 'EOF' > "$TMP_DIR/c1_map_mut.cco"
class Cache {
    entries: map[string]int;
}
fn main() -> int {
    let c = Cache { entries: map_new(string, int) };
    for (let i: int = 0; i < 200; i += 1) {
        let num_str: string = to_string(i);
        let k: string = concat("key_", num_str);
        c.entries = put(c.entries, k, i * 10);
    }
    print(len(c.entries));
    c.entries = put(c.entries, "key_42", 9999);
    print(get(c.entries, "key_42"));
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c1_map_mut.cco" -o "$TMP_DIR/c1_map_mut.c" 2>"$TMP_DIR/c1_map.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c1_map.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c1_map_mut.c" -o "$TMP_DIR/c1_map_mut.bin" 2>"$TMP_DIR/c1_map_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c1_map_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c1_map_mut.bin" >/dev/null 2>"$TMP_DIR/c1_map_val.err"; then
    fail_test "Valgrind error in map member mutation: $(cat "$TMP_DIR/c1_map_val.err")"
else
    pass_test
fi

# ==============================================================
# CHECK 2: For-Loop Cleanup across All 5 Exit Paths
# ==============================================================

# 2.1 For-loop executes zero times
run_test "Check 2.1: For-loop zero iterations: header resource destroyed exactly once"
cat << 'EOF' > "$TMP_DIR/c2_zero_iter.cco"
class Resource { id: int; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn main() -> int {
    let executed: int = 0;
    let dummy: int = 0;
    for (let r: Resource = Res_new(1); false; dummy += 1) {
        executed += 1;
    }
    print(executed);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c2_zero_iter.cco" -o "$TMP_DIR/c2_zero_iter.c" 2>"$TMP_DIR/c2_zero.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c2_zero.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c2_zero_iter.c" -o "$TMP_DIR/c2_zero_iter.bin" 2>"$TMP_DIR/c2_zero_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c2_zero_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c2_zero_iter.bin" >/dev/null 2>"$TMP_DIR/c2_zero_val.err"; then
    fail_test "Valgrind error in zero-iteration for loop: $(cat "$TMP_DIR/c2_zero_val.err")"
else
    pass_test
fi

# 2.2 For-loop completes normally: inner resources destroyed per iteration, header resource at end
run_test "Check 2.2: For-loop normal completion: inner per-iteration + header at exit"
cat << 'EOF' > "$TMP_DIR/c2_normal.cco"
class Resource { id: int; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn main() -> int {
    let sum: int = 0;
    let count: int = 0;
    for (let r: Resource = Res_new(10); count < 5; count += 1) {
        let inner: Resource = Res_new(r.id + count);
        sum += inner.id;
    }
    print(sum);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c2_normal.cco" -o "$TMP_DIR/c2_normal.c" 2>"$TMP_DIR/c2_norm.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c2_norm.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c2_normal.c" -o "$TMP_DIR/c2_normal.bin" 2>"$TMP_DIR/c2_norm_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c2_norm_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c2_normal.bin" >/dev/null 2>"$TMP_DIR/c2_norm_val.err"; then
    fail_test "Valgrind error in normal for loop exit: $(cat "$TMP_DIR/c2_norm_val.err")"
else
    pass_test
fi

# 2.3 For-loop exits through break: inner and header resources cleaned up cleanly
run_test "Check 2.3: For-loop break exit: inner resource and header resource cleaned up"
cat << 'EOF' > "$TMP_DIR/c2_break.cco"
class Resource { id: int; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn main() -> int {
    let reached: int = 0;
    let count: int = 0;
    for (let r: Resource = Res_new(100); count < 10; count += 1) {
        let inner: Resource = Res_new(r.id + count);
        if (count == 4) {
            reached = inner.id;
            break;
        }
    }
    print(reached);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c2_break.cco" -o "$TMP_DIR/c2_break.c" 2>"$TMP_DIR/c2_break.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c2_break.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c2_break.c" -o "$TMP_DIR/c2_break.bin" 2>"$TMP_DIR/c2_break_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c2_break_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c2_break.bin" >/dev/null 2>"$TMP_DIR/c2_break_val.err"; then
    fail_test "Valgrind error in for loop break exit: $(cat "$TMP_DIR/c2_break_val.err")"
else
    pass_test
fi

# 2.4 For-loop executes continue multiple times: header survives, inner cleaned up each iteration
run_test "Check 2.4: For-loop multiple continues: header survives, inner cleaned per iter"
cat << 'EOF' > "$TMP_DIR/c2_continue.cco"
class Resource { id: int; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn main() -> int {
    let processed: int = 0;
    let count: int = 0;
    for (let r: Resource = Res_new(1); count < 10; count += 1) {
        let inner: Resource = Res_new(r.id * 10 + count);
        if (count % 2 == 0) {
            continue;
        }
        processed += inner.id;
    }
    print(processed);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c2_continue.cco" -o "$TMP_DIR/c2_continue.c" 2>"$TMP_DIR/c2_cont.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c2_cont.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c2_continue.c" -o "$TMP_DIR/c2_continue.bin" 2>"$TMP_DIR/c2_cont_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c2_cont_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c2_continue.bin" >/dev/null 2>"$TMP_DIR/c2_cont_val.err"; then
    fail_test "Valgrind error in multiple continues: $(cat "$TMP_DIR/c2_cont_val.err")"
else
    pass_test
fi

# 2.5 For-loop body returns from function: inner and header cleaned up before return
run_test "Check 2.5: For-loop early return: header and inner cleaned up on return"
cat << 'EOF' > "$TMP_DIR/c2_return.cco"
class Resource { id: int; }
fn Res_new(id: int) -> Resource { return Resource { id: id }; }
fn compute(target: int) -> int {
    let outer: Resource = Res_new(999);
    let count: int = 0;
    for (let r: Resource = Res_new(10); count < 50; count += 1) {
        let inner: Resource = Res_new(r.id + count);
        if (count == target) {
            return inner.id + outer.id;
        }
    }
    return 0;
}
fn main() -> int {
    let res1: int = compute(15);
    let res2: int = compute(99);
    print(res1);
    print(res2);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c2_return.cco" -o "$TMP_DIR/c2_return.c" 2>"$TMP_DIR/c2_ret.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/c2_ret.err")"
elif ! gcc -Wall -Wextra -pedantic-errors -std=c11 "$TMP_DIR/c2_return.c" -o "$TMP_DIR/c2_return.bin" 2>"$TMP_DIR/c2_ret_c.err"; then
    fail_test "C compilation failed: $(cat "$TMP_DIR/c2_ret_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c2_return.bin" >/dev/null 2>"$TMP_DIR/c2_ret_val.err"; then
    fail_test "Valgrind error in loop early return: $(cat "$TMP_DIR/c2_ret_val.err")"
else
    pass_test
fi

# ==============================================================
# CHECK 3: Self-Hosting Compiler Differential Program Execution
# ==============================================================

# 3.1 Verify Stage 2 compiler reproduces Stage 0 on arithmetic and control flow
run_test "Check 3.1: Stage 2 self-hosted compiler differential: arithmetic & flow"
cat << 'EOF' > "$TMP_DIR/c3_prog1.cco"
fn fib(n: int) -> int {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}
fn main() -> int {
    print(fib(10));
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c3_prog1.cco" -o "$TMP_DIR/prog1_s0.c" 2>"$TMP_DIR/s0.err"; then
    fail_test "Stage 0 failed: $(cat "$TMP_DIR/s0.err")"
elif ! ./build/cco_stage2 "$TMP_DIR/c3_prog1.cco" -o "$TMP_DIR/prog1_s2.c" 2>"$TMP_DIR/s2.err"; then
    fail_test "Stage 2 failed: $(cat "$TMP_DIR/s2.err")"
elif ! gcc "$TMP_DIR/prog1_s0.c" -o "$TMP_DIR/p1_s0.bin" || ! gcc "$TMP_DIR/prog1_s2.c" -o "$TMP_DIR/p1_s2.bin"; then
    fail_test "C compilation failed for differential binaries"
else
    out_s0=$("$TMP_DIR/p1_s0.bin")
    out_s2=$("$TMP_DIR/p1_s2.bin")
    if [ "$out_s0" != "$out_s2" ]; then
        fail_test "Differential output mismatch: S0='$out_s0' vs S2='$out_s2'"
    else
        pass_test
    fi
fi

# 3.2 Verify Stage 2 compiler differential: classes and heap lifecycle
run_test "Check 3.2: Stage 2 self-hosted compiler differential: classes & cleanup"
cat << 'EOF' > "$TMP_DIR/c3_prog2.cco"
class Number { val: int; }
fn main() -> int {
    let n1 = Number { val: 10 };
    let n2 = Number { val: 20 };
    let n3 = Number { val: 30 };
    print(n1.val + n2.val + n3.val);
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c3_prog2.cco" -o "$TMP_DIR/prog2_s0.c" 2>"$TMP_DIR/s0_2.err"; then
    fail_test "Stage 0 failed: $(cat "$TMP_DIR/s0_2.err")"
elif ! ./build/cco_stage2 "$TMP_DIR/c3_prog2.cco" -o "$TMP_DIR/prog2_s2.c" 2>"$TMP_DIR/s2_2.err"; then
    fail_test "Stage 2 failed: $(cat "$TMP_DIR/s2_2.err")"
elif ! gcc "$TMP_DIR/prog2_s0.c" -o "$TMP_DIR/p2_s0.bin" || ! gcc "$TMP_DIR/prog2_s2.c" -o "$TMP_DIR/p2_s2.bin"; then
    fail_test "C compilation failed"
else
    out_s0=$("$TMP_DIR/p2_s0.bin")
    out_s2=$("$TMP_DIR/p2_s2.bin")
    if [ "$out_s0" != "$out_s2" ]; then
        fail_test "Differential output mismatch: S0='$out_s0' vs S2='$out_s2'"
    elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/p2_s2.bin" >/dev/null 2>"$TMP_DIR/val_s2.err"; then
        fail_test "Valgrind error in Stage 2 compiled binary: $(cat "$TMP_DIR/val_s2.err")"
    else
        pass_test
    fi
fi

# 3.3 Verify Stage 2 compiler differential: functions and borrowed class parameters
run_test "Check 3.3: Stage 2 self-hosted compiler differential: functions & borrowing"
cat << 'EOF' > "$TMP_DIR/c3_prog3.cco"
class Calculator {
    base: int;
}
fn calc_add(c: &Calculator, x: int) -> int {
    return c.base + x;
}
fn main() -> int {
    let c = Calculator { base: 100 };
    print(calc_add(c, 42));
    return 0;
}
EOF
if ! "$CCO" "$TMP_DIR/c3_prog3.cco" -o "$TMP_DIR/prog3_s0.c" 2>"$TMP_DIR/s0_3.err"; then
    fail_test "Stage 0 failed: $(cat "$TMP_DIR/s0_3.err")"
elif ! ./build/cco_stage2 "$TMP_DIR/c3_prog3.cco" -o "$TMP_DIR/prog3_s2.c" 2>"$TMP_DIR/s2_3.err"; then
    fail_test "Stage 2 failed: $(cat "$TMP_DIR/s2_3.err")"
elif ! gcc "$TMP_DIR/prog3_s0.c" -o "$TMP_DIR/p3_s0.bin" || ! gcc "$TMP_DIR/prog3_s2.c" -o "$TMP_DIR/p3_s2.bin"; then
    fail_test "C compilation failed"
else
    out_s0=$("$TMP_DIR/p3_s0.bin")
    out_s2=$("$TMP_DIR/p3_s2.bin")
    if [ "$out_s0" != "$out_s2" ]; then
        fail_test "Differential output mismatch: S0='$out_s0' vs S2='$out_s2'"
    elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/p3_s2.bin" >/dev/null 2>"$TMP_DIR/val_s3.err"; then
        fail_test "Valgrind error in Stage 2 compiled binary: $(cat "$TMP_DIR/val_s3.err")"
    else
        pass_test
    fi
fi

echo "=============================================================="
echo "Three Checks Audit Suite: $passed_count / $total_count Passed (100% Success)"
echo "=============================================================="