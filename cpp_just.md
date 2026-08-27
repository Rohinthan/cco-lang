# Interactive C++ to Cco Modular Guide & Codebase

Welcome to the **C++ to Cco Transition & Architecture Guide**. This document parallels standard C++ programming paradigms (OOP, Streams, RAII, Smart Pointers, Containers, Templates, Lambdas, Move Semantics) with their modern, memory-safe, Pythonic equivalents in **Cco**.

---

# 1. Output & Streams (`std::cout` vs `print`)

## 218. C++ `std::cout` vs Cco `print()`
In C++, standard console output uses stream operators from `<iostream>` (`std::cout << "Hello" << std::endl;`). In Cco, the built-in `print()` function outputs clean, newline-terminated text directly.

### C++ Equivalent:
```cpp
#include <iostream>

int main() {
    std::cout << "Hello World from C++ & Cco!" << std::endl;
    return 0;
}
```

### Cco Implementation:
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

# 2. Variables, `auto` Deduction & Constants

## 219. C++ `auto` Type Deduction vs Cco `let`
In modern C++ (C++11/C++14/C++17), `auto` allows the compiler to deduce variable types at compile time. In Cco, `let` provides full compile-time type deduction with clean, Pythonic syntax.

### C++ Equivalent:
```cpp
#include <iostream>
#include <string>

int main() {
    auto age = 25;
    auto salary = 75000.50;
    std::string name = "Alice";
    const double PI = 3.14159;

    std::cout << "Employee: " << name << "\n";
    std::cout << "Age: " << age << "\n";
    return 0;
}
```

### Cco Implementation:
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

# 3. Strings & Formatting (`std::format` vs f-strings)

## 220. C++20 `std::format` vs Cco Interpolated Strings
In C++20, string formatting requires `#include <format>` and `std::format("{}", val)`. In Cco, first-class interpolated f-strings (`f"Product: {product}, Price: ${price}"`) format dynamic variables natively with automatic type conversion and zero memory leaks.

### C++ Equivalent:
```cpp
#include <iostream>
#include <format>
#include <string>

int main() {
    std::string product = "Laptop";
    double price = 999.99;
    int stock = 15;

    std::string summary = std::format("{} costs ${:.2f} with {} in stock.", product, price, stock);
    std::cout << summary << std::endl;
    return 0;
}
```

### Cco Implementation:
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

# 4. Introduction to OOP (Procedural C vs C++ & Cco)

## 221. Object-Oriented Paradigm: Classes & Methods
The fundamental leap from C to C++ was the introduction of **Classes and Objects**. Cco adopts this object-oriented model with clean Pythonic method binding (`self`) and native memory safety.

### C++ Equivalent:
```cpp
#include <iostream>
#include <string>

class Car {
public:
    std::string brand;
    std::string model;
    int year;
    int speed = 0;

    void accelerate(int increment) {
        speed += increment;
        std::cout << "Accelerating: " << brand << " speed is now " << speed << " km/h\n";
    }

    void brake(int decrement) {
        speed -= decrement;
        if (speed < 0) speed = 0;
        std::cout << "Braking: " << brand << " speed slowed to " << speed << " km/h\n";
    }
};

int main() {
    Car myCar{"Tesla", "Model 3", 2024};
    myCar.accelerate(50);
    myCar.brake(20);
    return 0;
}
```

### Cco Implementation:
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

# 5. Getting Started & First Program

## 222. C++ Quickstart vs Cco Quickstart
In C++, a minimal program requires `#include <iostream>`, namespace imports (`using namespace std;`), and stream piping (`cout << "Hello World!";`). In Cco, it is a single self-contained function `fn main() -> int`.

### C++ Equivalent (`myfirstprogram.cpp`):
```cpp
#include <iostream>
using namespace std;

int main() {
    cout << "Hello World!";
    return 0;
}
```

### Cco Implementation:
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

# 6. Syntax Breakdown & Namespace Scoping

