# Part 1: C Basics & Fundamentals (Variables, Types, Operators, Booleans)

[← Back to Guide Index](README.md)

---

# 1. Quickstart & Basics

## 01. Hello World
Printing text to the screen.

```cco
// codebase/01_hello.cco
fn main() -> int {
    print("Hello World!");
    return 0;
}
```
**Run:** `./cco codebase/01_hello.cco --run`  
**Output:** `Hello World!`

---

## 02. Variables
Creating variables with modern v19 type inference.

```cco
// codebase/02_variables.cco
fn main() -> int {
    let myNum = 15;
    let myFloat = 19.99;
    let myLetter = 'D';
    let myText = "Hello Cco!";

    print(myNum);
    print(myFloat);
    print(myLetter);
    print(myText);
    return 0;
}
```
**Run:** `./cco codebase/02_variables.cco --run`  
**Output:**
```text
15
19.99
D
Hello Cco!
```

---

## 03. Reassigning Variables
Changing a variable's value over time.

```cco
// codebase/03_variable_reassign.cco
fn main() -> int {
    let myNum = 15;
    print(f"Initial: {myNum}");
    myNum = 10;
    print(f"Reassigned: {myNum}");
    return 0;
}
```
**Run:** `./cco codebase/03_variable_reassign.cco --run`  
**Output:**
```text
Initial: 15
Reassigned: 10
```

---

## 04. Copying Variables
Copying the value of one variable to another.

```cco
// codebase/04_variable_copy.cco
fn main() -> int {
    let myOtherNum = 23;
    let myNum = myOtherNum;
    print(f"Copied value: {myNum}");
    return 0;
}
```
**Run:** `./cco codebase/04_variable_copy.cco --run`  
**Output:** `Copied value: 23`

---

## 05. Adding Variables
Adding two variables together.

```cco
// codebase/05_variable_add.cco
fn main() -> int {
    let x = 5;
    let y = 6;
    let sum = x + y;
    print(f"Sum of {x} + {y} = {sum}");
    return 0;
}
```
**Run:** `./cco codebase/05_variable_add.cco --run`  
**Output:** `Sum of 5 + 6 = 11`

---

## 06. Updating Variables
Updating a variable using its own current value.

```cco
// codebase/06_variable_update.cco
fn main() -> int {
    let x = 5;
    x = x + 1;
    print(f"Updated x: {x}");
    return 0;
}
```
**Run:** `./cco codebase/06_variable_update.cco --run`  
**Output:** `Updated x: 6`

---

## 07. Multiple Variables
Declaring multiple variables cleanly.

```cco
// codebase/07_multiple_variables.cco
fn main() -> int {
    let x = 5;
    let y = 6;
    let z = 50;
    print(f"Sum: {x + y + z}");
    return 0;
}
```
**Run:** `./cco codebase/07_multiple_variables.cco --run`  
**Output:** `Sum: 61`

---

## 08. Multiple Same Values
Initializing multiple variables to the same value.

```cco
// codebase/08_multiple_same_value.cco
fn main() -> int {
    let base_val = 50;
    let x = base_val;
    let y = base_val;
    let z = base_val;
    print(f"Sum of identical values: {x + y + z}");
    return 0;
}
```
**Run:** `./cco codebase/08_multiple_same_value.cco --run`  
**Output:** `Sum of identical values: 150`

---

## 09. Variable Names
Using descriptive variable names.

```cco
// codebase/09_variable_names.cco
fn main() -> int {
    let minutes_per_hour = 60;
    let hours_worked = 8;
    let total_minutes = minutes_per_hour * hours_worked;
    print(f"Total minutes: {total_minutes}");
    return 0;
}
```
**Run:** `./cco codebase/09_variable_names.cco --run`  
**Output:** `Total minutes: 480`

---

## 10. Case Sensitivity
Demonstrating that `myVar` and `myvar` are distinct variables.

```cco
// codebase/10_variable_case_sensitivity.cco
fn main() -> int {
    let myVar = 100;
    let myvar = 200;
    print(f"myVar: {myVar}, myvar: {myvar}");
    return 0;
}
```
**Run:** `./cco codebase/10_variable_case_sensitivity.cco --run`  
**Output:** `myVar: 100, myvar: 200`

---

## 11. Student Profile
Real-life example storing a college student profile.

