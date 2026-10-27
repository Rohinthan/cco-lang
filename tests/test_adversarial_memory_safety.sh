#!/usr/bin/env bash
# ==============================================================================
# tests/test_adversarial_memory_safety.sh
# Cco Compiler Ownership & Memory-Safety: Allocation Failures, Partial Init,
# Maps, and Nested Resource Lifecycles Suite
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_adv_XXXXXX)

trap 'rm -rf "$TMP_DIR"' EXIT

passed_tests=0
failed_tests=0
total_tests=0

run_test() {
    local test_name="$1"
    total_tests=$((total_tests + 1))
    printf "[%2d] %s ... " "$total_tests" "$test_name"
}

pass_test() {
    passed_tests=$((passed_tests + 1))
    printf "PASSED\n"
}

fail_test() {
    local reason="$1"
    failed_tests=$((failed_tests + 1))
    printf "FAILED (%s)\n" "$reason"
}

if [ ! -x "$CCO" ]; then
    echo "Compiler binary '$CCO' not found. Building with make..."
    (cd "$REPO_ROOT" && make)
fi

# Build deterministic linker-wrapped allocator fault injector for Tests 1-5 and growth tests
cat << 'EOF' > "$TMP_DIR/fault_injector.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_alloc_at = -1;
static int alloc_count = 0;

static void check_env(void) {
    static int checked = 0;
    if (!checked) {
        checked = 1;
        const char *e = getenv("CCO_FAIL_ALLOC_AT");
        if (e) fail_alloc_at = atoi(e);
    }
}

void *__real_calloc(size_t, size_t);
void *__wrap_calloc(size_t nmemb, size_t size) {
    check_env();
    alloc_count++;
    if (fail_alloc_at > 0 && alloc_count == fail_alloc_at) {
        fprintf(stderr, "[FAULT INJECTOR] Injected failure at alloc #%d: calloc(%zu, %zu)\n", alloc_count, nmemb, size);
        return NULL;
    }
    return __real_calloc(nmemb, size);
}

char *__real_strdup(const char *);
char *__wrap_strdup(const char *s) {
    check_env();
    alloc_count++;
    if (fail_alloc_at > 0 && alloc_count == fail_alloc_at) {
        fprintf(stderr, "[FAULT INJECTOR] Injected failure at alloc #%d: strdup(\"%s\")\n", alloc_count, s ? s : "NULL");
        return NULL;
    }
    return __real_strdup(s);
}

void *__real_malloc(size_t);
void *__wrap_malloc(size_t size) {
    check_env();
    alloc_count++;
    if (fail_alloc_at > 0 && alloc_count == fail_alloc_at) {
        fprintf(stderr, "[FAULT INJECTOR] Injected failure at alloc #%d: malloc(%zu)\n", alloc_count, size);
        return NULL;
    }
    return __real_malloc(size);
}

void *__real_realloc(void *, size_t);
void *__wrap_realloc(void *ptr, size_t size) {
    check_env();
    alloc_count++;
    if (fail_alloc_at > 0 && alloc_count == fail_alloc_at) {
        fprintf(stderr, "[FAULT INJECTOR] Injected failure at alloc #%d: realloc(%zu)\n", alloc_count, size);
        return NULL;
    }
    return __real_realloc(ptr, size);
}
EOF

echo "=============================================================="
echo "Cco: Adversarial Memory-Safety & Allocation Failure Suite"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Test 1: Class allocation failure handling (calloc fails -> clean exit 1)
# ------------------------------------------------------------------------------
run_test "Test 1: Class allocation failure terminates safely with exit code 1"
cat << 'EOF' > "$TMP_DIR/t1.cco"
class Buffer {
    size: int;
}
fn main() -> int {
    let b = Buffer { size: 1024 };
    print(b.size);
    return 0;
}
EOF
rm -f "$TMP_DIR/t1.c" "$TMP_DIR/t1.bin"
if ! "$CCO" "$TMP_DIR/t1.cco" -o "$TMP_DIR/t1.c" 2>"$TMP_DIR/t1.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t1.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t1.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t1.bin" -lm 2>"$TMP_DIR/t1_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t1_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=1 "$TMP_DIR/t1.bin" >/dev/null 2>"$TMP_DIR/t1_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on OOM, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #1: calloc" "$TMP_DIR/t1_run.err"; then
        fail_test "fault injector did not target expected calloc: $(cat "$TMP_DIR/t1_run.err")"
    elif ! grep -q "Memory allocation failed in Buffer_new()" "$TMP_DIR/t1_run.err"; then
        fail_test "expected OOM message, got: $(cat "$TMP_DIR/t1_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 2: Class string field duplication failure (strdup fails -> clean exit 1)
# ------------------------------------------------------------------------------
run_test "Test 2: Class string duplication failure cleans partially initialized object"
cat << 'EOF' > "$TMP_DIR/t2.cco"
class Buffer {
    name: string;
}
fn main() -> int {
    let b = Buffer { name: "dynamic_buffer_name" };
    print(b.name);
    return 0;
}
EOF
rm -f "$TMP_DIR/t2.c" "$TMP_DIR/t2.bin"
if ! "$CCO" "$TMP_DIR/t2.cco" -o "$TMP_DIR/t2.c" 2>"$TMP_DIR/t2.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t2.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t2.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t2.bin" -lm 2>"$TMP_DIR/t2_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t2_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=2 "$TMP_DIR/t2.bin" >/dev/null 2>"$TMP_DIR/t2_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on strdup OOM, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #2: strdup(\"dynamic_buffer_name\")" "$TMP_DIR/t2_run.err"; then
        fail_test "fault injector did not target expected strdup: $(cat "$TMP_DIR/t2_run.err")"
    elif ! grep -q "Memory allocation failed in strdup() for Buffer.name" "$TMP_DIR/t2_run.err"; then
        fail_test "expected OOM message, got: $(cat "$TMP_DIR/t2_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 3: Enum allocation failure handling (calloc fails -> clean exit 1)
# ------------------------------------------------------------------------------
run_test "Test 3: Enum allocation failure terminates safely with exit code 1"
cat << 'EOF' > "$TMP_DIR/t3.cco"
enum Status {
    Ok,
    Err { code: int },
}
fn main() -> int {
    let s = Status.Err { code: 500 };
    match s {
        Status.Ok => { print("ok"); }
        Status.Err { code } => { print(code); }
    }
    return 0;
}
EOF
rm -f "$TMP_DIR/t3.c" "$TMP_DIR/t3.bin"
if ! "$CCO" "$TMP_DIR/t3.cco" -o "$TMP_DIR/t3.c" 2>"$TMP_DIR/t3.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t3.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t3.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t3.bin" -lm 2>"$TMP_DIR/t3_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t3_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=1 "$TMP_DIR/t3.bin" >/dev/null 2>"$TMP_DIR/t3_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on enum OOM, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #1: calloc" "$TMP_DIR/t3_run.err"; then
        fail_test "fault injector did not target expected calloc: $(cat "$TMP_DIR/t3_run.err")"
    elif ! grep -q "Memory allocation failed in Status_new_Err()" "$TMP_DIR/t3_run.err"; then
        fail_test "expected OOM message, got: $(cat "$TMP_DIR/t3_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 4: Enum string payload duplication failure
