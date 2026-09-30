fn estimate_pi(steps: f64, n_iterations: i32) -> f64 {
    let step_size: f64 = 1.0 / steps;
    let mut sum: f64 = 0.0;
    let mut x: f64 = 0.5 * step_size;
    let mut i: i32 = 0;
    while i < n_iterations {
        sum = sum + 4.0 / (1.0 + x * x);
        x = x + step_size;
        i = i + 1;
    }
    sum * step_size
}

fn fib(n: i32) -> i32 {
    if n <= 1 {
        return n;
    }
    fib(n - 1) + fib(n - 2)
}

fn main() {
    println!("==================================================");
    println!("             Rust Benchmark (rustc)               ");
    println!("==================================================");
    println!("[1] Running 10,000,000 float integration steps to compute Pi...");
    let pi = estimate_pi(10000000.0, 10000000);
    println!("Result (Estimated Pi):\n{}", pi);
    println!("[2] Running recursive Fibonacci(35)...");
    let f = fib(35);
    println!("Result (Fibonacci 35):\n{}", f);
    println!("==================================================");
}
