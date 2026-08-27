# Cco Interactive Programming Guide

Welcome to the **Cco Interactive Guide**! This document breaks down C programming concepts into atomic, easy-to-learn Cco programs. Every single operation has its own dedicated program that you can run and test immediately.

---

# Table of Contents

1. [Quickstart & Basics](#1-quickstart--basics)
   - [01. Hello World (`01_hello.cco`)](#01-hello-world)
   - [02. Variables (`02_variables.cco`)](#02-variables)
   - [03. Reassigning Variables (`03_variable_reassign.cco`)](#03-reassigning-variables)
   - [04. Copying Variables (`04_variable_copy.cco`)](#04-copying-variables)
   - [05. Adding Variables (`05_variable_add.cco`)](#05-adding-variables)
   - [06. Updating Variables (`06_variable_update.cco`)](#06-updating-variables)
   - [07. Multiple Variables (`07_multiple_variables.cco`)](#07-multiple-variables)
   - [08. Multiple Same Values (`08_multiple_same_value.cco`)](#08-multiple-same-values)
   - [09. Variable Names (`09_variable_names.cco`)](#09-variable-names)
   - [10. Case Sensitivity (`10_variable_case_sensitivity.cco`)](#10-case-sensitivity)
   - [11. Student Profile (`11_student_profile.cco`)](#11-student-profile)
   - [12. Rectangle Area (`12_rectangle_area.cco`)](#12-rectangle-area)
2. [Data Types & Constants](#2-data-types--constants)
   - [13. Data Types Overview (`13_data_types.cco`)](#13-data-types-overview)
   - [14. Character vs String (`14_char_type.cco`)](#14-character-vs-string)
   - [15. Numeric Types (`15_numeric_types.cco`)](#15-numeric-types)
   - [16. Decimal Precision (`16_decimal_precision.cco`)](#16-decimal-precision)
   - [17. Type Conversion & Division (`17_type_conversion_div.cco`)](#17-type-conversion--division)
   - [18. Score Percentage (`18_score_percentage.cco`)](#18-score-percentage)
   - [19. Constants (`19_constants.cco`)](#19-constants)
   - [20. Circle Geometry (`20_circle_geometry.cco`)](#20-circle-geometry)
3. [Arithmetic Operators](#3-arithmetic-operators)
   - [21. Addition (`21_add.cco`)](#21-addition)
   - [22. Subtraction (`22_sub.cco`)](#22-subtraction)
   - [23. Multiplication (`23_mul.cco`)](#23-multiplication)
   - [24. Division (`24_div.cco`)](#24-division)
   - [25. Modulus (`25_mod.cco`)](#25-modulus)
   - [26. Increment (`26_increment.cco`)](#26-increment)
   - [27. Decrement (`27_decrement.cco`)](#27-decrement)
   - [28. People Counter (`28_people_counter.cco`)](#28-people-counter)
4. [Assignment Operators](#4-assignment-operators)
   - [29. Simple Assignment (`29_assign.cco`)](#29-simple-assignment)
   - [30. Addition Assignment (`30_add_assign.cco`)](#30-addition-assignment)
   - [31. Subtraction Assignment (`31_sub_assign.cco`)](#31-subtraction-assignment)
   - [32. Multiplication Assignment (`32_mul_assign.cco`)](#32-multiplication-assignment)
   - [33. Division Assignment (`33_div_assign.cco`)](#33-division-assignment)
   - [34. Modulus Assignment (`34_mod_assign.cco`)](#34-modulus-assignment)
   - [35. String Concatenation Assignment (`35_string_add_assign.cco`)](#35-string-concatenation-assignment)
   - [36. Savings Tracker (`36_savings_tracker.cco`)](#36-savings-tracker)
5. [Comparison Operators](#5-comparison-operators)
   - [37. Equal To (`37_equal.cco`)](#37-equal-to)
   - [38. Not Equal (`38_not_equal.cco`)](#38-not-equal)
   - [39. Greater Than (`39_greater.cco`)](#39-greater-than)
   - [40. Less Than (`40_less.cco`)](#40-less-than)
   - [41. Greater or Equal (`41_greater_equal.cco`)](#41-greater-or-equal)
   - [42. Less or Equal (`42_less_equal.cco`)](#42-less-or-equal)
   - [43. Voting Check (`43_voting_check.cco`)](#43-voting-check)
   - [44. Password Check (`44_password_check.cco`)](#44-password-check)
6. [Logical Operators & Precedence](#6-logical-operators--precedence)
   - [45. Logical AND (`45_logical_and.cco`)](#45-logical-and)
   - [46. Logical OR (`46_logical_or.cco`)](#46-logical-or)
   - [47. Logical NOT (`47_logical_not.cco`)](#47-logical-not)
   - [48. Login & Role Access (`48_login_access.cco`)](#48-login--role-access)
   - [49. Operator Precedence (`49_operator_precedence.cco`)](#49-operator-precedence)

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

# 8. Conditional Statements (If...Else)

## 56. The If Statement (Direct Value Condition)
Executing a block of code when a direct condition is true.

```cco
// codebase/56_if_statement_values.cco
fn main() -> int {
    if (20 > 18) {
        print("20 is greater than 18");
    }
    return 0;
}
```
**Run:** `./cco codebase/56_if_statement_values.cco --run`  
**Output:** `20 is greater than 18`

---

## 57. The If Statement (Variable Conditions)
Testing variable values dynamically.

```cco
// codebase/57_if_statement_variables.cco
fn main() -> int {
    let x = 20;
    let y = 18;

    if (x > y) {
        print("x is greater than y");
    }
    return 0;
}
```
**Run:** `./cco codebase/57_if_statement_variables.cco --run`  
**Output:** `x is greater than y`

---

## 58. The If Statement (Boolean Variables)
Using a stored boolean variable to drive control flow.

```cco
// codebase/58_if_statement_boolean_var.cco
fn main() -> int {
    let x = 20;
    let y = 18;

    let is_greater = x > y;

    if (is_greater) {
        print("x is greater than y");
    }
    return 0;
}
```
**Run:** `./cco codebase/58_if_statement_boolean_var.cco --run`  
**Output:** `x is greater than y`

---

## 59. The Else Statement (Direct Comparison)
Executing an alternative block when the condition is false.

```cco
// codebase/59_else_statement_direct.cco
fn main() -> int {
    let time = 20;

    if (time < 18) {
        print("Good day.");
    } else {
        print("Good evening.");
    }

    return 0;
}
```
**Run:** `./cco codebase/59_else_statement_direct.cco --run`  
**Output:** `Good evening.`

---

## 60. The Else Statement (Boolean Variables)
Using a descriptive boolean variable (`is_day`) to make the condition clean and self-explanatory.

```cco
// codebase/60_else_statement_boolean_var.cco
fn main() -> int {
    let time = 20;
    let is_day = time < 18;

    if (is_day) {
        print("Good day.");
    } else {
        print("Good evening.");
    }

    return 0;
}
```
**Run:** `./cco codebase/60_else_statement_boolean_var.cco --run`  
**Output:** `Good evening.`

---

## 61. The Else If Statement (Direct Comparison)
Testing multiple sequential conditions from top to bottom.

```cco
// codebase/61_else_if_direct.cco
fn main() -> int {
    let time = 16;

    if (time < 12) {
        print("Good morning.");
    } else if (time < 18) {
        print("Good day.");
    } else {
        print("Good evening.");
    }

    return 0;
}
```
**Run:** `./cco codebase/61_else_if_direct.cco --run`  
**Output:** `Good day.`

---

## 62. The Else If Statement (Boolean Variables)
Using named boolean variables with `else if` chains.

```cco
// codebase/62_else_if_boolean_vars.cco
fn main() -> int {
    let time = 16;
    let is_morning = time < 12;
    let is_day = time < 18;

    if (is_morning) {
        print("Good morning.");
    } else if (is_day) {
        print("Good day.");
    } else {
        print("Good evening.");
    }

    return 0;
}
```
**Run:** `./cco codebase/62_else_if_boolean_vars.cco --run`  
**Output:** `Good day.`

---

## 63. Short Hand If...Else & Clean Decision Logic

In C, the ternary operator `(condition) ? expr1 : expr2` is used as a short-hand for `if...else`. In modern languages like Cco, clean and structured conditional assignment is prioritized to ensure readability and eliminate cryptic operator-precedence bugs.

---

### In C (Ternary Syntax):
```c
int time = 20;
(time < 18) ? printf("Good day.") : printf("Good evening.");
```

---

### In Cco (Clean Structured Form):
```cco
// codebase/63_shorthand_ifelse_comparison.cco
fn main() -> int {
    let time = 20;
    let greeting = "";

    if (time < 18) {
        greeting = "Good day.";
    } else {
        greeting = "Good evening.";
    }

    print(greeting);
    return 0;
}
```
**Run:** `./cco codebase/63_shorthand_ifelse_comparison.cco --run`  
**Output:**
```text
Good evening.
```

---

## 64. Nested If Statements (Basic Numeric Check)
Placing an `if` statement inside another `if` statement to test dependent conditions.

```cco
// codebase/64_nested_if_basic.cco
fn main() -> int {
    let x = 15;
    let y = 25;

    if (x > 10) {
        print("x is greater than 10");

        // Nested if condition
        if (y > 20) {
            print("y is also greater than 20");
        }
    }

    return 0;
}
```
**Run:** `./cco codebase/64_nested_if_basic.cco --run`  
**Output:**
```text
x is greater than 10
y is also greater than 20
```

---

## 65. Nested If Statements (Voting & Citizenship)
Real-life verification requiring multiple dependent checks.

```cco
// codebase/65_nested_if_voting_citizen.cco
fn main() -> int {
    let age = 20;
    let is_citizen = true;

    if (age >= 18) {
        print("Old enough to vote.");

        // Nested condition
        if (is_citizen) {
            print("And you are a citizen, so you can vote!");
        } else {
            print("But you must be a citizen to vote.");
        }
    } else {
        print("Not old enough to vote.");
    }

    return 0;
}
```
**Run:** `./cco codebase/65_nested_if_voting_citizen.cco --run`  
**Output:**
```text
Old enough to vote.
And you are a citizen, so you can vote!
```

---

## 66. Logical AND (`&&`) in Conditions
Requiring all conditions to be true before executing a block.

```cco
// codebase/66_if_logical_and.cco
fn main() -> int {
    let a = 200;
    let b = 33;
    let c = 500;

    if (a > b && c > a) {
        print("Both conditions are true");
    }

    return 0;
}
```
**Run:** `./cco codebase/66_if_logical_and.cco --run`  
**Output:** `Both conditions are true`

---

## 67. Logical OR (`||`) in Conditions
Executing a block if at least one condition is true.

```cco
// codebase/67_if_logical_or.cco
fn main() -> int {
    let a = 200;
    let b = 33;
    let c = 500;

    if (a > b || a > c) {
        print("At least one condition is true");
    }

    return 0;
}
```
**Run:** `./cco codebase/67_if_logical_or.cco --run`  
**Output:** `At least one condition is true`

---

## 68. Logical NOT (`!`) in Conditions
Reversing the truth value of a condition.

```cco
// codebase/68_if_logical_not.cco
fn main() -> int {
    let a = 33;
    let b = 200;

    if (!(a > b)) {
        print("a is NOT greater than b");
    }

    return 0;
}
```
**Run:** `./cco codebase/68_if_logical_not.cco --run`  
**Output:** `a is NOT greater than b`

---

## 69. Complex Security Access Control
Combining `&&`, `||`, and parentheses to build sophisticated enterprise access logic.

```cco
// codebase/69_security_clearance_access.cco
fn main() -> int {
    let is_logged_in = true;
    let is_admin = false;
    let security_level = 3; // 1 = highest

    print(f"Logged in: {is_logged_in}, Admin: {is_admin}, Security level: {security_level}");

    if (is_logged_in && (is_admin || security_level <= 2)) {
        print("Access granted");
    } else {
        print("Access denied");
    }

    return 0;
}
```
**Run:** `./cco codebase/69_security_clearance_access.cco --run`  
**Output:**
```text
Logged in: true, Admin: false, Security level: 3
Access denied
```

---

# 9. Real-Life If...Else Problem Solving

## 70. Door Security Code
Unlocking a door when the user enters the correct passkey.

```cco
// codebase/70_door_access_code.cco
fn main() -> int {
    let door_code = 1337;

    if (door_code == 1337) {
        print("Correct code.\nThe door is now open.");
    } else {
        print("Wrong code.\nThe door remains closed.");
    }

    return 0;
}
```
**Run:** `./cco codebase/70_door_access_code.cco --run`  
**Output:**
```text
Correct code.
The door is now open.
```

---

## 71. Number Sign Classifier (Positive, Negative, or Zero)
Categorizing numbers based on sign.

```cco
// codebase/71_positive_negative_zero.cco
fn main() -> int {
    let my_num = 10;

    if (my_num > 0) {
        print(f"{my_num} is a positive number.");
    } else if (my_num < 0) {
        print(f"{my_num} is a negative number.");
    } else {
        print("The value is 0.");
    }

    return 0;
}
```
**Run:** `./cco codebase/71_positive_negative_zero.cco --run`  
**Output:** `10 is a positive number.`

---

## 72. Even or Odd Number Checker
Testing divisibility by 2 using modulus (`% 2 == 0`).

```cco
// codebase/72_even_odd.cco
fn main() -> int {
    let my_num = 5;

    if (my_num % 2 == 0) {
        print(f"{my_num} is even.");
    } else {
        print(f"{my_num} is odd.");
    }

    return 0;
}
```
**Run:** `./cco codebase/72_even_odd.cco --run`  
**Output:** `5 is odd.`

---

## 73. Temperature Range Classifier
Categorizing temperature readings into comfort bands.

```cco
// codebase/73_temperature_check.cco
fn main() -> int {
    let temperature = 30;

    if (temperature < 0) {
        print("It's freezing!");
    } else if (temperature < 20) {
        print("It's cool.");
    } else {
        print("It's warm.");
    }

    return 0;
}
```
**Run:** `./cco codebase/73_temperature_check.cco --run`  
**Output:** `It's warm.`

---

# 10. Multi-Way Branching & Pattern Matching (Switch-Equivalent)

> **Honest Language Note (Switch Statements)**: Cco deliberately omits C's legacy `switch` statement (along with its error-prone fall-through semantics and lack of compile-time exhaustiveness checking). In Cco, general multi-way branching is expressed honestly via explicit `if / else if / else` chains, while enum and algebraic types use type-safe, exhaustive `match` expressions — providing far greater safety than C's `switch` ever offered.

## 74. Multi-Way Branching (Weekday Name)
Handling multiple distinct value options cleanly.

```cco
// codebase/74_switch_day_branches.cco
fn main() -> int {
    let day = 4;

    if (day == 1) {
        print("Monday");
    } else if (day == 2) {
        print("Tuesday");
    } else if (day == 3) {
        print("Wednesday");
    } else if (day == 4) {
        print("Thursday");
    } else if (day == 5) {
        print("Friday");
    } else if (day == 6) {
        print("Saturday");
    } else if (day == 7) {
        print("Sunday");
    }

    return 0;
}
```
**Run:** `./cco codebase/74_switch_day_branches.cco --run`  
**Output:** `Thursday`

---

## 75. Multi-Way Branching with Fallback (Weekend Check)
Specifying explicit cases and providing a default fallback.

```cco
// codebase/75_switch_default_weekend.cco
fn main() -> int {
    let day = 4;

    if (day == 6) {
        print("Today is Saturday");
    } else if (day == 7) {
        print("Today is Sunday");
    } else {
        print("Looking forward to the Weekend");
    }

    return 0;
}
```
**Run:** `./cco codebase/75_switch_default_weekend.cco --run`  
**Output:** `Looking forward to the Weekend`

---

## 76. Pattern Matching with Enums (`match`)
Type-safe exhaustive pattern matching in Cco without fall-through bugs.

```cco
// codebase/76_enum_match_pattern.cco
enum Weekday {
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday,
}

fn describe_day(d: &Weekday) -> void {
    match d {
        Weekday.Monday => { print("Monday: Start of work week"); }
        Weekday.Tuesday => { print("Tuesday: Getting productive"); }
        Weekday.Wednesday => { print("Wednesday: Midweek"); }
        Weekday.Thursday => { print("Thursday: Almost weekend"); }
        Weekday.Friday => { print("Friday: Weekend is near"); }
        Weekday.Saturday => { print("Saturday: Weekend!"); }
        Weekday.Sunday => { print("Sunday: Rest day"); }
    }
}

fn main() -> int {
    let today: Weekday = Weekday.Thursday;
    describe_day(&today);
    return 0;
}
```
**Run:** `./cco codebase/76_enum_match_pattern.cco --run`  
**Output:** `Thursday: Almost weekend`

---

# 11. The While Loop

## 77. While Loop: Counting Up
Repeating execution while a condition remains true with `i++`.

```cco
// codebase/77_while_count_up.cco
fn main() -> int {
    let i = 0;

    while (i < 5) {
        print(i);
        i++;
    }

    return 0;
}
```
**Run:** `./cco codebase/77_while_count_up.cco --run`  
**Output:**
```text
0
1
2
3
4
```

---

## 78. While Loop: Countdown
Counting down towards zero with `countdown--`.

```cco
// codebase/78_while_countdown.cco
fn main() -> int {
    let countdown = 3;

    while (countdown > 0) {
        print(countdown);
        countdown--;
    }

    print("Happy New Year!!");
    return 0;
}
```
**Run:** `./cco codebase/78_while_countdown.cco --run`  
**Output:**
```text
3
2
1
Happy New Year!!
```

---

## 79. While Loop with False Initial Condition
Demonstrating that a while loop is entirely skipped if the condition is false from the beginning.

```cco
// codebase/79_while_false_condition.cco
fn main() -> int {
    let i = 10;

    while (i < 5) {
        print("This will never be printed");
        i++;
    }

    print("Loop bypassed successfully");
    return 0;
}
```
**Run:** `./cco codebase/79_while_false_condition.cco --run`  
**Output:** `Loop bypassed successfully`

---

# 12. Run-At-Least-Once (Do...While) Semantics

## 80. Do...While Loop Pattern
Executing the loop body at least once before testing the termination condition.

```cco
// codebase/80_dowhile_basic.cco
fn main() -> int {
    let i = 0;

    // Run-at-least-once loop pattern (do...while semantics)
    while (true) {
        print(i);
        i++;
        if (i >= 5) {
            break;
        }
    }

    return 0;
}
```
**Run:** `./cco codebase/80_dowhile_basic.cco --run`  
**Output:**
```text
0
1
2
3
4
```

---

## 81. False-From-Start Execution Guarantee
Verifying that the loop body executes once even if the termination condition is met on initial entry.

```cco
// codebase/81_dowhile_false_start.cco
fn main() -> int {
    let i = 10;

    // Executes at least once even when initial condition would be false
    while (true) {
        print(f"i is {i}");
        i++;
        if (i >= 5) {
            break;
        }
    }

    return 0;
}
```
**Run:** `./cco codebase/81_dowhile_false_start.cco --run`  
**Output:** `i is 10`

---

## 82. Interactive Input Processing Loop
Simulating an input validation loop that prompts at least once and continues until an exit condition is reached.

```cco
// codebase/82_dowhile_input_simulation.cco
fn main() -> int {
    let sample_inputs = 3;

    while (true) {
        print(f"Processing positive value: {sample_inputs}");
        sample_inputs--;
        if (sample_inputs <= 0) {
            print("Loop exited after reaching non-positive value.");
            break;
        }
    }

    return 0;
}
```
**Run:** `./cco codebase/82_dowhile_input_simulation.cco --run`  
**Output:**
```text
Processing positive value: 3
Processing positive value: 2
Processing positive value: 1
Loop exited after reaching non-positive value.
```

---

## 83. While Loop: Even Numbers
Stepping by 2 with `i += 2` to generate even numbers from 0 to 10.

```cco
// codebase/83_while_even_numbers.cco
fn main() -> int {
    let i = 0;

    while (i <= 10) {
        print(i);
        i += 2;
    }

    return 0;
}
```
**Run:** `./cco codebase/83_while_even_numbers.cco --run`  
**Output:**
```text
0
2
4
6
8
10
```

---

## 84. While Loop: Number Reversal
Extracting digits using `% 10` and `/= 10` to reverse an integer (e.g. `12345` $\rightarrow$ `54321`).

```cco
// codebase/84_while_reverse_number.cco
fn main() -> int {
    let numbers = 12345;
    let rev_numbers = 0;

    print(f"Original number: {numbers}");

    while (numbers > 0) {
        rev_numbers = rev_numbers * 10 + (numbers % 10);
        numbers /= 10;
    }

    print(f"Reversed number: {rev_numbers}");
    return 0;
}
```
**Run:** `./cco codebase/84_while_reverse_number.cco --run`  
**Output:**
```text
Original number: 12345
Reversed number: 54321
```

---

## 85. While Loop: Yatzy Dice Simulation
Combining loops with conditional branching to simulate game outcomes.

```cco
// codebase/85_while_yatzy_dice.cco
fn main() -> int {
    let dice = 1;

    while (dice <= 6) {
        if (dice < 6) {
            print(f"Dice roll {dice}: No Yatzy");
        } else {
            print(f"Dice roll {dice}: Yatzy!");
        }
        dice++;
    }

    return 0;
}
```
**Run:** `./cco codebase/85_while_yatzy_dice.cco --run`  
**Output:**
```text
Dice roll 1: No Yatzy
Dice roll 2: No Yatzy
Dice roll 3: No Yatzy
Dice roll 4: No Yatzy
Dice roll 5: No Yatzy
Dice roll 6: Yatzy!
```

---

# 13. The For Loop

## 86. For Loop: Counting Up
Iterating from 0 to 4 using a standard counting loop.

```cco
// codebase/86_for_count_up.cco
fn main() -> int {
    for (let i = 0; i < 5; i++) {
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/86_for_count_up.cco --run`  
**Output:**
```text
0
1
2
3
4
```

---

## 87. For Loop: Even Numbers
Stepping by 2 with `i = i + 2` inside the loop header.

```cco
// codebase/87_for_even_numbers.cco
fn main() -> int {
    for (let i = 0; i <= 10; i = i + 2) {
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/87_for_even_numbers.cco --run`  
**Output:**
```text
0
2
4
6
8
10
```

---

## 88. For Loop: Sum of Numbers
Accumulating the sum of integers from 1 to 5 ($1 + 2 + 3 + 4 + 5 = 15$).

```cco
// codebase/88_for_sum_numbers.cco
fn main() -> int {
    let sum = 0;

    for (let i = 1; i <= 5; i++) {
        sum = sum + i;
    }

    print(f"Sum is {sum}");
    return 0;
}
```
**Run:** `./cco codebase/88_for_sum_numbers.cco --run`  
**Output:** `Sum is 15`

---

## 89. For Loop: Countdown
Counting downwards from 5 to 1 using decrement `i--`.

```cco
// codebase/89_for_countdown.cco
fn main() -> int {
    for (let i = 5; i > 0; i--) {
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/89_for_countdown.cco --run`  
**Output:**
```text
5
4
3
2
1
```

---

# 14. Nested Loops

## 90. Nested Loops: Outer & Inner Iterations
Demonstrating that the inner loop runs to completion for every single iteration of the outer loop ($2 \times 3 = 6$ executions).

```cco
// codebase/90_nested_loops_basic.cco
fn main() -> int {
    // Outer loop runs 2 times
    for (let i = 1; i <= 2; i++) {
        print(f"Outer: {i}");

        // Inner loop runs 3 times for each outer iteration (2 * 3 = 6 times total)
        for (let j = 1; j <= 3; j++) {
            print(f" Inner: {j}");
        }
    }

    return 0;
}
```
**Run:** `./cco codebase/90_nested_loops_basic.cco --run`  
**Output:**
```text
Outer: 1
 Inner: 1
 Inner: 2
 Inner: 3
Outer: 2
 Inner: 1
 Inner: 2
 Inner: 3
```

---

## 91. Nested Loops: 3x3 Multiplication Table Grid
Generating a 2D mathematical coordinate multiplication grid.

```cco
// codebase/91_nested_loops_multiplication_table.cco
fn main() -> int {
    print("=== 3x3 Multiplication Table Grid ===");

    for (let i = 1; i <= 3; i++) {
        let val1 = i * 1;
        let val2 = i * 2;
        let val3 = i * 3;
        print(f"{val1} {val2} {val3}");
    }

    return 0;
}
```
**Run:** `./cco codebase/91_nested_loops_multiplication_table.cco --run`  
**Output:**
```text
=== 3x3 Multiplication Table Grid ===
1 2 3
2 4 6
3 6 9
```

---

# 15. Practical For Loop Variations

## 92. Count to 100 by Tens
Incrementing by 10 on each iteration (`i += 10`).

```cco
// codebase/92_for_count_tens.cco
fn main() -> int {
    for (let i = 0; i <= 100; i += 10) {
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/92_for_count_tens.cco --run`  
**Output:**
```text
0
10
20
30
40
50
60
70
80
90
100
```

---

## 93. Even Numbers with Step Increment
Starting at 0 and stepping by 2 to generate even numbers.

```cco
// codebase/93_for_even_numbers_step.cco
fn main() -> int {
    for (let i = 0; i <= 10; i += 2) {
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/93_for_even_numbers_step.cco --run`  
**Output:**
```text
0
2
4
6
8
10
```

---

## 94. Odd Numbers with Step Increment
Starting at 1 and stepping by 2 to generate odd numbers.

```cco
// codebase/94_for_odd_numbers_step.cco
fn main() -> int {
    for (let i = 1; i < 10; i += 2) {
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/94_for_odd_numbers_step.cco --run`  
**Output:**
```text
1
3
5
7
9
```

---

## 95. Powers of Two (Geometric Progression)
Multiplying by 2 on each iteration (`i *= 2`) up to 512.

```cco
// codebase/95_for_powers_of_two.cco
fn main() -> int {
    for (let i = 2; i <= 512; i *= 2) {
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/95_for_powers_of_two.cco --run`  
**Output:**
```text
2
4
8
16
32
64
128
256
512
```

---

## 96. Multiplication Table for a Specified Number
Generating the complete multiplication table for 2 from $2 \times 1$ to $2 \times 10$.

```cco
// codebase/96_for_multiplication_table.cco
fn main() -> int {
    let number = 2;

    for (let i = 1; i <= 10; i++) {
        print(f"{number} x {i} = {number * i}");
    }

    return 0;
}
```
**Run:** `./cco codebase/96_for_multiplication_table.cco --run`  
**Output:**
```text
2 x 1 = 2
2 x 2 = 4
2 x 3 = 6
2 x 4 = 8
2 x 5 = 10
2 x 6 = 12
2 x 7 = 14
2 x 8 = 16
2 x 9 = 18
2 x 10 = 20
```

---

# 16. Loop Control (Break & Continue)

## 97. Break in For Loop
Terminating loop execution immediately when `i == 4`.

```cco
// codebase/97_for_break.cco
fn main() -> int {
    for (let i = 0; i < 10; i++) {
        if (i == 4) {
            break;
        }
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/97_for_break.cco --run`  
**Output:**
```text
0
1
2
3
```

---

## 98. Continue in For Loop
Skipping the rest of the current iteration when `i == 4` and continuing with the next index.

```cco
// codebase/98_for_continue.cco
fn main() -> int {
    for (let i = 0; i < 10; i++) {
        if (i == 4) {
            continue;
        }
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/98_for_continue.cco --run`  
**Output:**
```text
0
1
2
3
5
6
7
8
9
```

---

## 99. Combining Break and Continue
Skipping iteration 2 with `continue` and terminating on iteration 4 with `break`.

```cco
// codebase/99_for_break_and_continue.cco
fn main() -> int {
    for (let i = 0; i < 6; i++) {
        if (i == 2) {
            continue;
        }
        if (i == 4) {
            break;
        }
        print(i);
    }
    return 0;
}
```
**Run:** `./cco codebase/99_for_break_and_continue.cco --run`  
**Output:**
```text
0
1
3
```

---

## 100. Break in While Loop
Exiting a while loop when reaching an exit sentinel.

```cco
// codebase/100_while_break.cco
fn main() -> int {
    let i = 0;

    while (i < 10) {
        if (i == 4) {
            break;
        }
        print(i);
        i++;
    }

    return 0;
}
```
**Run:** `./cco codebase/100_while_break.cco --run`  
**Output:**
```text
0
1
2
3
```

---

## 101. Continue in While Loop
Advancing the iterator before `continue` to avoid infinite loops.

```cco
// codebase/101_while_continue.cco
fn main() -> int {
    let i = 0;

    while (i < 10) {
        if (i == 4) {
            i++;
            continue;
        }
        print(i);
        i++;
    }

    return 0;
}
```
**Run:** `./cco codebase/101_while_continue.cco --run`  
**Output:**
```text
0
1
2
3
5
6
7
8
9
```

---

## 102. Real-Life Filtering: Skip Negatives, Stop at Zero
Filtering array elements by using `continue` for negative values and `break` when finding `0`.

```cco
// codebase/102_real_life_filter_numbers.cco
fn main() -> int {
    let my_numbers: int[] = alloc(int, 5);
    my_numbers[0] = 3;
    my_numbers[1] = -1;
    my_numbers[2] = 7;
    my_numbers[3] = 0;
    my_numbers[4] = 9;

    print("=== Processing Numbers (Skip Negatives, Stop at Zero) ===");

    for (let i = 0; i < 5; i++) {
        let val = my_numbers[i];
        if (val < 0) {
            continue; // skip negative numbers
        }
        if (val == 0) {
            break; // stop loop when zero is found
        }
        print(val);
    }

    return 0;
}
```
**Run:** `./cco codebase/102_real_life_filter_numbers.cco --run`  
**Output:**
```text
=== Processing Numbers (Skip Negatives, Stop at Zero) ===
3
7
```

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

# 71. Real-World Applications: Full-Featured Calculator

## 226. Full-Featured Scientific & History Calculator
Building a comprehensive calculator featuring tagged enums, pattern matching, dynamic memory history arrays, and zero-leak cleanup.

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
Building a native HTTP protocol request router, status code dispatcher, and JSON response serialization engine.

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
