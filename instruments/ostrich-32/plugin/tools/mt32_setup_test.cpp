// SPDX-License-Identifier: GPL-3.0-or-later
//
// mt32_setup_test -- checks what a session keeps (mt32::Core::setup, readSetup,
// restoreSetup) without a host.  One machine is set up as a user would: the panel picks
// another sound for part 1 and turns the master volume down, a program change picks a patch
// for part 2, a SysEx switches part 3's reverb off.  Its setup is read and handed to a
// second, freshly started machine.  That one is read in turn; both readings must be the
// same, and so must the sound each of PART 1 to 5 shows on the display.  The level of a note
// on each part is printed besides.
//
//   mt32_setup_test <rom folder> [machine]

#include "mt32_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

static constexpr unsigned kRate = 48000, kBlock = 64;

/// Renders @p seconds, printing the display when it changes.
static void run(mt32::Core &core, double seconds, const char *who, std::vector<float> *keep = nullptr)
{
	static std::string shown;
	float l[kBlock], r[kBlock];
	for (uint32_t done = 0, total = uint32_t(seconds * kRate); done < total; done += kBlock)
	{
		core.render(l, r, kBlock);
		if (keep)
			for (unsigned i = 0; i < kBlock; i ++)
			{
				keep->push_back(l[i]);
				keep->push_back(r[i]);
			}
		char text[21];
		bool lamp = false;
		if (core.display(text, lamp) && shown != text)
		{
			shown = text;
			std::printf("  %s  [%-20s]\n", who, text);
		}
	}
}

static void press(mt32::Core &core, uint32_t buttons, const char *who)
{
	core.panelButtons(buttons);
	run(core, 0.1, who);
	core.panelButtons(0);
	run(core, 0.2, who);
}

static void send(mt32::Core &core, std::vector<uint8_t> m)
{
	if (m[0] == 0xF0)
	{
		// Roland checksum over address and data
		unsigned sum = 0;
		for (size_t i = 5; i + 2 < m.size(); i ++)
			sum += m[i];
		m[m.size() - 2] = uint8_t((128 - sum % 128) % 128);
	}
	core.midi(m.data(), m.size(), 0);
}

/// Reads the unit and waits for the answers.
static std::string readBack(mt32::Core &core, const char *who)
{
	core.readSetup();
	while (core.busy())
		run(core, 0.1, who);
	return core.setup();
}

