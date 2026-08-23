# Part 5: Data Modeling & Dynamic Memory (Files, Structs, RAII, Dynamic Lists)

[← Back to Guide Index](README.md)

---

# 38. Safe File I/O (Create, Write, and Read)

## 163. Creating & Writing Files (`write_file()`)
Creating a new file and writing textual contents safely with automatic descriptor management and flush.

```cco
// codebase/163_file_create_write.cco
fn main() -> int {
    let filename = "build/atomic/sample.txt";
    let content = "Hello from Cco File I/O!\nThis file was created safely.";

    print("=== Creating & Writing to File ===");
    let ok = write_file(filename, content);
    print(f"File write success: {ok}");

    return 0;
}
```
**Run:** `./cco codebase/163_file_create_write.cco --run`  
**Output:**
```text
=== Creating & Writing to File ===
File write success: true
```

---

## 164. Reading File Contents (`read_file()`)
Reading the complete contents of a file into a managed string.

```cco
// codebase/164_file_read_content.cco
fn main() -> int {
    let filename = "build/atomic/sample.txt";

    print("=== Reading from File ===");
    let text = read_file(filename);
    print(f"File contents:\n{text}");

    return 0;
}
```
**Run:** `./cco codebase/164_file_read_content.cco --run`  
**Output:**
```text
=== Reading from File ===
File contents:
Hello from Cco File I/O!
This file was created safely.
```

---


---

# 39. Writing & Appending to Files

## 165. Overwriting Existing File Content
Writing to an existing file replaces the old contents with the new string payload.

```cco
// codebase/165_file_overwrite_content.cco
fn main() -> int {
    let filename = "build/atomic/write_demo.txt";

    // Step 1: Initial Write
    write_file(filename, "Some text");
    print(f"Initial: {read_file(filename)}");

    // Step 2: Overwrite existing content
    write_file(filename, "Hello World!");
    print(f"After overwrite: {read_file(filename)}");

    return 0;
}
```
**Run:** `./cco codebase/165_file_overwrite_content.cco --run`  
**Output:**
```text
Initial: Some text
After overwrite: Hello World!
```

---

## 166. Appending Content to Existing Files
Preserving existing file content while adding new text at the end.

```cco
// codebase/166_file_append_content.cco
fn main() -> int {
    let filename = "build/atomic/append_demo.txt";

    // Step 1: Write initial line
    write_file(filename, "Initial Line 1");

    // Step 2: Append new line to existing content
    let existing = read_file(filename);
    let updated = concat(existing, "\nHi everybody!");
    write_file(filename, updated);

    print("=== Appended File Output ===");
    print(read_file(filename));

    return 0;
}
```
**Run:** `./cco codebase/166_file_append_content.cco --run`  
**Output:**
```text
=== Appended File Output ===
Initial Line 1
Hi everybody!
```

---


---

# 40. Reading Files & Multiline Text

## 167. Reading Multiline Files
Reading entire multiline text files in a single clean expression without fixed buffer arrays or `while(fgets(...))` loops.

```cco
// codebase/167_file_read_multiline.cco
fn main() -> int {
    let filename = "build/atomic/multiline_test.txt";
    let initial_text = "Hello World!\nHi everybody!\nLine 3 of text.";
    write_file(filename, initial_text);

    print("=== Reading Multiline File ===");
    let contents = read_file(filename);
    print(contents);

    return 0;
}
```
**Run:** `./cco codebase/167_file_read_multiline.cco --run`  
**Output:**
```text
=== Reading Multiline File ===
Hello World!
Hi everybody!
Line 3 of text.
```

---

## 168. Reading & Verifying File Content
Reading files and asserting content equality with `equals()`.

```cco
// codebase/168_file_read_verify.cco
fn main() -> int {
    let filename = "build/atomic/verify_test.txt";
    let expected = "First Line\nSecond Line";

    write_file(filename, expected);

    print("=== Reading & Verifying File ===");
    let actual = read_file(filename);

    if (equals(actual, expected)) {
        print("File content verified successfully!");
        print(f"Content:\n{actual}");
    } else {
        print("File content mismatch!");
    }

    return 0;
}
```
**Run:** `./cco codebase/168_file_read_verify.cco --run`  
**Output:**
```text
=== Reading & Verifying File ===
File content verified successfully!
Content:
First Line
Second Line
```

---


---

# 41. Structures & Data Modeling

