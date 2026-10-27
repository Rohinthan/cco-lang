#!/usr/bin/env bash
# ==============================================================================
# tests/test_recursive_type_descriptors.sh
# Cco Milestone 10: Recursive Type Descriptor Architecture
# Comprehensive Verification & Adversarial Memory Safety Test Suite
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_m10_XXXXXX)

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
    (cd "$REPO_ROOT" && make cco)
fi

echo "=============================================================="
echo "Cco Milestone 10: Recursive Type Descriptor Suite"
echo "=============================================================="

# ==============================================================================
# GROUP A: Parsing of Recursive Container Types
# ==============================================================================

# A1: map[string]int[]
run_test "Group A1: Parse map[string]int[]"
cat << 'EOF' > "$TMP_DIR/a1.cco"
fn main() -> int {
    let m: map[string]int[] = map_new(string, int[]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/a1.cco" -o "$TMP_DIR/a1.c" 2>"$TMP_DIR/a1.err"; then
    pass_test
else
    fail_test "Failed to parse map[string]int[]: $(cat "$TMP_DIR/a1.err")"
fi

# A2: (map[string]int)[]
run_test "Group A2: Parse (map[string]int)[]"
cat << 'EOF' > "$TMP_DIR/a2.cco"
fn main() -> int {
    let arr: (map[string]int)[] = list_new(map[string]int);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/a2.cco" -o "$TMP_DIR/a2.c" 2>"$TMP_DIR/a2.err"; then
    pass_test
else
    fail_test "Failed to parse (map[string]int)[]: $(cat "$TMP_DIR/a2.err")"
fi

# A3: map[string]map[string]int
run_test "Group A3: Parse map[string]map[string]int"
cat << 'EOF' > "$TMP_DIR/a3.cco"
fn main() -> int {
    let m: map[string]map[string]int = map_new(string, map[string]int);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/a3.cco" -o "$TMP_DIR/a3.c" 2>"$TMP_DIR/a3.err"; then
    pass_test
else
    fail_test "Failed to parse map[string]map[string]int: $(cat "$TMP_DIR/a3.err")"
fi

# A4: int[][] (2D array)
run_test "Group A4: Parse int[][]"
cat << 'EOF' > "$TMP_DIR/a4.cco"
fn main() -> int {
    let matrix: int[][] = list_new(int[]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/a4.cco" -o "$TMP_DIR/a4.c" 2>"$TMP_DIR/a4.err"; then
    pass_test
else
    fail_test "Failed to parse int[][]: $(cat "$TMP_DIR/a4.err")"
fi

# A5: map[string]string[]
run_test "Group A5: Parse map[string]string[]"
cat << 'EOF' > "$TMP_DIR/a5.cco"
fn main() -> int {
    let m: map[string]string[] = map_new(string, string[]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/a5.cco" -o "$TMP_DIR/a5.c" 2>"$TMP_DIR/a5.err"; then
    pass_test
else
    fail_test "Failed to parse map[string]string[]: $(cat "$TMP_DIR/a5.err")"
fi

# A6: map[string]Resource[]
run_test "Group A6: Parse map[string]Resource[]"
cat << 'EOF' > "$TMP_DIR/a6.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource[] = map_new(string, Resource[]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/a6.cco" -o "$TMP_DIR/a6.c" 2>"$TMP_DIR/a6.err"; then
    pass_test
else
    fail_test "Failed to parse map[string]Resource[]: $(cat "$TMP_DIR/a6.err")"
fi

# A7: map[string]map[string]Resource
run_test "Group A7: Parse map[string]map[string]Resource"
cat << 'EOF' > "$TMP_DIR/a7.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]map[string]Resource = map_new(string, map[string]Resource);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/a7.cco" -o "$TMP_DIR/a7.c" 2>"$TMP_DIR/a7.err"; then
    pass_test
else
    fail_test "Failed to parse map[string]map[string]Resource: $(cat "$TMP_DIR/a7.err")"
fi

# ==============================================================================
# GROUP B: Recursive Type Checking Diagnostics
# ==============================================================================

# B1: Reject assigning int[][] to int[]
run_test "Group B1: Diagnostic rejection of int[][] to int[]"
cat << 'EOF' > "$TMP_DIR/b1.cco"
fn main() -> int {
    let matrix: int[][] = list_new(int[]);
    let flat: int[] = matrix;
    return 0;
}
EOF
set +e
"$CCO" "$TMP_DIR/b1.cco" -o "$TMP_DIR/b1.c" 2>"$TMP_DIR/b1.err"
exit_code=$?
set -e
if [ "$exit_code" -ne 0 ] && grep -qi "type mismatch" "$TMP_DIR/b1.err"; then
    pass_test
else
    fail_test "Expected type mismatch error, got exit code $exit_code: $(cat "$TMP_DIR/b1.err")"
fi

# B2: Reject assigning map[string]int[] to map[string]int
run_test "Group B2: Diagnostic rejection of map[string]int[] to map[string]int"
cat << 'EOF' > "$TMP_DIR/b2.cco"
fn main() -> int {
    let m: map[string]int[] = map_new(string, int[]);
    let m_flat: map[string]int = m;
    return 0;
}
EOF
set +e
"$CCO" "$TMP_DIR/b2.cco" -o "$TMP_DIR/b2.c" 2>"$TMP_DIR/b2.err"
exit_code=$?
set -e
if [ "$exit_code" -ne 0 ] && grep -qi "type mismatch" "$TMP_DIR/b2.err"; then
    pass_test
else
    fail_test "Expected type mismatch error, got exit code $exit_code: $(cat "$TMP_DIR/b2.err")"
fi

# B3: Reject returning map[string]map[string]int from function declared as map[string]int
run_test "Group B3: Diagnostic rejection of mismatched return type descriptor"
cat << 'EOF' > "$TMP_DIR/b3.cco"
fn bad_return() -> map[string]int {
    let nested: map[string]map[string]int = map_new(string, map[string]int);
    return nested;
}
fn main() -> int {
    return 0;
}
EOF
set +e
"$CCO" "$TMP_DIR/b3.cco" -o "$TMP_DIR/b3.c" 2>"$TMP_DIR/b3.err"
exit_code=$?
set -e
if [ "$exit_code" -ne 0 ] && grep -qi "return type mismatch" "$TMP_DIR/b3.err"; then
    pass_test
else
    fail_test "Expected return type mismatch error, got exit code $exit_code: $(cat "$TMP_DIR/b3.err")"
fi

# B4: Reject returning int[][] from function declared as int[]
run_test "Group B4: Diagnostic rejection of 2D array return from 1D function"
cat << 'EOF' > "$TMP_DIR/b4.cco"
fn get_matrix() -> int[] {
    let m: int[][] = list_new(int[]);
    return m;
}
fn main() -> int {
    return 0;
}
EOF
set +e
"$CCO" "$TMP_DIR/b4.cco" -o "$TMP_DIR/b4.c" 2>"$TMP_DIR/b4.err"
exit_code=$?
set -e
if [ "$exit_code" -ne 0 ] && grep -qi "return type mismatch" "$TMP_DIR/b4.err"; then
    pass_test
else
    fail_test "Expected return type mismatch error, got exit code $exit_code: $(cat "$TMP_DIR/b4.err")"
fi

# ==============================================================================
# GROUP C: Valgrind Memory Safety Verification (0 Leaks, 0 Errors)
# ==============================================================================

# C1: map[string]int[] under Valgrind
run_test "Group C1: map[string]int[] lifecycle (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c1.cco"
fn main() -> int {
    let m: map[string]int[] = map_new(string, int[]);
    let a1: int[] = list_new(int);
    a1 = push(a1, 100);
    a1 = push(a1, 200);
    m = put(m, "even", a1);

    let a2: int[] = list_new(int);
    a2 = push(a2, 101);
    m = put(m, "odd", a2);

    let q: int[] = get(m, "even");
    print(q[0]);
    print(q[1]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c1.cco" -o "$TMP_DIR/c1.c" 2>"$TMP_DIR/c1.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c1.c" -o "$TMP_DIR/c1" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c1" > "$TMP_DIR/c1.out" 2>"$TMP_DIR/c1_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c1_val.err")"
fi

# C2: (map[string]int)[] under Valgrind
run_test "Group C2: (map[string]int)[] lifecycle (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c2.cco"
fn main() -> int {
    let arr: (map[string]int)[] = list_new(map[string]int);
    let m1: map[string]int = map_new(string, int);
    m1 = put(m1, "alpha", 11);
    arr = push(arr, m1);

    let m2: map[string]int = map_new(string, int);
    m2 = put(m2, "beta", 22);
    arr = push(arr, m2);

    let r0: map[string]int = arr[0];
    print(get(r0, "alpha"));
    let r1: map[string]int = arr[1];
    print(get(r1, "beta"));
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c2.cco" -o "$TMP_DIR/c2.c" 2>"$TMP_DIR/c2.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c2.c" -o "$TMP_DIR/c2" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c2" > "$TMP_DIR/c2.out" 2>"$TMP_DIR/c2_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c2_val.err")"
fi

# C3: map[string]map[string]int under Valgrind
run_test "Group C3: map[string]map[string]int lifecycle (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c3.cco"
fn main() -> int {
    let outer: map[string]map[string]int = map_new(string, map[string]int);
    let inner1: map[string]int = map_new(string, int);
    inner1 = put(inner1, "x", 42);
    outer = put(outer, "layer1", inner1);

    let inner2: map[string]int = map_new(string, int);
    inner2 = put(inner2, "y", 84);
    outer = put(outer, "layer2", inner2);

    let sub: map[string]int = get(outer, "layer1");
    print(get(sub, "x"));
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c3.cco" -o "$TMP_DIR/c3.c" 2>"$TMP_DIR/c3.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c3.c" -o "$TMP_DIR/c3" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c3" > "$TMP_DIR/c3.out" 2>"$TMP_DIR/c3_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c3_val.err")"
fi

# C4: int[][] (2D array) under Valgrind
run_test "Group C4: int[][] 2D array lifecycle (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c4.cco"
fn main() -> int {
    let matrix: int[][] = list_new(int[]);
    let r0: int[] = list_new(int);
    r0 = push(r0, 10);
    r0 = push(r0, 20);
    matrix = push(matrix, r0);

    let r1: int[] = list_new(int);
    r1 = push(r1, 30);
    r1 = push(r1, 40);
    matrix = push(matrix, r1);

    let row: int[] = matrix[1];
    print(row[0]);
    print(row[1]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c4.cco" -o "$TMP_DIR/c4.c" 2>"$TMP_DIR/c4.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c4.c" -o "$TMP_DIR/c4" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c4" > "$TMP_DIR/c4.out" 2>"$TMP_DIR/c4_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c4_val.err")"
fi

# C5: map[string]string[] under Valgrind
run_test "Group C5: map[string]string[] recursive string cleanup (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c5.cco"
fn main() -> int {
    let m: map[string]string[] = map_new(string, string[]);
    let words: string[] = list_new(string);
    words = push(words, concat("apple", "_pie"));
    words = push(words, concat("banana", "_split"));
    m = put(m, "desserts", words);

    let got: string[] = get(m, "desserts");
    print(got[0]);
    print(got[1]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c5.cco" -o "$TMP_DIR/c5.c" 2>"$TMP_DIR/c5.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c5.c" -o "$TMP_DIR/c5" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c5" > "$TMP_DIR/c5.out" 2>"$TMP_DIR/c5_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c5_val.err")"
fi

# C6: map[string]Resource[] with class destructors under Valgrind
run_test "Group C6: map[string]Resource[] class array cleanup (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c6.cco"
class Item {
    tag: string;
    value: int;
}
fn main() -> int {
    let m: map[string]Item[] = map_new(string, Item[]);
    let items: Item[] = list_new(Item);
    items = push(items, Item { tag: "alpha", value: 1 });
    items = push(items, Item { tag: "omega", value: 99 });
    m = put(m, "greek", items);

    let query: Item[] = get(m, "greek");
    print(query[0].tag);
    print(query[1].value);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c6.cco" -o "$TMP_DIR/c6.c" 2>"$TMP_DIR/c6.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c6.c" -o "$TMP_DIR/c6" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c6" > "$TMP_DIR/c6.out" 2>"$TMP_DIR/c6_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c6_val.err")"
fi

# C7: map[string]Payload with enum variants under Valgrind
run_test "Group C7: map[string]Payload enum value cleanup (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c7.cco"
enum Payload {
    Text { msg: string },
    Number { n: int }
}
fn main() -> int {
    let m: map[string]Payload = map_new(string, Payload);
    m = put(m, "p1", Payload.Text { msg: "hello_enum" });
    m = put(m, "p2", Payload.Number { n: 777 });

    let p: Payload = get(m, "p1");
    match p {
        Payload.Text { msg } => { print(msg); }
        Payload.Number { n } => { print(n); }
    }
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c7.cco" -o "$TMP_DIR/c7.c" 2>"$TMP_DIR/c7.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c7.c" -o "$TMP_DIR/c7" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c7" > "$TMP_DIR/c7.out" 2>"$TMP_DIR/c7_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c7_val.err")"
fi

# C8: Early return unwinding holding nested containers under Valgrind
run_test "Group C8: Early return unwinding with nested map (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/c8.cco"
fn helper(flag: bool) -> int {
    let m: map[string]int[] = map_new(string, int[]);
    let a: int[] = list_new(int);
    a = push(a, 12345);
    m = put(m, "key", a);
    if (flag) {
        return 1;
    }
    return 0;
}
fn main() -> int {
    let r1: int = helper(true);
    let r2: int = helper(false);
    print(r1 + r2);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/c8.cco" -o "$TMP_DIR/c8.c" 2>"$TMP_DIR/c8.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/c8.c" -o "$TMP_DIR/c8" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/c8" > "$TMP_DIR/c8.out" 2>"$TMP_DIR/c8_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/c8_val.err")"
fi

# ==============================================================================
# GROUP D: Ownership and Move Semantics
# ==============================================================================

# D1: Moving nested container into function call -> use after move rejected
run_test "Group D1: Rejection of use-after-move for nested container"
cat << 'EOF' > "$TMP_DIR/d1.cco"
fn consume(m: map[string]int[]) -> void {
    print(len(m));
}
fn main() -> int {
    let m: map[string]int[] = map_new(string, int[]);
    consume(m);
    print(len(m));
    return 0;
}
EOF
set +e
"$CCO" "$TMP_DIR/d1.cco" -o "$TMP_DIR/d1.c" 2>"$TMP_DIR/d1.err"
exit_code=$?
set -e
if [ "$exit_code" -ne 0 ] && grep -qi "moved" "$TMP_DIR/d1.err"; then
    pass_test
else
    fail_test "Expected use-after-move error, got exit code $exit_code: $(cat "$TMP_DIR/d1.err")"
fi

# D2: Borrowing nested container across calls (Valgrind clean)
run_test "Group D2: Borrowing nested container (&m) across multiple calls"
cat << 'EOF' > "$TMP_DIR/d2.cco"
fn inspect(m: &map[string]int[]) -> int {
    return len(m);
}
fn main() -> int {
    let m: map[string]int[] = map_new(string, int[]);
    let a: int[] = list_new(int);
    a = push(a, 99);
    m = put(m, "k", a);
    let l1: int = inspect(&m);
    let l2: int = inspect(&m);
    print(l1 + l2);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/d2.cco" -o "$TMP_DIR/d2.c" 2>"$TMP_DIR/d2.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/d2.c" -o "$TMP_DIR/d2" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/d2" > "$TMP_DIR/d2.out" 2>"$TMP_DIR/d2_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/d2_val.err")"
fi

# ==============================================================================
# GROUP E: Array Reassignment & Aliasing Stress
# ==============================================================================

# E1: Array self-assignment arr = arr (no double free or leak)
run_test "Group E1: Array self-assignment arr = arr (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/e1.cco"
fn main() -> int {
    let arr: int[] = list_new(int);
    arr = push(arr, 1);
    arr = push(arr, 2);
    arr = arr;
    print(arr[0]);
    print(arr[1]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/e1.cco" -o "$TMP_DIR/e1.c" 2>"$TMP_DIR/e1.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/e1.c" -o "$TMP_DIR/e1" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/e1" > "$TMP_DIR/e1.out" 2>"$TMP_DIR/e1_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/e1_val.err")"
fi

# E2: Array reassignment frees old buffer and adopts new
run_test "Group E2: Array reassignment buffer lifecycle (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/e2.cco"
fn main() -> int {
    let arr: int[] = list_new(int);
    arr = push(arr, 10);
    let arr2: int[] = list_new(int);
    arr2 = push(arr2, 20);
    arr = arr2;
    print(arr[0]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/e2.cco" -o "$TMP_DIR/e2.c" 2>"$TMP_DIR/e2.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/e2.c" -o "$TMP_DIR/e2" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/e2" > "$TMP_DIR/e2.out" 2>"$TMP_DIR/e2_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/e2_val.err")"
fi

# E3: Map self-assignment m = m (no double free or leak)
run_test "Group E3: Map self-assignment m = m (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/e3.cco"
fn main() -> int {
    let m: map[string]int = map_new(string, int);
    m = put(m, "x", 555);
    m = m;
    print(get(m, "x"));
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/e3.cco" -o "$TMP_DIR/e3.c" 2>"$TMP_DIR/e3.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/e3.c" -o "$TMP_DIR/e3" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/e3" > "$TMP_DIR/e3.out" 2>"$TMP_DIR/e3_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/e3_val.err")"
fi

# E4: Repeated array reassignment inside loop (Valgrind clean)
run_test "Group E4: Repeated array growth inside loop (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/e4.cco"
fn main() -> int {
    let arr: int[] = list_new(int);
    let i: int = 0;
    while (i < 50) {
        arr = push(arr, i);
        i = i + 1;
    }
    print(len(arr));
    print(arr[49]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/e4.cco" -o "$TMP_DIR/e4.c" 2>"$TMP_DIR/e4.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/e4.c" -o "$TMP_DIR/e4" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/e4" > "$TMP_DIR/e4.out" 2>"$TMP_DIR/e4_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/e4_val.err")"
fi

# E5: Multi-level nested container reassignment and replacement (Valgrind clean)
run_test "Group E5: Nested container replacement under Valgrind (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/e5.cco"
fn main() -> int {
    let m: map[string]int[] = map_new(string, int[]);
    let a1: int[] = list_new(int);
    a1 = push(a1, 1);
    m = put(m, "key", a1);

    // Overwrite "key" with a different array
    let a2: int[] = list_new(int);
    a2 = push(a2, 2);
    a2 = push(a2, 3);
    m = put(m, "key", a2);

    let res: int[] = get(m, "key");
    print(res[0]);
    print(res[1]);
    return 0;
}
EOF
if "$CCO" "$TMP_DIR/e5.cco" -o "$TMP_DIR/e5.c" 2>"$TMP_DIR/e5.err" && \
   gcc -Wall -Wextra -std=c11 -I"$REPO_ROOT/src" "$TMP_DIR/e5.c" -o "$TMP_DIR/e5" -lm && \
   valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/e5" > "$TMP_DIR/e5.out" 2>"$TMP_DIR/e5_val.err"; then
    pass_test
else
    fail_test "Execution failed or Valgrind reported error: $(cat "$TMP_DIR/e5_val.err")"
fi

echo "=============================================================="
echo "Milestone 10 Suite Results: $passed_tests / $total_tests Passed"
echo "=============================================================="

if [ "$failed_tests" -gt 0 ]; then
    exit 1
fi
exit 0
