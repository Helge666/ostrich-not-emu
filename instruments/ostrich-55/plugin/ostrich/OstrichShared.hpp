// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the DSP and the UI of Ostrich 55 agree on: parameter indices, state keys, the
// panel blob.

#ifndef OSTRICH_SHARED_HPP
#define OSTRICH_SHARED_HPP

#include <cstdint>
#include <cstring>

namespace ostrich {

enum Params : uint32_t
{
	kParamVolume = 0,           ///< dB, a post-gain like the unit's VOLUME knob

	// The front-panel keys, as boolean parameters: the UI presses them, and a generic host
	// UI can too.  Order = kKeyBits below.  Never insert before existing ones: hosts store
	// automation by index.
	kParamKeyFirst,
	kParamKeyEnd = kParamKeyFirst + 19,

	kParamCount = kParamKeyEnd
};

constexpr unsigned kNumKeys = kParamKeyEnd - kParamKeyFirst;

/// The keys in parameter order, as pairs where the unit has them: ◀ then ▶.
enum Key : unsigned
{
	kKeyPower = 0,
	kKeyAll, kKeyMute,
	kKeyPartL, kKeyPartR,
	kKeyInstL, kKeyInstR,
	kKeyLevelL, kKeyLevelR,
	kKeyPanL, kKeyPanR,
	kKeyReverbL, kKeyReverbR,
	kKeyChorusL, kKeyChorusR,
	kKeyKeyShiftL, kKeyKeyShiftR,
	kKeyMidiChL, kKeyMidiChR,
};
static_assert(kKeyMidiChR + 1 == kNumKeys);

/// Each key's bit in Nuked-SC55's key matrix (sc55_core.h, Button).
constexpr int kKeyBits[kNumKeys] = { 0, 6, 5, 22, 14, 3, 4, 20, 21, 12, 13, 18, 19, 10, 11, 16, 17, 8, 9 };

constexpr const char *kKeyNames[kNumKeys] = {
	"POWER", "ALL", "MUTE", "PART <", "PART >", "INSTRUMENT <", "INSTRUMENT >", "LEVEL <",
	"LEVEL >", "PAN <", "PAN >", "REVERB <", "REVERB >", "CHORUS <", "CHORUS >",
	"KEY SHIFT <", "KEY SHIFT >", "MIDI CH <", "MIDI CH >"
};
constexpr const char *kKeySymbols[kNumKeys] = {
	"key_power", "key_all", "key_mute", "key_part_l", "key_part_r", "key_inst_l", "key_inst_r",
	"key_level_l", "key_level_r", "key_pan_l", "key_pan_r", "key_reverb_l", "key_reverb_r",
	"key_chorus_l", "key_chorus_r", "key_keyshift_l", "key_keyshift_r", "key_midich_l",
	"key_midich_r"
};

enum States : uint32_t
{
	kStatePanel = 0,            ///< DSP -> UI, the LCD
	kStateStatus,               ///< DSP -> UI, one line of text: ROMs, what happened
	kStateKey,                  ///< UI -> DSP, "<key> <0|1>": a panel key
	kStateUiSize,               ///< host session: the panel's size, "<w> <h>" unscaled
	kStateRomSet,               ///< DSP -> UI: which ROM images were found, see below
	kStateRomFile,              ///< UI -> DSP, "<n> <path>": a file to find ROM images by
	kStateReset,                ///< UI -> DSP, "<n> gs|gm": a GS or GM reset into MIDI IN
	kStateCount
};

/// The SC-55's images, in the order the panel shows them (sc55_core.h, RomLocation).
constexpr unsigned kRomSlots = 5;
constexpr unsigned kRomSlotLocation[kRomSlots] = { 0, 1, 3, 4, 5 };
constexpr const char *kRomChips[kRomSlots] = { "H8/532 MCU", "H8/532 EXTRA CODE", "WAVE A", "WAVE B", "WAVE C" };
constexpr const char *kRomTitles[kRomSlots] = { "CPU ROM", "Program ROM", "Wave ROM A", "Wave ROM B", "Wave ROM C" };
constexpr const char *kRomSizes[kRomSlots] = { "32 KB", "256 KB", "1 MB", "1 MB", "1 MB" };

/// "romset" is fields separated by kFieldSep: one per ROM slot, empty when the image is
/// missing, else "found"; then the version of the set that runs (or would), e.g. "v1.21";
/// then the folder new images are copied to.
constexpr char kFieldSep = '\x1f';

/// The LCD as the UI draws it.  Sent as hex, only when it changed.
struct PanelBlob
{
	uint8_t data[80];           ///< the controller's data RAM (sc55_core.cpp, LCD layout)
	uint8_t cgram[64];          ///< the 8 custom glyphs: the bar display, the L/R marks
	uint8_t flags;              ///< bit 0: machine running; bit 1: MIDI in; bit 2: display on
};

inline void encodeHex(const uint8_t *p, size_t n, char *out)
{
	static const char *const d = "0123456789abcdef";
	for (size_t i = 0; i < n; i ++)
	{
		out[i * 2] = d[p[i] >> 4];
		out[i * 2 + 1] = d[p[i] & 15];
	}
	out[n * 2] = 0;
}

inline int hexNibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

inline bool decodeHex(const char *s, uint8_t *out, size_t n)
{
	if (std::strlen(s) != n * 2)
		return false;
	for (size_t i = 0; i < n; i ++)
	{
		const int hi = hexNibble(s[i * 2]), lo = hexNibble(s[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return false;
		out[i] = uint8_t((hi << 4) | lo);
	}
	return true;
}

} // namespace ostrich

#endif // OSTRICH_SHARED_HPP
