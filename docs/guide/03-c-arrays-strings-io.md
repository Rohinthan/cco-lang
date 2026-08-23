# Part 3: Data Collections & I/O (Arrays, Matrices, Strings, Input)

[← Back to Guide Index](README.md)

---

# 17. Array Fundamentals & Indexing

## 103. Accessing Array Elements
Creating an array and accessing elements via zero-based indexing (`[0]`, `[1]`, `[2]`, `[3]`).

```cco
// codebase/103_array_access.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 4);
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    print("=== Accessing Array Elements ===");
    print(f"First element [0]: {my_numbers[0]}");
    print(f"Second element [1]: {my_numbers[1]}");
    print(f"Third element [2]: {my_numbers[2]}");
    print(f"Fourth element [3]: {my_numbers[3]}");

    return 0;
}
```
**Run:** `./cco codebase/103_array_access.cco --run`  
**Output:**
```text
=== Accessing Array Elements ===
First element [0]: 25
Second element [1]: 50
Third element [2]: 75
Fourth element [3]: 100
```

---

## 104. Modifying Array Elements
Updating the value stored at a specific index (`my_numbers[0] = 33`).

```cco
// codebase/104_array_modify.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 4);
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    print(f"Original first element: {my_numbers[0]}");

    // Modify the first element
    my_numbers[0] = 33;

    print(f"Modified first element: {my_numbers[0]}");
    return 0;
}
```
**Run:** `./cco codebase/104_array_modify.cco --run`  
**Output:**
```text
Original first element: 25
Modified first element: 33
```

---

## 105. Allocating Array Size
Explicitly requesting array capacity and populating slots before iteration.

```cco
// codebase/105_array_allocate_size.cco
fn main() -> int {
    // Allocate space for 4 integers
    let my_numbers: int[] = alloc(int, 4);

    // Assign values to allocated slots
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    print("=== Array Allocated and Populated ===");
    for (let i = 0; i < 4; i++) {
        print(f"Index {i}: {my_numbers[i]}");
    }

    return 0;
}
```
**Run:** `./cco codebase/105_array_allocate_size.cco --run`  
**Output:**
```text
=== Array Allocated and Populated ===
Index 0: 25
Index 1: 50
Index 2: 75
Index 3: 100
```

---


---

# 18. Iterating Over Arrays

## 106. Array Looping with `len()`
Using the built-in `len()` function to dynamically determine array size without hardcoding bounds or complex `sizeof` formulas.

```cco
// codebase/106_array_loop_len.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 4);
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    let length = len(my_numbers);
    print(f"Array length dynamically determined: {length}");

    print("=== Iterating with len() ===");
    for (let i = 0; i < length; i++) {
        print(my_numbers[i]);
    }

    return 0;
}
```
**Run:** `./cco codebase/106_array_loop_len.cco --run`  
**Output:**
```text
Array length dynamically determined: 4
=== Iterating with len() ===
25
50
75
100
```

---

## 107. Python-Style `for item in array` Iteration
Iterating directly over array values without managing integer index counters.

```cco
// codebase/107_array_loop_foreach.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 4);
    my_numbers[0] = 25;
    my_numbers[1] = 50;
    my_numbers[2] = 75;
    my_numbers[3] = 100;

    print("=== Iterating with 'for item in array' ===");
    for num in my_numbers {
        print(num);
    }

    return 0;
}
```
**Run:** `./cco codebase/107_array_loop_foreach.cco --run`  
**Output:**
```text
=== Iterating with 'for item in array' ===
25
50
75
100
```

---


---

# 19. Real-Life Array Problem Solving

## 108. Calculating Average of Array Elements
Summing dynamic elements and calculating fractional average without truncation.

