// bmp.hpp - boyer-moore string matching

#ifndef BMP_HPP_
#define BMP_HPP_

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// start position of every match, in increasing order, overlaps included.
// an empty pattern, or one longer than the text, gives no matches.
std::vector<std::size_t> boyer_moore_search(const std::string& text,
                                            const std::string& pattern);

// last position of each character in pattern, or -1 if it does not appear
std::array<int, 256> buildBadCharTable(const std::string& pattern);

// goodSuffix[j] is the shift when pattern[j..m-1] matched and pattern[j-1]
// did not. goodSuffix[0] is the shift after a full match.
std::vector<int> buildGoodSuffixTable(const std::string& pattern);

#endif  // BMP_HPP_
