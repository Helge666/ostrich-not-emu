// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich != Emu (shared/plugin) -- the DSP half of the Windows standalone's own MIDI IN:
// which port is open, the choice saved for the next run, and the messages of a block at
// their place in it.  MidiInput.hpp does the ports; StandaloneBar.hpp is the panel's half.
//
// The states (the instrument names them in initState):
//   "midiin"     UI -> DSP, "<n> <choice>": open another port; "<n> ?" only asks for the
//                ports again (a keyboard plugged in since).  -> setState()
//   "midiports"  DSP -> UI: empty where there is none (plugins, Linux); else fields
//                separated by kFieldSep: the choice ("" off, "*" all inputs, or a port's
//                name), what went wrong with it (empty when nothing), then the ports there
//                are.  -> portsText(), takePorts()
//
// Threads: setState() and start() on the thread that sets states; collect(), event(),
// drop() and takePorts() on the audio thread (takePorts never waits).

#ifndef OSTRICH_STANDALONE_MIDI_HPP
#define OSTRICH_STANDALONE_MIDI_HPP

#include "MidiInput.hpp"
#include "FileIO.hpp"
#include "Folders.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

namespace ostrich {

class StandaloneMidi
{
public:
	static constexpr char kSep = '\x1f';
	static constexpr const char *kAll = "*";
	static constexpr const char *kRescan = "?";

	/// `active`: the Windows standalone (MidiInput::supported() and DPF's format name);
	/// `name`: the instrument's settings folder ("Ostrich 55").
	StandaloneMidi(bool active, const char *name)
		: m_active(active && MidiInput::supported()),
		  m_file(standaloneSettingFile(name, "standalone-midi-in.txt")) {}

	bool active() const { return m_active; }

	/// Opens the choice of the last run; the first time every input, so that a keyboard
	/// plays at once.
	void start()
	{
		if (!m_active)
			return;
		const std::vector<uint8_t> saved = m_file.empty() ? std::vector<uint8_t>() : readFile(m_file, 4096);
		std::string choice = saved.empty() ? std::string(kAll) : std::string(saved.begin(), saved.end());
		while (!choice.empty() && (choice.back() == '\n' || choice.back() == '\r'))
			choice.pop_back();
		apply(choice, false);
	}

	/// The "midiin" state.
	void setState(const char *value)
	{
		unsigned n = 0;
		int used = 0;
		if (!m_active || std::sscanf(value, "%u %n", &n, &used) < 1 || used == 0)
			return;
		const std::string choice = value + used;
		if (choice == kRescan)
			rescan();
		else
			apply(choice, true);
	}

	/// The "midiports" state, for getState().
	std::string portsText() const
	{
		std::lock_guard<std::mutex> lock(m_portsLock);
		return m_ports;
	}

	/// Audio thread: when the ports changed since, `send(text)` with the new text (for
	/// updateStateValue), under the lock; never waits for it.
	template <class Send>
	void takePorts(Send send)
	{
		if (!m_portsPending.load())
			return;
		std::unique_lock<std::mutex> lock(m_portsLock, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		m_portsPending.store(false);
		send(m_ports.c_str());
	}

	bool portsPending() const { return m_portsPending.load(); }

	// ---- the audio thread ----------------------------------------------------------------
	//
	// run() places what came in by its time stamp, one block late: block k plays what came
	// in during [m_end - its length, m_end).  m_end moves on by the block's length in audio
	// time, not by the clock: WASAPI calls in bursts (several small blocks at once, then a
	// pause), and a clock read at each call would pile a burst's notes up.  It never runs
	// ahead of the clock, and it is pulled towards the clock once a second by the smallest
	// lag seen, so that the two clocks' drift does not add up.  The latency is then a
	// block, steady, instead of 0 .. 1 block.

	struct Event { uint32_t frame; const uint8_t *data; uint32_t size; };

	/// The messages of a block of `frames` at `rate`; then event(0 .. n-1), in time order.
	uint32_t collect(uint32_t frames, double rate)
	{
		m_count = 0;
		if (!m_active || frames == 0)
			return 0;
		const double now = MidiInput::now();
		const double len = double(frames) / rate;
		double end = m_end + len;
		if (m_end == 0.0 || end > now || end < now - 0.25)
			end = now;                      // the first block, ahead, or after a stall
		m_minLag = std::min(m_minLag, now - end);
		m_lagSpan += len;
		if (m_lagSpan >= 1.0)
		{
			if (m_minLag > 0.0005)
				end += (m_minLag - 0.0005) * 0.5;
			m_minLag = 1e9;
			m_lagSpan = 0.0;
		}
		m_end = end;
		const double start = end - len;

		uint32_t n = 0;
		size_t used = 0;
		double t = 0.0;
		while (n < kMax && used + MidiInput::kMaxMessage <= sizeof(m_data))
		{
			const size_t size = m_in.pop(end, m_data + used, t);
			if (size == 0)
				break;
			const double f = std::floor((t - start) * rate);
			m_events[n].frame = f <= 0.0 ? 0 : std::min(uint32_t(f), frames - 1);
			m_events[n].offset = uint32_t(used);
			m_events[n].size = uint32_t(size);
			used += size;
			n ++;
		}
		m_count = n;
		return n;
	}

	Event event(uint32_t i) const
	{
		return { m_events[i].frame, m_data + m_events[i].offset, m_events[i].size };
	}

	/// Played while nothing listens (no machine yet): gone, not played all at once later.
	void drop()
	{
		if (!m_active)
			return;
		double t;
		while (m_in.pop(1e300, m_data, t) > 0)
			;
		m_end = 0.0;
		m_count = 0;
	}

private:
	void apply(const std::string &choice, bool save)
	{
		m_choice = choice;
		m_problem = m_in.open(choice);
		m_seen = MidiInput::ports();
		if (save && !m_file.empty())
		{
			std::error_code ec;
			std::filesystem::create_directories(m_file.parent_path(), ec);
			writeFile(m_file, std::vector<uint8_t>(choice.begin(), choice.end()));
		}
		publish();
	}

	// The panel opens its list: a port that came or went since.  A port that plays is left
	// open, so that no note-off gets lost.
	void rescan()
	{
		if (MidiInput::ports() != m_seen && (!m_problem.empty() || m_choice == kAll))
			apply(m_choice, false);
		else
		{
			m_seen = MidiInput::ports();
			publish();
		}
	}

	void publish()
	{
		std::string t = m_choice + kSep + m_problem;
		for (const std::string &p : m_seen)
			t += kSep + p;
		std::lock_guard<std::mutex> lock(m_portsLock);
		m_ports = t;
		m_portsPending.store(true);
	}

	const bool m_active;
	const std::filesystem::path m_file;
	MidiInput m_in;

	// The thread that sets states'; m_ports also under m_portsLock.
	std::string m_choice, m_problem;
	std::vector<std::string> m_seen;
	mutable std::mutex m_portsLock;
	std::string m_ports;
	std::atomic<bool> m_portsPending { false };

	// The audio thread's: the timeline, and what one block took in.
	double m_end = 0.0, m_minLag = 1e9, m_lagSpan = 0.0;
	static constexpr uint32_t kMax = 512;
	struct Stored { uint32_t frame, offset, size; };
	Stored m_events[kMax] = {};
	uint32_t m_count = 0;
	uint8_t m_data[MidiInput::kMaxMessage * 4] = {};
};

} // namespace ostrich

#endif // OSTRICH_STANDALONE_MIDI_HPP
