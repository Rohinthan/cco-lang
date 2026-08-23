# Part 6: Safety, Modules & Systems Programming (Error Handling, Imports, Bitwise)

[← Back to Guide Index](README.md)

---

# 55. Error Prevention & Runtime Guardrails

## 196. Proactive Divide-by-Zero Guards
Guarding against runtime arithmetic exceptions by validating divisors prior to evaluation.

```cco
// codebase/196_error_safe_division_guard.cco
fn safe_divide(numerator: int, denominator: int) -> void {
    if (denominator == 0) {
        print(f"Error: Cannot divide {numerator} by zero!");
    } else {
        let result = numerator / denominator;
        print(f"{numerator} / {denominator} = {result}");
    }
}

fn main() -> int {
    print("=== Safe Division Guards ===");
    safe_divide(10, 2);
    safe_divide(10, 0);

    return 0;
}
```
**Run:** `./cco codebase/196_error_safe_division_guard.cco --run`  
**Output:**
```text
=== Safe Division Guards ===
10 / 2 = 5
Error: Cannot divide 10 by zero!
```

---

## 197. Out-of-Bounds Protection via Iterators
In C, accessing invalid array indexes (`arr[8]`) reads arbitrary process memory without error. In Cco, `for in` iteration guarantees accessing only valid, allocated elements.

```cco
// codebase/197_error_safe_bounds_iteration.cco
fn main() -> int {
    let numbers: int[] = alloc(int, 3);
    numbers[0] = 10;
    numbers[1] = 20;
    numbers[2] = 30;

    print("=== Safe Array Traversal ===");
    // In C, accessing numbers[8] reads corrupt garbage from stack/heap.
    // In Cco, 'for in' loop guarantees traversing only valid, in-bounds elements!
    for n in numbers {
        print(n);
    }

    return 0;
}
```
**Run:** `./cco codebase/197_error_safe_bounds_iteration.cco --run`  
**Output:**
```text
=== Safe Array Traversal ===
10
20
30
```

---


---

# 56. Debugging Techniques & Tracepoint Diagnostics

## 198. Tracepoints & State Inspection
Using structured debug print logs to trace intermediate variable states and verify algorithm correctness.

```cco
// codebase/198_debugging_trace_and_variable_inspection.cco
fn calculate_total_price(price: int, tax: int, discount: int) -> int {
    print(f"[DEBUG] Inputs: price={price}, tax={tax}, discount={discount}");

    let with_tax = price + tax;
    print(f"[DEBUG] Intermediate price with tax: {with_tax}");

    let final_price = with_tax - discount;
    print(f"[DEBUG] Calculated final price: {final_price}");

    return final_price;
}

fn main() -> int {
    print("=== Tracepoint & Variable Inspection ===");
    let total = calculate_total_price(100, 15, 20);
    print(f"Final Total: {total}");

    return 0;
}
```
**Run:** `./cco codebase/198_debugging_trace_and_variable_inspection.cco --run`  
**Output:**
```text
=== Tracepoint & Variable Inspection ===
[DEBUG] Inputs: price=100, tax=15, discount=20
[DEBUG] Intermediate price with tax: 115
[DEBUG] Calculated final price: 95
Final Total: 95
```

---

## 199. Bounds-Checking & Pre-Condition Validation
Validating index boundaries against dynamic slice length (`len(numbers)`) to output helpful diagnostic messages instead of crashing.

```cco
// codebase/199_debugging_safe_index_bounds_check.cco
fn main() -> int {
    let numbers: int[] = alloc(int, 3);
    numbers[0] = 10;
    numbers[1] = 20;
    numbers[2] = 30;

    let target_index = 5;
    let count = len(numbers);

    print("=== Debugging Bounds-Checked Access ===");
    print(f"[DEBUG] Target index = {target_index}, Array length = {count}");

    if (target_index >= 0 && target_index < count) {
        print(f"Value = {numbers[target_index]}");
    } else {
        print(f"Error: Index {target_index} is out of bounds for array of length {count}!");
    }

    return 0;
}
```
**Run:** `./cco codebase/199_debugging_safe_index_bounds_check.cco --run`  
**Output:**
```text
=== Debugging Bounds-Checked Access ===
[DEBUG] Target index = 5, Array length = 3
Error: Index 5 is out of bounds for array of length 3!
```

---


---

# 57. NULL Pointers vs. Type-Safe Option Patterns