## 169. Basic Struct Member Access & Mutation
Grouping related primitive fields into a stack-allocated `struct` and accessing/modifying members via dot notation (`s1.my_num = 30`).

```cco
// codebase/169_struct_basic_members.cco
struct MyStructure {
    my_num: int;
    my_letter: char;
}

fn main() -> int {
    let s1: MyStructure = MyStructure {
        my_num: 13,
        my_letter: 'B'
    };

    print("=== Accessing Struct Members ===");
    print(f"My number: {s1.my_num}");
    print(f"My letter: {s1.my_letter}");

    // Modify members
    s1.my_num = 30;
    s1.my_letter = 'C';

    print("\n=== Modified Struct Members ===");
    print(f"My number: {s1.my_num}");
    print(f"My letter: {s1.my_letter}");

    return 0;
}
```
**Run:** `./cco codebase/169_struct_basic_members.cco --run`  
**Output:**
```text
=== Accessing Struct Members ===
My number: 13
My letter: B

=== Modified Struct Members ===
My number: 30
My letter: C
```

---

## 170. Value-Type Struct Copying
Copying struct values creates an independent clone where modifying `s2` does not affect `s1`.

```cco
// codebase/170_struct_value_copy.cco
struct MyStructure {
    my_num: int;
    my_letter: char;
}

fn main() -> int {
    let s1: MyStructure = MyStructure {
        my_num: 13,
        my_letter: 'B'
    };

    // Copy s1 to s2
    let s2: MyStructure = s1;

    // Change s2 values independently
    s2.my_num = 30;
    s2.my_letter = 'C';

    print("=== Independent Struct Copy ===");
    print(f"s1: {s1.my_num} {s1.my_letter}");
    print(f"s2: {s2.my_num} {s2.my_letter}");

    return 0;
}
```
**Run:** `./cco codebase/170_struct_value_copy.cco --run`  
**Output:**
```text
=== Independent Struct Copy ===
s1: 13 B
s2: 30 C
```

---

## 171. Real-Life Car Data Records
Modeling real-world domain records (`brand`, `model`, `year`) using first-class managed classes without `strcpy` boilerplate.

```cco
// codebase/171_class_car_template.cco
class Car {
    brand: string;
    model: string;
    year: int;
}

fn main() -> int {
    let car1: Car = Car { brand: "BMW", model: "X5", year: 1999 };
    let car2: Car = Car { brand: "Ford", model: "Mustang", year: 1969 };
    let car3: Car = Car { brand: "Toyota", model: "Corolla", year: 2011 };

    print("=== Real-Life Car Records ===");
    print(f"{car1.brand} {car1.model} {car1.year}");
    print(f"{car2.brand} {car2.model} {car2.year}");
    print(f"{car3.brand} {car3.model} {car3.year}");

    return 0;
}
```
**Run:** `./cco codebase/171_class_car_template.cco --run`  
**Output:**
```text
=== Real-Life Car Records ===
BMW X5 1999
Ford Mustang 1969
Toyota Corolla 2011
```

---


---

# 42. Nested Structures & Hierarchical Data

## 172. Nested Object Models (`Car` and `Owner`)
Modeling layered entities with nested object properties and accessing deep members with chained dot notation (`car1.owner.first_name`).

```cco
// codebase/172_nested_structures_car_owner.cco
class Owner {
    first_name: string;
    last_name: string;
}

class Car {
    brand: string;
    year: int;
    owner: Owner;
}

fn main() -> int {
    let car1: Car = Car {
        brand: "Toyota",
        year: 2010,
        owner: Owner {
            first_name: "John",
            last_name: "Doe"
        }
    };

    print("=== Nested Data Structure ===");
    print(f"Car: {car1.brand} ({car1.year})");
    print(f"Owner: {car1.owner.first_name} {car1.owner.last_name}");

    return 0;
}
```
**Run:** `./cco codebase/172_nested_structures_car_owner.cco --run`  
**Output:**
```text
=== Nested Data Structure ===
Car: Toyota (2010)
Owner: John Doe
```

---


---

# 43. Structures, References, & Mutation

## 173. In-Place Struct Mutation via Borrowed Reference (`&Car`)
Passing a borrowed reference into a function to mutate object fields directly without copying large structs.

