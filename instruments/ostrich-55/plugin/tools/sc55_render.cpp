// SPDX-License-Identifier: GPL-3.0-or-later
//
// sc55_render -- the core without a host: finds an SC-55 ROM set in a folder by the images'
// contents, boots it, plays a MIDI file or a test chord and writes a WAV at the core's rate.
// Prints the display at the end.  For tests on the build host and in CI.
//
//   sc55_render <rom folder> <out.wav> [seconds] [file.mid]

#include "sc55_core.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::vector<uint8_t> readFile(const fs::path &p)
{
	std::ifstream f(p, std::ios::binary);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

static void writeWav(const char *path, const std::vector<float> &l, const std::vector<float> &r, unsigned rate)
{
	FILE *f = std::fopen(path, "wb");
	if (!f)
		return;
	const uint32_t n = uint32_t(l.size()), data = n * 4;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32(rate); u32(rate * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(data);
	for (uint32_t i = 0; i < n; i ++)
		for (float v : { l[i], r[i] })
		{
			const int16_t s = int16_t(std::lrint(std::fmax(-1.f, std::fmin(1.f, v)) * 32767.f));
			u16(uint16_t(s));
		}
	std::fclose(f);
}

/// A type-0 or type-1 MIDI file, flattened to (seconds, bytes), tempo map applied.
struct Event { double t; std::vector<uint8_t> bytes; };

static std::vector<Event> readMidi(const fs::path &p)
{
	const std::vector<uint8_t> d = readFile(p);
	std::vector<Event> out;
	if (d.size() < 14 || std::memcmp(d.data(), "MThd", 4) != 0)
		return out;
	const unsigned ntrk = (d[10] << 8) | d[11], div = (d[12] << 8) | d[13];
	struct Raw { uint64_t tick; std::vector<uint8_t> bytes; uint32_t tempo; };   // tempo != 0: a tempo event
	std::vector<Raw> raw;
	size_t pos = 8 + ((d[4] << 24) | (d[5] << 16) | (d[6] << 8) | d[7]);
	for (unsigned t = 0; t < ntrk && pos + 8 <= d.size(); t ++)
	{
		const size_t len = (d[pos + 4] << 24) | (d[pos + 5] << 16) | (d[pos + 6] << 8) | d[pos + 7];
		size_t i = pos + 8;
		const size_t end = std::min(d.size(), i + len);
		pos = i + len;
		uint64_t tick = 0;
		uint8_t status = 0;
		auto vlq = [&]() { uint32_t v = 0; while (i < end) { const uint8_t b = d[i ++]; v = (v << 7) | (b & 0x7F); if (!(b & 0x80)) break; } return v; };
		while (i < end)
		{
			tick += vlq();
			if (i >= end) break;
			uint8_t b = d[i];
			if (b == 0xFF)
			{
				const uint8_t type = d[i + 1];
				i += 2;
				const uint32_t n = vlq();
				if (type == 0x51 && n == 3)
					raw.push_back({ tick, {}, uint32_t((d[i] << 16) | (d[i + 1] << 8) | d[i + 2]) });
				i += n;
				continue;
			}
			if (b == 0xF0 || b == 0xF7)
			{
				i ++;
				const uint32_t n = vlq();
				std::vector<uint8_t> bytes;
				if (b == 0xF0) bytes.push_back(0xF0);
				bytes.insert(bytes.end(), d.begin() + i, d.begin() + std::min(end, i + n));
				raw.push_back({ tick, bytes, 0 });
				i += n;
				continue;
			}
			if (b & 0x80) { status = b; i ++; }
			const unsigned n = ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 1 : 2;
			std::vector<uint8_t> bytes { status };
			for (unsigned k = 0; k < n && i < end; k ++) bytes.push_back(d[i ++]);
			raw.push_back({ tick, bytes, 0 });
		}
	}
	std::stable_sort(raw.begin(), raw.end(), [](const Raw &a, const Raw &b) { return a.tick < b.tick; });
	double sec = 0, perTick = 0.5 / div;
	uint64_t last = 0;
	for (const Raw &r : raw)
	{
		sec += double(r.tick - last) * perTick;
		last = r.tick;
		if (r.tempo) perTick = r.tempo / 1e6 / div;
		else out.push_back({ sec, r.bytes });
	}
	return out;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: sc55_render <rom folder> <out.wav> [seconds] [file.mid]\n");
		return 2;
	}
	const double seconds = argc > 3 ? std::atof(argv[3]) : 6.0;

	// Every file of a plausible size, identified by its SHA-256.
	std::map<std::string, std::array<std::string, sc55::kRomLocations>> sets;
	for (const auto &e : fs::recursive_directory_iterator(argv[1]))
	{
		if (!e.is_regular_file() || e.file_size() > 0x800000)
			continue;
		const std::vector<uint8_t> data = readFile(e.path());
		for (const sc55::RomsetPart &p : sc55::identifyRom(data.data(), data.size()))
		{
			const std::u8string u8 = e.path().u8string();
			sets[p.romset][p.location] = std::string(u8.begin(), u8.end());
		}
	}
	std::string chosen;
	for (const std::string &name : sc55::romsetsOfFamily("mk1"))
	{
		bool complete = sets.count(name) != 0;
		for (unsigned loc : sc55::romsetLocations(name))
			complete = complete && !sets[name][loc].empty();
		std::printf("%-12s %s\n", name.c_str(), complete ? "complete" : "-");
		if (complete)
			chosen = name;
	}
	if (chosen.empty())
	{
		std::fprintf(stderr, "no complete SC-55 (mk1) ROM set in %s\n", argv[1]);
		return 1;
	}

	std::array<std::vector<uint8_t>, sc55::kRomLocations> images;
	for (unsigned loc : sc55::romsetLocations(chosen))
		images[loc] = readFile(fs::path(reinterpret_cast<const char8_t *>(sets[chosen][loc].c_str())));
	sc55::Core core;
	std::string error;
	if (!core.start(chosen, std::move(images), error))
	{
		std::fprintf(stderr, "%s: %s\n", chosen.c_str(), error.c_str());
		return 1;
	}
	const unsigned rate = unsigned(core.sampleRate());
	std::printf("running %s at %u Hz\n", chosen.c_str(), rate);

	std::vector<Event> events;
	if (argc > 4)
		events = readMidi(argv[4]);
	else
	{
		// After the boot: a C major chord on part 1, then a GS drum hit on part 10.
		const double t0 = 1.5;
		for (uint8_t n : { 60, 64, 67 })
		{
			events.push_back({ t0, { 0x90, n, 100 } });
			events.push_back({ t0 + 2.0, { 0x80, n, 0 } });
		}
		events.push_back({ t0 + 2.5, { 0x99, 38, 120 } });
	}
	const double offset = argc > 4 ? 1.5 : 0.0;       // a MIDI file starts after the boot

	// SC55_KEYS="bit@seconds,...": panel keys (sc55_core.h, Button) held for 0.1 s.
	struct Press { unsigned bit; double at; };
	std::vector<Press> presses;
	if (const char *keys = std::getenv("SC55_KEYS"))
		for (const char *p = keys; *p; )
		{
			unsigned bit = 0;
			double at = 0;
			int used = 0;
			if (std::sscanf(p, "%u@%lf%n", &bit, &at, &used) != 2)
				break;
			presses.push_back({ bit, at });
			p += used;
			if (*p == ',') p ++;
		}

	std::vector<float> L(size_t(seconds * rate)), R(L.size());
	const uint32_t block = 64;
	size_t next = 0;
	const auto t0 = std::chrono::steady_clock::now();
	for (size_t pos = 0; pos < L.size(); pos += block)
	{
		while (next < events.size() && (events[next].t + offset) * rate <= double(pos))
		{
			core.midi(events[next].bytes.data(), events[next].bytes.size());
			next ++;
		}
		uint32_t mask = 0;
		for (const Press &k : presses)
			if (double(pos) >= k.at * rate && double(pos) < (k.at + 0.1) * rate)
				mask |= 1u << k.bit;
		core.setButtons(mask);
		core.render(&L[pos], &R[pos], uint32_t(std::min<size_t>(block, L.size() - pos)));
	}
	const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	std::printf("rendered %.1f s in %.2f s (%.1fx real time)\n", seconds, took, seconds / took);

	float peak = 0;
	for (size_t i = 0; i < L.size(); i ++)
		peak = std::fmax(peak, std::fmax(std::fabs(L[i]), std::fabs(R[i])));
	std::printf("peak %.3f (%.1f dBFS)\n", peak, 20 * std::log10(std::fmax(peak, 1e-9f)));
	writeWav(argv[2], L, R, rate);

	sc55::LcdState lcd;
	if (core.lcd(lcd))
	{
		auto field = [&](int at, int n) { std::string s; for (int i = 0; i < n; i ++) { const uint8_t c = lcd.data[at + i]; s += (c >= 32 && c < 127) ? char(c) : '.'; } return s; };
		std::printf("display %s: PART %s  INSTRUMENT %s\n", lcd.enabled ? "on" : "off",
				field(0, 3).c_str(), field(3, 16).c_str());
		std::printf("  LEVEL %s  PAN %s  REVERB %s  CHORUS %s  KEY SHIFT %s  MIDI CH %s\n",
				field(40, 3).c_str(), field(43, 3).c_str(), field(49, 3).c_str(),
				field(46, 3).c_str(), field(52, 3).c_str(), field(55, 3).c_str());
	}
	return 0;
}
