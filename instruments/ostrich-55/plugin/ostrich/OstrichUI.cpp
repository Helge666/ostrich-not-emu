// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 55 -- the panel: the SC-55's front after the unit (POWER, the LCD with its fields
// and the 16 x 16 dot bar display, the key pairs, VOLUME, the MIDI lamp), not a copy of it,
// and under it a bar for what the unit does not have (status line, GS/GM reset).
// Size handling and the ROM setup follow Ostrich 220's panel.

#include "DistrhoUI.hpp"
#include "OstrichShared.hpp"

#include "sc55_core.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

namespace {

using namespace ostrich;

// Design units: the unit's front is 1350 x 250, the bar below takes the rest of
// DISTRHO_UI_DEFAULT_WIDTH/HEIGHT's ratio.
constexpr float kW = 1350.0f, kPanelH = 250.0f;
constexpr float kH = kW * DISTRHO_UI_DEFAULT_HEIGHT / DISTRHO_UI_DEFAULT_WIDTH;
constexpr float kBarY = kPanelH, kBarH = kH - kPanelH;

struct Rect { float x, y, w, h; bool contains(float px, float py) const
	{ return px >= x && px < x + w && py >= y && py < y + h; } };

// ---- the LCD ---------------------------------------------------------------------------
//
// Drawn in its own units: Nuked-SC55's LCD picture (lcd.cpp), 741 wide, moved down by
// kLcdTop to make room for the field names, which that picture has in its background.
constexpr float kLcdUnitsW = 741.0f, kLcdTop = 22.0f, kLcdUnitsH = 268.0f + kLcdTop;
const Rect kLcd { 124.0f, 54.0f, 470.0f, 470.0f * kLcdUnitsH / kLcdUnitsW };   // the glass

// A field of 5 x 7 characters: where its first character is, how many, its name, and the
// key pair the mouse wheel over it presses.
struct Field { int at, count; float row, col; const char *name; int keyL; };
const Field kFields[] = {
	{  0,  3,  11,  34, "PART",       kKeyPartL },
	{  3, 16,  11, 153, "INSTRUMENT", kKeyInstL },
	{ 40,  3,  75,  34, "LEVEL",      kKeyLevelL },
	{ 43,  3,  75, 153, "PAN",        kKeyPanL },
	{ 49,  3, 139,  34, "REVERB",     kKeyReverbL },
	{ 46,  3, 139, 153, "CHORUS",     kKeyChorusL },
	{ 52,  3, 203,  34, "KEY SHIFT",  kKeyKeyShiftL },
	{ 55,  3, 203, 153, "MIDI CH",    kKeyMidiChL },
};
constexpr float kCharStep = 35.0f, kDot = 6.0f, kDotSize = 5.0f;

// The bar display: two rows of four glyphs (data 20..23 and 60..63), 8 x 5 dots each, the
// fourth only one dot wide -- 16 x 16 dots, a column per part.
constexpr float kBarRow0 = 71.0f, kBarRowStep = 88.0f, kBarCol0 = 293.0f, kBarColStep = 130.0f;
constexpr float kBarDotH = 11.0f, kBarDotW = 26.0f, kBarDotSizeH = 9.0f, kBarDotSizeW = 24.0f;

// The L/R marks left of the bar display, 12 x 11 dots of 1 unit (lcd.cpp, LR).
const uint8_t kMarkL[12][11] = {
	{1,1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0},
	{1,1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0},
	{1,1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0}, {1,1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1,1},
};
const uint8_t kMarkR[12][11] = {
	{1,1,1,1,1,1,1,1,1,0,0}, {1,1,1,1,1,1,1,1,1,1,0}, {1,1,0,0,0,0,0,0,1,1,0}, {1,1,0,0,0,0,0,0,1,1,0},
	{1,1,0,0,0,0,0,0,1,1,0}, {1,1,1,1,1,1,1,1,1,1,0}, {1,1,1,1,1,1,1,1,1,0,0}, {1,1,0,0,0,0,0,1,1,0,0},
	{1,1,0,0,0,0,0,0,1,1,0}, {1,1,0,0,0,0,0,0,1,1,0}, {1,1,0,0,0,0,0,0,0,1,1}, {1,1,0,0,0,0,0,0,0,1,1},
};

// The SC-55's amber backlight, the unlit dots a shade darker, the lit ones black (the
// colours Nuked-SC55 gives its LCD).
const Color kLcdBack(255, 111, 15), kLcdOff(214, 88, 6), kLcdInk(20, 8, 0);

// ---- the keys ----------------------------------------------------------------------------

constexpr float kKeyW = 40.0f, kKeyH = 24.0f, kPairGap = 6.0f;
constexpr float kRow0 = 92.0f, kRow1 = 172.0f;
constexpr float kPairX0 = 712.0f, kPairStep = 118.0f;
// The eight pairs, two rows of four, ◀ then ▶; ALL and MUTE in a column before them.
inline Rect keyRect(unsigned k)
{
	if (k == kKeyPower)
		return { 34.0f, kRow1, 52.0f, kKeyH };
	if (k == kKeyAll || k == kKeyMute)
		return { 628.0f, k == kKeyAll ? kRow0 : kRow1, 52.0f, kKeyH };
	const unsigned pair = (k - kKeyPartL) / 2, right = (k - kKeyPartL) % 2;
	return { kPairX0 + float(pair % 4) * kPairStep + float(right) * (kKeyW + kPairGap),
	         pair < 4 ? kRow0 : kRow1, kKeyW, kKeyH };
}
const char *const kPairNames[8] = { "PART", "INSTRUMENT", "LEVEL", "PAN", "REVERB", "CHORUS", "KEY SHIFT", "MIDI CH" };

const Rect kKnob { 1212.0f, 84.0f, 64.0f, 64.0f };

// The bar, from the right: GM RESET, GS RESET, in the Windows standalone MIDI IN; the status
// line takes the rest.  MIDI IN's list opens upwards, over the panel.
constexpr float kBarBtnH = 18.0f, kBarBtnY = kBarY + (kBarH - kBarBtnH) / 2;
const Rect kGmReset { kW - 16.0f - 80.0f, kBarBtnY, 80.0f, kBarBtnH };
const Rect kGsReset { kGmReset.x - 8.0f - 80.0f, kBarBtnY, 80.0f, kBarBtnH };
const Rect kMidiIn  { kGsReset.x - 16.0f - 240.0f, kBarBtnY, 240.0f, kBarBtnH };
// More ports than fit above the bar go into further columns, to the left.
constexpr float kMenuItemH = 18.0f, kMenuColW = 260.0f;
constexpr int kMenuRows = int((kBarY - 8.0f) / kMenuItemH);
constexpr int kMenuMaxItems = kMenuRows * 5;

// While ROM images are missing they take the place of the display and the keys: what is
// needed, a chip per image, and a button that asks for a file -- any one of them; the rest
// of its folder is found too.
const Rect kRomBox    { 112.0f, 50.0f, 1060.0f, 186.0f };
const Rect kRomChoose { 132.0f, 194.0f, 150.0f, 26.0f };
inline Rect romCell(unsigned i)
{
	return { 610.0f + float(i) * 110.0f, 92.0f, 100.0f, 70.0f };
}

} // anonymous namespace