```cco
// codebase/173_struct_pointer_reference_function.cco
class Car {
    brand: string;
    year: int;
}

fn update_year(c: &Car, new_year: int) -> void {
    c.year = new_year;
}

fn main() -> int {
    let my_car: Car = Car {
        brand: "Toyota",
        year: 2020
    };

    print("=== Before In-Place Update ===");
    print(f"Brand: {my_car.brand}");
    print(f"Year: {my_car.year}");

    // Pass borrowed reference to mutate in-place without copying
    update_year(&my_car, 2025);

    print("\n=== After In-Place Update ===");
    print(f"Brand: {my_car.brand}");
    print(f"Year: {my_car.year}");

    return 0;
}
```
**Run:** `./cco codebase/173_struct_pointer_reference_function.cco --run`  
**Output:**
```text
=== Before In-Place Update ===
Brand: Toyota
Year: 2020

=== After In-Place Update ===
Brand: Toyota
Year: 2025
```

---

## 174. In-Place Method Mutation via `self`
Encapsulating field modifications directly within methods using `self`.

```cco
// codebase/174_struct_pointer_method_mutation.cco
class Car {
    brand: string;
    year: int;

    fn set_year(self, new_year: int) -> void {
        self.year = new_year;
    }
}

fn main() -> int {
    let my_car: Car = Car {
        brand: "Toyota",
        year: 2020
    };

    print("=== Method In-Place Update ===");
    print(f"Initial: {my_car.brand} ({my_car.year})");

    my_car.set_year(2025);
    print(f"Updated: {my_car.brand} ({my_car.year})");

    return 0;
}
```
**Run:** `./cco codebase/174_struct_pointer_method_mutation.cco --run`  
**Output:**
```text
=== Method In-Place Update ===
Initial: Toyota (2020)
Updated: Toyota (2025)
```

---


---

# 44. Unions & Tagged Algebraic Data Types

## 175. Tagged Union Variants (`enum DataValue`)
In C, untagged unions share memory but lack safety checks (reading the wrong field yields corrupted bytes). In Cco, Tagged Enums safely hold one of several possible variant types with compile-time safety.

```cco
// codebase/175_union_tagged_enum_variants.cco
enum DataValue {
    IntValue { val: int },
    LetterValue { val: char },
    StringValue { text: string },
}

fn print_data(d: &DataValue) -> void {
    match d {
        DataValue.IntValue { val } => {
            print(f"Integer payload: {val}");
        }
        DataValue.LetterValue { val } => {
            print(f"Character payload: {val}");
        }
        DataValue.StringValue { text } => {
            print(f"String payload: {text}");
        }
    }
}

fn main() -> int {
    let d1: DataValue = DataValue.IntValue { val: 1000 };
    let d2: DataValue = DataValue.LetterValue { val: 'A' };
    let d3: DataValue = DataValue.StringValue { text: "Hello Cco" };

    print("=== Tagged Union Payloads ===");
    print_data(&d1);
    print_data(&d2);
    print_data(&d3);

    return 0;
}
```
**Run:** `./cco codebase/175_union_tagged_enum_variants.cco --run`  
**Output:**
```text
=== Tagged Union Payloads ===
Integer payload: 1000
Character payload: 65
String payload: Hello Cco
```

---

## 176. Discriminated Union Pattern Matching
Extracting payloads safely using pattern matching across variant types.

```cco
// codebase/176_union_pattern_matching_dispatch.cco
enum Measurement {
    Length { meters: int },
    Temperature { celsius: float },
    Unitless,
}

fn display_measurement(m: &Measurement) -> void {
    match m {
        Measurement.Length { meters } => {
            print(f"Length: {meters} m");
        }
        Measurement.Temperature { celsius } => {
            print(f"Temperature: {celsius} C");
        }
        Measurement.Unitless => {
            print("Unitless measurement value");
        }
    }
}

fn main() -> int {
    let m1: Measurement = Measurement.Length { meters: 150 };
    let m2: Measurement = Measurement.Temperature { celsius: 36.6 };
    let m3: Measurement = Measurement.Unitless;

    print("=== Discriminated Union Pattern Match ===");
    display_measurement(&m1);
    display_measurement(&m2);
    display_measurement(&m3);

    return 0;
}
```
**Run:** `./cco codebase/176_union_pattern_matching_dispatch.cco --run`  
**Output:**
```text
=== Discriminated Union Pattern Match ===
Length: 150 m
Temperature: 36.6 C
Unitless measurement value
```

---


---

