// kmp.hpp - knuth-morris-pratt string matching

#ifndef KMP_HPP_
#define KMP_HPP_

#include <cstddef>
#include <string>
#include <vector>

// counts from a search, used to test explanations of the timing results.
// comparisons is every text character compared with a pattern character.
// fallbacks is every time the pattern position jumped back using the lps
// table (the "reuse what we already matched" step).
struct KMPStats {
    std::size_t comparisons{ 0 };
    std::size_t fallbacks{ 0 };
};

// lps[i] is the length of the longest proper prefix of pattern[0..i] that
// is also a suffix of it. e.g. "ABABC" gives 0 0 1 2 0.
std::vector<std::size_t> buildLPS(const std::string& pattern);

// start position of every match, in increasing order, overlaps included.
// an empty pattern, or one longer than the text, gives no matches.
std::vector<std::size_t> kmp_search(const std::string& text,
    const std::string& pattern);

// the same search, but the counts are added to stats. kept separate so the
// counting does not slow down the version that is timed.
std::vector<std::size_t> kmp_search_counted(const std::string& text,
    const std::string& pattern,
    KMPStats& stats);

#endif  // KMP_HPP_