## 200. Type-Safe `Option` Pattern (Eliminating NULL Crashes)
In C, functions return `NULL` (address `0x0`) on missing values, which crashes the process if dereferenced without manual `ptr != NULL` checks. In Cco, the `Option` tagged enum (`Some` and `None`) enforces compile-time handling via `match`.

```cco
// codebase/200_null_safety_option_enum.cco
enum OptionInt {
    Some { value: int },
    None,
}

fn find_index(numbers: int[], target: int) -> OptionInt {
    for (let i = 0; i < len(numbers); i++) {
        if (numbers[i] == target) {
            return OptionInt.Some { value: i };
        }
    }
    return OptionInt.None;
}

fn print_result(opt: &OptionInt) -> void {
    match opt {
        OptionInt.Some { value } => {
            print(f"Found element at index: {value}");
        }
        OptionInt.None => {
            print("Element not found (None / NULL safe)!");
        }
    }
}

fn main() -> int {
    let numbers1: int[] = alloc(int, 4);
    numbers1[0] = 10;
    numbers1[1] = 20;
    numbers1[2] = 30;
    numbers1[3] = 40;

    let numbers2: int[] = alloc(int, 4);
    numbers2[0] = 10;
    numbers2[1] = 20;
    numbers2[2] = 30;
    numbers2[3] = 40;

    print("=== Type-Safe Option Pattern vs NULL ===");
    let found = find_index(numbers1, 30);
    print_result(&found);

    let not_found = find_index(numbers2, 99);
    print_result(&not_found);

    return 0;
}
```
**Run:** `./cco codebase/200_null_safety_option_enum.cco --run`  
**Output:**
```text
=== Type-Safe Option Pattern vs NULL ===
Found element at index: 2
Element not found (None / NULL safe)!
```

---

## 201. Safe Resource Verification
Safely asserting resource availability without segfaults.

```cco
// codebase/201_null_safety_file_check.cco
fn main() -> int {
    let filename = "build/atomic/null_test.txt";
    write_file(filename, "File opened successfully!");

    print("=== Safe Resource Verification ===");
    let content = read_file(filename);

    if (equals(content, "")) {
        print("Could not open file.");
    } else {
        print(f"Resource status: {content}");
    }

    return 0;
}
```
**Run:** `./cco codebase/201_null_safety_file_check.cco --run`  
**Output:**
```text
=== Safe Resource Verification ===
Resource status: File opened successfully!
```

---


---

# 58. Structured Error Handling & Result Enums

## 202. Result Enums vs Global `errno`
In C, error handling relies on global mutable `errno` and integer codes (`ENOENT`, `EACCES`, `ENOMEM`). In Cco, error handling is encapsulated cleanly via `Result` tagged enums and exhaustive `match` branching.

```cco
// codebase/202_error_result_enum_handling.cco
enum FileResult {
    Success { content: string },
    FileNotFound,
    PermissionDenied,
}

fn open_simulated_file(path: string) -> FileResult {
    if (equals(path, "config.txt")) {
        return FileResult.Success { content: "server_port=8080" };
    } else if (equals(path, "secret.txt")) {
        return FileResult.PermissionDenied;
    } else {
        return FileResult.FileNotFound;
    }
}

fn handle_file_operation(path: string) -> void {
    let result = open_simulated_file(path);

    print(f"--- Handling '{path}' ---");
    match &result {
        FileResult.Success { content } => {
            print(f"File loaded successfully: {content}");
        }
        FileResult.FileNotFound => {
            print("Error: File not found (ENOENT equivalent).");
        }
        FileResult.PermissionDenied => {
            print("Error: Access permission denied (EACCES equivalent).");
        }
    }
}

fn main() -> int {
    print("=== Structured Result Error Handling ===");
    handle_file_operation("config.txt");
    handle_file_operation("secret.txt");
    handle_file_operation("missing.txt");

    return 0;
}
```
**Run:** `./cco codebase/202_error_result_enum_handling.cco --run`  
**Output:**
```text
=== Structured Result Error Handling ===
--- Handling 'config.txt' ---
File loaded successfully: server_port=8080
--- Handling 'secret.txt' ---
Error: Access permission denied (EACCES equivalent).
--- Handling 'missing.txt' ---
Error: File not found (ENOENT equivalent).
```

---

## 203. Status Code Validation & Graceful Fallbacks
Validating inputs and returning discrete status codes without aborting the process abruptly.

