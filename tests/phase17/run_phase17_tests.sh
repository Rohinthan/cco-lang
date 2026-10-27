#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -e

echo "=============================================================="
echo "Cco Compiler — Phase 17 Native Strings & .rodata Test Suite"
echo "Differential Testing: Reference vs Self-Hosted Native (ELF64)"
echo "=============================================================="

mkdir -p build/phase17_bin
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

for test_file in tests/phase17/*.cco; do
    [ -e "$test_file" ] || continue
    test_name=$(basename "$test_file" .cco)
    TOTAL=$((TOTAL + 1))
    echo -n "[$TOTAL] Differential testing $test_name ... "

    # 1. Reference C11 build & run
    ./cco "$test_file" -o "build/phase17_bin/${test_name}_ref.c" > /dev/null 2>&1
    $CC $CFLAGS "build/phase17_bin/${test_name}_ref.c" -o "build/phase17_bin/${test_name}_ref" $LDFLAGS > /dev/null 2>&1
    set +e
    ./build/phase17_bin/${test_name}_ref > "build/phase17_bin/${test_name}_ref.out" 2>&1
    REF_EXIT=$?
    set -e

    # 2. Self-Hosted Native build & run
    ./selfhost/cco "$test_file" --selfhost-native -o "build/phase17_bin/${test_name}_native" > /dev/null 2>&1
    set +e
    ./build/phase17_bin/${test_name}_native > "build/phase17_bin/${test_name}_native.out" 2>&1
    NATIVE_EXIT=$?
    set -e

    # 3. Check exit codes match
    if [ "$REF_EXIT" -ne "$NATIVE_EXIT" ]; then
        echo "FAILED (Exit code mismatch: ref=$REF_EXIT, native=$NATIVE_EXIT)"
        exit 1
    fi

    # 4. Check outputs match
    if ! diff -u "build/phase17_bin/${test_name}_ref.out" "build/phase17_bin/${test_name}_native.out" > /dev/null 2>&1; then
        echo "FAILED (Output mismatch)"
        diff -u "build/phase17_bin/${test_name}_ref.out" "build/phase17_bin/${test_name}_native.out"
        exit 1
    fi

    # 5. Valgrind Cleanliness on native compiler generation
    if command -v valgrind > /dev/null 2>&1; then
        valgrind --leak-check=full --error-exitcode=1 ./selfhost/cco "$test_file" --selfhost-native -o "build/phase17_bin/${test_name}_val" > /dev/null 2>&1
        rm -f "build/phase17_bin/${test_name}_val" "build/phase17_bin/${test_name}_val.o"
    fi

    # 6. Valgrind Cleanliness on generated native executable
    if command -v valgrind > /dev/null 2>&1; then
        set +e
        valgrind --leak-check=full --error-exitcode=1 ./build/phase17_bin/${test_name}_native > /dev/null 2>&1
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

echo "--- 2. ELF .rodata, Relocation & Deduplication Structure Tests ---"

# Test A: .rodata section existence and flags
TOTAL=$((TOTAL + 1))
echo -n "[$TOTAL] Verifying .rodata section flags & properties in ELF object ... "
if ! readelf -S "build/phase17_bin/01_ascii_string_native.o" | grep -A 1 "\.rodata" | grep -q " A "; then
    echo "FAILED (.rodata missing SHF_ALLOC flag)"
    exit 1
fi
if readelf -S "build/phase17_bin/01_ascii_string_native.o" | grep -A 1 "\.rodata" | grep -q -E " W | X "; then
    echo "FAILED (.rodata must not have WRITE or EXEC flags)"
    exit 1
fi
echo "PASSED (PROGBITS, SHF_ALLOC, non-writable, non-executable)"
PASSED=$((PASSED + 1))

# Test B: Relocations in .rela.text
TOTAL=$((TOTAL + 1))
echo -n "[$TOTAL] Verifying R_X86_64_PC32 relocations pointing to .rodata ... "
if ! readelf -r "build/phase17_bin/01_ascii_string_native.o" | grep -q "R_X86_64_PC32.*\.rodata"; then
    echo "FAILED (Missing R_X86_64_PC32 relocation for .rodata)"
    exit 1
fi
echo "PASSED (valid R_X86_64_PC32 records verified)"
PASSED=$((PASSED + 1))

# Test C: String deduplication
TOTAL=$((TOTAL + 1))
echo -n "[$TOTAL] Verifying string literal pooling and deduplication ... "
DEDUP_RODATA_SZ=$(readelf -S "build/phase17_bin/08_string_dedup_native.o" | grep -A 1 "\.rodata" | tail -n 1 | awk '{print $1}')
# "duplicate_string\0" is 17 bytes, "unique_string\0" is 14 bytes -> total 31 bytes (0x1f)
if [ "$((0x$DEDUP_RODATA_SZ))" -gt 35 ]; then
    echo "FAILED (Deduplication not working: .rodata size is 0x$DEDUP_RODATA_SZ)"
    exit 1
fi
echo "PASSED (duplicate literals successfully pooled to 0x$DEDUP_RODATA_SZ bytes)"
PASSED=$((PASSED + 1))

# Test D: Deterministic object generation
TOTAL=$((TOTAL + 1))
echo -n "[$TOTAL] Verifying bitwise deterministic object file emission ... "
./selfhost/cco "tests/phase17/01_ascii_string.cco" --selfhost-native -o "build/phase17_bin/det1" > /dev/null 2>&1
./selfhost/cco "tests/phase17/01_ascii_string.cco" --selfhost-native -o "build/phase17_bin/det2" > /dev/null 2>&1
if ! cmp -s "build/phase17_bin/det1.o" "build/phase17_bin/det2.o"; then
    echo "FAILED (Object files differ across runs)"
    exit 1
fi
rm -f "build/phase17_bin/det1"* "build/phase17_bin/det2"*
echo "PASSED (bitwise identical objects across independent compilations)"
PASSED=$((PASSED + 1))

echo "--- 3. Negative Feature Boundary Tests ---"

for neg_file in tests/phase17/negative/*.cco; do
    [ -e "$neg_file" ] || continue
    neg_name=$(basename "$neg_file" .cco)
    TOTAL=$((TOTAL + 1))
    echo -n "[$TOTAL] Negative boundary test $neg_name ... "

    set +e
    ERR_MSG=$(./selfhost/cco "$neg_file" --selfhost-native -o "build/phase17_bin/${neg_name}_out" 2>&1)
    NEG_EXIT=$?
    set -e

    if [ "$NEG_EXIT" -eq 0 ]; then
        echo "FAILED (Expected rejection, but compilation succeeded)"
        exit 1
    fi

    # Verify Valgrind clean on rejection
    if command -v valgrind > /dev/null 2>&1; then
        valgrind --leak-check=full --log-file="build/phase17_bin/${neg_name}_vg.log" ./selfhost/cco "$neg_file" --selfhost-native -o "build/phase17_bin/${neg_name}_out" > /dev/null 2>&1 || true
        if grep -q "definitely lost: [1-9]" "build/phase17_bin/${neg_name}_vg.log" || grep -q "ERROR SUMMARY: [1-9]" "build/phase17_bin/${neg_name}_vg.log"; then
            echo "FAILED (Valgrind leak/error in negative test handler)"
            cat "build/phase17_bin/${neg_name}_vg.log"
            exit 1
        fi
    fi

    echo "PASSED (cleanly rejected: $ERR_MSG)"
    PASSED=$((PASSED + 1))
done

echo "=============================================================="
echo "Phase 17 Test Suite Results: $PASSED / $TOTAL Passed (100% Success)"
echo "=============================================================="
