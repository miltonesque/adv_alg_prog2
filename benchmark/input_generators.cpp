// input_generators.cpp - the input families used by the benchmark

#include "input_generators.hpp"

#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace InputGenerators {

    std::string alphabetOfSize(std::size_t sigma) {
        if (sigma == 0 || sigma > 256) {
            throw std::invalid_argument("sigma must be in [1, 256]");
        }
        std::string order{
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789" };
        // then every other byte value, in increasing order
        bool used[256]{};
        for (unsigned char c : order) {
            used[c] = true;
        }
        for (int b = 0; b < 256; ++b) {
            if (!used[b]) {
                order.push_back(static_cast<char>(b));
            }
        }
        return order.substr(0, sigma);
    }

    std::string randomText(std::size_t length, std::size_t sigma,
        std::uint64_t seed) {
        const std::string alphabet{ alphabetOfSize(sigma) };
        std::mt19937_64 rng{ seed };
        std::uniform_int_distribution<std::size_t> pick{ 0, alphabet.size() - 1 };
        std::string out(length, ' ');
        for (char& c : out) {
            c = alphabet[pick(rng)];
        }
        return out;
    }

    TestInput randomEmbedded(std::size_t textLength, std::size_t patternLength,
        std::size_t sigma, std::uint64_t seed) {
        if (patternLength == 0 || patternLength > textLength) {
            throw std::invalid_argument(
                "pattern must be non-empty and fit inside text");
        }
        TestInput input;
        input.name = "random_embedded";
        input.text = randomText(textLength, sigma, seed);
        // a different stream from the text's, for the cut position
        std::mt19937_64 rng{ seed ^ 0x9E3779B97F4A7C15ULL };
        std::uniform_int_distribution<std::size_t> where{
            0, textLength - patternLength };
        input.pattern = input.text.substr(where(rng), patternLength);
        return input;
    }

    TestInput allSame(std::size_t textLength, std::size_t patternLength) {
        TestInput input;
        input.name = "all_match";
        input.text = std::string(textLength, 'a');
        input.pattern = std::string(patternLength, 'a');
        return input;
    }

    TestInput allSameMismatchLast(std::size_t textLength,
        std::size_t patternLength) {
        if (patternLength == 0) {
            throw std::invalid_argument("pattern must be non-empty");
        }
        TestInput input;
        input.name = "mismatch_last";
        input.text = std::string(textLength, 'a');
        input.pattern = std::string(patternLength, 'a');
        input.pattern.back() = 'b';
        return input;
    }

    TestInput allSameMismatchFirst(std::size_t textLength,
        std::size_t patternLength) {
        if (patternLength == 0) {
            throw std::invalid_argument("pattern must be non-empty");
        }
        TestInput input;
        input.name = "mismatch_first";
        input.text = std::string(textLength, 'a');
        input.pattern = std::string(patternLength, 'a');
        input.pattern.front() = 'b';
        return input;
    }

    unsigned long long polynomialHash(const std::string& value,
        unsigned long long base, unsigned long long modulus) {
        base %= modulus;
        unsigned long long hash{ 0 };
        for (unsigned char ch : value) {
            hash = (hash * base + ch) % modulus;
        }
        return hash;
    }

    TestInput rabinKarpAdversarial(std::size_t textLength,
        std::size_t patternLength, unsigned long long base,
        unsigned long long modulus) {
        constexpr std::size_t kHalf{ 4 };
        constexpr std::size_t kTail{ 2 * kHalf };
        if (patternLength < kTail || patternLength > textLength) {
            throw std::invalid_argument(
                "adversarial pattern needs 8 <= m <= n");
        }

        // all 26^4 lowercase strings of length 4, enumerated in a fixed order
        std::size_t count{ 1 };
        for (std::size_t i = 0; i < kHalf; ++i) {
            count *= 26;
        }
        auto halfString = [kHalf](std::size_t index) {
            std::string s(kHalf, 'a');
            for (std::size_t i = kHalf; i-- > 0;) {
                s[i] = static_cast<char>('a' + index % 26);
                index /= 26;
            }
            return s;
            };

        // H(L R) = H(L) * base^4 + H(R). we need it to equal H(a^8).
        const unsigned long long target{
            polynomialHash(std::string(kTail, 'a'), base, modulus) };
        unsigned long long shift{ 1 };
        for (std::size_t i = 0; i < kHalf; ++i) {
            shift = shift * (base % modulus) % modulus;
        }

        std::unordered_map<unsigned long long, std::size_t> rightByHash{};
        rightByHash.reserve(count * 2);
        for (std::size_t r = 0; r < count; ++r) {
            rightByHash.emplace(polynomialHash(halfString(r), base, modulus), r);
        }

        std::string tail{};
        for (std::size_t l = 0; l < count && tail.empty(); ++l) {
            const std::string left{ halfString(l) };
            const unsigned long long hl{
                polynomialHash(left, base, modulus) * shift % modulus };
            const unsigned long long need{ (target + modulus - hl) % modulus };
            const auto it{ rightByHash.find(need) };
            if (it != rightByHash.end()) {
                const std::string candidate{ left + halfString(it->second) };
                if (candidate != std::string(kTail, 'a')) {
                    tail = candidate;
                }
            }
        }
        if (tail.empty()) {
            throw std::runtime_error("no colliding suffix found");
        }

        TestInput input;
        input.name = "rk_adversarial";
        input.text = std::string(textLength, 'a');
        input.pattern = std::string(patternLength - kTail, 'a') + tail;
        return input;
    }

} // namespace InputGenerators
