# Part 4: Memory & Functions (References, Pointer Concepts, Functions, Callbacks)

[← Back to Guide Index](README.md)

---

# 25. Memory Addresses & Safe References

> **Honest Language Note (Pointers & References)**: Cco's `&Type` borrow is a compile-time checked safe reference, **NOT** a raw C pointer. Cco references do not permit pointer arithmetic, raw address printing, or manual `*ptr` dereferencing. Fields and methods are accessed directly via dot notation (`.`) with automatic compiler dereferencing and zero-leak memory safety.

## 127. Memory Location & Safe Borrowing
Every variable lives at a distinct physical location in memory. In Cco, passing with `&` borrows the variable's memory reference without copying the underlying bytes.

```cco
// codebase/127_memory_address_concept.cco
struct User {
    age: int;
    score: int;
}

fn inspect_user(u: &User) -> void {
    print(f"Inspecting User (Age: {u.age}, Score: {u.score})");
}

fn main() -> int {
    let my_user: User = User { age: 43, score: 100 };

    print("=== Memory Addresses & References ===");
    inspect_user(&my_user);

    return 0;
}
```
**Run:** `./cco codebase/127_memory_address_concept.cco --run`  
**Output:**
```text
=== Memory Addresses & References ===
Inspecting User (Age: 43, Score: 100)
```

---

## 128. In-Place Mutation via Memory References
Passing a memory reference (`&Account`) allows functions to modify data in-place without returning or re-allocating copies.

```cco
// codebase/128_borrowed_reference_struct.cco
struct Account {
    balance: int;
}

fn deposit(acc: &Account, amount: int) -> void {
    acc.balance += amount;
}

fn main() -> int {
    let my_acc: Account = Account { balance: 100 };
    print(f"Initial balance: ${my_acc.balance}");

    // Pass by reference (&) to modify memory directly without copying
    deposit(&my_acc, 50);

    print(f"Balance after deposit: ${my_acc.balance}");
    return 0;
}
```
**Run:** `./cco codebase/128_borrowed_reference_struct.cco --run`  
**Output:**
```text
Initial balance: $100
Balance after deposit: $150
```

---


---

# 26. Pointers vs. Automatic Memory Management

## 129. Safe References vs. Raw C Pointers
In C, pointers require confusing `*ptr` dereferencing and manual pointer arithmetic. In Cco, safe borrowing with `&` allows direct, clean member access (`p.score += points`) with zero pointer bugs.

```cco
// codebase/129_pointer_vs_reference.cco
struct Player {
    score: int;
}

fn add_points(p: &Player, points: int) -> void {
    p.score += points;
}

fn main() -> int {
    let player: Player = Player { score: 100 };
    print(f"Original score: {player.score}");

    // Borrow player memory safely without manual pointer math
    add_points(&player, 50);

    print(f"Updated score after reference update: {player.score}");
    return 0;
}
```
**Run:** `./cco codebase/129_pointer_vs_reference.cco --run`  
**Output:**
```text
Original score: 100
Updated score after reference update: 150
```

---

## 130. Automatic Free & Compile-Time Lifetime Tracking
Unlike C where forgetting `free()` leaks memory and freeing twice crashes the program, Cco automatically emits the exact `free()` instructions when a variable leaves scope.

```cco
// codebase/130_automatic_free_scope.cco
fn main() -> int {
    print("=== Scope-Based Automatic Free in Cco ===");

    // In Cco, every allocated buffer is automatically freed when leaving scope!
    let numbers: int[] = alloc(int, 5);
    numbers[0] = 10;
    numbers[1] = 20;
    numbers[2] = 30;
    numbers[3] = 40;
    numbers[4] = 50;

    for n in numbers {
        print(n);
    }

    print("Exiting main: numbers array will be auto-freed by Cco with 0 leaks!");
    return 0;
}
```
**Run:** `./cco codebase/130_automatic_free_scope.cco --run`  
**Output:**
```text
=== Scope-Based Automatic Free in Cco ===
10
20
30
40
50
Exiting main: numbers array will be auto-freed by Cco with 0 leaks!
```

---


---

# 27. Pointers & Array Storage