# ------------------------------------------------------------------------------
run_test "Test 4: Enum string payload failure cleans partially initialized variant"
cat << 'EOF' > "$TMP_DIR/t4.cco"
enum Msg {
    Quit,
    Text { content: string },
}
fn main() -> int {
    let m = Msg.Text { content: "payload_content" };
    match m {
        Msg.Quit => { print("quit"); }
        Msg.Text { content } => { print(content); }
    }
    return 0;
}
EOF
rm -f "$TMP_DIR/t4.c" "$TMP_DIR/t4.bin"
if ! "$CCO" "$TMP_DIR/t4.cco" -o "$TMP_DIR/t4.c" 2>"$TMP_DIR/t4.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t4.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t4.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t4.bin" -lm 2>"$TMP_DIR/t4_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t4_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=2 "$TMP_DIR/t4.bin" >/dev/null 2>"$TMP_DIR/t4_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on enum strdup OOM, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #2: strdup(\"payload_content\")" "$TMP_DIR/t4_run.err"; then
        fail_test "fault injector did not target expected strdup: $(cat "$TMP_DIR/t4_run.err")"
    elif ! grep -q "Memory allocation failed in strdup() for Msg.Text.content" "$TMP_DIR/t4_run.err"; then
        fail_test "expected OOM message, got: $(cat "$TMP_DIR/t4_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 5: Partial initialization cleanup of multi-field class (field 2 failure)