```cco
// codebase/11_student_profile.cco
fn main() -> int {
    let student_id = 15;
    let student_age = 23;
    let student_fee = 75.25;
    let student_grade = 'B';

    print(f"Student ID: {student_id}");
    print(f"Student age: {student_age}");
    print(f"Student fee: ${student_fee}");
    print(f"Student grade: {student_grade}");
    return 0;
}
```
**Run:** `./cco codebase/11_student_profile.cco --run`  
**Output:**
```text
Student ID: 15
Student age: 23
Student fee: $75.25
Student grade: B
```

---

## 12. Rectangle Area
Real-life calculation of rectangle area.

```cco
// codebase/12_rectangle_area.cco
fn main() -> int {
    let length = 4;
    let width = 6;
    let area = length * width;

    print(f"Length: {length}");
    print(f"Width: {width}");
    print(f"Rectangle Area: {area}");
    return 0;
}
```
**Run:** `./cco codebase/12_rectangle_area.cco --run`  
**Output:**
```text
Length: 4
Width: 6
Rectangle Area: 24
```

---


---

# 2. Data Types & Constants

## 13. Data Types Overview
Overview of core primitive and string data types.

```cco
// codebase/13_data_types.cco
fn main() -> int {
    let my_int = 5;
    let my_float = 5.99;
    let my_char = 'D';
    let my_bool = true;
    let my_str = "Hello";

    print(f"int: {my_int} | float: {my_float} | char: {my_char} | bool: {my_bool} | string: {my_str}");
    return 0;
}
```
**Run:** `./cco codebase/13_data_types.cco --run`  
**Output:** `int: 5 | float: 5.99 | char: D | bool: true | string: Hello`

---

## 14. Character vs String
Difference between a 1-byte `char` (`'A'`) and a `string` (`"Apple"`).

```cco
// codebase/14_char_type.cco
fn main() -> int {
    let grade = 'A';
    let word = "Apple";

    print(f"Single char: {grade}");
    print(f"Full string: {word}");
    return 0;
}
```
**Run:** `./cco codebase/14_char_type.cco --run`  
**Output:**
```text
Single char: A
Full string: Apple
```

---

## 15. Numeric Types
Integer whole numbers vs. floating-point decimals.

```cco
// codebase/15_numeric_types.cco
fn main() -> int {
    let whole_number = 1000;
    let decimal_number = 19.99;

    print(f"Whole: {whole_number}");
    print(f"Decimal: {decimal_number}");
    return 0;
}
```
**Run:** `./cco codebase/15_numeric_types.cco --run`  
**Output:**
```text
Whole: 1000
Decimal: 19.99
```

---

## 16. Decimal Precision
Clean decimal printing with trailing zero cleanup.

```cco
// codebase/16_decimal_precision.cco
fn main() -> int {
    let price = 19.99;
    let whole_float = 3.5;

    print(f"Price: {price}");
    print(f"Float: {whole_float}");
    return 0;
}
```
**Run:** `./cco codebase/16_decimal_precision.cco --run`  
**Output:**
```text
Price: 19.99
Float: 3.5
```

---

## 17. Type Conversion & Division
Difference between integer division (`5 / 2 = 2`) and float division (`5.0 / 2.0 = 2.5`).

```cco
// codebase/17_type_conversion_div.cco
fn main() -> int {
    let int_div = 5 / 2;
    let float_div = 5.0 / 2.0;

    print(f"Integer division (5 / 2): {int_div}");
    print(f"Float division (5.0 / 2.0): {float_div}");
    return 0;
}
```
**Run:** `./cco codebase/17_type_conversion_div.cco --run`  
**Output:**
```text
Integer division (5 / 2): 2
Float division (5.0 / 2.0): 2.5
```

---

## 18. Score Percentage
Real-life score calculation with float conversion.

```cco
// codebase/18_score_percentage.cco
fn main() -> int {
    let max_score = 500;
    let user_score = 423;
    let percentage = (1.0 * user_score / max_score) * 100.0;

    print(f"User score: {user_score} / {max_score}");
    print(f"Percentage: {percentage}%");
    return 0;
}
```
**Run:** `./cco codebase/18_score_percentage.cco --run`  
**Output:**
```text
User score: 423 / 500
Percentage: 84.6%
```

---

## 19. Constants
Declaring constant-like values with UPPERCASE naming convention.

> **Honest Language Note**: Cco does not currently feature a dedicated `const` keyword or immutable binding modifier. All local bindings are declared using `let`. Constant values are established by convention (`UPPER_CASE_NAMES`) and deliberate non-reassignment.

