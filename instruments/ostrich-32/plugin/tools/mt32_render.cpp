// SPDX-License-Identifier: GPL-3.0-or-later
//
// mt32_render -- the core without a host: says which images in a folder 88emu recognises
// and which machines they make complete, starts one, plays a MIDI file or a test chord and
// writes a WAV.  Prints the display as it changes.  For tests on the build host and in CI.
//
//   mt32_render <rom folder> <out.wav> [seconds] [file.mid] [machine] [rate]
//
// machine: an id as listed, e.g. mt32old/1.04 (default: the first one listed); "" as
// file.mid keeps the test sequence.  A MIDI file starts after 0.5 s.  Without one: a
// chord on part 1 (MIDI channel 2, as the MT-32 is set up from power-on), a snare on the
// rhythm part (channel 10), then the panel: MASTER VOLUME and the knob turned down twice,
// then SOUND and the knob turned up a little.

#include "mt32_core.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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
			u16(uint16_t(int16_t(std::lrint(std::fmax(-1.f, std::fmin(1.f, v)) * 32767.f))));
	std::fclose(f);
}

/// A type-0 or type-1 MIDI file, flattened to (seconds, bytes), tempo map applied.
/// Without bytes: a panel action, the switches held (buttons >= 0) or a knob turn.
struct Event { double t; std::vector<uint8_t> bytes; int buttons = -1; int knob = 0; };

