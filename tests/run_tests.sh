#!/usr/bin/env bash
set -e

mkdir -p build

echo "=================================================="
echo "  Cco (C--) INTEGRATION & VALGRIND TEST SUITE    "
echo "=================================================="
if command -v clang >/dev/null 2>&1; then
    echo "  [Compiler] GCC: yes (-pedantic-errors), Clang: yes (-pedantic-errors)"
else
    echo "  [Compiler] GCC: yes (-pedantic-errors), Clang: not found (skipping)"
fi
if command -v tcc >/dev/null 2>&1; then
    echo "  [Compiler] TCC: yes"
fi
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

    # Check for optional .args sidecar file
    local_args=()
    args_file="tests/programs/${base}.args"
    if [ -f "$args_file" ]; then
        read -r -a local_args < "$args_file"
    fi

    # Check for optional .stdin sidecar file
    stdin_file="tests/programs/${base}.stdin"
    if [ ! -f "$stdin_file" ]; then
        stdin_file="/dev/null"
    fi

    if [[ "$base" == *"_RUNTIME_ERROR"* ]]; then
        err_out="build/${base}_err.txt"
        exp_err="tests/expected_output/${base}_stderr.txt"
        ./cco "$entry" -o "$c_out"
        gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 "$c_out" -o "$bin_out" -lm
        set +e
        "$bin_out" "${local_args[@]}" < "$stdin_file" 2> "$err_out" > /dev/null
        code=$?
        set -e
        if [ "$code" -eq 0 ]; then
            echo "FAILED (Expected runtime failure with non-zero exit code, got 0)"
            FAILED=$((FAILED + 1))
            continue
        fi
        if ! diff -u "$err_out" "$exp_err" > /dev/null; then
            echo "FAILED (Runtime stderr mismatch)"
            diff -u "$err_out" "$exp_err"
            FAILED=$((FAILED + 1))
            continue
        fi
        echo "PASSED (Runtime Failed as Expected)"
        PASSED=$((PASSED + 1))
        continue
    fi

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

    # Specific prelude emission assertions for v8 tests
    if [ "$base" = "38_prelude_minimal" ]; then
        if grep -q -E "__cco_(concat|alloc_arr|read_file|write_file|bounds_check|arr_len|substring|abs|min|max|map|free_arr)" "$c_out"; then
            echo "FAILED (38_prelude_minimal contains unexpected __cco_ helper functions in prelude)"
            FAILED=$((FAILED + 1))
            continue
        fi
    elif [ "$base" = "39_prelude_partial" ]; then
        if ! grep -q "__cco_concat" "$c_out"; then
            echo "FAILED (39_prelude_partial missing expected __cco_concat)"
            FAILED=$((FAILED + 1))
            continue
        fi
        if grep -q "__cco_read_file" "$c_out" || grep -q "__cco_alloc_arr" "$c_out"; then
            echo "FAILED (39_prelude_partial contains unused __cco_ helpers)"
            FAILED=$((FAILED + 1))
            continue
        fi
    elif [ "$base" = "40_prelude_transitive" ]; then
        if ! grep -q "__cco_bounds_check" "$c_out" || ! grep -q "__cco_arr_len" "$c_out"; then
            echo "FAILED (40_prelude_transitive missing expected __cco_bounds_check or __cco_arr_len)"
            FAILED=$((FAILED + 1))
            continue
        fi
    fi

    # 2. Compile C -> Executable (gcc with strict C11 -pedantic-errors)
    gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 "$c_out" -o "$bin_out" -lm

    # Optional Clang compilation pass
    if command -v clang >/dev/null 2>&1; then
        clang -Wall -Wextra -Werror -pedantic-errors -std=c11 "$c_out" -o "${bin_out}_clang" -lm
    fi

    # Optional TCC compilation pass
    if command -v tcc >/dev/null 2>&1; then
        tcc -std=c11 "$c_out" -o "${bin_out}_tcc" -lm
    fi

    # 3. Execute and capture stdout
    "$bin_out" "${local_args[@]}" < "$stdin_file" > "$act_out"

    # 4. Diff output against expected
    if ! diff -u "$act_out" "$exp_out" > /dev/null; then
        echo "FAILED (Output mismatch)"
        diff -u "$act_out" "$exp_out"
        FAILED=$((FAILED + 1))
        continue
    fi

    # 5. Valgrind leak check
    if ! valgrind --leak-check=full --error-exitcode=1 "$bin_out" "${local_args[@]}" < "$stdin_file" > /dev/null 2>&1; then
        echo "FAILED (Valgrind memory leak / error)"
        valgrind --leak-check=full "$bin_out" "${local_args[@]}" < "$stdin_file"
        FAILED=$((FAILED + 1))
        continue
    fi

    echo "PASSED (Diff Clean + 0 Leaks)"
    PASSED=$((PASSED + 1))
done

echo "=================================================="
echo "  Cco NATIVE POSIX NETWORKING & FD LEAK SUITE     "
echo "=================================================="
mkdir -p build/network
./cco tests/network/test_server.cco -o build/network/test_server.c
gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 build/network/test_server.c -o build/network/test_server -lm

NET_PASSED=0
NET_FAILED=0
for net_test in tests/network/*.sh; do
    tname=$(basename "$net_test" .sh)
    echo -n "Running ${tname}... "
    if bash "$net_test" > /dev/null 2>&1; then
        echo "PASSED (0 Leaks + 0 FD Leaks)"
        NET_PASSED=$((NET_PASSED + 1))
    else
        echo "FAILED"
        bash "$net_test"
        NET_FAILED=$((NET_FAILED + 1))
    fi
done

echo "--------------------------------------------------"
echo "Network Suite: ${NET_PASSED} Passed, ${NET_FAILED} Failed"
echo "=================================================="
echo "Total Suite Summary: $((PASSED + NET_PASSED)) Passed, $((FAILED + NET_FAILED)) Failed"
echo "=================================================="

if [ "$FAILED" -ne 0 ] || [ "$NET_FAILED" -ne 0 ]; then
    exit 1
fi
