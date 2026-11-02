// explanation_tests.cpp - experiments that TEST the explanations in the
// report
// 
// each test changes ONE thing that an explanation depends on, and states in
// advance which way the result must move if the explanation is right:
//
//   T1 kmp_dfa       claim: KMP is slow on small alphabets because its
//                    fallback loop is an unpredictable branch.
//                    control: a DFA version of KMP (one table lookup per
//                    character, no fallback loop) on the same inputs.
//                    prediction: the DFA's time is flat in sigma, while KMP's
//                    rises as sigma falls.
//   T2 bm_dependency claim: on a^n / a^(m-1)b Boyer-Moore is slow because each
//                    step must wait for the previous step's table lookup to
//                    know where to look next (a dependency chain).
//                    control: the same loop doing the same loads, comparisons
//                    and max, but the next position is s + 1, known in advance
//                    (valid only on this input, where every real shift is 1;
//                    checked).
//                    prediction: the control is several times faster than
//                    Boyer-Moore, at or below KMP's time.
//
// run with no arguments; scripts/reproduce.py runs it after the benchmark.
// it writes explain_kmp_dfa.csv and explain_bm_dependency.csv to results/.
//

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "benchmark.hpp"
#include "input_generators.hpp"
#include "boyer_moore.hpp"
#include "kmp.hpp"
#include "rabin_karp.hpp"

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

namespace {

    using Clock = std::chrono::steady_clock;
    using namespace InputGenerators;
    using SearchFn = Matches(*)(const std::string&, const std::string&);

    volatile std::size_t g_sink{ 0 };

    // ---- control 1: KMP as a DFA ----
    // state = length of the current partial match. dfa[state][c] is the next
    // state after reading c, precomputed from the failure function, so the
    // search loop is one load per character with no inner fallback loop.
    NOINLINE Matches kmpDfaSearch(const std::string& text, const std::string& pattern) {
        Matches out{};
        const std::size_t n{ text.size() };
        const std::size_t m{ pattern.size() };
        if (m == 0 || m > n) {
            return out;
        }
        const std::vector<std::size_t> lps{ buildLPS(pattern) };
        std::vector<std::uint32_t> dfa((m + 1) * 256, 0);
        for (std::size_t state = 0; state <= m; ++state) {
            for (int c = 0; c < 256; ++c) {
                std::uint32_t next{ 0 };
                if (state < m && static_cast<unsigned char>(pattern[state]) == c) {
                    next = static_cast<std::uint32_t>(state + 1);
                }
                else if (state > 0) {
                    next = dfa[lps[state - 1] * 256 + c];
                }
                dfa[state * 256 + c] = next;
            }
        }
        std::uint32_t state{ 0 };
        const std::uint32_t full{ static_cast<std::uint32_t>(m) };
        for (std::size_t i = 0; i < n; ++i) {
            state = dfa[state * 256 + static_cast<unsigned char>(text[i])];
            if (state == full) {
                out.push_back(i + 1 - m);
            }
        }
        return out;
    }

    // ---- control 2: Boyer-Moore with the next position known in advance ----
    // the same loads, comparisons and max as boyer_moore_search, but s += 1.
    // only valid when every real shift is 1 (checked by the caller through
    // shiftSum, which must equal the number of alignments).
    std::size_t g_shiftSum{ 0 };
    NOINLINE Matches bmDecoupledSearch(const std::string& text, const std::string& pattern) {
        Matches out{};
        const int n{ static_cast<int>(text.size()) };
        const int m{ static_cast<int>(pattern.size()) };
        if (m == 0 || m > n) {
            return out;
        }
        const std::array<int, 256> lastOcc{ buildBadCharTable(pattern) };
        const std::vector<int> goodSuffix{ buildGoodSuffixTable(pattern) };
        std::size_t shiftSum{ 0 };
        for (int s = 0; s <= n - m; ++s) {
            int j{ m - 1 };
            while (j >= 0 && pattern[j] == text[s + j]) {
                --j;
            }
            if (j < 0) {
                out.push_back(static_cast<std::size_t>(s));
                shiftSum += static_cast<std::size_t>(goodSuffix[0]);
            }
            else {
                const int badChar{ j - lastOcc[static_cast<unsigned char>(text[s + j])] };
                shiftSum += static_cast<std::size_t>(std::max(badChar, goodSuffix[j + 1]));
            }
        }
        g_shiftSum = shiftSum;
        return out;
    }

    // the library algorithms, wrapped so they are not inlined into the driver
    NOINLINE Matches kmpCall(const std::string& t, const std::string& p) { return kmp_search(t, p); }
    NOINLINE Matches bmCall(const std::string& t, const std::string& p) { return boyer_moore_search(t, p); }
    NOINLINE Matches rkCall(const std::string& t, const std::string& p) { return rabin_karp_search(t, p); }

    struct Named {
        std::string name;
        SearchFn fn;
    };

    double seconds(Clock::time_point a) {
        return std::chrono::duration<double>(Clock::now() - a).count();
    }

    // median and quartiles of `reps` samples, each sample batched to >= minMs.
    // algorithms are interleaved in a shuffled order each round, as in the main
    // benchmark.
    struct Stat { double median, q1, q3; };