# 45. Type Aliases & First-Class Type Definitions

## 177. Semantic Domain Value Types (`struct Temperature`)
In C, developers use `typedef float Temperature;` to give semantic meaning to raw numbers. In Cco, dedicated zero-overhead structs encapsulate values with clear domain names and strong type safety.

```cco
// codebase/177_typedef_semantic_value_type.cco
struct Temperature {
    celsius: float;
}

fn print_temp(label: string, t: Temperature) -> void {
    print(f"{label}: {t.celsius} C");
}

fn main() -> int {
    let today: Temperature = Temperature { celsius: 25.5 };
    let tomorrow: Temperature = Temperature { celsius: 18.6 };

    print("=== Semantic Temperature Types ===");
    print_temp("Today", today);
    print_temp("Tomorrow", tomorrow);

    return 0;
}
```
**Run:** `./cco codebase/177_typedef_semantic_value_type.cco --run`  
**Output:**
```text
=== Semantic Temperature Types ===
Today: 25.5 C
Tomorrow: 18.6 C
```

---

## 178. First-Class Models Without `typedef` Boilerplate
In C, using `struct Car` requires either repeating `struct Car` or writing `typedef struct { ... } Car;`. In Cco, all `class`, `struct`, and `enum` declarations are first-class type names immediately.

```cco
// codebase/178_typedef_first_class_models.cco
class Car {
    brand: string;
    model: string;
    year: int;
}

fn print_car(car: &Car) -> void {
    print(f"{car.brand} {car.model} {car.year}");
}

fn main() -> int {
    let car1: Car = Car { brand: "BMW", model: "X5", year: 1999 };
    let car2: Car = Car { brand: "Ford", model: "Mustang", year: 1969 };
    let car3: Car = Car { brand: "Toyota", model: "Corolla", year: 2011 };

    print("=== First-Class Car Models ===");
    print_car(&car1);
    print_car(&car2);
    print_car(&car3);

    return 0;
}
```
**Run:** `./cco codebase/178_typedef_first_class_models.cco --run`  
**Output:**
```text
=== First-Class Car Models ===
BMW X5 1999
Ford Mustang 1969
Toyota Corolla 2011
```

---


---

# 46. Struct Alignment, Hardware Padding, & Layout Optimization

## 179. Interleaved Field Layout (Unoptimized Padding)
When fields are interleaved (`char`, `int`, `char`), the CPU alignment requires 3 bytes of padding between `a` and `b` plus 3 bytes trailing padding (total 12 bytes).

```cco
// codebase/179_struct_alignment_unoptimized.cco
struct UnoptimizedData {
    a: char;
    b: int;
    c: char;
}

fn main() -> int {
    let data: UnoptimizedData = UnoptimizedData {
        a: 'X',
        b: 42,
        c: 'Y'
    };

    print("=== Unoptimized Field Alignment Struct ===");
    print(f"a: {data.a}");
    print(f"b: {data.b}");
    print(f"c: {data.c}");

    return 0;
}
```
**Run:** `./cco codebase/179_struct_alignment_unoptimized.cco --run`  
**Output:**
```text
=== Unoptimized Field Alignment Struct ===
a: X
b: 42
c: Y
```

---

## 180. Ordered Field Layout (Reduced Padding)
Grouping larger 4-byte types first (`int`, `char`, `char`) packs fields together and reduces total memory footprint to 8 bytes.

```cco
// codebase/180_struct_alignment_optimized.cco
struct OptimizedData {
    b: int;
    a: char;
    c: char;
}

fn main() -> int {
    let data: OptimizedData = OptimizedData {
        b: 42,
        a: 'X',
        c: 'Y'
    };

    print("=== Optimized Field Ordering Struct ===");
    print(f"b: {data.b}");
    print(f"a: {data.a}");
    print(f"c: {data.c}");

    return 0;
}
```
**Run:** `./cco codebase/180_struct_alignment_optimized.cco --run`  
**Output:**
```text
=== Optimized Field Ordering Struct ===
b: 42
a: X
c: Y
```

---


---

# 47. Enumerations & Strongly-Typed Constants

## 181. Enum Definition & Pattern Matching Dispatch
In C, enums are weak integer aliases requiring manual `break` switches. In Cco, enums are strongly-typed unit variants checked exhaustively with `match`.

