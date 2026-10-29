// benchmark_runner.cpp - experiment driver
//
// ---- method ----
// for every instance (one text and one pattern):
//   1. the instance is generated once, before any timing, and the same
//      strings are passed to every algorithm.
//   2. warm-up: each algorithm runs once. its output is checked against
//      std::string::find: every algorithm must return exactly the same
//      positions, so they also agree with each other.
//   3. calibration: from the warm-up time, a batch size is chosen so that one
//      timed sample lasts at least minSampleMs (short calls are repeated and
//      the time divided, so clock resolution does not matter).
//   4. reps rounds. in each round the algorithms run in a freshly shuffled
//      order, so slow drift (frequency scaling, other load) is spread evenly
//      over all of them instead of always penalising the one that runs last.
//   5. one extra counted run per algorithm (not timed) records comparisons,
//      shifts and hash hits for comparisons.csv.
// only the search call is timed: generation, verification and file output
// are outside the timed region. preprocessing (failure function, shift
// tables, pattern hash) is inside it, because it is part of each algorithm.
//
// every raw sample is written to runtime.csv; medians and spreads are
// computed by scripts/plot_results.py.

#include "benchmark_runner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "benchmark.hpp"
#include "input_generators.hpp"
#include "test_utils.hpp"

#ifndef BENCH_COMPILER
#define BENCH_COMPILER "unknown"
#endif
#ifndef BENCH_FLAGS
#define BENCH_FLAGS "unknown"
#endif
#ifndef BENCH_BUILD_TYPE
#define BENCH_BUILD_TYPE "unknown"
#endif

namespace {

    using Clock = std::chrono::steady_clock;
    using namespace InputGenerators;

    // results are folded into this so the compiler cannot drop a call whose
    // output is unused
    volatile std::size_t g_sink{ 0 };

    void consume(const Matches& r) {
        g_sink = g_sink + r.size() + (r.empty() ? 0 : r.front());
    }

    // one instance of one experiment
    struct Instance {
        std::string experiment;
        std::string family;
        std::string variant;
        std::size_t sigma{ 0 };  // 0 = not applicable (constructed text)
        TestInput input;
    };

    const Algorithm& algorithmByName(const std::string& name) {
        for (const Algorithm& a : allAlgorithms()) {
            if (a.name == name) {
                return a;
            }
        }
        throw std::invalid_argument("unknown algorithm " + name);
    }

    double secondsSince(Clock::time_point start) {
        return std::chrono::duration<double>(Clock::now() - start).count();
    }

    // ---- csv writers ----

    class Csv {
    public:
        Csv(const std::string& path, const std::string& header) : out_{ path } {
            if (!out_) {
                throw std::runtime_error("cannot write " + path);
            }
            out_ << header << '\n';
        }
        template <typename... T>
        void row(const T&... values) {
            bool first{ true };
            ((out_ << (first ? "" : ",") << values, first = false), ...);
            out_ << '\n';
        }
        void flush() { out_.flush(); }

    private:
        std::ofstream out_;
    };

    std::string fmt(double v) {
        std::ostringstream s;
        s << std::setprecision(6) << v;
        return s.str();
    }

    // ---- the main measurement loop ----

    class Runner {
    public:
        explicit Runner(const BenchConfig& cfg)
            : cfg_{ cfg },
            runtime_{ cfg.outDir + "/runtime.csv",
                      "experiment,family,variant,n,m,sigma,algorithm,rep,batch,"
                      "ns_per_call,ns_per_char,matches,verified" },
            counts_{ cfg.outDir + "/comparisons.csv",
                     "experiment,family,variant,n,m,sigma,algorithm,base,comparisons,"
                     "comparisons_per_char,alignments,fallbacks,hash_hits,"
                     "false_positives,matches" },
            orderRng_{ cfg.seed ^ 0xA5A5A5A5ULL } {
        }

        bool allVerified() const { return allVerified_; }

