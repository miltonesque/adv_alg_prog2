// benchmark_runner.hpp - experiment driver

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct BenchConfig {
    // timed repetitions per (instance, algorithm); odd so the median is a sample
    std::size_t reps{ 11 };
    // master seed: input instances, algorithm order and karp-rabin bases all
    // derive from it
    std::uint64_t seed{ 20261029 };
    std::string outDir{ "results" };
    // smaller sizes and fewer repetitions, for a fast end-to-end check
    bool quick{ false };
    // run only these experiments (empty = all)
    std::vector<std::string> only{};
    // each timed sample repeats the call until at least this long has passed,
    // so short calls are not dominated by clock resolution
    double minSampleMs{ 5.0 };
    // independent runs per scenario in the rk_tail experiment
    std::size_t tailTrials{ 1000 };
};

// the experiment names accepted by --only
const std::vector<std::string>& experimentNames();

// runs the selected experiments and writes the CSV files to cfg.outDir.
// returns 0 if every algorithm's output was verified on every instance.
int runBenchmarks(const BenchConfig& cfg);