```cco
// codebase/108_array_average_ages.cco
fn main() -> int {
    let ages: int[] = alloc(int, 8);
    ages[0] = 20;
    ages[1] = 22;
    ages[2] = 18;
    ages[3] = 35;
    ages[4] = 48;
    ages[5] = 26;
    ages[6] = 87;
    ages[7] = 70;

    let total_sum = 0.0;
    let length = len(ages);

    for (let i = 0; i < length; i++) {
        total_sum += ages[i];
    }

    let avg = total_sum / (1.0 * length);
    print(f"The average age is: {avg}");

    return 0;
}
```
**Run:** `./cco codebase/108_array_average_ages.cco --run`  
**Output:** `The average age is: 40.75`

---

## 109. Finding the Minimum Element in an Array
Linear scan using `for age in ages` to find the lowest age.

```cco
// codebase/109_array_lowest_age.cco
fn main() -> int {
    let ages: int[] = alloc(int, 8);
    ages[0] = 20;
    ages[1] = 22;
    ages[2] = 18;
    ages[3] = 35;
    ages[4] = 48;
    ages[5] = 26;
    ages[6] = 87;
    ages[7] = 70;

    let lowest_age = ages[0];

    for age in ages {
        if (age < lowest_age) {
            lowest_age = age;
        }
    }

    print(f"The lowest age is: {lowest_age}");
    return 0;
}
```
**Run:** `./cco codebase/109_array_lowest_age.cco --run`  
**Output:** `The lowest age is: 18`

---


---

# 20. Multi-Dimensional Arrays & Matrices

## 110. 2D Matrix Element Access & Modification
Accessing rows and columns in a 2D matrix structure.

```cco
// codebase/110_matrix_2d_access.cco
fn main() -> int {
    // 2x3 Matrix represented as rows
    let row0: int[] = alloc(int, 3);
    row0[0] = 1;
    row0[1] = 4;
    row0[2] = 2;

    let row1: int[] = alloc(int, 3);
    row1[0] = 3;
    row1[1] = 6;
    row1[2] = 8;

    print("=== Accessing 2D Matrix Elements ===");
    print(f"matrix[0][2] (row 0, col 2): {row0[2]}");

    // Modify element at row 0, col 0
    row0[0] = 9;
    print(f"Modified matrix[0][0]: {row0[0]}");

    return 0;
}
```
**Run:** `./cco codebase/110_matrix_2d_access.cco --run`  
**Output:**
```text
=== Accessing 2D Matrix Elements ===
matrix[0][2] (row 0, col 2): 2
Modified matrix[0][0]: 9
```

---

## 111. Iterating Over 2D Matrix Rows
Looping through the constituent rows of a matrix.

```cco
// codebase/111_matrix_2d_loop.cco
fn main() -> int {
    let row0: int[] = alloc(int, 3);
    row0[0] = 1;
    row0[1] = 4;
    row0[2] = 2;

    let row1: int[] = alloc(int, 3);
    row1[0] = 3;
    row1[1] = 6;
    row1[2] = 8;

    print("=== Looping Through 2D Matrix Rows ===");
    for val in row0 {
        print(val);
    }
    for val in row1 {
        print(val);
    }

    return 0;
}
```
**Run:** `./cco codebase/111_matrix_2d_loop.cco --run`  
**Output:**
```text
=== Looping Through 2D Matrix Rows ===
1
4
2
3
6
8
```

---

## 112. High-Performance Flat 2D Matrix Grid
Mapping 2D coordinates `(row, col)` into a flat contiguous memory buffer using the formula `row * cols + col`.

```cco
// codebase/112_matrix_flat_buffer.cco
fn main() -> int {
    // High-performance 2x3 Matrix (Rows = 2, Cols = 3)
    let rows = 2;
    let cols = 3;
    let matrix: int[] = alloc(int, rows * cols);

    // Populate matrix (Row 0: 1, 4, 2 | Row 1: 3, 6, 8)
    matrix[0 * cols + 0] = 1;
    matrix[0 * cols + 1] = 4;
    matrix[0 * cols + 2] = 2;

    matrix[1 * cols + 0] = 3;
    matrix[1 * cols + 1] = 6;
    matrix[1 * cols + 2] = 8;

    print("=== 2D Matrix Grid (2 Rows x 3 Columns) ===");
    for (let r = 0; r < rows; r++) {
        let v0 = matrix[r * cols + 0];
        let v1 = matrix[r * cols + 1];
        let v2 = matrix[r * cols + 2];
        print(f"{v0} {v1} {v2}");
    }

    return 0;
}
```
**Run:** `./cco codebase/112_matrix_flat_buffer.cco --run`  
**Output:**
```text
=== 2D Matrix Grid (2 Rows x 3 Columns) ===
1 4 2
3 6 8
```

