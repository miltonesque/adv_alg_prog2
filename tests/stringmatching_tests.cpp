// asserts are the checks here, so keep them on even in release builds
#undef NDEBUG
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "kmp.hpp"
#include "rabin_karp.hpp"
#include "boyer_moore.hpp"

template<typename SearchFn>
void run_common_tests(SearchFn search)
{
    //
    // single match
    //
    {
        auto result =
            search("hello world", "world");

        assert(result.size() == 1);
        assert(result[0] == 6);
    }

    //
    // no match
    //
    {
        auto result =
            search("abcdef", "xyz");

        assert(result.empty());
    }

    //
    // beginning
    //
    {
        auto result =
            search("abcdef", "abc");

        assert(result.size() == 1);
        assert(result[0] == 0);
    }

    //
    // end
    //
    {
        auto result =
            search("abcdef", "def");

        assert(result.size() == 1);
        assert(result[0] == 3);
    }

    //
    // banana
    //
    {
        auto result =
            search("banana", "ana");

        std::vector<std::size_t> expected
        {
            1, 3
        };

        assert(result == expected);
    }

    //
    // overlapping matches
    //
    {
        auto result =
            search("aaaaa", "aa");

        std::vector<std::size_t> expected
        {
            0,1,2,3
        };

        assert(result == expected);
    }

    //
    // repeated characters
    //
    {
        auto result =
            search("aaaaaaaaaa", "aaa");

        std::vector<std::size_t> expected
        {
            0,1,2,3,4,5,6,7
        };

        assert(result == expected);
    }

    //
    // pattern longer than text
    //
    {
        auto result =
            search("abc", "abcdef");

        assert(result.empty());
    }

    //
    // empty pattern
    //
    {
        auto result =
            search("abcdef", "");

        assert(result.empty());
    }
}

std::string randomString(
    std::size_t length,
    std::mt19937& rng)
{
    static constexpr char alphabet[]
    {
        'A','B','C','D'
    };

    std::uniform_int_distribution<int>
        dist(0, 3);

    std::string result;

    result.reserve(length);

    for (std::size_t i = 0; i < length; ++i)
    {
        result.push_back(
            alphabet[dist(rng)]);
    }

    return result;
}

std::vector<std::size_t> reference_search(
    const std::string& text,
    const std::string& pattern)
{
    std::vector<std::size_t> matches;

    if (pattern.empty())
    {
        return matches;
    }

    std::size_t pos =
        text.find(pattern);

    while (pos != std::string::npos)
    {
        matches.push_back(pos);

        pos =
            text.find(
                pattern,
                pos + 1);
    }

    return matches;
}

void run_random_tests()
{
    std::mt19937 rng(42);

    constexpr int NUM_TESTS = 10000;

    for (int t = 0;
        t < NUM_TESTS;
        ++t)
    {
        std::uniform_int_distribution<int>
            textLenDist(1, 200);

        std::uniform_int_distribution<int>
            patternLenDist(1, 20);

        const int textLen =
            textLenDist(rng);

        const int patternLen =
            patternLenDist(rng);

        const std::string text =
            randomString(textLen, rng);

        const std::string pattern =
            randomString(patternLen, rng);

        auto kmp =
            kmp_search(
                text,
                pattern);

        auto rk =
            rabin_karp_search(
                text,
                pattern);

        auto bm =
            boyer_moore_search(
                text,
                pattern);


        if (!(kmp == rk &&
            kmp == bm))
        {
            std::cerr
                << "\nMismatch found!\n";

            std::cerr
                << "Text: "
                << text << '\n';

            std::cerr
                << "Pattern: "
                << pattern << '\n';

            assert(false);
        }

        auto reference =
            reference_search(text, pattern);

        assert(kmp == reference);
        assert(rk == reference);
        assert(bm == reference);
    }

}

void test_kmp_stats()
{
    KMPStats stats{};

    auto matches =
        kmp_search_counted(
            "banana",
            "ana",
            stats);

    assert(matches.size() == 2);

    assert(stats.comparisons > 0);
}

void test_bm_stats()
{
    BMStats stats{};

    auto matches =
        boyer_moore_search_counted(
            "banana",
            "ana",
            stats);

    assert(matches.size() == 2);

    assert(stats.comparisons > 0);
    assert(stats.alignments > 0);
}

int main()
{
    std::cout
        << "Running deterministic tests...\n";

    run_common_tests(kmp_search);
    run_common_tests(rabin_karp_search);
    run_common_tests(boyer_moore_search);

    test_kmp_stats();
    test_bm_stats();

    std::cout
        << "Deterministic tests passed.\n";

    std::cout
        << "Running random correctness tests...\n";

    run_random_tests();

    std::cout
        << "All random correctness tests passed.\n";

    return 0;
}