        void run(const Instance& inst, const std::vector<std::string>& algorithmNames) {
            const std::string& text{ inst.input.text };
            const std::string& pattern{ inst.input.pattern };
            const std::size_t n{ text.size() };
            const std::size_t m{ pattern.size() };

            std::vector<const Algorithm*> algs{};
            for (const std::string& name : algorithmNames) {
                algs.push_back(&algorithmByName(name));
            }

            // reference answer, computed outside any timed region
            const Matches truth{ stdFindSearch(text, pattern) };

            // warm-up, verification and calibration
            std::vector<std::size_t> batch(algs.size(), 1);
            std::vector<bool> verified(algs.size(), true);
            for (std::size_t a = 0; a < algs.size(); ++a) {
                const auto start{ Clock::now() };
                const Matches got{ algs[a]->search(text, pattern) };
                const double seconds{ secondsSince(start) };
                consume(got);
                verified[a] = got == truth;
                if (!verified[a]) {
                    allVerified_ = false;
                    std::cerr << "  VERIFY FAILED: " << algs[a]->name << " on "
                        << inst.experiment << "/" << inst.variant << " n=" << n
                        << " m=" << m << '\n';
                }
                const double target{ cfg_.minSampleMs / 1000.0 };
                if (seconds > 0 && seconds < target) {
                    batch[a] = static_cast<std::size_t>(std::ceil(target / seconds));
                    batch[a] = std::min<std::size_t>(batch[a], 1000000);
                }
            }

            // timed rounds, algorithm order shuffled each round
            std::vector<std::size_t> order(algs.size());
            for (std::size_t i = 0; i < order.size(); ++i) {
                order[i] = i;
            }
            for (std::size_t rep = 0; rep < cfg_.reps; ++rep) {
                std::shuffle(order.begin(), order.end(), orderRng_);
                for (std::size_t a : order) {
                    std::size_t matchCount{ 0 };
                    const auto start{ Clock::now() };
                    for (std::size_t b = 0; b < batch[a]; ++b) {
                        const Matches got{ algs[a]->search(text, pattern) };
                        matchCount = got.size();
                        consume(got);
                    }
                    const double ns{ secondsSince(start) * 1e9 /
                                     static_cast<double>(batch[a]) };
                    runtime_.row(inst.experiment, inst.family, inst.variant, n, m,
                        inst.sigma, algs[a]->name, rep, batch[a], fmt(ns),
                        fmt(ns / static_cast<double>(n)), matchCount,
                        verified[a] ? 1 : 0);
                }
            }
            runtime_.flush();

            // one counted run per algorithm, not timed
            for (const Algorithm* a : algs) {
                writeCounts(inst, *a);
            }
            counts_.flush();
        }

    private:
        void writeCounts(const Instance& inst, const Algorithm& a) {
            const std::string& t{ inst.input.text };
            const std::string& p{ inst.input.pattern };
            const double n{ static_cast<double>(t.size()) };
            auto emit = [&](const std::string& base, std::size_t comparisons,
                const std::string& alignments, const std::string& fallbacks,
                const std::string& hits, const std::string& fps,
                std::size_t matches) {
                    counts_.row(inst.experiment, inst.family, inst.variant, t.size(),
                        p.size(), inst.sigma, a.name, base, comparisons,
                        fmt(static_cast<double>(comparisons) / n), alignments,
                        fallbacks, hits, fps, matches);
                };
            if (a.name == "kmp") {
                KMPStats s{};
                const std::size_t k{ kmp_search_counted(t, p, s).size() };
                emit("", s.comparisons, "", std::to_string(s.fallbacks), "", "", k);
            }
            else if (a.name == "boyer_moore") {
                BMStats s{};
                const std::size_t k{ boyer_moore_search_counted(t, p, s).size() };
                emit("", s.comparisons, std::to_string(s.alignments), "", "", "", k);
            }
            else if (a.name == "karp_rabin" || a.name == "karp_rabin_fixed31") {
                const unsigned long long base{ a.name == "karp_rabin"
                    ? rabin_karp_random_base(kRKModulus) : kRKFixedBase };
                RKStats s{};
                const std::size_t k{
                    rabin_karp_search_counted(t, p, base, kRKModulus, s).size() };
                emit(std::to_string(base), s.comparisons, "", "",
                    std::to_string(s.hashHits), std::to_string(s.falsePositives), k);
            }
            // std_find has no counters
        }

        const BenchConfig& cfg_;
        Csv runtime_;
        Csv counts_;
        std::mt19937_64 orderRng_;
        bool allVerified_{ true };
    };

    // ---- experiment definitions ----

    const std::vector<std::string> kAll{
        "kmp", "boyer_moore", "karp_rabin", "karp_rabin_fixed31", "std_find" };

