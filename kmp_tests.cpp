// test_kmp.cpp — focused correctness tests for the KMP implementation.
//
//   1. Edge cases with hand-checked answers
//   2. Small inputs: KMP vs brute force on EVERY text/pattern over {a, b}
//      (a two-letter alphabet forces the most partial matches and overlaps)
//
// Convention (from bmp.hpp and benchmark.cpp): an empty pattern, or one longer
// than the text, gives no matches.
//
// Build and run (kmp.cpp's own main() is skipped by -DKMP_NO_MAIN):
//   g++ -std=c++17 -O2 -DKMP_NO_MAIN kmp.cpp test_kmp.cpp -o test_kmp && ./test_kmp
// Exit code: 0 = all passed, 1 = a wrong answer, 3 = a crash.

#include "kmp.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using Matches = std::vector<std::size_t>;

// --- Reference implementations ----------------------------------------------

// Tries every alignment; an empty pattern gives no matches.
static Matches brute_force(const std::string& text, const std::string& pattern) {
	Matches result;
	if (pattern.empty()) return result;
	for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
		std::size_t j = 0;
		while (j < pattern.size() && text[i + j] == pattern[j]) ++j;
		if (j == pattern.size()) result.push_back(i);
	}
	return result;
}

// --- Reporting ---------------------------------------------------------------

static int g_checks = 0, g_failures = 0;
static std::string g_current; // case being run, printed if the program crashes

extern "C" void on_crash(int) {
	std::fprintf(stderr, "\nCRASH while running: %s\n", g_current.c_str());
	std::_Exit(3);
}

static std::string show(const std::string& s) {
	std::string out = "\"";
	for (std::size_t i = 0; i < s.size() && i < 30; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		if (c >= 32 && c < 127) {
			out += static_cast<char>(c);
		} else {
			char buf[8];
			std::snprintf(buf, sizeof buf, "\\x%02X", c);
			out += buf;
		}
	}
	out += "\"";
	if (s.size() > 30) out += "... (length " + std::to_string(s.size()) + ")";
	return out;
}

static std::string show(const Matches& m) {
	std::string out = "[";
	for (std::size_t i = 0; i < m.size() && i < 10; ++i) out += (i ? ", " : "") + std::to_string(m[i]);
	return out + (m.size() > 10 ? ", ...]" : "]") + " (" + std::to_string(m.size()) + " matches)";
}

static void expect(const std::string& name, const std::string& text, const std::string& pattern,
                   const Matches& expected, const std::string& source) {
	g_current = name + " | text=" + show(text) + " | pattern=" + show(pattern);
	const Matches got = kmp_search(text, pattern);
	++g_checks;
	if (got != expected && ++g_failures <= 10) {
		std::cout << "  FAIL: " << name << "\n    text:     " << show(text) << "\n    pattern:  " << show(pattern)
		          << "\n    expected: " << show(expected) << "  (" << source << ")\n    got:      " << show(got) << "\n";
	}
}

static void summary(const char* suite, int checks_before, int failures_before) {
	std::cout << (g_failures == failures_before ? "PASS  " : "FAIL  ") << suite << " ("
	          << g_checks - checks_before << " checks)\n";
}

// --- 1. Edge cases -------------------------------------------------------------

static void test_edge_cases() {
	const int c = g_checks, f = g_failures;
	const std::string nul(1, '\0');
	struct Case { const char* name; std::string text, pattern; Matches expected; };
	const std::vector<Case> cases = {
		{"empty pattern gives no matches", "ABC", "", {}},
		{"empty text and empty pattern", "", "", {}},
		{"empty text", "", "A", {}},
		{"pattern longer than text", "AB", "ABC", {}},
		{"pattern equals text", "ABC", "ABC", {0}},
		{"single character", "A", "A", {0}},
		{"match at start only", "ABXXX", "AB", {0}},
		{"match at end only", "XXXAB", "AB", {3}},
		{"overlapping matches", "AAAA", "AA", {0, 1, 2}},
		{"periodic pattern", "ABABABA", "ABA", {0, 2, 4}},
		{"partial match fails on last character", "ABCABCABD", "ABCABD", {3}},
		{"embedded NUL bytes", "A" + nul + "B" + nul + "B", nul + "B", {1, 3}},
		{"NUL pattern, no NUL in text", "ABC", nul, {}},
		{"high bytes (negative as signed char)", "\xFF\x80\xFF\x80", "\xFF\x80", {0, 2}},
		{"bytes differing only in the top bit", "A", "\xC1", {}},
	};
	for (const Case& t : cases) expect(t.name, t.text, t.pattern, t.expected, "hand-checked");
	summary("edge cases", c, f);
}

// --- 2. Small inputs vs brute force ----------------------------------------------

// Every string over the alphabet with length 0..max_len.
static std::vector<std::string> all_strings(const std::string& alphabet, std::size_t max_len) {
	std::vector<std::string> out = {""};
	for (std::size_t start = 0; start < out.size(); ++start) {
		if (out[start].size() == max_len) continue;
		for (char ch : alphabet) out.push_back(out[start] + ch);
	}
	return out;
}

static void test_small_vs_brute_force() {
	const int c = g_checks, f = g_failures;
	const std::vector<std::string> texts = all_strings("ab", 10);
	const std::vector<std::string> patterns = all_strings("ab", 5);
	for (const std::string& t : texts) {
		for (const std::string& p : patterns) expect("small exhaustive", t, p, brute_force(t, p), "brute force");
	}
	summary("small inputs vs brute force", c, f);
}

int main() {
	std::signal(SIGSEGV, on_crash);
	std::signal(SIGABRT, on_crash);
	test_edge_cases();
	test_small_vs_brute_force();
	std::cout << (g_failures == 0 ? "\nALL PASSED: " : "\nFAILED: ") << g_checks << " checks, " << g_failures
	          << " failures\n";
	return g_failures == 0 ? 0 : 1;
}
