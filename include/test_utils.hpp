// test_utils.hpp - helpers shared by the test executables
//
// header-only so each test can be built with a one-line compiler command.

#pragma once

#include <cstddef>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using Matches = std::vector<std::size_t>;

using SearchFunction =
std::function<
    std::vector<std::size_t>(
        const std::string&,
        const std::string&)>;

// ---- reference answers ----

// try every alignment. obviously correct, used as the ground truth.
// same agreed rule as the real algorithms: an empty pattern, or one longer
// than the text, gives no matches.
inline Matches naiveSearch(const std::string& text, const std::string& pattern) {
    Matches out{};
    const std::size_t n{ text.size() };
    const std::size_t m{ pattern.size() };
    if (m == 0 || m > n) {
        return out;
    }
    for (std::size_t s = 0; s + m <= n; ++s) {
        std::size_t j{ 0 };
        while (j < m && text[s + j] == pattern[j]) {
            ++j;
        }
        if (j == m) {
            out.push_back(s);
        }
    }
    return out;
}

// every match via std::string::find, restarting one past each hit so
// overlapping matches are found
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

// ---- input builders ----

inline std::string randomString(int length, const std::string& letters,
    std::mt19937& mt) {
    std::string s(static_cast<std::size_t>(length > 0 ? length : 0), ' ');
    for (char& c : s) {
        c = letters[mt() % letters.size()];
    }
    return s;
}

// every string over letters of length 0..maxLength
inline std::vector<std::string> allStrings(const std::string& letters,
    int maxLength) {
    std::vector<std::string> out{ "" };
    std::size_t begin{ 0 };
    for (int len = 1; len <= maxLength; ++len) {
        const std::size_t end{ out.size() };
        for (std::size_t i = begin; i < end; ++i) {
            for (char c : letters) {
                out.push_back(out[i] + c);
            }
        }
        begin = end;
    }
    return out;
}

inline std::string repeated(const std::string& unit, std::size_t times) {
    std::string s{};
    s.reserve(unit.size() * times);
    for (std::size_t i = 0; i < times; ++i) {
        s += unit;
    }
    return s;
}

// ---- reporting ----

// counts PASS / FAIL lines; each test file prints a SCORE line at the end
struct TestReport {
    int passed{ 0 };
    int total{ 0 };

    void check(bool ok, const std::string& name) {
        ++total;
        if (ok) {
            ++passed;
            std::cout << "PASS " << name << '\n';
        }
        else {
            std::cout << "FAIL " << name << '\n';
        }
    }

    int finish() const {
        std::cout << "SCORE: " << passed << " / " << total << '\n';
        return passed == total ? 0 : 1;
    }
};