```cco
// codebase/181_enum_level_match.cco
enum Level {
    Low,
    Medium,
    High,
}

fn check_level(lvl: &Level) -> void {
    match lvl {
        Level.Low => {
            print("Low Level");
        }
        Level.Medium => {
            print("Medium Level");
        }
        Level.High => {
            print("High Level");
        }
    }
}

fn main() -> int {
    let my_var: Level = Level.Medium;

    print("=== Enum Level Dispatch ===");
    check_level(&my_var);

    return 0;
}
```
**Run:** `./cco codebase/181_enum_level_match.cco --run`  
**Output:**
```text
=== Enum Level Dispatch ===
Medium Level
```

---

## 182. Day of Week Enumeration
Representing discrete weekday constants with named variants.

```cco
// codebase/182_enum_day_of_week.cco
enum Day {
    Mon,
    Tue,
    Wed,
    Thu,
    Fri,
    Sat,
    Sun,
}

fn describe_day(d: &Day) -> void {
    match d {
        Day.Wed => {
            print("It is Wednesday!");
        }
        Day.Sat => {
            print("It is the weekend!");
        }
        Day.Sun => {
            print("It is the weekend!");
        }
        Day.Mon => {
            print("It is a weekday.");
        }
        Day.Tue => {
            print("It is a weekday.");
        }
        Day.Thu => {
            print("It is a weekday.");
        }
        Day.Fri => {
            print("It is Friday!");
        }
    }
}

fn main() -> int {
    let today: Day = Day.Wed;

    print("=== Enum Day of Week ===");
    describe_day(&today);

    return 0;
}
```
**Run:** `./cco codebase/182_enum_day_of_week.cco --run`  
**Output:**
```text
=== Enum Day of Week ===
It is Wednesday!
```

---


---

# 48. Memory Management Fundamentals & RAII

## 183. Primitive Types & Value Representations
Handling fundamental data types (`int`, `float`, `char`, `bool`) with zero overhead.

```cco
// codebase/183_memory_primitive_sizes.cco
fn main() -> int {
    let my_int = 42;
    let my_float = 3.14159;
    let my_char = 'Z';
    let my_bool = true;

    print("=== Primitive Memory Values ===");
    print(f"Integer value: {my_int}");
    print(f"Float value: {my_float}");
    print(f"Char value: {my_char}");
    print(f"Bool value: {my_bool}");

    return 0;
}
```
**Run:** `./cco codebase/183_memory_primitive_sizes.cco --run`  
**Output:**
```text
=== Primitive Memory Values ===
Integer value: 42
Float value: 3.14159
Char value: Z
Bool value: true
```

---

## 184. Scope-Based Dynamic Lifecycles (RAII)
Dynamic heap allocations (`alloc(int, N)`) and reference objects (`SensorBuffer`) allocated inside a block are immediately and deterministically freed when execution leaves the block.

```cco
// codebase/184_memory_scope_lifecycles.cco
class SensorBuffer {
    name: string;
    reading: int;
}

fn main() -> int {
    print("=== Outer Scope Start ===");
    let outer_val = 100;

    // Inner scope with local dynamic allocations
    {
        print("--- Entering Inner Scope ---");
        let local_sensor: SensorBuffer = SensorBuffer {
            name: "Temperature Sensor A",
            reading: 75
        };
        let dynamic_arr: int[] = alloc(int, 4);
        dynamic_arr[0] = 10;
        dynamic_arr[1] = 20;
        dynamic_arr[2] = 30;
        dynamic_arr[3] = 40;

        print(f"Sensor: {local_sensor.name} -> {local_sensor.reading}");
        print(f"Buffer elements: {dynamic_arr[0]}, {dynamic_arr[1]}, {dynamic_arr[2]}, {dynamic_arr[3]}");
        print("--- Exiting Inner Scope (Memory Auto-Freed) ---");
    }

    print(f"=== Outer Scope End (outer_val={outer_val}) ===");
    return 0;
}
```
**Run:** `./cco codebase/184_memory_scope_lifecycles.cco --run`  
**Output:**
```text
=== Outer Scope Start ===
--- Entering Inner Scope ---
Sensor: Temperature Sensor A -> 75
Buffer elements: 10, 20, 30, 40
--- Exiting Inner Scope (Memory Auto-Freed) ---
=== Outer Scope End (outer_val=100) ===
```

---


---

# 49. Dynamic Memory Allocation (`alloc()`) & Lifecycle Management