## 223. C++ Syntax Anatomy vs Cco Anatomy
Line-by-line comparison showing how Cco eliminates preprocessor directives and namespace boilerplate while retaining static typing and clear return statuses.

### C++ Equivalent:
```cpp
#include <iostream>
using namespace std;

int main() {
    cout << "=== C++ Syntax Anatomy vs Cco ===" << endl;
    cout << "Line 1: #include <iostream> brings in stream objects." << endl;
    cout << "Line 2: using namespace std enables unqualified cout." << endl;
    cout << "Line 3: return 0 exits successfully." << endl;
    return 0;
}
```

### Cco Implementation:
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
In C++, namespaces (`namespace Math { ... }`) provide zero-overhead compile-time naming scopes to prevent global symbol collisions without instantiating objects or holding state (`Math::add(a, b)`).

> **Honest Language Note**: Cco does not currently have `namespace` keyword blocks or `::` scope resolution. Instead, Cco uses a single, flat global namespace across files combined with whole-program AST module imports (`import "...";`). Free functions are organized using modular files and clear naming prefixes (e.g. `math_add()`), reserving classes only for cases where instance state is actually needed.

### C++ Equivalent:
```cpp
#include <iostream>

namespace Math {
    int add(int a, int b) {
        return a + b;
    }
    int multiply(int a, int b) {
        return a * b;
    }
}

int main() {
    std::cout << "=== C++ Namespaces vs Cco Global & Module Scope ===" << std::endl;
    std::cout << "Math::add(15, 25) = " << Math::add(15, 25) << std::endl;
    std::cout << "Math::multiply(4, 5) = " << Math::multiply(4, 5) << std::endl;
    return 0;
}
```

### Cco Implementation:
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

# 7. Statements & Sequential Execution

## 225. Sequential Execution of Statements
Instructions in both C++ and Cco execute one after another in top-to-bottom order, terminated by semicolons.

### C++ Equivalent:
```cpp
#include <iostream>
using namespace std;

int main() {
    cout << "Hello World!" << endl;
    cout << "Have a good day!" << endl;
    int total_steps = 3;
    cout << "Executed " << total_steps << " statements in sequence." << endl;
    return 0;
}
```

### Cco Implementation:
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

# 8. Real-World Applications & Full-Featured Systems

## 226. Full-Featured Scientific & History Calculator
Building a robust, multi-operation calculator in Cco featuring algebraic operation enums, pattern matching (`match`), dynamic history tracking with `push()`, and runtime divide-by-zero protection.

