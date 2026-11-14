// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 32 -- what the DSP half (OstrichPlugin.cpp) and the panel (OstrichUI.cpp) share:
// parameter and state numbers, the panel's switches, the blob the display travels in.

#ifndef OSTRICH_SHARED_HPP
#define OSTRICH_SHARED_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ostrich {

// The parameters are the panel's switches, for automation and generic host UIs; the panel
// itself sends its clicks as states (kStateKey).  Their numbers are a contract with saved
// automation: append, never insert.
enum Params : uint32_t
{
	kParamKeyFirst = 0,
	kParamKeyEnd = kParamKeyFirst + 10,
	kParamCount = kParamKeyEnd
};

constexpr uint32_t kNumKeys = kParamKeyEnd - kParamKeyFirst;

// The MT-32's switches, as the unit has them: PART 1 2 3 / 4 5 RHYTHM, then SOUND GROUP,
// SOUND / VOLUME, MASTER VOLUME.
enum Key : uint32_t
{
	kKeyPart1, kKeyPart2, kKeyPart3, kKeyPart4, kKeyPart5, kKeyRhythm,
	kKeySoundGroup, kKeySound, kKeyVolume, kKeyMasterVolume,
};
static_assert(kKeyMasterVolume + 1 == kNumKeys);

/// The switch's bit for 88emu (mt32_core.h, mt32::Button).
constexpr uint32_t kKeyBits[kNumKeys] = {
	1u << 0, 1u << 1, 1u << 2, 1u << 8, 1u << 9, 1u << 10,
	1u << 3, 1u << 11, 1u << 4, 1u << 12,
};
const char *const kKeyNames[kNumKeys] = {
	"PART 1", "PART 2", "PART 3", "PART 4", "PART 5", "RHYTHM",
	"SOUND GROUP", "SOUND", "VOLUME", "MASTER VOLUME",
};
const char *const kKeySymbols[kNumKeys] = {
	"key_part1", "key_part2", "key_part3", "key_part4", "key_part5", "key_rhythm",
	"key_soundgroup", "key_sound", "key_volume", "key_mastervolume",
};

/// The knob's travel in the steps 88emu turns it by (32nds); it starts fully clockwise.
constexpr int kKnobSteps = 32;

enum States : uint32_t
{
	kStatePanel = 0,    // DSP -> UI: the display (PanelBlob, hex)
	kStateStatus,       // DSP -> UI: one line of text (ROMs, what happened)
	kStateKey,          // UI -> DSP: "<key> <0|1>"
	kStateKnob,         // UI -> DSP: "<n> <steps>", steps to turn the knob by
	kStateUiSize,       // host session: "<w> <h>" unscaled
	kStateRomSet,       // DSP -> UI: which images were found (below)
	kStateRomFile,      // UI -> DSP: "<n> <path>"
	kStateMachine,      // host session: the machine to run, e.g. "mt32old/1.04"
	kStateMachines,     // DSP -> UI: the machines the images make (below)
	kStateMidiIn,       // UI -> DSP: "<n> <choice>"
	kStateMidiPorts,    // DSP -> UI: MIDI IN ports
	kStateSetup,        // host session: "<machine> <SysEx as hex> ...", the unit's setup
	kStatePartCh,       // host session, UI <-> DSP: "1" = channels 1-8 play parts 1-8
	kStateCount
};

// "romset": for the old MT-32, whether its control ROM, PCM ROM and reverb ROM were found
// ("found" or ""), then the folder new images are copied to; fields split by kFieldSep.
constexpr unsigned kRomSlots = 3;
const char *const kRomTitles[kRomSlots] = { "Control ROM", "PCM ROM", "Reverb ROM" };
const char *const kRomChips[kRomSlots] = { "IC26 + IC27", "IC21 + IC22 / IC37", "IC13" };
const char *const kRomSizes[kRomSlots] = { "2 x 32 KB", "512 KB", "32 KB" };

// "machines": the running machine's id (empty while none runs), then id and name in turns.
constexpr char kFieldSep = '\x1f';

/// "midiin" and "midiports", the Windows standalone's own MIDI IN:
/// shared/plugin/StandaloneMidi.hpp.

// The display, sent as hex only when it changed (and once a second).
struct PanelBlob
{
	char text[20];      // the LCD's 20 characters, as the firmware wrote them
	uint8_t knob;       // the knob, 0 .. kKnobSteps
	uint8_t flags;      // bit 0 machine running, bit 1 MIDI MESSAGE lamp, bit 2 starting
};

inline void encodeHex(const uint8_t *in, size_t n, char *out)
{
	static const char digits[] = "0123456789abcdef";
	for (size_t i = 0; i < n; i ++)
	{
		out[2 * i] = digits[in[i] >> 4];
		out[2 * i + 1] = digits[in[i] & 15];
	}
	out[2 * n] = '\0';
}

inline int hexNibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

inline bool decodeHex(const char *in, uint8_t *out, size_t n)
{
	if (std::strlen(in) != 2 * n)
		return false;
	for (size_t i = 0; i < n; i ++)
	{
		const int hi = hexNibble(in[2 * i]), lo = hexNibble(in[2 * i + 1]);
		if (hi < 0 || lo < 0)
			return false;
		out[i] = uint8_t(hi << 4 | lo);
	}
	return true;
}

} // namespace ostrich

#endif // OSTRICH_SHARED_HPP