    std::vector<std::size_t> powersOfTwo(int lo, int hi) {
        std::vector<std::size_t> v{};
        for (int e = lo; e <= hi; ++e) {
            v.push_back(std::size_t{ 1 } << e);
        }
        return v;
    }

    void progress(const Instance& inst) {
        std::cerr << "  " << inst.experiment << " " << inst.variant
            << " n=" << inst.input.text.size() << " m=" << inst.input.pattern.size()
            << " sigma=" << inst.sigma << '\n';
    }

    // E1: text length. tests the linear-in-n claim for all of them and the
    // constant factors: per-character work of each method.
    void scaleN(Runner& r, const BenchConfig& cfg, std::uint64_t& seed) {
        for (std::size_t sigma : { 4, 26 }) {
            for (std::size_t n : powersOfTwo(14, cfg.quick ? 20 : 24)) {
                Instance inst{ "scale_n", "random_embedded",
                    "sigma" + std::to_string(sigma), sigma,
                    randomEmbedded(n, 16, sigma, ++seed) };
                progress(inst);
                r.run(inst, kAll);
            }
        }
    }

    // E2: pattern length. boyer-moore's skips grow with m (sublinear time);
    // kmp and karp-rabin should be flat.
    void scaleM(Runner& r, const BenchConfig& cfg, std::uint64_t& seed) {
        const std::size_t n{ std::size_t{ 1 } << (cfg.quick ? 20 : 22) };
        for (std::size_t sigma : { 4, 26 }) {
            for (std::size_t m : powersOfTwo(1, 10)) {
                Instance inst{ "scale_m", "random_embedded",
                    "sigma" + std::to_string(sigma), sigma,
                    randomEmbedded(n, m, sigma, ++seed) };
                progress(inst);
                r.run(inst, kAll);
            }
        }
    }

    // E3: alphabet size. small alphabets make bad-character shifts short and
    // partial matches long; large alphabets do the opposite.
    void alphabet(Runner& r, const BenchConfig& cfg, std::uint64_t& seed) {
        const std::size_t n{ std::size_t{ 1 } << (cfg.quick ? 20 : 22) };
        for (std::size_t sigma : { 2, 4, 8, 16, 32, 64, 128, 256 }) {
            Instance inst{ "alphabet", "random_embedded", "m32", sigma,
                randomEmbedded(n, 32, sigma, ++seed) };
            progress(inst);
            r.run(inst, kAll);
        }
    }

    // E4: highly repetitive inputs, where the worst cases live. a^n / a^m makes
    // every alignment a match: boyer-moore (no galil rule) and karp-rabin
    // verify each in full, Theta(n m); kmp stays Theta(n).
    void repetitive(Runner& r, const BenchConfig& cfg, std::uint64_t&) {
        const std::size_t n{ std::size_t{ 1 } << (cfg.quick ? 16 : 18) };
        for (std::size_t m : powersOfTwo(2, cfg.quick ? 8 : 10)) {
            for (int v = 0; v < 3; ++v) {
                TestInput in{ v == 0 ? allSame(n, m)
                              : v == 1 ? allSameMismatchLast(n, m)
                                       : allSameMismatchFirst(n, m) };
                Instance inst{ "repetitive", "constructed", in.name, 0, in };
                progress(inst);
                r.run(inst, kAll);
            }
        }
    }

    // E5: adversarial input for fixed-base karp-rabin. the same pattern is
    // harmless to a random base.
    void rkAdversarial(Runner& r, const BenchConfig& cfg, std::uint64_t&) {
        const std::size_t n{ std::size_t{ 1 } << (cfg.quick ? 16 : 18) };
        for (std::size_t m : powersOfTwo(3, cfg.quick ? 8 : 10)) {
            Instance inst{ "rk_adversarial", "constructed", "base31", 0,
                rabinKarpAdversarial(n, m, kRKFixedBase, kRKModulus) };
            progress(inst);
            r.run(inst, kAll);
        }
    }

    // ---- E6: distribution of karp-rabin over its own random choices ----
    //
    // the experiments above fix the input and repeat; here the input is fixed
    // and the algorithm's coin flips (the base) vary. each trial draws a new
    // base, times one call, and records hash hits and false positives from a
    // counted run with the same base. kmp and boyer-moore are run the same
    // number of times on the same input, so their spread is pure measurement
    // noise and gives a baseline for the randomised spread.