## 131. Contiguous Memory & In-Place Updates
Arrays occupy continuous memory blocks. In Cco, indexing `arr[i]` maps directly to the underlying contiguous buffer without manual pointer offsets (`*(arr + i)`).

```cco
// codebase/131_pointer_array_equivalence.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 4);
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    print("=== Original Array Elements ===");
    for (let i = 0; i < 4; i++) {
        print(f"Index {i}: {my_numbers[i]}");
    }

    // In C: *(my_numbers) = 13; *(my_numbers + 1) = 17;
    // In Cco: Safe, high-speed direct indexing
    my_numbers[0] = 13;
    my_numbers[1] = 17;

    print("\n=== After In-Place Element Updates ===");
    print(f"Element 0: {my_numbers[0]}");
    print(f"Element 1: {my_numbers[1]}");

    return 0;
}
```
**Run:** `./cco codebase/131_pointer_array_equivalence.cco --run`  
**Output:**
```text
=== Original Array Elements ===
Index 0: 25
Index 1: 50
Index 2: 75
Index 3: 100

=== After In-Place Element Updates ===
Element 0: 13
Element 1: 17
```

---

## 132. Safe Array Iteration vs. Pointer Walk
Iterating over contiguous elements directly with `for num in my_numbers` avoids out-of-bounds pointer offsets and memory corruption.

```cco
// codebase/132_array_traversal_comparison.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 4);
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    print("=== Sequential Array Traversal ===");
    for num in my_numbers {
        print(num);
    }

    return 0;
}
```
**Run:** `./cco codebase/132_array_traversal_comparison.cco --run`  
**Output:**
```text
=== Sequential Array Traversal ===
25
50
75
100
```

---


---

# 28. Pointer Arithmetic & Cursor Navigation

## 133. Cursor Navigation (`++`, `--`, `+= 2`)
Moving forward, backward, and jumping across array positions without risking wild dangling memory pointers.

```cco
// codebase/133_pointer_cursor_navigation.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 3);
    my_numbers[0] = 10;
    my_numbers[1] = 20;
    my_numbers[2] = 30;

    let cursor = 0; // points to index 0 (10)
    print(f"Current element: {my_numbers[cursor]}");

    cursor++; // move to index 1 (20)
    print(f"After cursor++: {my_numbers[cursor]}");

    cursor--; // move back to index 0 (10)
    print(f"After cursor--: {my_numbers[cursor]}");

    cursor += 2; // jump 2 positions to index 2 (30)
    print(f"After cursor += 2: {my_numbers[cursor]}");

    return 0;
}
```
**Run:** `./cco codebase/133_pointer_cursor_navigation.cco --run`  
**Output:**
```text
Current element: 10
After cursor++: 20
After cursor--: 10
After cursor += 2: 30
```

---

## 134. Distance Between Elements
Calculating the distance in elements between two array positions (`end_idx - start_idx`).

```cco
// codebase/134_pointer_distance_elements.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 5);
    my_numbers[0] = 10;
    my_numbers[1] = 20;
    my_numbers[2] = 30;
    my_numbers[3] = 40;
    my_numbers[4] = 50;

    let start_idx = 1; // points to 20
    let end_idx = 4;   // points to 50

    let distance = end_idx - start_idx;

    print(f"Start element [{start_idx}]: {my_numbers[start_idx]}");
    print(f"End element [{end_idx}]: {my_numbers[end_idx]}");
    print(f"Distance between elements: {distance} elements apart");

    return 0;
}
```
**Run:** `./cco codebase/134_pointer_distance_elements.cco --run`  
**Output:**
```text
Start element [1]: 20
End element [4]: 50
Distance between elements: 3 elements apart
```

---

## 135. Stepping Through Array via Cursor Loop
Walking through elements sequentially by advancing a cursor without managing raw memory addresses.

```cco
// codebase/135_pointer_subrange_iteration.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 4);
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    print("=== Stepping Through Array via Cursor ===");
    let ptr = 0;
    for (let i = 0; i < 4; i++) {
        print(f"Element at cursor: {my_numbers[ptr]}");
        ptr++;
    }

    return 0;
}
```
**Run:** `./cco codebase/135_pointer_subrange_iteration.cco --run`  
**Output:**
```text
=== Stepping Through Array via Cursor ===
Element at cursor: 25
Element at cursor: 50
Element at cursor: 75
Element at cursor: 100
```

