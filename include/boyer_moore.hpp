// boyer_moore.hpp - boyer-moore string matching
//
// primary author: rebecca kim

#ifndef BOYER_MOORE_HPP_
#define BOYER_MOORE_HPP_

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// start position of every match, in increasing order, overlaps included.
// an empty pattern, or one longer than the text, gives no matches.
std::vector<std::size_t> boyer_moore_search(const std::string& text,
                                            const std::string& pattern);

// counts from a search, used to test explanations of the timing results.
// comparisons is every text character compared with a pattern character.
// alignments is every position the pattern was placed at (so n - m + 1 for
// the naive method, and fewer when the shifts are bigger than 1).
struct BMStats {
  std::size_t comparisons {0};
  std::size_t alignments {0};
};

// the same search, but the counts are added to stats. kept separate so the
// counting does not slow down the version that is timed.
std::vector<std::size_t> boyer_moore_search_counted(const std::string& text,
                                                    const std::string& pattern,
                                                    BMStats& stats);

// last position of each character in pattern, or -1 if it does not appear
std::array<int, 256> buildBadCharTable(const std::string& pattern);

// goodSuffix[j] is the shift when pattern[j..m-1] matched and pattern[j-1]
// did not. goodSuffix[0] is the shift after a full match.
std::vector<int> buildGoodSuffixTable(const std::string& pattern);

#endif  // BOYER_MOORE_HPP_
