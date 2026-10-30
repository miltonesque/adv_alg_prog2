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

// base used by the fixed-base karp-rabin ablation.
constexpr unsigned long long kRKFixedBase{ 31 };

inline const std::vector<Algorithm>& allAlgorithms() {
    static const std::vector<Algorithm> algorithms{
        { "kmp", kmp_search },
        { "boyer_moore", boyer_moore_search },
        // random base: the randomised algorithm, through its public function
        { "karp_rabin", rabin_karp_search },
        // the same code with the base fixed at 31, called through the
        // existing rabin_karp_search_counted: shows what the random base buys
        { "karp_rabin_fixed31",
          [](const std::string& t, const std::string& p) {
              RKStats unused{};
              return rabin_karp_search_counted(t, p, kRKFixedBase, kRKModulus, unused);
          } },
        // reference point and independent correctness
        { "std_find", stdFindSearch },
    };
    return algorithms;
}