```cco
// codebase/203_error_status_code_dispatch.cco
fn validate_and_process(user_id: int, quota: int) -> int {
    if (user_id <= 0) {
        print(f"Error: Invalid user ID {user_id} (EINVAL)");
        return 1; // Return error status
    }

    if (quota <= 0) {
        print(f"Error: Quota depleted for user {user_id} (ENOMEM)");
        return 2; // Return quota error status
    }

    print(f"Success: User {user_id} allocated quota {quota} (EXIT_SUCCESS)");
    return 0;
}

fn main() -> int {
    print("=== Status Code & Graceful Error Dispatch ===");
    let s1 = validate_and_process(101, 50);
    let s2 = validate_and_process(-5, 50);
    let s3 = validate_and_process(102, 0);

    print(f"\nFinal Exit Codes: s1={s1}, s2={s2}, s3={s3}");
    return 0;
}
```
**Run:** `./cco codebase/203_error_status_code_dispatch.cco --run`  
**Output:**
```text
=== Status Code & Graceful Error Dispatch ===
Success: User 101 allocated quota 50 (EXIT_SUCCESS)
Error: Invalid user ID -5 (EINVAL)
Error: Quota depleted for user 102 (ENOMEM)

Final Exit Codes: s1=0, s2=1, s3=2
```

---


---

# 59. Input Validation & Safe Parsing

## 204. Numerical Range Validation ($1 \le x \le 5$)
Validating that user inputs fall strictly within prescribed boundaries before triggering downstream business logic.

```cco
// codebase/204_input_validate_number_range.cco
fn validate_range(input_num: int) -> bool {
    if (input_num >= 1 && input_num <= 5) {
        return true;
    } else {
        return false;
    }
}

fn main() -> int {
    print("=== Number Range Validation (1 to 5) ===");

    let sample_inputs: int[] = alloc(int, 4);
    sample_inputs[0] = 8;
    sample_inputs[1] = -2;
    sample_inputs[2] = 4;
    sample_inputs[3] = 5;

    for num in sample_inputs {
        let is_valid = validate_range(num);
        if (is_valid) {
            print(f"Testing {num}: VALID (Accepted: You chose {num})");
        } else {
            print(f"Testing {num}: INVALID (Out of range [1, 5])");
        }
    }

    return 0;
}
```
**Run:** `./cco codebase/204_input_validate_number_range.cco --run`  
**Output:**
```text
=== Number Range Validation (1 to 5) ===
Testing 8: INVALID (Out of range [1, 5])
Testing -2: INVALID (Out of range [1, 5])
Testing 4: VALID (Accepted: You chose 4)
Testing 5: VALID (Accepted: You chose 5)
```

---

## 205. Non-Empty Text Validation
Validating string lengths and contents without manual `strcspn()` newline stripping hacks.

```cco
// codebase/205_input_validate_non_empty_text.cco
fn validate_and_greet(name: string) -> void {
    if (len(name) > 0 && !equals(name, "")) {
        print(f"Accepted: Hello, {name}!");
    } else {
        print("Rejected: Name cannot be empty.");
    }
}

fn main() -> int {
    print("=== Non-Empty Text Validation ===");
    validate_and_greet("");
    validate_and_greet("");
    validate_and_greet("John");

    return 0;
}
```
**Run:** `./cco codebase/205_input_validate_non_empty_text.cco --run`  
**Output:**
```text
=== Non-Empty Text Validation ===
Rejected: Name cannot be empty.
Rejected: Name cannot be empty.
Accepted: Hello, John!
```

---

## 206. Integer Format Validation (`is_int()` & `to_int()`)
Safely inspecting whether a string is a valid parseable integer prior to conversion, avoiding `sscanf()` format string vulnerabilities.

```cco
// codebase/206_input_validate_integer_format.cco
fn process_user_input(raw_input: string) -> void {
    if (is_int(raw_input)) {
        let num = to_int(raw_input);
        print(f"Input '{raw_input}' is a VALID integer: {num}");
    } else {
        print(f"Input '{raw_input}' is INVALID (Not an integer!)");
    }
}

fn main() -> int {
    print("=== Integer Format Validation with is_int() ===");
    process_user_input("AB");
    process_user_input("3.5");
    process_user_input("35");
    process_user_input("-42");

    return 0;
}
```
**Run:** `./cco codebase/206_input_validate_integer_format.cco --run`  
**Output:**
```text
=== Integer Format Validation with is_int() ===
Input 'AB' is INVALID (Not an integer!)
Input '3.5' is INVALID (Not an integer!)
Input '35' is a VALID integer: 35
Input '-42' is a VALID integer: -42
```

