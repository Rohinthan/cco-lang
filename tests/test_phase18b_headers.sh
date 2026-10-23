#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18B: Dependency-Aware C Header Generation Verification Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_phase18b_tests_$$"

mkdir -p "$TMP_DIR"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

PASSED=0
FAILED=0

pass() {
    echo -e "  \033[32m[PASS]\033[0m $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo -e "  \033[31m[FAIL]\033[0m $1: $2"
    FAILED=$((FAILED + 1))
}

echo "=============================================================="
echo "Cco Phase 18B: Dependency-Aware C Header Generation Test Suite"
echo "=============================================================="

# Test 1: Minimal Hello World - Only stdio.h, no POSIX, no -lm
echo "Test 1: Minimal Hello World header emission..."
"$CCO" "$ROOT_DIR/tests/programs/01_hello.cco" --emit-c > "$TMP_DIR/hello.c"
if grep -q "#include <stdio.h>" "$TMP_DIR/hello.c" && \
   ! grep -q "_POSIX_C_SOURCE" "$TMP_DIR/hello.c" && \
   ! grep -q "<math.h>" "$TMP_DIR/hello.c" && \
   ! grep -q "<stdlib.h>" "$TMP_DIR/hello.c" && \
   ! grep -q "<string.h>" "$TMP_DIR/hello.c" && \
   ! grep -q "<sys/socket.h>" "$TMP_DIR/hello.c" && \
   ! grep -q "<unistd.h>" "$TMP_DIR/hello.c"; then
    pass "Minimal program emits only <stdio.h> without POSIX or unused headers"
else
    fail "Minimal program header check" "Unexpected headers emitted in hello.c"
fi

LINK_CMD=$(CC="echo" "$CCO" "$ROOT_DIR/tests/programs/01_hello.cco" -o "$TMP_DIR/hello_bin")
if [[ "$LINK_CMD" != *"-lm"* ]]; then
    pass "Minimal program does not link -lm"
else
    fail "Minimal program linker check" "Found unexpected -lm in linker command: $LINK_CMD"
fi

# Test 2: Integer Arithmetic - No math.h, no -lm
echo "Test 2: Pure integer arithmetic header emission..."
cat << 'EOF' > "$TMP_DIR/arith.cco"
fn main() -> int {
    let a: int = 10;
    let b: int = 20;
    let c: int = (a * b) + (b / a) - 5;
    print(c);
    return 0;
}
EOF
"$CCO" "$TMP_DIR/arith.cco" --emit-c > "$TMP_DIR/arith.c"
if ! grep -q "<math.h>" "$TMP_DIR/arith.c" && \
   ! grep -q "_POSIX_C_SOURCE" "$TMP_DIR/arith.c"; then
    pass "Integer arithmetic program emits no <math.h> or POSIX macros"
else
    fail "Arithmetic header check" "Unexpected math or POSIX header in arith.c"
fi

LINK_CMD=$(CC="echo" "$CCO" "$TMP_DIR/arith.cco" -o "$TMP_DIR/arith_bin")
if [[ "$LINK_CMD" != *"-lm"* ]]; then
    pass "Integer arithmetic does not link -lm"
else
    fail "Arithmetic linker check" "Unexpected -lm found in linker command"
fi

# Test 3: Math Library Call - Emits math.h, links -lm, runs cleanly
echo "Test 3: Math library calls..."
"$CCO" "$ROOT_DIR/tests/programs/22_stdlib_math.cco" --emit-c > "$TMP_DIR/math.c"
if grep -q "#include <math.h>" "$TMP_DIR/math.c"; then
    pass "Math program emits <math.h>"
else
    fail "Math header check" "Missing <math.h> in math.c"
fi

LINK_CMD=$(CC="echo" "$CCO" "$ROOT_DIR/tests/programs/22_stdlib_math.cco" -o "$TMP_DIR/math_bin")
if [[ "$LINK_CMD" == *"-lm"* ]]; then
    pass "Math program links -lm"
else
    fail "Math linker check" "Expected -lm in linker command, got: $LINK_CMD"
fi

"$CCO" "$ROOT_DIR/tests/programs/22_stdlib_math.cco" -o "$TMP_DIR/math_bin"
MATH_OUT=$("$TMP_DIR/math_bin")
if [[ "$MATH_OUT" == *"4"* ]]; then
    pass "Math program executes cleanly with correct output"
else
    fail "Math execution" "Unexpected output: $MATH_OUT"
fi

# Test 4: Boolean Types & Logic - Emits stdbool.h, no math.h
echo "Test 4: Boolean types and logical operators..."
cat << 'EOF' > "$TMP_DIR/bool.cco"
fn is_positive(x: int) -> bool {
    return x > 0;
}

fn main() -> int {
    let flag: bool = is_positive(5);
    if (flag && !false) {
        print(1);
    }
    return 0;
}
EOF
"$CCO" "$TMP_DIR/bool.cco" --emit-c > "$TMP_DIR/bool.c"
if grep -q "#include <stdbool.h>" "$TMP_DIR/bool.c" && \
   ! grep -q "<math.h>" "$TMP_DIR/bool.c" && \
   ! grep -q "<sys/socket.h>" "$TMP_DIR/bool.c"; then
    pass "Boolean program emits <stdbool.h> without math or socket headers"
else
    fail "Boolean header check" "Invalid headers in bool.c"
fi

# Test 5: String Variables - Emits string.h, stdlib.h, POSIX macros, no math.h
echo "Test 5: String variables and string operations..."
"$CCO" "$ROOT_DIR/tests/phase17/09_string_variable_assignment.cco" --emit-c > "$TMP_DIR/str.c"
if grep -q "#include <string.h>" "$TMP_DIR/str.c" && \
   grep -q "#include <stdlib.h>" "$TMP_DIR/str.c" && \
   grep -q "#define _POSIX_C_SOURCE 200809L" "$TMP_DIR/str.c" && \
   ! grep -q "<math.h>" "$TMP_DIR/str.c" && \
   ! grep -q "<sys/socket.h>" "$TMP_DIR/str.c"; then
    pass "String program emits <string.h>, <stdlib.h>, POSIX macros without math/socket headers"
else
    fail "String header check" "Invalid headers in str.c"
fi

LINK_CMD=$(CC="echo" "$CCO" "$ROOT_DIR/tests/phase17/09_string_variable_assignment.cco" -o "$TMP_DIR/str_bin")
if [[ "$LINK_CMD" != *"-lm"* ]]; then
    pass "String program does not link -lm"
else
    fail "String linker check" "Unexpected -lm found in linker command"
fi

# Test 6: Map Data Structures - Emits stdint.h, stdlib.h, string.h, POSIX macros
echo "Test 6: Map collections..."
"$CCO" "$ROOT_DIR/tests/programs/46_map_basic_primitive.cco" --emit-c > "$TMP_DIR/map.c"
if grep -q "#include <stdint.h>" "$TMP_DIR/map.c" && \
   grep -q "#include <stdlib.h>" "$TMP_DIR/map.c" && \
   grep -q "#define _POSIX_C_SOURCE 200809L" "$TMP_DIR/map.c" && \
   ! grep -q "<math.h>" "$TMP_DIR/map.c" && \
   ! grep -q "<sys/socket.h>" "$TMP_DIR/map.c"; then
    pass "Map program emits <stdint.h>, <stdlib.h>, POSIX macros without math/socket headers"
else
    fail "Map header check" "Invalid headers in map.c"
fi

# Test 7: Networking Calls - Emits sys/socket.h, netinet/in.h, unistd.h, POSIX macros
echo "Test 7: POSIX Networking headers..."
"$CCO" "$ROOT_DIR/tests/network/test_server.cco" --emit-c > "$TMP_DIR/net.c"
if grep -q "#include <sys/socket.h>" "$TMP_DIR/net.c" && \
   grep -q "#include <netinet/in.h>" "$TMP_DIR/net.c" && \
   grep -q "#include <unistd.h>" "$TMP_DIR/net.c" && \
   grep -q "#define _POSIX_C_SOURCE 200809L" "$TMP_DIR/net.c" && \
   ! grep -q "<math.h>" "$TMP_DIR/net.c"; then
    pass "Networking program emits socket headers and POSIX macros without math headers"
else
    fail "Networking header check" "Invalid headers in net.c"
fi

LINK_CMD=$(CC="echo" "$CCO" "$ROOT_DIR/tests/network/test_server.cco" -o "$TMP_DIR/net_bin")
if [[ "$LINK_CMD" != *"-lm"* ]]; then
    pass "Networking program does not link -lm"
else
    fail "Networking linker check" "Unexpected -lm found in linker command"
fi

# Test 8: Deterministic Output Emission (Bitwise Identical)
echo "Test 8: Deterministic compilation output..."
"$CCO" "$ROOT_DIR/tests/programs/01_hello.cco" --emit-c > "$TMP_DIR/det1.c"
"$CCO" "$ROOT_DIR/tests/programs/01_hello.cco" --emit-c > "$TMP_DIR/det2.c"
if cmp -s "$TMP_DIR/det1.c" "$TMP_DIR/det2.c"; then
    pass "Repeated compilation yields bitwise identical C output for hello.cco"
else
    fail "Determinism check" "hello.cco repeated compilation differed"
fi

"$CCO" "$ROOT_DIR/tests/network/test_server.cco" --emit-c > "$TMP_DIR/det_net1.c"
"$CCO" "$ROOT_DIR/tests/network/test_server.cco" --emit-c > "$TMP_DIR/det_net2.c"
if cmp -s "$TMP_DIR/det_net1.c" "$TMP_DIR/det_net2.c"; then
    pass "Repeated compilation yields bitwise identical C output for test_server.cco"
else
    fail "Determinism check" "test_server.cco repeated compilation differed"
fi

# Test 9: Header Ordering Invariant
echo "Test 9: Header ordering verification..."
# In net.c, verify order:
# 1. POSIX macros
# 2. stdio.h
# 3. stdlib.h
# 4. stdbool.h
# 5. stdint.h
# 6. string.h
# 7. errno.h
# 8. unistd.h
# 9. sys/socket.h
LINE_POSIX=$(grep -n "_POSIX_C_SOURCE" "$TMP_DIR/net.c" | head -n1 | cut -d: -f1)
LINE_STDIO=$(grep -n "<stdio.h>" "$TMP_DIR/net.c" | head -n1 | cut -d: -f1)
LINE_STDLIB=$(grep -n "<stdlib.h>" "$TMP_DIR/net.c" | head -n1 | cut -d: -f1)
LINE_SOCKET=$(grep -n "<sys/socket.h>" "$TMP_DIR/net.c" | head -n1 | cut -d: -f1)

if (( LINE_POSIX < LINE_STDIO && LINE_STDIO < LINE_STDLIB && LINE_STDLIB < LINE_SOCKET )); then
    pass "Header emission respects strict category ordering (POSIX macros -> C11 std -> POSIX sys)"
else
    fail "Header ordering check" "Headers are out of order in generated C file"
fi

# Test 10: Strict Compilation of Generated C with -pedantic-errors
echo "Test 10: Strict C11 compilation of diverse generated files..."
for f in "$TMP_DIR/hello.c" "$TMP_DIR/arith.c" "$TMP_DIR/math.c" "$TMP_DIR/bool.c" "$TMP_DIR/str.c" "$TMP_DIR/map.c"; do
    MATH_ARG=""
    if grep -q "<math.h>" "$f"; then
        MATH_ARG="-lm"
    fi
    if gcc -Wall -Wextra -Werror -pedantic-errors -std=c11 -c "$f" -o "$TMP_DIR/test.o" $MATH_ARG; then
        pass "Strict C11 -pedantic-errors compilation passed for $(basename "$f")"
    else
        fail "Strict compilation" "Failed to compile $(basename "$f") with strict C11 flags"
    fi
done

echo "=============================================================="
echo "Phase 18B Test Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
