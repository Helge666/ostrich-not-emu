// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 55 -- MIDI IN for the Windows standalone.  DPF's native standalone opens no MIDI
// port unless asked, and then all of them, with no way to choose; so the plugin opens the
// port the panel picks itself, through WinMM.  Short messages and SysEx are passed on as
// bytes, as a cable would: the SC-55's firmware parses them.  Each message carries the time
// it came in (now()), so that the audio thread can put it at its place in a block.
//
// Elsewhere (Linux: JACK has its own MIDI ports; plugins: the host's) it offers nothing.

#ifndef OSTRICH_MIDI_INPUT_HPP
#define OSTRICH_MIDI_INPUT_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ostrich {

class MidiInput
{
public:
	/// The choices besides a port's name.
	static constexpr const char *kOff = "";
	static constexpr const char *kAll = "*";

	/// The longest message handed over at once; a longer SysEx comes in pieces.
	static constexpr size_t kMaxMessage = 4096;

	MidiInput();
	~MidiInput();

	/// Whether this platform has it at all (Windows).
	static bool supported();

	/// The input ports there are now, by name; a name that repeats gets " (2)", " (3)" ...
	static std::vector<std::string> ports();

	/// Seconds on a monotonic clock, the one the messages are stamped with.
	static double now();

	/// Closes what is open and opens `choice`: kOff, kAll or a port's name.  Returns what
	/// went wrong, empty when all went well ("not connected", "in use by another program").
	/// Not on the audio thread.
	std::string open(const std::string &choice);
	void close();

	/// Audio thread, lock-free: the next message that came in at or before `until`, into
	/// `out` (at least kMaxMessage bytes); its length, 0 when there is none.
	size_t pop(double until, uint8_t *out, double &time);

	/// From WinMM's callback: a message, stamped now.
	void push(const uint8_t *data, size_t n);

private:
	struct Port;
	std::vector<std::unique_ptr<Port>> m_open;

	// Messages from the ports to the audio thread, each as [time, length, bytes]: written
	// under m_push by the ports' callbacks, read without a lock by the audio thread.  A
	// message that does not fit is dropped whole.
	static constexpr size_t kRing = 1 << 16;
	static constexpr size_t kHeader = sizeof(double) + sizeof(uint32_t);
	uint8_t m_ring[kRing] = {};
	std::atomic<size_t> m_write { 0 }, m_read { 0 };
	std::mutex m_push;

	void put(size_t at, const void *data, size_t n);
	void get(size_t at, void *data, size_t n) const;
};

} // namespace ostrich

#endif // OSTRICH_MIDI_INPUT_HPP
