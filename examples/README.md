# Cco Example Program Gallery

Welcome to the Cco example program gallery. These examples demonstrate the features, standard library, single ownership rules, and object-oriented syntax of the Cco programming language.

## Examples Index

| File | Description | Features Covered |
| :--- | :--- | :--- |
| [`01_hello_world.cco`](01_hello_world.cco) | Hello World & primitive types | Variables (`let`), primitive types, `print` statements |
| [`02_fibonacci.cco`](02_fibonacci.cco) | Iterative & recursive Fibonacci | Functions, `while` loops, `if` statements, recursion |
| [`03_point_distance.cco`](03_point_distance.cco) | Geometric Point distance | Classes (`class`), methods (`self`), `sqrt`, `pow` |
| [`04_string_builder.cco`](04_string_builder.cco) | String manipulation | `concat`, `len`, `substring`, `equals`, string memory safety |
| [`05_array_sum.cco`](05_array_sum.cco) | Array allocation & iteration | Dynamic allocation (`alloc`), array indexing, `for` loops |
| [`06_word_count.cco`](06_word_count.cco) | File I/O & word counting | File reading (`read_file`), character inspection (`char_at`) |
| [`07_ownership_demo.cco`](07_ownership_demo.cco) | Ownership & move semantics | Ownership transfer (moves), borrowed parameters (`&Class`) |
| [`08_stack_data_structure.cco`](08_stack_data_structure.cco) | OOP Stack data structure | Classes with heap arrays, method state mutation |

## Compiling & Running Examples

To compile and run any example program:

```bash
# Build the Cco compiler (if not already built)
make cco

# Compile and execute an example program
./cco examples/01_hello_world.cco && ./build/output
```

Or run all examples with Valgrind leak checking:

```bash
for f in examples/*.cco; do
    echo "=== Running $f ==="
    ./cco "$f" && valgrind --leak-check=full ./build/output
done
```
