# Part 7: C++ to Cco Architectural Comparisons (Streams, OOP, Namespaces, Flow)

[← Back to Guide Index](README.md)

---

# 66. C++ vs Cco: Streams, Auto Types & String Formatting

## 218. C++ `std::cout` vs Cco `print()`
Comparing C++ `<iostream>` console stream pipelines with Cco native `print()`.

```cco
// codebase/218_cpp_hello_world_cout.cco
fn main() -> int {
    print("Hello World from C++ & Cco!");
    return 0;
}
```
**Run:** `./cco codebase/218_cpp_hello_world_cout.cco --run`  
**Output:**
```text
Hello World from C++ & Cco!
```

---

## 219. C++ `auto` Deduction vs Cco `let`
Modern C++ `auto` type deduction compared with Cco compile-time type deduction.

```cco
// codebase/219_cpp_variables_and_auto_types.cco
fn main() -> int {
    let age = 25;
    let salary = 75000.50;
    let name = "Alice";
    let pi: float = 3.14159;

    print("=== C++ Auto Types & Constants vs Cco ===");
    print(f"Employee: {name}");
    print(f"Age: {age}");
    print(f"Salary: {salary}");
    print(f"Constant Pi: {pi}");

    return 0;
}
```
**Run:** `./cco codebase/219_cpp_variables_and_auto_types.cco --run`  
**Output:**
```text
=== C++ Auto Types & Constants vs Cco ===
Employee: Alice
Age: 25
Salary: 75000.5
Constant Pi: 3.14159
```

---

## 220. C++20 `std::format` vs Cco f-strings
Comparing C++20 `<format>` syntax with native Cco f-string interpolation.

```cco
// codebase/220_cpp_strings_and_formatting.cco
fn main() -> int {
    let product = "Laptop";
    let price = 999.99;
    let stock = 15;

    print("=== C++ std::string & std::format vs Cco f-strings ===");
    print(f"Product: {product}");
    print(f"Price: ${price}");
    print(f"Stock Remaining: {stock} units");
    print(f"Inventory Summary: {product} costs ${price} with {stock} units in stock.");

    return 0;
}
```
**Run:** `./cco codebase/220_cpp_strings_and_formatting.cco --run`  
**Output:**
```text
=== C++ std::string & std::format vs Cco f-strings ===
Product: Laptop
Price: $999.99
Stock Remaining: 15 units
Inventory Summary: Laptop costs $999.99 with 15 units in stock.
```

---


---

# 67. C++ Introduction: The Leap from Procedural to Object-Oriented

## 221. Object-Oriented Paradigm: Classes & Methods
Uniting state (fields) and behaviors (methods) in a unified class construct.

```cco
// codebase/221_cpp_intro_procedural_vs_oop.cco
class Car {
    brand: string;
    model: string;
    year: int;
    speed: int;

    fn accelerate(self, increment: int) -> void {
        self.speed += increment;
        print(f"Accelerating: {self.brand} {self.model} speed is now {self.speed} km/h");
    }

    fn brake(self, decrement: int) -> void {
        self.speed -= decrement;
        if (self.speed < 0) {
            self.speed = 0;
        }
        print(f"Braking: {self.brand} {self.model} speed slowed to {self.speed} km/h");
    }
}

fn main() -> int {
    print("=== C++ & Cco: The Object-Oriented Paradigm ===");

    let my_car: Car = Car {
        brand: "Tesla",
        model: "Model 3",
        year: 2024,
        speed: 0
    };

    print(f"Car Created: {my_car.year} {my_car.brand} {my_car.model}");
    my_car.accelerate(50);
    my_car.accelerate(30);
    my_car.brake(40);

    return 0;
}
```
**Run:** `./cco codebase/221_cpp_intro_procedural_vs_oop.cco --run`  
**Output:**
```text
=== C++ & Cco: The Object-Oriented Paradigm ===
Car Created: 2024 Tesla Model 3
Accelerating: Tesla Model 3 speed is now 50 km/h
Accelerating: Tesla Model 3 speed is now 80 km/h
Braking: Tesla Model 3 speed slowed to 40 km/h
```

