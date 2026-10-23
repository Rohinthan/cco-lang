#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18C — Milestone 2: RingDeque Test Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_phase18c_deque_tests_$$"

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
echo "Cco Phase 18C Milestone 2: RingDeque Test Suite"
echo "=============================================================="

# Test 1: Empty initialization and state queries
echo "Test 1: Empty initialization and state queries..."
cat << 'EOF' > "$TMP_DIR/test1_empty.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(5);
    if (!q.is_empty()) { return 1; }
    if (q.is_full()) { return 2; }
    if (q.count() != 0) { return 3; }
    if (q.peek_front() != -1) { return 4; }
    if (q.peek_back() != -1) { return 5; }
    if (q.pop_front() != -1) { return 6; }
    if (q.pop_back() != -1) { return 7; }
    if (q.count() != 0) { return 8; }
    print("Empty initialization OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test1_empty.cco" -o "$TMP_DIR/test1_empty"
OUT=$("$TMP_DIR/test1_empty")
if [[ "$OUT" == "Empty initialization OK" ]]; then
    pass "Empty deque state queries and empty removals return expected sentinels"
else
    fail "Empty deque test" "Output was: $OUT"
fi

# Test 2: FIFO Queue behavior (push_back / pop_front)
echo "Test 2: FIFO Queue behavior (push_back / pop_front)..."
cat << 'EOF' > "$TMP_DIR/test2_fifo.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(4);
    if (!q.push_back(10)) { return 1; }
    if (!q.push_back(20)) { return 2; }
    if (!q.push_back(30)) { return 3; }

    if (q.count() != 3) { return 4; }
    if (q.peek_front() != 10) { return 5; }
    if (q.peek_back() != 30) { return 6; }

    if (q.pop_front() != 10) { return 7; }
    if (q.pop_front() != 20) { return 8; }
    if (q.pop_front() != 30) { return 9; }

    if (!q.is_empty()) { return 10; }
    if (q.count() != 0) { return 11; }
    print("FIFO Queue OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test2_fifo.cco" -o "$TMP_DIR/test2_fifo"
OUT=$("$TMP_DIR/test2_fifo")
if [[ "$OUT" == "FIFO Queue OK" ]]; then
    pass "FIFO queue operations succeed in strict first-in-first-out order"
else
    fail "FIFO queue test" "Output was: $OUT"
fi

# Test 3: LIFO Stack behavior (push_back / pop_back)
echo "Test 3: LIFO Stack behavior (push_back / pop_back)..."
cat << 'EOF' > "$TMP_DIR/test3_lifo.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(4);
    if (!q.push_back(100)) { return 1; }
    if (!q.push_back(200)) { return 2; }
    if (!q.push_back(300)) { return 3; }

    if (q.pop_back() != 300) { return 4; }
    if (q.pop_back() != 200) { return 5; }
    if (q.pop_back() != 100) { return 6; }

    if (!q.is_empty()) { return 7; }
    print("LIFO Stack OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test3_lifo.cco" -o "$TMP_DIR/test3_lifo"
OUT=$("$TMP_DIR/test3_lifo")
if [[ "$OUT" == "LIFO Stack OK" ]]; then
    pass "LIFO stack operations succeed in strict last-in-first-out order"
else
    fail "LIFO stack test" "Output was: $OUT"
fi

# Test 4: Front operations and mixed double-ended access
echo "Test 4: Front operations and mixed double-ended access..."
cat << 'EOF' > "$TMP_DIR/test4_double_ended.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(6);
    // Push front 1, then push front 2 -> front is 2, then 1
    if (!q.push_front(1)) { return 1; }
    if (!q.push_front(2)) { return 2; }
    // Push back 3, 4 -> order is 2, 1, 3, 4
    if (!q.push_back(3)) { return 3; }
    if (!q.push_back(4)) { return 4; }

    if (q.count() != 4) { return 5; }
    if (q.peek_front() != 2) { return 6; }
    if (q.peek_back() != 4) { return 7; }

    // Pop from front: 2
    if (q.pop_front() != 2) { return 8; }
    // Pop from back: 4
    if (q.pop_back() != 4) { return 9; }
    // Remaining: 1, 3
    if (q.pop_front() != 1) { return 10; }
    if (q.pop_back() != 3) { return 11; }

    if (!q.is_empty()) { return 12; }
    print("Double-ended operations OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test4_double_ended.cco" -o "$TMP_DIR/test4_double_ended"
OUT=$("$TMP_DIR/test4_double_ended")
if [[ "$OUT" == "Double-ended operations OK" ]]; then
    pass "Double-ended pushes and pops interact correctly from both ends"
else
    fail "Double-ended test" "Output was: $OUT"
fi

# Test 5: Circular buffer wraparound across boundaries
echo "Test 5: Circular buffer wraparound across boundaries..."
cat << 'EOF' > "$TMP_DIR/test5_wraparound.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(4);

    // Fill completely: [1, 2, 3, 4]
    q.push_back(1);
    q.push_back(2);
    q.push_back(3);
    q.push_back(4);
    if (!q.is_full()) { return 1; }

    // Pop 2 from front: [3, 4]
    if (q.pop_front() != 1) { return 2; }
    if (q.pop_front() != 2) { return 3; }
    if (q.count() != 2) { return 4; }

    // Push 2 more (wraps tail back around to indices 0, 1): [3, 4, 5, 6]
    if (!q.push_back(5)) { return 5; }
    if (!q.push_back(6)) { return 6; }
    if (!q.is_full()) { return 7; }

    // Pop all 4 in FIFO order
    if (q.pop_front() != 3) { return 8; }
    if (q.pop_front() != 4) { return 9; }
    if (q.pop_front() != 5) { return 10; }
    if (q.pop_front() != 6) { return 11; }
    if (!q.is_empty()) { return 12; }

    // Test head wraparound via push_front:
    // Starting at head=0, push_front(7) wraps head to capacity-1 (index 3)
    if (!q.push_front(7)) { return 13; }
    if (!q.push_front(8)) { return 14; }
    if (q.peek_front() != 8) { return 15; }
    if (q.peek_back() != 7) { return 16; }
    if (q.pop_front() != 8) { return 17; }
    if (q.pop_front() != 7) { return 18; }
    if (!q.is_empty()) { return 19; }

    print("Circular wraparound OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test5_wraparound.cco" -o "$TMP_DIR/test5_wraparound"
OUT=$("$TMP_DIR/test5_wraparound")
if [[ "$OUT" == "Circular wraparound OK" ]]; then
    pass "Circular buffer wraps around head and tail boundaries correctly"
else
    fail "Circular wraparound test" "Output was: $OUT"
fi

# Test 6: Full capacity rejection
echo "Test 6: Full capacity rejection..."
cat << 'EOF' > "$TMP_DIR/test6_full.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(3);
    if (!q.push_back(10)) { return 1; }
    if (!q.push_back(20)) { return 2; }
    if (!q.push_back(30)) { return 3; }

    if (!q.is_full()) { return 4; }
    if (q.push_back(40)) { return 5; } // Must reject push_back when full
    if (q.push_front(50)) { return 6; } // Must reject push_front when full
    if (q.count() != 3) { return 7; }

    // Free one slot
    if (q.pop_front() != 10) { return 8; }
    if (q.is_full()) { return 9; }

    // Now push_front must succeed
    if (!q.push_front(99)) { return 10; }
    if (!q.is_full()) { return 11; }
    if (q.peek_front() != 99) { return 12; }

    print("Full capacity rejection OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test6_full.cco" -o "$TMP_DIR/test6_full"
OUT=$("$TMP_DIR/test6_full")
if [[ "$OUT" == "Full capacity rejection OK" ]]; then
    pass "Full capacity prevents overflow and recovers after pop"
else
    fail "Full capacity rejection test" "Output was: $OUT"
fi

# Test 7: Clear and reuse
echo "Test 7: Clear and reuse..."
cat << 'EOF' > "$TMP_DIR/test7_clear.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(4);
    q.push_back(1);
    q.push_back(2);
    q.push_back(3);

    q.clear();
    if (!q.is_empty()) { return 1; }
    if (q.is_full()) { return 2; }
    if (q.count() != 0) { return 3; }
    if (q.pop_front() != -1) { return 4; }

    // Reuse deque after clear
    if (!q.push_front(42)) { return 5; }
    if (q.count() != 1) { return 6; }
    if (q.peek_front() != 42) { return 7; }
    if (q.pop_back() != 42) { return 8; }
    if (!q.is_empty()) { return 9; }

    print("Clear and reuse OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test7_clear.cco" -o "$TMP_DIR/test7_clear"
OUT=$("$TMP_DIR/test7_clear")
if [[ "$OUT" == "Clear and reuse OK" ]]; then
    pass "Clear resets deque to empty state and allows clean reuse"
else
    fail "Clear and reuse test" "Output was: $OUT"
fi

# Test 8: Capacity 1 edge case
echo "Test 8: Capacity 1 edge case..."
cat << 'EOF' > "$TMP_DIR/test8_cap1.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(1);
    if (!q.is_empty()) { return 1; }
    if (q.is_full()) { return 2; }
    if (q.count() != 0) { return 3; }

    if (!q.push_back(123)) { return 4; }
    if (!q.is_full()) { return 5; }
    if (q.is_empty()) { return 6; }
    if (q.count() != 1) { return 7; }

    // Next push must be rejected
    if (q.push_back(456)) { return 8; }
    if (q.push_front(789)) { return 9; }

    if (q.peek_front() != 123) { return 10; }
    if (q.peek_back() != 123) { return 11; }
    if (q.pop_front() != 123) { return 12; }

    if (!q.is_empty()) { return 13; }

    // Reuse with push_front
    if (!q.push_front(999)) { return 14; }
    if (q.pop_back() != 999) { return 15; }
    if (!q.is_empty()) { return 16; }

    print("Capacity 1 OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test8_cap1.cco" -o "$TMP_DIR/test8_cap1"
OUT=$("$TMP_DIR/test8_cap1")
if [[ "$OUT" == "Capacity 1 OK" ]]; then
    pass "Capacity 1 single-element deque behaves correctly on boundary"
else
    fail "Capacity 1 test" "Output was: $OUT"
fi

# Test 9: Non-positive capacity safe clamping
echo "Test 9: Non-positive capacity safe clamping..."
cat << 'EOF' > "$TMP_DIR/test9_clamping.cco"
import "std/deque.cco";

fn main() -> int {
    // Capacity 0 safely clamped to 1
    let q0: RingDeque = deque_new(0);
    if (!q0.is_empty()) { return 1; }
    if (q0.is_full()) { return 2; }
    if (!q0.push_back(10)) { return 3; }
    if (q0.push_back(20)) { return 4; }
    if (q0.pop_front() != 10) { return 5; }

    // Capacity -5 safely clamped to 1
    let qneg: RingDeque = deque_new(-5);
    if (!qneg.is_empty()) { return 6; }
    if (!qneg.push_front(30)) { return 7; }
    if (qneg.push_front(40)) { return 8; }
    if (qneg.pop_back() != 30) { return 9; }

    print("Non-positive capacity clamping OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test9_clamping.cco" -o "$TMP_DIR/test9_clamping"
OUT=$("$TMP_DIR/test9_clamping")
if [[ "$OUT" == "Non-positive capacity clamping OK" ]]; then
    pass "Zero and negative capacities safely clamp to 1 without arithmetic exceptions"
else
    fail "Capacity clamping test" "Output was: $OUT"
fi

# Test 10: OptionInt methods resolving sentinel ambiguity
echo "Test 10: OptionInt methods resolving sentinel ambiguity..."
cat << 'EOF' > "$TMP_DIR/test10_option.cco"
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(4);

    // Empty checks with OptionInt
    let empty_front: OptionInt = q.peek_front_opt();
    let empty_back: OptionInt = q.peek_back_opt();
    let empty_pop_f: OptionInt = q.pop_front_opt();
    let empty_pop_b: OptionInt = q.pop_back_opt();

    if (opt_int_is_some(&empty_front)) { return 1; }
    if (opt_int_is_some(&empty_back)) { return 2; }
    if (opt_int_is_some(&empty_pop_f)) { return 3; }
    if (opt_int_is_some(&empty_pop_b)) { return 4; }

    // Store -1 and 0 (which are ambiguous when using sentinel values)
    q.push_back(-1);
    q.push_back(0);
    q.push_back(42);

    let p_front: OptionInt = q.peek_front_opt();
    if (!opt_int_is_some(&p_front)) { return 5; }
    if (opt_int_unwrap_or(&p_front, 999) != -1) { return 6; }

    let p_back: OptionInt = q.peek_back_opt();
    if (!opt_int_is_some(&p_back)) { return 7; }
    if (opt_int_unwrap_or(&p_back, 999) != 42) { return 8; }

    // Pop -1 using pop_front_opt
    let v1: OptionInt = q.pop_front_opt();
    if (!opt_int_is_some(&v1)) { return 9; }
    if (opt_int_unwrap_or(&v1, 999) != -1) { return 10; }

    // Pop 42 using pop_back_opt
    let v2: OptionInt = q.pop_back_opt();
    if (!opt_int_is_some(&v2)) { return 11; }
    if (opt_int_unwrap_or(&v2, 999) != 42) { return 12; }

    // Pop 0 using pop_front_opt
    let v3: OptionInt = q.pop_front_opt();
    if (!opt_int_is_some(&v3)) { return 13; }
    if (opt_int_unwrap_or(&v3, 999) != 0) { return 14; }

    // Now empty again
    let v_end: OptionInt = q.pop_front_opt();
    if (opt_int_is_some(&v_end)) { return 15; }

    print("OptionInt disambiguation OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test10_option.cco" -o "$TMP_DIR/test10_option"
OUT=$("$TMP_DIR/test10_option")
if [[ "$OUT" == "OptionInt disambiguation OK" ]]; then
    pass "OptionInt methods distinguish -1/0 payload values from empty sentinel results"
else
    fail "OptionInt disambiguation test" "Output was: $OUT"
fi

# Test 11: Valgrind memory leak verification
echo "Test 11: Valgrind memory leak verification across all RingDeque test binaries..."
VALG_FAIL=0
for bin in "$TMP_DIR"/test[0-9]*; do
    if [[ -x "$bin" ]]; then
        if valgrind --leak-check=full --error-exitcode=1 "$bin" > /dev/null 2>&1; then
            :
        else
            VALG_FAIL=$((VALG_FAIL + 1))
            fail "Valgrind memcheck" "$(basename "$bin") leaked memory or raised errors"
        fi
    fi
done
if [[ $VALG_FAIL -eq 0 ]]; then
    pass "All RingDeque test binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# Test 12: Negative Boundary — Clean rejection on native backend
echo "Test 12: Negative boundary — compile-time rejection by native backend..."
SELFHOST_CCO="$ROOT_DIR/selfhost/cco"
if [[ -x "$SELFHOST_CCO" ]]; then
    set +e
    NATIVE_ERR=$(cd "$ROOT_DIR" && "$SELFHOST_CCO" "$TMP_DIR/test1_empty.cco" --selfhost-native -o "$TMP_DIR/native_bin" 2>&1)
    NATIVE_CODE=$?
    set -e
    if [[ $NATIVE_CODE -eq 0 ]]; then
        fail "Native backend boundary" "Expected native backend to reject class RingDeque, but compilation succeeded"
    else
        if [[ "$NATIVE_ERR" == *"does not support class definitions"* ]]; then
            pass "Native backend cleanly rejected class RingDeque with informative diagnostic"
        else
            fail "Native backend diagnostic" "Unexpected error message: $NATIVE_ERR"
        fi
    fi
else
    echo "  (selfhost/cco binary not built, skipping native rejection check)"
fi

echo "=============================================================="
echo "Phase 18C Milestone 2 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
