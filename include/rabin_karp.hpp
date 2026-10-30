// rabin_karp.hpp - karp-rabin string matching

#ifndef RABIN_KARP_HPP_
#define RABIN_KARP_HPP_

#include <cstddef>
#include <string>
#include <vector>

// counts from a search. hashHits is every window whose hash equalled the
// pattern's hash, and falsePositives is the hits where the text did not
// actually match.
struct RKStats {
  std::size_t hashHits {0};
  std::size_t falsePositives {0};
  std::size_t comparisons{ 0 };
};

// 2^31 - 1, a prime. the modulus used by rabin_karp_search.
const unsigned long long kRKModulus {2147483647};

// start position of every match, in increasing order, overlaps included.
// an empty pattern, or one longer than the text, gives no matches.
// a new random base is picked for every call.
std::vector<std::size_t> rabin_karp_search(const std::string& text,
                                           const std::string& pattern);

// the same search with a chosen base and modulus. the counts are added to
// stats. modulus must be at least 2 and below 2^32 so the products fit in
// 64 bits.
std::vector<std::size_t> rabin_karp_search_counted(
    const std::string& text, const std::string& pattern,
    unsigned long long base, unsigned long long modulus, RKStats& stats);

#endif  // RABIN_KARP_HPP_
