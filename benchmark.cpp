// benchmark.cpp - times every algorithm on the same inputs and writes the
// results to results/*.csv
// 
//
// this is just a draft of the benchmark for now, the karp-rabin and kmp parts won't build until those files are done.
//
// what it does:
//   1. makes the test inputs. some are random text, some are repeated
//      text like aaaa or abab, and one is made so karp-rabin's hash gives
//      false matches. the seeds are fixed so we get the same inputs every
//      time we run it.
//   2. runs kmp, boyer-moore, karp-rabin and std::string::find on each
//      input and checks they all find the same matches.
//   3. times each search 7 times and saves all the times to a csv file in
//      the results folder.
//   4. for karp-rabin it also counts how many times the hash matched but
//      the text didn't (false positives).
//

// run in Release x64 with an empty "results" folder next to the .sln.

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "common.hpp"
#include "bmp.hpp"
#include "kmp.hpp"
#include "rabin_karp.hpp"

//// find is restarted one past each match so overlapping matches are counted,
//// the same as the other algorithms
//std::vector<std::size_t> stdFindSearch(const std::string& text,
//                                       const std::string& pattern) {
//  std::vector<std::size_t> matches {};
//  if (pattern.empty()) {
//    return matches;
//  }
//  std::size_t pos {text.find(pattern)};
//  while (pos != std::string::npos) {
//    matches.push_back(pos);
//    pos = text.find(pattern, pos + 1);
//  }
//  return matches;
//}

const std::vector<std::string> kAlgorithms {"kmp", "boyer-moore", "karp-rabin",
                                            "std-find"};

std::vector<std::size_t> runSearch(const std::string& name,
                                   const std::string& text,
                                   const std::string& pattern) {
  if (name == "kmp") {
    return kmp_search(text, pattern);
  }
  if (name == "boyer-moore") {
    return boyer_moore_search(text, pattern);
  }
  if (name == "karp-rabin") {
    return rabin_karp_search(text, pattern);
  }
  return stdFindSearch(text, pattern);
}

// all inputs use fixed seeds, so every run makes the same inputs.
// up to 26 letters starts at 'a'. 256 uses every byte value.
std::string randomText(int n, int sigma, unsigned seed) {
  std::mt19937 mt {seed};
  std::uniform_int_distribution<int> letterDist {0, sigma - 1};
  const int first {sigma <= 26 ? 'a' : 0};
  std::string text(n, ' ');
  for (int i = 0; i < n; ++i) {
    text[i] = static_cast<char>(first + letterDist(mt));
  }
  return text;
}

// taken from the text so the pattern always occurs at least once
std::string patternFrom(const std::string& text, int m, unsigned seed) {
  std::mt19937 mt {seed};
  std::uniform_int_distribution<int> startDist {
      0, static_cast<int>(text.size()) - m};
  return text.substr(startDist(mt), m);
}

// every block of m characters has the same karp-rabin hash as the pattern
// for the given base, but is a different string.
// hash = c[0]*base^(m-1) + ... + c[m-1]. adding 1 to c[i] and subtracting
// base from c[i+1] leaves the sum unchanged, for any modulus.
// the pattern must be lowercase letters so every changed byte stays valid.
std::string collidingText(const std::string& pattern, int base, int n) {
  std::mt19937 mt {503};
  std::uniform_int_distribution<int> posDist {
      0, static_cast<int>(pattern.size()) - 2};
  std::string text {};
  while (static_cast<int>(text.size()) < n) {
    std::string copy {pattern};
    const int i {posDist(mt)};
    copy[i] = static_cast<char>(copy[i] + 1);
    copy[i + 1] = static_cast<char>(copy[i + 1] - base);
    text += copy;
  }
  return text.substr(0, n);
}

const int kRepeats {7};

// only the search is inside the timer. a search on a short text is too fast
// to time once, so it is repeated until about 2^20 characters have been
// searched, and the time per search is returned.
long long timeSearch(const std::string& name, const std::string& text,
                     const std::string& pattern) {
  const int calls {static_cast<int>(1048576 / text.size()) + 1};
  std::size_t found {0};
  const auto start = std::chrono::steady_clock::now();
  for (int c = 0; c < calls; ++c) {
    found += runSearch(name, text, pattern).size();
  }
  const auto end = std::chrono::steady_clock::now();
  if (found > text.size() * calls) {
    // never true. using the result stops the compiler removing the search
    std::cout << "impossible\n";
  }
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
             .count() /
         calls;
}