---


---

# 29. Pointer to Pointer & Multi-Level Indirection

## 136. Multi-Level Reference Indirection
In C, pointer-to-pointer (`int **pptr`) requires manual address management and double dereferencing (`**pptr`). In Cco, multi-level references are structured through nested heap-allocated object models (`handle.target.val`).

```cco
// codebase/136_pointer_to_pointer_concept.cco
class ValueBox {
    val: int;
}

class ValueHandle {
    target: ValueBox;
}

fn main() -> int {
    let handle: ValueHandle = ValueHandle {
        target: ValueBox { val: 10 }
    };

    print("=== Multi-Level Reference Indirection ===");
    print(f"Value through handle: {handle.target.val}");

    return 0;
}
```
**Run:** `./cco codebase/136_pointer_to_pointer_concept.cco --run`  
**Output:**
```text
=== Multi-Level Reference Indirection ===
Value through handle: 10
```

---

## 137. Mutating Values Across Reference Boundaries
Mutating nested properties through borrowed references (`s.score.points = new_points`) without double-dereference crashes or manual address extraction.

```cco
// codebase/137_nested_reference_update.cco
class ScoreData {
    points: int;
}

class Session {
    score: ScoreData;
}

fn update_session_score(s: &Session, new_points: int) -> void {
    // In C: **pptr = 20; (dangerous manual double-dereference)
    // In Cco: Safe, structured property indirection
    s.score.points = new_points;
}

fn main() -> int {
    let session: Session = Session {
        score: ScoreData { points: 5 }
    };

    print(f"Original score: {session.score.points}");

    // Mutate through nested object reference
    update_session_score(&session, 20);

    print(f"Updated score: {session.score.points}");
    return 0;
}
```
**Run:** `./cco codebase/137_nested_reference_update.cco --run`  
**Output:**
```text
Original score: 5
Updated score: 20
```

---


---

# 30. Function Declaration & Invocation

## 138. Declaring and Calling Functions
Defining reusable code blocks with `fn name() -> return_type` and invoking them in `main()`.

```cco
// codebase/138_function_basic_call.cco
fn my_function() -> void {
    print("I just got executed!");
}

fn main() -> int {
    my_function();
    return 0;
}
```
**Run:** `./cco codebase/138_function_basic_call.cco --run`  
**Output:** `I just got executed!`

---

## 139. Multiple Function Calls (Code Reuse)
Calling the same function multiple times without duplicating logic.

```cco
// codebase/139_function_multiple_calls.cco
fn my_function() -> void {
    print("I just got executed!");
}

fn main() -> int {
    print("=== Calling Function Multiple Times ===");
    my_function();
    my_function();
    my_function();
    return 0;
}
```
**Run:** `./cco codebase/139_function_multiple_calls.cco --run`  
**Output:**
```text
=== Calling Function Multiple Times ===
I just got executed!
I just got executed!
I just got executed!
```

---

## 140. Function-Encapsulated Computation
Executing internal arithmetic computations and formatting inside a function body.

```cco
// codebase/140_function_calculate_sum.cco
fn calculate_sum() -> void {
    let x = 5;
    let y = 10;
    let sum = x + y;
    print(f"The sum of x + y is: {sum}");
}

fn main() -> int {
    print("=== Function Computation ===");
    calculate_sum();
    return 0;
}
```
**Run:** `./cco codebase/140_function_calculate_sum.cco --run`  
**Output:**
```text
=== Function Computation ===
The sum of x + y is: 15
```

---


---

# 31. Function Parameters & Return Values

## 141. Single Parameter Function
Passing a string parameter into a function.

```cco
// codebase/141_param_single_string.cco
fn greet(name: string) -> void {
    print(f"Hello {name}");
}

fn main() -> int {
    greet("Liam");
    greet("Jenny");
    greet("Anja");
    return 0;
}
```
**Run:** `./cco codebase/141_param_single_string.cco --run`  
**Output:**
```text
Hello Liam
Hello Jenny
Hello Anja
```

