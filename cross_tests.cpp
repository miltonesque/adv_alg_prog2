// cross_tests.cpp - every algorithm against every other on large inputs
//
// the per-algorithm test files check each algorithm against naive on small
// inputs. naive is too slow to be the reference on big inputs, so here the
// algorithms are checked against each other (and std::string::find), on the
// same kinds of input the benchmark uses, at full size.
//
// each test prints PASS or FAIL, and the last line is the score

#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "bmp.hpp"
#include "common.hpp"
#include "kmp.hpp"
#include "rabin_karp.hpp"

static int passed{ 0 };
static int total{ 0 };

// all four answers must be identical
static void allAgree(const std::string& name, const std::string& text,
    const std::string& pattern) {
    const Matches expected{ stdFindSearch(text, pattern) };
    const bool ok{ kmp_search(text, pattern) == expected &&
                   boyer_moore_search(text, pattern) == expected &&
                   rabin_karp_search(text, pattern) == expected };
    ++total;
    if (ok) {
        ++passed;
    }
    std::cout << (ok ? "PASS " : "FAIL ") << name << ", m = " << pattern.size()
        << " (" << expected.size() << " matches)\n";
}

int main() {
    const int n{ 1 << 20 };
    std::mt19937 mt{ 8 };

    // random text over 2, 4 and 26 letters, pattern cut from the text
    for (const std::string letters : {"ab", "acgt", "abcdefghijklmnopqrstuvwxyz"}) {
        const std::string text{ randomString(n, letters, mt) };
        for (int m : {1, 3, 16, 100, 1000}) {
            const std::string pattern{ text.substr(mt() % (text.size() - m), m) };
            allAgree("random, " + std::to_string(letters.size()) + " letters", text,
                pattern);
        }
    }

    // the worst-case families from benchmark.cpp
    const std::string allA(n, 'a');
    const std::string periodic{ repeated("ab", n) };
    for (int m : {4, 64, 1000}) {
        allAgree("all-a", allA, std::string(m, 'a'));
        allAgree("a-then-b", allA, std::string(m - 1, 'a') + "b");
        allAgree("periodic", periodic, repeated("ab", m));
    }

    // all 256 byte values
    std::string bytes(n, ' ');
    for (char& c : bytes) {
        c = static_cast<char>(mt() % 256);
    }
    allAgree("random bytes", bytes, bytes.substr(12345, 8));

    std::cout << "SCORE: " << passed << " / " << total << '\n';
    return passed == total ? 0 : 1;
}
