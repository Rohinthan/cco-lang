# Learn Cco by Example: A Python Programmer's Guide

Welcome to **Cco** (C--)! This guide is designed for developers who are familiar with basic programming concepts—especially in languages like Python—and want to see how to write clean, idiomatic code in Cco. 

Cco combines the visual conciseness and expressiveness of high-level scripting languages with the raw performance, zero-runtime overhead, and deterministic memory safety of C.

---

## 🔍 Cco vs. Python at a Glance

| Feature | Python | Cco (v19) |
| :--- | :--- | :--- |
| **Typing** | Dynamic typing (`x = 10`) | Static typing with type inference (`let x = 10;`) |
| **Execution** | Interpreted bytecode (CPython VM) | Compiled directly to standard ISO C11 native binaries |
| **Memory Management** | Automatic Garbage Collection (GC pauses) | Scope-based single ownership & auto-free (0 GC, 0 leaks) |
| **String Formatting** | F-strings: `f"Result: {val}"` | F-strings: `f"Result: {val}"` |
| **Variable Swapping** | Tuple unpacking: `a, b = b, a` | Explicit temporary variable swap |
| **Error Safety** | Runtime exceptions (`ValueError`, `ZeroDivisionError`) | Safe guard functions (`is_int()`, `is_float()`) + compile-time checks |

---

## 📚 The 10 Beginner Programs

---

## Program 1: Hello World

The traditional starting point in any language: printing a simple greeting to standard output.

```cco
// docs/guide/01_hello.cco
fn main() -> int {
    print("Hello, Cco!");
    return 0;
}
```

**Sample run:**
```text
$ ./01_hello
Hello, Cco!
```

> **Note**: Every executable Cco program begins execution at `fn main() -> int`. The `print()` built-in outputs text followed by a newline.

---

## Program 2: Basic Arithmetic with User Input

Read two numbers from the user, add them together, and format the output using an f-string.

```cco
// docs/guide/02_arithmetic.cco
fn main() -> int {
    print("Enter the first number: ");
    let input1 = read_line();
    if (!is_float(input1)) {
        print("Error: Invalid number entered.");
        return 1;
    }
    let num1 = to_float(input1);

    print("Enter the second number: ");
    let input2 = read_line();
    if (!is_float(input2)) {
        print("Error: Invalid number entered.");
        return 1;
    }
    let num2 = to_float(input2);

    let sum = num1 + num2;
    print(f"sum: {num1} + {num2} = {sum}");
    return 0;
}
```

**Sample run:**
```text
$ ./02_arithmetic
Enter the first number: 
15.5
Enter the second number: 
4.5
sum: 15.5 + 4.5 = 20
```

> **Note**: In Cco, user input via `read_line()` is always checked with guard functions like `is_float()` before calling `to_float()`. This safe pattern prevents runtime crashes on malformed inputs.

---

## Program 3: Division with Zero-Check Error Handling

Prompt the user for a numerator and denominator, check for division by zero before dividing, and display the result.

```cco
// docs/guide/03_division.cco
fn main() -> int {
    print("Enter numerator: ");
    let input1 = read_line();
    if (!is_float(input1)) {
        print("Error: Invalid number entered.");
        return 1;
    }
    let num1 = to_float(input1);

    print("Enter denominator: ");
    let input2 = read_line();
    if (!is_float(input2)) {
        print("Error: Invalid number entered.");
        return 1;
    }
    let num2 = to_float(input2);

    if (num2 == 0.0) {
        print("Error: Division by zero is not allowed.");
    } else {
        let result = num1 / num2;
        print(f"result: {num1} / {num2} = {result}");
    }
    return 0;
}
```

**Sample run (Valid Division):**
```text
$ ./03_division
Enter numerator: 
100
Enter denominator: 
4
result: 100 / 4 = 25
```

**Sample run (Division by Zero):**
```text
$ ./03_division
Enter numerator: 
50
Enter denominator: 
0
Error: Division by zero is not allowed.
```

