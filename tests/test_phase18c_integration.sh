#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18C — Milestone 4: Final Integration Test Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
STAGE2="$ROOT_DIR/build/cco_stage2"
SELFHOST_CCO="$ROOT_DIR/selfhost/cco"
TMP_DIR="/tmp/cco_phase18c_integration_tests_$$"

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
echo "Cco Phase 18C Milestone 4: Final Integration Test Suite"
echo "=============================================================="

# Test 1: Full pipeline: ByteBuffer serialization -> Result parsing -> RingDeque work-queue -> Option consumption
echo "Test 1: Full pipeline (ByteBuffer -> Result -> RingDeque -> Option)..."
cat << 'EOF' > "$TMP_DIR/test1_pipeline.cco"
import "std/option.cco";
import "std/result.cco";
import "std/deque.cco";
import "std/buffer.cco";

fn serialize_packet(buf: &ByteBuffer, msg_id: int, payload_val: int) -> bool {
    // Header: Magic 0xBE, 0xEF (190, 239), MsgID (u16), Payload (u32)
    if (!buf.write_u8(190)) { return false; }
    if (!buf.write_u8(239)) { return false; }
    if (!buf.write_u16_be(msg_id)) { return false; }
    if (!buf.write_u32_be(payload_val)) { return false; }
    return true;
}

fn parse_packet(buf: &ByteBuffer) -> ResultInt {
    if (buf.readable_bytes() < 8) {
        return err_int("packet truncated");
    }
    let m1: int = buf.read_u8();
    let m2: int = buf.read_u8();
    if (m1 != 190 || m2 != 239) {
        return err_int("invalid magic header");
    }
    let msg_id: int = buf.read_u16_be();
    if (msg_id <= 0) {
        return err_int("invalid msg_id");
    }
    let payload: int = buf.read_u32_be();
    // Return combined payload
    return ok_int(payload);
}