## 185. Dynamic Student Roster Allocation
Allocating the exact number of elements at runtime using `alloc(int, count)` without fixed-buffer waste, followed by deterministic automatic scope cleanup.

```cco
// codebase/185_dynamic_memory_alloc_students.cco
fn main() -> int {
    let num_students = 12;

    // Dynamically allocate exact memory needed for 12 students
    let students: int[] = alloc(int, num_students);

    // Populate student grades
    for (let i = 0; i < num_students; i++) {
        students[i] = 75 + (i * 2);
    }

    print("=== Dynamic Memory Student Grades ===");
    let total_sum = 0;
    for score in students {
        print(score);
        total_sum += score;
    }

    let avg = total_sum / num_students;
    print(f"\nTotal Students: {num_students}");
    print(f"Total Score Sum: {total_sum}");
    print(f"Class Average: {avg}");

    return 0;
}
```
**Run:** `./cco codebase/185_dynamic_memory_alloc_students.cco --run`  
**Output:**
```text
=== Dynamic Memory Student Grades ===
75
77
79
81
83
85
87
89
91
93
95
97

Total Students: 12
Total Score Sum: 1032
Class Average: 86
```

---

## 186. Static Stack vs. Dynamic Heap Allocation
Contrasting immediate stack variables with dynamic heap arrays.

```cco
// codebase/186_static_vs_dynamic_comparison.cco
fn main() -> int {
    // 1. Primitive stack memory (Fixed size)
    let fixed_val = 100;
    print(f"Fixed Stack Value: {fixed_val}");

    // 2. Dynamic heap allocation sized at runtime
    let dynamic_size = 5;
    let buffer: int[] = alloc(int, dynamic_size);

    for (let i = 0; i < dynamic_size; i++) {
        buffer[i] = (i + 1) * 10;
    }

    print("=== Dynamic Runtime Buffer Elements ===");
    for item in buffer {
        print(item);
    }

    return 0;
}
```
**Run:** `./cco codebase/186_static_vs_dynamic_comparison.cco --run`  
**Output:**
```text
Fixed Stack Value: 100
=== Dynamic Runtime Buffer Elements ===
10
20
30
40
50
```

---


---

# 50. Accessing Dynamic Memory & Memory Safety

## 187. Dynamic Memory Indexed Access
Allocating dynamic memory and reading/writing elements via index notation (`ptr[0]`, `ptr[1]`, etc.).

```cco
// codebase/187_access_dynamic_memory_indexed.cco
fn main() -> int {
    // Allocate dynamic memory for 4 integers
    let ptr: int[] = alloc(int, 4);

    // Write to the memory
    ptr[0] = 2;
    ptr[1] = 4;
    ptr[2] = 6;
    ptr[3] = 8;

    // Read from the memory
    print("=== Accessing Dynamic Memory ===");
    print(f"First element: {ptr[0]}");
    print(f"Remaining elements: {ptr[1]}, {ptr[2]}, {ptr[3]}");

    return 0;
}
```
**Run:** `./cco codebase/187_access_dynamic_memory_indexed.cco --run`  
**Output:**
```text
=== Accessing Dynamic Memory ===
First element: 2
Remaining elements: 4, 6, 8
```

---

## 188. Rust-Style Memory Safety & Structured Allocation
In C, dynamic memory is untyped bytes susceptible to type punning, out-of-bounds corruption, and leaks. In Cco, dynamic memory is strongly typed, tracked with hidden metadata headers, and cleaned up safely via deterministic RAII.

```cco
// codebase/188_rust_style_memory_safety.cco
fn inspect_buffer(buf: int[]) -> void {
    print("--- Inspecting Dynamic Buffer ---");
    for val in buf {
        print(f"Value: {val}");
    }
}

fn main() -> int {
    // Ownership: 'data' owns the dynamic heap allocation
    let data: int[] = alloc(int, 3);
    data[0] = 100;
    data[1] = 200;
    data[2] = 300;

    print("=== Rust-Style Safety & Structured Allocation ===");
    inspect_buffer(data);

    return 0;
    // 'data' is deterministically freed here with 0 leaks
}
```
**Run:** `./cco codebase/188_rust_style_memory_safety.cco --run`  
**Output:**
```text
=== Rust-Style Safety & Structured Allocation ===
--- Inspecting Dynamic Buffer ---
Value: 100
Value: 200
Value: 300
```