    std::vector<Stat> timeAll(const std::vector<Named>& algs, const std::string& t,
        const std::string& p, std::size_t reps, double minMs, std::mt19937_64& rng) {
        auto once = [&](SearchFn f) {
            const Matches got{ f(t, p) };
            g_sink = g_sink + got.size();
            };
        std::vector<std::size_t> batch(algs.size(), 1);
        for (std::size_t a = 0; a < algs.size(); ++a) {
            const auto s{ Clock::now() };
            once(algs[a].fn);
            const double sec{ seconds(s) };
            if (sec > 0 && sec < minMs / 1000) {
                batch[a] = static_cast<std::size_t>(std::ceil(minMs / 1000 / sec));
            }
        }
        std::vector<std::vector<double>> samples(algs.size());
        std::vector<std::size_t> order(algs.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        for (std::size_t r = 0; r < reps; ++r) {
            std::shuffle(order.begin(), order.end(), rng);
            for (std::size_t a : order) {
                const auto s{ Clock::now() };
                for (std::size_t b = 0; b < batch[a]; ++b) {
                    once(algs[a].fn);
                }
                samples[a].push_back(seconds(s) * 1e9 / static_cast<double>(batch[a]));
            }
        }
        std::vector<Stat> out{};
        for (auto& v : samples) {
            std::sort(v.begin(), v.end());
            auto q = [&](double f) { return v[static_cast<std::size_t>(f * (v.size() - 1) + 0.5)]; };
            out.push_back({ q(0.5), q(0.25), q(0.75) });
        }
        return out;
    }

    std::string f3(double v) {
        std::ostringstream s;
        s << std::fixed << std::setprecision(3) << v;
        return s.str();
    }

    // ---- T1 ----
    bool testKmpDfa(const std::string& dir, std::uint64_t seed, std::size_t reps,
        std::mt19937_64& rng) {
        std::ofstream out{ dir + "/explain_kmp_dfa.csv" };
        out << "sigma,n,m,algorithm,median_ns_per_char,q1_ns_per_char,q3_ns_per_char,fallbacks_per_char\n";
        const std::size_t n{ std::size_t{ 1 } << 22 };
        const std::vector<Named> algs{ { "kmp", kmpCall }, { "kmp_dfa", kmpDfaSearch },
                                       { "karp_rabin", rkCall } };
        bool ok{ true };
        // identical instances to the main benchmark's `alphabet` experiment
        std::uint64_t s{ seed + 3000 };
        for (std::size_t sigma : { 2, 4, 8, 16, 32, 64, 128, 256 }) {
            const TestInput in{ randomEmbedded(n, 32, sigma, ++s) };
            ok = ok && kmpDfaSearch(in.text, in.pattern) == kmp_search(in.text, in.pattern);
            KMPStats ks{};
            kmp_search_counted(in.text, in.pattern, ks);
            const double fb{ static_cast<double>(ks.fallbacks) / static_cast<double>(n) };
            const std::vector<Stat> st{ timeAll(algs, in.text, in.pattern, reps, 5.0, rng) };
            for (std::size_t a = 0; a < algs.size(); ++a) {
                out << sigma << ',' << n << ",32," << algs[a].name << ','
                    << f3(st[a].median / n) << ',' << f3(st[a].q1 / n) << ','
                    << f3(st[a].q3 / n) << ',' << f3(fb) << '\n';
            }
            std::cerr << "  T1 sigma=" << sigma << " kmp=" << f3(st[0].median / n)
                << " dfa=" << f3(st[1].median / n) << '\n';
        }
        return ok;
    }

    // ---- T2 ----
    bool testBmDependency(const std::string& dir, std::size_t reps, std::mt19937_64& rng) {
        std::ofstream out{ dir + "/explain_bm_dependency.csv" };
        out << "n,m,algorithm,median_ns_per_char,q1_ns_per_char,q3_ns_per_char\n";
        const std::size_t n{ std::size_t{ 1 } << 18 };
        const std::vector<Named> algs{ { "boyer_moore", bmCall },
                                       { "bm_next_position_known", bmDecoupledSearch },
                                       { "kmp", kmpCall } };
        bool ok{ true };
        for (std::size_t m : { 4, 16, 64, 256, 1024 }) {
            const TestInput in{ allSameMismatchLast(n, m) };
            BMStats bs{};
            const bool same{ boyer_moore_search_counted(in.text, in.pattern, bs) ==
                             bmDecoupledSearch(in.text, in.pattern) };
            // every real shift must be 1, or the control is not doing BM's work
            ok = ok && same && g_shiftSum == bs.alignments && bs.alignments == n - m + 1;
            const std::vector<Stat> st{ timeAll(algs, in.text, in.pattern, reps, 5.0, rng) };
            for (std::size_t a = 0; a < algs.size(); ++a) {
                out << n << ',' << m << ',' << algs[a].name << ',' << f3(st[a].median / n)
                    << ',' << f3(st[a].q1 / n) << ',' << f3(st[a].q3 / n) << '\n';
            }
            std::cerr << "  T2 m=" << m << " bm=" << f3(st[0].median / n)
                << " known=" << f3(st[1].median / n) << " kmp=" << f3(st[2].median / n) << '\n';
        }
        return ok;
    }

}  // namespace

int main() {
    // no options: results go to results/ (created if missing), with the
    // repetition count fixed here
    const std::string dir{ "results" };
    const std::size_t reps{ 11 };
    std::filesystem::create_directories(dir);
    const std::uint64_t seed{ 20261029 };
    std::mt19937_64 rng{ seed ^ 0x77ULL };
    const auto start{ Clock::now() };
    const bool t1{ testKmpDfa(dir, seed, reps, rng) };
    const bool t2{ testBmDependency(dir, reps, rng) };
    std::cerr << "explanation tests done in " << f3(seconds(start)) << " s; controls verified: "
        << (t1 && t2 ? "yes" : "NO") << '\n';
    return t1 && t2 ? 0 : 2;
}