---

## 142. Multiple Function Parameters
Passing mixed types (string and integer) across parameter positions.

```cco
// codebase/142_param_multiple_types.cco
fn introduce(name: string, age: int) -> void {
    print(f"Hello {name}. You are {age} years old.");
}

fn main() -> int {
    introduce("Liam", 3);
    introduce("Jenny", 14);
    introduce("Anja", 30);
    return 0;
}
```
**Run:** `./cco codebase/142_param_multiple_types.cco --run`  
**Output:**
```text
Hello Liam. You are 3 years old.
Hello Jenny. You are 14 years old.
Hello Anja. You are 30 years old.
```

---

## 143. Passing Arrays to Functions
Accepting an array parameter (`numbers: int[]`) and looping through elements dynamically.

```cco
// codebase/143_param_pass_array.cco
fn print_numbers(numbers: int[]) -> void {
    for n in numbers {
        print(n);
    }
}

fn main() -> int {
    let my_numbers: int[] = alloc(int, 5);
    my_numbers[0] = 10;
    my_numbers[1] = 20;
    my_numbers[2] = 30;
    my_numbers[3] = 40;
    my_numbers[4] = 50;

    print("=== Passing Array to Function ===");
    print_numbers(my_numbers);
    return 0;
}
```
**Run:** `./cco codebase/143_param_pass_array.cco --run`  
**Output:**
```text
=== Passing Array to Function ===
10
20
30
40
50
```

---

## 144. Function Return Values
Returning computed integer values (`-> int`) and storing them in variables.

```cco
// codebase/144_function_return_value.cco
fn calculate_sum(x: int, y: int) -> int {
    return x + y;
}

fn main() -> int {
    let result1 = calculate_sum(5, 3);
    let result2 = calculate_sum(8, 2);
    let result3 = calculate_sum(15, 15);

    print(f"Result1 is: {result1}");
    print(f"Result2 is: {result2}");
    print(f"Result3 is: {result3}");

    return 0;
}
```
**Run:** `./cco codebase/144_function_return_value.cco --run`  
**Output:**
```text
Result1 is: 8
Result2 is: 10
Result3 is: 30
```

---

## 145. Real-Life Conversion: Fahrenheit to Celsius
Converting temperature with floating-point return calculations.

```cco
// codebase/145_function_fahrenheit_celsius.cco
fn to_celsius(fahrenheit: float) -> float {
    return (5.0 / 9.0) * (fahrenheit - 32.0);
}

fn main() -> int {
    let f_value = 98.8;
    let result = to_celsius(f_value);

    print(f"Fahrenheit: {f_value}");
    print(f"Convert Fahrenheit to Celsius: {result}");

    return 0;
}
```
**Run:** `./cco codebase/145_function_fahrenheit_celsius.cco --run`  
**Output:**
```text
Fahrenheit: 98.8
Convert Fahrenheit to Celsius: 37.1111
```

---

## 146. Storing Multiple Function Results in an Array
Calling functions with varying inputs, storing computed returns in an array, and iterating over the results.

```cco
// codebase/146_function_results_in_array.cco
fn calculate_sum(x: int, y: int) -> int {
    return x + y;
}

fn main() -> int {
    let result_arr: int[] = alloc(int, 6);

    result_arr[0] = calculate_sum(5, 3);
    result_arr[1] = calculate_sum(8, 2);
    result_arr[2] = calculate_sum(15, 15);
    result_arr[3] = calculate_sum(9, 1);
    result_arr[4] = calculate_sum(7, 7);
    result_arr[5] = calculate_sum(1, 1);

    print("=== Storing Function Results in Array ===");
    for (let i = 0; i < 6; i++) {
        print(f"Result{i + 1} is = {result_arr[i]}");
    }

    return 0;
}
```
**Run:** `./cco codebase/146_function_results_in_array.cco --run`  
**Output:**
```text
=== Storing Function Results in Array ===
Result1 is = 8
Result2 is = 10
Result3 is = 30
Result4 is = 10
Result5 is = 14
Result6 is = 2
```

---


---

# 32. Function Declaration & Definition Order