# ------------------------------------------------------------------------------
run_test "Test 5: Partial initialization releases field 1 when field 2 strdup fails"
cat << 'EOF' > "$TMP_DIR/t5.cco"
class Resource {
    id: int;
}
class Multi {
    r: Resource;
    tag1: string;
    tag2: string;
}
fn main() -> int {
    let m = Multi {
        r: Resource { id: 77 },
        tag1: "first_tag",
        tag2: "second_tag"
    };
    print(m.r.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t5.c" "$TMP_DIR/t5.bin"
if ! "$CCO" "$TMP_DIR/t5.cco" -o "$TMP_DIR/t5.c" 2>"$TMP_DIR/t5.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t5.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t5.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t5.bin" -lm 2>"$TMP_DIR/t5_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t5_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=4 "$TMP_DIR/t5.bin" >/dev/null 2>"$TMP_DIR/t5_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on partial init OOM, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #4: strdup(\"second_tag\")" "$TMP_DIR/t5_run.err"; then
        fail_test "fault injector did not target expected strdup for tag2: $(cat "$TMP_DIR/t5_run.err")"
    elif ! grep -q "Memory allocation failed in strdup() for Multi.tag2" "$TMP_DIR/t5_run.err"; then
        fail_test "expected OOM message, got: $(cat "$TMP_DIR/t5_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 5b: Partial initialization cleanup when field 1 strdup fails
# ------------------------------------------------------------------------------
run_test "Test 5b: Partial initialization releases resource r when field 1 strdup fails"
set +e
CCO_FAIL_ALLOC_AT=3 "$TMP_DIR/t5.bin" >/dev/null 2>"$TMP_DIR/t5b_run.err"
exit_code=$?
set -e
if [ "$exit_code" -ne 1 ]; then
    fail_test "expected exit code 1 on partial init OOM, got $exit_code"
elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #3: strdup(\"first_tag\")" "$TMP_DIR/t5b_run.err"; then
    fail_test "fault injector did not target expected strdup for tag1: $(cat "$TMP_DIR/t5b_run.err")"
elif ! grep -q "Memory allocation failed in strdup() for Multi.tag1" "$TMP_DIR/t5b_run.err"; then
    fail_test "expected OOM message, got: $(cat "$TMP_DIR/t5b_run.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 6: Map key ownership with heap string key (0 leaks under Valgrind)
# ------------------------------------------------------------------------------
run_test "Test 6: Map key ownership with heap-allocated string key (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t6.cco"
fn main() -> int {
    let k = concat("dynamic_", "key");
    let m: map[string]int = map_new(string, int);
    m = put(m, k, 100);
    print(get(m, "dynamic_key"));
    return 0;
}
EOF
rm -f "$TMP_DIR/t6.bin"
if ! "$CCO" "$TMP_DIR/t6.cco" -o "$TMP_DIR/t6.bin" 2>"$TMP_DIR/t6.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t6.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t6.bin" >/dev/null 2>"$TMP_DIR/t6_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t6_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 7: Map key ownership with string literal key (0 leaks under Valgrind)
# ------------------------------------------------------------------------------
run_test "Test 7: Map key ownership with string literal key (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t7.cco"
fn main() -> int {
    let m: map[string]int = map_new(string, int);
    m = put(m, "static_key_1", 10);
    m = put(m, "static_key_2", 20);
    print(get(m, "static_key_1"));
    print(get(m, "static_key_2"));
    return 0;
}
EOF
rm -f "$TMP_DIR/t7.bin"
if ! "$CCO" "$TMP_DIR/t7.cco" -o "$TMP_DIR/t7.bin" 2>"$TMP_DIR/t7.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t7.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t7.bin" >/dev/null 2>"$TMP_DIR/t7_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t7_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 8: Map repeated insertion of same key (replaces value, frees old value)
# ------------------------------------------------------------------------------
run_test "Test 8: Map repeated insertion of same key replaces value and frees old value"
cat << 'EOF' > "$TMP_DIR/t8.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "shared_key", Resource { id: 101 });
    m = put(m, "shared_key", Resource { id: 202 });
    m = put(m, "shared_key", Resource { id: 303 });
    let r: Resource = get(m, "shared_key");
    print(r.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t8.bin"
if ! "$CCO" "$TMP_DIR/t8.cco" -o "$TMP_DIR/t8.bin" 2>"$TMP_DIR/t8.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t8.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t8.bin" >/dev/null 2>"$TMP_DIR/t8_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t8_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 9: Map containing class values that own strings (nested resource cleanup)
# ------------------------------------------------------------------------------
run_test "Test 9: Map containing classes owning strings cleans all nested resources"
cat << 'EOF' > "$TMP_DIR/t9.cco"
class Resource {
    id: int;
    name: string;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 1, name: "nested_res_one" });
    m = put(m, "k1", Resource { id: 2, name: "nested_res_replacement" });
    m = put(m, "k2", Resource { id: 3, name: "nested_res_two" });
    let r: Resource = get(m, "k1");
    print(r.name);
    return 0;
}
EOF
rm -f "$TMP_DIR/t9.bin"
if ! "$CCO" "$TMP_DIR/t9.cco" -o "$TMP_DIR/t9.bin" 2>"$TMP_DIR/t9.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t9.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t9.bin" >/dev/null 2>"$TMP_DIR/t9_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t9_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 10: Empty map destruction (0 leaks under Valgrind)
# ------------------------------------------------------------------------------
run_test "Test 10: Empty map lifecycle and destruction (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t10.cco"
fn main() -> int {
    let m1: map[string]int = map_new(string, int);
    let m2: map[int]int = map_new(int, int);
    print(len(m1));
    print(len(m2));
    return 0;
}
EOF
rm -f "$TMP_DIR/t10.bin"
if ! "$CCO" "$TMP_DIR/t10.cco" -o "$TMP_DIR/t10.bin" 2>"$TMP_DIR/t10.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t10.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t10.bin" >/dev/null 2>"$TMP_DIR/t10_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t10_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 11: Map nested inside a class (class destructor cleans map)
# ------------------------------------------------------------------------------
run_test "Test 11: Class destructor cleans nested map field and entries (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t11.cco"
class Resource {
    id: int;
}
class Container {
    tag: string;
    entries: map[string]Resource;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "resA", Resource { id: 10 });
    m = put(m, "resB", Resource { id: 20 });
    let c = Container {
        tag: "active_container",
        entries: m
    };
    print(f"Container tag: {c.tag}, count: {len(c.entries)}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t11.bin"
if ! "$CCO" "$TMP_DIR/t11.cco" -o "$TMP_DIR/t11.bin" 2>"$TMP_DIR/t11.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t11.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t11.bin" >/dev/null 2>"$TMP_DIR/t11_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t11_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 12: Map returned from a function (caller takes ownership)
# ------------------------------------------------------------------------------
run_test "Test 12: Map returned from a function across scope boundaries (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t12.cco"
fn build_map() -> map[string]int {
    let m: map[string]int = map_new(string, int);
    m = put(m, "alpha", 11);
    m = put(m, "beta", 22);
    return m;
}
fn main() -> int {
    let m = build_map();
    print(get(m, "alpha"));
    print(get(m, "beta"));
    return 0;
}
EOF
rm -f "$TMP_DIR/t12.bin"
if ! "$CCO" "$TMP_DIR/t12.cco" -o "$TMP_DIR/t12.bin" 2>"$TMP_DIR/t12.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t12.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t12.bin" >/dev/null 2>"$TMP_DIR/t12_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t12_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 13: Class containing map of classes returned from a function
# ------------------------------------------------------------------------------
run_test "Test 13: Function returning class with map of resource classes (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t13.cco"
class Item {
    id: int;
    name: string;
}
class Inventory {
    tag: string;
    items: map[string]Item;
}
fn create_inventory(tag: string) -> Inventory {
    let m: map[string]Item = map_new(string, Item);
    m = put(m, "potion", Item { id: 1, name: "Health Potion" });
    m = put(m, "scroll", Item { id: 2, name: "Fire Scroll" });
    return Inventory {
        tag: tag,
        items: m
    };
}
fn main() -> int {
    let inv = create_inventory("starter_pack");
    let item: Item = get(inv.items, "potion");
    print(f"Inventory: {inv.tag}, Item: {item.name}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t13.bin"
if ! "$CCO" "$TMP_DIR/t13.cco" -o "$TMP_DIR/t13.bin" 2>"$TMP_DIR/t13.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t13.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t13.bin" >/dev/null 2>"$TMP_DIR/t13_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t13_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 14: Function returning class with multiple string fields
# ------------------------------------------------------------------------------
run_test "Test 14: Function returning class with multiple string fields (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t14.cco"
class Record {
    first_name: string;
    last_name: string;
    title: string;
}
fn make_record(f: string, l: string, t: string) -> Record {
    return Record {
        first_name: f,
        last_name: l,
        title: t
    };
}
fn main() -> int {
    let r = make_record("Ada", "Lovelace", "Pioneer");
    print(f"{r.title}: {r.first_name} {r.last_name}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t14.bin"
if ! "$CCO" "$TMP_DIR/t14.cco" -o "$TMP_DIR/t14.bin" 2>"$TMP_DIR/t14.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t14.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t14.bin" >/dev/null 2>"$TMP_DIR/t14_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t14_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 15: Map remove() with tombstone cleanup and value ownership transfer
# ------------------------------------------------------------------------------
run_test "Test 15: Map remove() tombstone handling and value transfer (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t15.cco"
class Resource {
    id: int;
    name: string;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 1, name: "res1" });
    m = put(m, "k2", Resource { id: 2, name: "res2" });
    let removed: Resource = remove(m, "k1");
    print(f"Removed: {removed.name}, remaining count: {len(m)}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t15.bin"
if ! "$CCO" "$TMP_DIR/t15.cco" -o "$TMP_DIR/t15.bin" 2>"$TMP_DIR/t15.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t15.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t15.bin" >/dev/null 2>"$TMP_DIR/t15_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t15_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 16: Array of classes owning strings deallocated cleanly
# ------------------------------------------------------------------------------
run_test "Test 16: Array of classes owning strings deallocated cleanly (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t16.cco"
class Packet {
    header: string;
    payload_len: int;
}
class Batch {
    name: string;
    packets: Packet[];
}
fn main() -> int {
    let pkts: Packet[] = alloc(Packet, 2);
    pkts[0] = Packet { header: "pkt_0", payload_len: 64 };
    pkts[1] = Packet { header: "pkt_1", payload_len: 128 };
    let b = Batch {
        name: "batch_alpha",
        packets: pkts
    };
    print(f"Batch {b.name}: {b.packets[0].header} and {b.packets[1].header}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t16.bin"
if ! "$CCO" "$TMP_DIR/t16.cco" -o "$TMP_DIR/t16.bin" 2>"$TMP_DIR/t16.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t16.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t16.bin" >/dev/null 2>"$TMP_DIR/t16_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t16_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 17: Container growth via push() of classes owning strings
# ------------------------------------------------------------------------------
run_test "Test 17: Dynamic array push() of string-owning classes across growth (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t17.cco"
class Packet {
    header: string;
}
fn main() -> int {
    let list: Packet[] = list_new(Packet);
    list = push(list, Packet { header: "pkt_push_1" });
    list = push(list, Packet { header: "pkt_push_2" });
    list = push(list, Packet { header: "pkt_push_3" });
    list = push(list, Packet { header: "pkt_push_4" });
    list = push(list, Packet { header: "pkt_push_5" });
    print(f"List len: {len(list)}, [0]: {list[0].header}, [4]: {list[4].header}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t17.bin"
if ! "$CCO" "$TMP_DIR/t17.cco" -o "$TMP_DIR/t17.bin" 2>"$TMP_DIR/t17.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t17.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t17.bin" >/dev/null 2>"$TMP_DIR/t17_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t17_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 18: Negative: Direct member extraction of map field rejected (M5)
# ------------------------------------------------------------------------------
run_test "Test 18: Negative: Direct member extraction of map field rejected"
cat << 'EOF' > "$TMP_DIR/t18.cco"
class Cache {
    entries: map[string]int;
}
fn main() -> int {
    let c = Cache { entries: map_new(string, int) };
    let m = c.entries;
    return 0;
}
EOF
rm -f "$TMP_DIR/t18.bin"
if "$CCO" "$TMP_DIR/t18.cco" -o "$TMP_DIR/t18.bin" 2>"$TMP_DIR/t18.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot move out of class member 'c.entries'" "$TMP_DIR/t18.err"; then
    fail_test "expected 'cannot move out of class member' diagnostic, got: $(cat "$TMP_DIR/t18.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 19: Negative: Use after move into map put() rejected
# ------------------------------------------------------------------------------
run_test "Test 19: Negative: Use after move into map put() rejected"
cat << 'EOF' > "$TMP_DIR/t19.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    let r = Resource { id: 42 };
    m = put(m, "k", r);
    print(r.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t19.bin"
if "$CCO" "$TMP_DIR/t19.cco" -o "$TMP_DIR/t19.bin" 2>"$TMP_DIR/t19.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "use of moved value 'r'" "$TMP_DIR/t19.err"; then
    fail_test "expected 'use of moved value' diagnostic, got: $(cat "$TMP_DIR/t19.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 20: Complex nested stress test combining classes, maps, and enums
# ------------------------------------------------------------------------------
run_test "Test 20: Comprehensive nested stress test under Valgrind (0 leaks, 0 errors)"
cat << 'EOF' > "$TMP_DIR/t20.cco"
enum Payload {
    None,
    Data { text: string },
}
class Node {
    id: int;
    name: string;
    payload: Payload;
}
class Graph {
    label: string;
    nodes: map[string]Node;
}
fn build_graph(label: string) -> Graph {
    let m: map[string]Node = map_new(string, Node);
    m = put(m, "n1", Node { id: 1, name: "Node_1", payload: Payload.Data { text: "alpha_data" } });
    m = put(m, "n2", Node { id: 2, name: "Node_2", payload: Payload.None });
    m = put(m, "n1", Node { id: 10, name: "Node_1_Updated", payload: Payload.Data { text: "beta_data" } });
    return Graph {
        label: label,
        nodes: m
    };
}
fn main() -> int {
    let g = build_graph("main_graph");
    let n: Node = get(g.nodes, "n1");
    match n.payload {
        Payload.None => { print("none"); }
        Payload.Data { text } => { print(f"Graph {g.label}: {n.name} has {text}"); }
    }
    return 0;
}
EOF
rm -f "$TMP_DIR/t20.bin"
if ! "$CCO" "$TMP_DIR/t20.cco" -o "$TMP_DIR/t20.bin" 2>"$TMP_DIR/t20.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t20.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t20.bin" >/dev/null 2>"$TMP_DIR/t20_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t20_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 21: Array push() growth reallocation failure (realloc fails -> clean exit 1)
# ------------------------------------------------------------------------------
run_test "Test 21: Array push() reallocation failure terminates safely with exit code 1"
cat << 'EOF' > "$TMP_DIR/t21.cco"
class Packet {
    id: int;
}
fn main() -> int {
    let list: Packet[] = list_new(Packet);
    list = push(list, Packet { id: 1 });
    list = push(list, Packet { id: 2 });
    list = push(list, Packet { id: 3 });
    list = push(list, Packet { id: 4 });
    list = push(list, Packet { id: 5 });
    return 0;
}
EOF
rm -f "$TMP_DIR/t21.c" "$TMP_DIR/t21.bin"
if ! "$CCO" "$TMP_DIR/t21.cco" -o "$TMP_DIR/t21.c" 2>"$TMP_DIR/t21.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t21.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t21.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t21.bin" -lm 2>"$TMP_DIR/t21_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t21_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=6 "$TMP_DIR/t21.bin" >/dev/null 2>"$TMP_DIR/t21_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on push realloc OOM, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #6: realloc" "$TMP_DIR/t21_run.err"; then
        fail_test "fault injector did not target expected realloc: $(cat "$TMP_DIR/t21_run.err")"
    elif ! grep -q "Memory allocation failed in push()" "$TMP_DIR/t21_run.err"; then
        fail_test "expected OOM message, got: $(cat "$TMP_DIR/t21_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 22: Map rehash / growth allocation failure (calloc fails -> clean exit 1)
# ------------------------------------------------------------------------------
run_test "Test 22: Map rehash allocation failure terminates safely with exit code 1"
cat << 'EOF' > "$TMP_DIR/t22.cco"
fn main() -> int {
    let m: map[int]int = map_new(int, int);
    m = put(m, 1, 10);
    m = put(m, 2, 20);
    m = put(m, 3, 30);
    m = put(m, 4, 40);
    m = put(m, 5, 50);
    m = put(m, 6, 60);
    m = put(m, 7, 70);
    m = put(m, 8, 80);
    m = put(m, 9, 90);
    m = put(m, 10, 100);
    m = put(m, 11, 110);
    return 0;
}
EOF
rm -f "$TMP_DIR/t22.c" "$TMP_DIR/t22.bin"
if ! "$CCO" "$TMP_DIR/t22.cco" -o "$TMP_DIR/t22.c" 2>"$TMP_DIR/t22.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t22.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t22.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t22.bin" -lm 2>"$TMP_DIR/t22_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t22_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=3 "$TMP_DIR/t22.bin" >/dev/null 2>"$TMP_DIR/t22_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on map_rehash OOM, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #3: calloc" "$TMP_DIR/t22_run.err"; then
        fail_test "fault injector did not target expected calloc: $(cat "$TMP_DIR/t22_run.err")"
    elif ! grep -q "Memory allocation failed during map rehash" "$TMP_DIR/t22_run.err"; then
        fail_test "expected OOM message, got: $(cat "$TMP_DIR/t22_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 23: Negative: Passing borrowed parameter to consuming parameter rejected
# ------------------------------------------------------------------------------
run_test "Test 23: Negative: Passing borrowed parameter to consuming parameter rejected"
cat << 'EOF' > "$TMP_DIR/t23.cco"
class Resource {
    id: int;
}
fn consume(r: Resource) -> void {
    print(r.id);
}
fn test(b: &Resource) -> void {
    consume(b);
}
fn main() -> int {
    let r = Resource { id: 10 };
    test(&r);
    return 0;
}
EOF
rm -f "$TMP_DIR/t23.bin"
if "$CCO" "$TMP_DIR/t23.cco" -o "$TMP_DIR/t23.bin" 2>"$TMP_DIR/t23.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot pass borrowed value 'b' to consuming parameter" "$TMP_DIR/t23.err"; then
    fail_test "expected 'cannot pass borrowed value' diagnostic, got: $(cat "$TMP_DIR/t23.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 24: Negative: Passing borrowed map value from get() into put() rejected
# ------------------------------------------------------------------------------
run_test "Test 24: Negative: Passing borrowed value from get() into put() rejected"
cat << 'EOF' > "$TMP_DIR/t24.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "key", Resource { id: 42 });
    m = put(m, "key", get(m, "key"));
    return 0;
}
EOF
rm -f "$TMP_DIR/t24.bin"
if "$CCO" "$TMP_DIR/t24.cco" -o "$TMP_DIR/t24.bin" 2>"$TMP_DIR/t24.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot transfer ownership of borrowed value from get() into map" "$TMP_DIR/t24.err"; then
    fail_test "expected 'cannot transfer ownership' diagnostic, got: $(cat "$TMP_DIR/t24.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 25: Negative: Passing borrowed variable from get() into put() rejected
# ------------------------------------------------------------------------------
run_test "Test 25: Negative: Passing borrowed variable from get() into put() rejected"
cat << 'EOF' > "$TMP_DIR/t25.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 42 });
    let r: Resource = get(m, "k1");
    m = put(m, "k2", r);
    return 0;
}
EOF
rm -f "$TMP_DIR/t25.bin"
if "$CCO" "$TMP_DIR/t25.cco" -o "$TMP_DIR/t25.bin" 2>"$TMP_DIR/t25.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot transfer ownership of borrowed value 'r' into map" "$TMP_DIR/t25.err"; then
    fail_test "expected 'cannot transfer ownership' diagnostic, got: $(cat "$TMP_DIR/t25.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 26: Negative: Passing borrowed reference into class constructor rejected
# ------------------------------------------------------------------------------
run_test "Test 26: Negative: Passing borrowed reference into class field rejected"
cat << 'EOF' > "$TMP_DIR/t26.cco"
class Resource {
    id: int;
}
class Container {
    r: Resource;
}
fn test(b: &Resource) -> void {
    let c = Container { r: b };
}
fn main() -> int {
    return 0;
}
EOF
rm -f "$TMP_DIR/t26.bin"
if "$CCO" "$TMP_DIR/t26.cco" -o "$TMP_DIR/t26.bin" 2>"$TMP_DIR/t26.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot transfer ownership of borrowed value 'b' into class field" "$TMP_DIR/t26.err"; then
    fail_test "expected 'cannot transfer ownership' diagnostic, got: $(cat "$TMP_DIR/t26.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 27: Negative: Passing get() call into class constructor rejected
# ------------------------------------------------------------------------------
run_test "Test 27: Negative: Passing get() call into class field rejected"
cat << 'EOF' > "$TMP_DIR/t27.cco"
class Resource {
    id: int;
}
class Container {
    r: Resource;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k", Resource { id: 1 });
    let c = Container { r: get(m, "k") };
    return 0;
}
EOF
rm -f "$TMP_DIR/t27.bin"
if "$CCO" "$TMP_DIR/t27.cco" -o "$TMP_DIR/t27.bin" 2>"$TMP_DIR/t27.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot transfer ownership of borrowed value from get() into class field" "$TMP_DIR/t27.err"; then
    fail_test "expected 'cannot transfer ownership' diagnostic, got: $(cat "$TMP_DIR/t27.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 28: Runtime map self-replacement defense-in-depth (Valgrind clean)
# ------------------------------------------------------------------------------
run_test "Test 28: Runtime map self-replacement defense-in-depth (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t28.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 101 });
    m = put(m, "k2", Resource { id: 202 });
    let r1: Resource = get(m, "k1");
    print(r1.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t28.c" "$TMP_DIR/t28.bin"
if ! "$CCO" "$TMP_DIR/t28.cco" -o "$TMP_DIR/t28.c" 2>"$TMP_DIR/t28.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t28.err")"
else
    # Insert a direct C-level self-replacement call to test __cco_map_put defense
    sed -i '/int __cco_ret_val = 0;/i m = __cco_map_put(m, (void *)("k1"), (void *)r1, (__cco_val_free_fn)Resource_free);' "$TMP_DIR/t28.c"
    if ! gcc -std=c11 "$TMP_DIR/t28.c" -o "$TMP_DIR/t28.bin" -lm 2>"$TMP_DIR/t28_c.err"; then
        fail_test "gcc failed: $(cat "$TMP_DIR/t28_c.err")"
    elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t28.bin" >/dev/null 2>"$TMP_DIR/t28_val.err"; then
        fail_test "Valgrind error on self-replacement: $(cat "$TMP_DIR/t28_val.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 29: Transitive borrow propagation (let b2 = b) - Valgrind clean
# ------------------------------------------------------------------------------
run_test "Test 29: Transitive borrow propagation (let b2 = b) does not free b2 (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t29.cco"
class Resource {
    id: int;
}
fn print_ref(r: &Resource) -> void {
    print(r.id);
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 999 });
    let b: Resource = get(m, "k1");
    let b2: Resource = b;
    print_ref(b);
    print_ref(b2);
    return 0;
}
EOF
rm -f "$TMP_DIR/t29.c" "$TMP_DIR/t29.bin"
if ! "$CCO" "$TMP_DIR/t29.cco" -o "$TMP_DIR/t29.c" 2>"$TMP_DIR/t29.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t29.err")"
elif ! gcc -std=c11 "$TMP_DIR/t29.c" -o "$TMP_DIR/t29.bin" -lm 2>"$TMP_DIR/t29_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t29_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t29.bin" >/dev/null 2>"$TMP_DIR/t29_val.err"; then
    fail_test "Valgrind detected error in transitive borrow: $(cat "$TMP_DIR/t29_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 30: Negative: Transitive borrow b2 cannot pass to consuming parameter
# ------------------------------------------------------------------------------
run_test "Test 30: Negative: Transitive borrow b2 cannot pass to consuming parameter"
cat << 'EOF' > "$TMP_DIR/t30.cco"
class Resource {
    id: int;
}
fn consume(r: Resource) -> void {
    print(r.id);
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 10 });
    let b: Resource = get(m, "k1");
    let b2: Resource = b;
    consume(b2);
    return 0;
}
EOF
rm -f "$TMP_DIR/t30.bin"
if "$CCO" "$TMP_DIR/t30.cco" -o "$TMP_DIR/t30.bin" 2>"$TMP_DIR/t30.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot pass borrowed value 'b2' to consuming parameter" "$TMP_DIR/t30.err"; then
    fail_test "expected 'cannot pass borrowed value' diagnostic, got: $(cat "$TMP_DIR/t30.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 31: Negative: Transitive borrow b2 cannot enter map put()
# ------------------------------------------------------------------------------
run_test "Test 31: Negative: Transitive borrow b2 cannot enter map put()"
cat << 'EOF' > "$TMP_DIR/t31.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 10 });
    let b: Resource = get(m, "k1");
    let b2: Resource = b;
    m = put(m, "k2", b2);
    return 0;
}
EOF
rm -f "$TMP_DIR/t31.bin"
if "$CCO" "$TMP_DIR/t31.cco" -o "$TMP_DIR/t31.bin" 2>"$TMP_DIR/t31.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot transfer ownership of borrowed value 'b2' into map" "$TMP_DIR/t31.err"; then
    fail_test "expected 'cannot transfer ownership' diagnostic, got: $(cat "$TMP_DIR/t31.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 32: Negative: Transitive borrow b2 cannot enter class field (NODE_NEW)
# ------------------------------------------------------------------------------
run_test "Test 32: Negative: Transitive borrow b2 cannot enter class field"
cat << 'EOF' > "$TMP_DIR/t32.cco"
class Resource {
    id: int;
}
class Box {
    res: Resource;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 10 });
    let b: Resource = get(m, "k1");
    let b2: Resource = b;
    let bx = Box { res: b2 };
    return 0;
}
EOF
rm -f "$TMP_DIR/t32.bin"
if "$CCO" "$TMP_DIR/t32.cco" -o "$TMP_DIR/t32.bin" 2>"$TMP_DIR/t32.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot transfer ownership of borrowed value 'b2' into class field" "$TMP_DIR/t32.err"; then
    fail_test "expected 'cannot transfer ownership' diagnostic, got: $(cat "$TMP_DIR/t32.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 33: Negative: Transitive borrow b2 cannot be returned
# ------------------------------------------------------------------------------
run_test "Test 33: Negative: Transitive borrow b2 cannot be returned"
cat << 'EOF' > "$TMP_DIR/t33.cco"
class Resource {
    id: int;
}
fn escape() -> Resource {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 10 });
    let b: Resource = get(m, "k1");
    let b2: Resource = b;
    return b2;
}
fn main() -> int {
    let r = escape();
    return 0;
}
EOF
rm -f "$TMP_DIR/t33.bin"
if "$CCO" "$TMP_DIR/t33.cco" -o "$TMP_DIR/t33.bin" 2>"$TMP_DIR/t33.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot return borrowed value 'b2'" "$TMP_DIR/t33.err"; then
    fail_test "expected 'cannot return borrowed value' diagnostic, got: $(cat "$TMP_DIR/t33.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 34: Negative: Assigning borrowed value to owned variable rejected
