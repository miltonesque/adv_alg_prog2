/*
* knuth morris pratt 
* it searches for occurence sof a word within a text string. it will slide through
* the pattern character by character but WILL NOT backtrack to the text pointer. it will
* it will skip redundant character comparisons
* so when a mismatch is detected (after matches are found), we already know some of the characters in the text of the next window and this will be used to avoid matching characters that will match anyways.
*
* asking how much of what i just matched is still useful
*/

/*
* need a "failure table" or "LPS" array longest  proper prefix
* so for string "ABAB" consider prefix and suffix. a real prefix is a prefix that is shorter than entire string
* longest prefix that is also a suffix is "AB" with length 2. this is what will be in the failure table
*/

/*
* function to take a pattern in and return its LPS array
* take pattern length
* create LPS vector with all values set to 0
* initialise pointers
* loop through pattern and build lps
* return lps
*/

#include <vector>
#include <string>
#include <iostream>
#include "kmp.hpp"

std::vector<std::size_t> buildLPS(const std::string& pattern) {
	
	//store pattern length
	const std::size_t m = pattern.size();
	std::vector<std::size_t> lps(m, 0);

	//track length of current matching prefix
	std::size_t length = 0;

	//loop through pattern, starts at 1 because the first character lps[0] will always be zero
	for (std::size_t i = 1; i < m; i++) {

		//if the length prefix can be shortened, then try the next shorter prefix that will match
		while (length > 0 && pattern[i] != pattern[length]) {

			length = lps[length - 1];
		}
		//extend prefix lenth is possible and increment the length
		if (pattern[i] == pattern[length]) {
			++length;
		}
		lps[i] = length;
	}
	return lps;
}

/*
* search function
* take in the text and pattern
* loop through and if characters match, increase text and pattern
* if pattern index = pattern size, text index - pattern index?
* if mismatch and pattern > 0, no increment to text
* otherwise, increment i
*/

std::vector<std::size_t> kmp_search(const std::string& text, const std::string& pattern) {

	const std::size_t n = text.size();
	const std::size_t m = pattern.size();
	std::vector<std::size_t> matches;

	if (m == 0 || m > n) {
    return matches;
	}	

	const std::vector<std::size_t> lps = buildLPS(pattern);
	std::size_t k = 0; //pattern index

	for (std::size_t i = 0; i < n; ++i) {
		while (k > 0 && text[i] != pattern[k]) {
			k = lps[k - 1];
		}

		if (text[i] == pattern[k]) {
			++k;
		}

		if (k == m) {
			matches.push_back(i + 1 - m);
			k = lps[k - 1];
		}
	}
	return matches;
}

// ---- counted search ----
// exactly the same steps as kmp_search, with counters added. each text
// character / pattern character pair that is compared is counted once.
std::vector<std::size_t> kmp_search_counted(const std::string& text,
	const std::string& pattern,
	KMPStats& stats) {
	std::vector<std::size_t> matches{};
	const std::size_t n{ text.size() };
	const std::size_t m{ pattern.size() };
	if (m == 0 || m > n) {
		return matches;
	}

	const std::vector<std::size_t> lps{ buildLPS(pattern) };
	std::size_t k{ 0 };
	for (std::size_t i = 0; i < n; ++i) {
		while (k > 0) {
			++stats.comparisons;
			if (text[i] == pattern[k]) {
				break;
			}
			k = lps[k - 1];
			++stats.fallbacks;
		}
		if (k == 0) {
			// nothing matches yet (or it fell all the way back), so compare
			// with the first pattern character
			++stats.comparisons;
			if (text[i] == pattern[0]) {
				++k;
			}
		}
		else {
			++k;  // the loop broke out on a matching character
		}
		if (k == m) {
			matches.push_back(i + 1 - m);
			k = lps[k - 1];
		}
	}
	return matches;
}