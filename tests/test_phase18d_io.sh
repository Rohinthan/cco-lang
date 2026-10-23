#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# ==============================================================================
# Cco Phase 18D — Milestone 1: File I/O Hardening & Result Integration Test Suite
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$ROOT_DIR/cco"
TMP_DIR="/tmp/cco_phase18d_io_tests_$$"

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
echo "Cco Phase 18D Milestone 1: File I/O & Result Integration Tests"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Test 1: Valid File Reading via file_read_result
# ------------------------------------------------------------------------------
echo "Test 1: Valid file reading via file_read_result (ResultStr.Ok)..."
cat << 'EOF' > "$TMP_DIR/test1_valid.cco"
import "std/io.cco";

fn main() -> int {
    let res: ResultStr = file_read_result("LICENSE");
    match res {
        ResultStr.Ok { value } => {
            if (len(value) > 100) {
                print("Valid read OK");
                return 0;
            } else {
                print("Content unexpectedly short");
                return 1;
            }
        }
        ResultStr.Err { message } => {
            print(concat("Unexpected read failure: ", message));
            return 2;
        }
    }
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test1_valid.cco" -o "$TMP_DIR/test1_valid"
OUT=$("$TMP_DIR/test1_valid")
if [[ "$OUT" == "Valid read OK" ]]; then
    pass "file_read_result returns ResultStr.Ok with full file contents"
else
    fail "Test 1 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 2: Empty File Reading (0 bytes)
# ------------------------------------------------------------------------------
echo "Test 2: Empty file reading (0 bytes)..."
touch "$TMP_DIR/empty.txt"
cat << EOF > "$TMP_DIR/test2_empty.cco"
import "std/io.cco";

fn main() -> int {
    let res: ResultStr = file_read_result("$TMP_DIR/empty.txt");
    match res {
        ResultStr.Ok { value } => {
            if (len(value) == 0 && equals(value, "")) {
                print("Empty file read OK");
                return 0;
            } else {
                print("Expected empty string");
                return 1;
            }
        }
        ResultStr.Err { message } => {
            print(concat("Unexpected error: ", message));
            return 2;
        }
    }
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test2_empty.cco" -o "$TMP_DIR/test2_empty"
OUT=$("$TMP_DIR/test2_empty")
if [[ "$OUT" == "Empty file read OK" ]]; then
    pass "file_read_result handles 0-byte file returning ResultStr.Ok with empty string"
else
    fail "Test 2 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 3: Missing File Non-Fatal Error Handling
# ------------------------------------------------------------------------------
echo "Test 3: Missing file non-fatal error handling (ResultStr.Err)..."
cat << 'EOF' > "$TMP_DIR/test3_missing.cco"
import "std/io.cco";

fn main() -> int {
    let res: ResultStr = file_read_result("non_existent_file_definitely_not_here_123.txt");
    match res {
        ResultStr.Ok { value } => {
            print("Expected Err for missing file, got Ok");
            return 1;
        }
        ResultStr.Err { message } => {
            if (len(message) > 0) {
                print(concat("Missing file handled: ", message));
                return 0;
            } else {
                print("Empty error message");
                return 2;
            }
        }
    }
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test3_missing.cco" -o "$TMP_DIR/test3_missing"
OUT=$("$TMP_DIR/test3_missing")
if [[ "$OUT" == "Missing file handled: No such file or directory" ]]; then
    pass "Missing file cleanly produces ResultStr.Err without process termination"
else
    fail "Test 3 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 4: Inaccessible Path / Directory Handling
# ------------------------------------------------------------------------------
echo "Test 4: Inaccessible path / directory handling..."
cat << 'EOF' > "$TMP_DIR/test4_dir.cco"
import "std/io.cco";

fn main() -> int {
    let res: ResultStr = file_read_result("/");
    match res {
        ResultStr.Ok { value } => {
            print("Expected Err for directory, got Ok");
            return 1;
        }
        ResultStr.Err { message } => {
            if (len(message) > 0) {
                print(concat("Directory read rejected: ", message));
                return 0;
            } else {
                print("Empty error message");
                return 2;
            }
        }
    }
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test4_dir.cco" -o "$TMP_DIR/test4_dir"
OUT=$("$TMP_DIR/test4_dir")
if [[ "$OUT" == *"Directory read rejected: Is a directory"* ]]; then
    pass "Directory read cleanly returns ResultStr.Err('Is a directory') without abort"
else
    fail "Test 4 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 5: Backward Compatibility of legacy read_file / file_read
# ------------------------------------------------------------------------------
echo "Test 5: Compatibility with legacy file_read / read_file..."
cat << 'EOF' > "$TMP_DIR/test5_legacy_ok.cco"
import "std/io.cco";

fn main() -> int {
    let s: string = file_read("LICENSE");
    if (len(s) > 100) {
        print("Legacy read OK");
        return 0;
    }
    return 1;
}
EOF

"$CCO" "$TMP_DIR/test5_legacy_ok.cco" -o "$TMP_DIR/test5_legacy_ok"
OUT=$("$TMP_DIR/test5_legacy_ok")
if [[ "$OUT" == "Legacy read OK" ]]; then
    pass "Legacy file_read succeeds and returns expected file contents"
else
    fail "Test 5A execution" "Output was: $OUT"
fi

cat << 'EOF' > "$TMP_DIR/test5_legacy_err.cco"
import "std/io.cco";

fn main() -> int {
    let s: string = file_read("non_existent_file_legacy_404.txt");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test5_legacy_err.cco" -o "$TMP_DIR/test5_legacy_err"
set +e
ERR_OUT=$("$TMP_DIR/test5_legacy_err" 2>&1)
ERR_CODE=$?
set -e
if [[ $ERR_CODE -eq 1 && "$ERR_OUT" == *"Could not open file"* ]]; then
    pass "Legacy file_read exits with status 1 and informative error message on failure"
else
    fail "Test 5B execution" "Code: $ERR_CODE, Output: $ERR_OUT"
fi

# ------------------------------------------------------------------------------
# Test 6: Unseekable Virtual Files & Streams (/proc and pipes)
# ------------------------------------------------------------------------------
echo "Test 6: Unseekable virtual files and streams (/proc and pipes)..."
cat << 'EOF' > "$TMP_DIR/test6_proc.cco"
import "std/io.cco";

fn main() -> int {
    let res: ResultStr = file_read_result("/proc/version");
    match res {
        ResultStr.Ok { value } => {
            if (len(value) > 0) {
                print("Proc version read OK");
                return 0;
            } else {
                print("Proc version read 0 bytes");
                return 1;
            }
        }
        ResultStr.Err { message } => {
            print(concat("Proc read failed: ", message));
            return 2;
        }
    }
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test6_proc.cco" -o "$TMP_DIR/test6_proc"
OUT=$("$TMP_DIR/test6_proc")
if [[ "$OUT" == "Proc version read OK" ]]; then
    pass "file_read_result reads /proc virtual file where size is reported as 0"
else
    fail "Test 6A execution" "Output was: $OUT"
fi

cat << 'EOF' > "$TMP_DIR/test6_pipe.cco"
import "std/io.cco";

fn main() -> int {
    let res: ResultStr = file_read_result("/dev/stdin");
    match res {
        ResultStr.Ok { value } => {
            if (equals(value, "Stream payload 42\n")) {
                print("Pipe stream read OK");
                return 0;
            } else {
                print(concat("Unexpected pipe content: ", value));
                return 1;
            }
        }
        ResultStr.Err { message } => {
            print(concat("Pipe stream failed: ", message));
            return 2;
        }
    }
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test6_pipe.cco" -o "$TMP_DIR/test6_pipe"
PIPE_OUT=$(printf "Stream payload 42\n" | "$TMP_DIR/test6_pipe")
if [[ "$PIPE_OUT" == "Pipe stream read OK" ]]; then
    pass "file_read_result reads unseekable stream via /dev/stdin cleanly"
else
    fail "Test 6B execution" "Output was: $PIPE_OUT"
fi

# ------------------------------------------------------------------------------
# Test 7: Direct read_file_checked and io_last_error built-in primitives
# ------------------------------------------------------------------------------
echo "Test 7: Direct read_file_checked and io_last_error primitives..."
cat << 'EOF' > "$TMP_DIR/test7_primitives.cco"
import "std/io.cco";

fn main() -> int {
    let s_ok: string = read_file_checked("LICENSE");
    let err_ok: string = io_last_error();
    if (len(s_ok) == 0 || !equals(err_ok, "")) {
        print("Checked read on valid file failed");
        return 1;
    }

    let s_err: string = read_file_checked("missing_file_primitives_999.txt");
    let err_err: string = io_last_error();
    if (!equals(s_err, "") || equals(err_err, "")) {
        print("Checked read on missing file failed");
        return 2;
    }

    print("Checked primitives OK");
    return 0;
}
EOF

"$CCO" "$TMP_DIR/test7_primitives.cco" -o "$TMP_DIR/test7_primitives"
OUT=$("$TMP_DIR/test7_primitives")
if [[ "$OUT" == "Checked primitives OK" ]]; then
    pass "read_file_checked and io_last_error primitives function accurately"
else
    fail "Test 7 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 8: Large file reading (~100 KB)
# ------------------------------------------------------------------------------
echo "Test 8: Large file reading (>64 KB dynamic buffer reallocation)..."
python3 -c "print('A' * 100000)" > "$TMP_DIR/large_100k.txt"
cat << EOF > "$TMP_DIR/test8_large.cco"
import "std/io.cco";

fn main() -> int {
    let res: ResultStr = file_read_result("$TMP_DIR/large_100k.txt");
    match res {
        ResultStr.Ok { value } => {
            if (len(value) == 100001) {
                print("Large file read OK");
                return 0;
            } else {
                print("Length mismatch");
                return 1;
            }
        }
        ResultStr.Err { message } => {
            print(concat("Large read failed: ", message));
            return 2;
        }
    }
    return 3;
}
EOF

"$CCO" "$TMP_DIR/test8_large.cco" -o "$TMP_DIR/test8_large"
OUT=$("$TMP_DIR/test8_large")
if [[ "$OUT" == "Large file read OK" ]]; then
    pass "file_read_result reads large file exceeding initial chunk buffer"
else
    fail "Test 8 execution" "Output was: $OUT"
fi

# ------------------------------------------------------------------------------
# Test 9: Valgrind memory leak verification across all test binaries
# ------------------------------------------------------------------------------
echo "Test 9: Valgrind memory leak verification across all test binaries..."
VALG_FAIL=0
for bin in "$TMP_DIR"/test1_valid "$TMP_DIR"/test2_empty "$TMP_DIR"/test3_missing "$TMP_DIR"/test4_dir "$TMP_DIR"/test5_legacy_ok "$TMP_DIR"/test6_proc "$TMP_DIR"/test7_primitives "$TMP_DIR"/test8_large; do
    if [[ -x "$bin" ]]; then
        if valgrind --leak-check=full --error-exitcode=1 "$bin" > /dev/null 2>&1; then
            :
        else
            VALG_FAIL=$((VALG_FAIL + 1))
            fail "Valgrind memcheck" "$(basename "$bin") leaked memory or raised errors"
        fi
    fi
done

# Also test pipe under valgrind
if printf "Stream payload 42\n" | valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/test6_pipe" > /dev/null 2>&1; then
    :
else
    VALG_FAIL=$((VALG_FAIL + 1))
    fail "Valgrind memcheck" "test6_pipe leaked memory or raised errors"
fi

if [[ $VALG_FAIL -eq 0 ]]; then
    pass "All Phase 18D I/O test binaries run 100% clean under Valgrind (0 leaks, 0 errors)"
fi

# ------------------------------------------------------------------------------
# Test 10: Negative boundary — Native backend compile-time rejection
# ------------------------------------------------------------------------------
echo "Test 10: Negative boundary — compile-time rejection by native backend..."
set +e
NATIVE_ERR=$("$CCO" "$TMP_DIR/test1_valid.cco" --use-native -o "$TMP_DIR/native_bin" 2>&1)
NATIVE_CODE=$?
set -e
if [[ $NATIVE_CODE -eq 0 ]]; then
    fail "Native backend boundary" "Expected native backend to reject enum Result types, but compilation succeeded"
else
    if [[ "$NATIVE_ERR" == *"Cco IR verification failed"* || "$NATIVE_ERR" == *"does not support"* ]]; then
        pass "Native backend cleanly rejected enum Result types with expected diagnostic"
    else
        fail "Native backend diagnostic" "Unexpected error message: $NATIVE_ERR"
    fi
fi

echo "=============================================================="
echo "Phase 18D Milestone 1 Results: $PASSED Passed, $FAILED Failed"
echo "=============================================================="

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
