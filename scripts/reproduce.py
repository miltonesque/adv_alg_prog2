#!/usr/bin/env python3
"""reproduce.py - rebuilds everything and regenerates every number in the report.

Works the same on Windows, macOS and Linux. Run from anywhere:

    python3 scripts/reproduce.py            (Windows: py scripts\\reproduce.py)

Steps:
  1. configure and build with CMake in Release mode (build/)
  2. run all four test programs with ctest - stops if any test fails
  3. run the benchmark from the repository root - writes results/*.csv
"""
import os
import platform
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"


def run(cmd, **kwargs):
    print("\n$ " + " ".join(str(c) for c in cmd), flush=True)
    subprocess.run(cmd, check=True, **kwargs)


def find_benchmark():
    # single-config generators (Makefiles, Ninja) put it in build/,
    # Visual Studio puts it in build/Release/
    for candidate in ["benchmark", "benchmark.exe",
                      "Release/benchmark.exe", "Release/benchmark"]:
        path = BUILD / candidate
        if path.is_file():
            return path
    sys.exit("could not find the benchmark program in build/")


def cmake_cache(key):
    cache = BUILD / "CMakeCache.txt"
    if cache.exists():
        m = re.search(rf"^{key}:[A-Z]+=(.*)$", cache.read_text(errors="ignore"), re.M)
        if m:
            return m.group(1)
    return "unknown"

def main():

    run(["cmake", "-S", ROOT, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", BUILD, "--config", "Release", "--parallel"])
    run(["ctest", "-C", "Release", "--output-on-failure"], cwd=BUILD)
    # the benchmark writes to results/ relative to where it runs
    run([find_benchmark()], cwd=ROOT)
    print("\ndone - results/ now holds the CSVs")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(f"\nstopped: a step failed (exit code {error.returncode})")
