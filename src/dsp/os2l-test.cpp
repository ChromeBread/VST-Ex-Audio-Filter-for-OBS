// Self-check for the OS2L stream splitter and the beat counting helpers. Exits non-zero on failure.
#include "dsp/os2l.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool ok, const char *what)
{
	if (!ok) {
		printf("FAIL: %s\n", what);
		failures++;
	}
}

int main()
{
	Os2lSplitter s;
	std::vector<std::string> got;
	auto collect = [&](const std::string &o) { got.push_back(o); };

	// Two objects back to back, split across reads in awkward places.
	std::string stream = "{\"evt\":\"beat\",\"pos\":21,\"bpm\":121.96}  {\"evt\":\"btn\",\"name\":\"a}b{\\\"c\"}";
	for (size_t i = 0; i < stream.size(); i += 5)
		s.feed(stream.data() + i, std::min<size_t>(5, stream.size() - i), collect);
	check(got.size() == 2, "two objects");
	check(got.size() == 2 && got[0] == "{\"evt\":\"beat\",\"pos\":21,\"bpm\":121.96}", "first object whole");
	check(got.size() == 2 && got[1] == "{\"evt\":\"btn\",\"name\":\"a}b{\\\"c\"}", "braces and quotes in strings");

	// Garbage before an object, and a nested object.
	got.clear();
	const char *nested = "xx\r\n{\"a\":{\"b\":1}}";
	s.feed(nested, strlen(nested), collect);
	check(got.size() == 1 && got[0] == "{\"a\":{\"b\":1}}", "nested object, garbage skipped");

	// An object that never ends is dropped, and the next one still comes through.
	got.clear();
	std::string huge = "{\"x\":\"" + std::string(Os2lSplitter::kMaxPending + 10, 'a');
	s.feed(huge.data(), huge.size(), collect);
	check(got.empty(), "oversized object dropped");
	const char *after = "\"}{\"ok\":1}";
	s.feed(after, strlen(after), collect);
	check(!got.empty() && got.back() == "{\"ok\":1}", "recovers after oversized object");

	check(beat_in_cycle(0, 16) == 0 && beat_in_cycle(17, 16) == 1 && beat_in_cycle(-1, 16) == 15, "beat_in_cycle");
	check(beat_in_cycle(-8, 8) == 0 && beat_in_cycle(7, 8) == 7, "beat_in_cycle 8");

	// 120 BPM: a beat every 500 ms. Last beat pos 40 at t=10 s.
	const int64_t beat = 500000000, last = 10000000000;
	check(os2l_reset_offset(40, last, last + 100000000, beat) == 40, "reset just after a beat");
	check(os2l_reset_offset(40, last, last + 400000000, beat) == 41, "reset just before the next beat");

	// Song info batches: header, the values in that order, then the end mark. VirtualDJ sends ~0.1 s apart.
	const int64_t ms = 1000000;
	{
		Os2lBatch b;
		check(!b.feed("F#", 0), "value before any header ignored");
		check(!b.feed("cb|k,1cm,1ti", 0) && !b.feed("F#", 100 * ms) && !b.feed("コメント１", 200 * ms) &&
			      !b.feed("曲名１", 300 * ms),
		      "values alone do not finish a send");
		check(b.feed("cb|.k,1cm,1ti", 400 * ms), "end mark finishes it");
		const auto &r = b.result();
		check(r.size() == 3 && r[0] == std::make_pair(std::string("k"), std::string("F#")) && r[1].first == "1cm" &&
			      r[1].second == "コメント１" && r[2].second == "曲名１",
		      "values paired with the header's names in order");
		check(!b.feed("extra", 500 * ms) && !b.feed("cb|.k,1cm,1ti", 600 * ms), "after a send, nothing more counts");
	}
	{
		// VirtualDJ skipped a value (an empty deck's comment): the end mark comes one value early.
		Os2lBatch b;
		b.feed("cb|3ti,4ti,4cm", 0);
		b.feed("曲名３", 100 * ms);
		b.feed("Drag a song on this deck to load it", 200 * ms);
		check(!b.feed("cb|.3ti,4ti,4cm", 300 * ms), "short send dropped at its end mark");
		// ... and the next send of the same button is read again.
		b.feed("cb|3ti", 1000 * ms);
		b.feed("x", 1100 * ms);
		check(b.feed("cb|.3ti", 1200 * ms) && b.result()[0].second == "x", "next send still read");
	}
	{
		// Two buttons' sends crossing: deck 2's header in the middle of deck 1's send.
		Os2lBatch b;
		b.feed("cb|k,1cm", 0);
		b.feed("F#", 100 * ms);
		b.feed("cb|m,2cm", 150 * ms); // deck 1's send is given up here
		b.feed("deck1 comment", 200 * ms);
		b.feed("2", 250 * ms); // the count matches deck 2's header, but ...
		check(!b.feed("cb|.k,1cm", 300 * ms), "crossed sends: deck 1's end mark does not close deck 2's send");
		check(!b.feed("deck2 comment", 350 * ms) && !b.feed("cb|.m,2cm", 400 * ms), "and nothing after it counts");
		b.feed("cb|m,2cm", 1000 * ms);
		b.feed("2", 1100 * ms);
		check(!b.feed("cb|.m,2cm", 1200 * ms), "too few values, dropped");
	}
	{
		// The end mark far too late to belong to the send.
		Os2lBatch b;
		b.feed("cb|k", 0);
		b.feed("F#", 100 * ms);
		check(!b.feed("cb|.k", 5000 * ms), "late end mark does not finish a send");
	}
	{
		// Values may be empty or look like anything, including a header-ish text that is not ours.
		Os2lBatch b;
		b.feed("cb|k,1cm", 0);
		b.feed("", 100 * ms);
		b.feed("cb", 200 * ms);
		check(b.feed("cb|.k,1cm", 300 * ms) && b.result()[0].second.empty() && b.result()[1].second == "cb", "empty and odd values");
	}
	{
		Os2lBatch b;
		check(!b.feed("cb|", 0) && !b.feed("cb|.", 100 * ms), "header with no names is not ours");
		check(!b.feed("cb|k,,m", 0) && !b.feed("x", 100 * ms) && !b.feed("cb|.k,,m", 200 * ms), "header with an empty name");
		check(!b.feed("cb|toolongname", 0) && !b.feed("x", 100 * ms) && !b.feed("cb|.toolongname", 200 * ms), "header with a long name");
	}

	if (failures == 0)
		printf("os2l-test: all passed\n");
	return failures == 0 ? 0 : 1;
}
