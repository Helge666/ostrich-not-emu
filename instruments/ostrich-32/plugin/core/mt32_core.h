// SPDX-License-Identifier: LGPL-2.1-or-later
//
// mt32_core -- Ostrich 32's narrow wrapper around Munt's libmt32emu (munt/mt32emu, used
// unmodified): finds the ROM sets among the user's images, runs the synth at the host's
// rate and reports its display.  Under the LGPL 2.1 or later like libmt32emu itself.
// The plugin includes only this header, never libmt32emu's.

#ifndef MT32_CORE_H
#define MT32_CORE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mt32 {

/// One image as libmt32emu recognises it by size and SHA-1.
struct RomId
{
	std::string name;          ///< libmt32emu's short name, e.g. "ctrl_mt32_1_07_a"
	std::string description;   ///< e.g. "MT-32 Control v1.07"
	bool control = false;      ///< control ROM; otherwise PCM ROM
	bool partial = false;      ///< one half (or one interleaved chip) of a ROM
};

/// Identifies an image; false if libmt32emu does not know it.  Stateless, thread-safe.
bool identifyRom(const uint8_t *data, size_t size, RomId &out);

/// A machine libmt32emu can run from the given images: control and PCM ROM both present
/// (whole, or as the two halves).
struct Machine
{
	std::string id;            ///< libmt32emu's machine ID, e.g. "mt32_1_07", "cm32l_1_02"
	std::string description;   ///< the control ROM's description, e.g. "MT-32 Control v1.07"
};

/// The machines the images make complete, in libmt32emu's order (old MT-32 first).
std::vector<Machine> findMachines(const std::vector<std::vector<uint8_t>> &images);

/// libmt32emu's version, e.g. "2.8.6".
const char *libraryVersion();

class Core
{
public:
	Core();
	~Core();
	Core(const Core &) = delete;
	Core &operator=(const Core &) = delete;

	/// Opens `machine` from `images` (any order; unknown ones are ignored) and renders at
	/// `sampleRate` from then on.  Starts from power-on.  On failure returns false and
	/// says why in `error`.
	bool start(const std::string &machine, std::vector<std::vector<uint8_t>> images,
	           double sampleRate, std::string &error);
	void stop();

	bool running() const { return m_running; }
	const std::string &machine() const { return m_machine; }
	double sampleRate() const { return m_rate; }

	/// A complete MIDI message (short message or SysEx F0 ... F7), to be heard `offset`
	/// frames into the next render() call.  Not for System Realtime.
	void midi(const uint8_t *data, size_t size, uint32_t offset = 0);

	/// Renders `frames` frames at sampleRate(); full scale = 1.0.  Real-time safe.
	void render(float *left, float *right, uint32_t frames);

	/// The 20 characters of the LCD as the unit shows them (plus a terminating 0), and the
	/// MIDI MESSAGE lamp.  False while not running.
	bool display(char text[21], bool &midiLamp) const;

	/// Back to the main display after a message (what a front-panel button does first).
	void mainDisplay();

	/// Which of the 9 parts (8 melodic, then rhythm) are sounding: bit n set = part n+1.
	uint32_t partStates() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m;
	bool m_running = false;
	std::string m_machine;
	double m_rate = 0;
};

} // namespace mt32

#endif // MT32_CORE_H
