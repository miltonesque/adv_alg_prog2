// bmp.cpp - boyer-moore string matching
//
// primary author: r
//
// ---- overview ----
// the pattern is placed under the text and compared from right to left.
// after a mismatch, two rules each give a safe shift to the right, and the
// larger one is used:
//   bad character rule: move the pattern so that the mismatched text
//                       character lines up with its last copy in the
//                       pattern, or move past it if the pattern does not
//                       contain it.
//   good suffix rule:   the characters that already matched must match again
//                       after the shift, so move to the next position where
//                       they fit.
// both rules only skip positions where a match is impossible, so no match
// is missed.
//
// indexes are stored as int, so the text must be shorter than 2^31 characters.

#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "bmp.hpp"

// ---- bad character table ----
// last position of each character in the pattern, or -1 if it does not
// appear. e.g. "abcab" gives a = 3, b = 4, c = 2.
std::array<int, 256> buildBadCharTable(const std::string& pattern) {
  std::array<int, 256> lastOcc {};
  lastOcc.fill(-1);
  for (int i = 0; i < static_cast<int>(pattern.size()); ++i) {
    // unsigned char keeps characters above 127 from becoming negative indexes
    lastOcc[static_cast<unsigned char>(pattern[i])] = i;
  }
  return lastOcc;
}

// ---- good suffix table ----
// shift[j] is the shift to use when pattern[j..m-1] matched and pattern[j-1]
// did not. shift[0] is the shift after a full match.
// e.g. "abcab" gives 3 3 3 3 5 1.
//
// the table is built in O(m) using borders. a border of a string is a prefix
// that is also a suffix, but not the whole string, e.g. "ab" in "abcab".
// border[i] = j means pattern[j..m-1] is the longest border of
// pattern[i..m-1].
std::vector<int> buildGoodSuffixTable(const std::string& pattern) {
  const int m {static_cast<int>(pattern.size())};
  std::vector<int> shift(m + 1, 0);
  std::vector<int> border(m + 1, 0);

  // case 1: the matched suffix appears again earlier in the pattern, with a
  // different character before it. a copy with the same character before it
  // would fail at the same place, so it is not used (the strong version of
  // the rule). each time a border cannot be extended, j - i is the shift
  // that lines up that copy.
  int i {m};
  int j {m + 1};
  border[i] = j;
  while (i > 0) {
    while (j <= m && pattern[i - 1] != pattern[j - 1]) {
      // keep the first value found, which is the smallest safe shift
      if (shift[j] == 0) {
        shift[j] = j - i;
      }
      j = border[j];
    }
    --i;
    --j;
    border[i] = j;
  }

  // case 2: the matched suffix does not appear again, so line up the longest
  // border of the pattern that fits inside it. once i passes the current
  // border, the next shorter border is used.
  // this fills every entry still set to 0. a shift of 0 would never move the
  // pattern, and a shift of m would be too large, e.g. it would miss the
  // match at 3 for "abcab" in "abcabcab".
  j = border[0];
  for (i = 0; i <= m; ++i) {
    if (shift[i] == 0) {
      shift[i] = j;
    }
    if (i == j) {
      j = border[j];
    }
  }
  return shift;
}

// ---- search ----
// same inputs and output as kmp_search.
std::vector<std::size_t> boyer_moore_search(const std::string& text,
                                            const std::string& pattern) {
  std::vector<std::size_t> matches {};
  const int n {static_cast<int>(text.size())};
  const int m {static_cast<int>(pattern.size())};
  if (m == 0 || m > n) {
    return matches;
  }

  const std::array<int, 256> lastOcc {buildBadCharTable(pattern)};
  const std::vector<int> goodSuffix {buildGoodSuffixTable(pattern)};

  // s is the position in the text where the pattern currently starts
  int s {0};
  while (s <= n - m) {
    // compare from right to left. afterwards pattern[j+1..m-1] has matched,
    // and j < 0 means the whole pattern matched
    int j {m - 1};
    while (j >= 0 && pattern[j] == text[s + j]) {
      --j;
    }

    if (j < 0) {
      // matches can overlap, so the shift is goodSuffix[0], not m
      matches.push_back(static_cast<std::size_t>(s));
      s += goodSuffix[0];
    } else {
      // the bad character shift is 0 or negative when the last copy of the
      // text character is to the right of j. the good suffix shift is
      // always at least 1, so the pattern always moves forward.
      const int badChar {j - lastOcc[static_cast<unsigned char>(text[s + j])]};
      s += std::max(badChar, goodSuffix[j + 1]);
    }
  }
  return matches;
}