class OstrichUI : public UI
{
public:
	OstrichUI()
		// DPF scales for the desktop itself and keeps the aspect ratio; everything below
		// draws in design units scaled to whatever size the window has.
		: UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true)
	{
		loadSharedResources();
		std::memset(&m_panel, 0, sizeof(m_panel));
	}

protected:
	void parameterChanged(uint32_t index, float value) override
	{
		if (index == kParamVolume)
			m_volumeDb = value;
		else if (index >= kParamKeyFirst && index < kParamKeyEnd)
			m_keyLit[index - kParamKeyFirst] = value > 0.5f;
		repaint();
	}

	void stateChanged(const char *key, const char *value) override
	{
		if (std::strcmp(key, "panel") == 0)
		{
			PanelBlob b;
			if (decodeHex(value, reinterpret_cast<uint8_t *>(&b), sizeof(b)))
			{
				m_panel = b;
				repaint();
			}
		}
		else if (std::strcmp(key, "uisize") == 0)
			restoreSize(value);
		else if (std::strcmp(key, "status") == 0)
		{
			m_status = value;
			m_romChecking = false;          // the answer to a picked ROM file, if one was asked
			repaint();
		}
		else if (std::strcmp(key, "romset") == 0)
		{
			setRomSet(value);
			repaint();
		}
		else if (std::strcmp(key, "midiports") == 0)
		{
			setMidiPorts(value);
			repaint();
		}
	}

	void uiFileBrowserSelected(const char *filename) override
	{
		const bool asked = m_browsing;
		m_browsing = false;
		if (!asked || filename == nullptr || filename[0] == '\0')
			return;                         // cancelled
		m_romDir = dirOf(filename);
		m_romChecking = true;
		m_romAskedAt = std::chrono::steady_clock::now();
		const std::string msg = std::to_string(++ m_seq) + " " + filename;
		setState("romfile", msg.c_str());
		repaint();
	}

	void onNanoDisplay() override
	{
		// Nothing may escape a draw: under Windows it unwinds through the window procedure,
		// and the panel would never redraw again (Ostrich 220's lesson).
		try
		{
			drawPanel();
		}
		catch (...)
		{
		}
	}

	void drawPanel()
	{
		const float s = getWidth() / kW;
		scale(s, s);

		// The body: dark grey, a lighter edge on top.
		beginPath();
		rect(0, 0, kW, kPanelH);
		fillColor(Color(30, 30, 33));
		fill();
		beginPath();
		rect(0, 0, kW, 4);
		fillColor(Color(66, 66, 70));
		fill();

		fontFace(NANOVG_DEJAVU_SANS_TTF);
		textAlign(ALIGN_LEFT | ALIGN_BASELINE);
		fillColor(Color(235, 235, 235));
		fontSize(24);
		text(24, 38, "OSTRICH 55", nullptr);
		fontSize(11);
		fillColor(Color(170, 170, 170));
		text(178, 38, "MIDI SOUND MODULE  \xc2\xb7  SC-55 EMULATION", nullptr);

		drawPower();
		drawKnob();
		drawMidiLamp();
		if (romSetup())
			drawRomSetup();
		else
		{
			drawLcd();
			drawKeys();
		}
		drawBar();
		if (m_midiMenu)
			drawMidiMenu();
	}

	bool onMouse(const MouseEvent &ev) override
	{
		if (ev.button != 1)
			return false;
		const float s = mouseScale();
		const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;

		if (ev.press)
		{
			getWindow().focus();                // for the computer keyboard (onKeyboard)
			if (m_midiMenu)
			{
				// A click into the list picks; anywhere else only closes it.
				const int i = midiItemAt(x, y);
				m_midiMenu = false;
				if (i >= 0)
					chooseMidi(i);
				repaint();
				return true;
			}
			if (m_midiOffered && kMidiIn.contains(x, y))
			{
				m_midiMenu = true;
				m_midiHover = -1;
				sendMidiIn(kMidiRescan);        // the list again: a keyboard plugged in since
				repaint();
				return true;
			}
			if (romSetup())
			{
				bool hit = kRomChoose.contains(x, y);
				for (unsigned i = 0; i < kRomSlots; i ++)
					hit = hit || romCell(i).contains(x, y);
				if (hit)
				{
					browseRom();
					return true;
				}
			}
			for (unsigned k = 0; k < kNumKeys; k ++)
			{
				if (romSetup() && k != kKeyPower)
					continue;
				if (keyRect(k).contains(x, y))
				{
					m_pressed = int(k);
					m_keyLit[k] = true;
					sendKey(k, true);
					repaint();
					return true;
				}
			}
			if (kGsReset.contains(x, y) || kGmReset.contains(x, y))
			{
				const std::string msg = std::to_string(++ m_seq) + (kGsReset.contains(x, y) ? " gs" : " gm");
				setState("reset", msg.c_str());
				return true;
			}
			if (kKnob.contains(x, y))
			{
				m_dragKnob = true;
				m_dragY = y;
				m_dragStartDb = m_volumeDb;
				editParameter(kParamVolume, true);
				return true;
			}
			return false;
		}

		if (m_pressed >= 0)
		{
			const uint32_t k = uint32_t(m_pressed);
			m_pressed = -1;
			m_keyLit[k] = false;
			sendKey(k, false);
			repaint();
			return true;
		}
		if (m_dragKnob)
		{
			m_dragKnob = false;
			editParameter(kParamVolume, false);
			return true;
		}
		return false;
	}

	bool onMotion(const MotionEvent &ev) override
	{
		const float s = mouseScale();
		if (m_midiMenu)
		{
			const int i = midiItemAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
			if (i != m_midiHover)
			{
				m_midiHover = i;
				repaint();
			}
			return true;
		}
		if (!m_dragKnob)
			return false;
		const float y = float(ev.pos.getY()) / s;
		const float db = std::fmax(-40.0f, std::fmin(12.0f, m_dragStartDb + (m_dragY - y) * 0.3f));
		m_volumeDb = db;
		setParameterValue(kParamVolume, db);
		repaint();
		return true;
	}

	// The computer keyboard: ← → PART, ↑ ↓ INSTRUMENT, held as long as the key is (the
	// firmware repeats by itself; the system's auto-repeat is not passed on).  Keys with
	// Ctrl, Alt or the Windows key stay the host's.
	bool onKeyboard(const KeyboardEvent &ev) override
	{
		if (ev.mod & (kModifierControl | kModifierAlt | kModifierSuper))
			return false;
		if (m_midiMenu && ev.press && ev.key == kKeyEscape)
		{
			m_midiMenu = false;
			repaint();
			return true;
		}
		if (!ev.press)
		{
			for (unsigned k = 0; k < kNumKeys; k ++)
				if (m_kbdHeld[k] == ev.key)
				{
					m_kbdHeld[k] = 0;
					m_keyLit[k] = false;
					sendKey(k, false);
					repaint();
					return true;
				}
			return false;
		}
		int k = -1;
		switch (ev.key)
		{
		case kKeyLeft:  case kKeyPadLeft:  k = kKeyPartL; break;
		case kKeyRight: case kKeyPadRight: k = kKeyPartR; break;
		case kKeyDown:  case kKeyPadDown:  k = kKeyInstL; break;
		case kKeyUp:    case kKeyPadUp:    k = kKeyInstR; break;
		default: return false;
		}
		if (m_kbdHeld[k] == ev.key)
			return true;                                // auto-repeat
		m_kbdHeld[k] = ev.key;
		m_keyLit[k] = true;
		sendKey(uint32_t(k), true);
		repaint();
		return true;
	}

	bool onScroll(const ScrollEvent &ev) override
	{
		const float s = mouseScale();
		const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
		if (!romSetup() && kLcd.contains(x, y))
		{
			// The wheel over a field: its ◀ ▶ keys, one press a notch (the plugin queues them).
			const float u = kLcd.w / kLcdUnitsW;
			const float col = (x - kLcd.x) / u, row = (y - kLcd.y) / u - kLcdTop;
			for (const Field &f : kFields)
				if (row >= f.row - kLcdTop && row < f.row + 7 * kDot + 4 && col >= f.col - 4
					&& col < f.col + f.count * kCharStep)
				{
					m_wheel += float(ev.delta.getY());
					for (; m_wheel >= 1.0f; m_wheel -= 1.0f)
						tapKey(uint32_t(f.keyL + 1));
					for (; m_wheel <= -1.0f; m_wheel += 1.0f)
						tapKey(uint32_t(f.keyL));
					return true;
				}
			return false;
		}
		if (!kKnob.contains(x, y))
			return false;
		const float db = std::fmax(-40.0f, std::fmin(12.0f, m_volumeDb + float(ev.delta.getY())));
		m_volumeDb = db;
		editParameter(kParamVolume, true);
		setParameterValue(kParamVolume, db);
		editParameter(kParamVolume, false);
		repaint();
		return true;
	}

	void uiIdle() override
	{
		if (!m_frameFitted && std::chrono::steady_clock::now() - m_created < std::chrono::seconds(2))
			fitHostFrame();
		saveSizeWhenSettled();
		if (m_romChecking && std::chrono::steady_clock::now() - m_romAskedAt > std::chrono::seconds(15))
		{
			m_romChecking = false;          // no answer: let the user try again
			repaint();
		}
	}