    // fraction of bases in [2, p-1] for which pattern and window collide, by
    // trying all of them (only feasible for small p)
    double exactCollisionRate(const std::string& a, const std::string& b,
        unsigned long long p) {
        std::size_t bad{ 0 };
        for (unsigned long long base = 2; base < p; ++base) {
            if (polynomialHash(a, base, p) == polynomialHash(b, base, p)) {
                ++bad;
            }
        }
        return static_cast<double>(bad) / static_cast<double>(p - 2);
    }

    void rkTail(const BenchConfig& cfg, std::uint64_t& seed) {
        Csv out{ cfg.outDir + "/rk_tail.csv",
                 "scenario,modulus,n,m,sigma,algorithm,trial,base,ns,hash_hits,"
                 "false_positives,true_matches,reported,windows,bound_per_window,"
                 "exact_rate" };
        std::mt19937_64 orderRng{ cfg.seed ^ 0x5A5A5A5AULL };
        const std::size_t trials{ cfg.quick ? std::min<std::size_t>(cfg.tailTrials, 200)
                                            : cfg.tailTrials };

        struct Scenario {
            std::string name;
            unsigned long long modulus;
            std::size_t sigma;
            TestInput input;
            double exactRate;  // NaN if not computed
        };
        const double nan{ std::numeric_limits<double>::quiet_NaN() };
        std::vector<Scenario> scenarios{};

        // (a) ordinary random text, full modulus: the normal operating regime
        scenarios.push_back({ "random_full", kRKModulus, 4,
            randomEmbedded(std::size_t{ 1 } << (cfg.quick ? 18 : 20), 16, 4, ++seed),
            nan });
        // (b) random text, small moduli: false positives become frequent enough
        // to measure, and can be set against the (m-1)/(p-2) bound
        for (unsigned long long p : { 101ULL, 1009ULL, 10007ULL }) {
            scenarios.push_back({ "random_small_p", p, 26,
                randomEmbedded(std::size_t{ 1 } << 16, 16, 26, ++seed), nan });
        }
        // (c) the adversarial a^n input, built to collide for base 31 under each
        // modulus. with a random base every window collides or none does, so the
        // running time is all-or-nothing: the heavy tail of the random choice.
        for (unsigned long long p : { 101ULL, 1009ULL, kRKModulus }) {
            const std::size_t n{ std::size_t{ 1 } << 16 };
            const std::size_t m{ 256 };
            TestInput in{ rabinKarpAdversarial(n, m, kRKFixedBase, p) };
            const double rate{ p == kRKModulus ? nan
                : exactCollisionRate(in.pattern, std::string(m, 'a'), p) };
            scenarios.push_back({ "adversarial", p, 0, in, rate });
        }

        for (const Scenario& sc : scenarios) {
            const std::string& t{ sc.input.text };
            const std::string& pat{ sc.input.pattern };
            const std::size_t n{ t.size() };
            const std::size_t m{ pat.size() };
            const std::size_t windows{ n - m + 1 };
            const double bound{ static_cast<double>(m - 1) /
                                static_cast<double>(sc.modulus - 2) };
            const std::size_t truth{ stdFindSearch(t, pat).size() };
            std::cerr << "  rk_tail " << sc.name << " p=" << sc.modulus << " n=" << n
                << " m=" << m << " trials=" << trials << '\n';

            // warm-up
            consume(kmp_search(t, pat));
            consume(boyer_moore_search(t, pat));

            std::vector<int> order{ 0, 1, 2 };
            for (std::size_t trial = 0; trial < trials; ++trial) {
                std::shuffle(order.begin(), order.end(), orderRng);
                for (int which : order) {
                    std::string name{};
                    unsigned long long base{ 0 };
                    RKStats s{};
                    std::size_t reported{ 0 };
                    double ns{ 0 };
                    if (which == 0) {
                        // the original karp-rabin keeps its counters in the search
                        // itself (they only change on a hash hit), so the timed
                        // run also gives the hash hits and false positives
                        name = "karp_rabin";
                        base = rabin_karp_random_base(sc.modulus);
                        const auto start{ Clock::now() };
                        const Matches got{
                            rabin_karp_search_counted(t, pat, base, sc.modulus, s) };
                        ns = secondsSince(start) * 1e9;
                        consume(got);
                        reported = got.size();
                    }
                    else {
                        name = which == 1 ? "kmp" : "boyer_moore";
                        const auto start{ Clock::now() };
                        const Matches got{ which == 1 ? kmp_search(t, pat)
                                                      : boyer_moore_search(t, pat) };
                        ns = secondsSince(start) * 1e9;
                        consume(got);
                        reported = got.size();
                    }
                    out.row(sc.name, sc.modulus, n, m, sc.sigma, name, trial,
                        which == 0 ? std::to_string(base) : std::string{}, fmt(ns),
                        which == 0 ? std::to_string(s.hashHits) : std::string{},
                        which == 0 ? std::to_string(s.falsePositives) : std::string{},
                        truth, reported, windows, fmt(bound),
                        std::isnan(sc.exactRate) ? std::string{} : fmt(sc.exactRate));
                }
            }
            out.flush();
        }
    }