> **Note**: Conditional `if / else` branches in Cco use standard boolean expressions without requiring parenthesis surrounding the block bodies.

---

## Program 4: Area of a Triangle

Read the base and height of a triangle from the user, compute the area using `0.5 * base * height`, and print the result.

```cco
// docs/guide/04_triangle_area.cco
fn main() -> int {
    print("Enter base of the triangle: ");
    let b_str = read_line();
    if (!is_float(b_str)) {
        print("Error: Invalid base value.");
        return 1;
    }
    let base = to_float(b_str);

    print("Enter height of the triangle: ");
    let h_str = read_line();
    if (!is_float(h_str)) {
        print("Error: Invalid height value.");
        return 1;
    }
    let height = to_float(h_str);

    let area = 0.5 * base * height;
    print(f"The area of the triangle is: {area}");
    return 0;
}
```

**Sample run:**
```text
$ ./04_triangle_area
Enter base of the triangle: 
8.0
Enter height of the triangle: 
5.0
The area of the triangle is: 20
```

> **Note**: Cco evaluates floating-point expressions with double precision IEEE 754 standards, matching C's raw mathematical speed.

---

## Program 5: Swapping Two Variables

Read two values and swap them using a temporary variable.

```cco
// docs/guide/05_swap.cco
fn main() -> int {
    print("Enter value for a: ");
    let a_str = read_line();
    if (!is_float(a_str)) {
        print("Error: Invalid number.");
        return 1;
    }
    let a = to_float(a_str);

    print("Enter value for b: ");
    let b_str = read_line();
    if (!is_float(b_str)) {
        print("Error: Invalid number.");
        return 1;
    }
    let b = to_float(b_str);

    print(f"Before swap: a = {a}, b = {b}");

    let temp = a;
    a = b;
    b = temp;

    print(f"After swap: a = {a}, b = {b}");
    return 0;
}
```

**Sample run:**
```text
$ ./05_swap
Enter value for a: 
10
Enter value for b: 
20
Before swap: a = 10, b = 20
After swap: a = 20, b = 10
```

> **Note**: Unlike Python's tuple-unpacking idiom (`a, b = b, a`), Cco uses an explicit temporary variable swap (`let temp = a; a = b; b = temp;`), emphasizing straightforward and inspectable low-level execution.

---

## Program 6: Generating Random Numbers

Initialize a pseudo-random number generator and generate a random integer within a specified range.

```cco
// docs/guide/06_random.cco
fn main() -> int {
    random_seed(42);
    let secret = random_int(1, 100);
    print(f"Random number between 1 and 100: {secret}");
    return 0;
}
```

**Sample run:**
```text
$ ./06_random
Random number between 1 and 100: 67
```

> **Note**: `random_seed(seed)` sets a reproducible starting state, while `random_int(min, max)` produces an inclusive random integer in that interval.

---

## Program 7: Converting Kilometers to Miles

Convert a user-provided distance from kilometers to miles using the conversion ratio `0.621371`.

```cco
// docs/guide/07_km_to_miles.cco
fn main() -> int {
    print("Enter distance in kilometers: ");
    let km_str = read_line();
    if (!is_float(km_str)) {
        print("Error: Invalid kilometer value.");
        return 1;
    }
    let km = to_float(km_str);
    let miles = km * 0.621371;

    print(f"{km} kilometers is equal to {miles} miles");
    return 0;
}
```

**Sample run:**
```text
$ ./07_km_to_miles
Enter distance in kilometers: 
10.0
10 kilometers is equal to 6.21371 miles
```

> **Note**: Type inference with `let` allows writing mathematical scripts with zero type clutter while maintaining static compiler optimizations.

---

## Program 8: Converting Celsius to Fahrenheit

Convert temperature from Celsius to Fahrenheit using the formula `(c * 9/5) + 32`.

```cco
// docs/guide/08_celsius_to_fahrenheit.cco
fn main() -> int {
    print("Enter temperature in Celsius: ");
    let c_str = read_line();
    if (!is_float(c_str)) {
        print("Error: Invalid temperature value.");
        return 1;
    }
    let celsius = to_float(c_str);
    let fahrenheit = (celsius * 9.0 / 5.0) + 32.0;

    print(f"{celsius} Celsius is equal to {fahrenheit} Fahrenheit");
    return 0;
}
```