// every algorithm is first checked against std-find. this run also warms
// up the cache and is not timed. each timed round then runs all four once,
// so a slowdown on the machine affects them all equally.
void runAll(std::ofstream& out, const std::string& input,
            const std::string& text, const std::string& pattern) {
  const std::vector<std::size_t> expected {stdFindSearch(text, pattern)};
  for (const std::string& name : kAlgorithms) {
    if (runSearch(name, text, pattern) != expected) {
      std::cerr << "WRONG ANSWER: " << name << " on " << input << ", m = "
                << pattern.size() << ". not timing this input.\n";
      return;
    }
  }
  for (int rep = 0; rep < kRepeats; ++rep) {
    for (const std::string& name : kAlgorithms) {
      out << name << ',' << input << ',' << text.size() << ','
          << pattern.size() << ',' << rep << ','
          << timeSearch(name, text, pattern) << ',' << expected.size()
          << '\n';
    }
  }
}

const std::string kHeader {"algorithm,input,n,m,rep,ns,matches"};

void lengthExperiment() {
  std::ofstream out {"results/length.csv"};
  out << kHeader << '\n';
  for (int n : {256, 1024, 4096, 16384, 65536, 262144, 1048576, 4194304}) {
    const std::string text {randomText(n, 26, 100)};
    runAll(out, "random-26", text, patternFrom(text, 16, 200));
  }
}

void patternExperiment() {
  std::ofstream out {"results/pattern.csv"};
  out << kHeader << '\n';
  for (int sigma : {2, 4, 26, 256}) {
    const std::string text {randomText(1048576, sigma, 300 + sigma)};
    for (int m : {2, 4, 8, 16, 32, 64, 128, 256}) {
      runAll(out, "random-" + std::to_string(sigma), text,
             patternFrom(text, m, 400 + m));
    }
  }
}

void worstExperiment() {
  std::ofstream out {"results/worst.csv"};
  out << kHeader << '\n';
  const std::string allA(1048576, 'a');
  const std::string periodic {repeated("ab", 1048576)};
  for (int m : {4, 16, 64, 256}) {
    runAll(out, "all-a", allA, std::string(m, 'a'));
    runAll(out, "periodic", periodic, repeated("ab", m));
    runAll(out, "a-then-b", allA, std::string(m - 1, 'a') + "b");
  }
}

// fixedBase = 0 means a new random base for each trial. each trial is
// checked and warmed up with one untimed run first.
void collisionTrials(std::ofstream& out, const std::string& input,
                     const std::string& text, const std::string& pattern,
                     unsigned long long modulus,
                     unsigned long long fixedBase) {
  const std::vector<std::size_t> expected {stdFindSearch(text, pattern)};
  std::mt19937 mt {502};
  std::uniform_int_distribution<unsigned long long> baseDist {2, modulus - 1};
  for (int trial = 0; trial < 10; ++trial) {
    const unsigned long long base {fixedBase != 0 ? fixedBase : baseDist(mt)};
    RKStats warmUp {};
    if (rabin_karp_search_counted(text, pattern, base, modulus, warmUp) !=
        expected) {
      std::cerr << "WRONG ANSWER: karp-rabin on " << input << '\n';
      return;
    }
    RKStats stats {};
    const auto start = std::chrono::steady_clock::now();
    rabin_karp_search_counted(text, pattern, base, modulus, stats);
    const auto end = std::chrono::steady_clock::now();
    out << input << ',' << modulus << ',' << base << ','
        << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
               .count()
        << ',' << stats.hashHits << ',' << stats.falsePositives << '\n';
  }
}