    std::string cpuModel() {
        std::ifstream in{ "/proc/cpuinfo" };
        std::string line{};
        while (std::getline(in, line)) {
            if (line.rfind("model name", 0) == 0) {
                const auto colon{ line.find(':') };
                if (colon != std::string::npos) {
                    return line.substr(colon + 2);
                }
            }
        }
        return "unknown (not Linux: record the CPU model by hand)";
    }

    void writeEnvironment(const BenchConfig& cfg) {
        std::ofstream env{ cfg.outDir + "/environment.txt" };
        const std::time_t now{ std::time(nullptr) };
        char when[64]{};
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        env << "date: " << when << '\n'
            << "cpu: " << cpuModel() << '\n'
            << "hardware threads: " << std::thread::hardware_concurrency() << '\n'
            << "compiler: " << BENCH_COMPILER << '\n'
            << "build type: " << BENCH_BUILD_TYPE << '\n'
            << "flags: " << BENCH_FLAGS << '\n'
            << "c++ standard: " << __cplusplus << '\n'
            << "timer: std::chrono::steady_clock, period "
            << static_cast<double>(Clock::period::num) / Clock::period::den * 1e9
            << " ns\n"
            << "seed: " << cfg.seed << '\n'
            << "reps: " << cfg.reps << '\n'
            << "min sample ms: " << cfg.minSampleMs << '\n'
            << "tail trials: " << cfg.tailTrials << '\n'
            << "quick: " << (cfg.quick ? "yes" : "no") << '\n'
            << "karp-rabin modulus: " << kRKModulus << '\n'
            << "karp-rabin fixed base: " << kRKFixedBase << '\n';
    }

}  // namespace

const std::vector<std::string>& experimentNames() {
    static const std::vector<std::string> names{
        "scale_n", "scale_m", "alphabet", "repetitive", "rk_adversarial", "rk_tail" };
    return names;
}

int runBenchmarks(const BenchConfig& cfg) {
    rabin_karp_seed(cfg.seed);
    writeEnvironment(cfg);
    Runner runner{ cfg };
    std::uint64_t seed{ cfg.seed };

    auto selected = [&](const std::string& name) {
        return cfg.only.empty() ||
            std::find(cfg.only.begin(), cfg.only.end(), name) != cfg.only.end();
        };

    const auto start{ Clock::now() };
    // each experiment gets its own seed range, so running a subset with
    // --only produces the same instances as a full run
    if (selected("scale_n")) { seed = cfg.seed + 1000; scaleN(runner, cfg, seed); }
    if (selected("scale_m")) { seed = cfg.seed + 2000; scaleM(runner, cfg, seed); }
    if (selected("alphabet")) { seed = cfg.seed + 3000; alphabet(runner, cfg, seed); }
    if (selected("repetitive")) { seed = cfg.seed + 4000; repetitive(runner, cfg, seed); }
    if (selected("rk_adversarial")) { seed = cfg.seed + 5000; rkAdversarial(runner, cfg, seed); }
    if (selected("rk_tail")) { seed = cfg.seed + 6000; rkTail(cfg, seed); }

    std::cerr << "done in " << std::fixed << std::setprecision(1)
        << secondsSince(start) << " s; output verified: "
        << (runner.allVerified() ? "yes" : "NO") << '\n';
    return runner.allVerified() ? 0 : 2;
}