---


---

# 68. C++ Getting Started & First Program

## 222. C++ Quickstart vs Cco Quickstart
First program comparison: `cout << "Hello World!"` vs `print("Hello World!")`.

```cco
// codebase/222_cpp_get_started_first_program.cco
fn main() -> int {
    print("Hello World!");
    return 0;
}
```
**Run:** `./cco codebase/222_cpp_get_started_first_program.cco --run`  
**Output:**
```text
Hello World!
```

---


---

# 69. C++ Syntax Anatomy & Namespace Scoping

## 223. C++ Syntax Breakdown vs Cco
Comparing `#include <iostream>` and `using namespace std` with Cco native anatomy.

```cco
// codebase/223_cpp_syntax_anatomy_breakdown.cco
fn main() -> int {
    print("=== C++ Syntax Anatomy vs Cco ===");
    print("Line 1: fn main() -> int defines the entry point.");
    print("Line 2: print() outputs text with an automatic newline.");
    print("Line 3: return 0 signals successful execution to the OS.");

    return 0;
}
```
**Run:** `./cco codebase/223_cpp_syntax_anatomy_breakdown.cco --run`  
**Output:**
```text
=== C++ Syntax Anatomy vs Cco ===
Line 1: fn main() -> int defines the entry point.
Line 2: print() outputs text with an automatic newline.
Line 3: return 0 signals successful execution to the OS.
```

---

## 224. C++ Namespaces vs Cco Global & Module Scope
In C++, namespaces (`namespace Math { ... }`) provide zero-overhead compile-time naming scopes without creating objects or state.

> **Honest Language Note**: Cco does not currently have `namespace` keyword blocks or `::` scope resolution. Instead, Cco uses a single, flat global namespace across files combined with whole-program AST module imports (`import "...";`). Functions are cleanly organized via modular files and descriptive prefixes (e.g. `math_add()`), reserving classes only for cases where instance state is actually needed.

```cco
// codebase/224_cpp_namespace_scoping_concept.cco
// In Cco, free functions live in a clean global namespace without C++ `namespace` blocks.
// Symbol organization across files is achieved using `import` and clear naming conventions.

fn math_add(a: int, b: int) -> int {
    return a + b;
}

fn math_multiply(a: int, b: int) -> int {
    return a * b;
}

fn main() -> int {
    print("=== C++ Namespaces vs Cco Global & Module Scope ===");
    let sum = math_add(15, 25);
    let product = math_multiply(4, 5);

    print(f"math_add(15, 25) = {sum}");
    print(f"math_multiply(4, 5) = {product}");

    return 0;
}
```
**Run:** `./cco codebase/224_cpp_namespace_scoping_concept.cco --run`  
**Output:**
```text
=== C++ Namespaces vs Cco Global & Module Scope ===
math_add(15, 25) = 40
math_multiply(4, 5) = 20
```

---


---

# 70. C++ Statements & Sequential Flow

## 225. Sequential Statement Execution
Top-to-bottom instruction execution order and semicolon termination.

```cco
// codebase/225_cpp_sequential_statements.cco
fn main() -> int {
    // Statement 1: First instruction
    print("Hello World!");

    // Statement 2: Second instruction
    print("Have a good day!");

    // Statement 3: Variable computation instruction
    let total_steps = 3;
    print(f"Executed {total_steps} statements in sequence.");

    // Statement 4: Exit statement
    return 0;
}
```
**Run:** `./cco codebase/225_cpp_sequential_statements.cco --run`  
**Output:**
```text
Hello World!
Have a good day!
Executed 3 statements in sequence.
```

---


---


[← Back to Guide Index](README.md)