# ------------------------------------------------------------------------------
run_test "Test 34: Negative: Assigning borrowed value to owned variable rejected"
cat << 'EOF' > "$TMP_DIR/t34.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 10 });
    let owned = Resource { id: 20 };
    let b: Resource = get(m, "k1");
    owned = b;
    return 0;
}
EOF
rm -f "$TMP_DIR/t34.bin"
if "$CCO" "$TMP_DIR/t34.cco" -o "$TMP_DIR/t34.bin" 2>"$TMP_DIR/t34.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot assign borrowed value to owned variable 'owned'" "$TMP_DIR/t34.err"; then
    fail_test "expected 'cannot assign borrowed value' diagnostic, got: $(cat "$TMP_DIR/t34.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 35: Multi-level helper borrow chain (&Resource) - Valgrind clean
# ------------------------------------------------------------------------------
run_test "Test 35: Multi-level helper borrow chain (&Resource) (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t35.cco"
class Resource {
    id: int;
}
fn h3(r: &Resource) -> int {
    return r.id * 2;
}
fn h2(r: &Resource) -> int {
    return h3(r) + 5;
}
fn h1(r: &Resource) -> int {
    return h2(r) + 1;
}
fn main() -> int {
    let r = Resource { id: 20 };
    let val = h1(r);
    print(val);
    return 0;
}
EOF
rm -f "$TMP_DIR/t35.c" "$TMP_DIR/t35.bin"
if ! "$CCO" "$TMP_DIR/t35.cco" -o "$TMP_DIR/t35.c" 2>"$TMP_DIR/t35.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t35.err")"
elif ! gcc -std=c11 "$TMP_DIR/t35.c" -o "$TMP_DIR/t35.bin" -lm 2>"$TMP_DIR/t35_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t35_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t35.bin" >/dev/null 2>"$TMP_DIR/t35_val.err"; then
    fail_test "Valgrind detected error in helper borrow chain: $(cat "$TMP_DIR/t35_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 36: Negative: Helper chain passing borrowed parameter to consuming sink