```cco
// codebase/19_constants.cco
fn main() -> int {
    let MINUTES_PER_HOUR = 60;
    let MONTHS_IN_YEAR = 12;

    print(f"Minutes per hour: {MINUTES_PER_HOUR}");
    print(f"Months in year: {MONTHS_IN_YEAR}");
    return 0;
}
```
**Run:** `./cco codebase/19_constants.cco --run`  
**Output:**
```text
Minutes per hour: 60
Months in year: 12
```

---

## 20. Circle Geometry
Using the mathematical constant $\pi$ to calculate area.

```cco
// codebase/20_circle_geometry.cco
fn main() -> int {
    let PI = 3.14159265;
    let radius = 5.0;
    let area = PI * radius * radius;

    print(f"Radius: {radius}");
    print(f"Area: {area}");
    return 0;
}
```
**Run:** `./cco codebase/20_circle_geometry.cco --run`  
**Output:**
```text
Radius: 5
Area: 78.5397
```

---


---

# 3. Arithmetic Operators

## 21. Addition
Adding two numbers with `+`.

```cco
// codebase/21_add.cco
fn main() -> int {
    let x = 10;
    let y = 5;
    let sum = x + y;
    print(f"Addition: {x} + {y} = {sum}");
    return 0;
}
```
**Run:** `./cco codebase/21_add.cco --run`  
**Output:** `Addition: 10 + 5 = 15`

---

## 22. Subtraction
Subtracting numbers with `-`.

```cco
// codebase/22_sub.cco
fn main() -> int {
    let x = 10;
    let y = 5;
    let diff = x - y;
    print(f"Subtraction: {x} - {y} = {diff}");
    return 0;
}
```
**Run:** `./cco codebase/22_sub.cco --run`  
**Output:** `Subtraction: 10 - 5 = 5`

---

## 23. Multiplication
Multiplying numbers with `*`.

```cco
// codebase/23_mul.cco
fn main() -> int {
    let x = 10;
    let y = 5;
    let prod = x * y;
    print(f"Multiplication: {x} * {y} = {prod}");
    return 0;
}
```
**Run:** `./cco codebase/23_mul.cco --run`  
**Output:** `Multiplication: 10 * 5 = 50`

---

## 24. Division
Dividing numbers with `/`.

```cco
// codebase/24_div.cco
fn main() -> int {
    let x = 10;
    let y = 5;
    let quotient = x / y;
    print(f"Division: {x} / {y} = {quotient}");
    return 0;
}
```
**Run:** `./cco codebase/24_div.cco --run`  
**Output:** `Division: 10 / 5 = 2`

---

## 25. Modulus
Calculating remainder with `%`.

```cco
// codebase/25_mod.cco
fn main() -> int {
    let x = 10;
    let y = 3;
    let remainder = x % y;
    print(f"Modulus: {x} % {y} = {remainder}");
    return 0;
}
```
**Run:** `./cco codebase/25_mod.cco --run`  
**Output:** `Modulus: 10 % 3 = 1`

---

## 26. Increment
Increasing a variable by 1 using `++`.

```cco
// codebase/26_increment.cco
fn main() -> int {
    let x = 5;
    x++;
    print(f"Incremented x (x++): {x}");
    return 0;
}
```
**Run:** `./cco codebase/26_increment.cco --run`  
**Output:** `Incremented x (x++): 6`

---

## 27. Decrement
Decreasing a variable by 1 using `--`.

```cco
// codebase/27_decrement.cco
fn main() -> int {
    let x = 5;
    x--;
    print(f"Decremented x (x--): {x}");
    return 0;
}
```
**Run:** `./cco codebase/27_decrement.cco --run`  
**Output:** `Decremented x (x--): 4`

---

## 28. People Counter
Real-life counter tracking people entering and leaving a room.

```cco
// codebase/28_people_counter.cco
fn main() -> int {
    let people = 0;
    people++;
    people++;
    people++;
    print(f"3 people entered: {people}");
    people--;
    print(f"1 person left: {people}");
    return 0;
}
```
**Run:** `./cco codebase/28_people_counter.cco --run`  
**Output:**
```text
3 people entered: 3
1 person left: 2
```

---


---

# 4. Assignment Operators

## 29. Simple Assignment
Assigning a value using `=`.

```cco
// codebase/29_assign.cco
fn main() -> int {
    let x = 10;
    print(f"Assigned value (=): {x}");
    return 0;
}
```
**Run:** `./cco codebase/29_assign.cco --run`  
**Output:** `Assigned value (=): 10`

---

## 30. Addition Assignment
Adding and assigning using `+=`.

