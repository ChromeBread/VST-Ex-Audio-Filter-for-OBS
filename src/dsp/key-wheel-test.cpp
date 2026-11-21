// Self-check for key parsing and the Camelot wheel. Exits non-zero on failure.
#include "dsp/key-wheel.h"

#include <cstdio>
#include <cstring>

static int failures = 0;

static void check(bool ok, const char *what)
{
	if (!ok) {
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static bool is(const char *text, int number, bool minor)
{
	CamelotKey k = parse_key(text);
	return k.number == number && k.minor == minor;
}

int main()
{
	// Every major and minor key against the standard Camelot table.
	const char *majors[12] = {"B", "F#", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E"};
	const char *minors[12] = {"Abm", "Ebm", "Bbm", "Fm", "Cm", "Gm", "Dm", "Am", "Em", "Bm", "F#m", "C#m"};
	for (int n = 1; n <= 12; n++) {
		check(is(majors[n - 1], n, false), majors[n - 1]);
		check(is(minors[n - 1], n, true), minors[n - 1]);
	}
	// What VirtualDJ sent in the test, enharmonic spellings and other notations.
	check(is("F#", 2, false) && is("A#", 6, false) && is("C#m", 12, true), "VirtualDJ samples");
	check(is("G#m", 1, true) && is("D#m", 2, true) && is("Gb", 2, false), "enharmonics");
	check(is("8A", 8, true) && is("12B", 12, false) && is("1b", 1, false), "Camelot");
	check(is("1d", 8, false) && is("1m", 8, true) && is("6d", 1, false), "Open Key");
	check(is(" A minor ", 8, true) && is("Cmaj", 8, false) && is("B\xE2\x99\xAD", 6, false), "spaces, words, flat sign");
	check(!parse_key("").valid() && !parse_key("test").valid() && !parse_key("13A").valid() && !parse_key("H").valid(),
	      "rejects non-keys");
	check(!parse_key("Cx").valid() && !parse_key("8C").valid(), "rejects bad suffixes");

	check(strcmp(key_name({1, false}), "B") == 0 && strcmp(key_name({8, true}), "Am") == 0, "names");
	check(keys_compatible({1, false}, {1, true}) && keys_compatible({1, false}, {12, false}) &&
		      keys_compatible({1, false}, {2, false}),
	      "compatible: relative and neighbours");
	check(!keys_compatible({1, false}, {2, true}) && !keys_compatible({1, false}, {1, false}) &&
		      !keys_compatible({1, false}, {3, false}),
	      "not compatible");

	if (failures == 0)
		printf("key-wheel-test: all passed\n");
	return failures == 0 ? 0 : 1;
}
