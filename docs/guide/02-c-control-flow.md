# Part 2: Control Flow & Logic (If/Else, Switch-Equivalent, Loops)

[← Back to Guide Index](README.md)

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


---


[← Back to Guide Index](README.md)