---


---

# 51. Memory Reallocation & Dynamic Capacity Growth

## 189. Manual Buffer Reallocation & Data Preservation
Resizing a dynamic memory buffer to accommodate more items while copying over existing elements.

```cco
// codebase/189_memory_reallocate_manual.cco
fn main() -> int {
    // Step 1: Allocate initial buffer for 4 elements
    let initial_size = 4;
    let ptr1: int[] = alloc(int, initial_size);
    ptr1[0] = 10;
    ptr1[1] = 20;
    ptr1[2] = 30;
    ptr1[3] = 40;

    print("=== Initial Buffer (4 elements) ===");
    for x in ptr1 {
        print(x);
    }

    // Step 2: Resize buffer to 6 elements (reallocating and copying data)
    let new_size = 6;
    let ptr2: int[] = alloc(int, new_size);
    for (let i = 0; i < initial_size; i++) {
        ptr2[i] = ptr1[i];
    }
    ptr2[4] = 50;
    ptr2[5] = 60;

    print("\n=== Resized Buffer (6 elements) ===");
    for x in ptr2 {
        print(x);
    }

    return 0;
}
```
**Run:** `./cco codebase/189_memory_reallocate_manual.cco --run`  
**Output:**
```text
=== Initial Buffer (4 elements) ===
10
20
30
40

=== Resized Buffer (6 elements) ===
10
20
30
40
50
60
```

---

## 190. Automatic Geometric Capacity Growth (`push()`)
Using built-in `push(arr, val)` to automatically trigger capacity doubling and reallocation under the hood.

```cco
// codebase/190_memory_dynamic_growth_push.cco
fn main() -> int {
    // Initialize an empty dynamic buffer
    let dynamic_list: int[] = alloc(int, 0);

    print("=== Dynamic Growth via push() ===");
    // push() reallocates and returns the updated pointer
    dynamic_list = push(dynamic_list, 100);
    dynamic_list = push(dynamic_list, 200);
    dynamic_list = push(dynamic_list, 300);
    dynamic_list = push(dynamic_list, 400);
    dynamic_list = push(dynamic_list, 500);

    for item in dynamic_list {
        print(item);
    }

    return 0;
}
```
**Run:** `./cco codebase/190_memory_dynamic_growth_push.cco --run`  
**Output:**
```text
=== Dynamic Growth via push() ===
100
200
300
400
500
```

---


---

# 52. Automatic Memory Deallocation & Zero-Leak Safety

## 191. Function-Local Memory Deallocation
In C, forgetting `free()` before returning from a function causes silent memory leaks. In Cco, function-local buffers are automatically freed upon return.

```cco
// codebase/191_memory_deallocate_function_scope.cco
fn compute_temporary_data(seed: int) -> int {
    // In C, forgetting free() in a function leaks memory.
    // In Cco, 'temp_buf' is automatically deallocated on return!
    let temp_buf: int[] = alloc(int, 4);
    temp_buf[0] = seed * 1;
    temp_buf[1] = seed * 2;
    temp_buf[2] = seed * 3;
    temp_buf[3] = seed * 4;

    let sum = temp_buf[0] + temp_buf[1] + temp_buf[2] + temp_buf[3];
    return sum;
}

fn main() -> int {
    print("=== Function-Local Scope Memory Deallocation ===");
    let result1 = compute_temporary_data(10);
    let result2 = compute_temporary_data(20);

    print(f"Result 1: {result1}");
    print(f"Result 2: {result2}");
    print("Function calls completed with 0 leaks!");

    return 0;
}
```
**Run:** `./cco codebase/191_memory_deallocate_function_scope.cco --run`  
**Output:**
```text
=== Function-Local Scope Memory Deallocation ===
Result 1: 100
Result 2: 200
Function calls completed with 0 leaks!
```

---

## 192. Loop Iteration Memory Deallocation
Allocating dynamic memory inside loop iterations without accumulating leaks. Each iteration's memory is reclaimed at the iteration boundary.

