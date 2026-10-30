// bmp_tests.cpp - tests for boyer-moore (bmp.cpp)
//
// primary author: r
//
// this has its own main, so it's built on its own (see CMakeLists.txt) and
// isn't part of String Matching.vcxproj. scripts/reproduce.py builds and
// runs it, or to build and run it alone with g++:
//   g++ -std=c++17 bmp_tests.cpp bmp.cpp -o bmp_tests
//   ./bmp_tests
// or in developer powershell for visual studio:
//   cl /std:c++17 /EHsc /W4 bmp_tests.cpp bmp.cpp
//   .\bmp_tests.exe
//
// each test prints PASS or FAIL, and the last line is the score

#include <array>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "bmp.hpp"
#include "common.hpp"

static int passed {0};
static int total {0};

static void check(bool ok, const std::string& name) {
  ++total;
  if (ok) {
    ++passed;
    std::cout << "PASS " << name << '\n';
  } else {
    std::cout << "FAIL " << name << '\n';
  }
}

// ---- helpers ----

// works out one good suffix shift the slow way, straight from what it
// means, so we can check the fast table. pattern[j..m-1] matched and
// pattern[j-1] didn't. the shift is the smallest d >= 1 where
//   (a) everything that matched still lines up after moving right by d
//       (anything that slides off the left end doesn't count), and
//   (b) the character now sitting where the mismatch was isn't
//       pattern[j-1] again, or it's slid off the left end.
// j = 0 means the whole pattern matched, so only (a) matters.
static int goodSuffixByDefinition(const std::string& p, int j) {
  const int m {static_cast<int>(p.size())};
  for (int d = 1; d <= m; ++d) {
    bool ok {true};
    for (int k = j; k < m && ok; ++k) {
      if (k - d >= 0 && p[k - d] != p[k]) {
        ok = false;
      }
    }
    if (ok && j > 0 && j - 1 - d >= 0 && p[j - 1 - d] == p[j - 1]) {
      ok = false;
    }
    if (ok) {
      return d;
    }
  }
  return m;
}

// ---- tests ----

// small examples worked out by hand
static void handWorked() {
  check(boyer_moore_search("HERE IS A SIMPLE EXAMPLE", "EXAMPLE") ==
            Matches {17},
        "textbook example");
  check(boyer_moore_search("ABAAABCD", "ABC") == Matches {4},
        "match in the middle");
  check(boyer_moore_search("abcabcabc", "xyz").empty(), "no match");
}

// matches can share characters, and every one has to be found
static void overlapping() {
  check(boyer_moore_search("aaaa", "aa") == Matches {0, 1, 2},
        "overlapping aa in aaaa");
  check(boyer_moore_search("abababab", "abab") == Matches {0, 2, 4},
        "overlapping abab");
  check(boyer_moore_search("abcabcab", "abcab") == Matches {0, 3},
        "overlap through the border ab");
}

// the agreed rule: an empty pattern, or one longer than the text, gives
// no matches. same as kmp_search and rabin_karp_search
static void edgeCases() {
  check(boyer_moore_search("abc", "").empty(), "empty pattern");
  check(boyer_moore_search("", "").empty(), "empty text and pattern");
  check(boyer_moore_search("", "a").empty(), "empty text");
  check(boyer_moore_search("ab", "abc").empty(), "pattern longer than text");
  check(boyer_moore_search("abc", "abc") == Matches {0},
        "pattern is the whole text");
  check(boyer_moore_search("abxxx", "ab") == Matches {0},
        "match at the very start");
  check(boyer_moore_search("xxxab", "ab") == Matches {3},
        "match at the very end");
}

// char is signed on most compilers, so bytes above 127 would be negative
// table indexes if bmp.cpp didn't cast them. NUL bytes have to work too
static void byteValues() {
  check(boyer_moore_search("ab\xff\xfe" "ab\xff", "b\xff") == Matches {1, 5},
        "bytes above 127");
  const std::string nul(1, '\0');
  check(boyer_moore_search("a" + nul + "b" + nul + "b", nul + "b") ==
            Matches {1, 3},
        "NUL bytes in text and pattern");
}

// the two tables on their own, using the abcab example from bmp.cpp
static void tables() {
  const std::array<int, 256> lastOcc {buildBadCharTable("abcab")};
  check(lastOcc['a'] == 3 && lastOcc['b'] == 4 && lastOcc['c'] == 2 &&
            lastOcc['x'] == -1,
        "bad character table for abcab");

  check(buildGoodSuffixTable("abcab") == std::vector<int> {3, 3, 3, 3, 5, 1},
        "good suffix table for abcab is 3 3 3 3 5 1");

  // 3000 random patterns. only a few letters, so there are lots of
  // repeats and borders, which is where this table usually goes wrong
  bool ok {true};
  std::mt19937 mt {1};
  for (int trial = 0; trial < 3000 && ok; ++trial) {
    const std::string p {
        randomString(1 + static_cast<int>(mt() % 12), "abc", mt)};
    const std::vector<int> table {buildGoodSuffixTable(p)};
    for (int j = 0; j <= static_cast<int>(p.size()) && ok; ++j) {
      ok = table[j] == goodSuffixByDefinition(p, j);
    }
  }
  check(ok, "good suffix table matches the slow version (3000 patterns)");
}