/// A note on each part (channels 2-9) and a snare (10), one after another.
static std::vector<float> notes(mt32::Core &core, const char *who)
{
	std::vector<float> out;
	for (uint8_t ch = 1; ch <= 9; ch ++)
	{
		const uint8_t key = ch == 9 ? 38 : 60;
		const uint8_t on[3] = { uint8_t(0x90 | ch), key, 100 }, off[3] = { uint8_t(0x80 | ch), key, 0 };
		core.midi(on, 3, 0);
		run(core, 0.25, who, &out);
		core.midi(off, 3, 0);
		run(core, 0.25, who, &out);
	}
	return out;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		std::fprintf(stderr, "usage: %s <rom folder> [machine]\n", argv[0]);
		return 2;
	}
	std::vector<std::vector<uint8_t>> images;
	std::error_code ec;
	for (fs::recursive_directory_iterator it(argv[1], ec), end; !ec && it != end; it.increment(ec))
	{
		mt32::RomId id;
		if (!it->is_regular_file(ec) || it->file_size(ec) > 0x200000)
			continue;
		std::vector<uint8_t> d = readFile(it->path());
		if (mt32::identifyRom(d.data(), d.size(), id))
			images.push_back(std::move(d));
	}
	const auto machines = mt32::findMachines(images);
	if (machines.empty())
	{
		std::printf("no complete machine\n");
		return 1;
	}
	const std::string machine = argc > 2 ? argv[2] : machines.front().id;

	std::string error;
	mt32::Core a;
	if (!a.start(machine, images, kRate, error))
	{
		std::printf("start failed: %s\n", error.c_str());
		return 1;
	}
	std::printf("%s\n", machine.c_str());
	run(a, 2.0, "A");
	const std::string fresh = readBack(a, "A");
	std::printf("power-on setup: %zu characters\n", fresh.size());

	// The panel: another sound for part 1, master volume a little down.  The knob is a pot
	// (it starts fully clockwise): where it stands is what the firmware reads.
	press(a, mt32::ButtonPart1, "A");
	press(a, mt32::ButtonSound, "A");
	a.panelKnob(-6);
	run(a, 0.5, "A");
	press(a, mt32::ButtonMasterVolume, "A");
	a.panelKnob(-3);
	run(a, 0.5, "A");
	// MIDI: patch 20 on part 2, reverb off on part 3 (patch temp 03 00 20 + 6).
	send(a, { 0xC2, 19 });
	send(a, { 0xF0, 0x41, 0x10, 0x16, 0x12, 0x03, 0x00, 0x26, 0x00, 0x00, 0xF7 });
	run(a, 0.5, "A");

	const std::string setupA = readBack(a, "A");
	std::printf("setup A: %zu characters, %zu messages\n", setupA.size(),
	            size_t(std::count(setupA.begin(), setupA.end(), ' ') + 1));
	if (setupA == fresh)
	{
		std::printf("FAIL: the setup did not change\n");
		return 1;
	}

	mt32::Core b;
	if (!b.start(machine, images, kRate, error))
	{
		std::printf("start failed: %s\n", error.c_str());
		return 1;
	}
	if (!b.restoreSetup(setupA))
	{
		std::printf("FAIL: restoreSetup refused the setup\n");
		return 1;
	}
	while (b.busy())
		run(b, 0.1, "B");
	run(b, 0.5, "B");
	const std::string setupB = readBack(b, "B");
	std::printf("setup B: %zu characters\n", setupB.size());

	bool same = setupA == setupB;
	std::printf("readings %s\n", same ? "the same" : "DIFFERENT");
	if (!same)
	{
		size_t i = 0;
		while (i < setupA.size() && i < setupB.size() && setupA[i] == setupB[i])
			i ++;
		std::printf("  first difference at %zu:\n  A ...%s\n  B ...%s\n", i,
		            setupA.substr(i > 40 ? i - 40 : 0, 120).c_str(), setupB.substr(i > 40 ? i - 40 : 0, 120).c_str());
	}

	// The sound each part plays, as the firmware shows it on PART n: from its timbre temp,
	// which the readings leave out (a restore selects the sound, the unit loads it).
	static const uint32_t partButtons[5] = { mt32::ButtonPart1, mt32::ButtonPart2, mt32::ButtonPart3,
	                                         mt32::ButtonPart4, mt32::ButtonPart5 };
	bool sounds = true;
	for (unsigned k = 0; k < 5; k ++)
	{
		std::string shown[2];
		mt32::Core *const both[2] = { &a, &b };
		for (int i = 0; i < 2; i ++)
		{
			// "1|Fretless 2|vol> 80", or in SOUND mode "1|Bass   >Fretless 2"; a press from
			// another mode may only bring the main screen back.
			char text[21] = {};
			bool lamp = false;
			for (int tries = 0; tries < 2 && text[1] != '|'; tries ++)
			{
				press(*both[i], partButtons[k], i ? "B" : "A");
				both[i]->display(text, lamp);
			}
			const std::string t = text;
			shown[i] = t.size() < 20 ? t : t[9] == '>' ? t.substr(10, 10) : t.substr(2, 10);
		}
		sounds = sounds && shown[0] == shown[1];
		std::printf("  PART %u:  A [%s]  B [%s]%s\n", k + 1, shown[0].c_str(), shown[1].c_str(),
		            shown[0] == shown[1] ? "" : "  DIFFERENT");
	}
	same = same && sounds;

	for (const auto &[name, text] : { std::pair<const char *, const std::string *>("A", &setupA), { "B", &setupB } })
	{
		std::printf("  setup %s:\n", name);
		for (size_t i = 0, j; i < text->size(); i = j + 1)
		{
			j = text->find(' ', i);
			if (j == std::string::npos)
				j = text->size();
			std::printf("    %s\n", text->substr(i, j - i).c_str());
		}
	}

	// What each part sounds like, as levels, next to machines fresh from power-on that played
	// after different times.  For the eye only: the readings decide.  Machines with the same
	// memory do not sound exactly alike: what the LA32 and the reverb hold from earlier notes,
	// and where the firmware's timers stand, depend on how a machine got there.  Master
	// volume 90 set from the panel and set by SysEx gives the same memory and levels up to
	// 1.6 dB apart on some parts (parts 4 and 6 of the power-on setup are the touchy ones),
	// and so does A against the restored machines.  A part that plays another sound differs
	// by more than that.
	const std::vector<float> na = notes(a, "A"), nb = notes(b, "B");
	const size_t per = size_t(0.5 * kRate) * 2;
	auto level = [&](const std::vector<float> &v, size_t p) {
		double e = 0;
		for (size_t i = p * per; i < (p + 1) * per && i < v.size(); i ++)
			e += double(v[i]) * v[i];
		return 10 * std::log10(e + 1e-12);
	};
	double low[9], high[9];
	std::fill(low, low + 9, 1e9);
	std::fill(high, high + 9, -1e9);
	for (double wait : { 2.0, 2.13, 2.37, 2.71, 3.05, 4.4 })
	{
		mt32::Core c;
		if (!c.start(machine, images, kRate, error))
			return 1;
		run(c, wait, "C");
		const std::vector<float> nc = notes(c, "C");
		for (size_t p = 0; p < 9; p ++)
		{
			low[p] = std::min(low[p], level(nc, p));
			high[p] = std::max(high[p], level(nc, p));
		}
	}
	// The same for more machines restored from A's setup, again after different times.
	double rlow[9], rhigh[9];
	std::fill(rlow, rlow + 9, 1e9);
	std::fill(rhigh, rhigh + 9, -1e9);
	for (double wait : { 0.0, 0.29, 0.61, 1.37 })
	{
		mt32::Core r;
		if (!r.start(machine, images, kRate, error) || !r.restoreSetup(setupA))
			return 1;
		while (r.busy())
			run(r, 0.1, "R");
		run(r, 0.5 + wait, "R");
		const std::vector<float> nr = notes(r, "R");
		for (size_t p = 0; p < 9; p ++)
		{
			rlow[p] = std::min(rlow[p], level(nr, p));
			rhigh[p] = std::max(rhigh[p], level(nr, p));
		}
	}
	std::printf("  levels (dB)   A set up  B restored   more restored    fresh machines\n");
	for (size_t p = 0; p < 9; p ++)
		std::printf("  channel %2zu: %9.1f %11.1f   %5.1f .. %5.1f   %5.1f .. %5.1f\n", p + 2, level(na, p), level(nb, p),
		            rlow[p], rhigh[p], low[p], high[p]);
	std::printf("%s\n", same ? "PASS" : "FAIL");
	return same ? 0 : 1;
}
