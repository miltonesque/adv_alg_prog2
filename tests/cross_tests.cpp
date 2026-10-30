// correctness.cpp - every benchmarked algorithm against a naive reference
//
// the algorithms come from allAlgorithms() in include/benchmark.hpp, which is
// the same list the benchmark times, so nothing is timed that has not been
// checked here. every algorithm must equal the naive answer exactly.
//
// the counted versions are checked against the uncounted ones, the counters
// against bounds from the theory, and karp-rabin's fast remainder step
// (added for benchmarking) against plain % arithmetic.

#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "benchmark.hpp"
#include "input_generators.hpp"
#include "test_utils.hpp"

static TestReport report{};

// run every algorithm on one case; returns false and prints the first failure
static bool allAgree(const std::string& text, const std::string& pattern) {
    const Matches expected{ naiveSearch(text, pattern) };
    for (const Algorithm& a : allAlgorithms()) {
        const Matches got{ a.search(text, pattern) };
        if (got != expected) {
            std::cout << "  " << a.name << " differs on text(" << text.size()
                << ") pattern \"" << pattern << "\"\n";
            return false;
        }
    }
    return true;
}

// exhaustive: every text and pattern over a small alphabet up to a length
static void exhaustive(const std::string& letters, int maxText, int maxPattern) {
    const std::vector<std::string> texts{ allStrings(letters, maxText) };
    const std::vector<std::string> patterns{ allStrings(letters, maxPattern) };
    bool ok{ true };
    for (const std::string& t : texts) {
        for (const std::string& p : patterns) {
            if (ok) {
                ok = allAgree(t, p);
            }
        }
    }
    report.check(ok, "exhaustive over {" + letters + "}: text <= " +
        std::to_string(maxText) + ", pattern <= " + std::to_string(maxPattern) +
        " (" + std::to_string(texts.size() * patterns.size()) + " cases)");
}

static void randomSmall() {
    std::mt19937 mt{ 101 };
    const std::vector<std::string> alphabets{ "a", "ab", "abcd",
        "abcdefghijklmnopqrstuvwxyz" };
    bool ok{ true };
    for (int trial = 0; trial < 20000 && ok; ++trial) {
        const std::string& letters{ alphabets[mt() % alphabets.size()] };
        const std::string text{ randomString(static_cast<int>(mt() % 200), letters, mt) };
        const std::string pattern{
            randomString(1 + static_cast<int>(mt() % 12), letters, mt) };
        ok = allAgree(text, pattern);
    }
    report.check(ok, "20000 random small cases, alphabets of size 1, 2, 4, 26");

    ok = true;
    for (int trial = 0; trial < 5000 && ok; ++trial) {
        std::string letters(1 + mt() % 6, ' ');
        for (char& c : letters) {
            c = static_cast<char>(mt() % 256);
        }
        const std::string text{ randomString(static_cast<int>(mt() % 150), letters, mt) };
        const std::string pattern{
            randomString(1 + static_cast<int>(mt() % 8), letters, mt) };
        ok = allAgree(text, pattern);
    }
    report.check(ok, "5000 random cases over arbitrary byte values");
}

// the benchmark's own generators at moderate size
static void benchmarkFamilies() {
    using namespace InputGenerators;
    bool ok{ true };
    std::uint64_t seed{ 7 };
    for (std::size_t sigma : { 2, 4, 26, 256 }) {
        for (std::size_t m : { 1, 2, 5, 16, 64, 300 }) {
            const TestInput in{ randomEmbedded(50000, m, sigma, ++seed) };
            ok = ok && allAgree(in.text, in.pattern);
        }
    }
    report.check(ok, "random_embedded family, n = 50000, sigma in {2,4,26,256}");

    ok = true;
    for (std::size_t m : { 1, 2, 8, 33, 128 }) {
        ok = ok && allAgree(allSame(5000, m).text, allSame(5000, m).pattern);
        const TestInput last{ allSameMismatchLast(5000, m) };
        ok = ok && allAgree(last.text, last.pattern);
        const TestInput first{ allSameMismatchFirst(5000, m) };
        ok = ok && allAgree(first.text, first.pattern);
    }
    report.check(ok, "repetitive families (all_match, mismatch_last, mismatch_first)");

    ok = true;
    for (std::size_t m : { 8, 9, 20, 100 }) {
        const TestInput adv{ rabinKarpAdversarial(3000, m, kRKFixedBase, kRKModulus) };
        ok = ok && allAgree(adv.text, adv.pattern);
    }
    report.check(ok, "rk_adversarial family: all algorithms still exact");
}

