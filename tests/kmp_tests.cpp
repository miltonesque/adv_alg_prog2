// kmp_tests.cpp - tests for knuth-morris-pratt (kmp.cpp)
//
//
// this has its own main, so it's built on its own (see CMakeLists.txt).
// build and run everything with cmake (see README), or this file alone:
//   g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -g kmp_tests.cpp kmp.cpp -o kmp_tests && ./kmp_tests
//
// each test prints PASS or FAIL, and the last line is the score

#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "common.hpp"
#include "kmp.hpp"

static int passed{ 0 };
static int total{ 0 };

static void check(bool ok, const std::string& name) {
    ++total;
    if (ok) {
        ++passed;
        std::cout << "PASS " << name << '\n';
    }
    else {
        std::cout << "FAIL " << name << '\n';
    }
}

// ---- helpers ----

// works out the lps table the slow way, straight from what it means, so we
// can check the fast one. lps[i] is the longest proper prefix of
// p[0..i] that is also a suffix of it.
static std::vector<std::size_t> lpsByDefinition(const std::string& p) {
    std::vector<std::size_t> out(p.size(), 0);
    for (std::size_t i = 0; i < p.size(); ++i) {
        for (std::size_t len = i; len > 0; --len) {  // proper, so len <= i
            if (p.compare(0, len, p, i + 1 - len, len) == 0) {
                out[i] = len;
                break;
            }
        }
    }
    return out;
}

// ---- tests ----

// small examples worked out by hand
static void handWorked() {
    check(kmp_search("ABABDABACDABABCABAB", "ABABCABAB") == Matches{ 10 },
        "textbook example");
    check(kmp_search("ABCABCABD", "ABCABD") == Matches{ 3 },
        "partial match fails on the last character");
    check(kmp_search("abcabcabc", "xyz").empty(), "no match");
}

// matches can share characters, and every one has to be found
static void overlapping() {
    check(kmp_search("aaaa", "aa") == Matches{ 0, 1, 2 }, "overlapping aa in aaaa");
    check(kmp_search("ABABABA", "ABA") == Matches{ 0, 2, 4 },
        "overlapping ABA");
}

// the agreed rule: an empty pattern, or one longer than the text, gives
// no matches. same as boyer_moore_search and rabin_karp_search
static void edgeCases() {
    check(kmp_search("abc", "").empty(), "empty pattern");
    check(kmp_search("", "").empty(), "empty text and pattern");
    check(kmp_search("", "a").empty(), "empty text");
    check(kmp_search("ab", "abc").empty(), "pattern longer than text");
    check(kmp_search("abc", "abc") == Matches{ 0 }, "pattern is the whole text");
    check(kmp_search("abxxx", "ab") == Matches{ 0 }, "match at the very start");
    check(kmp_search("xxxab", "ab") == Matches{ 3 }, "match at the very end");
}

// char is signed on most compilers, so bytes above 127 are negative. NUL
// bytes have to work too
static void byteValues() {
    check(kmp_search("\xff\x80\xff\x80", "\xff\x80") == Matches{ 0, 2 },
        "bytes above 127");
    const std::string nul(1, '\0');
    check(kmp_search("a" + nul + "b" + nul + "b", nul + "b") == Matches{ 1, 3 },
        "NUL bytes in text and pattern");
}

// the lps table on its own
static void lpsTable() {
    check(buildLPS("ABABC") == std::vector<std::size_t> {0, 0, 1, 2, 0},
        "lps for ABABC is 0 0 1 2 0");
    check(buildLPS("AABAAA") == std::vector<std::size_t> {0, 1, 0, 1, 2, 2},
        "lps for AABAAA is 0 1 0 1 2 2 (falls back twice)");
    check(buildLPS("aaaa") == std::vector<std::size_t> {0, 1, 2, 3},
        "lps for aaaa is 0 1 2 3");
    check(buildLPS("abcd") == std::vector<std::size_t> {0, 0, 0, 0},
        "lps for abcd is 0 0 0 0");
    check(buildLPS("").empty(), "lps for the empty pattern is empty");

    // 3000 random patterns. only a few letters, so there are lots of
    // repeats and borders, which is where this table usually goes wrong
    bool ok{ true };
    std::mt19937 mt{ 1 };
    for (int trial = 0; trial < 3000 && ok; ++trial) {
        const std::string p{ randomString(1 + static_cast<int>(mt() % 12), "abc", mt) };
        ok = buildLPS(p) == lpsByDefinition(p);
    }
    check(ok, "lps table matches the slow version (3000 patterns)");
}

