// input_generators.hpp - the input families used by the benchmark
//
// every generator takes an explicit seed and owns its own generator, so an
// instance depends only on its parameters and seed, never on which other
// instances were generated first. the same instance is given to every
// algorithm.

#ifndef INPUT_GENERATORS_HPP
#define INPUT_GENERATORS_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace InputGenerators {

    struct TestInput
    {
        std::string name;

        std::string text;

        std::string pattern;
    };

    // the first sigma symbols of a fixed ordering: a-z, A-Z, 0-9, then the
    // remaining byte values. sigma must be in [1, 256].
    std::string alphabetOfSize(std::size_t sigma);

    // uniform i.i.d. characters from alphabetOfSize(sigma)
    std::string randomText(std::size_t length, std::size_t sigma,
        std::uint64_t seed);

    // random text; the pattern is cut out of the text at a random position,
    // so there is at least one match (plus any that occur by chance)
    TestInput randomEmbedded(std::size_t textLength, std::size_t patternLength,
        std::size_t sigma, std::uint64_t seed);

    // text a^n, pattern a^m: every alignment is a match (n - m + 1 matches)
    TestInput allSame(std::size_t textLength, std::size_t patternLength);

    // text a^n, pattern a^(m-1) b: no matches; the mismatch is at the
    // pattern's last character
    TestInput allSameMismatchLast(std::size_t textLength,
        std::size_t patternLength);

    // text a^n, pattern b a^(m-1): no matches; the mismatch is at the
    // pattern's first character, after m - 1 equal characters
    TestInput allSameMismatchFirst(std::size_t textLength,
        std::size_t patternLength);

    // hash used by karp-rabin: horner's rule over unsigned char values,
    // mod modulus. must match src/rabin_karp.cpp exactly.
    unsigned long long polynomialHash(const std::string& value,
        unsigned long long base, unsigned long long modulus);

    // adversarial input for karp-rabin with a FIXED base:
    // text a^n, pattern a^(m-8) S where S != a^8 has the same hash as a^8
    // under (base, modulus). every one of the n - m + 1 windows then has the
    // pattern's hash, and each is verified over at least m - 8 characters
    // before failing, so fixed-base karp-rabin does Theta(n m) work.
    // S is found by meet-in-the-middle over 4 + 4 lowercase letters.
    // patternLength must be at least 8.
    TestInput rabinKarpAdversarial(std::size_t textLength,
        std::size_t patternLength, unsigned long long base,
        unsigned long long modulus);

} // namespace InputGenerators

#endif
