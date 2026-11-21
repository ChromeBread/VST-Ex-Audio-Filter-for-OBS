#pragma once

// OS2L (VirtualDJ's beat feed for lighting software): a TCP stream of JSON objects with no separator between them,
// e.g. {"evt":"beat","change":false,"pos":21,"bpm":121.96,"strength":0.9}. Qt-free so it can be tested alone.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Cuts the stream into whole top-level {...} objects; braces inside strings do not count.
class Os2lSplitter {
public:
	static constexpr size_t kMaxPending = 64 * 1024; // a peer that never closes an object cannot grow memory

	template<class Fn> void feed(const char *data, size_t size, Fn &&onObject)
	{
		for (size_t i = 0; i < size; i++) {
			char c = data[i];
			if (depth == 0) {
				if (c != '{')
					continue; // whitespace or garbage between objects
				pending.clear();
			}
			pending.push_back(c);
			if (inString) {
				if (escaped)
					escaped = false;
				else if (c == '\\')
					escaped = true;
				else if (c == '"')
					inString = false;
			} else if (c == '"') {
				inString = true;
			} else if (c == '{') {
				depth++;
			} else if (c == '}' && --depth == 0) {
				onObject(pending);
				pending.clear();
			}
			if (pending.size() > kMaxPending)
				reset();
		}
	}

	void reset()
	{
		pending.clear();
		depth = 0;
		inString = escaped = false;
	}

private:
	std::string pending;
	int depth = 0;
	bool inString = false, escaped = false;
};

// Place of a beat in a cycle of n beats, 0..n-1, also for beats before the anchor.
inline int beat_in_cycle(int64_t beat, int n)
{
	return int(((beat % n) + n) % n);
}

// Song info from our VirtualDJ scripts (one custom button per deck). Each send is a header button "cb|k,1cm,1ti"
// naming what follows, one button per value in that order, then the end mark "cb|.k,1cm,1ti" repeating the names.
// VirtualDJ sends them about 0.1 s apart. The order comes from the script itself, so the plugin can never pair a
// value with the wrong name. A send counts only when its own end mark comes right after exactly as many values as its
// header named: VirtualDJ silently skips a value it cannot make (a tag of an empty deck), and two buttons' sends
// could cross; both would shift values, and such a send is dropped whole.
class Os2lBatch {
public:
	static constexpr int64_t kPerValueNs = 400000000; // VirtualDJ needs about 0.1 s per message; generous margin
	static constexpr size_t kMaxCodes = 64;

	using Values = std::vector<std::pair<std::string, std::string>>;

	// The end mark that closes a send whose header is `header` ("cb|k,1cm" -> "cb|.k,1cm").
	static std::string endOf(const std::string &header) { return "cb|." + header.substr(3); }

	// Feeds the name of one button message. True when it completed a send; its values are then in result().
	bool feed(const std::string &name, int64_t nowNs)
	{
		if (name.rfind("cb|.", 0) == 0) {
			bool ok = collecting && name == end && values.size() == codes.size() &&
				  nowNs - startedNs <= int64_t(codes.size() + 2) * kPerValueNs;
			collecting = false;
			if (!ok)
				return false;
			done.clear();
			for (size_t i = 0; i < codes.size(); i++)
				done.push_back({codes[i], values[i]});
			return true;
		}
		if (name.rfind("cb|", 0) == 0) {
			codes.clear();
			values.clear();
			size_t start = 3;
			while (start <= name.size() && codes.size() <= kMaxCodes) {
				size_t end = name.find(',', start);
				if (end == std::string::npos)
					end = name.size();
				std::string code = name.substr(start, end - start);
				if (code.empty() || code.size() > 8)
					break; // not a header we wrote
				codes.push_back(code);
				start = end + 1;
			}
			bool ok = !codes.empty() && codes.size() <= kMaxCodes && start > name.size();
			collecting = ok;
			if (!ok)
				codes.clear();
			end = endOf(name);
			startedNs = nowNs;
			return false;
		}
		if (!collecting)
			return false;
		values.push_back(name);
		if (values.size() > codes.size())
			collecting = false; // more values than named: not one clean send
		return false;
	}

	const Values &result() const { return done; }

private:
	std::vector<std::string> codes, values;
	std::string end;
	Values done;
	bool collecting = false;
	int64_t startedNs = 0;
};

// Offset to take from VirtualDJ's beat number so that the beat nearest to `nowNs` becomes beat 0.
inline int64_t os2l_reset_offset(int64_t lastPos, int64_t lastBeatNs, int64_t nowNs, int64_t beatNs)
{
	return lastPos + std::llround(double(nowNs - lastBeatNs) / double(beatNs));
}
