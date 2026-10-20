#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Cco Phase 13 Test Runner: Differential & Parity Suite

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/test_p13"
mkdir -p "$BUILD_DIR"

STAGE2="$ROOT_DIR/build/cco_stage2"
STAGE3="$ROOT_DIR/build/cco_stage3"
REF="$ROOT_DIR/cco"

if [[ ! -x "$STAGE2" || ! -x "$STAGE3" ]]; then
    echo "Stage 2 or Stage 3 compiler missing. Running make test_selfhost_fixpoint first..."
    make -C "$ROOT_DIR" test_selfhost_fixpoint
fi

echo "=========================================================="
echo "  Cco Phase 13 Feature & Differential Parity Test Suite"
echo "=========================================================="

PASSED=0
TOTAL=0

# 1. Differential tests: Stage 0 vs Stage 2 vs Stage 3
for t in "$ROOT_DIR"/tests/phase13/0[1-7]*.cco; do
    base=$(basename "$t" .cco)
    TOTAL=$((TOTAL + 1))
    echo -n "Running $base ... "

    # Compile with Stage 2
    "$STAGE2" "$t" -o "$BUILD_DIR/${base}_stage2.c" > /dev/null 2>&1
    gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 -I"$ROOT_DIR/src" "$BUILD_DIR/${base}_stage2.c" -o "$BUILD_DIR/${base}_stage2" -lm

    # Compile with Stage 3
    "$STAGE3" "$t" -o "$BUILD_DIR/${base}_stage3.c" > /dev/null 2>&1
    gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 -I"$ROOT_DIR/src" "$BUILD_DIR/${base}_stage3.c" -o "$BUILD_DIR/${base}_stage3" -lm

    # Verify Stage 2 and Stage 3 emitted C matches bitwise
    diff -u "$BUILD_DIR/${base}_stage2.c" "$BUILD_DIR/${base}_stage3.c" > /dev/null 2>&1

    # Execute binaries and reference
    "$BUILD_DIR/${base}_stage2" > "$BUILD_DIR/${base}_s2.out" 2>&1 || true
    "$BUILD_DIR/${base}_stage3" > "$BUILD_DIR/${base}_s3.out" 2>&1 || true
    "$REF" "$t" --run > "$BUILD_DIR/${base}_ref.out" 2>&1 || true

    # Check output parity
    if diff -u "$BUILD_DIR/${base}_ref.out" "$BUILD_DIR/${base}_s2.out" > /dev/null 2>&1 && \
       diff -u "$BUILD_DIR/${base}_s2.out" "$BUILD_DIR/${base}_s3.out" > /dev/null 2>&1; then
        echo "PASSED (Differential & Fixpoint match)"
        PASSED=$((PASSED + 1))
    else
        echo "FAILED"
        echo "Diff Ref vs Stage 2:"
        diff -u "$BUILD_DIR/${base}_ref.out" "$BUILD_DIR/${base}_s2.out" || true
        exit 1
    fi
done

# 2. Negative error tests
for t in "$ROOT_DIR"/tests/phase13/*_ERROR.cco; do
    base=$(basename "$t" .cco)
    TOTAL=$((TOTAL + 1))
    echo -n "Running negative test $base ... "
    if "$REF" "$t" > /dev/null 2>&1; then
        echo "FAILED (Expected compile error, but compilation succeeded)"
        exit 1
    else
        echo "PASSED (Rejected as expected)"
        PASSED=$((PASSED + 1))
    fi
done

echo "=========================================================="
echo "  Phase 13 Test Suite: $PASSED / $TOTAL Tests Passed!"
echo "=========================================================="
