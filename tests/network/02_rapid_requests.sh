#!/usr/bin/env bash
set -e

PORT=8092
CONNS=25
mkdir -p build/network

valgrind --track-fds=yes --leak-check=full --error-exitcode=1 ./build/network/test_server $PORT $CONNS > build/network/02_server.log 2> build/network/02_valgrind.log &
SERVER_PID=$!

sleep 0.3

# Fire 25 sequential rapid requests
for i in $(seq 1 $CONNS); do
    RESP=$(curl --retry 5 --retry-connrefused --retry-delay 1 -s http://127.0.0.1:$PORT/health)
    if [[ "$RESP" != *'"status": "HEALTHY"'* ]]; then
        echo "FAILED on request #$i: got $RESP"
        kill $SERVER_PID 2>/dev/null || true
        exit 1
    fi
done

wait $SERVER_PID
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo "FAILED: Server exited with error code $EXIT_CODE"
    cat build/network/02_valgrind.log
    exit 1
fi

if grep -q "ERROR SUMMARY: 0 errors" build/network/02_valgrind.log && \
   grep -q "All heap blocks were freed -- no leaks are possible" build/network/02_valgrind.log; then
    echo "PASSED (25/25 Requests OK + 0 Leaks + 0 FD Leaks)"
else
    echo "FAILED: Valgrind detected leaks or errors:"
    cat build/network/02_valgrind.log
    exit 1
fi