```cco
// codebase/30_add_assign.cco
fn main() -> int {
    let x = 10;
    x += 5;
    print(f"After addition assignment (x += 5): {x}");
    return 0;
}
```
**Run:** `./cco codebase/30_add_assign.cco --run`  
**Output:** `After addition assignment (x += 5): 15`

---

## 31. Subtraction Assignment
Subtracting and assigning using `-=`.

```cco
// codebase/31_sub_assign.cco
fn main() -> int {
    let x = 10;
    x -= 3;
    print(f"After subtraction assignment (x -= 3): {x}");
    return 0;
}
```
**Run:** `./cco codebase/31_sub_assign.cco --run`  
**Output:** `After subtraction assignment (x -= 3): 7`

---

## 32. Multiplication Assignment
Multiplying and assigning using `*=`.

```cco
// codebase/32_mul_assign.cco
fn main() -> int {
    let x = 10;
    x *= 2;
    print(f"After multiplication assignment (x *= 2): {x}");
    return 0;
}
```
**Run:** `./cco codebase/32_mul_assign.cco --run`  
**Output:** `After multiplication assignment (x *= 2): 20`

---

## 33. Division Assignment
Dividing and assigning using `/=`.

```cco
// codebase/33_div_assign.cco
fn main() -> int {
    let x = 12;
    x /= 3;
    print(f"After division assignment (x /= 3): {x}");
    return 0;
}
```
**Run:** `./cco codebase/33_div_assign.cco --run`  
**Output:** `After division assignment (x /= 3): 4`

---

## 34. Modulus Assignment
Calculating remainder and assigning using `%=`.

```cco
// codebase/34_mod_assign.cco
fn main() -> int {
    let x = 10;
    x %= 4;
    print(f"After modulus assignment (x %= 4): {x}");
    return 0;
}
```
**Run:** `./cco codebase/34_mod_assign.cco --run`  
**Output:** `After modulus assignment (x %= 4): 2`

---

## 35. String Concatenation Assignment
Appending to a string using `+=` with zero memory leaks.

```cco
// codebase/35_string_add_assign.cco
fn main() -> int {
    let message = "Hello";
    message += ", World!";
    print(f"String += assignment: {message}");
    return 0;
}
```
**Run:** `./cco codebase/35_string_add_assign.cco --run`  
**Output:** `String += assignment: Hello, World!`

---

## 36. Savings Tracker
Real-life savings account balance tracker.

```cco
// codebase/36_savings_tracker.cco
fn main() -> int {
    let savings = 100;
    savings += 50;
    savings -= 30;
    print(f"Final savings balance: ${savings}");
    return 0;
}
```
**Run:** `./cco codebase/36_savings_tracker.cco --run`  
**Output:** `Final savings balance: $120`

---


---

# 5. Comparison Operators

## 37. Equal To
Checking equality with `==`.

```cco
// codebase/37_equal.cco
fn main() -> int {
    let x = 5;
    let y = 5;
    print(f"Equal check ({x} == {y}): {x == y}");
    return 0;
}
```
**Run:** `./cco codebase/37_equal.cco --run`  
**Output:** `Equal check (5 == 5): true`

---

## 38. Not Equal
Checking inequality with `!=`.

```cco
// codebase/38_not_equal.cco
fn main() -> int {
    let x = 5;
    let y = 3;
    print(f"Not equal check ({x} != {y}): {x != y}");
    return 0;
}
```
**Run:** `./cco codebase/38_not_equal.cco --run`  
**Output:** `Not equal check (5 != 3): true`

---

## 39. Greater Than
Checking if left is strictly larger with `>`.

```cco
// codebase/39_greater.cco
fn main() -> int {
    let x = 5;
    let y = 3;
    print(f"Greater check ({x} > {y}): {x > y}");
    return 0;
}
```
**Run:** `./cco codebase/39_greater.cco --run`  
**Output:** `Greater check (5 > 3): true`

---

## 40. Less Than
Checking if left is strictly smaller with `<`.

```cco
// codebase/40_less.cco
fn main() -> int {
    let x = 3;
    let y = 5;
    print(f"Less check ({x} < {y}): {x < y}");
    return 0;
}
```
**Run:** `./cco codebase/40_less.cco --run`  
**Output:** `Less check (3 < 5): true`

---

## 41. Greater or Equal
Checking if left is equal or larger with `>=`.