## 147. Functions Defined Below `main()`
In C, defining functions below `main()` requires manual forward prototype declarations. In Cco, the multi-pass compiler automatically resolves and emits forward prototypes.

```cco
// codebase/147_function_defined_below_main.cco
fn main() -> int {
    let result = add_numbers(5, 3);
    print(f"Result is = {result}");
    return 0;
}

// Function defined below main without forward declaration boilerplate
fn add_numbers(x: int, y: int) -> int {
    return x + y;
}
```
**Run:** `./cco codebase/147_function_defined_below_main.cco --run`  
**Output:** `Result is = 8`

---

## 148. Functions Calling Subsequent Functions
Functions can call other helper functions declared later in the file seamlessly.

```cco
// codebase/148_functions_calling_functions.cco
fn main() -> int {
    first_function();
    return 0;
}

fn first_function() -> void {
    print("Some text in first_function");
    second_function();
}

fn second_function() -> void {
    print("Hey! Some text in second_function");
}
```
**Run:** `./cco codebase/148_functions_calling_functions.cco --run`  
**Output:**
```text
Some text in first_function
Hey! Some text in second_function
```

---


---

# 33. Built-In Math Functions

## 149. Square Root (`sqrt()`)
Computing square roots directly without `#include <math.h>` boilerplate.

```cco
// codebase/149_math_sqrt.cco
fn main() -> int {
    let result = sqrt(16.0);
    print(f"Square root of 16.0: {result}");
    return 0;
}
```
**Run:** `./cco codebase/149_math_sqrt.cco --run`  
**Output:** `Square root of 16.0: 4`

---

## 150. Rounding (`ceil()` and `floor()`)
Rounding upwards to the nearest integer with `ceil()` and downwards with `floor()`.

```cco
// codebase/150_math_ceil_floor.cco
fn main() -> int {
    let val = 1.4;
    let ceil_val = ceil(val);
    let floor_val = floor(val);

    print(f"Original value: {val}");
    print(f"ceil(1.4): {ceil_val}");
    print(f"floor(1.4): {floor_val}");

    return 0;
}
```
**Run:** `./cco codebase/150_math_ceil_floor.cco --run`  
**Output:**
```text
Original value: 1.4
ceil(1.4): 2
floor(1.4): 1
```

---

## 151. Exponentiation (`pow()`)
Calculating powers ($x^y$) using built-in `pow(base, exponent)`.

```cco
// codebase/151_math_power.cco
fn main() -> int {
    let base = 4.0;
    let exponent = 3.0;
    let result = pow(base, exponent);

    print(f"{base} raised to the power of {exponent}: {result}");
    return 0;
}
```
**Run:** `./cco codebase/151_math_power.cco --run`  
**Output:** `4 raised to the power of 3: 64`

---

## 152. Absolute Values, Minimum, and Maximum
Using `abs_int()`, `min_int()`, and `max_int()` for standard numerical operations.

```cco
// codebase/152_math_abs_min_max.cco
fn main() -> int {
    let neg_num = -42;
    let abs_val = abs_int(neg_num);

    let smaller = min_int(10, 20);
    let larger = max_int(10, 20);

    print(f"abs_int({neg_num}): {abs_val}");
    print(f"min_int(10, 20): {smaller}");
    print(f"max_int(10, 20): {larger}");

    return 0;
}
```
**Run:** `./cco codebase/152_math_abs_min_max.cco --run`  
**Output:**
```text
abs_int(-42): 42
min_int(10, 20): 10
max_int(10, 20): 20
```

---


---

# 34. Small Functions & Inlining Optimization

## 153. Small Utility Function (`square()`)
Writing small, single-purpose functions that eliminate call overhead and optimize cleanly during compilation.

```cco
// codebase/153_inline_optimization_square.cco
fn square(x: int) -> int {
    return x * x;
}

fn main() -> int {
    let num = 5;
    let result = square(num);

    print(f"Square of {num}: {result}");
    return 0;
}
```
**Run:** `./cco codebase/153_inline_optimization_square.cco --run`  
**Output:** `Square of 5: 25`

---

## 154. Arithmetic Helper (`add()`)
Encapsulating basic arithmetic operations into modular reusable routines.