// same text and pattern as length.csv at n = 1048576, so kmp's times there
// can be compared with these. 2147483647 = 2^31 - 1 is the default modulus,
// and 11 is smaller than m = 16.
void collisionExperiment() {
  std::ofstream out {"results/collisions.csv"};
  out << "input,modulus,base,ns,hash_hits,false_positives\n";
  const std::string text {randomText(1048576, 26, 100)};
  const std::string pattern {patternFrom(text, 16, 200)};
  for (unsigned long long modulus : {11, 31, 1021, 65521, 2147483647}) {
    collisionTrials(out, "random-26", text, pattern, modulus, 0);
  }
  const std::string built {collidingText(pattern, 31, 1048576)};
  collisionTrials(out, "built-for-31", built, pattern, 2147483647, 31);
  collisionTrials(out, "built-for-31", built, pattern, 2147483647, 0);
}

// ---- operation counts ----
// the counted versions are run once on the same inputs as the timing
// experiments (same seeds), and every counter is written as its own row:
//   kmp:         comparisons, fallbacks
//   boyer-moore: comparisons, alignments
//   karp-rabin:  comparisons, hash_hits, false_positives
// karp-rabin uses a fixed seeded base here so the counts are reproducible.
// timing and counting are kept apart so the counters never slow the timed
// runs.
void countAll(std::ofstream& out, const std::string& input,
              const std::string& text, const std::string& pattern) {
  const std::vector<std::size_t> expected {stdFindSearch(text, pattern)};
  auto row = [&](const std::string& algo, const std::string& counter,
                 std::size_t value) {
    out << algo << ',' << input << ',' << text.size() << ',' << pattern.size()
        << ',' << expected.size() << ',' << counter << ',' << value << '\n';
  };

  KMPStats kmp {};
  BMStats bm {};
  RKStats rk {};
  static std::mt19937_64 baseGen {600};
  std::uniform_int_distribution<unsigned long long> baseDist {2, kRKModulus - 1};
  if (kmp_search_counted(text, pattern, kmp) != expected ||
      boyer_moore_search_counted(text, pattern, bm) != expected ||
      rabin_karp_search_counted(text, pattern, baseDist(baseGen), kRKModulus,
                                rk) != expected) {
    std::cerr << "WRONG ANSWER from a counted search on " << input
              << ", m = " << pattern.size() << '\n';
    return;
  }
  row("kmp", "comparisons", kmp.comparisons);
  row("kmp", "fallbacks", kmp.fallbacks);
  row("boyer-moore", "comparisons", bm.comparisons);
  row("boyer-moore", "alignments", bm.alignments);
  row("karp-rabin", "comparisons", rk.comparisons);
  row("karp-rabin", "hash_hits", rk.hashHits);
  row("karp-rabin", "false_positives", rk.falsePositives);
}

void countExperiment() {
  std::ofstream out {"results/counts.csv"};
  out << "algorithm,input,n,m,matches,counter,value\n";
  // same texts and patterns (same seeds) as patternExperiment
  for (int sigma : {2, 4, 26, 256}) {
    const std::string text {randomText(1048576, sigma, 300 + sigma)};
    for (int m : {2, 4, 8, 16, 32, 64, 128, 256}) {
      countAll(out, "random-" + std::to_string(sigma), text,
               patternFrom(text, m, 400 + m));
    }
  }
  // same inputs as worstExperiment
  const std::string allA(1048576, 'a');
  const std::string periodic {repeated("ab", 1048576)};
  for (int m : {4, 16, 64, 256}) {
    countAll(out, "all-a", allA, std::string(m, 'a'));
    countAll(out, "periodic", periodic, repeated("ab", m));
    countAll(out, "a-then-b", allA, std::string(m - 1, 'a') + "b");
  }
}

int main() {
#ifndef NDEBUG
  std::cout << "warning: debug build. times are only valid in release.\n";
#endif
  std::ofstream check {"results/length.csv"};
  if (!check) {
    std::cerr << "make an empty folder called results first\n";
    return 1;
  }
  check.close();
  lengthExperiment();
  std::cout << "length done\n";
  patternExperiment();
  std::cout << "pattern done\n";
  worstExperiment();
  std::cout << "worst done\n";
  collisionExperiment();
  std::cout << "collisions done\n";
  countExperiment();
  std::cout << "counts done\n";
  return 0;
}
