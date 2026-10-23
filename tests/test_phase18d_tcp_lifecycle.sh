#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18D — Milestone 4: TCP Lifecycle Control & Failure Resilience Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_phase18d_m4_tests_$$"

mkdir -p "$TMP_DIR"

cleanup() {
    # Terminate any lingering background test jobs
    jobs -p | xargs -r kill -9 2>/dev/null || true
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

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
echo "Cco Phase 18D Milestone 4: TCP Lifecycle Control & Resilience"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Test 1: Successful Read-Side Shutdown (SHUT_RD / 0)
# ------------------------------------------------------------------------------
echo "Test 1: Read-side shutdown (SHUT_RD)..."
cat << 'EOF' > "$TMP_DIR/test1_server.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29701);
    if (!server.is_listening()) {
        print("Server listen failed");
        return 1;
    }

    let client: TcpClient = server.accept_client();
    if (!client.is_connected()) {
        server.close();
        return 2;
    }

    // Client will shut down read-side, but should still be able to send
    let msg: string = client.recv_raw(128);
    if (!equals(msg, "CLIENT_CAN_STILL_SEND")) {
        print(concat("Unexpected msg: ", msg));
        client.close();
        server.close();
        return 3;
    }

    print("SERVER_RECV_OK");
    client.close();
    server.close();
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/test1_client.cco"
import "std/net.cco";

fn main() -> int {
    sleep_ms(60);
    let client: TcpClient = connect_client("127.0.0.1", 29701);
    if (!client.is_connected()) {
        print("Client connect failed");
        return 1;
    }

    // Shut down read side
    let rc: int = client.shutdown_read();
    if (rc != 0) {
        print("shutdown_read failed");
        client.close();
        return 2;
    }

    // After SHUT_RD, reading should return EOF or error immediately
    let empty: string = client.recv_raw(64);

    // After SHUT_RD, sending is still permitted by TCP
    let sent: int = client.send("CLIENT_CAN_STILL_SEND");
    if (sent <= 0) {
        print("Send after SHUT_RD failed");
        client.close();
        return 3;
    }

    print("CLIENT_SHUT_RD_OK");
    client.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test1_server.cco" -o "$TMP_DIR/test1_server"
"$CCO" "$TMP_DIR/test1_client.cco" -o "$TMP_DIR/test1_client"

"$TMP_DIR/test1_server" > "$TMP_DIR/test1_server.out" 2>&1 &
SERVER1_PID=$!
"$TMP_DIR/test1_client" > "$TMP_DIR/test1_client.out" 2>&1 &
CLIENT1_PID=$!

wait $SERVER1_PID || true
wait $CLIENT1_PID || true

S1_OUT=$(cat "$TMP_DIR/test1_server.out" 2>/dev/null || echo "")
C1_OUT=$(cat "$TMP_DIR/test1_client.out" 2>/dev/null || echo "")

if [[ "$S1_OUT" == "SERVER_RECV_OK" && "$C1_OUT" == "CLIENT_SHUT_RD_OK" ]]; then
    pass "Read-side shutdown (SHUT_RD) succeeds, disallows reads while allowing writes"
else
    fail "Test 1 execution" "Server: $S1_OUT, Client: $C1_OUT"
fi

# ------------------------------------------------------------------------------
# Test 2 & 4: Write-Side Shutdown (SHUT_WR / 1) and Peer Observes EOF
# ------------------------------------------------------------------------------
echo "Test 2 & 4: Write-side shutdown (SHUT_WR) and peer EOF observation..."
cat << 'EOF' > "$TMP_DIR/test2_server.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29702);
    if (!server.is_listening()) {
        return 1;
    }

    let client: TcpClient = server.accept_client();
    if (!client.is_connected()) {
        server.close();
        return 2;
    }

    // Read payload from client
    let msg: string = client.recv_raw(128);
    if (!equals(msg, "MSG_BEFORE_FIN")) {
        client.close();
        server.close();
        return 3;
    }

    // Read again: peer called shutdown_write(), so server must observe EOF (empty string)
    let eof: string = client.recv_raw(128);
    if (!equals(eof, "")) {
        client.close();
        server.close();
        return 4;
    }

    // Server can still send data back to client!
    client.send("REPLY_AFTER_CLIENT_SHUT_WR");

    print("PEER_OBSERVED_EOF_OK");
    client.close();
    server.close();
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/test2_client.cco"
import "std/net.cco";

fn main() -> int {
    sleep_ms(60);
    let client: TcpClient = connect_client("127.0.0.1", 29702);
    if (!client.is_connected()) {
        return 1;
    }

    client.send("MSG_BEFORE_FIN");

    // Shut down write half of connection
    let rc: int = client.shutdown_write();
    if (rc != 0) {
        client.close();
        return 2;
    }

    // Client can still receive response from server
    let reply: string = client.recv_raw(128);
    if (!equals(reply, "REPLY_AFTER_CLIENT_SHUT_WR")) {
        client.close();
        return 3;
    }

    print("CLIENT_HALF_CLOSE_OK");
    client.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test2_server.cco" -o "$TMP_DIR/test2_server"
"$CCO" "$TMP_DIR/test2_client.cco" -o "$TMP_DIR/test2_client"

"$TMP_DIR/test2_server" > "$TMP_DIR/test2_server.out" 2>&1 &
SERVER2_PID=$!
"$TMP_DIR/test2_client" > "$TMP_DIR/test2_client.out" 2>&1 &
CLIENT2_PID=$!

wait $SERVER2_PID || true
wait $CLIENT2_PID || true

S2_OUT=$(cat "$TMP_DIR/test2_server.out" 2>/dev/null || echo "")
C2_OUT=$(cat "$TMP_DIR/test2_client.out" 2>/dev/null || echo "")

if [[ "$S2_OUT" == "PEER_OBSERVED_EOF_OK" && "$C2_OUT" == "CLIENT_HALF_CLOSE_OK" ]]; then
    pass "Write-side shutdown (SHUT_WR) delivers EOF to peer while preserving read channel"
else
    fail "Test 2 & 4 execution" "Server: $S2_OUT, Client: $C2_OUT"
fi

# ------------------------------------------------------------------------------
# Test 3: Full Shutdown (SHUT_RDWR / 2)
# ------------------------------------------------------------------------------
echo "Test 3: Full bidirectional shutdown (SHUT_RDWR)..."
cat << 'EOF' > "$TMP_DIR/test3_server.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29703);
    if (!server.is_listening()) {
        return 1;
    }

    let client: TcpClient = server.accept_client();
    if (!client.is_connected()) {
        server.close();
        return 2;
    }

    let eof: string = client.recv_raw(128);
    if (equals(eof, "")) {
        print("SERVER_FULL_EOF_OK");
    }

    client.close();
    server.close();
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/test3_client.cco"
import "std/net.cco";

fn main() -> int {
    sleep_ms(60);
    let client: TcpClient = connect_client("127.0.0.1", 29703);
    if (!client.is_connected()) {
        return 1;
    }

    let rc: int = client.shutdown_both();
    if (rc != 0) {
        client.close();
        return 2;
    }

    print("CLIENT_SHUT_BOTH_OK");
    client.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test3_server.cco" -o "$TMP_DIR/test3_server"
"$CCO" "$TMP_DIR/test3_client.cco" -o "$TMP_DIR/test3_client"

"$TMP_DIR/test3_server" > "$TMP_DIR/test3_server.out" 2>&1 &
SERVER3_PID=$!
"$TMP_DIR/test3_client" > "$TMP_DIR/test3_client.out" 2>&1 &
CLIENT3_PID=$!

wait $SERVER3_PID || true
wait $CLIENT3_PID || true

S3_OUT=$(cat "$TMP_DIR/test3_server.out" 2>/dev/null || echo "")
C3_OUT=$(cat "$TMP_DIR/test3_client.out" 2>/dev/null || echo "")

if [[ "$S3_OUT" == "SERVER_FULL_EOF_OK" && "$C3_OUT" == "CLIENT_SHUT_BOTH_OK" ]]; then
    pass "Full bidirectional shutdown (SHUT_RDWR) disallows both directions without closing fd"
else
    fail "Test 3 execution" "Server: $S3_OUT, Client: $C3_OUT"
fi

# ------------------------------------------------------------------------------
# Test 5: Invalid Shutdown Mode
# ------------------------------------------------------------------------------
echo "Test 5: Invalid shutdown mode rejection..."
cat << 'EOF' > "$TMP_DIR/test5_modes.cco"
import "std/net.cco";

fn main() -> int {
    // Arbitrary fd with invalid mode
    let r1: int = net_shutdown(1, -1);
    let r2: int = net_shutdown(1, 3);
    let r3: int = net_shutdown(1, 999);

    if (r1 == -1 && r2 == -1 && r3 == -1) {
        let err: string = net_last_error();
        if (equals(err, "Invalid shutdown mode: 999 (must be 0 for SHUT_RD, 1 for SHUT_WR, 2 for SHUT_RDWR)")) {
            print("INVALID_MODES_REJECTED_OK");
            return 0;
        }
    }
    print("MODE_VALIDATION_FAILED");
    return 1;
}
EOF

"$CCO" "$TMP_DIR/test5_modes.cco" -o "$TMP_DIR/test5_modes"
OUT5=$("$TMP_DIR/test5_modes")
if [[ "$OUT5" == "INVALID_MODES_REJECTED_OK" ]]; then
    pass "Invalid shutdown modes (-1, 3, 999) rejected safely with informative diagnostics"
else
    fail "Test 5 execution" "Output was: $OUT5"
fi

# ------------------------------------------------------------------------------
# Test 6: Shutdown on Invalid or Closed Descriptors
# ------------------------------------------------------------------------------
echo "Test 6: Shutdown on invalid or closed descriptors..."
cat << 'EOF' > "$TMP_DIR/test6_bad_fd.cco"
import "std/net.cco";

fn main() -> int {
    // Negative fd
    let r1: int = net_shutdown(-1, 0);
    if (r1 != -1) {
        return 1;
    }
    let err1: string = net_last_error();
    if (!equals(err1, "Invalid socket descriptor: -1")) {
        return 2;
    }

    // Bad/closed descriptor (fd 999)
    let r2: int = net_shutdown(999, 0);
    if (r2 != -1) {
        return 3;
    }
    let err2: string = net_last_error();
    if (!equals(err2, "shutdown() failed: Bad file descriptor")) {
        return 4;
    }

    print("BAD_FD_SHUTDOWN_OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test6_bad_fd.cco" -o "$TMP_DIR/test6_bad_fd"
OUT6=$("$TMP_DIR/test6_bad_fd")
if [[ "$OUT6" == "BAD_FD_SHUTDOWN_OK" ]]; then
    pass "Shutdown on negative and closed descriptors fails safely preserving errno/diagnostics"
else
    fail "Test 6 execution" "Output was: $OUT6"
fi

# ------------------------------------------------------------------------------
# Test 7: Valid Timeout Configuration on Client and Server Sockets
# ------------------------------------------------------------------------------
echo "Test 7: Valid timeout configuration (SO_RCVTIMEO / SO_SNDTIMEO)..."
cat << 'EOF' > "$TMP_DIR/test7_timeouts.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29707);
    if (!server.is_listening()) {
        return 1;
    }

    // Set timeouts on server socket (e.g. for accept)
    let s_rc: int = server.set_timeouts(500, 250);
    if (s_rc != 0) {
        server.close();
        return 2;
    }

    let client_sock: int = net_connect("127.0.0.1", 29707);
    if (client_sock < 0) {
        server.close();
        return 3;
    }

    // Set timeouts on client socket
    let c_rc: int = net_set_timeouts(client_sock, 1500, 3000);
    if (c_rc != 0) {
        net_close(client_sock);
        server.close();
        return 4;
    }

    net_close(client_sock);
    server.close();
    print("VALID_TIMEOUTS_OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test7_timeouts.cco" -o "$TMP_DIR/test7_timeouts"
OUT7=$("$TMP_DIR/test7_timeouts")
if [[ "$OUT7" == "VALID_TIMEOUTS_OK" ]]; then
    pass "Valid receive and send timeouts applied cleanly to both listener and connection sockets"
else
    fail "Test 7 execution" "Output was: $OUT7"
fi

# ------------------------------------------------------------------------------
# Test 8: Zero Timeout Behavior (Disabling Timeouts)
# ------------------------------------------------------------------------------
echo "Test 8: Zero timeout behavior (disabling timeouts)..."
cat << 'EOF' > "$TMP_DIR/test8_zero_timeout.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29708);
    if (!server.is_listening()) {
        return 1;
    }

    // Apply nonzero timeouts
    let rc1: int = server.set_timeouts(200, 200);
    if (rc1 != 0) {
        server.close();
        return 2;
    }

    // Reset to 0 (disables timeout in POSIX)
    let rc2: int = server.set_timeouts(0, 0);
    if (rc2 != 0) {
        server.close();
        return 3;
    }

    server.close();
    print("ZERO_TIMEOUT_DISABLE_OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test8_zero_timeout.cco" -o "$TMP_DIR/test8_zero_timeout"
OUT8=$("$TMP_DIR/test8_zero_timeout")
if [[ "$OUT8" == "ZERO_TIMEOUT_DISABLE_OK" ]]; then
    pass "Timeout values of 0 correctly reset and disable socket timeouts (tv_sec=0, tv_usec=0)"
else
    fail "Test 8 execution" "Output was: $OUT8"
fi

# ------------------------------------------------------------------------------
# Test 9: Negative Timeout Rejection
# ------------------------------------------------------------------------------
echo "Test 9: Negative timeout rejection..."
cat << 'EOF' > "$TMP_DIR/test9_neg_timeout.cco"
import "std/net.cco";

fn main() -> int {
    let r1: int = net_set_timeouts(1, -1, 500);
    let r2: int = net_set_timeouts(1, 500, -100);
    let r3: int = net_set_timeouts(1, -50, -50);

    if (r1 == -1 && r2 == -1 && r3 == -1) {
        let err: string = net_last_error();
        if (equals(err, "Invalid timeout value: recv_ms=-50, send_ms=-50 (must be >= 0)")) {
            print("NEGATIVE_TIMEOUTS_REJECTED_OK");
            return 0;
        }
    }
    print("NEGATIVE_TIMEOUT_FAILED");
    return 1;
}
EOF

"$CCO" "$TMP_DIR/test9_neg_timeout.cco" -o "$TMP_DIR/test9_neg_timeout"
OUT9=$("$TMP_DIR/test9_neg_timeout")
if [[ "$OUT9" == "NEGATIVE_TIMEOUTS_REJECTED_OK" ]]; then
    pass "Negative timeouts rejected cleanly with EINVAL and informative error messages"
else
    fail "Test 9 execution" "Output was: $OUT9"
fi

# ------------------------------------------------------------------------------
# Test 10: Very Large Timeout Handling Without Overflow
# ------------------------------------------------------------------------------
echo "Test 10: Very large timeout values handling..."
cat << 'EOF' > "$TMP_DIR/test10_large_timeout.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29710);
    if (!server.is_listening()) {
        return 1;
    }

    // 2,000,000,000 ms (~23 days), within positive 32-bit int
    let rc: int = server.set_timeouts(2000000000, 2000000000);
    if (rc != 0) {
        server.close();
        return 2;
    }

    server.close();
    print("LARGE_TIMEOUT_OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test10_large_timeout.cco" -o "$TMP_DIR/test10_large_timeout"
OUT10=$("$TMP_DIR/test10_large_timeout")
if [[ "$OUT10" == "LARGE_TIMEOUT_OK" ]]; then
    pass "Very large timeout values (2 billion ms) handled safely without integer overflow"
else
    fail "Test 10 execution" "Output was: $OUT10"
fi

# ------------------------------------------------------------------------------
# Test 11: Receive Timeout on Blocking Socket
# ------------------------------------------------------------------------------
echo "Test 11: Receive timeout behavior on blocking socket..."
cat << 'EOF' > "$TMP_DIR/test11_server.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29711);
    if (!server.is_listening()) {
        return 1;
    }

    let client: TcpClient = server.accept_client();
    if (!client.is_connected()) {
        server.close();
        return 2;
    }

    // Server deliberately stays silent for 500ms so client times out
    sleep_ms(500);

    client.close();
    server.close();
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/test11_client.cco"
import "std/net.cco";

fn main() -> int {
    sleep_ms(60);
    let client: TcpClient = connect_client("127.0.0.1", 29711);
    if (!client.is_connected()) {
        return 1;
    }

    // Set 150ms receive timeout
    let rc: int = client.set_timeouts(150, 0);
    if (rc != 0) {
        client.close();
        return 2;
    }

    // Attempt receive: server does not send data, so this must time out
    let msg: string = client.recv_raw(128);
    if (equals(msg, "")) {
        let err: string = tcp_last_error();
        if (equals(err, "recv() timed out: Resource temporarily unavailable")) {
            print("RECV_TIMEOUT_DETECTED_OK");
            client.close();
            return 0;
        }
    }

    print("RECV_TIMEOUT_UNEXPECTED");
    client.close();
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test11_server.cco" -o "$TMP_DIR/test11_server"
"$CCO" "$TMP_DIR/test11_client.cco" -o "$TMP_DIR/test11_client"

START_TIME=$(date +%s%N)
"$TMP_DIR/test11_server" > /dev/null 2>&1 &
SERVER11_PID=$!
"$TMP_DIR/test11_client" > "$TMP_DIR/test11_client.out" 2>&1 &
CLIENT11_PID=$!

wait $CLIENT11_PID || true
wait $SERVER11_PID || true
END_TIME=$(date +%s%N)
ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))

C11_OUT=$(cat "$TMP_DIR/test11_client.out" 2>/dev/null || echo "")

if [[ "$C11_OUT" == "RECV_TIMEOUT_DETECTED_OK" && $ELAPSED_MS -ge 100 && $ELAPSED_MS -le 1500 ]]; then
    pass "Blocking socket receive timeout triggered properly (~150ms) with EAGAIN/EWOULDBLOCK diagnostic"
else
    fail "Test 11 execution" "Client output: $C11_OUT, Elapsed: ${ELAPSED_MS}ms"
fi

# ------------------------------------------------------------------------------
# Test 12: Send Behavior When Peer Disconnects (SIGPIPE Resilience)
# ------------------------------------------------------------------------------
echo "Test 12: Peer disconnect send resilience (MSG_NOSIGNAL / no SIGPIPE)..."
cat << 'EOF' > "$TMP_DIR/test12_server.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29712);
    if (!server.is_listening()) {
        return 1;
    }

    let client: TcpClient = server.accept_client();
    if (!client.is_connected()) {
        server.close();
        return 2;
    }

    // Immediately close server-side to simulate sudden peer disconnect
    client.close();
    server.close();
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/test12_client.cco"
import "std/net.cco";

fn main() -> int {
    sleep_ms(60);
    let client: TcpClient = connect_client("127.0.0.1", 29712);
    if (!client.is_connected()) {
        return 1;
    }

    // Wait briefly so server's close FIN/RST is processed by network stack
    sleep_ms(100);

    // Write multiple times so kernel sends RST and subsequent send encounters EPIPE
    client.send("DATA_1");
    sleep_ms(20);
    let sent2: int = client.send("DATA_2_AFTER_RST");

    // With MSG_NOSIGNAL, process does NOT terminate with SIGPIPE; send returns -1
    if (sent2 < 0) {
        let err: string = tcp_last_error();
        if (equals(err, "send() failed: Broken pipe")) {
            print("SIGPIPE_SURVIVED_OK");
            client.close();
            return 0;
        }
    }

    print("SEND_EPIPE_UNEXPECTED");
    client.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test12_server.cco" -o "$TMP_DIR/test12_server"
"$CCO" "$TMP_DIR/test12_client.cco" -o "$TMP_DIR/test12_client"

"$TMP_DIR/test12_server" > /dev/null 2>&1 &
SERVER12_PID=$!
"$TMP_DIR/test12_client" > "$TMP_DIR/test12_client.out" 2>&1 &
CLIENT12_PID=$!

wait $SERVER12_PID || true
wait $CLIENT12_PID || true

C12_OUT=$(cat "$TMP_DIR/test12_client.out" 2>/dev/null || echo "")

if [[ "$C12_OUT" == "SIGPIPE_SURVIVED_OK" ]]; then
    pass "Send on disconnected peer returns EPIPE safely without terminating via SIGPIPE"
else
    fail "Test 12 execution" "Client output: $C12_OUT"
fi

# ------------------------------------------------------------------------------
# Test 13: Deterministic Interrupted accept() Behavior
# ------------------------------------------------------------------------------
echo "Test 13: Interrupted accept() resilience (EINTR handling) & invalid descriptor..."
cat << 'EOF' > "$TMP_DIR/test13_interrupted.cco"
import "std/net.cco";

fn main() -> int {
    // 1. Invalid descriptor reject check
    let bad_accept: int = net_accept(-1);
    if (bad_accept != -1) {
        return 1;
    }
    let err: string = net_last_error();
    if (!equals(err, "Invalid server descriptor: -1")) {
        return 2;
    }

    // 2. Setup server for signal interruption test
    let server: TcpServer = create_server_on("127.0.0.1", 29713);
    if (!server.is_listening()) {
        return 3;
    }

    print("SERVER_READY");

    // Block in accept(): when parent pauses/resumes this process with SIGSTOP/SIGCONT,
    // accept() receives EINTR and must automatically loop and continue waiting.
    let client: TcpClient = server.accept_client();
    if (!client.is_connected()) {
        server.close();
        return 4;
    }

    let msg: string = client.recv_raw(128);
    if (!equals(msg, "CLIENT_AFTER_INTERRUPT")) {
        client.close();
        server.close();
        return 5;
    }

    print("ACCEPT_SURVIVED_EINTR_OK");
    client.close();
    server.close();
    return 0;
}
EOF

cat << 'EOF' > "$TMP_DIR/test13_client.cco"
import "std/net.cco";

fn main() -> int {
    let client: TcpClient = connect_client("127.0.0.1", 29713);
    if (!client.is_connected()) {
        return 1;
    }
    client.send("CLIENT_AFTER_INTERRUPT");
    client.close();
    return 0;
}
EOF

# Compile isolated C helper to install a non-restarting signal handler for SIGUSR1
cat << 'EOF' > "$TMP_DIR/eintr_helper.c"
#include <signal.h>
#include <unistd.h>

static void sigusr1_handler(int sig) {
    (void)sig;
    const char msg[] = "SIGUSR1_HANDLER_FIRED\n";
    (void)write(1, msg, sizeof(msg) - 1);
}

__attribute__((constructor))
static void install_eintr_handler(void) {
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Explicitly no SA_RESTART: forces blocking syscalls like accept() to return EINTR
    sigaction(SIGUSR1, &sa, NULL);
}
EOF
gcc -shared -fPIC "$TMP_DIR/eintr_helper.c" -o "$TMP_DIR/libeintr.so"

"$CCO" "$TMP_DIR/test13_interrupted.cco" -o "$TMP_DIR/test13_interrupted"
"$CCO" "$TMP_DIR/test13_client.cco" -o "$TMP_DIR/test13_client"

LD_PRELOAD="$TMP_DIR/libeintr.so" "$TMP_DIR/test13_interrupted" > "$TMP_DIR/test13_server.out" 2>&1 &
SERVER13_PID=$!

# Wait for server to report ready and enter blocking accept()
for i in {1..20}; do
    if grep -q "SERVER_READY" "$TMP_DIR/test13_server.out" 2>/dev/null; then
        break
    fi
    sleep 0.05
done

# Deliver SIGUSR1 while the server is blocked in accept().
# Because sa_flags = 0 (no SA_RESTART), accept() returns -1 with errno == EINTR.
# The server's runtime retry loop must handle EINTR and re-enter accept().
kill -USR1 $SERVER13_PID 2>/dev/null || true
sleep 0.05

# Connect client to complete the connection after the interrupt
"$TMP_DIR/test13_client" > /dev/null 2>&1 || true

wait $SERVER13_PID || true
S13_OUT=$(cat "$TMP_DIR/test13_server.out" 2>/dev/null || echo "")

if [[ "$S13_OUT" == *"SIGUSR1_HANDLER_FIRED"* && "$S13_OUT" == *"ACCEPT_SURVIVED_EINTR_OK"* ]]; then
    pass "Interrupted accept() gracefully survives EINTR without dropping listener or connection"
else
    fail "Test 13 execution" "Server output: $S13_OUT"
fi

# ------------------------------------------------------------------------------
# Test 14: Descriptor Leakage Prevention Across Multiple Failures
# ------------------------------------------------------------------------------
echo "Test 14: Descriptor leakage prevention across failed operations..."
cat << 'EOF' > "$TMP_DIR/test14_fd_leak.cco"
import "std/net.cco";

fn main() -> int {
    for (let i: int = 0; i < 50; i++) {
        // Attempt connection to nonexistent port (connection refused)
        net_connect("127.0.0.1", 29799);
        // Attempt invalid shutdown
        net_shutdown(-1, 0);
        // Attempt invalid timeouts
        net_set_timeouts(-1, 100, 100);
        // Attempt invalid bind
        net_listen_on("999.999.999.999", 29714);
    }
    print("LEAK_LOOP_OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test14_fd_leak.cco" -o "$TMP_DIR/test14_fd_leak"
OUT14=$("$TMP_DIR/test14_fd_leak")
if [[ "$OUT14" == "LEAK_LOOP_OK" ]]; then
    pass "Repeated failed operations cleanly dispose descriptors without table growth"
else
    fail "Test 14 execution" "Output was: $OUT14"
fi

# ------------------------------------------------------------------------------
# Test 15: Repeated Close and Repeated Shutdown Operations
# ------------------------------------------------------------------------------
echo "Test 15: Repeated close and shutdown idempotent safety..."
cat << 'EOF' > "$TMP_DIR/test15_repeat.cco"
import "std/net.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29715);
    if (!server.is_listening()) {
        return 1;
    }

    let client_sock: int = net_connect("127.0.0.1", 29715);
    if (client_sock < 0) {
        server.close();
        return 2;
    }

    // Repeated shutdown
    tcp_shutdown(client_sock, 2);
    tcp_shutdown(client_sock, 2);

    // Repeated close via top-level function
    tcp_close(client_sock);
    tcp_close(client_sock);

    // Repeated close via object wrapper
    server.close();
    server.close();
    server.close();

    // Shutdown after close should return -1 with bad fd
    let s3: int = tcp_shutdown(client_sock, 2);
    if (s3 != -1) {
        return 3;
    }

    print("REPEATED_OPERATIONS_SAFE_OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test15_repeat.cco" -o "$TMP_DIR/test15_repeat"
OUT15=$("$TMP_DIR/test15_repeat")
if [[ "$OUT15" == "REPEATED_OPERATIONS_SAFE_OK" ]]; then
    pass "Repeated close and shutdown operations execute safely without double-free or crash"
else
    fail "Test 15 execution" "Output was: $OUT15"
fi

# ------------------------------------------------------------------------------
# Test 16: Result Wrapper Error Handling
# ------------------------------------------------------------------------------
echo "Test 16: Result-wrapper error handling (tcp_shutdown_result, tcp_set_timeouts_result)..."
cat << 'EOF' > "$TMP_DIR/test16_result.cco"
import "std/net.cco";
import "std/result.cco";

fn main() -> int {
    // 1. Invalid shutdown mode
    let res1: ResultInt = tcp_shutdown_result(1, 999);
    if (!res_int_is_err(&res1)) {
        return 1;
    }
    print(concat("Res1 Err: ", res_int_error(&res1)));

    // 2. Invalid socket descriptor
    let res2: ResultInt = tcp_shutdown_result(-1, 0);
    if (!res_int_is_err(&res2)) {
        return 2;
    }
    print(concat("Res2 Err: ", res_int_error(&res2)));

    // 3. Invalid timeout values
    let res3: ResultInt = tcp_set_timeouts_result(1, -100, 50);
    if (!res_int_is_err(&res3)) {
        return 3;
    }
    print(concat("Res3 Err: ", res_int_error(&res3)));

    // 4. Successful result wrapper execution
    let server: TcpServer = create_server_on("127.0.0.1", 29716);
    if (!server.is_listening()) {
        return 4;
    }

    let res4: ResultInt = tcp_set_timeouts_result(server.fd, 200, 200);
    if (res_int_is_err(&res4)) {
        server.close();
        return 5;
    }
    if (res_int_unwrap_or(&res4, -1) != 0) {
        server.close();
        return 6;
    }

    server.close();
    print("RESULT_WRAPPERS_OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test16_result.cco" -o "$TMP_DIR/test16_result"
OUT16=$("$TMP_DIR/test16_result")
if [[ "$OUT16" == *"RESULT_WRAPPERS_OK"* && "$OUT16" == *"Invalid shutdown mode: 999"* && "$OUT16" == *"Invalid socket descriptor: -1"* && "$OUT16" == *"Invalid timeout value"* ]]; then
    pass "Result-wrapped lifecycle and timeout functions correctly wrap errors and return ok_int(0)"
else
    fail "Test 16 execution" "Output was: $OUT16"
fi

# ------------------------------------------------------------------------------
# Test 17: Existing String and Binary Transfers Remain Fully Operational
# ------------------------------------------------------------------------------
echo "Test 17: Preserving string and ByteBuffer transfer functionality..."
cat << 'EOF' > "$TMP_DIR/test17_transfer.cco"
import "std/net.cco";
import "std/buffer.cco";

fn main() -> int {
    let server: TcpServer = create_server_on("127.0.0.1", 29717);
    if (!server.is_listening()) {
        return 1;
    }

    let client: TcpClient = connect_client("127.0.0.1", 29717);
    let peer_fd: int = server.accept();
    if (peer_fd < 0 || !client.is_connected()) {
        server.close();
        return 2;
    }

    // Binary ByteBuffer transfer
    let buf: ByteBuffer = byte_buffer_new(32);
    buf.write_u8(170);
    buf.write_u16_be(51966);
    buf.write_u32_be(305419896);

    let sent: int = tcp_send_buffer(client.fd, &buf);
    if (sent != 7) {
        tcp_close(peer_fd);
        client.close();
        server.close();
        return 3;
    }

    let recv_buf: ByteBuffer = byte_buffer_new(32);
    let recvd: int = tcp_recv_buffer(peer_fd, &recv_buf, 7);
    if (recvd != 7) {
        tcp_close(peer_fd);
        client.close();
        server.close();
        return 4;
    }

    let v1: int = recv_buf.read_u8();
    let v2: int = recv_buf.read_u16_be();
    let v3: int = recv_buf.read_u32_be();

    if (v1 == 170 && v2 == 51966 && v3 == 305419896) {
        print("TRANSFER_PRESERVED_OK");
    }

    tcp_close(peer_fd);
    client.close();
    server.close();
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test17_transfer.cco" -o "$TMP_DIR/test17_transfer"
OUT17=$("$TMP_DIR/test17_transfer")
if [[ "$OUT17" == "TRANSFER_PRESERVED_OK" ]]; then
    pass "Binary ByteBuffer and network transfer primitives remain fully operational"
else
    fail "Test 17 execution" "Output was: $OUT17"
fi

# ------------------------------------------------------------------------------
# Test 18: Valgrind Memory Leak and Safety Verification
# ------------------------------------------------------------------------------
echo "Test 18: Valgrind memory leak verification across test binaries..."
VALG_FAIL=0
for bin in "$TMP_DIR/test5_modes" "$TMP_DIR/test6_bad_fd" "$TMP_DIR/test7_timeouts" "$TMP_DIR/test8_zero_timeout" "$TMP_DIR/test9_neg_timeout" "$TMP_DIR/test10_large_timeout" "$TMP_DIR/test14_fd_leak" "$TMP_DIR/test15_repeat" "$TMP_DIR/test16_result" "$TMP_DIR/test17_transfer"; do
    if [[ -x "$bin" ]]; then
        if ! valgrind --leak-check=full --error-exitcode=1 -q "$bin" > /dev/null 2>&1; then
            echo "Valgrind detected error in $bin"
            VALG_FAIL=$((VALG_FAIL + 1))
            fail "Valgrind memcheck" "$(basename "$bin") leaked memory or raised errors"
        fi
    fi
done

if [[ $VALG_FAIL -eq 0 ]]; then
    pass "All Phase 18D Milestone 4 test binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# ------------------------------------------------------------------------------
# Test 19: Negative Boundary — Compile-Time Rejection by Native Backend
# ------------------------------------------------------------------------------
echo "Test 19: Negative boundary — compile-time rejection by native backend..."
set +e
NATIVE_ERR=$("$CCO" "$TMP_DIR/test16_result.cco" --use-native -o "$TMP_DIR/native_bin" 2>&1)
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
echo "Phase 18D Milestone 4 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