---


---

# 21. String Handling & Character Access

## 113. String Basics
Creating and printing first-class `string` variables without format specifiers.

```cco
// codebase/113_string_basics.cco
fn main() -> int {
    let greetings = "Hello World!";
    print(greetings);
    return 0;
}
```
**Run:** `./cco codebase/113_string_basics.cco --run`  
**Output:** `Hello World!`

---

## 114. Character Access with `char_at()`
Accessing individual characters from a string using `char_at(str, index)`.

```cco
// codebase/114_string_char_at.cco
fn main() -> int {
    let greetings = "Hello World!";
    let first_char = char_at(greetings, 0);

    print(f"Full string: {greetings}");
    print(f"First character: {first_char}");

    return 0;
}
```
**Run:** `./cco codebase/114_string_char_at.cco --run`  
**Output:**
```text
Full string: Hello World!
First character: H
```

---

## 115. Looping Through Characters
Iterating through every character of a string from index `0` to `len(str) - 1`.

```cco
// codebase/115_string_loop_chars.cco
fn main() -> int {
    let car_name = "Volvo";
    let length = len(car_name);

    print("=== Characters of 'Volvo' ===");
    for (let i = 0; i < length; i++) {
        let ch = char_at(car_name, i);
        print(ch);
    }

    return 0;
}
```
**Run:** `./cco codebase/115_string_loop_chars.cco --run`  
**Output:**
```text
=== Characters of 'Volvo' ===
V
o
l
v
o
```

---

## 116. Real-Life String Formatting (Welcome Message)
Combining and formatting text using clean Python-style string interpolation.

```cco
// codebase/116_string_welcome_message.cco
fn main() -> int {
    let message = "Good to see you,";
    let fname = "John";

    print(f"{message} {fname}!");
    return 0;
}
```
**Run:** `./cco codebase/116_string_welcome_message.cco --run`  
**Output:** `Good to see you, John!`

---


---

# 22. String Escape Characters

## 117. Escaping Quotes (`\"` and `\'`)
Embedding double quotes and single quotes inside strings.

```cco
// codebase/117_string_escape_quotes.cco
fn main() -> int {
    let txt1 = "We are the so-called \"Vikings\" from the north.";
    let txt2 = "It\'s alright.";

    print(txt1);
    print(txt2);

    return 0;
}
```
**Run:** `./cco codebase/117_string_escape_quotes.cco --run`  
**Output:**
```text
We are the so-called "Vikings" from the north.
It's alright.
```

---

## 118. Escaping Backslashes (`\\`)
Printing literal backslash characters within string literals.

```cco
// codebase/118_string_escape_backslash.cco
fn main() -> int {
    let txt = "The character \\ is called backslash.";
    print(txt);
    return 0;
}
```
**Run:** `./cco codebase/118_string_escape_backslash.cco --run`  
**Output:** `The character \ is called backslash.`

---

## 119. Newlines and Tabs (`\n` and `\t`)
Formatting multiline and columnated output using control characters.

```cco
// codebase/119_string_escape_newline_tab.cco
fn main() -> int {
    let multiline = "Line 1\nLine 2";
    let tabular = "Name:\tAlice\nAge:\t25";

    print("=== Newline Escape (\\n) ===");
    print(multiline);

    print("\n=== Tab Escape (\\t) ===");
    print(tabular);

    return 0;
}
```
**Run:** `./cco codebase/119_string_escape_newline_tab.cco --run`  
**Output:**
```text
=== Newline Escape (\n) ===
Line 1
Line 2

=== Tab Escape (\t) ===
Name:	Alice
Age:	25
```