# ------------------------------------------------------------------------------
run_test "Test 36: Negative: Helper chain passing borrowed parameter to consuming sink"
cat << 'EOF' > "$TMP_DIR/t36.cco"
class Resource {
    id: int;
}
fn sink(r: Resource) -> void {
    print(r.id);
}
fn helper(r: &Resource) -> void {
    sink(r);
}
fn main() -> int {
    let r = Resource { id: 1 };
    helper(r);
    return 0;
}
EOF
rm -f "$TMP_DIR/t36.bin"
if "$CCO" "$TMP_DIR/t36.cco" -o "$TMP_DIR/t36.bin" 2>"$TMP_DIR/t36.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot pass borrowed value 'r' to consuming parameter" "$TMP_DIR/t36.err"; then
    fail_test "expected 'cannot pass borrowed value' diagnostic, got: $(cat "$TMP_DIR/t36.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 37: Map remove() followed by put() transfer across keys (Valgrind clean)
# ------------------------------------------------------------------------------
run_test "Test 37: Map remove() followed by put() transfer across keys (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t37.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "src_key", Resource { id: 777 });
    let r: Resource = remove(m, "src_key");
    m = put(m, "dst_key", r);
    let check: Resource = get(m, "dst_key");
    print(check.id);
    return 0;
}
EOF
rm -f "$TMP_DIR/t37.c" "$TMP_DIR/t37.bin"
if ! "$CCO" "$TMP_DIR/t37.cco" -o "$TMP_DIR/t37.c" 2>"$TMP_DIR/t37.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t37.err")"
elif ! gcc -std=c11 "$TMP_DIR/t37.c" -o "$TMP_DIR/t37.bin" -lm 2>"$TMP_DIR/t37_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t37_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t37.bin" >/dev/null 2>"$TMP_DIR/t37_val.err"; then
    fail_test "Valgrind error in remove-put transfer: $(cat "$TMP_DIR/t37_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 38: Multi-field class constructor partial initialization unwinding
