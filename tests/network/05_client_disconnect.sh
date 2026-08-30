#!/usr/bin/env bash
set -e

PORT=8095
CONNS=4
mkdir -p build/network

valgrind --track-fds=yes --leak-check=full --error-exitcode=1 ./build/network/test_server $PORT $CONNS > build/network/05_server.log 2> build/network/05_valgrind.log &
SERVER_PID=$!

python3 - << PYEOF
import socket, time, sys

def get_connected_socket():
    for _ in range(40):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(('127.0.0.1', $PORT))
            return s
        except Exception:
            time.sleep(0.1)
    print("FAILED to connect to server")
    sys.exit(1)

# 1. Connect and immediately close (0 bytes sent)
s1 = get_connected_socket()
s1.close()
time.sleep(0.1)

# 2. Connect, send partial fragment, and abruptly close
s2 = get_connected_socket()
s2.sendall(b'GET /')
s2.close()
time.sleep(0.1)

# 3. Connect, send request, and close socket before reading response (triggers MSG_NOSIGNAL in send)
s3 = get_connected_socket()
s3.sendall(b'GET /health HTTP/1.1\r\n\r\n')
s3.close()
time.sleep(0.1)

# 4. Standard clean request
s4 = get_connected_socket()
s4.sendall(b'GET /health HTTP/1.1\r\n\r\n')
r4 = s4.recv(2048).decode(errors='ignore')
s4.close()
assert '200 OK' in r4 and 'HEALTHY' in r4, f"Final req failed: {r4}"

PYEOF

wait $SERVER_PID
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "FAILED: Server exited with error code $EXIT_CODE"
    cat build/network/05_valgrind.log
    exit 1
fi

if grep -q "ERROR SUMMARY: 0 errors" build/network/05_valgrind.log && \
   grep -q "All heap blocks were freed -- no leaks are possible" build/network/05_valgrind.log; then
    echo "PASSED (Client Disconnects Handled Cleanly + 0 Leaks + 0 FD Leaks)"
else
    echo "FAILED: Valgrind detected leaks or errors:"
    cat build/network/05_valgrind.log
    exit 1
fi
