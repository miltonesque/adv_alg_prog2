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

std::vector<std::size_t> lps(const std::string& pattern) {
	
	//store pattern length
	const std::size_t m = pattern.size();
	std::vector<std::size_t> lps(m, 0);

	//track length of current matching prefix
	int length = 0;

	//loop through pattern, starts at 1 because the first character lps[0] will always be zero
	for (int i = 1; i < m; i++) {

		//if the length prefix can be shortened, then try the next shorter prefix that will match
		while (length > 0) {

			length = lps[length - 1 ];
		}
		//extend prefix lenth is possible and increment the length
		if (pattern[i] == pattern[length]) {
			++length;
		}
		lps[i] = length;
	}
	return lps;
}