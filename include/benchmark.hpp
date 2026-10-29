// benchmark.hpp - the list of algorithms under comparison
//
// the tests and the benchmark both iterate over this list, so anything that
// is benchmarked has also been checked for correctness.

#pragma once

#include <string>
#include <vector>

#include "boyer_moore.hpp"
#include "kmp.hpp"
#include "rabin_karp.hpp"
#include "test_utils.hpp"

struct Algorithm {
    std::string name;
    SearchFunction search;
};

// base used by the fixed-base karp-rabin ablation. 31 is the textbook choice.
constexpr unsigned long long kRKFixedBase{ 31 };

inline const std::vector<Algorithm>& allAlgorithms() {
    static const std::vector<Algorithm> algorithms{
        { "kmp", kmp_search },
        { "boyer_moore", boyer_moore_search },
        // random base: the randomised algorithm
        { "karp_rabin", rabin_karp_search },
        // reference point and independent correctness oracle
        { "std_find", stdFindSearch },
    };
    return algorithms;
}
