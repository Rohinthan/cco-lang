#!/usr/bin/env bash
set -e

mkdir -p build

echo "=================================================="
echo "  CMM (C--) INTEGRATION & VALGRIND TEST SUITE    "
echo "=================================================="

PASSED=0
FAILED=0

for prog in tests/programs/*.cmm; do
    base=$(basename "$prog" .cmm)
    c_out="build/${base}.c"
    bin_out="build/${base}"
    act_out="build/${base}_actual.txt"
    exp_out="tests/expected_output/${base}.txt"

    echo -n "Testing ${base}... "

    # 1. Transpile CMM -> C
    ./cmm "$prog" -o "$c_out"

    # 2. Compile C -> Executable
    gcc -Wall -Wextra -Werror -std=c11 "$c_out" -o "$bin_out"

    # 3. Execute and capture stdout
    "$bin_out" > "$act_out"

    # 4. Diff output against expected
    if ! diff -u "$act_out" "$exp_out" > /dev/null; then
        echo "FAILED (Output mismatch)"
        diff -u "$act_out" "$exp_out"
        FAILED=$((FAILED + 1))
        continue
    fi

    # 5. Valgrind leak check
    if ! valgrind --leak-check=full --error-exitcode=1 "$bin_out" > /dev/null 2>&1; then
        echo "FAILED (Valgrind memory leak / error)"
        valgrind --leak-check=full "$bin_out"
        FAILED=$((FAILED + 1))
        continue
    fi

    echo "PASSED (Diff Clean + 0 Leaks)"
    PASSED=$((PASSED + 1))
done

echo "--------------------------------------------------"
echo "Summary: ${PASSED} Passed, ${FAILED} Failed"
echo "=================================================="

if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
