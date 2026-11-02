#!/usr/bin/env python3
"""reproduce.py - rebuilds everything and regenerates every number in the report.

    python3 scripts/reproduce.py            (Windows: py scripts\\reproduce.py)

Steps:
  1. configure and build with CMake in Release mode     -> build/
  2. run all four test programs with ctest (stops if any test fails)
  3. run the benchmark                                  -> results/runtime.csv,
                                                           comparisons.csv,
                                                           rk_tail.csv,
                                                           environment.txt
  4. run the explanation tests                          -> results/explain_*.csv
  5. draw the charts and summary tables                 -> results/plots/,
     (needs the python packages pandas and matplotlib)     results/summary/

Old results are deleted first, so everything in results/ comes from the latest run.
"""
import importlib.util
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
RESULTS = ROOT / "results"


def run(cmd, **kwargs):
    print("\n$ " + " ".join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], check=True, **kwargs)


def find_program(name):
    # single-config generators (Makefiles, Ninja) put it in build/,
    # Visual Studio puts it in build/Release/
    for candidate in [name, name + ".exe", "Release/" + name + ".exe",
                      "Release/" + name]:
        path = BUILD / candidate
        if path.is_file():
            return path
    sys.exit(f"could not find {name} in build/")


def main():
    if not all(importlib.util.find_spec(m) for m in ("pandas", "matplotlib")):
        sys.exit("pandas and matplotlib are needed for the charts: "
                 "pip install pandas matplotlib")

    run(["cmake", "-S", ROOT, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", BUILD, "--config", "Release", "--parallel"])
    run(["ctest", "-C", "Release", "--output-on-failure"], cwd=BUILD)

    # start from an empty results folder so no stale file survives
    if RESULTS.exists():
        shutil.rmtree(RESULTS)
    RESULTS.mkdir()

    run([find_program("benchmark_app")], cwd=ROOT)
    run([find_program("explain_app")], cwd=ROOT)
    run([sys.executable, ROOT / "scripts" / "plot_results.py", RESULTS])

    print("\ndone - see results/ (CSVs), results/plots/ and results/summary/")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(f"\nstopped: a step failed (exit code {error.returncode})")
