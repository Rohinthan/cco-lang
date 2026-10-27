#!/usr/bin/env bash
# ==============================================================================
# tests/test_ownership_string_field_lifecycle.sh
# Cco Compiler Ownership Analysis: String Field Lifecycle & Memory Safety Suite
# ==============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCO="$REPO_ROOT/cco"
TMP_DIR=$(mktemp -d /tmp/cco_test_str_lifecycle_XXXXXX)

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

echo "=============================================================="
echo "Cco: Ownership String Field Lifecycle & Memory Safety Suite"
echo "=============================================================="

# ------------------------------------------------------------------------------
# Test 1: Class field initialized from string literal
# ------------------------------------------------------------------------------
run_test "Test 1: Class field initialized from string literal (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t1.cco"
class Buffer {
    size: int;
    name: string;
}
fn main() -> int {
    let b = Buffer { size: 1024, name: "static_literal_name" };
    print(b.name);
    return 0;
}
EOF
rm -f "$TMP_DIR/t1.bin"
if ! "$CCO" "$TMP_DIR/t1.cco" -o "$TMP_DIR/t1.bin" 2>"$TMP_DIR/t1.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t1.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t1.bin" >/dev/null 2>"$TMP_DIR/t1_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t1_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 2: Class field initialized from heap-backed string
# ------------------------------------------------------------------------------
run_test "Test 2: Class field initialized from heap-backed string (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t2.cco"
class Buffer {
    size: int;
    name: string;
}
fn main() -> int {
    let s = concat("heap_", "string");
    let b = Buffer { size: 2048, name: s };
    print(b.name);
    return 0;
}
EOF
rm -f "$TMP_DIR/t2.bin"
if ! "$CCO" "$TMP_DIR/t2.cco" -o "$TMP_DIR/t2.bin" 2>"$TMP_DIR/t2.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t2.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t2.bin" >/dev/null 2>"$TMP_DIR/t2_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t2_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 3: Owned string passed through constructor
# ------------------------------------------------------------------------------
run_test "Test 3: Owned string passed through constructor (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t3.cco"
class Item {
    id: int;
    tag: string;
}
fn make_item(t: string, id: int) -> Item {
    return Item { id: id, tag: t };
}
fn main() -> int {
    let tag = "sample_tag";
    let it = make_item(tag, 42);
    print(it.tag);
    print(tag);
    return 0;
}
EOF
rm -f "$TMP_DIR/t3.bin"
if ! "$CCO" "$TMP_DIR/t3.cco" -o "$TMP_DIR/t3.bin" 2>"$TMP_DIR/t3.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t3.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t3.bin" >/dev/null 2>"$TMP_DIR/t3_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t3_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 4: Function returning a class
# ------------------------------------------------------------------------------
run_test "Test 4: Function returning a class containing a string (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t4.cco"
class User {
    name: string;
    age: int;
}
fn create_user(name: string, age: int) -> User {
    return User { name: name, age: age };
}
fn main() -> int {
    let u = create_user("Alice", 30);
    print(u.name);
    return 0;
}
EOF
rm -f "$TMP_DIR/t4.bin"
if ! "$CCO" "$TMP_DIR/t4.cco" -o "$TMP_DIR/t4.bin" 2>"$TMP_DIR/t4.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t4.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t4.bin" >/dev/null 2>"$TMP_DIR/t4_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t4_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 5: Class return with the original field pointer (resize scenario)
# ------------------------------------------------------------------------------
run_test "Test 5: Class return transferring original field value (resize) (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t5.cco"
class Buffer {
    size: int;
    name: string;
}
fn resize(b: Buffer, new_size: int) -> Buffer {
    return Buffer {
        size: new_size,
        name: b.name
    };
}
fn main() -> int {
    let b1 = Buffer { size: 100, name: "resizable_buf" };
    let b2 = resize(b1, 200);
    print(b2.name);
    print(b2.size);
    return 0;
}
EOF
rm -f "$TMP_DIR/t5.bin"
if ! "$CCO" "$TMP_DIR/t5.cco" -o "$TMP_DIR/t5.bin" 2>"$TMP_DIR/t5.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t5.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t5.bin" >/dev/null 2>"$TMP_DIR/t5_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t5_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 6: Class return with a newly produced string
# ------------------------------------------------------------------------------
run_test "Test 6: Class return with a newly produced string (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t6.cco"
class Buffer {
    size: int;
    name: string;
}
fn rename_buffer(b: Buffer, suffix: string) -> Buffer {
    let new_name = concat(b.name, suffix);
    return Buffer {
        size: b.size,
        name: new_name
    };
}
fn main() -> int {
    let b1 = Buffer { size: 512, name: "base" };
    let b2 = rename_buffer(b1, "_extended");
    print(b2.name);
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
# Test 7: Repeated borrowing
# ------------------------------------------------------------------------------
run_test "Test 7: Repeated borrowing of class with string field (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t7.cco"
class Buffer {
    size: int;
    name: string;
}
fn inspect(b: &Buffer) -> void {
    print(b.name);
    print(b.size);
}
fn main() -> int {
    let buf = Buffer { size: 4096, name: "main_buf" };
    inspect(&buf);
    inspect(&buf);
    inspect(&buf);
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
# Test 8: Final consumption
# ------------------------------------------------------------------------------
run_test "Test 8: Final consumption of class with string field (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t8.cco"
class Buffer {
    size: int;
    name: string;
}
fn consume(b: Buffer) -> void {
    print(b.name);
}
fn main() -> int {
    let buf = Buffer { size: 1024, name: "consumed_buf" };
    consume(buf);
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
# Test 9: Two objects with distinct owned strings
# ------------------------------------------------------------------------------
run_test "Test 9: Two objects with distinct owned strings (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t9.cco"
class Pair {
    first: string;
    second: string;
}
fn main() -> int {
    let p = Pair { first: "hello", second: "world" };
    print(p.first);
    print(p.second);
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
# Test 10: Shared-pointer/alias scenario
# ------------------------------------------------------------------------------
run_test "Test 10: Construction from shared string source produces independent copies"
cat << 'EOF' > "$TMP_DIR/t10.cco"
class Item {
    name: string;
}
fn main() -> int {
    let shared_source = "shared_name";
    let a = Item { name: shared_source };
    let b = Item { name: shared_source };
    print(a.name);
    print(b.name);
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
# Test 11: Replacement and self-assignment
# ------------------------------------------------------------------------------
run_test "Test 11: Member replacement, self-assignment, and local string transfer"
cat << 'EOF' > "$TMP_DIR/t11.cco"
class Item {
    name: string;
}
fn main() -> int {
    let c = Item { name: "initial" };
    // 11a: self-assignment
    c.name = c.name;
    print(c.name);
    // 11b: fresh value replacement
    c.name = "replaced";
    print(c.name);
    // 11c: local string transfer
    let local_s = concat("local_", "value");
    c.name = local_s;
    print(c.name);
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
# Test 12: Borrowed class fields or parameters
# ------------------------------------------------------------------------------
run_test "Test 12: Borrowed class fields passed to print and functions (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t12.cco"
class Document {
    title: string;
    body: string;
}
fn print_title(t: string) -> void {
    print(t);
}
fn main() -> int {
    let doc = Document { title: "Draft", body: "Content" };
    print_title(doc.title);
    print(doc.body);
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
# Test 13: Existing ownership protections (M5 linear moves rejected)
# ------------------------------------------------------------------------------
run_test "Test 13: Linear resource member-move extraction restrictions preserved"
cat << 'EOF' > "$TMP_DIR/t13.cco"
class Resource { id: int; }
class Container { res: Resource; }
fn main() -> int {
    let c = Container { res: Resource { id: 1 } };
    let extracted = c.res;
    return 0;
}
EOF
rm -f "$TMP_DIR/t13.bin"
if "$CCO" "$TMP_DIR/t13.cco" -o "$TMP_DIR/t13.bin" 2>"$TMP_DIR/t13.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t13.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'c.res'" "$TMP_DIR/t13.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t13.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 14: Negative cases (consuming parameter with linear member rejected)
# ------------------------------------------------------------------------------
run_test "Test 14: Negative: Passing linear member to consuming parameter rejected"
cat << 'EOF' > "$TMP_DIR/t14.cco"
class Resource { id: int; }
class Box { r: Resource; }
fn take_resource(r: Resource) -> void {
    print(r.id);
}
fn main() -> int {
    let b = Box { r: Resource { id: 99 } };
    take_resource(b.r);
    return 0;
}
EOF
rm -f "$TMP_DIR/t14.bin"
if "$CCO" "$TMP_DIR/t14.cco" -o "$TMP_DIR/t14.bin" 2>"$TMP_DIR/t14.err"; then
    fail_test "compilation succeeded but should have failed"
elif [ -f "$TMP_DIR/t14.bin" ]; then
    fail_test "output binary produced for invalid program"
elif ! grep -q "cannot move out of class member 'b.r'" "$TMP_DIR/t14.err"; then
    fail_test "expected diagnostic not found in: $(cat "$TMP_DIR/t14.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 15: Enum variant constructor with string literal and pattern matching
# ------------------------------------------------------------------------------
run_test "Test 15: Enum variant with string payload (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t15.cco"
enum Message {
    Quit,
    Text { content: string },
}
fn handle(m: Message) -> void {
    match m {
        Message.Quit => { print("quit"); }
        Message.Text { content } => { print(content); }
    }
}
fn main() -> int {
    let m = Message.Text { content: "enum_string_payload" };
    handle(m);
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
# Test 16: Complete user reproducer end-to-end
# ------------------------------------------------------------------------------
run_test "Test 16: Original demonstration program end-to-end (Valgrind clean)"
cat << 'EOF' > "$TMP_DIR/t16.cco"
class Buffer {
    size: int;
    name: string;
}

fn create_buffer(name: string, size: int) -> Buffer {
    return Buffer {
        size: size,
        name: name
    };
}

fn inspect(b: &Buffer) -> void {
    print(f"Inspecting '{b.name}' -> size = {b.size}");
}

fn consume(b: Buffer) -> void {
    print(f"Consuming '{b.name}' (size: {b.size})");
}

fn resize(b: Buffer, new_size: int) -> Buffer {
    print(f"Resizing '{b.name}' from {b.size} -> {new_size}");

    return Buffer {
        size: new_size,
        name: b.name
    };
}

fn main() -> int {
    let buf = create_buffer("main_buffer", 4096);

    inspect(&buf);
    inspect(&buf);

    let buf2 = resize(buf, 8192);

    inspect(&buf2);

    consume(buf2);
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
# Test 17: String member self-assignment (b.name = b.name)
# ------------------------------------------------------------------------------
run_test "Test 17: String member self-assignment does not double-free or leak"
cat << 'EOF' > "$TMP_DIR/t17.cco"
class Buffer {
    name: string;
}

fn main() -> int {
    let b = Buffer { name: "initial_string" };
    b.name = b.name;
    b.name = b.name;
    print(f"Self-assigned name: {b.name}");
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
# Test 18: String replacement from another object's field (b1.name = b2.name)
# ------------------------------------------------------------------------------
run_test "Test 18: String replacement from another object's field safely deep-copies"
cat << 'EOF' > "$TMP_DIR/t18.cco"
class Buffer {
    name: string;
}

fn main() -> int {
    let b1 = Buffer { name: "alpha" };
    let b2 = Buffer { name: "beta" };
    b1.name = b2.name;
    print(f"b1: {b1.name}, b2: {b2.name}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t18.bin"
if ! "$CCO" "$TMP_DIR/t18.cco" -o "$TMP_DIR/t18.bin" 2>"$TMP_DIR/t18.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t18.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t18.bin" >/dev/null 2>"$TMP_DIR/t18_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t18_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 19: Repeated construction from same local string binding
# ------------------------------------------------------------------------------
run_test "Test 19: Multiple objects constructed from same string retain independent ownership"
cat << 'EOF' > "$TMP_DIR/t19.cco"
class Item {
    name: string;
    id: int;
}

fn main() -> int {
    let base = concat("base_", "item");
    let it1 = Item { name: base, id: 1 };
    let it2 = Item { name: base, id: 2 };
    let it3 = Item { name: base, id: 3 };
    print(f"Items: {it1.name}#{it1.id}, {it2.name}#{it2.id}, {it3.name}#{it3.id}");
    return 0;
}
EOF
rm -f "$TMP_DIR/t19.bin"
if ! "$CCO" "$TMP_DIR/t19.cco" -o "$TMP_DIR/t19.bin" 2>"$TMP_DIR/t19.err"; then
    fail_test "compilation failed: $(cat "$TMP_DIR/t19.err")"
elif ! valgrind --leak-check=full --error-exitcode=1 "$TMP_DIR/t19.bin" >/dev/null 2>"$TMP_DIR/t19_val.err"; then
    fail_test "Valgrind error: $(cat "$TMP_DIR/t19_val.err")"
else
    pass_test
fi

# ------------------------------------------------------------------------------
# Test 20: Compound class with string tag and map of heap resources
# ------------------------------------------------------------------------------
run_test "Test 20: Class with string and map of resources cleans all fields cleanly"
cat << 'EOF' > "$TMP_DIR/t20.cco"
class Resource {
    id: int;
}

class Cache {
    tag: string;
    entries: map[string]Resource;
}

fn make_cache(tag: string) -> Cache {
    let m: map[string]Resource = map_new(string, Resource);
    m = put(m, "k1", Resource { id: 101 });
    m = put(m, "k2", Resource { id: 202 });
    return Cache {
        tag: tag,
        entries: m
    };
}

fn main() -> int {
    let c = make_cache("active_cache");
    print(f"Cache tag: {c.tag}, count: {len(c.entries)}");
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
echo "=============================================================="
echo "String Field Lifecycle Suite: $passed_tests / $total_tests Passed"
echo "=============================================================="

if [ "$failed_tests" -gt 0 ]; then
    exit 1
fi
exit 0
