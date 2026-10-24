// rabin_karp.cpp - karp-rabin string matching
//
// primary author: mallory li
//
// ---- overview ----
// each window of m characters in the text is turned into a number, its
// hash, and only windows with the same hash as the pattern are compared
// character by character.
//   hash:         c[0]*base^(m-1) + c[1]*base^(m-2) + ... + c[m-1], all
//                 mod modulus.
//   rolling hash: the hash of the next window comes from the current one in
//                 O(1), by taking off the first character and adding the
//                 next one.
// two different strings can have the same hash, so every hash hit is
// checked against the text. a hit that fails the check is a false positive.
// because of the check a wrong match is never reported, and the randomness
// only changes the running time.
//
// the base is picked at random for each search. the hashes of two different
// windows differ by a polynomial in base of degree below m, and mod a prime
// that has at most m - 1 roots. so for any fixed input, a window gives a
// false positive with probability at most (m - 1) / (modulus - 2), which is
// tiny for modulus = 2^31 - 1. the expected time is then O(n + m) plus m for
// each real match. the worst case is still O(nm), e.g. all-a text with an
// all-a pattern, where every window is a real match and is checked in full.
//
// with a fixed base the bound no longer holds, since text can be built to
// collide on purpose. benchmark.cpp does this for base 31.

#include <cstddef>
#include <random>
#include <string>
#include <vector>
#include "rabin_karp.hpp"

// ---- random base ----
// one generator for the whole run, seeded from the system so each run uses
// different bases. the base is drawn from 2 to modulus - 1, since 0 and 1
// would make the hash ignore the order of the characters.
static unsigned long long randomBase(unsigned long long modulus) {
  static std::mt19937_64 mt {std::random_device {}()};
  std::uniform_int_distribution<unsigned long long> baseDist {2, modulus - 1};
  return baseDist(mt);
}

// ---- search ----
std::vector<std::size_t> rabin_karp_search_counted(
    const std::string& text, const std::string& pattern,
    unsigned long long base, unsigned long long modulus, RKStats& stats) {
  std::vector<std::size_t> matches {};
  const std::size_t n {text.size()};
  const std::size_t m {pattern.size()};
  if (m == 0 || m > n) {
    return matches;
  }
  base %= modulus;

  // base^(m-1), the weight of the first character in a window. it is taken
  // off the hash when the window moves.
  unsigned long long highPow {1};
  for (std::size_t i = 1; i < m; ++i) {
    highPow = highPow * base % modulus;
  }

  // both hashes use horner's rule, e.g. "abc" gives (a*base + b)*base + c.
  // unsigned char keeps characters above 127 from becoming negative. the
  // characters are used as they are, not mapped to 1..26, so the hash
  // matches the one collidingText in benchmark.cpp is built against.
  unsigned long long patternHash {0};
  unsigned long long windowHash {0};
  for (std::size_t i = 0; i < m; ++i) {
    patternHash =
        (patternHash * base + static_cast<unsigned char>(pattern[i])) %
        modulus;
    windowHash =
        (windowHash * base + static_cast<unsigned char>(text[i])) % modulus;
  }

  // s is the position in the text where the current window starts
  for (std::size_t s = 0; s <= n - m; ++s) {
    if (windowHash == patternHash) {
      ++stats.hashHits;
      std::size_t j {0};
      while (j < m && text[s + j] == pattern[j]) {
        ++j;
      }
      if (j == m) {
        matches.push_back(s);
      } else {
        ++stats.falsePositives;
      }
    }

    // roll to the window starting at s + 1. modulus is added before
    // subtracting so the unsigned value never goes below 0.
    if (s + m < n) {
      const unsigned long long first {
          static_cast<unsigned char>(text[s]) * highPow % modulus};
      windowHash = (windowHash + modulus - first) % modulus;
      windowHash =
          (windowHash * base + static_cast<unsigned char>(text[s + m])) %
          modulus;
    }
  }
  return matches;
}

// same inputs and output as kmp_search and boyer_moore_search
std::vector<std::size_t> rabin_karp_search(const std::string& text,
                                           const std::string& pattern) {
  RKStats stats {};
  return rabin_karp_search_counted(text, pattern, randomBase(kRKModulus),
                                   kRKModulus, stats);
}