static std::vector<Event> readMidi(const fs::path &p)
{
	const std::vector<uint8_t> d = readFile(p);
	std::vector<Event> out;
	if (d.size() < 14 || std::memcmp(d.data(), "MThd", 4) != 0)
		return out;
	auto be32 = [&](size_t i) { return uint32_t(d[i]) << 24 | uint32_t(d[i + 1]) << 16 | uint32_t(d[i + 2]) << 8 | d[i + 3]; };
	const unsigned ntrk = (d[10] << 8) | d[11], div = (d[12] << 8) | d[13];
	if (div == 0 || (div & 0x8000))
		return out;                                    // SMPTE time: not supported
	struct Raw { uint64_t tick; std::vector<uint8_t> bytes; uint32_t tempo; };   // tempo != 0: a tempo event
	std::vector<Raw> raw;
	size_t pos = 8 + be32(4);
	for (unsigned t = 0; t < ntrk && pos + 8 <= d.size(); t ++)
	{
		const size_t len = be32(pos + 4);
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
			const uint8_t b = d[i];
			if (b == 0xFF)
			{
				if (i + 2 > end) break;
				const uint8_t type = d[i + 1];
				i += 2;
				const uint32_t l = vlq();
				if (i + l > end) break;
				if (type == 0x51 && l == 3)
					raw.push_back({ tick, {}, uint32_t(d[i]) << 16 | uint32_t(d[i + 1]) << 8 | d[i + 2] });
				i += l;
			}
			else if (b == 0xF0 || b == 0xF7)
			{
				i ++;
				const uint32_t l = vlq();
				if (i + l > end) break;
				std::vector<uint8_t> s;
				if (b == 0xF0) s.push_back(0xF0);
				s.insert(s.end(), d.begin() + i, d.begin() + i + l);
				if (b == 0xF0 && !s.empty() && s.back() == 0xF7)
					raw.push_back({ tick, s, 0 });
				i += l;
			}
			else
			{
				if (b & 0x80) { status = b; i ++; }
				if (!status) break;
				const size_t n = ((status & 0xE0) == 0xC0) ? 1 : 2;
				if (i + n > end) break;
				std::vector<uint8_t> m { status };
				m.insert(m.end(), d.begin() + i, d.begin() + i + n);
				i += n;
				raw.push_back({ tick, m, 0 });
			}
		}
	}
	std::stable_sort(raw.begin(), raw.end(), [](const Raw &a, const Raw &b) { return a.tick < b.tick; });
	double sec = 0, perTick = 0.5 / div;
	uint64_t last = 0;
	for (const Raw &r : raw)
	{
		sec += (r.tick - last) * perTick;
		last = r.tick;
		if (r.tempo)
			perTick = r.tempo / 1e6 / div;
		else
			out.push_back({ sec, r.bytes });
	}
	return out;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: %s <rom folder> <out.wav> [seconds] [file.mid] [machine] [rate]\n", argv[0]);
		return 2;
	}
	const double seconds = argc > 3 ? std::atof(argv[3]) : 6.5;
	const std::string midiFile = argc > 4 ? argv[4] : "";
	const std::string wanted = argc > 5 ? argv[5] : "";
	const unsigned rate = argc > 6 ? unsigned(std::atoi(argv[6])) : 48000;

	std::printf("88emu %s\n", mt32::libraryVersion());
	std::vector<std::vector<uint8_t>> images;
	std::vector<fs::path> paths;
	std::error_code ec;
	for (fs::recursive_directory_iterator it(argv[1], ec), end; !ec && it != end; it.increment(ec))
	{
		if (!it->is_regular_file(ec) || it->file_size(ec) > 0x200000)
			continue;
		paths.push_back(it->path());
	}
	std::sort(paths.begin(), paths.end());
	for (const fs::path &p : paths)
	{
		std::vector<uint8_t> d = readFile(p);
		mt32::RomId id;
		if (mt32::identifyRom(d.data(), d.size(), id))
		{
			std::printf("  %-28s %-7s %-30s %s\n", p.filename().string().c_str(), id.slot.c_str(),
			            id.version.c_str(), id.devices.c_str());
			images.push_back(std::move(d));
		}
		else
			std::printf("  %-28s (not known to 88emu)\n", p.filename().string().c_str());
	}
	const std::vector<mt32::Machine> machines = mt32::findMachines(images);
	std::printf("complete machines:");
	for (const auto &m : machines)
		std::printf("\n  %-16s %s", m.id.c_str(), m.description.c_str());
	std::printf("\n");
	if (machines.empty())
	{
		std::printf("no complete machine\n");
		return 1;
	}
	std::string machine = machines.front().id;
	if (!wanted.empty())
		machine = wanted;

	mt32::Core core;
	std::string error;
	const auto t0 = std::chrono::steady_clock::now();
	if (!core.start(machine, images, rate, error))
	{
		std::printf("start failed: %s\n", error.c_str());
		return 1;
	}
	std::printf("running %s at %u Hz\n", machine.c_str(), rate);

	std::vector<Event> events;
	if (!midiFile.empty())
	{
		events = readMidi(midiFile);
		for (Event &e : events)
			e.t += 0.5;
		std::printf("%zu MIDI events from %s\n", events.size(), midiFile.c_str());
	}
	else
	{
		for (uint8_t k : { 60, 64, 67 })
		{
			events.push_back({ 0.5, { 0x91, k, 100 } });
			events.push_back({ 2.0, { 0x81, k, 0 } });
		}
		events.push_back({ 2.5, { 0x99, 38, 120 } });
		events.push_back({ 2.7, { 0x89, 38, 0 } });
		events.push_back({ 3.0, {}, int(mt32::ButtonMasterVolume) });
		events.push_back({ 3.2, {}, 0 });
		events.push_back({ 3.5, {}, -1, -4 });
		events.push_back({ 4.0, {}, -1, -4 });
		events.push_back({ 5.0, {}, int(mt32::ButtonSound) });
		events.push_back({ 5.2, {}, 0 });
		events.push_back({ 5.5, {}, -1, 1 });
	}

	const uint32_t total = uint32_t(seconds * rate), block = 64;
	std::vector<float> L(total), R(total);
	size_t next = 0;
	std::string shown;
	for (uint32_t pos = 0; pos < total; pos += block)
	{
		const uint32_t n = std::min(block, total - pos);
		while (next < events.size() && events[next].t * rate < pos + n)
		{
			const double at = events[next].t * rate;
			const uint32_t offset = at > pos ? uint32_t(at - pos) : 0;
			const Event &e = events[next];
			if (!e.bytes.empty())
				core.midi(e.bytes.data(), e.bytes.size(), offset);
			else if (e.knob)
				core.panelKnob(e.knob);
			else
				core.panelButtons(uint32_t(e.buttons));
			next ++;
		}
		core.render(&L[pos], &R[pos], n);
		char text[21];
		bool lamp = false;
		if (core.display(text, lamp) && shown != text)
		{
			shown = text;
			std::printf("  %6.2f s  [%-20s]%s\n", double(pos) / rate, text, lamp ? "  MIDI" : "");
		}
	}
	const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	float peak = 0;
	for (uint32_t i = 0; i < total; i ++)
		peak = std::max({ peak, std::fabs(L[i]), std::fabs(R[i]) });
	std::printf("rendered %.1f s in %.2f s (%.0fx real time), peak %.1f dBFS\n", seconds, took,
	            seconds / took, peak > 0 ? 20 * std::log10(peak) : -999.0);
	writeWav(argv[2], L, R, rate);
	std::printf("wrote %s\n", argv[2]);
	return 0;
}