```cco
// codebase/154_inline_optimization_add.cco
fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() -> int {
    let sum = add(5, 3);
    print(f"Sum of 5 + 3: {sum}");
    return 0;
}
```
**Run:** `./cco codebase/154_inline_optimization_add.cco --run`  
**Output:** `Sum of 5 + 3: 8`

---


---

# 35. Recursion & Self-Calling Functions

## 155. Recursive Sum of a Range (1 to 10)
Breaking a range sum down into repeated binary additions until reaching base case 0.

```cco
// codebase/155_recursion_sum_range.cco
fn sum_range(k: int) -> int {
    if (k > 0) {
        return k + sum_range(k - 1);
    } else {
        return 0;
    }
}

fn main() -> int {
    let result = sum_range(10);
    print(f"Recursive sum of 1 to 10: {result}");
    return 0;
}
```
**Run:** `./cco codebase/155_recursion_sum_range.cco --run`  
**Output:** `Recursive sum of 1 to 10: 55`

---

## 156. Recursive Countdown (5 to 1)
Printing numbers downwards via recursive tail calls.

```cco
// codebase/156_recursion_countdown.cco
fn countdown(n: int) -> void {
    if (n > 0) {
        print(n);
        countdown(n - 1);
    }
}

fn main() -> int {
    print("=== Recursive Countdown ===");
    countdown(5);
    return 0;
}
```
**Run:** `./cco codebase/156_recursion_countdown.cco --run`  
**Output:**
```text
=== Recursive Countdown ===
5
4
3
2
1
```

---

## 157. Recursive Factorial ($N!$)
Computing the product of all integers down to 1 ($5! = 5 \times 4 \times 3 \times 2 \times 1 = 120$).

```cco
// codebase/157_recursion_factorial.cco
fn factorial(n: int) -> int {
    if (n > 1) {
        return n * factorial(n - 1);
    } else {
        return 1;
    }
}

fn main() -> int {
    let result = factorial(5);
    print(f"Factorial of 5 is {result}");
    return 0;
}
```
**Run:** `./cco codebase/157_recursion_factorial.cco --run`  
**Output:** `Factorial of 5 is 120`

---


---

# 36. Function Pointers & Interface Dispatch

## 158. Dynamic Callback Dispatch via Interfaces
In C, callbacks require raw function pointers (`void (*func)()`). In Cco, callbacks are structured and type-safe using Interfaces (`impl Greeter`) and polymorphic dynamic dispatch.

```cco
// codebase/158_callback_interface_greeter.cco
interface Greeter {
    fn greet(self) -> void;
}

class MorningGreeter {
    greeting: string;

    fn greet(self) -> void {
        print(self.greeting);
    }
}

class EveningGreeter {
    greeting: string;

    fn greet(self) -> void {
        print(self.greeting);
    }
}

impl Greeter for MorningGreeter;
impl Greeter for EveningGreeter;

fn execute_greeting(g: impl Greeter) -> void {
    g.greet();
}

fn main() -> int {
    let morning: MorningGreeter = MorningGreeter { greeting: "Good morning!" };
    let evening: EveningGreeter = EveningGreeter { greeting: "Good evening!" };

    print("=== Dynamic Callback Dispatch ===");
    execute_greeting(morning);
    execute_greeting(evening);

    return 0;
}
```
**Run:** `./cco codebase/158_callback_interface_greeter.cco --run`  
**Output:**
```text
=== Dynamic Callback Dispatch ===
Good morning!
Good evening!
```

---

## 159. Operation Dispatch & Calculator
Dispatching arithmetic operations dynamically through an `Operation` interface contract.

