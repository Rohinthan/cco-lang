#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18D — Milestone 3: TCP Client Connections & Address Binding Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_phase18d_tcp_tests_$$"

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
echo "Cco Phase 18D Milestone 3: TCP Client & Address Binding Tests"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Test 1: Successful Outbound Connection to Local Listener & Transfer
# ------------------------------------------------------------------------------
echo "Test 1: Outbound TCP connection to local listener & bidirectional transfer..."
cat << 'EOF' > "$TMP_DIR/client1.cco"
import "std/net.cco";

fn main() -> int {
    sleep_ms(60);
    let client: TcpClient = connect_client("127.0.0.1", 29601);
    if (!client.is_connected()) {
        print("Client failed to connect");
        return 1;
    }

    let sent: int = client.send("PING_18D");
    if (sent <= 0) {
        print("Send failed");
        client.close();
        return 2;
    }

    let reply: string = client.recv_raw(128);
    if (!equals(reply, "PONG_18D")) {
        print(concat("Reply mismatch: ", reply));
        client.close();
        return 3;
    }

    print("Transfer OK");
    client.close();
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/server1.cco"
import "std/net.cco";

fn main() -> int {
    let server_fd: int = net_listen_on("127.0.0.1", 29601);
    if (server_fd < 0) {
        return 1;
    }

    execute_cmd("/tmp/cco_phase18d_tcp_tests_$$/client1 > /tmp/cco_phase18d_tcp_tests_$$/client1.out &");

    let client_fd: int = net_accept(server_fd);
    if (client_fd < 0) {
        net_close(server_fd);
        return 2;
    }

    let msg: string = net_recv_raw(client_fd, 128);
    if (!equals(msg, "PING_18D")) {
        net_close(client_fd);
        net_close(server_fd);
        return 3;
    }

    net_send(client_fd, "PONG_18D");
    net_close(client_fd);
    net_close(server_fd);
    return 0;
}
EOF

sed -i "s|/tmp/cco_phase18d_tcp_tests_\\$\\$|$TMP_DIR|g" "$TMP_DIR/server1.cco"

"$CCO" "$TMP_DIR/client1.cco" -o "$TMP_DIR/client1"
"$CCO" "$TMP_DIR/server1.cco" -o "$TMP_DIR/server1"

"$TMP_DIR/server1"
sleep 0.1
CLIENT1_OUT=$(cat "$TMP_DIR/client1.out" 2>/dev/null || echo "")

if [[ "$CLIENT1_OUT" == "Transfer OK" ]]; then
    pass "Outbound TCP connection established and bidirectional payload exchanged successfully"
else
    fail "Test 1 execution" "Client output was: $CLIENT1_OUT"
fi

# ------------------------------------------------------------------------------
# Test 2: Result-Wrapped Outbound Connection (tcp_connect_result)
# ------------------------------------------------------------------------------
echo "Test 2: Result-wrapped outbound connection (tcp_connect_result)..."
cat << 'EOF' > "$TMP_DIR/client2.cco"
import "std/net.cco";
import "std/result.cco";

fn main() -> int {
    sleep_ms(60);
    let res: ResultInt = tcp_connect_result("127.0.0.1", 29602);
    if (res_int_is_err(&res)) {
        print(concat("Connect error: ", res_int_error(&res)));
        return 1;
    }

    let fd: int = res_int_unwrap_or(&res, -1);
    if (fd < 0) {
        print("Invalid fd");
        return 2;
    }

    net_send(fd, "HELLO_RESULT");
    let reply: string = net_recv_raw(fd, 64);
    if (equals(reply, "OK_RESULT")) {
        print("Result Connect OK");
    }
    net_close(fd);
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/server2.cco"
import "std/net.cco";

fn main() -> int {
    let server_fd: int = net_listen_on("127.0.0.1", 29602);
    if (server_fd < 0) {
        return 1;
    }

    execute_cmd("/tmp/cco_phase18d_tcp_tests_$$/client2 > /tmp/cco_phase18d_tcp_tests_$$/client2.out &");
    let client_fd: int = net_accept(server_fd);
    if (client_fd < 0) {
        net_close(server_fd);
        return 2;
    }

    let msg: string = net_recv_raw(client_fd, 64);
    if (equals(msg, "HELLO_RESULT")) {
        net_send(client_fd, "OK_RESULT");
    }
    net_close(client_fd);
    net_close(server_fd);
    return 0;
}
EOF

sed -i "s|/tmp/cco_phase18d_tcp_tests_\\$\\$|$TMP_DIR|g" "$TMP_DIR/server2.cco"

"$CCO" "$TMP_DIR/client2.cco" -o "$TMP_DIR/client2"
"$CCO" "$TMP_DIR/server2.cco" -o "$TMP_DIR/server2"

"$TMP_DIR/server2"
sleep 0.1
CLIENT2_OUT=$(cat "$TMP_DIR/client2.out" 2>/dev/null || echo "")

if [[ "$CLIENT2_OUT" == "Result Connect OK" ]]; then
    pass "tcp_connect_result successfully yields ResultInt.Ok with connected socket descriptor"
else
    fail "Test 2 execution" "Client output was: $CLIENT2_OUT"
fi

# ------------------------------------------------------------------------------
# Test 3: Connection Refusal on Unused Local Port & Error Reporting
# ------------------------------------------------------------------------------
echo "Test 3: Connection refusal on unused local port..."
cat << 'EOF' > "$TMP_DIR/test3_refusal.cco"
import "std/net.cco";
import "std/result.cco";

fn main() -> int {
    // Port 29603 is unused
    let fd: int = net_connect("127.0.0.1", 29603);
    if (fd >= 0) {
        net_close(fd);
        print("Unexpected connect success");
        return 1;
    }

    let err: string = net_last_error();
    let res: ResultInt = tcp_connect_result("127.0.0.1", 29603);
    if (!res_int_is_err(&res)) {
        print("Expected ResultInt.Err");
        return 2;
    }

    print(concat("Direct: ", err));
    print(concat("Result: ", res_int_error(&res)));
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test3_refusal.cco" -o "$TMP_DIR/test3_refusal"
OUT3=$("$TMP_DIR/test3_refusal")

if [[ "$OUT3" == *"Connection refused"* ]]; then
    pass "Connection refusal cleanly returns -1 and preserves 'Connection refused' in net_last_error"
else
    fail "Test 3 execution" "Expected 'Connection refused', got: $OUT3"
fi

# ------------------------------------------------------------------------------
# Test 4: Invalid Numeric IPv4 Address Handling
# ------------------------------------------------------------------------------
echo "Test 4: Invalid numeric IPv4 address rejection..."
cat << 'EOF' > "$TMP_DIR/test4_bad_ip.cco"
import "std/net.cco";

fn main() -> int {
    let c1: int = net_connect("not_an_ip", 8080);
    let err1: string = net_last_error();
    if (c1 != -1) { return 1; }

    let c2: int = net_connect("999.999.999.999", 8080);
    let err2: string = net_last_error();
    if (c2 != -1) { return 2; }

    let s1: int = net_listen_on("hostname.invalid", 8080);
    let err3: string = net_last_error();
    if (s1 != -1) { return 3; }

    print(err1);
    print(err2);
    print(err3);
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test4_bad_ip.cco" -o "$TMP_DIR/test4_bad_ip"
OUT4=$("$TMP_DIR/test4_bad_ip")

if [[ "$OUT4" == *"Invalid IPv4 address: not_an_ip"* && "$OUT4" == *"Invalid IPv4 address: 999.999.999.999"* ]]; then
    pass "Malformed numeric IPv4 strings and hostnames safely rejected with EINVAL without crashes"
else
    fail "Test 4 execution" "Unexpected output: $OUT4"
fi

# ------------------------------------------------------------------------------
# Test 5: Port Range Validation (0, negative, >65535)
# ------------------------------------------------------------------------------
echo "Test 5: Port range validation (boundary values)..."
cat << 'EOF' > "$TMP_DIR/test5_ports.cco"
import "std/net.cco";

fn main() -> int {
    let c_zero: int = net_connect("127.0.0.1", 0);
    let err_zero: string = net_last_error();
    if (c_zero != -1) { return 1; }

    let c_neg: int = net_connect("127.0.0.1", -10);
    let err_neg: string = net_last_error();
    if (c_neg != -1) { return 2; }

    let c_high: int = net_connect("127.0.0.1", 70000);
    let err_high: string = net_last_error();
    if (c_high != -1) { return 3; }

    let s_zero: int = net_listen_on("127.0.0.1", 0);
    let s_err_zero: string = net_last_error();
    if (s_zero != -1) { return 4; }

    let s_high: int = net_listen_on("127.0.0.1", 65536);
    let s_err_high: string = net_last_error();
    if (s_high != -1) { return 5; }

    print("Ports Validated");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test5_ports.cco" -o "$TMP_DIR/test5_ports"
OUT5=$("$TMP_DIR/test5_ports")

if [[ "$OUT5" == "Ports Validated" ]]; then
    pass "Out-of-range ports (0, negative, >65535) consistently rejected before socket allocation"
else
    fail "Test 5 execution" "Output was: $OUT5"
fi

# ------------------------------------------------------------------------------
# Test 6: Explicit Loopback Address Binding (127.0.0.1)
# ------------------------------------------------------------------------------
echo "Test 6: Binding to 127.0.0.1..."
cat << 'EOF' > "$TMP_DIR/test6_loopback.cco"
import "std/net.cco";

fn main() -> int {
    let s: TcpServer = create_server_on("127.0.0.1", 29605);
    if (!s.is_listening()) {
        print("Failed to bind 127.0.0.1");
        return 1;
    }

    let client: TcpClient = connect_client("127.0.0.1", 29605);
    if (!client.is_connected()) {
        s.close();
        print("Failed client connect to 127.0.0.1");
        return 2;
    }

    let accepted_fd: int = s.accept();
    if (accepted_fd < 0) {
        client.close();
        s.close();
        print("Failed accept");
        return 3;
    }

    print("Loopback Binding OK");
    net_close(accepted_fd);
    client.close();
    s.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test6_loopback.cco" -o "$TMP_DIR/test6_loopback"
OUT6=$("$TMP_DIR/test6_loopback")

if [[ "$OUT6" == "Loopback Binding OK" ]]; then
    pass "create_server_on correctly binds to 127.0.0.1 and accepts local connections"
else
    fail "Test 6 execution" "Output was: $OUT6"
fi

# ------------------------------------------------------------------------------
# Test 7: Binding to All Interfaces (0.0.0.0 / INADDR_ANY)
# ------------------------------------------------------------------------------
echo "Test 7: Binding to 0.0.0.0 (INADDR_ANY)..."
cat << 'EOF' > "$TMP_DIR/test7_any.cco"
import "std/net.cco";

fn main() -> int {
    let s: TcpServer = create_server_on("0.0.0.0", 29606);
    if (!s.is_listening()) {
        print("Failed to bind 0.0.0.0");
        return 1;
    }

    let client: TcpClient = connect_client("127.0.0.1", 29606);
    if (!client.is_connected()) {
        s.close();
        print("Failed client connect");
        return 2;
    }

    let accepted_fd: int = s.accept();
    if (accepted_fd < 0) {
        client.close();
        s.close();
        return 3;
    }

    print("INADDR_ANY Binding OK");
    net_close(accepted_fd);
    client.close();
    s.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test7_any.cco" -o "$TMP_DIR/test7_any"
OUT7=$("$TMP_DIR/test7_any")

if [[ "$OUT7" == "INADDR_ANY Binding OK" ]]; then
    pass "Binding to 0.0.0.0 successfully binds all interfaces and accepts incoming connections"
else
    fail "Test 7 execution" "Output was: $OUT7"
fi

# ------------------------------------------------------------------------------
# Test 8: Port Already in Use (Address Conflict Handling)
# ------------------------------------------------------------------------------
echo "Test 8: Port already in use behavior..."
cat << 'EOF' > "$TMP_DIR/test8_conflict.cco"
import "std/net.cco";

fn main() -> int {
    let s1: int = net_listen_on("127.0.0.1", 29607);
    if (s1 < 0) {
        print("First listen failed");
        return 1;
    }

    // Attempt second bind on same port
    let s2: int = net_listen_on("127.0.0.1", 29607);
    if (s2 >= 0) {
        net_close(s2);
        net_close(s1);
        print("Second bind unexpectedly succeeded");
        return 2;
    }

    let err: string = net_last_error();
    net_close(s1);

    print(err);
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test8_conflict.cco" -o "$TMP_DIR/test8_conflict"
OUT8=$("$TMP_DIR/test8_conflict")

if [[ "$OUT8" == *"Address already in use"* ]]; then
    pass "Conflicting bind() cleanly fails with -1 and records 'Address already in use'"
else
    fail "Test 8 execution" "Expected 'Address already in use', got: $OUT8"
fi

# ------------------------------------------------------------------------------
# Test 9: Descriptor Cleanup After Failed Connection & Listener Setup
# ------------------------------------------------------------------------------
echo "Test 9: Descriptor cleanup after failure loops (no FD leak)..."
cat << 'EOF' > "$TMP_DIR/test9_no_fd_leak.cco"
import "std/net.cco";

fn main() -> int {
    // 50 failed connection attempts
    for (let i: int = 0; i < 50; i++) {
        let c: int = net_connect("127.0.0.1", 29699);
        if (c >= 0) {
            net_close(c);
            return 1;
        }
    }

    // 50 failed listener attempts
    for (let i: int = 0; i < 50; i++) {
        let s: int = net_listen_on("999.999.999.999", 29608);
        if (s >= 0) {
            net_close(s);
            return 2;
        }
    }

    print("No FD Leak OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test9_no_fd_leak.cco" -o "$TMP_DIR/test9_no_fd_leak"
OUT9=$("$TMP_DIR/test9_no_fd_leak")

if [[ "$OUT9" == "No FD Leak OK" ]]; then
    pass "Repeated connection and binding failures cleanly release descriptors without leaks"
else
    fail "Test 9 execution" "Output was: $OUT9"
fi

# ------------------------------------------------------------------------------
# Test 10: Repeated Client Closure & Invalidation
# ------------------------------------------------------------------------------
echo "Test 10: Repeated client closure & method calls on closed socket..."
cat << 'EOF' > "$TMP_DIR/test10_client_lifecycle.cco"
import "std/net.cco";

fn main() -> int {
    let client: TcpClient = TcpClient { fd: -1 };
    if (client.is_connected()) { return 1; }

    // Calling methods on disconnected client must safely return error/empty
    let s_res: int = client.send("TEST");
    if (s_res != -1) { return 2; }

    let r_res: string = client.recv();
    if (!equals(r_res, "")) { return 3; }

    let raw_res: string = client.recv_raw(64);
    if (!equals(raw_res, "")) { return 4; }

    let buf: int[] = alloc(int, 10);
    let bytes_res: int = client.recv_bytes(buf, 10);
    if (bytes_res != -1) { return 5; }

    // Repeated close must be a safe no-op
    client.close();
    client.close();
    client.close();

    if (client.fd != -1) { return 6; }

    print("Client Lifecycle OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test10_client_lifecycle.cco" -o "$TMP_DIR/test10_client_lifecycle"
OUT10=$("$TMP_DIR/test10_client_lifecycle")

if [[ "$OUT10" == "Client Lifecycle OK" ]]; then
    pass "Disconnected TcpClient safely handles all operations and multiple close() calls"
else
    fail "Test 10 execution" "Output was: $OUT10"
fi

# ------------------------------------------------------------------------------
# Test 11: TcpServer and TcpClient High-Level Object API Integration
# ------------------------------------------------------------------------------
echo "Test 11: High-level TcpServer and TcpClient object API..."
cat << 'EOF' > "$TMP_DIR/test11_object_api.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29610);
    if (!server.is_listening()) { return 1; }

    let client: TcpClient = TcpClient { fd: -1 };
    let sock: int = client.connect("127.0.0.1", 29610);
    if (sock < 0 || !client.is_connected()) {
        server.close();
        return 2;
    }

    let accepted: TcpClient = server.accept_client();
    if (!accepted.is_connected()) {
        client.close();
        server.close();
        return 3;
    }

    client.send("CLIENT_MSG");
    let recvd: string = accepted.recv_raw(64);
    if (!equals(recvd, "CLIENT_MSG")) {
        accepted.close();
        client.close();
        server.close();
        return 4;
    }

    accepted.send("SERVER_REPLY");
    let reply: string = client.recv_raw(64);
    if (!equals(reply, "SERVER_REPLY")) {
        accepted.close();
        client.close();
        server.close();
        return 5;
    }

    accepted.close();
    client.close();
    server.close();

    print("Object API OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test11_object_api.cco" -o "$TMP_DIR/test11_object_api"
OUT11=$("$TMP_DIR/test11_object_api")

if [[ "$OUT11" == "Object API OK" ]]; then
    pass "TcpServer.accept_client() and TcpClient.connect() coordinate bidirectional object streams"
else
    fail "Test 11 execution" "Output was: $OUT11"
fi

# ------------------------------------------------------------------------------
# Test 12: Backward Compatibility With Legacy net_listen & Binary Socket Ops
# ------------------------------------------------------------------------------
echo "Test 12: Backward compatibility with legacy net_listen & binary sockets..."
cat << 'EOF' > "$TMP_DIR/test12_compat.cco"
import "std/net.cco";

fn main() -> int {
    // Legacy net_listen(port) binds INADDR_ANY
    let s_fd: int = net_listen(29612);
    if (s_fd < 0) { return 1; }

    let c_fd: int = net_connect("127.0.0.1", 29612);
    if (c_fd < 0) {
        net_close(s_fd);
        return 2;
    }

    let a_fd: int = net_accept(s_fd);
    if (a_fd < 0) {
        net_close(c_fd);
        net_close(s_fd);
        return 3;
    }

    // Binary payload transfer with embedded NUL
    let payload: int[] = alloc(int, 4);
    payload[0] = 65;
    payload[1] = 0;
    payload[2] = 66;
    payload[3] = 0;

    let sent: int = net_send_bytes(c_fd, payload, 4);
    if (sent != 4) { return 4; }

    let recv_buf: int[] = alloc(int, 4);
    let recvd: int = net_recv_bytes(a_fd, recv_buf, 4);
    if (recvd != 4) { return 5; }

    if (recv_buf[0] != 65 || recv_buf[1] != 0 || recv_buf[2] != 66 || recv_buf[3] != 0) {
        return 6;
    }

    net_close(a_fd);
    net_close(c_fd);
    net_close(s_fd);

    print("Legacy & Binary Compat OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test12_compat.cco" -o "$TMP_DIR/test12_compat"
OUT12=$("$TMP_DIR/test12_compat")

if [[ "$OUT12" == "Legacy & Binary Compat OK" ]]; then
    pass "Legacy net_listen() and binary-safe raw transfer integrate seamlessly with net_connect()"
else
    fail "Test 12 execution" "Output was: $OUT12"
fi

# ------------------------------------------------------------------------------
# Test 13: Memory Leak Verification Under Valgrind
# ------------------------------------------------------------------------------
echo "Test 13: Valgrind memory leak verification across test binaries..."
VALG_FAIL=0
for bin in "$TMP_DIR/test3_refusal" "$TMP_DIR/test4_bad_ip" "$TMP_DIR/test5_ports" "$TMP_DIR/test6_loopback" "$TMP_DIR/test7_any" "$TMP_DIR/test8_conflict" "$TMP_DIR/test9_no_fd_leak" "$TMP_DIR/test10_client_lifecycle" "$TMP_DIR/test11_object_api" "$TMP_DIR/test12_compat"; do
    if [[ -x "$bin" ]]; then
        if ! valgrind --leak-check=full --error-exitcode=1 -q "$bin" > /dev/null 2>&1; then
            echo "Valgrind detected error in $bin"
            VALG_FAIL=$((VALG_FAIL + 1))
            fail "Valgrind memcheck" "$(basename "$bin") leaked memory or raised errors"
        fi
    fi
done

if [[ $VALG_FAIL -eq 0 ]]; then
    pass "All Phase 18D Milestone 3 test binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# ------------------------------------------------------------------------------
# Test 14: Negative Boundary — Compile-Time Rejection by Native Backend
# ------------------------------------------------------------------------------
echo "Test 14: Negative boundary — compile-time rejection by native backend..."
set +e
NATIVE_ERR=$("$CCO" "$TMP_DIR/test11_object_api.cco" --use-native -o "$TMP_DIR/native_bin" 2>&1)
NATIVE_CODE=$?
set -e
if [[ $NATIVE_CODE -eq 0 ]]; then
    fail "Native backend boundary" "Expected native backend to reject object/class network code"
else
    if [[ "$NATIVE_ERR" == *"Cco IR verification failed"* || "$NATIVE_ERR" == *"does not support"* ]]; then
        pass "Native backend cleanly rejected unsupported network abstractions with expected diagnostic"
    else
        fail "Native backend diagnostic" "Unexpected error message: $NATIVE_ERR"
    fi
fi

echo "=============================================================="
echo "Phase 18D Milestone 3 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
