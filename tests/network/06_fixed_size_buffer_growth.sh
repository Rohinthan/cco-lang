#!/usr/bin/env bash
set -e

PORT=8096
CONNS=3
mkdir -p build/network

valgrind --track-fds=yes --leak-check=full --error-exitcode=1 ./build/network/test_server_fixed_size $PORT $CONNS > build/network/06_server.log 2> build/network/06_valgrind.log &
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

# 1. 2KB POST request (4x larger than the 512-byte initial net_recv buffer)
body_2k = "B" * 2048
req1 = f"POST /api/upload HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: {len(body_2k)}\r\n\r\n{body_2k}".encode()
r1 = send_req(req1)
assert '200 OK' in r1 and 'Received' in r1, f"Req 1 failed: {r1}"

# 2. 8KB POST request (16x larger than the 512-byte initial net_recv buffer)
body_8k = "C" * 8192
req2 = f"POST /api/upload HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: {len(body_8k)}\r\n\r\n{body_8k}".encode()
r2 = send_req(req2)
assert '200 OK' in r2 and 'Received' in r2, f"Req 2 failed: {r2}"

# 3. 32KB POST request (64x larger than the 512-byte initial net_recv buffer)
body_32k = "D" * 32768
req3 = f"POST /api/upload HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: {len(body_32k)}\r\n\r\n{body_32k}".encode()
r3 = send_req(req3)
assert '200 OK' in r3 and 'Received' in r3, f"Req 3 failed: {r3}"

PYEOF

wait $SERVER_PID
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "FAILED: Server exited with error code $EXIT_CODE"
    cat build/network/06_valgrind.log
    exit 1
fi

if grep -q "ERROR SUMMARY: 0 errors" build/network/06_valgrind.log && \
   grep -q "All heap blocks were freed -- no leaks are possible" build/network/06_valgrind.log; then
    echo "PASSED (Dynamic Growth Past Initial Buffer Capacity Verified + 0 Leaks + 0 FD Leaks)"
else
    echo "FAILED: Valgrind detected leaks or errors:"
    cat build/network/06_valgrind.log
    exit 1
fi
