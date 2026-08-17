#!/usr/bin/env bash
set -e

mkdir -p build

echo "=================================================="
echo "  Cco (C--) INTEGRATION & VALGRIND TEST SUITE    "
echo "=================================================="

PASSED=0
FAILED=0

for item in tests/programs/*; do
    if [ -f "$item" ]; then
        if [[ "$item" != *.cco ]]; then continue; fi
        base=$(basename "$item" .cco)
        entry="$item"
    elif [ -d "$item" ]; then
        base=$(basename "$item")
        if [ -f "$item/main.cco" ]; then
            entry="$item/main.cco"
        elif [ -f "$item/a.cco" ]; then
            entry="$item/a.cco"
        else
            continue
        fi
    else
        continue
    fi

    c_out="build/${base}.c"
    bin_out="build/${base}"
    act_out="build/${base}_actual.txt"
    exp_out="tests/expected_output/${base}.txt"

    echo -n "Testing ${base}... "

    if [[ "$base" == *"_ERROR"* ]]; then
        err_out="build/${base}_err.txt"
        exp_err="tests/expected_output/${base}_stderr.txt"
        set +e
        ./cco "$entry" -o "$c_out" 2> "$err_out"
        code=$?
        set -e
        if [ "$code" -eq 0 ]; then
            echo "FAILED (Expected compilation failure with exit code 1, got 0)"
            FAILED=$((FAILED + 1))
            continue
        fi
        if ! diff -u "$err_out" "$exp_err" > /dev/null; then
            echo "FAILED (Stderr error message mismatch)"
            diff -u "$err_out" "$exp_err"
            FAILED=$((FAILED + 1))
            continue
        fi
        echo "PASSED (Compilation Failed as Expected)"
        PASSED=$((PASSED + 1))
        continue
    fi

    # 1. Transpile Cco -> C
    ./cco "$entry" -o "$c_out"

    # 2. Compile C -> Executable
    gcc -Wall -Wextra -Werror -std=c11 "$c_out" -o "$bin_out" -lm

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
