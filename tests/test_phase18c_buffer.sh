#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18C — Milestone 3: ByteBuffer Test Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_phase18c_buffer_tests_$$"

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
echo "Cco Phase 18C Milestone 3: ByteBuffer Test Suite"
echo "=============================================================="

# Test 1: Capacity and initial state
echo "Test 1: Capacity and initial state..."
cat << 'EOF' > "$TMP_DIR/test1_init.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(16);
    if (buf.capacity != 16) { return 1; }
    if (buf.readable_bytes() != 0) { return 2; }
    if (buf.writable_bytes() != 16) { return 3; }
    if (buf.reader_pos != 0) { return 4; }
    if (buf.writer_pos != 0) { return 5; }

    let opt_u8: OptionInt = buf.read_u8_opt();
    if (opt_int_is_some(&opt_u8)) { return 6; }

    let opt_u16: OptionInt = buf.read_u16_be_opt();
    if (opt_int_is_some(&opt_u16)) { return 7; }

    let opt_u32: OptionInt = buf.read_u32_be_opt();
    if (opt_int_is_some(&opt_u32)) { return 8; }

    let peek_opt: OptionInt = buf.peek_u8_opt();
    if (opt_int_is_some(&peek_opt)) { return 9; }

    if (buf.read_u8() != -1) { return 10; }
    if (buf.read_u16_be() != -1) { return 11; }
    if (buf.read_u32_be() != -1) { return 12; }
    if (buf.peek_u8() != -1) { return 13; }

    print("Initial state OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test1_init.cco" -o "$TMP_DIR/test1_init"
OUT=$("$TMP_DIR/test1_init")
if [[ "$OUT" == "Initial state OK" ]]; then
    pass "Empty buffer initializes positions, capacities, and empty sentinels correctly"
else
    fail "Initial state test" "Output was: $OUT"
fi

# Test 2: Writing and reading a single byte (u8)
echo "Test 2: Writing and reading single byte (u8)..."
cat << 'EOF' > "$TMP_DIR/test2_u8.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(8);
    if (!buf.write_u8(42)) { return 1; }
    if (buf.readable_bytes() != 1) { return 2; }
    if (buf.writable_bytes() != 7) { return 3; }

    // Peek without consuming
    let p: OptionInt = buf.peek_u8_opt();
    if (!opt_int_is_some(&p)) { return 4; }
    if (opt_int_unwrap_or(&p, 0) != 42) { return 5; }
    if (buf.readable_bytes() != 1) { return 6; }
    if (buf.peek_u8() != 42) { return 7; }

    // Read and consume
    let r: OptionInt = buf.read_u8_opt();
    if (!opt_int_is_some(&r)) { return 8; }
    if (opt_int_unwrap_or(&r, 0) != 42) { return 9; }
    if (buf.readable_bytes() != 0) { return 10; }

    // Read after EOF
    let eof: OptionInt = buf.read_u8_opt();
    if (opt_int_is_some(&eof)) { return 11; }

    print("Single byte u8 OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test2_u8.cco" -o "$TMP_DIR/test2_u8"
OUT=$("$TMP_DIR/test2_u8")
if [[ "$OUT" == "Single byte u8 OK" ]]; then
    pass "Single byte write, peek, read, and EOF behavior succeed"
else
    fail "Single byte u8 test" "Output was: $OUT"
fi

# Test 3: 16-bit big-endian round-trip
echo "Test 3: 16-bit big-endian round-trip..."
cat << 'EOF' > "$TMP_DIR/test3_u16.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(10);
    if (!buf.write_u16_be(4660)) { return 1; } // 0x1234
    if (!buf.write_u16_be(500)) { return 2; }
    if (buf.readable_bytes() != 4) { return 3; }

    let v1: OptionInt = buf.read_u16_be_opt();
    if (!opt_int_is_some(&v1)) { return 4; }
    if (opt_int_unwrap_or(&v1, 0) != 4660) { return 5; }

    let v2: OptionInt = buf.read_u16_be_opt();
    if (!opt_int_is_some(&v2)) { return 6; }
    if (opt_int_unwrap_or(&v2, 0) != 500) { return 7; }

    if (buf.readable_bytes() != 0) { return 8; }
    print("16-bit big-endian OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test3_u16.cco" -o "$TMP_DIR/test3_u16"
OUT=$("$TMP_DIR/test3_u16")
if [[ "$OUT" == "16-bit big-endian OK" ]]; then
    pass "16-bit big-endian round-trip serializes and deserializes values accurately"
else
    fail "16-bit big-endian test" "Output was: $OUT"
fi

# Test 4: 32-bit big-endian round-trip
echo "Test 4: 32-bit big-endian round-trip..."
cat << 'EOF' > "$TMP_DIR/test4_u32.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(16);
    if (!buf.write_u32_be(100500)) { return 1; }
    if (!buf.write_u32_be(305419896)) { return 2; } // 0x12345678
    if (buf.readable_bytes() != 8) { return 3; }

    let v1: OptionInt = buf.read_u32_be_opt();
    if (!opt_int_is_some(&v1)) { return 4; }
    if (opt_int_unwrap_or(&v1, 0) != 100500) { return 5; }

    let v2: OptionInt = buf.read_u32_be_opt();
    if (!opt_int_is_some(&v2)) { return 6; }
    if (opt_int_unwrap_or(&v2, 0) != 305419896) { return 7; }

    if (buf.readable_bytes() != 0) { return 8; }
    print("32-bit big-endian OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test4_u32.cco" -o "$TMP_DIR/test4_u32"
OUT=$("$TMP_DIR/test4_u32")
if [[ "$OUT" == "32-bit big-endian OK" ]]; then
    pass "32-bit big-endian round-trip serializes and deserializes values accurately"
else
    fail "32-bit big-endian test" "Output was: $OUT"
fi

# Test 5: Mixed-width binary protocol frame serialization (codebase 232 pattern)
echo "Test 5: Mixed-width binary protocol frame serialization..."
cat << 'EOF' > "$TMP_DIR/test5_protocol.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = create_byte_buffer(32);

    // Frame: Magic [204, 1], Opcode [5], PayloadLen [128], SeqID [100500], Payload [65, 66, 67]
    buf.write_u8(204);
    buf.write_u8(1);
    buf.write_u8(5);
    buf.write_u16_be(128);
    buf.write_u32_be(100500);
    buf.write_u8(65);
    buf.write_u8(66);
    buf.write_u8(67);

    // Total bytes: 1 + 1 + 1 + 2 + 4 + 3 = 12 bytes
    if (buf.readable_bytes() != 12) { return 1; }

    if (buf.read_u8() != 204) { return 2; }
    if (buf.read_u8() != 1) { return 3; }
    if (buf.read_u8() != 5) { return 4; }
    if (buf.read_u16_be() != 128) { return 5; }
    if (buf.read_u32_be() != 100500) { return 6; }
    if (buf.read_u8() != 65) { return 7; }
    if (buf.read_u8() != 66) { return 8; }
    if (buf.read_u8() != 67) { return 9; }

    if (buf.readable_bytes() != 0) { return 10; }
    print("Binary protocol frame OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test5_protocol.cco" -o "$TMP_DIR/test5_protocol"
OUT=$("$TMP_DIR/test5_protocol")
if [[ "$OUT" == "Binary protocol frame OK" ]]; then
    pass "Mixed-width packet framing correctly serializes and unpacks wire frames"
else
    fail "Binary protocol frame test" "Output was: $OUT"
fi

# Test 6: Zero bytes and distinguishing zero from EOF
echo "Test 6: Zero bytes and distinguishing zero from EOF..."
cat << 'EOF' > "$TMP_DIR/test6_zero.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(10);
    buf.write_u8(0);
    buf.write_u16_be(0);
    buf.write_u32_be(0);

    // Read zero u8
    let z8: OptionInt = buf.read_u8_opt();
    if (!opt_int_is_some(&z8)) { return 1; } // MUST be Some
    if (opt_int_unwrap_or(&z8, 99) != 0) { return 2; } // Value is 0

    // Read zero u16
    let z16: OptionInt = buf.read_u16_be_opt();
    if (!opt_int_is_some(&z16)) { return 3; }
    if (opt_int_unwrap_or(&z16, 99) != 0) { return 4; }

    // Read zero u32
    let z32: OptionInt = buf.read_u32_be_opt();
    if (!opt_int_is_some(&z32)) { return 5; }
    if (opt_int_unwrap_or(&z32, 99) != 0) { return 6; }

    // Now empty - must return None
    let eof: OptionInt = buf.read_u8_opt();
    if (opt_int_is_some(&eof)) { return 7; } // MUST be None

    print("Zero byte disambiguation OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test6_zero.cco" -o "$TMP_DIR/test6_zero"
OUT=$("$TMP_DIR/test6_zero")
if [[ "$OUT" == "Zero byte disambiguation OK" ]]; then
    pass "OptionInt methods distinguish valid zero payloads from EOF"
else
    fail "Zero byte test" "Output was: $OUT"
fi

# Test 7: Minimum and maximum supported values & out-of-range rejection
echo "Test 7: Minimum and maximum supported values & out-of-range rejection..."
cat << 'EOF' > "$TMP_DIR/test7_ranges.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(16);

    // u8 range [0..255]
    if (!buf.write_u8(0)) { return 1; }
    if (!buf.write_u8(255)) { return 2; }
    if (buf.write_u8(-1)) { return 3; }
    if (buf.write_u8(256)) { return 4; }

    // u16 range [0..65535]
    if (!buf.write_u16_be(0)) { return 5; }
    if (!buf.write_u16_be(65535)) { return 6; }
    if (buf.write_u16_be(-1)) { return 7; }
    if (buf.write_u16_be(65536)) { return 8; }

    // u32 range [0..2147483647]
    if (!buf.write_u32_be(0)) { return 9; }
    if (!buf.write_u32_be(2147483647)) { return 10; }
    if (buf.write_u32_be(-1)) { return 11; }
    if (buf.write_u32_be(2147483648)) { return 12; }

    // Verify written max values read back identically
    if (buf.read_u8() != 0) { return 13; }
    if (buf.read_u8() != 255) { return 14; }
    if (buf.read_u16_be() != 0) { return 15; }
    if (buf.read_u16_be() != 65535) { return 16; }
    if (buf.read_u32_be() != 0) { return 17; }
    if (buf.read_u32_be() != 2147483647) { return 18; }
    if (buf.readable_bytes() != 0) { return 19; }

    print("Value range boundaries OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test7_ranges.cco" -o "$TMP_DIR/test7_ranges"
OUT=$("$TMP_DIR/test7_ranges")
if [[ "$OUT" == "Value range boundaries OK" ]]; then
    pass "Boundary values round-trip perfectly and out-of-range inputs are rejected"
else
    fail "Value range boundaries test" "Output was: $OUT"
fi

# Test 8: Explicit Big-Endian Byte Order Evidence
echo "Test 8: Explicit Big-Endian Byte Order Evidence..."
cat << 'EOF' > "$TMP_DIR/test8_endianness.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(10);

    // 0x1234 -> MSB 0x12 (18), LSB 0x34 (52)
    buf.write_u16_be(4660);
    let b0: int = buf.read_u8();
    let b1: int = buf.read_u8();
    if (b0 != 18 || b1 != 52) { return 1; }

    // 0x12345678 -> MSB 0x12 (18), 0x34 (52), 0x56 (86), LSB 0x78 (120)
    buf.write_u32_be(305419896);
    let c0: int = buf.read_u8();
    let c1: int = buf.read_u8();
    let c2: int = buf.read_u8();
    let c3: int = buf.read_u8();
    if (c0 != 18 || c1 != 52 || c2 != 86 || c3 != 120) { return 2; }

    print("Explicit Big-Endian order OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test8_endianness.cco" -o "$TMP_DIR/test8_endianness"
OUT=$("$TMP_DIR/test8_endianness")
if [[ "$OUT" == "Explicit Big-Endian order OK" ]]; then
    pass "Explicit byte inspection proves MSB-first big-endian wire order in memory"
else
    fail "Big-Endian order test" "Output was: $OUT"
fi

# Test 9: Insufficient capacity for multi-byte write (Atomic rejection & no position movement)
echo "Test 9: Insufficient capacity for multi-byte write..."
cat << 'EOF' > "$TMP_DIR/test9_write_capacity.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(3);
    buf.write_u8(1);
    buf.write_u8(2);
    // Writable bytes = 1
    if (buf.writable_bytes() != 1) { return 1; }
    if (buf.writer_pos != 2) { return 2; }

    // Attempt u16 write (needs 2 bytes) -> must reject
    if (buf.write_u16_be(500)) { return 3; }
    // Must NOT advance writer_pos
    if (buf.writer_pos != 2) { return 4; }
    if (buf.writable_bytes() != 1) { return 5; }

    // Attempt u32 write (needs 4 bytes) -> must reject
    if (buf.write_u32_be(100500)) { return 6; }
    if (buf.writer_pos != 2) { return 7; }

    // 1-byte write must succeed
    if (!buf.write_u8(3)) { return 8; }
    if (buf.writer_pos != 3) { return 9; }
    if (buf.writable_bytes() != 0) { return 10; }

    // Further writes rejected
    if (buf.write_u8(4)) { return 11; }
    if (buf.writer_pos != 3) { return 12; }

    print("Atomic write capacity OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test9_write_capacity.cco" -o "$TMP_DIR/test9_write_capacity"
OUT=$("$TMP_DIR/test9_write_capacity")
if [[ "$OUT" == "Atomic write capacity OK" ]]; then
    pass "Insufficient capacity cleanly and atomically rejects multi-byte writes without partial writes"
else
    fail "Write capacity test" "Output was: $OUT"
fi

# Test 10: Insufficient readable data for multi-byte read (Atomic rejection & no reader advancement)
echo "Test 10: Insufficient readable data for multi-byte read..."
cat << 'EOF' > "$TMP_DIR/test10_read_eof.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(4);
    buf.write_u8(10);
    buf.write_u8(20);
    buf.write_u8(30);
    // 3 bytes readable
    if (buf.readable_bytes() != 3) { return 1; }
    if (buf.reader_pos != 0) { return 2; }

    // Attempt u32 read (needs 4 bytes) -> must return None and NOT advance reader_pos
    let bad_u32: OptionInt = buf.read_u32_be_opt();
    if (opt_int_is_some(&bad_u32)) { return 3; }
    if (buf.reader_pos != 0) { return 4; }
    if (buf.readable_bytes() != 3) { return 5; }

    // u16 read (needs 2 bytes) -> succeeds
    let ok_u16: OptionInt = buf.read_u16_be_opt();
    if (!opt_int_is_some(&ok_u16)) { return 6; }
    if (opt_int_unwrap_or(&ok_u16, 0) != (10 * 256 + 20)) { return 7; }
    if (buf.reader_pos != 2) { return 8; }
    if (buf.readable_bytes() != 1) { return 9; }

    // Attempt u16 read again (needs 2 bytes, only 1 left) -> must fail without advancing reader_pos
    let bad_u16: OptionInt = buf.read_u16_be_opt();
    if (opt_int_is_some(&bad_u16)) { return 10; }
    if (buf.reader_pos != 2) { return 11; }
    if (buf.readable_bytes() != 1) { return 12; }

    // Read remaining single byte
    let ok_u8: OptionInt = buf.read_u8_opt();
    if (!opt_int_is_some(&ok_u8)) { return 13; }
    if (opt_int_unwrap_or(&ok_u8, 0) != 30) { return 14; }
    if (buf.readable_bytes() != 0) { return 15; }

    print("Atomic read EOF OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test10_read_eof.cco" -o "$TMP_DIR/test10_read_eof"
OUT=$("$TMP_DIR/test10_read_eof")
if [[ "$OUT" == "Atomic read EOF OK" ]]; then
    pass "Failed multi-byte reads return None and preserve reader_pos atomically"
else
    fail "Read EOF test" "Output was: $OUT"
fi

# Test 11: Reset and reuse
echo "Test 11: Reset and reuse..."
cat << 'EOF' > "$TMP_DIR/test11_reset.cco"
import "std/buffer.cco";

fn main() -> int {
    let buf: ByteBuffer = byte_buffer_new(8);
    buf.write_u32_be(123456);
    buf.read_u16_be();

    buf.reset();
    if (buf.reader_pos != 0) { return 1; }
    if (buf.writer_pos != 0) { return 2; }
    if (buf.readable_bytes() != 0) { return 3; }
    if (buf.writable_bytes() != 8) { return 4; }

    // Write and read anew
    buf.write_u16_be(999);
    let v: OptionInt = buf.read_u16_be_opt();
    if (!opt_int_is_some(&v)) { return 5; }
    if (opt_int_unwrap_or(&v, 0) != 999) { return 6; }

    print("Reset and reuse OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test11_reset.cco" -o "$TMP_DIR/test11_reset"
OUT=$("$TMP_DIR/test11_reset")
if [[ "$OUT" == "Reset and reuse OK" ]]; then
    pass "Reset restores buffer positions and allows immediate reuse without reallocation"
else
    fail "Reset and reuse test" "Output was: $OUT"
fi

# Test 12: Non-positive capacity safe clamping
echo "Test 12: Non-positive capacity safe clamping..."
cat << 'EOF' > "$TMP_DIR/test12_clamp.cco"
import "std/buffer.cco";

fn main() -> int {
    let b0: ByteBuffer = byte_buffer_new(0);
    if (b0.capacity != 1) { return 1; }
    if (!b0.write_u8(77)) { return 2; }
    if (b0.write_u8(88)) { return 3; } // full
    if (b0.read_u8() != 77) { return 4; }

    let bneg: ByteBuffer = byte_buffer_new(-10);
    if (bneg.capacity != 1) { return 5; }
    if (!bneg.write_u8(99)) { return 6; }
    if (bneg.read_u8() != 99) { return 7; }

    print("Capacity clamp OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test12_clamp.cco" -o "$TMP_DIR/test12_clamp"
OUT=$("$TMP_DIR/test12_clamp")
if [[ "$OUT" == "Capacity clamp OK" ]]; then
    pass "Zero and negative capacities safely clamp to 1 without arithmetic issues"
else
    fail "Capacity clamp test" "Output was: $OUT"
fi

# Test 13: Importing buffer alongside std/option.cco
echo "Test 13: Importing buffer alongside std/option.cco..."
cat << 'EOF' > "$TMP_DIR/test13_import.cco"
import "std/option.cco";
import "std/buffer.cco";

fn main() -> int {
    let opt: OptionInt = some_int(123);
    let buf: ByteBuffer = byte_buffer_new(4);
    buf.write_u8(opt_int_unwrap_or(&opt, 0));
    let r: OptionInt = buf.read_u8_opt();
    if (!opt_int_is_some(&r)) { return 1; }
    if (opt_int_unwrap_or(&r, 0) != 123) { return 2; }
    print("Double import OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test13_import.cco" -o "$TMP_DIR/test13_import"
OUT=$("$TMP_DIR/test13_import")
if [[ "$OUT" == "Double import OK" ]]; then
    pass "Buffer imports alongside std/option.cco without symbol collisions or duplicate declarations"
else
    fail "Double import test" "Output was: $OUT"
fi

# Test 14: Valgrind memory leak verification across all test binaries
echo "Test 14: Valgrind memory leak verification across all ByteBuffer test binaries..."
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
    pass "All ByteBuffer test binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# Test 15: Negative boundary — compile-time rejection by native backend
echo "Test 15: Negative boundary — compile-time rejection by native backend..."
SELFHOST_CCO="$ROOT_DIR/selfhost/cco"
if [[ -x "$SELFHOST_CCO" ]]; then
    set +e
    NATIVE_ERR=$(cd "$ROOT_DIR" && "$SELFHOST_CCO" "$TMP_DIR/test1_init.cco" --selfhost-native -o "$TMP_DIR/native_bin" 2>&1)
    NATIVE_CODE=$?
    set -e
    if [[ $NATIVE_CODE -eq 0 ]]; then
        fail "Native backend boundary" "Expected native backend to reject class ByteBuffer, but compilation succeeded"
    else
        if [[ "$NATIVE_ERR" == *"does not support class definitions"* ]]; then
            pass "Native backend cleanly rejected class ByteBuffer with informative diagnostic"
        else
            fail "Native backend diagnostic" "Unexpected error message: $NATIVE_ERR"
        fi
    fi
else
    echo "  (selfhost/cco binary not built, skipping native rejection check)"
fi

echo "=============================================================="
echo "Phase 18C Milestone 3 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
