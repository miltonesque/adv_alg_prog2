#!/usr/bin/env python3
"""reproduce.py - rebuilds everything and regenerates every number in the report.

One command, no options. Works on Windows, macOS and Linux. Run from anywhere:

    python3 scripts/reproduce.py            (Windows: py scripts\\reproduce.py)

Steps:
  1. configure and build with CMake in Release mode     -> build/
  2. run all four test programs with ctest (stops if any test fails)
  3. run the benchmark                                  -> results/runtime.csv,
                                                           comparisons.csv,
                                                           rk_tail.csv,
                                                           environment.txt
  4. run the explanation tests                          -> results/explain_*.csv
  5. count simulated branch mispredictions with valgrind,
     if it is installed (Linux/macOS only)              -> results/explain_branch_sim.csv
  6. draw the charts and summary tables                 -> results/plots/,
     (needs the python packages pandas and matplotlib)     results/summary/

Steps 5 and 6 are skipped, with a message, if their tool is not installed;
everything else still runs.
"""
import csv
import importlib.util
import re
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


def pinned(cmd):
    # pin to one core when possible, so the operating system does not move
    # the process between cores mid-measurement (Linux only)
    if shutil.which("taskset"):
        return ["taskset", "-c", "0"] + cmd
    return cmd


# ---- step 5: simulated branch mispredictions ----
# valgrind's callgrind tool runs one search on a simulated processor and
# counts how often a simple branch predictor guesses wrong. only the search
# function is measured (--toggle-collect), not input generation. the inputs
# are the same ones the explanation tests time (T1 and T2), so the counts and
# the times can be compared directly. the simulator is a model, not a real
# processor: the counts show how predictable each algorithm's branches are.

# algorithm -> the function in explanation_tests.cpp that callgrind measures
MEASURED_FUNCTION = {
    "kmp": "*kmpCall*",
    "kmp_dfa": "*kmpDfaSearch*",
    "boyer_moore": "*bmCall*",
    "bm_next_position_known": "*bmDecoupledSearch*",
    "karp_rabin": "*rkCall*",
}

BRANCH_CASES = (
    # T1's instances: random text, m = 32, n = 2^22, every alphabet size
    [(alg, "random", sigma, 32, 1 << 22)
     for sigma in (2, 4, 8, 16, 32, 64, 128, 256)
     for alg in ("kmp", "kmp_dfa", "karp_rabin")]
    # T2's instances: text a^n, pattern a^(m-1) b, n = 2^18
    + [(alg, "mismatch_last", 2, m, 1 << 18)
       for m in (16, 256)
       for alg in ("boyer_moore", "bm_next_position_known", "kmp")]
)


def count_number(label, text):
    match = re.search(label + r":\s*([0-9,]+)", text)
    return int(match.group(1).replace(",", "")) if match else ""


def branch_simulation():
    explain = find_program("explain_app")
    out_path = RESULTS / "explain_branch_sim.csv"
    print(f"\n$ valgrind --tool=callgrind --branch-sim=yes ... "
          f"({len(BRANCH_CASES)} runs) -> {out_path}", flush=True)
    with open(out_path, "w", newline="") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(["algorithm", "family", "sigma", "m", "n",
                         "branches", "mispredicts", "kmp_fallbacks"])
        for alg, family, sigma, m, n in BRANCH_CASES:
            result = subprocess.run(
                ["valgrind", "--tool=callgrind", "--branch-sim=yes",
                 "--toggle-collect=" + MEASURED_FUNCTION[alg],
                 "--callgrind-out-file=" + ("NUL" if sys.platform == "win32"
                                            else "/dev/null"),
                 str(explain), "--one", alg, family, str(sigma), str(m), str(n)],
                capture_output=True, text=True, check=True)
            log = result.stdout + result.stderr
            mispredicts = count_number("Mispredicts", log)
            fallbacks = re.search(r"kmp_fallbacks=([0-9]+)", log)
            writer.writerow([alg, family, sigma, m, n,
                             count_number("Branches", log), mispredicts,
                             fallbacks.group(1) if fallbacks else ""])
            print(f"  {alg:24s} {family:14s} sigma={sigma:<4d} m={m:<5d} "
                  f"mispredicts/char={mispredicts / n:.3f}", flush=True)


def main():
    run(["cmake", "-S", ROOT, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", BUILD, "--config", "Release", "--parallel"])
    run(["ctest", "-C", "Release", "--output-on-failure"], cwd=BUILD)

    RESULTS.mkdir(exist_ok=True)
    for old in list(RESULTS.glob("*.csv")):
        old.unlink()
    run(pinned([find_program("benchmark_app")]), cwd=ROOT)
    run(pinned([find_program("explain_app")]), cwd=ROOT)

    if shutil.which("valgrind"):
        branch_simulation()
    else:
        print("\nvalgrind not found: skipping the simulated branch counts "
              "(explain_branch_sim.csv); everything else is unaffected")

    if all(importlib.util.find_spec(m) for m in ("pandas", "matplotlib")):
        run([sys.executable, ROOT / "scripts" / "plot_results.py", RESULTS])
    else:
        print("\npandas/matplotlib not installed: skipping the charts. Install "
              "them with: pip install pandas matplotlib, then rerun.")

    print("\ndone - see results/ (CSVs), results/plots/ and results/summary/")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(f"\nstopped: a step failed (exit code {error.returncode})")
