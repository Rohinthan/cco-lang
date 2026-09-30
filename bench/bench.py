def estimate_pi(steps: float, n_iterations: int) -> float:
    step_size = 1.0 / steps
    total = 0.0
    x = 0.5 * step_size
    i = 0
    while i < n_iterations:
        total = total + 4.0 / (1.0 + x * x)
        x = x + step_size
        i = i + 1
    return total * step_size

def fib(n: int) -> int:
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

if __name__ == "__main__":
    print("==================================================")
    print("              Python Benchmark                    ")
    print("==================================================")
    print("[1] Running 10,000,000 float integration steps to compute Pi...")
    pi = estimate_pi(10000000.0, 10000000)
    print(f"Result (Estimated Pi):\n{pi}")
    print("[2] Running recursive Fibonacci(35)...")
    f = fib(35)
    print(f"Result (Fibonacci 35):\n{f}")
    print("==================================================")