---


---

# 60. Pseudo-Random Number Generation & Simulations

## 207. Direct Random Range Generation (`random_int()`)
In C, generating numbers in a range requires `#include <stdlib.h>`, `#include <time.h>`, and manual modulo arithmetic (`rand() % 10`). In Cco, `random_int(min, max)` generates inclusive range integers directly.

```cco
// codebase/207_random_number_range.cco
fn main() -> int {
    // Optional: seed the generator for deterministic reproducible testing
    random_seed(42);

    print("=== Random Numbers in Range [0, 9] ===");
    let r1 = random_int(0, 9);
    let r2 = random_int(0, 9);
    let r3 = random_int(0, 9);

    print(f"Random 1: {r1}");
    print(f"Random 2: {r2}");
    print(f"Random 3: {r3}");

    return 0;
}
```
**Run:** `./cco codebase/207_random_number_range.cco --run`  
**Output:**
```text
=== Random Numbers in Range [0, 9] ===
Random 1: 6
Random 2: 0
Random 3: 1
```

---

## 208. Real-Life Simulation: Rolling Two Dice
Simulating rolling two independent six-sided dice ($1$ to $6$).

```cco
// codebase/208_random_dice_roll_simulation.cco
fn roll_dice() -> int {
    return random_int(1, 6);
}

fn main() -> int {
    random_seed(12345);

    print("=== Rolling Two Six-Sided Dice [1, 6] ===");
    let dice1 = roll_dice();
    let dice2 = roll_dice();
    let total = dice1 + dice2;

    print(f"You rolled {dice1} and {dice2} (total = {total})");

    return 0;
}
```
**Run:** `./cco codebase/208_random_dice_roll_simulation.cco --run`  
**Output:**
```text
=== Rolling Two Six-Sided Dice [1, 6] ===
You rolled 4 and 6 (total = 10)
```

---


---

# 61. Preprocessor Directives & Safe Modern Alternatives

## 209. Typed Constants vs `#define` Macro Values
In C, `#define PI 3.14` performs untyped textual replacement across the source. In Cco, typed bindings (`let pi: float = 3.14159`) provide strong type safety and compiler optimization without global macro pollution.

> **Honest Language Note**: While Cco avoids the hazards of preprocessor `#define` macros by using statically typed bindings, Cco does not yet feature a dedicated `const` keyword. Constant values follow naming conventions (`UPPER_CASE`) and non-reassignment.

```cco
// codebase/209_constants_vs_define_macros.cco
fn calculate_circle_area(radius: float) -> float {
    // Type-safe constant replacing untyped C '#define PI 3.14159'
    let pi: float = 3.14159;
    return pi * radius * radius;
}

fn main() -> int {
    let radius = 5.0;
    let area = calculate_circle_area(radius);

    print("=== Type-Safe Constants vs #define ===");
    print(f"Radius: {radius}");
    print(f"Calculated Circle Area: {area}");

    return 0;
}
```
**Run:** `./cco codebase/209_constants_vs_define_macros.cco --run`  
**Output:**
```text
=== Type-Safe Constants vs #define ===
Radius: 5
Calculated Circle Area: 78.5397
```

---

## 210. Pure Functions vs Parametric Function Macros
In C, macros like `#define SQUARE(x) ((x)*(x))` suffer from double-evaluation bugs if expressions have side-effects (e.g. `SQUARE(x++)`). In Cco, functions evaluate arguments exactly once with full type verification and zero inlining overhead.

```cco
// codebase/210_functions_vs_macro_functions.cco
fn square(x: int) -> int {
    // In C, '#define SQUARE(x) ((x)*(x))' evaluates expressions multiple times.
    // In Cco, functions evaluate arguments exactly once with full type safety!
    return x * x;
}

fn main() -> int {
    let num = 4;
    let result = square(num);

    print("=== Safe Functions vs Parameter Macros ===");
    print(f"Square of {num}: {result}");
    print(f"Square of (num + 2): {square(num + 2)}");

    return 0;
}
```
**Run:** `./cco codebase/210_functions_vs_macro_functions.cco --run`  
**Output:**
```text
=== Safe Functions vs Parameter Macros ===
Square of 4: 16
Square of (num + 2): 36
```

---


---

# 62. Modular Programming & Native Import System