// the adversarial generator really does collide every window under base 31,
// and a random base does not
static void adversarialCollides() {
    using namespace InputGenerators;
    const std::size_t n{ 4000 };
    const std::size_t m{ 64 };
    const TestInput adv{ rabinKarpAdversarial(n, m, kRKFixedBase, kRKModulus) };
    report.check(adv.pattern != std::string(m, 'a') &&
        polynomialHash(adv.pattern, kRKFixedBase, kRKModulus) ==
        polynomialHash(std::string(m, 'a'), kRKFixedBase, kRKModulus),
        "adversarial pattern differs from a^m but has the same base-31 hash");

    RKStats fixed{};
    rabin_karp_search_counted(adv.text, adv.pattern, kRKFixedBase, kRKModulus, fixed);
    report.check(fixed.hashHits == n - m + 1 && fixed.falsePositives == n - m + 1,
        "fixed base 31: every window is a false positive");
    report.check(fixed.comparisons >= (n - m + 1) * (m - 8),
        "fixed base 31: at least (n-m+1)(m-8) verification comparisons");

    RKStats random{};
    rabin_karp_search_counted(adv.text, adv.pattern, 123456789ULL, kRKModulus, random);
    report.check(random.hashHits == 0, "another base: no hash hits on the same input");
}

// counted == uncounted, and the counters respect the theory
static void countedVersions() {
    std::mt19937 mt{ 202 };
    bool same{ true };
    bool kmpBound{ true };
    bool rkConsistent{ true };
    for (int trial = 0; trial < 3000; ++trial) {
        const std::string letters{ trial % 3 == 0 ? "a" : trial % 3 == 1 ? "ab" : "abcd" };
        const std::string text{ randomString(static_cast<int>(mt() % 400), letters, mt) };
        const std::string pattern{
            randomString(1 + static_cast<int>(mt() % 10), letters, mt) };
        const Matches expected{ naiveSearch(text, pattern) };

        KMPStats ks{};
        same = same && kmp_search_counted(text, pattern, ks) == expected;
        // each text character is compared at most once more than it is
        // passed, giving the classic bound of 2n comparisons
        kmpBound = kmpBound && ks.comparisons <= 2 * text.size() &&
            ks.fallbacks <= text.size();

        BMStats bs{};
        same = same && boyer_moore_search_counted(text, pattern, bs) == expected;

        const unsigned long long base{ 2 + mt() % 1000 };
        RKStats rs{};
        same = same && rabin_karp_search_counted(text, pattern, base, kRKModulus, rs) ==
            expected;
        rkConsistent = rkConsistent &&
            rs.hashHits == expected.size() + rs.falsePositives;
    }
    report.check(same, "counted versions return the same matches (3000 cases)");
    report.check(kmpBound, "kmp: comparisons <= 2n and fallbacks <= n");
    report.check(rkConsistent, "karp-rabin: hash hits = true matches + false positives");

    KMPStats allA{};
    kmp_search_counted(std::string(1000, 'a'), std::string(10, 'a'), allA);
    report.check(allA.comparisons == 1000 && allA.fallbacks == 0,
        "kmp on a^1000 / a^10: exactly 1000 comparisons, 0 fallbacks");
}

// a tiny modulus forces many collisions. karp-rabin must still be exact,
// because every hash hit is checked character by character.
static void forcedCollisions() {
    std::mt19937 mt{ 303 };
    bool exact{ true };
    bool sawFalsePositive{ false };
    for (int trial = 0; trial < 3000; ++trial) {
        const std::string text{ randomString(static_cast<int>(mt() % 300), "abc", mt) };
        const std::string pattern{ randomString(1 + static_cast<int>(mt() % 6), "abc", mt) };
        const unsigned long long modulus{ trial % 2 == 0 ? 7ULL : 13ULL };
        const unsigned long long base{ 2 + mt() % (modulus - 2) };
        RKStats rs{};
        exact = exact &&
            rabin_karp_search_counted(text, pattern, base, modulus, rs) ==
            naiveSearch(text, pattern);
        sawFalsePositive = sawFalsePositive || rs.falsePositives > 0;
    }
    report.check(exact && sawFalsePositive,
        "karp-rabin stays exact under modulus 7 and 13, where false positives occur");
}

int main() {
    rabin_karp_seed(20261029);
    exhaustive("ab", 10, 5);
    exhaustive("abc", 7, 4);
    randomSmall();
    benchmarkFamilies();
    adversarialCollides();
    countedVersions();
    forcedCollisions();
    return report.finish();
}
