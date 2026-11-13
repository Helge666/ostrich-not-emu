// SPDX-License-Identifier: GPL-2.0-or-later
//
// Ostrich 55 -- the Roland SC-55 as the plugin sees it.  Nuked-SC55 (jcmoyer's fork, in
// nuked-sc55/) does the emulation; this is the narrow interface around it, so that the
// plugin needs none of its headers (they want C++23).
//
// Under the GPL 2.0 or later, as Nuked-SC55 itself.

#ifndef SC55_CORE_H
#define SC55_CORE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sc55 {

/// Where an image goes, in Nuked-SC55's order (RomLocation).  The SC-55 (mk1) uses the
/// first two and the three wave ROMs; the others are there for the later models.
enum RomLocation : unsigned
{
	kRom1 = 0,          ///< H8/532 MCU, internal ROM
	kRom2,              ///< H8/532 program
	kSubRom,            ///< M37450 sub-MCU (mk2, SC-55st)
	kWave1,
	kWave2,
	kWave3,
	kWaveCard,
	kWaveExp,
	kRomLocations
};

const char *romLocationName(unsigned location);

/// One ROM set Nuked-SC55 knows: "mk1-v1.21" and the like.
struct RomsetPart
{
	std::string romset;         ///< Nuked-SC55's name, e.g. "mk1-v1.21"
	unsigned location;          ///< RomLocation
};

/// What an image is, by its SHA-256: every (set, location) it belongs to.  Empty when
/// Nuked-SC55 does not know it.  Thread-safe, no state.
std::vector<RomsetPart> identifyRom(const uint8_t *data, size_t size);

/// The sets of one family, by Nuked-SC55's name: "mk1" gives "mk1-v1.00", "mk1-v1.10" ...
std::vector<std::string> romsetsOfFamily(const char *family);

/// The locations a set needs.
std::vector<unsigned> romsetLocations(const std::string &romset);

/// The SC-55's display as the firmware wrote it: the controller's 80 bytes of data RAM
/// and its 8 custom glyphs.  Which byte is which field: sc55_core.cpp, LCD layout.
struct LcdState
{
	uint8_t data[80];
	uint8_t cgram[64];
	bool enabled;               ///< the firmware has switched the display on
};

/// The LCD controller's character ROM as Nuked-SC55 has it: for a character code of 16 or
/// more, its rows top down, bit 4 the leftmost of 5 dots (7 rows, 10 for the few with
/// descenders).  Codes below 16 are the custom glyphs, LcdState::cgram.
const uint8_t *lcdGlyph(uint8_t code);

/// The front-panel keys, as bits of setButtons().  The values are Nuked-SC55's key
/// matrix positions (mcu.h, MCU_BUTTON_*).
enum Button : unsigned
{
	kButtonPower = 0,
	kButtonInstL = 3,
	kButtonInstR = 4,
	kButtonInstMute = 5,
	kButtonInstAll = 6,
	kButtonMidiChL = 8,
	kButtonMidiChR = 9,
	kButtonChorusL = 10,
	kButtonChorusR = 11,
	kButtonPanL = 12,
	kButtonPanR = 13,
	kButtonPartR = 14,
	kButtonKeyShiftL = 16,
	kButtonKeyShiftR = 17,
	kButtonReverbL = 18,
	kButtonReverbR = 19,
	kButtonLevelL = 20,
	kButtonLevelR = 21,
	kButtonPartL = 22,
};

class Core
{
public:
	Core();
	~Core();
	Core(const Core &) = delete;
	Core &operator=(const Core &) = delete;

	/// Loads the set `romset` ("mk1-v1.21") from `images` (one per RomLocation, empty where
	/// unused), as dumped: the wave ROMs are descrambled here.  Starts the machine from
	/// power-on.  `error` says what went wrong.
	bool start(const std::string &romset, std::array<std::vector<uint8_t>, kRomLocations> images,
			std::string &error);

	/// Runs `frames` frames at sampleRate() and throws the sound away: the boot, before
	/// the host's first block.
	void skip(uint32_t frames);

	bool running() const { return m_running; }
	const std::string &romset() const { return m_romset; }

	/// The rate render() produces: 64 kHz for the SC-55 (mk1) with Nuked-SC55's
	/// oversampling, 32 kHz without; the mk2 runs at 66207 Hz.
	double sampleRate() const;

	/// Nuked-SC55 doubles the PCM chip's rate by interpolating, as the unit's chip does
	/// (on by default).  Takes effect at once; sampleRate() changes with it.
	void setOversampling(bool on);

	/// MIDI bytes into the unit's MIDI IN; they reach the firmware at 31250 baud from the
	/// sample rendered next.
	void midi(const uint8_t *data, size_t size);

	/// Renders `frames` frames at sampleRate(), full scale = 1.0.  Real-time safe.
	void render(float *left, float *right, uint32_t frames);

	/// The keys held down, as a mask of (1 << Button).
	void setButtons(uint32_t mask);

	/// A copy of the display.  Returns false (and leaves `out` alone) when the firmware
	/// is writing to it right now; ask again on the next block.
	bool lcd(LcdState &out);

private:
	struct Impl;
	std::unique_ptr<Impl> m;
	bool m_running = false;
	std::string m_romset;
};

} // namespace sc55

#endif // SC55_CORE_H
