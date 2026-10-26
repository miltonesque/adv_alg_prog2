// common.hpp - small helpers shared by the tests and the benchmark
//
// every algorithm in this repo has the same signature:
//   std::vector<std::size_t> f(const std::string& text, const std::string& pattern)
// and returns the start position of every match, in increasing order,
// overlaps included. an empty pattern, or one longer than the text, gives
// no matches. that shared signature is the only "interface" we use.

#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <cstddef>
#include <random>
#include <string>
#include <vector>

using Matches = std::vector<std::size_t>;

// tries every position, so it's slow but you can see it's right.
// the answer the tests compare against.
inline Matches naiveSearch(const std::string& text, const std::string& pattern) {
    Matches out{};
    if (pattern.empty() || pattern.size() > text.size()) {
        return out;
    }
    for (std::size_t s = 0; s + pattern.size() <= text.size(); ++s) {
        if (text.compare(s, pattern.size(), pattern) == 0) {
            out.push_back(s);
        }
    }
    return out;
}

// a second answer from the standard library. find starts again one past
// each match, so overlapping matches are counted too, the same as the
// other algorithms.
inline Matches stdFindSearch(const std::string& text, const std::string& pattern) {
    Matches out{};
    if (pattern.empty()) {
        return out;
    }
    std::size_t pos{ text.find(pattern) };
    while (pos != std::string::npos) {
        out.push_back(pos);
        pos = text.find(pattern, pos + 1);
    }
    return out;
}

// ---- input helpers for the tests ----

// random string of length n, only using the characters in letters
inline std::string randomString(int n, const std::string& letters,
    std::mt19937& mt) {
    std::uniform_int_distribution<int> pick{
        0, static_cast<int>(letters.size()) - 1 };
    std::string s(n, ' ');
    for (char& c : s) {
        c = letters[pick(mt)];
    }
    return s;
}

// keeps adding unit until the string is n long, e.g. ("ab", 5) is "ababa"
inline std::string repeated(const std::string& unit, int n) {
    std::string s{};
    while (static_cast<int>(s.size()) < n) {
        s += unit;
    }
    return s.substr(0, n);
}

// every string made from letters with length 0 up to maxLen
inline std::vector<std::string> allStrings(const std::string& letters,
    int maxLen) {
    std::vector<std::string> out{ "" };
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (static_cast<int>(out[i].size()) == maxLen) {
            continue;
        }
        for (char c : letters) {
            out.push_back(out[i] + c);
        }
    }
    return out;
}

#endif  // COMMON_HPP_