fn main() -> int {
    let wire: ByteBuffer = byte_buffer_new(32);
    let queue: RingDeque = deque_new(4);

    // 1. Serialize 3 packets into wire buffer
    if (!serialize_packet(&wire, 1, 100)) { return 1; }
    if (!serialize_packet(&wire, 2, 200)) { return 2; }
    if (!serialize_packet(&wire, 3, 300)) { return 3; }

    // 2. Parse packets from wire buffer and enqueue valid payloads into RingDeque
    for (let i: int = 0; i < 3; i++) {
        let res: ResultInt = parse_packet(&wire);
        if (!res_int_is_ok(&res)) { return 4; }
        let payload: int = res_int_unwrap_or(&res, 0);
        if (!queue.push_back(payload)) { return 5; }
    }

    if (queue.count() != 3) { return 6; }
    if (wire.readable_bytes() != 0) { return 7; }

    // 3. Worker dequeues and consumes tasks using OptionInt
    let task1: OptionInt = queue.pop_front_opt();
    if (!opt_int_is_some(&task1)) { return 8; }
    if (opt_int_unwrap_or(&task1, 0) != 100) { return 9; }

    let task2: OptionInt = queue.pop_front_opt();
    if (!opt_int_is_some(&task2)) { return 10; }
    if (opt_int_unwrap_or(&task2, 0) != 200) { return 11; }

    let task3: OptionInt = queue.pop_front_opt();
    if (!opt_int_is_some(&task3)) { return 12; }
    if (opt_int_unwrap_or(&task3, 0) != 300) { return 13; }

    let empty: OptionInt = queue.pop_front_opt();
    if (opt_int_is_some(&empty)) { return 14; }

    print("Pipeline integration OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test1_pipeline.cco" -o "$TMP_DIR/test1_pipeline"
OUT=$("$TMP_DIR/test1_pipeline")
if [[ "$OUT" == "Pipeline integration OK" ]]; then
    pass "Complete cross-module processing pipeline executes flawlessly"
else
    fail "Pipeline integration" "Output was: $OUT"
fi

# Test 2: Error handling & partial stream recovery with Result and ByteBuffer
echo "Test 2: Stream parsing error handling and recovery (Result + ByteBuffer)..."
cat << 'EOF' > "$TMP_DIR/test2_errors.cco"
import "std/result.cco";
import "std/buffer.cco";

fn parse_fixed_header(buf: &ByteBuffer) -> ResultInt {
    if (buf.readable_bytes() < 4) {
        return err_int("EOF: incomplete header");
    }
    let magic: int = buf.read_u16_be();
    if (magic != 43981) { // 0xABCD
        return err_int("ERR: corrupted magic");
    }
    let code: int = buf.read_u16_be();
    return ok_int(code);
}

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(16);

    // Case A: Incomplete data (only 2 bytes written)
    buf.write_u16_be(43981);
    let r1: ResultInt = parse_fixed_header(&buf);
    if (!res_int_is_err(&r1)) { return 1; }
    if (!equals(res_int_error(&r1), "EOF: incomplete header")) { return 2; }
    if (buf.reader_pos != 0) { return 3; } // Pos must not advance

    // Case B: Finish packet and parse
    buf.write_u16_be(777);
    let r2: ResultInt = parse_fixed_header(&buf);
    if (!res_int_is_ok(&r2)) { return 4; }
    if (res_int_unwrap_or(&r2, 0) != 777) { return 5; }

    // Case C: Corrupted magic
    buf.reset();
    buf.write_u16_be(1234); // bad magic
    buf.write_u16_be(888);
    let r3: ResultInt = parse_fixed_header(&buf);
    if (!res_int_is_err(&r3)) { return 6; }
    if (!equals(res_int_error(&r3), "ERR: corrupted magic")) { return 7; }

    print("Stream recovery OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test2_errors.cco" -o "$TMP_DIR/test2_errors"
OUT=$("$TMP_DIR/test2_errors")
if [[ "$OUT" == "Stream recovery OK" ]]; then
    pass "Result error propagation and buffer recovery interact cleanly"
else
    fail "Stream recovery" "Output was: $OUT"
fi

# Test 3: Zero and negative integer handling across RingDeque and OptionInt
echo "Test 3: Zero and negative value disambiguation across RingDeque and OptionInt..."
cat << 'EOF' > "$TMP_DIR/test3_sentinels.cco"
import "std/option.cco";
import "std/deque.cco";

fn main() -> int {
    let q: RingDeque = deque_new(6);

    // Storing negative values and zero
    q.push_back(0);
    q.push_back(-1); // Valid stored -1 (must not be confused with empty sentinel)
    q.push_back(-42);
    q.push_back(100);

    // 1. Check peek_front_opt for 0
    let p0: OptionInt = q.peek_front_opt();
    if (!opt_int_is_some(&p0)) { return 1; }
    if (opt_int_unwrap_or(&p0, 999) != 0) { return 2; }

    // 2. Pop 0
    let v0: OptionInt = q.pop_front_opt();
    if (!opt_int_is_some(&v0)) { return 3; }
    if (opt_int_unwrap_or(&v0, 999) != 0) { return 4; }

    // 3. Pop -1: OptionInt.Some with value -1
    let v_neg1: OptionInt = q.pop_front_opt();
    if (!opt_int_is_some(&v_neg1)) { return 5; }
    if (opt_int_unwrap_or(&v_neg1, 999) != -1) { return 6; }

    // 4. Pop -42
    let v_neg42: OptionInt = q.pop_front_opt();
    if (!opt_int_is_some(&v_neg42)) { return 7; }
    if (opt_int_unwrap_or(&v_neg42, 999) != -42) { return 8; }

    // 5. Pop 100
    let v100: OptionInt = q.pop_front_opt();
    if (!opt_int_is_some(&v100)) { return 9; }
    if (opt_int_unwrap_or(&v100, 0) != 100) { return 10; }

    // 6. Now queue is genuinely empty
    let v_empty: OptionInt = q.pop_front_opt();
    if (opt_int_is_some(&v_empty)) { return 11; }
    if (!opt_int_is_none(&v_empty)) { return 12; }

    // Legacy sentinel check: pop_front() on empty returns -1
    if (q.pop_front() != -1) { return 13; }

    print("Sentinel disambiguation OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test3_sentinels.cco" -o "$TMP_DIR/test3_sentinels"
OUT=$("$TMP_DIR/test3_sentinels")
if [[ "$OUT" == "Sentinel disambiguation OK" ]]; then
    pass "Payload values of -1 and 0 are unambiguously handled through OptionInt"
else
    fail "Sentinel disambiguation" "Output was: $OUT"
fi

# Test 4: Repeated multi-cycle reuse under load (50 iterations)
echo "Test 4: Repeated multi-cycle creation, reuse and destruction..."
cat << 'EOF' > "$TMP_DIR/test4_load_cycles.cco"
import "std/option.cco";
import "std/result.cco";
import "std/deque.cco";
import "std/buffer.cco";

fn run_cycle(iter: int) -> bool {
    let buf: ByteBuffer = byte_buffer_new(16);
    let q: RingDeque = deque_new(8);

    // Fill buffer
    buf.write_u8(iter % 256);
    buf.write_u16_be(iter * 10);
    buf.write_u32_be(iter * 1000);

    // Read back and push to queue
    let b: OptionInt = buf.read_u8_opt();
    let s: OptionInt = buf.read_u16_be_opt();
    let w: OptionInt = buf.read_u32_be_opt();

    if (!opt_int_is_some(&b) || !opt_int_is_some(&s) || !opt_int_is_some(&w)) {
        return false;
    }

    q.push_back(opt_int_unwrap_or(&b, 0));
    q.push_back(opt_int_unwrap_or(&s, 0));
    q.push_back(opt_int_unwrap_or(&w, 0));

    if (q.count() != 3) { return false; }

    // Drain queue
    q.pop_front();
    q.pop_front();
    q.pop_front();

    if (!q.is_empty()) { return false; }

    // Reuse via reset / clear
    buf.reset();
    q.clear();

    buf.write_u16_be(42);
    let v: OptionInt = buf.read_u16_be_opt();
    if (opt_int_unwrap_or(&v, 0) != 42) { return false; }

    return true;
}

fn main() -> int {
    for (let i: int = 0; i < 50; i++) {
        if (!run_cycle(i)) {
            return 1;
        }
    }
    print("Multi-cycle reuse OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test4_load_cycles.cco" -o "$TMP_DIR/test4_load_cycles"
OUT=$("$TMP_DIR/test4_load_cycles")
if [[ "$OUT" == "Multi-cycle reuse OK" ]]; then
    pass "50 cycles of allocation, serialization, deserialization, reset, and teardown succeed"
else
    fail "Multi-cycle reuse" "Output was: $OUT"
fi

# Test 5: Valgrind memory leak verification across all integration test binaries
echo "Test 5: Valgrind memory leak verification across integration test binaries..."
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
    pass "All Phase 18C integration binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# Test 6: Negative boundary — Native backend cleanly rejects multi-module integrated code
echo "Test 6: Negative boundary — compile-time rejection by native backend..."
if [[ -x "$SELFHOST_CCO" ]]; then
    set +e
    NATIVE_ERR=$(cd "$ROOT_DIR" && "$SELFHOST_CCO" "$TMP_DIR/test1_pipeline.cco" --selfhost-native -o "$TMP_DIR/native_bin" 2>&1)
    NATIVE_CODE=$?
    set -e
    if [[ $NATIVE_CODE -eq 0 ]]; then
        fail "Native backend boundary" "Expected native backend to reject integrated code, but compilation succeeded"
    else
        if [[ "$NATIVE_ERR" == *"does not support enum definitions"* || "$NATIVE_ERR" == *"does not support class definitions"* ]]; then
            pass "Native backend cleanly rejected unsupported features in integrated code with informative diagnostic"
        else
            fail "Native backend diagnostic" "Unexpected error message: $NATIVE_ERR"
        fi
    fi
else
    echo "  (selfhost/cco binary not built, skipping native rejection check)"
fi

echo "=============================================================="
echo "Phase 18C Milestone 4 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