// every text over {a, b} up to length 10 against every pattern up to
// length 5. two letters means heaps of half matches and overlaps
static void everySmallInput() {
  const std::vector<std::string> texts {allStrings("ab", 10)};
  const std::vector<std::string> patterns {allStrings("ab", 5)};
  bool ok {true};
  for (const std::string& t : texts) {
    for (const std::string& p : patterns) {
      if (ok && boyer_moore_search(t, p) != naiveSearch(t, p)) {
        std::cout << "  first wrong one: text \"" << t << "\" pattern \""
                  << p << "\"\n";
        ok = false;
      }
    }
  }
  check(ok, "every small {a, b} input matches naive (" +
                std::to_string(texts.size() * patterns.size()) + " cases)");
}

// bigger random inputs, checked against naive and std::string::find
static void randomInputs() {
  bool ok {true};
  std::mt19937 mt {2};
  const std::vector<std::string> alphabets {"a", "ab", "abc", "abcd"};
  for (int trial = 0; trial < 20000 && ok; ++trial) {
    const std::string& letters {alphabets[mt() % alphabets.size()]};
    const std::string text {
        randomString(static_cast<int>(mt() % 60), letters, mt)};
    const std::string pattern {
        randomString(1 + static_cast<int>(mt() % 8), letters, mt)};
    ok = boyer_moore_search(text, pattern) == naiveSearch(text, pattern);
  }
  check(ok, "20000 random small cases match naive");

  // any of the 256 byte values, but only 4 per test so matches still happen
  ok = true;
  mt.seed(4);
  for (int trial = 0; trial < 5000 && ok; ++trial) {
    std::string letters(4, ' ');
    for (char& c : letters) {
      c = static_cast<char>(mt() % 256);
    }
    const std::string text {
        randomString(static_cast<int>(mt() % 80), letters, mt)};
    const std::string pattern {
        randomString(1 + static_cast<int>(mt() % 6), letters, mt)};
    ok = boyer_moore_search(text, pattern) == naiveSearch(text, pattern);
  }
  check(ok, "5000 random byte strings match naive");

  // longer texts. the pattern is cut out of the text so it's always there
  ok = true;
  mt.seed(5);
  for (int trial = 0; trial < 200 && ok; ++trial) {
    const std::string letters {trial % 2 == 0 ? "ab"
                                              : "abcdefghijklmnopqrstuvwxyz"};
    const std::string text {randomString(5000, letters, mt)};
    const int m {1 + trial % 40};
    const std::string pattern {text.substr(mt() % (text.size() - m), m)};
    const Matches got {boyer_moore_search(text, pattern)};
    ok = got == naiveSearch(text, pattern) &&
         got == stdFindSearch(text, pattern);
  }
  check(ok, "200 long cases match naive and std::string::find");
}

// the inputs benchmark.cpp uses for the worst cases. these are where
// boyer-moore does the most work, so the answers need to be right here too
static void worstCases() {
  Matches everyPosition {};
  for (std::size_t i = 0; i <= 990; ++i) {
    everyPosition.push_back(i);
  }
  check(boyer_moore_search(std::string(1000, 'a'), std::string(10, 'a')) ==
            everyPosition,
        "all a's: every position is a match");

  check(boyer_moore_search(std::string(1000, 'a'),
                           std::string(9, 'a') + "b").empty(),
        "all a's, pattern a's then b: no match");

  const std::string text {repeated("ab", 1000)};
  const std::string pattern {repeated("ab", 10)};
  check(boyer_moore_search(text, pattern) == naiveSearch(text, pattern),
        "repeating ab text and pattern");
}

// the counted version is used for counts.csv, so it must give the same
// matches as boyer_moore_search, and its counts must make sense: at most one
// alignment per position (n - m + 1), at least one comparison per alignment,
// and at most m comparisons per alignment
static void countedVersion() {
  bool sameAnswer {true};
  bool withinBounds {true};
  std::mt19937 mt {6};
  for (int trial = 0; trial < 2000; ++trial) {
    const std::string text {
        randomString(static_cast<int>(mt() % 300), "ab", mt)};
    const std::string pattern {
        randomString(1 + static_cast<int>(mt() % 8), "ab", mt)};
    BMStats stats {};
    sameAnswer = sameAnswer && boyer_moore_search_counted(text, pattern,
                                                          stats) ==
                                   boyer_moore_search(text, pattern);
    if (pattern.size() <= text.size()) {
      const std::size_t positions {text.size() - pattern.size() + 1};
      withinBounds = withinBounds && stats.alignments <= positions &&
                     stats.comparisons >= stats.alignments &&
                     stats.comparisons <= stats.alignments * pattern.size();
    }
  }
  check(sameAnswer, "counted version gives the same matches (2000 cases)");
  check(withinBounds, "counts stay within their bounds (2000 cases)");

  // worked out by hand. all a's with pattern a's: every position is a
  // match and the good suffix shift is 1, so 991 alignments of 10
  // comparisons each
  BMStats allA {};
  boyer_moore_search_counted(std::string(1000, 'a'), std::string(10, 'a'),
                             allA);
  check(allA.alignments == 991 && allA.comparisons == 9910,
        "all a's counts are exact: 991 alignments, 9910 comparisons");

  // pattern with a letter the text never has: the first comparison fails
  // and the bad character rule moves the pattern past it, so n / m
  // alignments of 1 comparison each
  BMStats skip {};
  boyer_moore_search_counted(std::string(1000, 'a'), std::string(10, 'b'),
                             skip);
  check(skip.alignments == 100 && skip.comparisons == 100,
        "skipping counts are exact: 100 alignments, 100 comparisons");
}

int main() {
  handWorked();
  overlapping();
  edgeCases();
  byteValues();
  tables();
  everySmallInput();
  randomInputs();
  worstCases();
  countedVersion();
  std::cout << "SCORE: " << passed << " / " << total << '\n';
  return passed == total ? 0 : 1;
}