```cco
// codebase/159_operation_dispatch_calculator.cco
interface Operation {
    fn compute(self, a: int, b: int) -> int;
}

class AddOp {
    tag: int;

    fn compute(self, a: int, b: int) -> int {
        return a + b + self.tag;
    }
}

class SubOp {
    tag: int;

    fn compute(self, a: int, b: int) -> int {
        return a - b + self.tag;
    }
}

class MulOp {
    tag: int;

    fn compute(self, a: int, b: int) -> int {
        return (a * b) + self.tag;
    }
}

impl Operation for AddOp;
impl Operation for SubOp;
impl Operation for MulOp;

fn calculate(op: impl Operation, a: int, b: int) -> void {
    let res = op.compute(a, b);
    print(res);
}

fn main() -> int {
    let adder: AddOp = AddOp { tag: 0 };
    let suber: SubOp = SubOp { tag: 0 };
    let muler: MulOp = MulOp { tag: 0 };

    let x = 10;
    let y = 5;

    print("=== Dynamic Operation Dispatch ===");
    calculate(adder, x, y);
    calculate(suber, x, y);
    calculate(muler, x, y);

    return 0;
}
```
**Run:** `./cco codebase/159_operation_dispatch_calculator.cco --run`  
**Output:**
```text
=== Dynamic Operation Dispatch ===
15
5
50
```

---


---

# 37. Callback Patterns & Sorting Comparators

## 160. Simple Callback Lifecycle Hooks
Executing pre- and post-processing lifecycle hooks around a passed callback action.

```cco
// codebase/160_callback_hook_lifecycle.cco
interface Action {
    fn run(self) -> void;
}

class HelloAction {
    msg: string;

    fn run(self) -> void {
        print(self.msg);
    }
}

impl Action for HelloAction;

fn run_callback(cb: impl Action) -> void {
    print("Before calling the callback...");
    cb.run();
    print("After calling the callback.");
}

fn main() -> int {
    let action: HelloAction = HelloAction { msg: "Hello from the callback!" };
    run_callback(action);
    return 0;
}
```
**Run:** `./cco codebase/160_callback_hook_lifecycle.cco --run`  
**Output:**
```text
Before calling the callback...
Hello from the callback!
After calling the callback.
```

---

## 161. Parameterized Callback Execution
Passing arguments into a callback handler dynamically.

```cco
// codebase/161_callback_with_parameters.cco
interface BinaryAction {
    fn execute(self, a: int, b: int) -> void;
}

class AddLogger {
    prefix: string;

    fn execute(self, a: int, b: int) -> void {
        let sum = a + b;
        print(f"{self.prefix} {a} + {b} = {sum}");
    }
}

impl BinaryAction for AddLogger;

fn calculate(action: impl BinaryAction, x: int, y: int) -> void {
    action.execute(x, y);
}

fn main() -> int {
    let logger: AddLogger = AddLogger { prefix: "The sum is:" };
    calculate(logger, 5, 3);
    return 0;
}
```
**Run:** `./cco codebase/161_callback_with_parameters.cco --run`  
**Output:** `The sum is: 5 + 3 = 8`

---

## 162. Custom Sorting Comparator Callback
Custom comparator callback sorting integers in ascending order (comparable to C's `qsort()` callback).

```cco
// codebase/162_comparator_sort_callback.cco
interface Comparator {
    fn compare(self, a: int, b: int) -> int;
}

class AscendingComparator {
    tag: int;

    fn compare(self, a: int, b: int) -> int {
        return a - b + self.tag;
    }
}

impl Comparator for AscendingComparator;

fn sort_and_print(numbers: int[], count: int, cmp: impl Comparator) -> void {
    for (let i = 0; i < count; i++) {
        for (let j = 0; j < count - 1; j++) {
            if (cmp.compare(numbers[j], numbers[j + 1]) > 0) {
                let temp = numbers[j];
                numbers[j] = numbers[j + 1];
                numbers[j + 1] = temp;
            }
        }
    }

    print("=== Sorted Numbers via Comparator Callback ===");
    for n in numbers {
        print(n);
    }
}

fn main() -> int {
    let numbers: int[] = alloc(int, 5);
    numbers[0] = 5;
    numbers[1] = 2;
    numbers[2] = 9;
    numbers[3] = 1;
    numbers[4] = 7;

    let cmp: AscendingComparator = AscendingComparator { tag: 0 };
    sort_and_print(numbers, 5, cmp);

    return 0;
}
```
**Run:** `./cco codebase/162_comparator_sort_callback.cco --run`  
**Output:**
```text
=== Sorted Numbers via Comparator Callback ===
1
2
5
7
9
```

---


---


[← Back to Guide Index](README.md)