// every text over {a, b} up to length 8 against every pattern up to
// length 4. two letters means heaps of half matches and overlaps
static void everySmallInput() {
    const std::vector<std::string> texts{ allStrings("ab", 8) };
    const std::vector<std::string> patterns{ allStrings("ab", 4) };
    bool ok{ true };
    for (const std::string& t : texts) {
        for (const std::string& p : patterns) {
            if (ok && kmp_search(t, p) != naiveSearch(t, p)) {
                std::cout << "  first wrong one: text \"" << t << "\" pattern \"" << p
                    << "\"\n";
                ok = false;
            }
        }
    }
    check(ok, "every small {a, b} input matches naive (" +
        std::to_string(texts.size() * patterns.size()) + " cases)");
}

// bigger random inputs, checked against naive and std::string::find
static void randomInputs() {
    // any of the 256 byte values, but only 4 per test so matches still happen
    bool ok{ true };
    std::mt19937 mt{ 4 };
    for (int trial = 0; trial < 5000 && ok; ++trial) {
        std::string letters(4, ' ');
        for (char& c : letters) {
            c = static_cast<char>(mt() % 256);
        }
        const std::string text{ randomString(static_cast<int>(mt() % 80), letters, mt) };
        const std::string pattern{
            randomString(1 + static_cast<int>(mt() % 6), letters, mt) };
        ok = kmp_search(text, pattern) == naiveSearch(text, pattern);
    }
    check(ok, "5000 random byte strings match naive");

    // longer texts. the pattern is cut out of the text so it's always there
    ok = true;
    mt.seed(5);
    for (int trial = 0; trial < 200 && ok; ++trial) {
        const std::string letters{ trial % 2 == 0 ? "ab" : "abcdefghijklmnopqrstuvwxyz" };
        const std::string text{ randomString(5000, letters, mt) };
        const int m{ 1 + trial % 40 };
        const std::string pattern{ text.substr(mt() % (text.size() - m), m) };
        const Matches got{ kmp_search(text, pattern) };
        ok = got == naiveSearch(text, pattern) && got == stdFindSearch(text, pattern);
    }
    check(ok, "200 long cases match naive and std::string::find");
}

// the inputs benchmark.cpp uses for the worst cases
static void worstCases() {
    Matches everyPosition{};
    for (std::size_t i = 0; i <= 990; ++i) {
        everyPosition.push_back(i);
    }
    check(kmp_search(std::string(1000, 'a'), std::string(10, 'a')) == everyPosition,
        "all a's: every position is a match");
    check(kmp_search(std::string(1000, 'a'), std::string(9, 'a') + "b").empty(),
        "all a's, pattern a's then b: no match");
    const std::string text{ repeated("ab", 1000) };
    const std::string pattern{ repeated("ab", 10) };
    check(kmp_search(text, pattern) == naiveSearch(text, pattern),
        "repeating ab text and pattern");
}

// the counted version must give the same answer, and its counts must
// respect the O(n) bound: at most 2n comparisons (each text character is
// matched once, and each fallback undoes an earlier match)
static void countedVersion() {
    bool sameAnswer{ true };
    bool withinBound{ true };
    std::mt19937 mt{ 6 };
    for (int trial = 0; trial < 2000; ++trial) {
        const std::string text{ randomString(static_cast<int>(mt() % 300), "ab", mt) };
        const std::string pattern{ randomString(1 + static_cast<int>(mt() % 8), "ab", mt) };
        KMPStats stats{};
        sameAnswer = sameAnswer && kmp_search_counted(text, pattern, stats) ==
            kmp_search(text, pattern);
        withinBound = withinBound && stats.comparisons <= 2 * text.size();
    }
    check(sameAnswer, "counted version gives the same matches (2000 cases)");
    check(withinBound, "comparisons never exceed 2n (2000 cases)");

    // all a's with pattern a..ab: worked out by hand, every text character
    // after the first m - 1 costs two comparisons (the b fails, then the
    // fallback to m - 2 succeeds)
    KMPStats stats{};
    kmp_search_counted(std::string(100, 'a'), std::string(9, 'a') + "b", stats);
    check(stats.comparisons == 9 + 2 * 91 && stats.fallbacks == 91,
        "a-then-b counts are exact: 191 comparisons, 91 fallbacks");
}

int main() {
    handWorked();
    overlapping();
    edgeCases();
    byteValues();
    lpsTable();
    everySmallInput();
    randomInputs();
    worstCases();
    countedVersion();
    std::cout << "SCORE: " << passed << " / " << total << '\n';
    return passed == total ? 0 : 1;
}
