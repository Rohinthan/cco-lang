#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Cco Compiler - Phase 9 Research-Grade Benchmarking, Reproducibility & Evaluation
Comprehensive statistical benchmarking harness supporting:
- Multi-run statistical execution time (min, median, max, mean, stddev) with N=10 default
- Output correctness validation across Reference C11, -O0, -O1, -O2, and PGO
- Compiler pass timing decomposition (--time-passes)
- Detailed binary size decomposition (Total, .text, .rodata, .data)
- Native instruction & register allocation metrics (--dump-code-stats)
- PGO effectiveness classification (improved, unchanged, regressed)
- Deterministic byte-for-byte reproducibility verification (--check-reproducibility)
- Regression detection with configurable thresholds (--check-regression)
- Machine-readable CSV output generation in bench/results/
"""

import os
import sys
import time
import math
import statistics
import subprocess
import argparse
import platform
import csv
from datetime import datetime

BENCHMARKS = [
    "bench_arith",
    "bench_float",
    "bench_float_expr",
    "bench_loop",
    "bench_fib",
    "bench_call",
    "bench_branch",
    "bench_mandelbrot",
    "bench_hot_cold",
    "bench_regpressure",
    "bench_mixed_pressure",
    "bench_nested_call",
]

BUILD_DIR = "build/bench_out"
REPRO_DIR = "build/repro_out"

def run_cmd(cmd):
    p = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return p.returncode, p.stdout.strip(), p.stderr.strip()

def measure_compile(cmd):
    start = time.perf_counter()
    ret, out, err = run_cmd(cmd)
    elapsed = (time.perf_counter() - start) * 1000.0  # ms
    return ret, out, err, elapsed

def time_binary(path, runs=10):
    times = []
    last_output = ""
    for _ in range(runs):
        start = time.perf_counter()
        p = subprocess.run([path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        elapsed = (time.perf_counter() - start) * 1000.0  # ms
        if p.returncode != 0:
            return None, p.stderr.strip()
        last_output = p.stdout.strip()
        times.append(elapsed)

    stats = {
        "min": min(times),
        "median": statistics.median(times),
        "max": max(times),
        "mean": statistics.mean(times),
        "stddev": statistics.stdev(times) if len(times) > 1 else 0.0,
        "runs": times
    }
    return stats, last_output

def parse_code_stats(src_file, extra_flags=""):
    cmd = f"./cco {src_file} {extra_flags} --dump-code-stats"
    ret, out, _ = run_cmd(cmd)
    if ret != 0:
        return {}
    stats = {}
    for line in out.splitlines():
        line = line.strip()
        if ":" in line:
            parts = line.split(":", 1)
            k = parts[0].strip()
            v = parts[1].strip()
            stats[k] = v
    return stats

def parse_pass_timings(src_file, extra_flags=""):
    cmd = f"./cco {src_file} {extra_flags} --time-passes=csv -o /tmp/pass_tmp_bin"
    ret, out, _ = run_cmd(cmd)
    if os.path.exists("/tmp/pass_tmp_bin"):
        os.remove("/tmp/pass_tmp_bin")
    timings = {}
    if ret == 0:
        for line in out.splitlines():
            line = line.strip()
            if "," in line:
                parts = line.split(",")
                if len(parts) == 2:
                    try:
                        timings[parts[0].strip()] = float(parts[1].strip())
                    except ValueError:
                        pass
    return timings

def get_elf_sizes(path):
    if not os.path.exists(path):
        return {"total": 0, "text": 0, "rodata": 0, "data": 0}
    total_size = os.path.getsize(path)
    text_size, rodata_size, data_size = 0, 0, 0

    # 1. Try Section Headers (GCC / External Linker)
    p = subprocess.run(["readelf", "-W", "-S", path], stdout=subprocess.PIPE, text=True)
    if "Section Headers:" in p.stdout:
        for line in p.stdout.splitlines():
            parts = line.strip().split()
            if len(parts) >= 6:
                if parts[1] == ".text":
                    try: text_size = int(parts[5], 16)
                    except ValueError: pass
                elif parts[1] == ".rodata":
                    try: rodata_size = int(parts[5], 16)
                    except ValueError: pass
                elif parts[1] == ".data":
                    try: data_size = int(parts[5], 16)
                    except ValueError: pass
        return {"total": total_size, "text": text_size, "rodata": rodata_size, "data": data_size}

    # 2. Try Program Headers (Cco Internal Linker)
    p = subprocess.run(["readelf", "-l", path], stdout=subprocess.PIPE, text=True)
    lines = p.stdout.splitlines()
    for i, line in enumerate(lines):
        if "LOAD" in line and i + 1 < len(lines):
            next_line = lines[i+1].strip()
            parts = next_line.split()
            if len(parts) >= 3:
                try:
                    filesiz = int(parts[0], 16)
                    if "E" in next_line:
                        text_size = filesiz
                    elif "R" in next_line and int(line.split()[1], 16) > 0:
                        rodata_size = filesiz
                    elif "W" in next_line:
                        data_size = filesiz
                except ValueError:
                    pass

    return {"total": total_size, "text": text_size, "rodata": rodata_size, "data": data_size}

def capture_environment(results_dir):
    os.makedirs(results_dir, exist_ok=True)
    env_file = os.path.join(results_dir, "environment.txt")
    
    cpu_model = "Unknown"
    try:
        with open("/proc/cpuinfo", "r") as f:
            for line in f:
                if "model name" in line:
                    cpu_model = line.split(":", 1)[1].strip()
                    break
    except Exception:
        pass

    gcc_ver = "Unknown"
    _, gcc_out, _ = run_cmd("gcc --version")
    if gcc_out:
        gcc_ver = gcc_out.splitlines()[0]

    git_rev = "Unknown"
    _, git_out, _ = run_cmd("git rev-parse HEAD")
    if git_out:
        git_rev = git_out

    git_status = "Clean"
    ret, status_out, _ = run_cmd("git status --porcelain")
    if status_out:
        git_status = "Dirty (Uncommitted Changes)"

    with open(env_file, "w") as f:
        f.write("====================================================\n")
        f.write(" CCO BENCHMARK ENVIRONMENT & SYSTEM METADATA\n")
        f.write("====================================================\n")
        f.write(f"Timestamp:           {datetime.now().isoformat()}\n")
        f.write(f"Architecture:        {platform.machine()}\n")
        f.write(f"Processor:           {cpu_model}\n")
        f.write(f"OS:                  {platform.system()} {platform.release()}\n")
        f.write(f"C Compiler:          {gcc_ver}\n")
        f.write(f"Cco Git Revision:    {git_rev}\n")
        f.write(f"Working Tree:        {git_status}\n")
        f.write(f"Cco Profile Format:  CCO_PROFILE_V1\n")
        f.write(f"Default Runs:        N = 10\n")
        f.write("====================================================\n")

    return env_file

def check_reproducibility(benchmarks):
    print("\n" + "=" * 105)
    print(" CCO COMPILER DETERMINISTIC REPRODUCIBILITY VERIFICATION")
    print("=" * 105)
    os.makedirs(f"{REPRO_DIR}/c1", exist_ok=True)
    os.makedirs(f"{REPRO_DIR}/c2", exist_ok=True)

    all_matched = True
    for name in benchmarks:
        src = f"bench/{name}.cco"
        if not os.path.exists(src):
            continue

        # 1. Compare IR Output
        cmd_ir1 = f"./cco {src} --emit-ir > {REPRO_DIR}/c1/{name}.ir"
        cmd_ir2 = f"./cco {src} --emit-ir > {REPRO_DIR}/c2/{name}.ir"
        run_cmd(cmd_ir1)
        run_cmd(cmd_ir2)
        r_ir, _, _ = run_cmd(f"cmp {REPRO_DIR}/c1/{name}.ir {REPRO_DIR}/c2/{name}.ir")

        # 2. Compare Assembly Output
        cmd_asm1 = f"./cco {src} --emit-asm > {REPRO_DIR}/c1/{name}.s"
        cmd_asm2 = f"./cco {src} --emit-asm > {REPRO_DIR}/c2/{name}.s"
        run_cmd(cmd_asm1)
        run_cmd(cmd_asm2)
        r_asm, _, _ = run_cmd(f"cmp {REPRO_DIR}/c1/{name}.s {REPRO_DIR}/c2/{name}.s")

        # 3. Compare ELF Object File (.o)
        cmd_obj1 = f"./cco {src} -O2 --emit-object -o {REPRO_DIR}/c1/{name}.o"
        cmd_obj2 = f"./cco {src} -O2 --emit-object -o {REPRO_DIR}/c2/{name}.o"
        run_cmd(cmd_obj1)
        run_cmd(cmd_obj2)
        r_obj, _, _ = run_cmd(f"cmp {REPRO_DIR}/c1/{name}.o {REPRO_DIR}/c2/{name}.o")

        # 4. Compare Executable Binary
        cmd_bin1 = f"./cco {src} -O2 --use-internal-linker -o {REPRO_DIR}/c1/{name}_bin"
        cmd_bin2 = f"./cco {src} -O2 --use-internal-linker -o {REPRO_DIR}/c2/{name}_bin"
        run_cmd(cmd_bin1)
        run_cmd(cmd_bin2)
        r_bin, _, _ = run_cmd(f"cmp {REPRO_DIR}/c1/{name}_bin {REPRO_DIR}/c2/{name}_bin")

        match = (r_ir == 0 and r_asm == 0 and r_obj == 0 and r_bin == 0)
        status = "100% IDENTICAL" if match else "DIFF DETECTED"
        if not match:
            all_matched = False
        print(f"  [*] {name:<20} | IR: {'MATCH' if r_ir==0 else 'FAIL'} | ASM: {'MATCH' if r_asm==0 else 'FAIL'} | OBJ: {'MATCH' if r_obj==0 else 'FAIL'} | BIN: {'MATCH' if r_bin==0 else 'FAIL'} | Status: {status}")

    print("-" * 105)
    print(f"Overall Reproducibility: {'PASSED (Deterministic byte-for-byte identity confirmed)' if all_matched else 'FAILED'}")
    return all_matched

def check_regressions(results, runtime_tol=0.15, size_tol=0.10):
    print("\n" + "=" * 105)
    print(" CCO COMPILER AUTOMATED OPTIMIZATION REGRESSION DETECTION")
    print(f" Tolerances: Runtime Slowdown > {runtime_tol*100:.0f}%, Binary Bloat > {size_tol*100:.0f}%")
    print("=" * 105)
    print(f"{'Benchmark':<22} | {'-O0 Median':<12} | {'-O2 Median':<12} | {'Runtime Diff':<14} | {'Size Diff':<12} | {'Status':<10}")
    print("-" * 105)

    all_ok = True
    for r in results:
        o0_time = r['o0_stats']['median']
        o2_time = r['o2_stats']['median']
        o0_size = r['o0_sizes']['total']
        o2_size = r['o2_sizes']['total']

        time_ratio = (o2_time - o0_time) / o0_time if o0_time > 0 else 0.0
        size_ratio = (o2_size - o0_size) / o0_size if o0_size > 0 else 0.0

        is_regressed = (time_ratio > runtime_tol or size_ratio > size_tol)
        if is_regressed:
            all_ok = False
            status = "REGRESSION"
        elif time_ratio < -0.05:
            status = "IMPROVED"
        else:
            status = "NEUTRAL"

        print(f"{r['name']:<22} | {o0_time:>10.2f}ms | {o2_time:>10.2f}ms | {time_ratio*100:>+12.1f}% | {size_ratio*100:>+10.1f}% | {status:<10}")

    print("-" * 105)
    print(f"Regression Check: {'PASSED (All workloads within configured tolerances)' if all_ok else 'FAILED (Regressions detected)'}")
    return all_ok

def main():
    parser = argparse.ArgumentParser(description="Cco Compiler Phase 9 Scientific Benchmark Harness")
    parser.add_argument("--runs", type=int, default=10, help="Number of independent executions per configuration (default: 10)")
    parser.add_argument("--workloads", type=str, default="", help="Comma-separated workload names (default: all)")
    parser.add_argument("--results-dir", type=str, default="bench/results", help="Directory for CSV and environment logs")
    parser.add_argument("--check-reproducibility", action="store_true", help="Run byte-for-byte reproducibility tests")
    parser.add_argument("--check-regression", action="store_true", help="Run automated regression check")
    parser.add_argument("--runtime-tolerance", type=float, default=0.15, help="Runtime regression tolerance fraction (default: 0.15)")
    parser.add_argument("--size-tolerance", type=float, default=0.10, help="Binary size bloat tolerance fraction (default: 0.10)")
    args = parser.parse_args()

    workloads = [w.strip() for w in args.workloads.split(",") if w.strip()] if args.workloads else BENCHMARKS

    print("=" * 105)
    print(" CCO COMPILER - PHASE 9 RESEARCH-GRADE BENCHMARKING & EVALUATION SUITE")
    print(f" Configured Workloads: {len(workloads)} | Runs per Config: N = {args.runs} | Output Dir: {args.results_dir}")
    print("=" * 105)

    os.makedirs(BUILD_DIR, exist_ok=True)
    os.makedirs(args.results_dir, exist_ok=True)
    capture_environment(args.results_dir)

    results = []
    raw_runs = []

    for name in workloads:
        src = f"bench/{name}.cco"
        if not os.path.exists(src):
            print(f"[!] Warning: {src} not found, skipping.")
            continue

        print(f"\n[*] Evaluating {name} ({src})...")

        # 1. Reference C11 (GCC -O3)
        ref_bin = f"{BUILD_DIR}/{name}_ref"
        ret, _, err, ref_comp_time = measure_compile(f"./cco {src} -o {ref_bin}")
        if ret != 0:
            print(f"  [!] Failed to compile Reference C11: {err}")
            continue
        ref_stats, ref_out = time_binary(ref_bin, runs=args.runs)
        ref_sizes = get_elf_sizes(ref_bin)

        # 2. Native -O0 (Internal Linker)
        o0_bin = f"{BUILD_DIR}/{name}_o0"
        ret, _, err, o0_comp_time = measure_compile(f"./cco {src} --use-internal-linker -o {o0_bin}")
        if ret != 0:
            print(f"  [!] Failed to compile Native -O0: {err}")
            continue
        o0_stats, o0_out = time_binary(o0_bin, runs=args.runs)
        o0_sizes = get_elf_sizes(o0_bin)
        o0_code_stats = parse_code_stats(src, "")
        o0_pass_timings = parse_pass_timings(src, "--use-internal-linker")

        # 3. Native -O1 (Internal Linker + SSA -O1)
        o1_bin = f"{BUILD_DIR}/{name}_o1"
        ret, _, err, o1_comp_time = measure_compile(f"./cco {src} --use-internal-linker --ssa -O1 -o {o1_bin}")
        if ret != 0:
            print(f"  [!] Failed to compile Native -O1: {err}")
            continue
        o1_stats, o1_out = time_binary(o1_bin, runs=args.runs)
        o1_sizes = get_elf_sizes(o1_bin)
        o1_code_stats = parse_code_stats(src, "--ssa -O1")
        o1_pass_timings = parse_pass_timings(src, "--use-internal-linker --ssa -O1")

        # 4. Native -O2 (Internal Linker + SSA -O2 + Inlining + DFE)
        o2_bin = f"{BUILD_DIR}/{name}_o2"
        ret, _, err, o2_comp_time = measure_compile(f"./cco {src} --use-internal-linker -O2 -o {o2_bin}")
        if ret != 0:
            print(f"  [!] Failed to compile Native -O2: {err}")
            continue
        o2_stats, o2_out = time_binary(o2_bin, runs=args.runs)
        o2_sizes = get_elf_sizes(o2_bin)
        o2_code_stats = parse_code_stats(src, "-O2")
        o2_pass_timings = parse_pass_timings(src, "--use-internal-linker -O2")

        # 5. Native Profile-Guided Optimization (PGO)
        pgo_prof_file = f"{BUILD_DIR}/{name}.profile"
        gen_bin = f"{BUILD_DIR}/{name}_profgen"
        ret, _, err, _ = measure_compile(f"./cco {src} --profile-generate --profile-file {pgo_prof_file} -o {gen_bin}")
        if ret != 0:
            print(f"  [!] Failed to compile profile instrumented binary: {err}")
            continue
        subprocess.run([gen_bin], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        pgo_bin = f"{BUILD_DIR}/{name}_pgo"
        ret, _, err, pgo_comp_time = measure_compile(f"./cco {src} --use-internal-linker --profile-use {pgo_prof_file} -O2 -o {pgo_bin}")
        if ret != 0:
            print(f"  [!] Failed to compile PGO binary: {err}")
            continue
        pgo_stats, pgo_out = time_binary(pgo_bin, runs=args.runs)
        pgo_sizes = get_elf_sizes(pgo_bin)
        pgo_code_stats = parse_code_stats(src, f"--profile-use {pgo_prof_file} -O2")
        pgo_pass_timings = parse_pass_timings(src, f"--use-internal-linker --profile-use {pgo_prof_file} -O2")

        # Parity Verification
        match = (ref_out == o0_out == o1_out == o2_out == pgo_out)
        status = "PASS" if match else "MISMATCH"

        # Record raw individual runs for scientific reproducibility
        configs = [
            ("Ref_C11", ref_stats),
            ("Native_O0", o0_stats),
            ("Native_O1", o1_stats),
            ("Native_O2", o2_stats),
            ("Native_PGO", pgo_stats),
        ]
        for cfg_name, cfg_s in configs:
            for idx, r_val in enumerate(cfg_s["runs"]):
                raw_runs.append({
                    "benchmark": name,
                    "configuration": cfg_name,
                    "run_index": idx + 1,
                    "elapsed_ms": f"{r_val:.4f}",
                    "output_match": "YES" if match else "NO"
                })

        entry = {
            "name": name,
            "match": match,
            "ref_stats": ref_stats,
            "ref_comp_time": ref_comp_time,
            "ref_sizes": ref_sizes,
            "o0_stats": o0_stats,
            "o0_comp_time": o0_comp_time,
            "o0_sizes": o0_sizes,
            "o0_code_stats": o0_code_stats,
            "o0_pass_timings": o0_pass_timings,
            "o1_stats": o1_stats,
            "o1_comp_time": o1_comp_time,
            "o1_sizes": o1_sizes,
            "o1_code_stats": o1_code_stats,
            "o1_pass_timings": o1_pass_timings,
            "o2_stats": o2_stats,
            "o2_comp_time": o2_comp_time,
            "o2_sizes": o2_sizes,
            "o2_code_stats": o2_code_stats,
            "o2_pass_timings": o2_pass_timings,
            "pgo_stats": pgo_stats,
            "pgo_comp_time": pgo_comp_time,
            "pgo_sizes": pgo_sizes,
            "pgo_code_stats": pgo_code_stats,
            "pgo_pass_timings": pgo_pass_timings,
        }
        results.append(entry)
        print(f"  [+] Status: {status} | Ref: {ref_stats['median']:.2f}ms | -O0: {o0_stats['median']:.2f}ms | -O2: {o2_stats['median']:.2f}ms | PGO: {pgo_stats['median']:.2f}ms")

    # Table 1: Execution Runtime
    print("\n" + "=" * 105)
    print(f" TABLE 1: EXECUTION RUNTIME (ms, median [min - max] ± stddev over N={args.runs} runs)")
    print("=" * 105)
    print(f"{'Benchmark':<20} | {'Ref C11 (ms)':<14} | {'Native -O0 (ms)':<15} | {'Native -O2 (ms)':<15} | {'Native PGO (ms)':<15} | {'Speedup':<8} | {'Parity':<6}")
    print("-" * 105)
    for r in results:
        ref_s = f"{r['ref_stats']['median']:.1f} [{r['ref_stats']['min']:.1f}-{r['ref_stats']['max']:.1f}]"
        o0_s = f"{r['o0_stats']['median']:.1f} [{r['o0_stats']['min']:.1f}-{r['o0_stats']['max']:.1f}]"
        o2_s = f"{r['o2_stats']['median']:.1f} [{r['o2_stats']['min']:.1f}-{r['o2_stats']['max']:.1f}]"
        pgo_s = f"{r['pgo_stats']['median']:.1f} [{r['pgo_stats']['min']:.1f}-{r['pgo_stats']['max']:.1f}]"
        speedup = (r['o0_stats']['median'] / r['pgo_stats']['median']) if r['pgo_stats']['median'] > 0 else 0.0
        parity = "PASS" if r['match'] else "FAIL"
        print(f"{r['name']:<20} | {ref_s:<14} | {o0_s:<15} | {o2_s:<15} | {pgo_s:<15} | {speedup:>7.2f}x | {parity:<6}")
    print("-" * 105)

    # Table 2: Binary Sizes
    print("\n" + "=" * 105)
    print(" TABLE 2: EXECUTABLE BINARY SIZE & SECTION DECOMPOSITION (bytes)")
    print("=" * 105)
    print(f"{'Benchmark':<20} | {'Ref Total':<10} | {'-O0 Total':<10} | {'-O2 Total':<10} | {'-O2 .text':<10} | {'-O2 .rodata':<11} | {'PGO Total':<10} | {'PGO .text':<10}")
    print("-" * 105)
    for r in results:
        print(f"{r['name']:<20} | {r['ref_sizes']['total']:>10} | {r['o0_sizes']['total']:>10} | {r['o2_sizes']['total']:>10} | {r['o2_sizes']['text']:>10} | {r['o2_sizes']['rodata']:>11} | {r['pgo_sizes']['total']:>10} | {r['pgo_sizes']['text']:>10}")
    print("-" * 105)

    # Table 3: Code Metrics & Optimizer Impact
    print("\n" + "=" * 105)
    print(" TABLE 3: INSTRUCTION METRICS & REGISTER PRESSURE (--dump-code-stats)")
    print("=" * 105)
    print(f"{'Benchmark':<20} | {'-O0 Inst':<9} | {'-O2 Inst':<9} | {'x86 Inst':<9} | {'Int Ops':<8} | {'Float Ops':<9} | {'Spills':<7} | {'Peak Live':<10} | {'Reg Util':<9}")
    print("-" * 105)
    for r in results:
        cs = r['o2_code_stats']
        o0_inst = r['o0_code_stats'].get('Post-SSA IR Instructions', '?')
        o2_inst = cs.get('Post-SSA IR Instructions', '?')
        x86_inst = cs.get('x86 Machine Instructions', '?')
        int_ops = cs.get('Integer Operations', '0')
        flt_ops = cs.get('Floating-Point Operations', '0')
        spills = cs.get('Spilled Registers', '0')
        peak_int = cs.get('Peak Integer Live', '0')
        peak_flt = cs.get('Peak Float Live', '0')
        peak_live = f"{peak_int}i / {peak_flt}f"
        reg_util = cs.get('Register Utilization', '0.0%')
        print(f"{r['name']:<20} | {o0_inst:>9} | {o2_inst:>9} | {x86_inst:>9} | {int_ops:>8} | {flt_ops:>9} | {spills:>7} | {peak_live:>10} | {reg_util:>9}")
    print("-" * 105)

    # Table 4: Compilation Time & Pass Breakdown
    print("\n" + "=" * 105)
    print(" TABLE 4: COMPILATION PASS TIMINGS BREAKDOWN (--time-passes, ms)")
    print("=" * 105)
    print(f"{'Benchmark':<20} | {'Frontend':<9} | {'IR Lower':<9} | {'SSA Opt':<9} | {'IPA':<8} | {'RegAlloc':<9} | {'Codegen':<9} | {'Linking':<9} | {'Total Compile':<13}")
    print("-" * 105)
    for r in results:
        pt = r['o2_pass_timings']
        fe = pt.get('Frontend', 0.0)
        ir = pt.get('IR lowering', 0.0)
        ssa = pt.get('SSA', 0.0)
        ipa = pt.get('IPA', 0.0)
        ra = pt.get('RegAlloc', 0.0)
        cg = pt.get('Codegen', 0.0)
        lk = pt.get('Linking', 0.0)
        tot = pt.get('Total', r['o2_comp_time'])
        print(f"{r['name']:<20} | {fe:>7.2f}ms | {ir:>7.2f}ms | {ssa:>7.2f}ms | {ipa:>6.2f}ms | {ra:>7.2f}ms | {cg:>7.2f}ms | {lk:>7.2f}ms | {tot:>11.2f}ms")
    print("-" * 105)

    # Table 5: PGO Effectiveness Analysis
    print("\n" + "=" * 105)
    print(" TABLE 5: PROFILE-GUIDED OPTIMIZATION (PGO) EFFECTIVENESS CLASSIFICATION")
    print("=" * 105)
    print(f"{'Benchmark':<20} | {'-O2 Median':<12} | {'PGO Median':<12} | {'Diff (%)':<10} | {'Hot Blocks':<11} | {'Cold Blocks':<12} | {'PGO Classification':<18}")
    print("-" * 105)
    pgo_records = []
    for r in results:
        o2_m = r['o2_stats']['median']
        pgo_m = r['pgo_stats']['median']
        diff_pct = ((o2_m - pgo_m) / o2_m * 100.0) if o2_m > 0 else 0.0
        hot_bb = r['pgo_code_stats'].get('Hot Blocks', '0')
        cold_bb = r['pgo_code_stats'].get('Cold Blocks', '0')
        if diff_pct > 3.0:
            classification = "improved"
        elif diff_pct < -3.0:
            classification = "regressed"
        else:
            classification = "approximately unchanged"
        print(f"{r['name']:<20} | {o2_m:>10.2f}ms | {pgo_m:>10.2f}ms | {diff_pct:>+8.1f}% | {hot_bb:>11} | {cold_bb:>12} | {classification:<18}")
        pgo_records.append({
            "benchmark": r['name'],
            "o2_median_ms": f"{o2_m:.2f}",
            "pgo_median_ms": f"{pgo_m:.2f}",
            "speedup_pct": f"{diff_pct:.2f}",
            "hot_blocks": hot_bb,
            "cold_blocks": cold_bb,
            "classification": classification
        })
    print("-" * 105)

    # Save Machine-Readable CSVs
    raw_csv = os.path.join(args.results_dir, "results_raw.csv")
    with open(raw_csv, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["benchmark", "configuration", "run_index", "elapsed_ms", "output_match"])
        writer.writeheader()
        writer.writerows(raw_runs)

    summary_csv = os.path.join(args.results_dir, "results_summary.csv")
    with open(summary_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["benchmark", "configuration", "median_ms", "min_ms", "max_ms", "mean_ms", "stddev_ms", "output_parity"])
        for r in results:
            for cfg_label, s_data in [("Ref_C11", r['ref_stats']), ("Native_O0", r['o0_stats']), ("Native_O1", r['o1_stats']), ("Native_O2", r['o2_stats']), ("Native_PGO", r['pgo_stats'])]:
                writer.writerow([r['name'], cfg_label, f"{s_data['median']:.3f}", f"{s_data['min']:.3f}", f"{s_data['max']:.3f}", f"{s_data['mean']:.3f}", f"{s_data['stddev']:.3f}", "PASS" if r['match'] else "FAIL"])

    sizes_csv = os.path.join(args.results_dir, "results_sizes.csv")
    with open(sizes_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["benchmark", "config", "total_bytes", "text_bytes", "rodata_bytes", "data_bytes"])
        for r in results:
            writer.writerow([r['name'], "Ref_C11", r['ref_sizes']['total'], r['ref_sizes']['text'], r['ref_sizes']['rodata'], r['ref_sizes']['data']])
            writer.writerow([r['name'], "Native_O0", r['o0_sizes']['total'], r['o0_sizes']['text'], r['o0_sizes']['rodata'], r['o0_sizes']['data']])
            writer.writerow([r['name'], "Native_O1", r['o1_sizes']['total'], r['o1_sizes']['text'], r['o1_sizes']['rodata'], r['o1_sizes']['data']])
            writer.writerow([r['name'], "Native_O2", r['o2_sizes']['total'], r['o2_sizes']['text'], r['o2_sizes']['rodata'], r['o2_sizes']['data']])
            writer.writerow([r['name'], "Native_PGO", r['pgo_sizes']['total'], r['pgo_sizes']['text'], r['pgo_sizes']['rodata'], r['pgo_sizes']['data']])

    codestats_csv = os.path.join(args.results_dir, "results_codestats.csv")
    with open(codestats_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["benchmark", "ir_inst", "x86_inst", "int_ops", "float_ops", "branches", "loads", "stores", "spills", "reg_util"])
        for r in results:
            cs = r['o2_code_stats']
            writer.writerow([
                r['name'],
                cs.get('Post-SSA IR Instructions', '0'),
                cs.get('x86 Machine Instructions', '0'),
                cs.get('Integer Operations', '0'),
                cs.get('Floating-Point Operations', '0'),
                cs.get('Branches', '0'),
                cs.get('Memory Loads', '0'),
                cs.get('Memory Stores', '0'),
                cs.get('Spilled Registers', '0'),
                cs.get('Register Utilization', '0.0%'),
            ])

    pgo_csv = os.path.join(args.results_dir, "results_pgo_analysis.csv")
    with open(pgo_csv, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["benchmark", "o2_median_ms", "pgo_median_ms", "speedup_pct", "hot_blocks", "cold_blocks", "classification"])
        writer.writeheader()
        writer.writerows(pgo_records)

    comp_time_csv = os.path.join(args.results_dir, "results_compilation_time.csv")
    with open(comp_time_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["benchmark", "config", "frontend_ms", "ir_lower_ms", "ssa_ms", "opt_ms", "ipa_ms", "pgo_ms", "regalloc_ms", "codegen_ms", "elf_gen_ms", "link_ms", "total_compile_ms"])
        for r in results:
            for cfg_label, pt in [("Native_O0", r['o0_pass_timings']), ("Native_O1", r['o1_pass_timings']), ("Native_O2", r['o2_pass_timings']), ("Native_PGO", r['pgo_pass_timings'])]:
                writer.writerow([
                    r['name'],
                    cfg_label,
                    f"{pt.get('Frontend', 0.0):.3f}",
                    f"{pt.get('IR lowering', 0.0):.3f}",
                    f"{pt.get('SSA', 0.0):.3f}",
                    f"{pt.get('Optimization', 0.0):.3f}",
                    f"{pt.get('IPA', 0.0):.3f}",
                    f"{pt.get('PGO processing', 0.0):.3f}",
                    f"{pt.get('RegAlloc', 0.0):.3f}",
                    f"{pt.get('Codegen', 0.0):.3f}",
                    f"{pt.get('ELF generation', 0.0):.3f}",
                    f"{pt.get('Linking', 0.0):.3f}",
                    f"{pt.get('Total', 0.0):.3f}",
                ])

    print(f"\n[+] Machine-readable CSV results successfully written to '{args.results_dir}/'.")

    # Optional checks
    if args.check_reproducibility:
        repro_ok = check_reproducibility(workloads)
        if not repro_ok:
            sys.exit(1)

    if args.check_regression:
        reg_ok = check_regressions(results, runtime_tol=args.runtime_tolerance, size_tol=args.size_tolerance)
        if not reg_ok:
            sys.exit(1)

if __name__ == "__main__":
    main()
