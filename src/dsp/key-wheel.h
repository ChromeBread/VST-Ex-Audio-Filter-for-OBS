#pragma once

// Musical keys on the Camelot wheel: 1..12 around the circle of fifths, A = minor, B = major (8B = C major).
// Qt-free so it can be tested alone.

#include <cctype>
#include <string>

struct CamelotKey {
	int number = 0; // 1..12, 0 = unknown
	bool minor = false;
	bool valid() const { return number > 0; }
	bool operator==(const CamelotKey &o) const { return number == o.number && minor == o.minor; }
};

inline int camelot_of_major_pitch(int pitchClass) // C = 0
{
	return (pitchClass * 7 % 12 + 7) % 12 + 1;
}

// What VirtualDJ's get_key prints ("F#", "Bb", "C#m", "Ebm"), also Camelot ("8A", "12B") and Open Key ("1m", "6d").
inline CamelotKey parse_key(const std::string &text)
{
	std::string s;
	for (char c : text)
		if (!std::isspace(static_cast<unsigned char>(c)))
			s += c;
	if (s.empty())
		return {};
	if (std::isdigit(static_cast<unsigned char>(s[0]))) {
		size_t i = 0;
		int n = 0;
		while (i < s.size() && i < 2 && std::isdigit(static_cast<unsigned char>(s[i])))
			n = n * 10 + (s[i++] - '0');
		if (n < 1 || n > 12 || i + 1 != s.size())
			return {};
		char m = char(std::tolower(static_cast<unsigned char>(s[i])));
		if (m == 'a' || m == 'b')
			return {n, m == 'a'};
		if (m == 'm' || m == 'd') // Open Key: 1d = C major = 8B
			return {(n + 6) % 12 + 1, m == 'm'};
		return {};
	}
	static const int letters[7] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
	char l = char(std::toupper(static_cast<unsigned char>(s[0])));
	if (l < 'A' || l > 'G')
		return {};
	int pc = letters[l - 'A'];
	size_t i = 1;
	if (s.compare(i, 1, "#") == 0) {
		pc++;
		i += 1;
	} else if (s.compare(i, 3, "\xE2\x99\xAF") == 0) { // ♯
		pc++;
		i += 3;
	} else if (s.compare(i, 1, "b") == 0) {
		pc--;
		i += 1;
	} else if (s.compare(i, 3, "\xE2\x99\xAD") == 0) { // ♭
		pc--;
		i += 3;
	}
	pc = (pc + 12) % 12;
	std::string rest;
	for (size_t j = i; j < s.size(); j++)
		rest += char(std::tolower(static_cast<unsigned char>(s[j])));
	bool minor = rest == "m" || rest == "min" || rest == "minor";
	if (!minor && !(rest.empty() || rest == "maj" || rest == "major"))
		return {};
	if (minor)
		return {camelot_of_major_pitch((pc + 3) % 12), true}; // the relative major shares the number
	return {camelot_of_major_pitch(pc), false};
}

// Short name as on the wheel, e.g. "B", "F♯", "A♭m".
inline const char *key_name(CamelotKey k)
{
	static const char *major[12] = {"B", "F\xE2\x99\xAF", "D\xE2\x99\xAD", "A\xE2\x99\xAD", "E\xE2\x99\xAD", "B\xE2\x99\xAD",
					"F", "C", "G", "D", "A", "E"};
	static const char *minor[12] = {"A\xE2\x99\xADm", "E\xE2\x99\xADm", "B\xE2\x99\xADm", "Fm", "Cm", "Gm",
					"Dm", "Am", "Em", "Bm", "F\xE2\x99\xAFm", "C\xE2\x99\xAFm"};
	if (!k.valid())
		return "";
	return (k.minor ? minor : major)[k.number - 1];
}

// Keys that mix well: the same number in the other ring, and one step either way in the same ring.
inline bool keys_compatible(CamelotKey a, CamelotKey b)
{
	if (!a.valid() || !b.valid() || a == b)
		return false;
	if (a.number == b.number)
		return true;
	int d = (a.number - b.number + 12) % 12;
	return a.minor == b.minor && (d == 1 || d == 11);
}
