// benchmark_main.cpp - runs the whole benchmark
//
//
// writes runtime.csv, comparisons.csv, rk_tail.csv and environment.txt to
// results/ (created if missing) in the current folder.

#include <filesystem>
#include <iostream>

#include "benchmark_runner.hpp"

int main() {
    try {
        const BenchConfig cfg{};
        std::filesystem::create_directories(cfg.outDir);
        return runBenchmarks(cfg);
    }
    catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}