#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -e

echo "=============================================================="
echo "Cco Compiler — Phase 15 Self-Hosted Native Backend Test Suite"
echo "Differential Testing: Reference vs Self-Hosted Native (ELF64)"
echo "=============================================================="

mkdir -p build/phase15_bin
PASSED=0
TOTAL=0
CC="${CC:-gcc}"
CFLAGS="-Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-but-set-variable -pedantic-errors -std=c11 -Isrc"
LDFLAGS="-lm"

# Ensure compilers and linkers are ready
make cco cco-link > /dev/null 2>&1
./cco selfhost/cco.cco -o selfhost/cco.c > /dev/null 2>&1
$CC $CFLAGS selfhost/cco.c -o selfhost/cco $LDFLAGS > /dev/null 2>&1

echo "--- 1. Differential Correctness Tests (Reference vs Native) ---"

for test_file in tests/phase15/*.cco; do
    [ -e "$test_file" ] || continue
    test_name=$(basename "$test_file" .cco)
    TOTAL=$((TOTAL + 1))
    echo -n "[$TOTAL] Differential testing $test_name ... "

    # 1. Reference C11 build & run
    ./cco "$test_file" -o "build/phase15_bin/${test_name}_ref.c" > /dev/null 2>&1
    $CC $CFLAGS "build/phase15_bin/${test_name}_ref.c" -o "build/phase15_bin/${test_name}_ref" $LDFLAGS > /dev/null 2>&1
    set +e
    ./build/phase15_bin/${test_name}_ref > "build/phase15_bin/${test_name}_ref.out" 2>&1
    REF_EXIT=$?
    set -e

    # 2. Self-Hosted Native build & run
    ./selfhost/cco "$test_file" --selfhost-native -o "build/phase15_bin/${test_name}_native" > /dev/null 2>&1
    set +e
    ./build/phase15_bin/${test_name}_native > "build/phase15_bin/${test_name}_native.out" 2>&1
    NATIVE_EXIT=$?
    set -e

    # 3. Check exit codes match
    if [ "$REF_EXIT" -ne "$NATIVE_EXIT" ]; then
        echo "FAILED (Exit code mismatch: ref=$REF_EXIT, native=$NATIVE_EXIT)"
        exit 1
    fi

    # 4. Check outputs match
    if ! diff -u "build/phase15_bin/${test_name}_ref.out" "build/phase15_bin/${test_name}_native.out" > /dev/null 2>&1; then
        echo "FAILED (Output mismatch)"
        diff -u "build/phase15_bin/${test_name}_ref.out" "build/phase15_bin/${test_name}_native.out"
        exit 1
    fi

    # 5. Valgrind Cleanliness on native compiler generation
    if command -v valgrind > /dev/null 2>&1; then
        valgrind --leak-check=full --error-exitcode=1 ./selfhost/cco "$test_file" --selfhost-native -o "build/phase15_bin/${test_name}_val" > /dev/null 2>&1
        rm -f "build/phase15_bin/${test_name}_val" "build/phase15_bin/${test_name}_val.o"
    fi

    # 6. Valgrind Cleanliness on generated native executable
    if command -v valgrind > /dev/null 2>&1; then
        set +e
        valgrind --leak-check=full --error-exitcode=1 ./build/phase15_bin/${test_name}_native > /dev/null 2>&1
        VG_EXIT=$?
        set -e
        if [ "$VG_EXIT" -ne "$REF_EXIT" ]; then
            echo "FAILED (Valgrind error in native executable: exit $VG_EXIT)"
            exit 1
        fi
    fi

    echo "PASSED (exit code $REF_EXIT, 0 leaks, 0 errors)"
    PASSED=$((PASSED + 1))
done

echo "--- 2. Negative Feature Boundary Tests ---"

for neg_file in tests/phase15/negative/*.cco; do
    [ -e "$neg_file" ] || continue
    neg_name=$(basename "$neg_file" .cco)
    if [ "$neg_name" = "neg_string_lit" ]; then
        echo "[$((TOTAL + 1))] Negative test $neg_name ... SKIPPED (Graduated to supported feature in Phase 17)"
        continue
    fi
    TOTAL=$((TOTAL + 1))
    echo -n "[$TOTAL] Negative boundary test $neg_name ... "

    set +e
    ERR_MSG=$(./selfhost/cco "$neg_file" --selfhost-native -o "build/phase15_bin/${neg_name}_out" 2>&1)
    NEG_EXIT=$?
    set -e

    if [ "$NEG_EXIT" -eq 0 ]; then
        echo "FAILED (Expected rejection, but compilation succeeded)"
        exit 1
    fi

    # Verify Valgrind clean on rejection
    if command -v valgrind > /dev/null 2>&1; then
        set +e
        valgrind --leak-check=full --error-exitcode=1 ./selfhost/cco "$neg_file" --selfhost-native -o "build/phase15_bin/${neg_name}_out" > /dev/null 2>&1
        VG_NEG_EXIT=$?
        set -e
        # Valgrind returns 1 if leaks, and the compiler also returns 1 on error,
        # so check if valgrind reported actual errors:
        valgrind --leak-check=full --log-file="build/phase15_bin/${neg_name}_vg.log" ./selfhost/cco "$neg_file" --selfhost-native -o "build/phase15_bin/${neg_name}_out" > /dev/null 2>&1 || true
        if grep -q "definitely lost: [1-9]" "build/phase15_bin/${neg_name}_vg.log" || grep -q "ERROR SUMMARY: [1-9]" "build/phase15_bin/${neg_name}_vg.log"; then
            echo "FAILED (Valgrind leak/error in negative test handler)"
            cat "build/phase15_bin/${neg_name}_vg.log"
            exit 1
        fi
    fi

    echo "PASSED (cleanly rejected: $ERR_MSG)"
    PASSED=$((PASSED + 1))
done

echo "=============================================================="
echo "Phase 15 Test Suite Results: $PASSED / $TOTAL Passed (100% Success)"
echo "=============================================================="