# ------------------------------------------------------------------------------
run_test "Test 38: Multi-field class constructor partial initialization unwinding"
cat << 'EOF' > "$TMP_DIR/t38.cco"
class Triple {
    s1: string;
    s2: string;
    s3: string;
}
fn main() -> int {
    let t = Triple { s1: "first", s2: "second", s3: "third" };
    print(t.s1);
    return 0;
}
EOF
rm -f "$TMP_DIR/t38.c" "$TMP_DIR/t38.bin"
if ! "$CCO" "$TMP_DIR/t38.cco" -o "$TMP_DIR/t38.c" 2>"$TMP_DIR/t38.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t38.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t38.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t38.bin" -lm 2>"$TMP_DIR/t38_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t38_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=4 "$TMP_DIR/t38.bin" >/dev/null 2>"$TMP_DIR/t38_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on strdup failure, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #4: strdup(\"third\")" "$TMP_DIR/t38_run.err"; then
        fail_test "fault injector did not target expected strdup: $(cat "$TMP_DIR/t38_run.err")"
    elif ! grep -q "Memory allocation failed in strdup() for Triple.s3" "$TMP_DIR/t38_run.err"; then
        fail_test "expected OOM message for Triple.s3, got: $(cat "$TMP_DIR/t38_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 39: Negative: Prohibit transferring borrowed element into dynamic array
# ------------------------------------------------------------------------------
run_test "Test 39: Negative: Prohibit transferring borrowed element into dynamic array"
cat << 'EOF' > "$TMP_DIR/t39.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 10 });
    let b: Resource = get(m, "k1");
    let list: Resource[] = list_new(Resource);
    list = push(list, b);
    return 0;
}
EOF
rm -f "$TMP_DIR/t39.bin"
if "$CCO" "$TMP_DIR/t39.cco" -o "$TMP_DIR/t39.bin" 2>"$TMP_DIR/t39.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot transfer ownership of borrowed value 'b' into array" "$TMP_DIR/t39.err"; then
    fail_test "expected 'cannot transfer ownership' diagnostic, got: $(cat "$TMP_DIR/t39.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 40: Negative: Reject assigning owned resource to borrowed variable (DEF-M8-01)
# ------------------------------------------------------------------------------
run_test "Test 40: Negative: Reject assigning owned resource to borrowed variable (DEF-M8-01)"
cat << 'EOF' > "$TMP_DIR/t40.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 1 });
    let b: Resource = get(m, "k1");
    let r = Resource { id: 10 };
    b = r;
    return 0;
}
EOF
rm -f "$TMP_DIR/t40.bin"
if "$CCO" "$TMP_DIR/t40.cco" -o "$TMP_DIR/t40.bin" 2>"$TMP_DIR/t40.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot assign owned value to borrowed variable 'b'" "$TMP_DIR/t40.err"; then
    fail_test "expected 'cannot assign owned value to borrowed variable' diagnostic, got: $(cat "$TMP_DIR/t40.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 41: Negative: Reject assigning owned resource to borrowed variable in branch (DEF-M8-01)
# ------------------------------------------------------------------------------
run_test "Test 41: Negative: Reject assigning owned resource to borrow in branch"
cat << 'EOF' > "$TMP_DIR/t41.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 1 });
    let b: Resource = get(m, "k1");
    if true {
        let r = Resource { id: 20 };
        b = r;
    }
    return 0;
}
EOF
rm -f "$TMP_DIR/t41.bin"
if "$CCO" "$TMP_DIR/t41.cco" -o "$TMP_DIR/t41.bin" 2>"$TMP_DIR/t41.err"; then
    fail_test "expected compilation failure, but succeeded"