---


---

# 23. Built-In String Functions

## 120. String Length (`len()`)
Getting the exact character count of a string without manual `sizeof` adjustments.

```cco
// codebase/120_string_length_len.cco
fn main() -> int {
    let alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    let length = len(alphabet);

    print(f"Alphabet string: {alphabet}");
    print(f"String length (len): {length}");

    return 0;
}
```
**Run:** `./cco codebase/120_string_length_len.cco --run`  
**Output:**
```text
Alphabet string: ABCDEFGHIJKLMNOPQRSTUVWXYZ
String length (len): 26
```

---

## 121. String Concatenation (`concat()` and `+=`)
Combining strings safely with automatic memory resizing and cleanup.

```cco
// codebase/121_string_concat.cco
fn main() -> int {
    let str1 = "Hello ";
    let str2 = "World!";

    // Method 1: Using built-in concat()
    let combined = concat(str1, str2);
    print(f"concat(str1, str2): {combined}");

    // Method 2: Using compound += assignment
    let greeting = "Hello ";
    greeting += "World!";
    print(f"greeting += 'World!': {greeting}");

    return 0;
}
```
**Run:** `./cco codebase/121_string_concat.cco --run`  
**Output:**
```text
concat(str1, str2): Hello World!
greeting += 'World!': Hello World!
```

---

## 122. Safe String Copying
Creating independent heap-managed string copies without buffer overrun vulnerabilities.

```cco
// codebase/122_string_copy.cco
fn main() -> int {
    let str1 = "Hello World!";

    // Create an independent copied string buffer using interpolation
    let str2 = f"{str1}";

    print(f"Original str1: {str1}");
    print(f"Copied str2: {str2}");

    return 0;
}
```
**Run:** `./cco codebase/122_string_copy.cco --run`  
**Output:**
```text
Original str1: Hello World!
Copied str2: Hello World!
```

---

## 123. String Comparison (`equals()`)
Comparing two strings for content equality and returning boolean `true` or `false`.

```cco
// codebase/123_string_compare.cco
fn main() -> int {
    let str1 = "Hello";
    let str2 = "Hello";
    let str3 = "Hi";

    let match1 = equals(str1, str2);
    let match2 = equals(str1, str3);

    print(f"Comparing '{str1}' and '{str2}': {match1}");
    print(f"Comparing '{str1}' and '{str3}': {match2}");

    return 0;
}
```
**Run:** `./cco codebase/123_string_compare.cco --run`  
**Output:**
```text
Comparing 'Hello' and 'Hello': true
Comparing 'Hello' and 'Hi': false
```

---


---

# 24. Standard User Input & Parsing

## 124. Reading String Input (`read_line()`)
Reading a line of text directly from standard input.

```cco
// codebase/124_input_string.cco
fn main() -> int {
    let first_name = read_line();
    print(f"Hello {first_name}");
    return 0;
}
```
**Run:** `echo "John" | ./cco codebase/124_input_string.cco --run`  
**Output:** `Hello John`

---

## 125. Reading and Parsing Integers (`to_int()`)
Reading standard input and converting text into an integer.

```cco
// codebase/125_input_number.cco
fn main() -> int {
    let input_str = read_line();
    let my_num = to_int(input_str);
    print(f"Your number is: {my_num}");
    return 0;
}
```
**Run:** `echo "42" | ./cco codebase/125_input_number.cco --run`  
**Output:** `Your number is: 42`

---

## 126. Reading Full Strings with Spaces
Reading multi-word inputs without truncation (unlike C's `scanf("%s")`).

```cco
// codebase/126_input_full_name.cco
fn main() -> int {
    let full_name = read_line();
    print(f"Hello {full_name}");
    return 0;
}
```
**Run:** `echo "John Doe" | ./cco codebase/126_input_full_name.cco --run`  
**Output:** `Hello John Doe`

---


---


[← Back to Guide Index](README.md)
