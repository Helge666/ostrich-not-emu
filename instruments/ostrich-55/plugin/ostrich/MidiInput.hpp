// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 55 -- MIDI IN for the Windows standalone.  DPF's native standalone opens no MIDI
// port unless asked, and then all of them, with no way to choose; so the plugin opens the
// port the panel picks itself, through WinMM.  Short messages and SysEx are passed on as
// bytes, as a cable would: the SC-55's firmware parses them.
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

	MidiInput();
	~MidiInput();

	/// Whether this platform has it at all (Windows).
	static bool supported();

	/// The input ports there are now, by name; a name that repeats gets " (2)", " (3)" ...
	static std::vector<std::string> ports();

	/// Closes what is open and opens `choice`: kOff, kAll or a port's name.  Returns what
	/// went wrong, empty when all went well ("not connected", "in use by another program").
	/// Not on the audio thread.
	std::string open(const std::string &choice);
	void close();

	/// Audio thread: up to `n` bytes that came in since, in order.  Lock-free.
	size_t read(uint8_t *out, size_t n);

	/// From WinMM's callback.
	void push(const uint8_t *data, size_t n);

private:
	struct Port;
	std::vector<std::unique_ptr<Port>> m_open;

	// Bytes from the ports to the audio thread: written under m_push by the ports' callbacks,
	// read without a lock by the audio thread.  A message that does not fit is dropped whole.
	static constexpr size_t kRing = 1 << 16;
	uint8_t m_ring[kRing] = {};
	std::atomic<size_t> m_write { 0 }, m_read { 0 };
	std::mutex m_push;
};

} // namespace ostrich

#endif // OSTRICH_MIDI_INPUT_HPP
