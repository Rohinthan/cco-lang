#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18D — Milestone 2: Raw Socket I/O & Binary Transfer Test Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_phase18d_network_tests_$$"

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
echo "Cco Phase 18D Milestone 2: Raw Socket I/O & Binary Tests"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Test 1: Raw TCP Payload Transfer Without HTTP Framing
# ------------------------------------------------------------------------------
echo "Test 1: Raw TCP payload transfer without HTTP framing..."
cat << 'EOF' > "$TMP_DIR/py_client1.py"
import socket, time
time.sleep(0.05)
s = socket.socket()
s.connect(('127.0.0.1', 29401))
# Send raw non-HTTP message without \r\n\r\n and keep connection open
s.sendall(b'RAW_NON_HTTP_REQUEST_42')
reply = s.recv(64)
assert reply == b'RAW_NON_HTTP_REPLY_42', f"Unexpected reply: {reply}"
s.close()
EOF

cat << EOF > "$TMP_DIR/test1_raw_tcp.cco"
import "std/net.cco";

fn main() -> int {
    let server_fd: int = net_listen(29401);
    if (server_fd < 0) {
        return 1;
    }

    execute_cmd("python3 $TMP_DIR/py_client1.py &");

    let client_fd: int = net_accept(server_fd);
    if (client_fd < 0) {
        net_close(server_fd);
        return 2;
    }

    // net_recv_raw must return immediately without waiting for \r\n\r\n or EOF
    let msg: string = net_recv_raw(client_fd, 1024);
    if (!equals(msg, "RAW_NON_HTTP_REQUEST_42")) {
        print(concat("Payload mismatch: ", msg));
        net_close(client_fd);
        net_close(server_fd);
        return 3;
    }

    let sent: int = net_send(client_fd, "RAW_NON_HTTP_REPLY_42");
    if (sent <= 0) {
        print("Reply send failed");
        net_close(client_fd);
        net_close(server_fd);
        return 4;
    }

    print("Raw TCP OK");
    net_close(client_fd);
    net_close(server_fd);
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test1_raw_tcp.cco" -o "$TMP_DIR/test1_raw_tcp"
OUT=$("$TMP_DIR/test1_raw_tcp")
if [[ "$OUT" == "Raw TCP OK" ]]; then
    pass "Raw TCP transfer succeeds immediately without HTTP framing or peer EOF"
else
    fail "Test 1 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 2: Binary Payload with Embedded NUL Bytes
# ------------------------------------------------------------------------------
echo "Test 2: Binary payload containing embedded NUL bytes..."
cat << 'EOF' > "$TMP_DIR/py_client2.py"
import socket, time
time.sleep(0.05)
s = socket.socket()
s.connect(('127.0.0.1', 29402))
# Send 8 bytes with multiple embedded 0x00s: [0x00, 0x01, 0x00, 0x02, 0x00, 0xFF, 0xFE, 0x00]
s.sendall(bytes([0, 1, 0, 2, 0, 255, 254, 0]))
echo_data = s.recv(16)
assert list(echo_data) == [0, 1, 0, 2, 0, 255, 254, 0], f"Bad echo: {list(echo_data)}"
s.close()
EOF

cat << EOF > "$TMP_DIR/test2_binary_nul.cco"
import "std/net.cco";

fn main() -> int {
    let server_fd: int = net_listen(29402);
    if (server_fd < 0) {
        return 1;
    }

    execute_cmd("python3 $TMP_DIR/py_client2.py &");

    let client_fd: int = net_accept(server_fd);
    if (client_fd < 0) {
        net_close(server_fd);
        return 2;
    }

    let buf: int[] = alloc(int, 16);
    let n: int = net_recv_bytes(client_fd, buf, 16);
    if (n != 8) {
        print(f"Expected 8 bytes, got {n}");
        net_close(client_fd);
        net_close(server_fd);
        return 3;
    }

    // Verify all bytes including NULs
    if (buf[0] != 0 || buf[1] != 1 || buf[2] != 0 || buf[3] != 2 ||
        buf[4] != 0 || buf[5] != 255 || buf[6] != 254 || buf[7] != 0) {
        print("Binary byte content mismatch");
        net_close(client_fd);
        net_close(server_fd);
        return 4;
    }

    // Echo bytes back through net_send_bytes
    let sent: int = net_send_bytes(client_fd, buf, 8);
    if (sent != 8) {
        print(f"Echo send failed: {sent}");
        net_close(client_fd);
        net_close(server_fd);
        return 5;
    }

    print("Binary NUL OK");
    net_close(client_fd);
    net_close(server_fd);
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test2_binary_nul.cco" -o "$TMP_DIR/test2_binary_nul"
OUT=$("$TMP_DIR/test2_binary_nul")
if [[ "$OUT" == "Binary NUL OK" ]]; then
    pass "Binary transfer safely preserves embedded NUL bytes without truncation"
else
    fail "Test 2 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 3: Explicit-Length Send Behavior & Validation
# ------------------------------------------------------------------------------
echo "Test 3: Explicit-length send behavior..."
cat << 'EOF' > "$TMP_DIR/py_client3.py"
import socket, time
time.sleep(0.05)
s = socket.socket()
s.connect(('127.0.0.1', 29403))
data = s.recv(64)
# Buffer has 32 bytes in Cco, but only 10 were requested to send
assert len(data) == 10, f"Expected 10 bytes, received {len(data)}"
assert list(data) == [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
s.close()
EOF

cat << EOF > "$TMP_DIR/test3_explicit_len.cco"
import "std/net.cco";

fn main() -> int {
    let server_fd: int = net_listen(29403);
    if (server_fd < 0) {
        return 1;
    }

    execute_cmd("python3 $TMP_DIR/py_client3.py &");

    let client_fd: int = net_accept(server_fd);
    if (client_fd < 0) {
        net_close(server_fd);
        return 2;
    }

    let big_buf: int[] = alloc(int, 32);
    for (let i: int = 0; i < 32; i++) {
        big_buf[i] = (i + 1) * 10;
    }

    // Send only 10 bytes out of 32
    let sent: int = net_send_bytes(client_fd, big_buf, 10);
    if (sent != 10) {
        print("Send length mismatch");
        net_close(client_fd);
        net_close(server_fd);
        return 3;
    }

    print("Explicit length OK");
    net_close(client_fd);
    net_close(server_fd);
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test3_explicit_len.cco" -o "$TMP_DIR/test3_explicit_len"
OUT=$("$TMP_DIR/test3_explicit_len")
if [[ "$OUT" == "Explicit length OK" ]]; then
    pass "Explicit-length send transmits exactly requested count from larger buffer"
else
    fail "Test 3 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 4: Receiver Behavior on Peer Orderly Shutdown (EOF)
# ------------------------------------------------------------------------------
echo "Test 4: Receiver behavior on peer orderly shutdown (EOF)..."
cat << 'EOF' > "$TMP_DIR/py_client4.py"
import socket, time
time.sleep(0.05)
s = socket.socket()
s.connect(('127.0.0.1', 29404))
s.sendall(b'HELO')
# Orderly close of client socket
s.close()
EOF

cat << EOF > "$TMP_DIR/test4_eof.cco"
import "std/net.cco";

fn main() -> int {
    let server_fd: int = net_listen(29404);
    if (server_fd < 0) {
        return 1;
    }

    execute_cmd("python3 $TMP_DIR/py_client4.py &");

    let client_fd: int = net_accept(server_fd);
    if (client_fd < 0) {
        net_close(server_fd);
        return 2;
    }

    let buf: int[] = alloc(int, 16);
    let n1: int = net_recv_bytes(client_fd, buf, 16);
    if (n1 != 4) {
        print("Expected 4 bytes initial read");
        net_close(client_fd);
        net_close(server_fd);
        return 3;
    }

    // Second read on closed peer connection must return 0 (EOF)
    let n2: int = net_recv_bytes(client_fd, buf, 16);
    if (n2 != 0) {
        print("Expected 0 bytes on peer EOF");
        net_close(client_fd);
        net_close(server_fd);
        return 4;
    }

    // Raw string receive on EOF must return empty string
    let s_eof: string = net_recv_raw(client_fd, 16);
    if (!equals(s_eof, "")) {
        print("Expected empty string on EOF");
        net_close(client_fd);
        net_close(server_fd);
        return 5;
    }

    print("Peer EOF OK");
    net_close(client_fd);
    net_close(server_fd);
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test4_eof.cco" -o "$TMP_DIR/test4_eof"
OUT=$("$TMP_DIR/test4_eof")
if [[ "$OUT" == "Peer EOF OK" ]]; then
    pass "Peer connection shutdown returns EOF 0 and empty string consistently"
else
    fail "Test 4 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 5: Invalid Descriptors and Robust Error Handling
# ------------------------------------------------------------------------------
echo "Test 5: Invalid file descriptors and error handling..."
cat << 'EOF' > "$TMP_DIR/test5_invalid_fd.cco"
import "std/net.cco";

fn main() -> int {
    let buf: int[] = alloc(int, 8);

    // Negative FD must return error, not hang or abort
    let r1: int = net_send_bytes(-1, buf, 4);
    if (r1 != -1) { return 1; }

    let r2: int = net_recv_bytes(-1, buf, 4);
    if (r2 != -1) { return 2; }

    let r3: int = net_send(-1, "hello");
    if (r3 != -1) { return 3; }

    let r4: string = net_recv_raw(-1, 1024);
    if (!equals(r4, "")) { return 4; }

    let r5: string = net_recv(-1, 0);
    if (!equals(r5, "")) { return 5; }

    // Closing negative FD must safely no-op
    net_close(-1);
    net_close(-99);

    print("Invalid FD OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test5_invalid_fd.cco" -o "$TMP_DIR/test5_invalid_fd"
OUT=$("$TMP_DIR/test5_invalid_fd")
if [[ "$OUT" == "Invalid FD OK" ]]; then
    pass "Invalid file descriptors safely return error codes without crashes or leaks"
else
    fail "Test 5 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 6: Socket Lifecycle & Double-Close Safety
# ------------------------------------------------------------------------------
echo "Test 6: Socket lifecycle & repeated close attempts (double-close safety)..."
cat << 'EOF' > "$TMP_DIR/test6_lifecycle.cco"
import "std/net.cco";

fn main() -> int {
    let s: TcpServer = create_server(29406);
    if (s.fd < 0) { return 1; }

    // First close
    s.close();
    if (s.fd != -1) {
        print("Server fd not reset to -1");
        return 2;
    }

    // Repeated close must safely no-op without double closing
    s.close();
    s.close();

    // Client wrapper lifecycle
    let c: TcpClient = create_client(42);
    if (c.fd != 42) { return 3; }
    c.close();
    if (c.fd != -1) {
        print("Client fd not reset to -1");
        return 4;
    }

    // Repeated client close
    c.close();
    c.close();

    // Calling operations on closed client
    let b: int[] = alloc(int, 4);
    if (c.send("test") != -1) { return 5; }
    if (c.send_bytes(b, 4) != -1) { return 6; }
    if (c.recv_bytes(b, 4) != -1) { return 7; }
    let raw_closed: string = c.recv_raw(10);
    if (!equals(raw_closed, "")) { return 8; }

    print("Lifecycle OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test6_lifecycle.cco" -o "$TMP_DIR/test6_lifecycle"
OUT=$("$TMP_DIR/test6_lifecycle")
if [[ "$OUT" == "Lifecycle OK" ]]; then
    pass "Socket wrapper lifecycle invalidates fd and prevents repeated close calls"
else
    fail "Test 6 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 7: ByteBuffer Binary Serialization over Socket & Offset Transmit
# ------------------------------------------------------------------------------
echo "Test 7: ByteBuffer binary serialization over live TCP stream..."
cat << 'EOF' > "$TMP_DIR/py_client7.py"
import socket, struct, time
time.sleep(0.05)
s = socket.socket()
s.connect(('127.0.0.1', 29407))

# 1. Read first frame (sent with reader_pos == 0): 8 bytes
frame1 = s.recv(8)
assert len(frame1) == 8, f"Expected 8 bytes, got {len(frame1)}"
magic1, msg_id1, val1 = struct.unpack(">HHI", frame1)
assert magic1 == 0xCAFE
assert msg_id1 == 100
assert val1 == 987654

# 2. Read second frame (sent after consuming 4-byte prefix): 8 bytes
frame2 = s.recv(8)
assert len(frame2) == 8, f"Expected 8 bytes, got {len(frame2)}"
magic2, msg_id2, val2 = struct.unpack(">HHI", frame2)
assert magic2 == 0xCAFE
assert msg_id2 == 200
assert val2 == 123456

# Reply with response frame: magic 0xBEEF (u16), id 101 (u16), value 456789 (u32)
s.sendall(struct.pack(">HHI", 0xBEEF, 101, 456789))
time.sleep(0.05)
s.close()
EOF

cat << EOF > "$TMP_DIR/test7_byte_buffer.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server(29407);
    if (server.fd < 0) { return 1; }

    execute_cmd("python3 $TMP_DIR/py_client7.py &");

    let client_fd: int = server.accept();
    if (client_fd < 0) {
        server.close();
        return 2;
    }
    let client: TcpClient = create_client(client_fd);

    // 1. Buffer with no readable bytes: must return 0 immediately without sending
    let empty_buf: ByteBuffer = byte_buffer_new(16);
    let zero_sent: int = tcp_send_buffer(client.fd, &empty_buf);
    if (zero_sent != 0) {
        print("empty send failed");
        client.close();
        server.close();
        return 3;
    }

    // 2. Buffer sending from reader_pos == 0
    let send_buf: ByteBuffer = byte_buffer_new(32);
    send_buf.write_u16_be(51966);  // 0xCAFE
    send_buf.write_u16_be(100);
    send_buf.write_u32_be(987654);

    let sent: int = tcp_send_buffer(client.fd, &send_buf);
    if (sent != 8 || send_buf.reader_pos != 8 || send_buf.readable_bytes() != 0) {
        print("tcp_send_buffer pos 0 failed");
        client.close();
        server.close();
        return 4;
    }

    // 3. Repeated send on exhausted buffer returns 0
    let exhausted_sent: int = tcp_send_buffer(client.fd, &send_buf);
    if (exhausted_sent != 0) {
        print("exhausted send failed");
        client.close();
        server.close();
        return 5;
    }

    // 4. Consuming a prefix before sending: only unread suffix should be transmitted
    let prefix_buf: ByteBuffer = byte_buffer_new(32);
    prefix_buf.write_u32_be(305419896);  // 0x12345678 (4-byte prefix to consume)
    prefix_buf.write_u16_be(51966);      // 0xCAFE
    prefix_buf.write_u16_be(200);
    prefix_buf.write_u32_be(123456);

    // Consume the 4-byte prefix
    let prefix: int = prefix_buf.read_u32_be();
    if (prefix != 305419896 || prefix_buf.reader_pos != 4 || prefix_buf.readable_bytes() != 8) {
        print("prefix consumption failed");
        client.close();
        server.close();
        return 6;
    }

    // Send: transmits ONLY the 8 remaining bytes starting at reader_pos (4)
    let suffix_sent: int = tcp_send_buffer(client.fd, &prefix_buf);
    if (suffix_sent != 8 || prefix_buf.reader_pos != 12) {
        print("suffix send failed");
        client.close();
        server.close();
        return 7;
    }

    // 5. Error handling: sending on invalid fd returns -1 and preserves reader_pos
    let bad_buf: ByteBuffer = byte_buffer_new(16);
    bad_buf.write_u32_be(112233);
    let bad_sent: int = tcp_send_buffer(-1, &bad_buf);
    if (bad_sent != -1 || bad_buf.reader_pos != 0) {
        print("bad fd send failed");
        client.close();
        server.close();
        return 8;
    }

    // 6. Receive incoming binary frame into ByteBuffer
    let recv_buf: ByteBuffer = byte_buffer_new(32);
    let recvd: int = tcp_recv_buffer(client.fd, &recv_buf, 8);
    if (recvd != 8) {
        print("tcp_recv_buffer failed");
        client.close();
        server.close();
        return 9;
    }

    let magic: int = recv_buf.read_u16_be();
    let msg_id: int = recv_buf.read_u16_be();
    let val: int = recv_buf.read_u32_be();

    if (magic != 48879 || msg_id != 101 || val != 456789) {
        print("Frame payload decoding failed");
        client.close();
        server.close();
        return 10;
    }

    print("ByteBuffer Network OK");
    client.close();
    server.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test7_byte_buffer.cco" -o "$TMP_DIR/test7_byte_buffer"
OUT=$("$TMP_DIR/test7_byte_buffer")
if [[ "$OUT" == "ByteBuffer Network OK" ]]; then
    pass "ByteBuffer integrates with socket streams for structured binary framing"
else
    fail "Test 7 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 8: Result Sum Type Integration (tcp_recv_bytes_result)
# ------------------------------------------------------------------------------
echo "Test 8: Result sum type integration (tcp_recv_bytes_result)..."
cat << 'EOF' > "$TMP_DIR/test8_result.cco"
import "std/net.cco";

fn main() -> int {
    let buf: int[] = alloc(int, 8);

    // On invalid FD, tcp_recv_bytes_result returns ResultInt.Err
    let err_res: ResultInt = tcp_recv_bytes_result(-1, buf, 8);
    match err_res {
        ResultInt.Ok { value } => {
            print("Expected Err on invalid fd");
            return 1;
        }
        ResultInt.Err { message } => {
            if (!equals(message, "Socket receive error")) {
                print(concat("Unexpected err msg: ", message));
                return 2;
            }
        }
    }

    print("ResultInt Network OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test8_result.cco" -o "$TMP_DIR/test8_result"
OUT=$("$TMP_DIR/test8_result")
if [[ "$OUT" == "ResultInt Network OK" ]]; then
    pass "tcp_recv_bytes_result correctly returns ResultInt error on failed socket"
else
    fail "Test 8 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 9: Valgrind Memory Leak Verification Across All Test Binaries
# ------------------------------------------------------------------------------
echo "Test 9: Valgrind memory leak verification across all test binaries..."
VALG_FAIL=0
for bin in "$TMP_DIR"/test1_raw_tcp "$TMP_DIR"/test2_binary_nul "$TMP_DIR"/test3_explicit_len "$TMP_DIR"/test4_eof "$TMP_DIR"/test5_invalid_fd "$TMP_DIR"/test6_lifecycle "$TMP_DIR"/test7_byte_buffer "$TMP_DIR"/test8_result; do
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
    pass "All Phase 18D Network I/O test binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# ------------------------------------------------------------------------------
# Test 10: Negative Boundary — Compile-Time Rejection by Native Backend
# ------------------------------------------------------------------------------
echo "Test 10: Negative boundary — compile-time rejection by native backend..."
set +e
NATIVE_ERR=$("$CCO" "$TMP_DIR/test8_result.cco" --use-native -o "$TMP_DIR/native_bin" 2>&1)
NATIVE_CODE=$?
set -e
if [[ $NATIVE_CODE -eq 0 ]]; then
    fail "Native backend boundary" "Expected native backend to reject enum/class network code"
else
    if [[ "$NATIVE_ERR" == *"Cco IR verification failed"* || "$NATIVE_ERR" == *"does not support"* ]]; then
        pass "Native backend cleanly rejected unsupported network abstractions with expected diagnostic"
    else
        fail "Native backend diagnostic" "Unexpected error message: $NATIVE_ERR"
    fi
fi

echo "=============================================================="
echo "Phase 18D Milestone 2 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