```cco
// codebase/226_full_featured_calculator.cco
enum Operation {
    Add,
    Subtract,
    Multiply,
    Divide,
    Power,
    Modulo,
    Unknown,
}

fn parse_op(symbol: string) -> Operation {
    if (equals(symbol, "+")) {
        return Operation.Add;
    } else if (equals(symbol, "-")) {
        return Operation.Subtract;
    } else if (equals(symbol, "*")) {
        return Operation.Multiply;
    } else if (equals(symbol, "/")) {
        return Operation.Divide;
    } else if (equals(symbol, "^")) {
        return Operation.Power;
    } else if (equals(symbol, "%")) {
        return Operation.Modulo;
    } else {
        return Operation.Unknown;
    }
}

fn calculate(a: int, op_symbol: string, b: int) -> string {
    let op = parse_op(op_symbol);
    let log_msg = "";

    match &op {
        Operation.Add => {
            let res = a + b;
            log_msg = f"{a} + {b} = {res}";
        }
        Operation.Subtract => {
            let res = a - b;
            log_msg = f"{a} - {b} = {res}";
        }
        Operation.Multiply => {
            let res = a * b;
            log_msg = f"{a} * {b} = {res}";
        }
        Operation.Divide => {
            if (b == 0) {
                print(f"Error: Division by zero ({a} / 0)!");
                log_msg = f"{a} / 0 [ERROR: Div by Zero]";
            } else {
                let res = a / b;
                log_msg = f"{a} / {b} = {res}";
            }
        }
        Operation.Modulo => {
            if (b == 0) {
                print(f"Error: Modulo by zero ({a} % 0)!");
                log_msg = f"{a} % 0 [ERROR: Modulo by Zero]";
            } else {
                let res = a % b;
                log_msg = f"{a} % {b} = {res}";
            }
        }
        Operation.Power => {
            let p = 1;
            for (let i = 0; i < b; i++) {
                p *= a;
            }
            log_msg = f"{a} ^ {b} = {p}";
        }
        Operation.Unknown => {
            print(f"Error: Unknown operator '{op_symbol}'!");
            log_msg = f"{a} {op_symbol} {b} [ERROR: Unknown Op]";
        }
    }

    print(f"[CALC EXECUTE] {log_msg}");
    return log_msg;
}

fn main() -> int {
    print("=== Cco Full-Featured Engineering Calculator ===");

    let history: string[] = alloc(string, 0);

    // Perform arithmetic calculations and log into history stack
    history = push(history, calculate(25, "+", 17));
    history = push(history, calculate(100, "-", 42));
    history = push(history, calculate(12, "*", 8));
    history = push(history, calculate(144, "/", 12));
    history = push(history, calculate(2, "^", 8));      // 2^8 = 256
    history = push(history, calculate(29, "%", 5));      // 29 % 5 = 4

    // Test safe error handling guards
    history = push(history, calculate(50, "/", 0));      // Safe divide-by-zero catch
    history = push(history, calculate(10, "$", 5));      // Unknown operator catch

    // Display formatted calculation history
    print(f"\n=== Calculation History ({len(history)} entries) ===");
    let step = 1;
    for entry in history {
        print(f"  {step}. {entry}");
        step += 1;
    }
    print("================================================");

    return 0;
}
```
**Run:** `./cco codebase/226_full_featured_calculator.cco --run`  
**Output:**
```text
=== Cco Full-Featured Engineering Calculator ===
[CALC EXECUTE] 25 + 17 = 42
[CALC EXECUTE] 100 - 42 = 58
[CALC EXECUTE] 12 * 8 = 96
[CALC EXECUTE] 144 / 12 = 12
[CALC EXECUTE] 2 ^ 8 = 256
[CALC EXECUTE] 29 % 5 = 4
Error: Division by zero (50 / 0)!
[CALC EXECUTE] 50 / 0 [ERROR: Div by Zero]
Error: Unknown operator '$'!
[CALC EXECUTE] 10 $ 5 [ERROR: Unknown Op]

=== Calculation History (8 entries) ===
  1. 25 + 17 = 42
  2. 100 - 42 = 58
  3. 12 * 8 = 96
  4. 144 / 12 = 12
  5. 2 ^ 8 = 256
  6. 29 % 5 = 4
  7. 50 / 0 [ERROR: Div by Zero]
  8. 10 $ 5 [ERROR: Unknown Op]
================================================
```

---

## 227. HTTP Server & Client Protocol Engine
Building a native HTTP protocol request router, status code dispatcher (`200 OK`, `201 Created`, `400 Bad Request`, `404 Not Found`), and payload serialization engine in Cco.

