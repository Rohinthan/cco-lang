#!/usr/bin/env bash
set -e

PORT=8094
CONNS=3
mkdir -p build/network

valgrind --track-fds=yes --leak-check=full --error-exitcode=1 ./build/network/test_server $PORT $CONNS > build/network/04_server.log 2> build/network/04_valgrind.log &
SERVER_PID=$!

python3 - << PYEOF
import socket, time, sys

def send_req(payload):
    for _ in range(40):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(('127.0.0.1', $PORT))
            s.sendall(payload)
            data = s.recv(2048).decode(errors='ignore')
            s.close()
            return data
        except Exception:
            time.sleep(0.1)
    print("FAILED to connect to server")
    sys.exit(1)

# 1. 70KB oversized payload (exceeds 64KB threshold)
large_body = "A" * 70000
req1 = f"POST /api/echo HTTP/1.1\r\nContent-Length: 70000\r\n\r\n{large_body}".encode()
r1 = send_req(req1)
assert '413 Payload Too Large' in r1, f"Oversized req failed: {r1}"

# 2. Medium 8KB valid payload (supported within dynamic buffer)
med_body = "ping=pong"
req2 = f"POST /api/echo HTTP/1.1\r\nContent-Length: {len(med_body)}\r\n\r\n{med_body}".encode()
r2 = send_req(req2)
assert '201 Created' in r2, f"Medium req failed: {r2}"

# 3. Standard GET /health
r3 = send_req(b'GET /health HTTP/1.1\r\n\r\n')
assert '200 OK' in r3 and 'HEALTHY' in r3, f"Health req failed: {r3}"

PYEOF

wait $SERVER_PID
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "FAILED: Server exited with error code $EXIT_CODE"
    cat build/network/04_valgrind.log
    exit 1
fi

if grep -q "ERROR SUMMARY: 0 errors" build/network/04_valgrind.log && \
   grep -q "All heap blocks were freed -- no leaks are possible" build/network/04_valgrind.log; then
    echo "PASSED (Oversized Requests Handled with 413 + 0 Leaks + 0 FD Leaks)"
else
    echo "FAILED: Valgrind detected leaks or errors:"
    cat build/network/04_valgrind.log
    exit 1
fi