```cco
// codebase/41_greater_equal.cco
fn main() -> int {
    let x = 5;
    let y = 5;
    print(f"Greater or equal ({x} >= {y}): {x >= y}");
    return 0;
}
```
**Run:** `./cco codebase/41_greater_equal.cco --run`  
**Output:** `Greater or equal (5 >= 5): true`

---

## 42. Less or Equal
Checking if left is equal or smaller with `<=`.

```cco
// codebase/42_less_equal.cco
fn main() -> int {
    let x = 3;
    let y = 5;
    print(f"Less or equal ({x} <= {y}): {x <= y}");
    return 0;
}
```
**Run:** `./cco codebase/42_less_equal.cco --run`  
**Output:** `Less or equal (3 <= 5): true`

---

## 43. Voting Check
Real-life check for age voting eligibility.

```cco
// codebase/43_voting_check.cco
fn main() -> int {
    let age = 18;
    print(f"Eligible to vote (age >= 18): {age >= 18}");
    return 0;
}
```
**Run:** `./cco codebase/43_voting_check.cco --run`  
**Output:** `Eligible to vote (age >= 18): true`

---

## 44. Password Check
Real-life check for password character length.

```cco
// codebase/44_password_check.cco
fn main() -> int {
    let password_len = 5;
    print(f"Secure password (len >= 8): {password_len >= 8}");
    return 0;
}
```
**Run:** `./cco codebase/44_password_check.cco --run`  
**Output:** `Secure password (len >= 8): false`

---


---

# 6. Logical Operators & Precedence

## 45. Logical AND
Combining two conditions with `&&` (both must be true).

```cco
// codebase/45_logical_and.cco
fn main() -> int {
    let x = 7;
    let is_between = x > 5 && x < 10;
    print(f"Logical AND ({x} > 5 && {x} < 10): {is_between}");
    return 0;
}
```
**Run:** `./cco codebase/45_logical_and.cco --run`  
**Output:** `Logical AND (7 > 5 && 7 < 10): true`

---

## 46. Logical OR
Combining conditions with `||` (at least one must be true).

```cco
// codebase/46_logical_or.cco
fn main() -> int {
    let x = 7;
    let is_outer = x < 5 || x > 6;
    print(f"Logical OR ({x} < 5 || {x} > 6): {is_outer}");
    return 0;
}
```
**Run:** `./cco codebase/46_logical_or.cco --run`  
**Output:** `Logical OR (7 < 5 || 7 > 6): true`

---

## 47. Logical NOT
Inverting boolean truth value with `!`.

```cco
// codebase/47_logical_not.cco
fn main() -> int {
    let is_open = true;
    print(f"Logical NOT (!is_open): {!is_open}");
    return 0;
}
```
**Run:** `./cco codebase/47_logical_not.cco --run`  
**Output:** `Logical NOT (!is_open): false`

---

## 48. Login & Role Access
Real-life role-based access control.

```cco
// codebase/48_login_access.cco
fn main() -> int {
    let is_logged_in = true;
    let is_admin = false;
    let regular_access = is_logged_in && !is_admin;
    print(f"Regular user access: {regular_access}");
    return 0;
}
```
**Run:** `./cco codebase/48_login_access.cco --run`  
**Output:** `Regular user access: true`

---

## 49. Operator Precedence
Order of operations: multiplication before addition, and parentheses overrides.

```cco
// codebase/49_operator_precedence.cco
fn main() -> int {
    let result1 = 2 + 3 * 4;
    let result2 = (2 + 3) * 4;
    print(f"2 + 3 * 4 = {result1}");
    print(f"(2 + 3) * 4 = {result2}");
    return 0;
}
```
**Run:** `./cco codebase/49_operator_precedence.cco --run`  
**Output:**
```text
2 + 3 * 4 = 14
(2 + 3) * 4 = 20
```

---

## 50. Left-to-Right Associativity in Operators

Addition and subtraction have the same precedence, so they are evaluated from left to right unless parentheses are used.

```cco
// codebase/50_operator_precedence_assoc.cco
fn main() -> int {
    let result1 = 10 - 2 + 5;   // (10 - 2) + 5 = 13 (Evaluated Left-to-Right)
    let result2 = 10 - (2 + 5); // 10 - 7 = 3        (Parentheses evaluated first)

    print(f"10 - 2 + 5 = {result1}");
    print(f"10 - (2 + 5) = {result2}");
    return 0;
}
```
**Run:** `./cco codebase/50_operator_precedence_assoc.cco --run`  
**Output:**
```text
10 - 2 + 5 = 13
10 - (2 + 5) = 3
```

---

