// SPDX-License-Identifier: GPL-3.0-or-later
//
// mt32_core -- Ostrich 32's narrow wrapper around 88emu (gearmulator/source/ronaldo/88emu,
// by the gearmulator authors; LA32 from Nuked-MT32 by nukeykt, reverb after Sergey V.
// Mikayev).  88emu runs the unit's own firmware, so the display, the buttons and the knob
// behave as the firmware makes them.  This wrapper finds the machines among the user's
// images, runs one at the host's rate with MIDI at frame offsets, and passes the panel
// through.  The plugin includes only this header, never 88emu's.

#ifndef MT32_CORE_H
#define MT32_CORE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mt32 {

/// One image as 88emu's ROM registry knows it (by size and MD5 or SHA-1, never by name).
struct RomId
{
	std::string devices;       ///< the boards that take it, e.g. "MT-32 1.x, MT-32 2.x"
	std::string slot;          ///< "control", "wave" or "reverb"
	std::string version;       ///< the registry's text, e.g. "1.04 A, IC27", "R15179857"
	bool chip = false;         ///< one of two chips that together make the image
};

/// Identifies an image; false if 88emu does not know it or it is not for an LA board.
/// Stateless, thread-safe.
bool identifyRom(const uint8_t *data, size_t size, RomId &out);

/// A machine the images make complete: control ROM (whole, or both chips), wave ROM
/// (whole, or both chips) and the reverb microcode of that board.
struct Machine
{
	std::string id;            ///< e.g. "mt32old/1.04", "mt32new/2.04", "cm32l/1.02"
	std::string description;   ///< e.g. "MT-32 1.x, control ROM 1.04"
};

/// The machines the images make complete: old MT-32 first, then new MT-32, CM-32L,
/// CM-32LN; within a board by firmware name (so 1.04 before 1.07).
std::vector<Machine> findMachines(const std::vector<std::vector<uint8_t>> &images);

/// 88emu's version, e.g. "2.2.27".
const char *libraryVersion();

/// The MT-32's switches, for Core::panelButtons() (88emu's numbering).
enum Button : uint32_t
{
	ButtonPart1 = 1u << 0, ButtonPart2 = 1u << 1, ButtonPart3 = 1u << 2,
	ButtonSoundGroup = 1u << 3, ButtonVolume = 1u << 4,
	ButtonPart4 = 1u << 8, ButtonPart5 = 1u << 9, ButtonRhythm = 1u << 10,
	ButtonSound = 1u << 11, ButtonMasterVolume = 1u << 12,
};

/// One running machine.  Not thread-safe: call everything from one thread (the plugin's
/// audio thread) or guard it.
class Core
{
public:
	Core();
	~Core();
	Core(const Core &) = delete;
	Core &operator=(const Core &) = delete;

	/// Starts `machine` (an id from findMachines) from `images` (any order; others are
	/// ignored) and renders at `sampleRate` from then on, from power-on.  Writes the images
	/// it needs into a private folder under the system's temp folder while it opens and
	/// removes it again.  On failure returns false and says why in `error`.
	bool start(const std::string &machine, const std::vector<std::vector<uint8_t>> &images,
	           double sampleRate, std::string &error);
	void stop();

	bool running() const { return m_running; }
	const std::string &machine() const { return m_machine; }
	double sampleRate() const { return m_rate; }

	/// A complete MIDI message (short message or SysEx F0 ... F7), to be heard `offset`
	/// frames into the next render() call.  Not for System Realtime.  Real-time safe; a
	/// message that does not fit the queue is dropped.
	void midi(const uint8_t *data, size_t size, uint32_t offset = 0);

	/// Renders `frames` frames at sampleRate(); full scale = 1.0.  Real-time safe.
	void render(float *left, float *right, uint32_t frames);

	/// The switches held down (Button bits, 0 = all released).  The firmware reads the panel
	/// at its own pace: hold a press for a few milliseconds of rendering.
	void panelButtons(uint32_t buttons);

	/// Turns the knob by 32nds of its travel, negative = anticlockwise.
	void panelKnob(int steps);

	/// The 20 characters of the LCD as the firmware left them (plus a terminating 0), and
	/// the MIDI MESSAGE lamp.  False while not running.
	bool display(char text[21], bool &midiLamp) const;

	/// What a host's stop does: hold pedal up and All Notes Off on every channel.
	void silence();

private:
	struct Impl;
	std::unique_ptr<Impl> m;
	bool m_running = false;
	std::string m_machine;
	double m_rate = 0;
};

} // namespace mt32

#endif // MT32_CORE_H
