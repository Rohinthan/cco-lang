#!/usr/bin/env bash
set -e

PORT=8091
CONNS=5
mkdir -p build/network

valgrind --track-fds=yes --leak-check=full --error-exitcode=1 ./build/network/test_server $PORT $CONNS > build/network/01_server.log 2> build/network/01_valgrind.log &
SERVER_PID=$!

sleep 0.3

# Request 1: GET /health
RESP1=$(curl --retry 5 --retry-connrefused --retry-delay 1 -s http://127.0.0.1:$PORT/health)
if [[ "$RESP1" != *'"status": "HEALTHY"'* ]]; then
    echo "FAILED: /health returned unexpected: $RESP1"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# Request 2: GET /api/greet
RESP2=$(curl -s http://127.0.0.1:$PORT/api/greet)
if [[ "$RESP2" != *"Hello from Cco Live TCP Socket Server!"* ]]; then
    echo "FAILED: /api/greet returned unexpected: $RESP2"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# Request 3: GET /api/stats
RESP3=$(curl -s http://127.0.0.1:$PORT/api/stats)
if [[ "$RESP3" != *'"server": "ONLINE"'* ]]; then
    echo "FAILED: /api/stats returned unexpected: $RESP3"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# Request 4: POST /api/echo
RESP4=$(curl -s -X POST http://127.0.0.1:$PORT/api/echo -d "ping=pong")
if [[ "$RESP4" != *'"message": "Echo received successfully via live TCP stream"'* ]]; then
    echo "FAILED: /api/echo returned unexpected: $RESP4"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# Request 5: GET /unknown_route
RESP5=$(curl -s http://127.0.0.1:$PORT/unknown_route)
if [[ "$RESP5" != *'"error": "Route not found on live socket server"'* ]]; then
    echo "FAILED: /unknown_route returned unexpected: $RESP5"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# Wait for server process to cleanly exit
wait $SERVER_PID
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "FAILED: Server exited with error code $EXIT_CODE"
    cat build/network/01_valgrind.log
    exit 1
fi

# Check Valgrind for memory leaks and file descriptor leaks
if grep -q "ERROR SUMMARY: 0 errors" build/network/01_valgrind.log && \
   grep -q "All heap blocks were freed -- no leaks are possible" build/network/01_valgrind.log && \
   ! grep -q "<inherited from parent>" build/network/01_valgrind.log && \
   ! grep -q "Open file descriptor" build/network/01_valgrind.log; then
    echo "PASSED (0 Leaks + 0 FD Leaks)"
elif grep -q "ERROR SUMMARY: 0 errors" build/network/01_valgrind.log && \
     grep -q "All heap blocks were freed -- no leaks are possible" build/network/01_valgrind.log && \
     grep -q "FILE DESCRIPTORS: 4 open (3 std) at exit" build/network/01_valgrind.log; then
    # Valgrind log file itself is fd 4
    echo "PASSED (0 Leaks + 0 FD Leaks)"
else
    echo "FAILED: Valgrind detected leaks or errors:"
    cat build/network/01_valgrind.log
    exit 1
fi