elif ! grep -q "cannot assign owned value to borrowed variable 'b'" "$TMP_DIR/t41.err"; then
    fail_test "expected 'cannot assign owned value to borrowed variable' diagnostic, got: $(cat "$TMP_DIR/t41.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 42: Map of enum values cleaned up cleanly under Valgrind (DEF-M8-02)
# ------------------------------------------------------------------------------
run_test "Test 42: Map of enum values cleaned up cleanly under Valgrind (DEF-M8-02)"
cat << 'EOF' > "$TMP_DIR/t42.cco"
enum Payload {
    Empty,
    Text { msg: string },
}
fn main() -> int {
    let m: map[string]Payload = map_new(string, Payload);
    m = put(m, "k1", Payload.Text { msg: "hello_enum" });
    m = put(m, "k2", Payload.Empty);
    return 0;
}
EOF
rm -f "$TMP_DIR/t42.c" "$TMP_DIR/t42.bin"
if ! "$CCO" "$TMP_DIR/t42.cco" -o "$TMP_DIR/t42.c" 2>"$TMP_DIR/t42.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t42.err")"
elif ! gcc -std=c11 "$TMP_DIR/t42.c" -o "$TMP_DIR/t42.bin" -lm 2>"$TMP_DIR/t42_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t42_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t42.bin" >/dev/null 2>"$TMP_DIR/t42_val.err"; then
    fail_test "Valgrind detected leak: $(cat "$TMP_DIR/t42_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 43: Map enum replacement frees overwritten value cleanly (DEF-M8-02)
# ------------------------------------------------------------------------------
run_test "Test 43: Map enum replacement frees overwritten value cleanly (DEF-M8-02)"
cat << 'EOF' > "$TMP_DIR/t43.cco"
enum Payload {
    Empty,
    Text { msg: string },
}
fn main() -> int {
    let m: map[string]Payload = map_new(string, Payload);
    m = put(m, "k", Payload.Text { msg: "initial_string" });
    m = put(m, "k", Payload.Text { msg: "replacement_string" });
    return 0;
}
EOF
rm -f "$TMP_DIR/t43.c" "$TMP_DIR/t43.bin"
if ! "$CCO" "$TMP_DIR/t43.cco" -o "$TMP_DIR/t43.c" 2>"$TMP_DIR/t43.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t43.err")"
elif ! gcc -std=c11 "$TMP_DIR/t43.c" -o "$TMP_DIR/t43.bin" -lm 2>"$TMP_DIR/t43_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t43_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t43.bin" >/dev/null 2>"$TMP_DIR/t43_val.err"; then
    fail_test "Valgrind detected leak on replacement: $(cat "$TMP_DIR/t43_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 44: Map enum remove() and destruction under Valgrind (DEF-M8-02)
# ------------------------------------------------------------------------------
run_test "Test 44: Map enum remove() transfer and clean destruction (DEF-M8-02)"
cat << 'EOF' > "$TMP_DIR/t44.cco"
enum Payload {
    Empty,
    Text { msg: string },
}
fn main() -> int {
    let m: map[string]Payload = map_new(string, Payload);
    m = put(m, "k1", Payload.Text { msg: "extracted_payload" });
    m = put(m, "k2", Payload.Empty);
    let p: Payload = remove(m, "k1");
    return 0;
}
EOF
rm -f "$TMP_DIR/t44.c" "$TMP_DIR/t44.bin"
if ! "$CCO" "$TMP_DIR/t44.cco" -o "$TMP_DIR/t44.c" 2>"$TMP_DIR/t44.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t44.err")"
elif ! gcc -std=c11 "$TMP_DIR/t44.c" -o "$TMP_DIR/t44.bin" -lm 2>"$TMP_DIR/t44_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t44_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t44.bin" >/dev/null 2>"$TMP_DIR/t44_val.err"; then
    fail_test "Valgrind detected leak on remove: $(cat "$TMP_DIR/t44_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 45: Phase C: Map key allocation failure (strdup) terminates safely (exit 1)
# ------------------------------------------------------------------------------
run_test "Test 45: Map key strdup failure terminates safely with exit code 1"
cat << 'EOF' > "$TMP_DIR/t45.cco"
class Item {
    val: int;
}
fn main() -> int {
    let m: map[string]Item = map_new(string, Item);
    m = put(m, "alpha_key", Item { val: 42 });
    return 0;
}
EOF
rm -f "$TMP_DIR/t45.c" "$TMP_DIR/t45.bin"
if ! "$CCO" "$TMP_DIR/t45.cco" -o "$TMP_DIR/t45.c" 2>"$TMP_DIR/t45.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t45.err")"
elif ! gcc -std=c11 "$TMP_DIR/fault_injector.c" "$TMP_DIR/t45.c" -Wl,--wrap=calloc -Wl,--wrap=strdup -Wl,--wrap=malloc -Wl,--wrap=realloc -o "$TMP_DIR/t45.bin" -lm 2>"$TMP_DIR/t45_c.err"; then
    fail_test "gcc linking failed: $(cat "$TMP_DIR/t45_c.err")"
else
    set +e
    CCO_FAIL_ALLOC_AT=4 "$TMP_DIR/t45.bin" >/dev/null 2>"$TMP_DIR/t45_run.err"
    exit_code=$?
    set -e
    if [ "$exit_code" -ne 1 ]; then
        fail_test "expected exit code 1 on strdup key failure, got $exit_code"
    elif ! grep -q "\[FAULT INJECTOR\] Injected failure at alloc #4: strdup(\"alpha_key\")" "$TMP_DIR/t45_run.err"; then
        fail_test "fault injector did not target expected strdup: $(cat "$TMP_DIR/t45_run.err")"
    elif ! grep -q "Memory allocation failed in strdup key" "$TMP_DIR/t45_run.err"; then
        fail_test "expected OOM message for strdup key, got: $(cat "$TMP_DIR/t45_run.err")"
    else
        pass_test
    fi
fi

# ------------------------------------------------------------------------------
# Test 46: Phase D: Complex control flow: early return unwinding under Valgrind
# ------------------------------------------------------------------------------
run_test "Test 46: Complex control flow with early return unwinding (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t46.cco"
class Resource {
    id: int;
}
fn compute(flag: bool) -> int {
    let r1 = Resource { id: 100 };
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "held", Resource { id: 200 });
    let b: Resource = get(m, "held");
    if flag {
        let early_res = Resource { id: 300 };
        return b.id + early_res.id;
    }
    let r2 = Resource { id: 400 };
    return r1.id + r2.id;
}
fn main() -> int {
    let a = compute(true);
    let b = compute(false);
    return 0;
}
EOF
rm -f "$TMP_DIR/t46.c" "$TMP_DIR/t46.bin"
if ! "$CCO" "$TMP_DIR/t46.cco" -o "$TMP_DIR/t46.c" 2>"$TMP_DIR/t46.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t46.err")"
elif ! gcc -std=c11 "$TMP_DIR/t46.c" -o "$TMP_DIR/t46.bin" -lm 2>"$TMP_DIR/t46_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t46_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t46.bin" >/dev/null 2>"$TMP_DIR/t46_val.err"; then
    fail_test "Valgrind detected leak on complex early return: $(cat "$TMP_DIR/t46_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 47: DEF-M9-01: Local dynamic array of enums with string payload (Valgrind clean)