private:
	// Straight to the DSP, not through the host: see kStateKey in OstrichPlugin.cpp.
	void sendKey(uint32_t k, bool down)
	{
		char msg[16];
		std::snprintf(msg, sizeof(msg), "%u %u", unsigned(k), down ? 1u : 0u);
		setState("key", msg);
	}

	void tapKey(uint32_t k)
	{
		sendKey(k, true);
		sendKey(k, false);
	}

	// ---- drawing -----------------------------------------------------------------------

	void drawLcd()
	{
		// The bezel, then the glass: lit while the firmware has the display on.
		beginPath();
		roundedRect(kLcd.x - 10, kLcd.y - 10, kLcd.w + 20, kLcd.h + 20, 5);
		fillColor(Color(12, 12, 13));
		fill();
		const bool on = (m_panel.flags & 4) != 0;
		beginPath();
		rect(kLcd.x, kLcd.y, kLcd.w, kLcd.h);
		fillColor(on ? kLcdBack : Color(40, 22, 10));
		fill();
		if (!on)
			return;

		save();
		translate(kLcd.x, kLcd.y);
		const float u = kLcd.w / kLcdUnitsW;
		scale(u, u);
		translate(0, kLcdTop);

		// The field names, printed on the glass.
		fontSize(15);
		textAlign(ALIGN_LEFT | ALIGN_BASELINE);
		fillColor(kLcdInk);
		for (const Field &f : kFields)
			text(f.col, f.row - 5, f.name, nullptr);

		// All the dots in two paths: unlit, then lit.
		for (int lit = 0; lit < 2; lit ++)
		{
			beginPath();
			for (const Field &f : kFields)
				for (int i = 0; i < f.count; i ++)
					charDots(m_panel.data[f.at + i], f.row, f.col + float(i) * kCharStep, lit != 0);
			for (int half = 0; half < 2; half ++)
				for (int j = 0; j < 4; j ++)
					barDots(m_panel.data[20 + j + half * 40], kBarRow0 + float(half) * kBarRowStep,
					        kBarCol0 + float(j) * kBarColStep, j == 3 ? 1 : 5, lit != 0);
			markDots(m_panel.data[58], lit != 0);
			fillColor(lit ? kLcdInk : kLcdOff);
			fill();
		}
		restore();
	}

	/// A glyph's rows: the controller's character ROM, or a custom glyph.
	const uint8_t *glyph(uint8_t c) const
	{
		return c >= 16 ? sc55::lcdGlyph(c) : &m_panel.cgram[(c & 7) * 8];
	}

	void charDots(uint8_t c, float row, float col, bool lit)
	{
		const uint8_t *g = glyph(c);
		for (int i = 0; i < 7; i ++)
			for (int j = 0; j < 5; j ++)
				if (((g[i] >> (4 - j)) & 1) == (lit ? 1 : 0))
					rect(col + j * kDot, row + i * kDot, kDotSize, kDotSize);
	}

	void barDots(uint8_t c, float row, float col, int width, bool lit)
	{
		const uint8_t *g = glyph(c);
		for (int i = 0; i < 8; i ++)
			for (int j = 0; j < width; j ++)
				if (((g[i] >> (4 - j)) & 1) == (lit ? 1 : 0))
					rect(col + j * kBarDotW, row + i * kBarDotH, kBarDotSizeW, kBarDotSizeH);
	}

	// Both marks show when bit 0 of the glyph's first row is set (lcd.cpp, LCD_FontRenderLR).
	void markDots(uint8_t c, bool lit)
	{
		if (((glyph(c)[0] & 1) != 0) != lit)
			return;
		for (int letter = 0; letter < 2; letter ++)
		{
			const uint8_t (*m)[11] = letter ? kMarkR : kMarkL;
			const float row = letter ? 232.0f : 70.0f, col = 264.0f;
			for (int i = 0; i < 12; i ++)
				for (int j = 0; j < 11; j ++)
					if (m[i][j])
						rect(col + j, row + i, 1.0f, 1.0f);
		}
	}

	void drawKeys()
	{
		fontSize(10);
		textAlign(ALIGN_CENTER | ALIGN_BASELINE);
		fillColor(Color(200, 200, 204));
		for (unsigned p = 0; p < 8; p ++)
		{
			const Rect l = keyRect(kKeyPartL + 2 * p);
			text(l.x + kKeyW + kPairGap / 2, l.y - 9, kPairNames[p], nullptr);
		}
		for (unsigned k : { unsigned(kKeyAll), unsigned(kKeyMute) })
		{
			const Rect r = keyRect(k);
			text(r.x + r.w / 2, r.y - 9, k == kKeyAll ? "ALL" : "MUTE", nullptr);
		}
		for (unsigned k = kKeyAll; k < kNumKeys; k ++)
		{
			const Rect r = keyRect(k);
			key(r, m_keyLit[k]);
			if (k >= kKeyPartL)
			{
				// ◀ ▶ as triangles: the font's arrows differ between systems.
				const bool right = (k - kKeyPartL) % 2 != 0;
				const float cx = r.x + r.w / 2, cy = r.y + r.h / 2, a = 5.0f;
				beginPath();
				if (right)
				{
					moveTo(cx - a * 0.7f, cy - a);
					lineTo(cx + a * 0.9f, cy);
					lineTo(cx - a * 0.7f, cy + a);
				}
				else
				{
					moveTo(cx + a * 0.7f, cy - a);
					lineTo(cx - a * 0.9f, cy);
					lineTo(cx + a * 0.7f, cy + a);
				}
				closePath();
				fillColor(Color(225, 225, 228));
				fill();
			}
		}
	}

	void key(const Rect &r, bool lit)
	{
		beginPath();
		roundedRect(r.x, r.y, r.w, r.h, 3);
		fillColor(lit ? Color(140, 140, 148) : Color(78, 78, 84));
		fill();
		strokeColor(Color(118, 118, 124));
		strokeWidth(1);
		stroke();
	}

	// POWER: the unit's own soft switch (the firmware reads it as a key); the lamp above it
	// is lit while the display is.
	void drawPower()
	{
		const Rect r = keyRect(kKeyPower);
		fontSize(10);
		textAlign(ALIGN_CENTER | ALIGN_BASELINE);
		fillColor(Color(200, 200, 204));
		text(r.x + r.w / 2, r.y - 9, "POWER", nullptr);
		key(r, m_keyLit[kKeyPower]);
		const bool on = (m_panel.flags & 4) != 0;
		lamp(r.x + r.w / 2, kRow0 + kKeyH / 2, on, Color(255, 60, 40));
	}

	void lamp(float cx, float cy, bool lit, const Color &c)
	{
		const float w = 18, h = 5;
		if (lit)
		{
			beginPath();
			rect(cx - w / 2 - 8, cy - h / 2 - 8, w + 16, h + 16);
			Color glow = c;
			glow.alpha = 0.45f;
			Color none = c;
			none.alpha = 0.0f;
			fillPaint(boxGradient(cx - w / 2, cy - h / 2, w, h, 2, 8, glow, none));
			fill();
		}
		beginPath();
		roundedRect(cx - w / 2, cy - h / 2, w, h, 1.5f);
		Color dark = c;
		dark.red *= 0.25f;
		dark.green *= 0.25f;
		dark.blue *= 0.25f;
		fillColor(lit ? c : dark);
		fill();
		strokeColor(Color(70, 70, 76));
		strokeWidth(1);
		stroke();
	}

	// MIDI MESSAGE: lit while MIDI comes in, under VOLUME.
	void drawMidiLamp()
	{
		const float cx = kKnob.x + kKnob.w / 2, cy = 204;
		lamp(cx, cy, (m_panel.flags & 2) != 0, Color(120, 255, 120));
		fontSize(9);
		textAlign(ALIGN_CENTER | ALIGN_BASELINE);
		fillColor(Color(170, 170, 170));
		text(cx, cy + 20, "MIDI MESSAGE", nullptr);
	}

	void drawKnob()
	{
		const float cx = kKnob.x + kKnob.w / 2, cy = kKnob.y + kKnob.h / 2, r = kKnob.w / 2;
		beginPath();
		circle(cx, cy, r);
		fillColor(Color(48, 48, 52));
		fill();
		strokeColor(Color(96, 96, 102));
		strokeWidth(2);
		stroke();
		// -40 .. +12 dB over 270 degrees, from bottom left.
		const float t = (m_volumeDb + 40.0f) / 52.0f;
		const float a = float(M_PI) * (0.75f + 1.5f * t);
		beginPath();
		moveTo(cx + std::cos(a) * r * 0.3f, cy + std::sin(a) * r * 0.3f);
		lineTo(cx + std::cos(a) * r * 0.9f, cy + std::sin(a) * r * 0.9f);
		strokeColor(Color(240, 240, 240));
		strokeWidth(3);
		stroke();
		fontSize(11);
		textAlign(ALIGN_CENTER | ALIGN_BASELINE);
		fillColor(Color(200, 200, 204));
		text(cx, kKnob.y - 10, "VOLUME", nullptr);
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%+.1f dB", double(m_volumeDb));
		fillColor(Color(140, 140, 140));
		text(cx, kKnob.y + kKnob.h + 18, buf, nullptr);
	}

	// What the unit does not have, under its front.
	void drawBar()
	{
		beginPath();
		rect(0, kBarY, kW, kBarH);
		fillColor(Color(14, 14, 16));
		fill();
		beginPath();
		rect(0, kBarY, kW, 1);
		fillColor(Color(52, 52, 56));
		fill();

		barButton(kGsReset, "GS RESET", false);
		barButton(kGmReset, "GM RESET", false);
		if (m_midiOffered)
			drawMidiIn();

		const float w = (m_midiOffered ? kMidiIn.x : kGsReset.x) - 24 - 16;
		fontSize(11);
		textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
		fillColor(m_panel.flags & 1 ? Color(140, 140, 140) : Color(240, 170, 60));
		text(16, kBarY + kBarH / 2 + 1, fit(m_status, w, 11).c_str(), nullptr);
	}

	// A control in the bar: dark with a thin edge, as a host's own; lit when on.
	void barButton(const Rect &r, const char *label, bool lit)
	{
		beginPath();
		roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 3);
		fillColor(lit ? Color(255, 140, 40) : Color(30, 30, 33));
		fill();
		strokeColor(lit ? Color(255, 140, 40) : Color(84, 84, 90));
		strokeWidth(1);
		stroke();
		fontSize(10);
		textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
		fillColor(lit ? Color(40, 20, 0) : Color(200, 200, 204));
		text(r.x + r.w / 2, r.y + r.h / 2 + 1, label, nullptr);
	}

	// ---- MIDI IN (the Windows standalone) ------------------------------------------------

	void setMidiPorts(const char *value)
	{
		std::vector<std::string> part(1);
		for (const char *c = value; *c; c ++)
		{
			if (*c == kFieldSep)
				part.emplace_back();
			else
				part.back() += *c;
		}
		m_midiOffered = part.size() >= 2;
		if (!m_midiOffered)
		{
			m_midiMenu = false;
			return;
		}
		m_midiChoice = part[0];
		m_midiProblem = part[1];
		m_midiPortList.assign(part.begin() + 2, part.end());
	}

	void sendMidiIn(const std::string &choice)
	{
		const std::string msg = std::to_string(++ m_seq) + " " + choice;
		setState("midiin", msg.c_str());
	}

	// The list: Off, All inputs, then the ports.
	int midiItems() const
	{
		return std::min(int(m_midiPortList.size()) + 2, kMenuMaxItems);
	}

	int midiRows() const { return std::min(midiItems(), kMenuRows); }
	int midiCols() const { return (midiItems() + kMenuRows - 1) / kMenuRows; }

	Rect midiItem(int i) const
	{
		const float left = std::min(kMidiIn.x, kW - 8.0f - float(midiCols()) * kMenuColW);
		const float top = kBarY - 4.0f - float(midiRows()) * kMenuItemH;
		return { left + float(i / kMenuRows) * kMenuColW, top + float(i % kMenuRows) * kMenuItemH,
		         kMenuColW, kMenuItemH };
	}

	int midiItemAt(float x, float y) const
	{
		for (int i = 0; i < midiItems(); i ++)
			if (midiItem(i).contains(x, y))
				return i;
		return -1;
	}

	std::string midiItemChoice(int i) const
	{
		return i == 0 ? std::string() : i == 1 ? std::string(kMidiAll) : m_midiPortList[size_t(i - 2)];
	}

	void chooseMidi(int i)
	{
		const std::string c = midiItemChoice(i);
		m_midiChoice = c;                   // shown at once; the plugin's answer follows
		m_midiProblem.clear();
		sendMidiIn(c);
	}

	static std::string midiLabel(const std::string &choice)
	{
		return choice.empty() ? "Off" : choice == kMidiAll ? "All inputs" : choice;
	}

	void drawMidiIn()
	{
		const Rect &r = kMidiIn;
		beginPath();
		roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 3);
		fillColor(Color(30, 30, 33));
		fill();
		strokeColor(m_midiMenu ? Color(255, 140, 40) : Color(84, 84, 90));
		strokeWidth(1);
		stroke();

		textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
		fontSize(9);
		fillColor(Color(150, 150, 156));
		text(r.x + 8, r.y + r.h / 2 + 1, "MIDI IN", nullptr);
		std::string t = midiLabel(m_midiChoice);
		if (!m_midiProblem.empty())
			t += " \xe2\x80\x94 " + m_midiProblem;
		fillColor(m_midiProblem.empty() ? Color(220, 220, 224) : Color(240, 170, 60));
		text(r.x + 52, r.y + r.h / 2 + 1, fit(t, r.w - 52 - 22, 10).c_str(), nullptr);
		// ▾
		beginPath();
		moveTo(r.x + r.w - 15, r.y + r.h / 2 - 2);
		lineTo(r.x + r.w - 7, r.y + r.h / 2 - 2);
		lineTo(r.x + r.w - 11, r.y + r.h / 2 + 3);
		closePath();
		fillColor(Color(200, 200, 204));
		fill();
	}

	void drawMidiMenu()
	{
		const int n = midiItems();
		const Rect top = midiItem(0);
		beginPath();
		roundedRect(top.x - 1, top.y - 1, float(midiCols()) * kMenuColW + 2, float(midiRows()) * kMenuItemH + 2, 3);
		fillColor(Color(24, 24, 27));
		fill();
		strokeColor(Color(255, 140, 40));
		strokeWidth(1);
		stroke();
		for (int i = 0; i < n; i ++)
		{
			const Rect r = midiItem(i);
			if (i == m_midiHover)
			{
				beginPath();
				rect(r.x, r.y, r.w, r.h);
				fillColor(Color(60, 44, 24));
				fill();
			}
			const std::string c = midiItemChoice(i);
			const bool current = c == m_midiChoice;
			textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
			fontSize(11);
			fillColor(current ? Color(255, 140, 40) : Color(220, 220, 224));
			if (current)
				text(r.x + 8, r.y + r.h / 2 + 1, "\xe2\x9c\x93", nullptr);
			text(r.x + 26, r.y + r.h / 2 + 1, fit(midiLabel(c), r.w - 34, 11).c_str(), nullptr);
			if (i == 1)
			{
				beginPath();                // the ports below a line
				rect(r.x + 6, r.y + r.h - 0.5f, r.w - 12, 1);
				fillColor(Color(60, 60, 66));
				fill();
			}
		}
	}

	// Cut text to a width, with an ellipsis.
	std::string fit(const std::string &t, float w, float size)
	{
		fontSize(size);
		std::string s = t;
		Rectangle<float> b;
		while (!s.empty() && textBounds(0, 0, (s + (s.size() < t.size() ? "\xe2\x80\xa6" : "")).c_str(), nullptr, b) > w)
			s.pop_back();
		return s.size() < t.size() ? s + "\xe2\x80\xa6" : s;
	}

	// ---- ROM setup -------------------------------------------------------------------------

	void browseRom()
	{
		if (m_romChecking)
			return;
		FileBrowserOptions opts;
		opts.title = "Pick an SC-55 ROM image or a zip -- the rest of its folder is searched too";
		opts.startDir = m_romDir.empty() ? nullptr : m_romDir.c_str();
		m_browsing = true;
		if (!openFileBrowser(opts))
			m_browsing = false;
	}

	void setRomSet(const char *value)
	{
		std::string part[kRomSlots + 2];
		unsigned k = 0;
		for (const char *c = value; *c; c ++)
		{
			if (*c == kFieldSep) { if (++ k >= kRomSlots + 2) break; continue; }
			part[k] += *c;
		}
		m_romsAll = k >= kRomSlots;
		for (unsigned s = 0; s < kRomSlots; s ++)
		{
			m_romWhat[s] = part[s];
			m_romsAll = m_romsAll && !part[s].empty();
		}
		m_romVersion = part[kRomSlots];
		m_romTarget = part[kRomSlots + 1];
		m_romSetSeen = k >= kRomSlots;
		m_romChecking = false;
	}

	// Only once the plugin has said what is missing, and not once the machine runs.
	bool romSetup() const
	{
		return m_romSetSeen && !m_romsAll && !(m_panel.flags & 1);
	}

	void drawRomSetup()
	{
		const Rect &b = kRomBox;
		beginPath();
		roundedRect(b.x, b.y, b.w, b.h, 4);
		fillColor(Color(14, 14, 16));
		fill();
		strokeColor(Color(70, 70, 76));
		strokeWidth(1);
		stroke();

		textAlign(ALIGN_LEFT | ALIGN_BASELINE);
		fontSize(15);
		fillColor(Color(255, 140, 40));
		text(132, 80, "ROM IMAGES NEEDED", nullptr);
		fontSize(11);
		textLineHeight(1.35f);
		fillColor(Color(190, 190, 194));
		textBox(132, 102, 440,
			"Ostrich 55 runs the SC-55's own firmware on Nuked-SC55 and needs dumps of its ROM "
			"chips. Click CHOOSE FILE and pick any one of the images, or a zip with them: the rest "
			"of that folder is searched too, by contents, not names. They are copied to:", nullptr);
		fillColor(Color(140, 140, 146));
		text(132, 172, fit(m_romTarget, 440, 11).c_str(), nullptr);
		textLineHeight(1.0f);

		barButton(kRomChoose, m_romChecking ? "CHECKING\xe2\x80\xa6" : "CHOOSE FILE\xe2\x80\xa6", !m_romChecking);
		fontSize(10);
		textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
		fillColor(Color(110, 110, 116));
		text(kRomChoose.x + kRomChoose.w + 14, kRomChoose.y + kRomChoose.h / 2 + 1,
		     "SC-55 (mk1) v1.00, 1.10, 1.20, 1.21 or 2.00  \xc2\xb7  roms/README.md lists them", nullptr);

		for (unsigned i = 0; i < kRomSlots; i ++)
		{
			const Rect r = romCell(i);
			const bool ok = !m_romWhat[i].empty();
			// A chip: dark body, a notch, pins along both long edges.
			fillColor(Color(120, 120, 126));
			for (float px = r.x + 10; px < r.x + r.w - 8; px += 10)
			{
				beginPath();
				rect(px, r.y - 3, 4, 3);
				rect(px, r.y + r.h, 4, 3);
				fill();
			}
			beginPath();
			roundedRect(r.x, r.y, r.w, r.h, 2);
			fillColor(ok ? Color(44, 32, 16) : Color(28, 28, 31));
			fill();
			strokeColor(ok ? Color(255, 140, 40) : Color(110, 110, 116));
			strokeWidth(1);
			stroke();
			beginPath();
			circle(r.x + 8, r.y + 8, 2.5f);
			fillColor(Color(10, 10, 12));
			fill();

			textAlign(ALIGN_LEFT | ALIGN_BASELINE);
			fontSize(8);
			fillColor(Color(130, 130, 136));
			text(r.x + 16, r.y + 15, kRomChips[i], nullptr);
			fontSize(12);
			fillColor(Color(225, 225, 228));
			text(r.x + 10, r.y + 38, kRomTitles[i], nullptr);
			fontSize(10);
			fillColor(ok ? Color(255, 140, 40) : Color(150, 150, 156));
			const std::string state = ok ? "found" : std::string(kRomSizes[i]) + " \xc2\xb7 missing";
			text(r.x + 10, r.y + 57, state.c_str(), nullptr);
		}
	}

	static std::string dirOf(const std::string &p)
	{
		const size_t cut = p.find_last_of("/\\");
		return cut == std::string::npos ? std::string() : p.substr(0, cut);
	}

	// ---- size --------------------------------------------------------------------------

	// The panel's size lives in the plugin's state, so the host saves it with the session.
	// Unscaled -- window pixels over the desktop scale factor -- so that it opens the same on
	// a screen with other scaling.
	void restoreSize(const char *value)
	{
		unsigned w = 0, h = 0;
		if (std::sscanf(value, "%u %u", &w, &h) != 2 || w < 100 || h < 20)
			return;
		if (std::fabs(double(w) / h - kW / kH) > 0.05)
			h = unsigned(std::lround(w * kH / kW));
		m_sizeSaved = value;
		const double scale = getWindow().getScaleFactor();
		const uint pw = uint(std::lround(w * scale)), ph = uint(std::lround(h * scale));
		if (std::abs(int(pw) - int(getWindow().getWidth())) > 2 || std::abs(int(ph) - int(getWindow().getHeight())) > 2)
			getWindow().setSize(pw, ph);
	}

	// Sends the size once it has stood still for a second -- not while the user drags, and
	// not in the first seconds, while the host still settles the window.
	void saveSizeWhenSettled()
	{
		const auto now = std::chrono::steady_clock::now();
		const uint pw = getWindow().getWidth(), ph = getWindow().getHeight();
		if (pw != m_sizeSeenW || ph != m_sizeSeenH)
		{
			m_sizeSeenW = pw;
			m_sizeSeenH = ph;
			m_sizeSeenSince = now;
			return;
		}
		if (now - m_created < std::chrono::seconds(3) || now - m_sizeSeenSince < std::chrono::seconds(1))
			return;
		const double scale = getWindow().getScaleFactor();
		char value[32];
		std::snprintf(value, sizeof(value), "%u %u", unsigned(std::lround(pw / scale)), unsigned(std::lround(ph / scale)));
		if (m_sizeSaved == value)
			return;
		unsigned sw = 0, sh = 0;
		if (std::sscanf(m_sizeSaved.c_str(), "%u %u", &sw, &sh) == 2
				&& std::abs(int(sw * scale) - int(pw)) <= 2 && std::abs(int(sh * scale) - int(ph)) <= 2)
			return;                     // the host's rounding, not a resize
		m_sizeSaved = value;
		setState("uisize", value);
	}

	// Cubase at 125 % sizes a new view for scale 1.0 inside a window made for 125 %
	// (Ostrich 220): a view below its least size asks the host for it, once.
	void fitHostFrame()
	{
		if (std::chrono::steady_clock::now() - m_created < std::chrono::milliseconds(200))
			return;
		m_frameFitted = true;
		bool keep = false;
		const Size<uint> least = getWindow().getGeometryConstraints(keep);
		const double scale = getWindow().getScaleFactor();
		const uint lw = uint(std::lround(least.getWidth() * scale)), lh = uint(std::lround(least.getHeight() * scale));
		if (getWindow().getWidth() + 2 < lw || getWindow().getHeight() + 2 < lh)
			getWindow().setSize(lw, lh);
	}

	// Mouse events come in the window's own pixels; the panel is drawn in design units
	// scaled to the window's width.
	float mouseScale() const
	{
		return float(getWindow().getWidth()) / kW;
	}

	PanelBlob m_panel;
	std::string m_status;
	bool m_browsing = false;
	unsigned m_seq = 0;                         // makes the same request twice two requests
	std::string m_romWhat[kRomSlots], m_romVersion, m_romTarget, m_romDir;
	bool m_romSetSeen = false, m_romsAll = false, m_romChecking = false;
	std::chrono::steady_clock::time_point m_romAskedAt {};
	const std::chrono::steady_clock::time_point m_created = std::chrono::steady_clock::now();
	std::string m_sizeSaved;                    // "<w> <h>" unscaled, as in the plugin state
	uint m_sizeSeenW = 0, m_sizeSeenH = 0;
	std::chrono::steady_clock::time_point m_sizeSeenSince {};
	bool m_frameFitted = false;
	float m_volumeDb = 0.0f;
	bool m_keyLit[kNumKeys] = {};
	uint m_kbdHeld[kNumKeys] = {};
	float m_wheel = 0.0f;
	int m_pressed = -1;
	bool m_dragKnob = false;
	float m_dragY = 0, m_dragStartDb = 0;
	bool m_midiOffered = false, m_midiMenu = false;     // MIDI IN, the Windows standalone only
	std::string m_midiChoice, m_midiProblem;
	std::vector<std::string> m_midiPortList;
	int m_midiHover = -1;

	DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstrichUI)
};

UI *createUI()
{
	return new OstrichUI();
}

END_NAMESPACE_DISTRHO