## 211. Native Module Imports (`import`)
In C, modular programming requires maintaining three separate files (`calc.h`, `calc.c`, `main.c`) with `#ifndef` include guards and manual multi-file compiler flags (`gcc main.c calc.c`). In Cco, modular code is imported directly via `import "path/to/module.cco"` with automatic circular dependency prevention and whole-program AST merging.

### Module: `codebase/modules/calculator_math.cco`
```cco
// codebase/modules/calculator_math.cco
fn add(x: int, y: int) -> int {
    return x + y;
}

fn subtract(x: int, y: int) -> int {
    return x - y;
}
```

### Main Application: `codebase/211_modular_import_calculator.cco`
```cco
// codebase/211_modular_import_calculator.cco
import "modules/calculator_math.cco";

fn main() -> int {
    print("=== Modular Programming in Cco ===");
    let sum = add(5, 5);
    let diff = subtract(6, 4);

    print(f"5 + 5 = {sum}");
    print(f"6 - 4 = {diff}");

    return 0;
}
```
**Run:** `./cco codebase/211_modular_import_calculator.cco --run`  
**Output:**
```text
=== Modular Programming in Cco ===
5 + 5 = 10
6 - 4 = 2
```

---


---

# 63. Storage Classes & Encapsulated State

## 212. Stateful Objects vs Hidden `static` Function Variables
In C, `static int myNum` preserves state across calls but introduces hidden global state that breaks multi-threading and unit tests. In Cco, stateful objects encapsulate persistence cleanly, allowing multiple independent instances.

```cco
// codebase/212_storage_stateful_counter_class.cco
class Counter {
    count: int;

    fn next(self) -> int {
        self.count += 1;
        return self.count;
    }
}

fn main() -> int {
    // In C, 'static int myNum' inside a function creates global hidden state that cannot be reset.
    // In Cco, stateful objects encapsulate persistent state safely!
    let c1: Counter = Counter { count: 0 };

    print("=== Persistent Counter Calls ===");
    print(f"num = {c1.next()}");
    print(f"num = {c1.next()}");
    print(f"num = {c1.next()}");

    // We can also create a second independent counter:
    let c2: Counter = Counter { count: 10 };
    print(f"Second counter: {c2.next()}");

    return 0;
}
```
**Run:** `./cco codebase/212_storage_stateful_counter_class.cco --run`  
**Output:**
```text
=== Persistent Counter Calls ===
num = 1
num = 2
num = 3
Second counter: 11
```

---

## 213. Automatic Stack Scoping & Lifetime Isolation
Stack variables declared with `let` are scoped strictly within their enclosing block with zero memory leaks.

```cco
// codebase/213_storage_auto_scope_isolation.cco
fn main() -> int {
    // Local 'auto' variable
    let x = 50;
    print(f"Outer x: {x}");

    {
        let inner_x = 100;
        print(f"Inner block x: {inner_x}");
    }

    print(f"Outer x remains unchanged: {x}");
    return 0;
}
```
**Run:** `./cco codebase/213_storage_auto_scope_isolation.cco --run`  
**Output:**
```text
Outer x: 50
Inner block x: 100
Outer x remains unchanged: 50
```

---


---

# 64. Bitwise Concepts & Structured Permission Flags

## 214. Binary Shift Logic ($x \times 2^n$ & $x / 2^n$)
Understanding how bit shifting translates to powers of two ($3 \ll 2 = 12$, $12 \gg 2 = 3$).

```cco
// codebase/214_bitwise_shift_simulation.cco
// Binary Left Shift (x << n) is equivalent to x * (2^n)
fn shift_left(val: int, bits: int) -> int {
    let multiplier = 1;
    for (let i = 0; i < bits; i++) {
        multiplier *= 2;
    }
    return val * multiplier;
}

// Binary Right Shift (x >> n) is equivalent to x / (2^n)
fn shift_right(val: int, bits: int) -> int {
    let divisor = 1;
    for (let i = 0; i < bits; i++) {
        divisor *= 2;
    }
    return val / divisor;
}

fn main() -> int {
    let a = 3;
    let b = 12;

    print("=== Binary Shift Arithmetic ===");
    let shifted_left = shift_left(a, 2);
    print(f"3 << 2 (3 * 2^2): {shifted_left}");

    let shifted_right = shift_right(b, 2);
    print(f"12 >> 2 (12 / 2^2): {shifted_right}");

    return 0;
}
```
**Run:** `./cco codebase/214_bitwise_shift_simulation.cco --run`  
**Output:**
```text
=== Binary Shift Arithmetic ===
3 << 2 (3 * 2^2): 12
12 >> 2 (12 / 2^2): 3
```