# ------------------------------------------------------------------------------
run_test "Test 47: Local dynamic array of enums with string payload (DEF-M9-01, Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t47.cco"
enum Payload {
    Text { msg: string },
    Empty,
}
fn test_lifecycle() -> void {
    let empty_list: Payload[] = list_new(Payload);
    let list: Payload[] = list_new(Payload);
    list = push(list, Payload.Text { msg: "first_variant" });
    list = push(list, Payload.Empty);
    list = push(list, Payload.Text { msg: "second_variant" });
    list = push(list, Payload.Text { msg: "third_variant" });
}
fn main() -> int {
    test_lifecycle();
    return 0;
}
EOF
rm -f "$TMP_DIR/t47.c" "$TMP_DIR/t47.bin"
if ! "$CCO" "$TMP_DIR/t47.cco" -o "$TMP_DIR/t47.c" 2>"$TMP_DIR/t47.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t47.err")"
elif ! gcc -std=c11 "$TMP_DIR/t47.c" -o "$TMP_DIR/t47.bin" -lm 2>"$TMP_DIR/t47_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t47_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t47.bin" >/dev/null 2>"$TMP_DIR/t47_val.err"; then
    fail_test "Valgrind detected leak or error in enum dynamic array lifecycle: $(cat "$TMP_DIR/t47_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 48: Array member reassignment and container class destructor cleanup (Valgrind clean)
# ------------------------------------------------------------------------------
run_test "Test 48: Array member reassignment and container class destructor cleanup (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t48.cco"
enum Payload {
    Text { msg: string }
}
class Container {
    items: Payload[];
}
fn test_member_reassign() -> void {
    let initial: Payload[] = list_new(Payload);
    initial = push(initial, Payload.Text { msg: "old_item" });
    let c = Container { items: initial };
    let replacement: Payload[] = list_new(Payload);
    replacement = push(replacement, Payload.Text { msg: "new_item1" });
    replacement = push(replacement, Payload.Text { msg: "new_item2" });
    c.items = replacement;
}
fn main() -> int {
    test_member_reassign();
    return 0;
}
EOF
rm -f "$TMP_DIR/t48.c" "$TMP_DIR/t48.bin"
if ! "$CCO" "$TMP_DIR/t48.cco" -o "$TMP_DIR/t48.c" 2>"$TMP_DIR/t48.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t48.err")"
elif ! gcc -std=c11 "$TMP_DIR/t48.c" -o "$TMP_DIR/t48.bin" -lm 2>"$TMP_DIR/t48_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t48_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t48.bin" >/dev/null 2>"$TMP_DIR/t48_val.err"; then
    fail_test "Valgrind detected leak in array member reassignment: $(cat "$TMP_DIR/t48_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 49: Dynamic array cleanup on early return unwinding (Valgrind clean)
# ------------------------------------------------------------------------------
run_test "Test 49: Dynamic array cleanup on early return unwinding (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t49.cco"
enum Payload {
    Text { msg: string }
}
class Resource {
    name: string;
}
fn early_exit(take_branch: bool) -> int {
    let elist: Payload[] = list_new(Payload);
    elist = push(elist, Payload.Text { msg: "early_branch_enum" });
    let rlist: Resource[] = list_new(Resource);
    rlist = push(rlist, Resource { name: "early_branch_resource" });
    if take_branch {
        return 42;
    }
    elist = push(elist, Payload.Text { msg: "late_branch_enum" });
    rlist = push(rlist, Resource { name: "late_branch_resource" });
    return 100;
}
fn main() -> int {
    let a = early_exit(true);
    let b = early_exit(false);
    return 0;
}
EOF
rm -f "$TMP_DIR/t49.c" "$TMP_DIR/t49.bin"
if ! "$CCO" "$TMP_DIR/t49.cco" -o "$TMP_DIR/t49.c" 2>"$TMP_DIR/t49.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t49.err")"
elif ! gcc -std=c11 "$TMP_DIR/t49.c" -o "$TMP_DIR/t49.bin" -lm 2>"$TMP_DIR/t49_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t49_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t49.bin" >/dev/null 2>"$TMP_DIR/t49_val.err"; then
    fail_test "Valgrind detected leak on array early return unwinding: $(cat "$TMP_DIR/t49_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 50: Local dynamic array of strings lifecycle and growth (Valgrind clean)
# ------------------------------------------------------------------------------
run_test "Test 50: Local dynamic array of strings lifecycle and growth (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t50.cco"
fn test_string_array() -> void {
    let empty_str_list: string[] = list_new(string);
    let slist: string[] = list_new(string);
    slist = push(slist, "first_str");
    slist = push(slist, "second_str");
    slist = push(slist, "third_str");
    slist = push(slist, "fourth_str");
    slist = push(slist, "fifth_str");
}
fn main() -> int {
    test_string_array();
    return 0;
}
EOF
rm -f "$TMP_DIR/t50.c" "$TMP_DIR/t50.bin"
if ! "$CCO" "$TMP_DIR/t50.cco" -o "$TMP_DIR/t50.c" 2>"$TMP_DIR/t50.err"; then
    fail_test "compilation to C failed: $(cat "$TMP_DIR/t50.err")"
elif ! gcc -std=c11 "$TMP_DIR/t50.c" -o "$TMP_DIR/t50.bin" -lm 2>"$TMP_DIR/t50_c.err"; then
    fail_test "gcc build failed: $(cat "$TMP_DIR/t50_c.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t50.bin" >/dev/null 2>"$TMP_DIR/t50_val.err"; then
    fail_test "Valgrind detected leak in string array lifecycle: $(cat "$TMP_DIR/t50_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 51: Negative: Reject unsupported nested map types (Phase F)
# ------------------------------------------------------------------------------
run_test "Test 51: Negative: Reject unsupported nested map types (Phase F)"
cat << 'EOF' > "$TMP_DIR/t51.cco"
class Resource {
    id: int;
}
fn main() -> int {
    let m: map[string]map[string]Resource = map_new(string, Resource);
    return 0;
}
EOF
set +e
"$CCO" "$TMP_DIR/t51.cco" -o "$TMP_DIR/t51.c" 2>"$TMP_DIR/t51.err"
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "expected compilation failure for nested map, but compilation succeeded"
elif ! grep -E -q "nested map types are not supported|type mismatch" "$TMP_DIR/t51.err"; then
    fail_test "expected diagnostic 'nested map types are not supported' or 'type mismatch', got: $(cat "$TMP_DIR/t51.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 52: Negative: Reject unsupported map of array types (Phase F)
# ------------------------------------------------------------------------------
run_test "Test 52: Negative: Reject unsupported map of array types (Phase F)"
cat << 'EOF' > "$TMP_DIR/t52.cco"
fn main() -> int {
    let m: map[string]int[] = map_new(string, int);
    return 0;
}
EOF
set +e
"$CCO" "$TMP_DIR/t52.cco" -o "$TMP_DIR/t52.c" 2>"$TMP_DIR/t52.err"
exit_code=$?
set -e
if [ "$exit_code" -eq 0 ]; then
    fail_test "expected compilation failure for map of arrays, but compilation succeeded"
elif ! grep -E -q "nested container types are not supported|type mismatch" "$TMP_DIR/t52.err"; then
    fail_test "expected diagnostic 'nested container types are not supported' or 'type mismatch', got: $(cat "$TMP_DIR/t52.err")"
else
    pass_test
fi

echo "=============================================================="
echo "Adversarial Memory-Safety Suite: $passed_tests / $total_tests Passed"
echo "=============================================================="

if [ "$failed_tests" -gt 0 ]; then
    exit 1
fi
exit 0
