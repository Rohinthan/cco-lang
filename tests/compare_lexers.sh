#!/usr/bin/env bash
set -e

mkdir -p build

echo "=================================================="
echo "  Cco v12 SELF-HOSTED LEXER COMPARISON HARNESS   "
echo "=================================================="

# 1. Ensure compiler is built
make cco > /dev/null

# 2. Build the self-hosted lexer
./cco selfhost/lexer.cco -o selfhost/lexer_selfhosted.c
gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 selfhost/lexer_selfhosted.c -o selfhost/lexer_selfhosted -lm

PASSED=0
FAILED=0
TOTAL=0

for file in $(find tests/programs examples -name "*.cco" | sort); do
    TOTAL=$((TOTAL + 1))
    echo -n "Comparing lexers on ${file}... "

    cp "$file" target.cco
    ./cco --dump-tokens target.cco > build/ground_truth.txt
    ./selfhost/lexer_selfhosted > build/selfhosted_output.txt

    if diff -u build/ground_truth.txt build/selfhosted_output.txt > /dev/null; then
        echo "PASS"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL (Token stream mismatch)"
        diff -u build/ground_truth.txt build/selfhosted_output.txt | head -n 30
        FAILED=$((FAILED + 1))
    fi
done

# Cleanup temporary target and output files
rm -f target.cco build/ground_truth.txt build/selfhosted_output.txt

echo "=================================================="
echo "Summary: ${PASSED}/${TOTAL} Passed, ${FAILED} Failed"
echo "=================================================="

if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
