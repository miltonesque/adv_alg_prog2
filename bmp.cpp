// bmp.cpp - boyer-moore string matching
//
// primary author: r
//
// ---- overview ----
// the pattern is placed under the text and compared from right to left.
// after a mismatch, two rules each give a safe shift to the right, and the
// larger one is used:
//   bad character rule: move the pattern so that the mismatched text
//                       character is aligned with its last copy in the pattern.
//                       if the pattern does not contain it, move the pattern
//                       past it.
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
// stores the last position of each character in the pattern, or -1 if the
// character does not appear. e.g. "abcab" gives a = 3, b = 4, c = 2.
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
// border[i] = j means pattern[j..m-1] is the longest border of pattern[i..m-1].
std::vector<int> buildGoodSuffixTable(const std::string& pattern) {
  int m {static_cast<int>(pattern.size())};
  std::vector<int> shift(m + 1, 0);
  std::vector<int> border(m + 1, 0);

  // case 1: the matched suffix appears again earlier in the pattern, with a
  // different character before it. a copy with the same character before it
  // would fail at the same place, so it is not used (the strong version of
  // the rule). each time a border cannot be extended, j - i is the shift
  // that aligns that copy.
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

  // case 2: the matched suffix does not appear again, so align the longest
  // border of the pattern that fits inside it. once i passes the current
  // border, the next shorter border is used.
  // this case fills every entry still set to 0. a shift of 0 would never
  // move the pattern, and a shift of m would be too large, e.g. it would
  // miss the match at 3 for "abcab" in "abcabcab".
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

namespace {

// ---- search ----
// shared by both public functions at the end of the file.
// stats is nullptr when nothing is counted (this is the version to time).
// useGoodSuffix = false uses the bad character rule only, for experiments.
std::vector<std::size_t> search(const std::string& text,
                                const std::string& pattern,
                                BMStats* stats, bool useGoodSuffix) {
  std::vector<std::size_t> matches {};
  int n {static_cast<int>(text.size())};
  int m {static_cast<int>(pattern.size())};
  if (m == 0 || m > n) {
    return matches;
  }

  std::array<int, 256> lastOcc {buildBadCharTable(pattern)};
  std::vector<int> goodSuffix {};
  if (useGoodSuffix) {
    goodSuffix = buildGoodSuffixTable(pattern);
  }

  // s is the position in the text where the pattern currently starts
  int s {0};
  while (s <= n - m) {
    if (stats != nullptr) {
      ++stats->alignments;
    }

    // compare from right to left. when the loop ends, pattern[j+1..m-1]
    // has matched, and j < 0 means the whole pattern matched
    int j {m - 1};
    while (j >= 0) {
      if (stats != nullptr) {
        ++stats->comparisons;
      }
      if (pattern[j] != text[s + j]) {
        break;
      }
      --j;
    }

    // full match. matches can overlap, so the shift is goodSuffix[0], not m
    if (j < 0) {
      matches.push_back(static_cast<std::size_t>(s));
      s += useGoodSuffix ? goodSuffix[0] : 1;
      continue;
    }

    // mismatch at pattern[j]. the bad character shift is 0 or negative when
    // the last copy of the text character is to the right of j. the good
    // suffix shift is always at least 1, so the larger shift is always
    // positive and the pattern always moves forward.
    int badChar {j - lastOcc[static_cast<unsigned char>(text[s + j])]};
    int goodSuf {useGoodSuffix ? goodSuffix[j + 1] : 1};
    if (stats != nullptr && useGoodSuffix) {
      if (badChar > goodSuf) {
        ++stats->badCharWins;
      } else if (goodSuf > badChar) {
        ++stats->goodSuffixWins;
      } else {
        ++stats->ties;
      }
    }
    s += std::max(badChar, goodSuf);
  }
  return matches;
}

}  // namespace

// ---- public functions ----

// the standard search. same inputs and output as kmp_search.
// this is the version to use in the benchmark.
std::vector<std::size_t> boyer_moore_search(const std::string& text,
                                            const std::string& pattern) {
  return search(text, pattern, nullptr, true);
}

// the same search, but it also records the work done in stats
std::vector<std::size_t> boyer_moore_search_counted(const std::string& text,
                                                    const std::string& pattern,
                                                    BMStats& stats,
                                                    bool useGoodSuffix) {
  return search(text, pattern, &stats, useGoodSuffix);
}