### 📊 Full Operator Precedence Table (Highest to Lowest):

| Priority | Operators | Description |
| :--- | :--- | :--- |
| **1 (Highest)** | `()` | Parentheses (grouping) |
| **2** | `++`, `--` | Increment / Decrement statements |
| **3** | `*`, `/`, `%` | Multiplication, Division, Modulus |
| **4** | `+`, `-` | Addition, Subtraction |
| **5** | `>`, `<`, `>=`, `<=` | Relational comparisons |
| **6** | `==`, `!=` | Equality comparisons |
| **7** | `&&` | Logical AND |
| **8** | `\|\|` | Logical OR |
| **9 (Lowest)** | `=`, `+=`, `-=`, `*=`, `/=`, `%=` | Assignment |

---


---

# 7. Booleans & Decision Fundamentals

## 51. Boolean Variables
Creating native boolean variables in Cco (`true` and `false`).

```cco
// codebase/51_boolean_variables.cco
fn main() -> int {
    let is_programming_fun = true;
    let is_fish_tasty = false;

    print("=== Boolean Variables ===");
    print(is_programming_fun); // Prints true
    print(is_fish_tasty);        // Prints false

    print(f"Is programming fun: {is_programming_fun}");
    print(f"Is fish tasty: {is_fish_tasty}");
    return 0;
}
```
**Run:** `./cco codebase/51_boolean_variables.cco --run`  
**Output:**
```text
=== Boolean Variables ===
true
false
Is programming fun: true
Is fish tasty: false
```

---

## 52. Comparing Booleans and Values
Comparing numbers and boolean expressions.

```cco
// codebase/52_boolean_comparison.cco
fn main() -> int {
    // 1. Comparing numeric values
    print("=== Comparing Values ===");
    print(f"10 > 9: {10 > 9}");
    print(f"10 == 10: {10 == 10}");
    print(f"10 == 15: {10 == 15}");

    // 2. Comparing boolean variables
    let is_hamburger_tasty = true;
    let is_pizza_tasty = true;
    let both_tasty = is_hamburger_tasty == is_pizza_tasty;

    print("\n=== Comparing Boolean Variables ===");
    print(f"Both foods tasty (is_hamburger_tasty == is_pizza_tasty): {both_tasty}");
    return 0;
}
```
**Run:** `./cco codebase/52_boolean_comparison.cco --run`  
**Output:**
```text
=== Comparing Values ===
10 > 9: true
10 == 10: true
10 == 15: false

=== Comparing Boolean Variables ===
Both foods tasty (is_hamburger_tasty == is_pizza_tasty): true
```

---

## 53. Storing Comparison Results
Saving comparison results in boolean variables for later reuse.

```cco
// codebase/53_boolean_store_result.cco
fn main() -> int {
    let x = 10;
    let y = 9;

    let is_greater = x > y;

    print(f"Values: x = {x}, y = {y}");
    print(f"Stored comparison result (is_greater): {is_greater}");
    return 0;
}
```
**Run:** `./cco codebase/53_boolean_store_result.cco --run`  
**Output:**
```text
Values: x = 10, y = 9
Stored comparison result (is_greater): true
```

---

## 54. Real-Life Boolean Check: Voting Eligibility
Evaluating boolean conditions with `>=`.

```cco
// codebase/54_voting_age_boolean.cco
fn main() -> int {
    let my_age = 25;
    let voting_age = 18;

    let is_eligible = my_age >= voting_age;

    print(f"My age: {my_age}, Voting age limit: {voting_age}");
    print(f"Eligible to vote (my_age >= voting_age): {is_eligible}");
    return 0;
}
```
**Run:** `./cco codebase/54_voting_age_boolean.cco --run`  
**Output:**
```text
My age: 25, Voting age limit: 18
Eligible to vote (my_age >= voting_age): true
```

---

## 55. Real-Life Control Flow: If-Else Decision Making
Using boolean expressions inside `if...else` blocks to perform different actions based on conditions.

```cco
// codebase/55_voting_age_ifelse.cco
fn main() -> int {
    let my_age = 25;
    let voting_age = 18;

    print(f"Checking eligibility for age {my_age}:");

    if (my_age >= voting_age) {
        print("Old enough to vote!");
    } else {
        print("Not old enough to vote.");
    }

    return 0;
}
```
**Run:** `./cco codebase/55_voting_age_ifelse.cco --run`  
**Output:**
```text
Checking eligibility for age 25:
Old enough to vote!
```

---


---


[← Back to Guide Index](README.md)