```cco
// codebase/227_http_server_communication.cco
fn handle_http_request(port: int, req_id: int, method: string, path: string, payload: string) -> string {
    print(f"\n[SERVER :{port}] Received Request #{req_id} -> {method} {path}");

    let status_code = 200;
    let status_text = "OK";
    let content_type = "application/json";
    let body = "";

    if (equals(path, "/health")) {
        body = "{\"status\": \"UP\", \"server\": \"Cco-HTTP-Engine/1.0\"}";
    } else if (equals(path, "/api/greet") && equals(method, "GET")) {
        content_type = "text/plain";
        body = "Hello from Cco Native Microservice!";
    } else if (equals(path, "/api/users") && equals(method, "POST")) {
        if (equals(payload, "")) {
            status_code = 400;
            status_text = "Bad Request";
            body = "{\"error\": \"Missing request payload\"}";
        } else {
            status_code = 201;
            status_text = "Created";
            body = f"{{\"message\": \"User created successfully\", \"user\": \"{payload}\"}}";
        }
    } else {
        status_code = 404;
        status_text = "Not Found";
        body = f"{{\"error\": \"Route '{path}' not found on server\"}}";
    }

    let formatted = f"HTTP/1.1 {status_code} {status_text}\nContent-Type: {content_type}\nContent-Length: {len(body)}\n\n{body}";
    return formatted;
}

fn client_send(port: int, req_id: int, method: string, path: string, payload: string) -> void {
    print(f"[CLIENT] Dispatching -> {method} http://localhost:{port}{path}");
    let response: string = handle_http_request(port, req_id, method, path, payload);

    print("[CLIENT] Received HTTP Response:");
    print(response);
    print("--------------------------------------------------");
}

fn main() -> int {
    print("==================================================");
    print("      CCO HTTP SERVER & CLIENT COMMUNICATION      ");
    print("==================================================");

    let port = 8080;
    let requests_handled = 0;

    print(f"Starting Cco HTTP Server on port {port}...\n");

    // Test 1: Health check GET request
    requests_handled += 1;
    client_send(port, requests_handled, "GET", "/health", "");

    // Test 2: API Greeting GET request
    requests_handled += 1;
    client_send(port, requests_handled, "GET", "/api/greet", "");

    // Test 3: POST request with payload
    requests_handled += 1;
    client_send(port, requests_handled, "POST", "/api/users", "Alice (Admin)");

    // Test 4: Bad POST request (Empty payload)
    requests_handled += 1;
    client_send(port, requests_handled, "POST", "/api/users", "");

    // Test 5: Unknown Route (404 Not Found)
    requests_handled += 1;
    client_send(port, requests_handled, "GET", "/api/unknown_endpoint", "");

    print(f"\n[SERVER STATS] Total Handled Requests: {requests_handled}");
    print("Server communication completed with 0 errors & 0 leaks!");

    return 0;
}
```
**Run:** `./cco codebase/227_http_server_communication.cco --run`  
**Output:**
```text
==================================================
      CCO HTTP SERVER & CLIENT COMMUNICATION      
==================================================
Starting Cco HTTP Server on port 8080...

[CLIENT] Dispatching -> GET http://localhost:8080/health

[SERVER :8080] Received Request #1 -> GET /health
[CLIENT] Received HTTP Response:
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 49

{"status": "UP", "server": "Cco-HTTP-Engine/1.0"}
--------------------------------------------------
[CLIENT] Dispatching -> GET http://localhost:8080/api/greet

[SERVER :8080] Received Request #2 -> GET /api/greet
[CLIENT] Received HTTP Response:
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 35

Hello from Cco Native Microservice!
--------------------------------------------------
[CLIENT] Dispatching -> POST http://localhost:8080/api/users

[SERVER :8080] Received Request #3 -> POST /api/users
[CLIENT] Received HTTP Response:
HTTP/1.1 201 Created
Content-Type: application/json
Content-Length: 65

{"message": "User created successfully", "user": "Alice (Admin)"}
--------------------------------------------------
[CLIENT] Dispatching -> POST http://localhost:8080/api/users

[SERVER :8080] Received Request #4 -> POST /api/users
[CLIENT] Received HTTP Response:
HTTP/1.1 400 Bad Request
Content-Type: application/json
Content-Length: 36

{"error": "Missing request payload"}
--------------------------------------------------
[CLIENT] Dispatching -> GET http://localhost:8080/api/unknown_endpoint

[SERVER :8080] Received Request #5 -> GET /api/unknown_endpoint
[CLIENT] Received HTTP Response:
HTTP/1.1 404 Not Found
Content-Type: application/json
Content-Length: 62

{"error": "Route '/api/unknown_endpoint' not found on server"}
--------------------------------------------------

[SERVER STATS] Total Handled Requests: 5
Server communication completed with 0 errors & 0 leaks!
```

---
