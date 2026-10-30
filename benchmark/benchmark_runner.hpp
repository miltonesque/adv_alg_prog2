// benchmark_runner.hpp - experiment driver
//
// the whole experiment is fixed in code (sizes, seeds, repetitions); there
// are no command-line options. scripts/reproduce.py runs it.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct BenchConfig {
    // timed repetitions per (instance, algorithm); odd so the median is a sample.
    // raised from 11 to 21 after a run on the shared cloud VM showed a
    // coefficient of variation of up to 27% for KMP at large alphabets
    std::size_t reps{ 21 };
    // master seed: input instances, algorithm order and karp-rabin bases all
    // derive from it
    std::uint64_t seed{ 20261029 };
    std::string outDir{ "results" };
    // each timed sample repeats the call until at least this long has passed,
    // so short calls are not dominated by clock resolution
    double minSampleMs{ 5.0 };
    // independent runs per scenario in the rk_tail experiment
    std::size_t tailTrials{ 1000 };
};

// runs every experiment and writes the CSV files to cfg.outDir.
// returns 0 if every algorithm's output was verified on every instance.
int runBenchmarks(const BenchConfig& cfg);
