#!/usr/bin/env bash
set -e

# Benchmark suite runner for Phase 7
cd "$(dirname "$0")/.."

python3 bench/run_benchmarks.py "$@"
