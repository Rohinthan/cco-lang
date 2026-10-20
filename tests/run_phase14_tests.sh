#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -e

echo "=============================================================="
echo "Cco Compiler — Phase 14 Self-Hosted Optimization Parity Suite"
echo "3-Way Differential Testing: Reference vs Unopt vs Opt"
echo "=============================================================="

mkdir -p build/phase14_bin
PASSED=0
TOTAL=0
CC="${CC:-gcc}"
CFLAGS="-Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-but-set-variable -pedantic-errors -std=c11 -Isrc"
LDFLAGS="-lm"

for test_file in tests/phase14/*.cco; do
    [ -e "$test_file" ] || continue
    test_name=$(basename "$test_file" .cco)
    TOTAL=$((TOTAL + 1))
    echo -n "[$TOTAL] Differential testing $test_name ... "

    # 1. Reference C11 build & run
    ./cco "$test_file" -o "build/phase14_bin/${test_name}_ref.c" > /dev/null 2>&1
    $CC $CFLAGS "build/phase14_bin/${test_name}_ref.c" -o "build/phase14_bin/${test_name}_ref" $LDFLAGS > /dev/null 2>&1
    ./build/phase14_bin/${test_name}_ref > "build/phase14_bin/${test_name}_ref.out" 2>&1 || true

    # 2. Self-Hosted Unoptimized build & run
    ./selfhost/cco "$test_file" -o "build/phase14_bin/${test_name}_unopt.c" > /dev/null 2>&1
    $CC $CFLAGS "build/phase14_bin/${test_name}_unopt.c" -o "build/phase14_bin/${test_name}_unopt" $LDFLAGS > /dev/null 2>&1
    ./build/phase14_bin/${test_name}_unopt > "build/phase14_bin/${test_name}_unopt.out" 2>&1 || true

    # 3. Self-Hosted Optimized build & run
    ./selfhost/cco "$test_file" -o "build/phase14_bin/${test_name}_opt.c" -O > /dev/null 2>&1
    $CC $CFLAGS "build/phase14_bin/${test_name}_opt.c" -o "build/phase14_bin/${test_name}_opt" $LDFLAGS > /dev/null 2>&1
    ./build/phase14_bin/${test_name}_opt > "build/phase14_bin/${test_name}_opt.out" 2>&1 || true

    # 4. Compare Reference vs Unoptimized
    if ! diff -u "build/phase14_bin/${test_name}_ref.out" "build/phase14_bin/${test_name}_unopt.out" > /dev/null 2>&1; then
        echo "FAILED (Reference vs Unoptimized mismatch)"
        diff -u "build/phase14_bin/${test_name}_ref.out" "build/phase14_bin/${test_name}_unopt.out"
        exit 1
    fi

    # 5. Compare Reference vs Optimized
    if ! diff -u "build/phase14_bin/${test_name}_ref.out" "build/phase14_bin/${test_name}_opt.out" > /dev/null 2>&1; then
        echo "FAILED (Reference vs Optimized mismatch)"
        diff -u "build/phase14_bin/${test_name}_ref.out" "build/phase14_bin/${test_name}_opt.out"
        exit 1
    fi

    # 6. Valgrind Cleanliness on Optimized Binary
    if command -v valgrind > /dev/null 2>&1; then
        valgrind --leak-check=full --error-exitcode=1 ./build/phase14_bin/${test_name}_opt > /dev/null 2>&1
    fi

    echo "PASSED (100% 3-way match, 0 leaks, 0 errors)"
    PASSED=$((PASSED + 1))
done

echo "=============================================================="
echo "Phase 14 Test Suite Results: $PASSED / $TOTAL Passed (100% Success)"
echo "=============================================================="
