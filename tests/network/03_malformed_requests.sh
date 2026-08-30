#!/usr/bin/env bash
set -e

PORT=8093
CONNS=5
mkdir -p build/network

valgrind --track-fds=yes --leak-check=full --error-exitcode=1 ./build/network/test_server $PORT $CONNS > build/network/03_server.log 2> build/network/03_valgrind.log &
SERVER_PID=$!

python3 - << PYEOF
import socket, time, sys

def send_req(payload):
    for _ in range(40):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(('127.0.0.1', $PORT))
            s.sendall(payload)
            if b'NO' in payload:
                s.shutdown(socket.SHUT_WR)
            data = s.recv(2048).decode(errors='ignore')
            s.close()
            return data
        except Exception:
            time.sleep(0.1)
    print("FAILED to connect to server after 4s")
    sys.exit(1)

# 1. Garbage random bytes
r1 = send_req(b'GARBAGE_RANDOM_DATA_TEST_12345\r\n\r\n')
assert '400 Bad Request' in r1, f"Req 1 failed: {r1}"

# 2. Missing path
r2 = send_req(b'GET \r\n\r\n')
assert '400 Bad Request' in r2, f"Req 2 failed: {r2}"

# 3. Short fragment < 4 bytes
r3 = send_req(b'NO')
assert '400 Bad Request' in r3, f"Req 3 failed: {r3}"

# 4. Unsupported HTTP method
r4 = send_req(b'DELETE /api/test HTTP/1.1\r\n\r\n')
assert '400 Bad Request' in r4, f"Req 4 failed: {r4}"

# 5. Valid request following malformed traffic
r5 = send_req(b'GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n')
assert '200 OK' in r5 and 'HEALTHY' in r5, f"Req 5 failed: {r5}"

PYEOF

wait $SERVER_PID
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "FAILED: Server exited with error code $EXIT_CODE"
    cat build/network/03_valgrind.log
    exit 1
fi

if grep -q "ERROR SUMMARY: 0 errors" build/network/03_valgrind.log && \
   grep -q "All heap blocks were freed -- no leaks are possible" build/network/03_valgrind.log; then
    echo "PASSED (Malformed Requests Responded Safely + 0 Leaks + 0 FD Leaks)"
else
    echo "FAILED: Valgrind detected leaks or errors:"
    cat build/network/03_valgrind.log
    exit 1
fi
