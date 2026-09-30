#include <stdio.h>

double estimate_pi(double steps, int n_iterations) {
    double step_size = 1.0 / steps;
    double sum = 0.0;
    double x = 0.5 * step_size;
    int i = 0;
    while (i < n_iterations) {
        sum = sum + 4.0 / (1.0 + x * x);
        x = x + step_size;
        i = i + 1;
    }
    return sum * step_size;
}

int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    printf("==================================================\n");
    printf("              C Benchmark (GCC)                   \n");
    printf("==================================================\n");
    printf("[1] Running 10,000,000 float integration steps to compute Pi...\n");
    double pi = estimate_pi(10000000.0, 10000000);
    printf("Result (Estimated Pi):\n%g\n", pi);
    printf("[2] Running recursive Fibonacci(35)...\n");
    int f = fib(35);
    printf("Result (Fibonacci 35):\n%d\n", f);
    printf("==================================================\n");
    return 0;
}