**Sample run:**
```text
$ ./08_celsius_to_fahrenheit
Enter temperature in Celsius: 
25.0
25 Celsius is equal to 77 Fahrenheit
```

---

## Program 9: Object-Oriented Programming with Classes and Methods

Define a `Point` class with mutable state and member methods.

```cco
// docs/guide/09_point_class.cco
class Point {
    x: float;
    y: float;

    fn translate(self, dx: float, dy: float) -> void {
        self.x += dx;
        self.y += dy;
    }

    fn display(self) -> void {
        print(f"Point coordinates: ({self.x}, {self.y})");
    }
}

fn main() -> int {
    let p = Point { x: 3.5, y: 7.0 };
    p.display();

    print("Translating point by dx = 1.5, dy = -2.0...");
    p.translate(1.5, -2.0);
    p.display();

    return 0;
}
```

**Sample run:**
```text
$ ./09_point_class
Point coordinates: (3.5, 7)
Translating point by dx = 1.5, dy = -2.0...
Point coordinates: (5, 5)
```

> **Note**: Unlike Python (where objects rely on a garbage collector), Cco's classes use **compile-time single ownership**. The object is automatically freed as soon as `p` goes out of scope—guaranteeing 0 memory leaks without runtime GC pauses.

---

## Program 10: Interactive Number-Guessing Game

A complete mini-game combining random numbers, input loops, conditional feedback, and attempt counters.

```cco
// docs/guide/10_number_guess.cco
fn main() -> int {
    random_seed(12345);
    let secret = random_int(1, 20);
    print("=== Guess the Number (between 1 and 20) ===");

    let attempts = 0;
    let won = false;

    while (!won && attempts < 5) {
        attempts++;
        print(f"Attempt #{attempts}: Enter your guess: ");
        let guess_str = read_line();

        if (!is_int(guess_str)) {
            print("Please enter a valid whole number!");
        } else {
            let guess = to_int(guess_str);
            if (guess == secret) {
                print(f"Congratulations! You guessed the number {secret} in {attempts} attempts!");
                won = true;
            } else if (guess < secret) {
                print("Too low! Try higher.");
            } else {
                print("Too high! Try lower.");
            }
        }
    }

    if (!won) {
        print(f"Game over! The secret number was {secret}.");
    }
    return 0;
}
```

**Sample run:**
```text
$ ./10_number_guess
=== Guess the Number (between 1 and 20) ===
Attempt #1: Enter your guess: 
10
Too low! Try higher.
Attempt #2: Enter your guess: 
15
Too low! Try higher.
Attempt #3: Enter your guess: 
20
Congratulations! You guessed the number 20 in 3 attempts!
```

---

## 🚀 Where to Go Next

Now that you've mastered the fundamentals of Cco, explore more advanced language features in the [`examples/`](file:///home/raccoon/cco-lang/examples) directory:

- **[Operator Overloading](file:///home/raccoon/cco-lang/examples/vec2_operators.cco)**: Learn how to overload `+`, `-`, `*`, `/`, `==`, `<`, `>`, `<=`, `>=` on value structs.
- **[Interfaces via Monomorphization](file:///home/raccoon/cco-lang/examples/printable_interface.cco)**: Zero-cost generic traits with static monomorphization.
- **[Tagged Unions & Pattern Matching](file:///home/raccoon/cco-lang/examples/expr_eval.cco)**: Type-safe algebraic data types (`enum`) and exhaustive `match` expressions.
- **[Hash Maps & Dynamic Lists](file:///home/raccoon/cco-lang/examples/word_frequency.cco)**: Using `map[string]int`, `map_new()`, and growable arrays.
- **[Multi-File Modules](file:///home/raccoon/cco-lang/examples/10_import_demo)**: Organizing projects using `import "module.cco";`.