---

## 215. Structured Permissions vs Bitmasks
In C, permission flags (`READ = 1`, `WRITE = 2`, `EXEC = 4`) use raw integers combined with bitwise OR (`|`) and bitwise AND (`&`). In Cco, structured boolean records (`Permissions`) provide self-documenting type safety without bitmasking bugs.

```cco
// codebase/215_structured_permission_flags.cco
struct Permissions {
    can_read: bool;
    can_write: bool;
    can_execute: bool;
}

fn print_access(user: string, p: Permissions) -> void {
    print(f"--- Access Rights for {user} ---");
    if (p.can_read) {
        print("Read: ALLOWED");
    } else {
        print("Read: DENIED");
    }

    if (p.can_write) {
        print("Write: ALLOWED");
    } else {
        print("Write: DENIED");
    }

    if (p.can_execute) {
        print("Execute: ALLOWED");
    } else {
        print("Execute: DENIED");
    }
}

fn main() -> int {
    let editor_perms: Permissions = Permissions {
        can_read: true,
        can_write: true,
        can_execute: false
    };

    let admin_perms: Permissions = Permissions {
        can_read: true,
        can_write: true,
        can_execute: true
    };

    print("=== Structured Permission Flags ===");
    print_access("Editor", editor_perms);
    print_access("Admin", admin_perms);

    return 0;
}
```
**Run:** `./cco codebase/215_structured_permission_flags.cco --run`  
**Output:**
```text
=== Structured Permission Flags ===
--- Access Rights for Editor ---
Read: ALLOWED
Write: ALLOWED
Execute: DENIED
--- Access Rights for Admin ---
Read: ALLOWED
Write: ALLOWED
Execute: ALLOWED
```

---


---

# 65. Integer Bit Widths & Bounded Domains

## 216. Standard Integer Ranges
Inspecting integer ranges spanning 8-bit, 16-bit, and 32-bit capacities ($100$, $30000$, $2000000$, $2000000000$).

```cco
// codebase/216_fixed_width_large_integers.cco
fn main() -> int {
    // Testing integer ranges across sizes
    let a = 100;        // Fits in 8-bit [-128..127]
    let b = 30000;      // Fits in 16-bit [-32768..32767]
    let c = 2000000;    // Fits in 32-bit [-2147483648..2147483647]
    let d = 2000000000; // 2 Billion (near 32-bit maximum)

    print("=== Integer Range Demonstration ===");
    print(f"8-bit range value: {a}");
    print(f"16-bit range value: {b}");
    print(f"32-bit range value: {c}");
    print(f"Large integer: {d}");

    return 0;
}
```
**Run:** `./cco codebase/216_fixed_width_large_integers.cco --run`  
**Output:**
```text
=== Integer Range Demonstration ===
8-bit range value: 100
16-bit range value: 30000
32-bit range value: 2000000
Large integer: 2000000000
```

---

## 217. Bounded Domain Validation (Battery Level)
In C, `uint8_t` is used to save memory for values within $[0, 255]$ (such as battery percentage). In Cco, domain validation routines ensure values remain within $[0, 100]$.

```cco
// codebase/217_bounded_domain_battery_level.cco
struct DeviceStatus {
    battery_percent: int;
    is_charging: bool;
}

fn set_battery_level(status: &DeviceStatus, percent: int) -> void {
    if (percent < 0 || percent > 100) {
        print(f"Error: Invalid battery level {percent}% (Must be 0 to 100)");
    } else {
        status.battery_percent = percent;
        print(f"Battery level updated to: {status.battery_percent} out of 100");
    }
}

fn main() -> int {
    let dev: DeviceStatus = DeviceStatus {
        battery_percent: 87,
        is_charging: false
    };

    print("=== Bounded Domain Validation (Battery Level) ===");
    print(f"Initial Battery: {dev.battery_percent} out of 100");

    set_battery_level(&dev, 95);
    set_battery_level(&dev, 150);

    return 0;
}
```
**Run:** `./cco codebase/217_bounded_domain_battery_level.cco --run`  
**Output:**
```text
=== Bounded Domain Validation (Battery Level) ===
Initial Battery: 87 out of 100
Battery level updated to: 95 out of 100
Error: Invalid battery level 150% (Must be 0 to 100)
```

---


---


[← Back to Guide Index](README.md)