```cco
// codebase/192_memory_deallocate_loop_iterations.cco
fn main() -> int {
    print("=== Loop Iteration Memory Deallocation ===");

    // In C, allocating inside a loop without free() causes unbounded leak.
    // In Cco, each iteration's buffer is deterministically freed at the loop boundary!
    for (let i = 0; i < 5; i++) {
        let loop_buf: int[] = alloc(int, 3);
        loop_buf[0] = (i + 1) * 10;
        loop_buf[1] = (i + 1) * 20;
        loop_buf[2] = (i + 1) * 30;

        print(f"Iteration {i}: [{loop_buf[0]}, {loop_buf[1]}, {loop_buf[2]}]");
    }

    print("5 iterations completed with 0 bytes leaked!");
    return 0;
}
```
**Run:** `./cco codebase/192_memory_deallocate_loop_iterations.cco --run`  
**Output:**
```text
=== Loop Iteration Memory Deallocation ===
Iteration 0: [10, 20, 30]
Iteration 1: [20, 40, 60]
Iteration 2: [30, 60, 90]
Iteration 3: [40, 80, 120]
Iteration 4: [50, 100, 150]
5 iterations completed with 0 bytes leaked!
```

---


---

# 53. Dynamic Struct Allocations & Collections

## 193. Dynamic Arrays of Structured Objects
Allocating dynamic arrays of class instances (`alloc(Car, 3)`) with automatic cascading destruction of each object's heap fields and outer array buffer.

```cco
// codebase/193_dynamic_struct_array_allocation.cco
class Car {
    brand: string;
    year: int;
}

fn main() -> int {
    // Allocate dynamic array for 3 Car objects
    let cars: Car[] = alloc(Car, 3);

    cars[0] = Car { brand: "Ford", year: 2015 };
    cars[1] = Car { brand: "BMW", year: 2018 };
    cars[2] = Car { brand: "Volvo", year: 2023 };

    print("=== Dynamic Array of Car Objects ===");
    for c in cars {
        print(f"{c.brand} - {c.year}");
    }

    return 0;
}
```
**Run:** `./cco codebase/193_dynamic_struct_array_allocation.cco --run`  
**Output:**
```text
=== Dynamic Array of Car Objects ===
Ford - 2015
BMW - 2018
Volvo - 2023
```

---

## 194. Dynamically Growing Object Collections (`push()`)
Incrementally adding new struct objects into dynamic collections without manual `realloc()` pointer juggling or uninitialized buffer padding.

```cco
// codebase/194_dynamic_struct_growth_push.cco
class Car {
    brand: string;
    year: int;
}

fn main() -> int {
    // Start with empty dynamic collection
    let cars: Car[] = alloc(Car, 0);

    print("=== Growing Array of Structs via push() ===");
    cars = push(cars, Car { brand: "Toyota", year: 2010 });
    cars = push(cars, Car { brand: "Audi", year: 2019 });
    cars = push(cars, Car { brand: "Kia", year: 2022 });

    for c in cars {
        print(f"{c.brand} - {c.year}");
    }

    return 0;
}
```
**Run:** `./cco codebase/194_dynamic_struct_growth_push.cco --run`  
**Output:**
```text
=== Growing Array of Structs via push() ===
Toyota - 2010
Audi - 2019
Kia - 2022
```

---


---

# 54. Real-Life Dynamic Lists & Auto-Scaling Arrays

## 195. Dynamic Growable List (44 Items)
In C, implementing a dynamically growing list requires 70+ lines of custom structs (`data`, `numItems`, `size`), `malloc()`, chunked `realloc()`, temporary pointer validation, and manual `free()`. In Cco, growable dynamic arrays are native first-class primitives with automatic capacity scaling and zero-leak cleanup.

```cco
// codebase/195_dynamic_growable_list_44_items.cco
fn main() -> int {
    // In C, creating a growable dynamic list takes 70+ lines of struct, malloc, realloc, and free boilerplate.
    // In Cco, dynamic growable arrays are first-class built-ins!
    let my_list: int[] = alloc(int, 0);
    let amount = 44;

    print("=== Adding 44 Items Dynamically to List ===");
    for (let i = 0; i < amount; i++) {
        my_list = push(my_list, i + 1);
    }

    // Display the contents of the list
    for item in my_list {
        print(item);
    }

    print(f"\nSuccessfully added {amount} items with automatic capacity management!");
    return 0;
}
```
**Run:** `./cco codebase/195_dynamic_growable_list_44_items.cco --run`  
**Output:**
```text
=== Adding 44 Items Dynamically to List ===
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
26
27
28
29
30
31
32
33
34
35
36
37
38
39
40
41
42
43
44

Successfully added 44 items with automatic capacity management!
```

---


---


[← Back to Guide Index](README.md)
