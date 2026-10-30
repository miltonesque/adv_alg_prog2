# adv_alg_prog2

41052 Advanced Algorithms, Programming Assignment 2. 

Theme 2, deterministic vs randomised: KMP and Boyer-Moore (deterministic) against Karp-Rabin (randomised), with `std::string::find` as a reference.

## Algorithms and primary authors

| Algorithm | Primary author |
|---|---|
| Knuth-Morris-Pratt | Aeesha Bello |
| Boyer-Moore | Rebecca Kim |
| Karp-Rabin | Mallory Li |

## What you need
 
- CMake 3.20 or newer
- A C++17 compiler (Visual Studio 2022, GCC or Clang)
- Python 3, with `pip install pandas matplotlib` for the charts
## Run everything
 
From the repository root:
 
```
python3 scripts/reproduce.py        # Windows: py scripts\reproduce.py
```
 
This builds in Release mode, runs all four test programs (and stops if any fails), runs the benchmark and the explanation tests, and draws the charts. It takes about 5 minutes.
 
## Or step by step
 
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd build
ctest -C Release --output-on-failure
cd ..
```
 
Then, from the repository root:
 
| Step | Linux / macOS | Windows |
|---|---|---|
| Benchmark | `build/benchmark_app` | `build\Release\benchmark_app.exe` |
| Explanation tests | `build/explain_app` | `build\Release\explain_app.exe` |
| Charts and tables | `python3 scripts/plot_results.py results` | `py scripts\plot_results.py results` |
 
## Output
 
Everything goes into `results/`:
 
- `runtime.csv`: every timed run
- `comparisons.csv`: operation counts (the same on any machine)
- `rk_tail.csv`: 1000 Karp-Rabin runs per input, each with a new random base
- `explain_*.csv`: the explanation tests
- `environment.txt`: CPU, compiler, flags and settings of the run
- `summary/` and `plots/`: tables and charts made from the files above
 
