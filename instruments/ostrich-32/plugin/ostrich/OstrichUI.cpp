// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 32 -- the panel: the MT-32's front after the unit (the LCD with its 20
// characters, the MIDI MESSAGE lamp, PART 1-5 and RHYTHM, SOUND GROUP, SOUND, VOLUME,
// MASTER VOLUME, the VOLUME knob), not a copy of it, and under it a bar for what the unit
// does not have (status line, the control ROM, MIDI IN).  Every switch and the knob go to
// the firmware; the display is what the firmware wrote (docs/PANEL.md).  Size handling,
// the bar and the ROM setup follow Ostrich 55's panel.
//
// This is the plain first panel: the LCD is drawn with a font, not yet as the unit's
// dot matrix, and the body is not yet the unit's.

#include "DistrhoUI.hpp"
#include "DistrhoStandaloneUtils.hpp"
#include "OstrichShared.hpp"
#include "../../../../shared/plugin/PanelSize.hpp"
#include "../../../../shared/plugin/StandaloneBar.hpp"

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

using Rect = ostrich::BarRect;

// The LCD: 20 characters in one row, behind a bezel.
const Rect kLcd { 112.0f, 92.0f, 560.0f, 76.0f };
constexpr float kCharW = 25.0f, kCharH = 40.0f;
const Color kLcdBack(150, 196, 40), kLcdOff(78, 98, 30), kLcdInk(22, 38, 8);

// The switches: PART 1 2 3 / 4 5 RHYTHM, then SOUND GROUP SOUND / VOLUME MASTER VOLUME.
constexpr float kKeyW = 56.0f, kKeyH = 22.0f;
constexpr float kRow0 = 100.0f, kRow1 = 160.0f;
Rect keyRect(uint32_t k)
{
	if (k < kKeySoundGroup)
		return { 730.0f + float(k % 3) * 76.0f, k < 3 ? kRow0 : kRow1, kKeyW, kKeyH };
	const uint32_t i = k - kKeySoundGroup;
	return { 980.0f + float(i % 2) * 96.0f, i < 2 ? kRow0 : kRow1, kKeyW + 20.0f, kKeyH };
}

const Rect kKnob { 1218.0f, 98.0f, 72.0f, 72.0f };

// The bar, from the right: CONTROL ROM, PART = CH, in the Windows standalone MIDI IN and
// BUFFER; the status line takes the rest.  The lists open upwards, over the panel.
constexpr float kBarBtnH = 18.0f, kBarBtnY = kBarY + (kBarH - kBarBtnH) / 2;
const Rect kMachineBtn { kW - 16.0f - 220.0f, kBarBtnY, 220.0f, kBarBtnH };
const Rect kPartChBtn { kMachineBtn.x - 12.0f - 92.0f, kBarBtnY, 92.0f, kBarBtnH };
const Rect kMidiIn  { kPartChBtn.x - 16.0f - 240.0f, kBarBtnY, 240.0f, kBarBtnH };
const Rect kBufferBtn { kMidiIn.x - 8.0f - 150.0f, kBarBtnY, 150.0f, kBarBtnH };

// While ROM images are missing they take the place of the display and the switches: what
// is needed, a chip per image, and a button that asks for a file -- any one of them; the
// rest of its folder is found too.
const Rect kRomBox    { 112.0f, 50.0f, 1060.0f, 186.0f };
const Rect kRomChoose { 132.0f, 194.0f, 150.0f, 26.0f };
inline Rect romCell(unsigned i)
{
	return { 620.0f + float(i) * 170.0f, 92.0f, 150.0f, 70.0f };
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
		m_panel.knob = uint8_t(kKnobSteps);
		m_bar.place(kMidiIn, kBufferBtn);
	}

protected:
	void parameterChanged(uint32_t index, float value) override
	{
		if (index >= kParamKeyFirst && index < kParamKeyEnd)
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
			m_size.restore(value);
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
		else if (std::strcmp(key, "machines") == 0)
		{
			setMachines(value);
			repaint();
		}
		else if (std::strcmp(key, "machine") == 0)
		{
			m_wantMachine = value;
			repaint();
		}
		else if (std::strcmp(key, "midiports") == 0)
		{
			m_bar.setMidiPorts(value);
			repaint();
		}
		else if (std::strcmp(key, "partch") == 0)
		{
			m_partCh = std::strcmp(value, "0") != 0;
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
		fillColor(Color(34, 34, 36));
		fill();
		beginPath();
		rect(0, 0, kW, 4);
		fillColor(Color(70, 70, 74));
		fill();

		fontFace(NANOVG_DEJAVU_SANS_TTF);
		textAlign(ALIGN_LEFT | ALIGN_BASELINE);
		fillColor(Color(235, 235, 235));
		fontSize(24);
		text(24, 40, "OSTRICH 32", nullptr);
		fontSize(11);
		fillColor(Color(170, 170, 170));
		text(178, 40, "MULTI-TIMBRE SOUND MODULE  \xc2\xb7  MT-32 EMULATION", nullptr);

		drawKnob();
		if (romSetup())
			drawRomSetup();
		else
		{
			drawLcd();
			drawMidiLamp();
			drawKeys();
		}
		drawBar();
		m_bar.drawList();
	}

	bool onMouse(const MouseEvent &ev) override
	{
		if (ev.button != 1)
			return false;
		const float s = m_size.mouseScale();
		const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;

		if (ev.press)
		{
			getWindow().focus();                // for the computer keyboard (onKeyboard)
			if (m_bar.press(x, y))
				return true;
			if (machineOffered() && kMachineBtn.contains(x, y))
			{
				m_bar.open(m_machineList, kMachineBtn, kMachineBtn.w);
				return true;
			}
			if (kPartChBtn.contains(x, y))
			{
				m_partCh = !m_partCh;
				setState("partch", m_partCh ? "1" : "0");
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
				return false;
			}
			for (unsigned k = 0; k < kNumKeys; k ++)
				if (keyRect(k).contains(x, y))
				{
					m_pressed = int(k);
					m_keyLit[k] = true;
					sendKey(k, true);
					repaint();
					return true;
				}
			if (kKnob.contains(x, y))
			{
				m_dragKnob = true;
				m_dragY = y;
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
			return true;
		}
		return false;
	}

	bool onMotion(const MotionEvent &ev) override
	{
		const float s = m_size.mouseScale();
		if (m_bar.motion(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s))
			return true;
		if (!m_dragKnob)
			return false;
		// Up turns clockwise: a step every 2 design units (the whole travel in 200).
		const float y = float(ev.pos.getY()) / s;
		const int steps = int((m_dragY - y) / 2.0f);
		if (steps != 0)
		{
			turnKnob(steps);
			m_dragY -= float(steps) * 2.0f;
		}
		return true;
	}

	// The computer keyboard: ↑ → turn the knob clockwise, ↓ ← anticlockwise, a step a press
	// (the system's auto-repeat too).  Keys with Ctrl, Alt or the Windows key stay the
	// host's.
	bool onKeyboard(const KeyboardEvent &ev) override
	{
		if (ev.mod & (kModifierControl | kModifierAlt | kModifierSuper))
			return false;
		if (ev.press && ev.key == kKeyEscape && m_bar.escape())
			return true;
		if (!ev.press || romSetup())
			return false;
		switch (ev.key)
		{
		case kKeyUp: case kKeyRight: turnKnob(1); return true;
		case kKeyDown: case kKeyLeft: turnKnob(-1); return true;
		default: return false;
		}
	}

	bool onScroll(const ScrollEvent &ev) override
	{
		const float s = m_size.mouseScale();
		const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
		// The wheel over the knob or the display turns the knob, a step a notch.
		if (romSetup() || !(kKnob.contains(x, y) || kLcd.contains(x, y)))
			return false;
		m_wheel += float(ev.delta.getY());
		int steps = 0;
		for (; m_wheel >= 1.0f; m_wheel -= 1.0f) steps ++;
		for (; m_wheel <= -1.0f; m_wheel += 1.0f) steps --;
		if (steps != 0)
			turnKnob(steps);
		return true;
	}

	void uiIdle() override
	{
		m_size.idle();
		m_bar.idle();
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

	void turnKnob(int steps)
	{
		char msg[32];
		std::snprintf(msg, sizeof(msg), "%u %d", ++ m_seq, steps);
		setState("knob", msg);
	}

	// ---- drawing -----------------------------------------------------------------------

	void drawLcd()
	{
		// The bezel, then the glass: lit while a machine runs.
		const Rect &r = kLcd;
		beginPath();
		roundedRect(r.x - 10, r.y - 10, r.w + 20, r.h + 20, 4);
		fillColor(Color(16, 16, 18));
		fill();
		const bool on = (m_panel.flags & 1) != 0;
		beginPath();
		rect(r.x, r.y, r.w, r.h);
		fillColor(on ? kLcdBack : kLcdOff);
		fill();
		if (on)
		{
			// The backlight is brightest in the middle.
			beginPath();
			rect(r.x, r.y, r.w, r.h);
			Color hi = Color(200, 240, 90), none = hi;
			hi.alpha = 0.35f;
			none.alpha = 0.0f;
			fillPaint(boxGradient(r.x + 40, r.y + 14, r.w - 80, r.h - 28, 20, 50, hi, none));
			fill();
		}

		// The characters, each in its own cell, as the controller's 5 x 8 cells.
		const float x0 = r.x + (r.w - 20 * kCharW) / 2, y0 = r.y + (r.h - kCharH) / 2;
		for (int i = 0; i < 20; i ++)
		{
			const float cx = x0 + float(i) * kCharW;
			// The unlit dots of a cell show faintly on the unit's glass.
			beginPath();
			rect(cx + 2, y0, kCharW - 4, kCharH);
			fillColor(on ? Color(140, 184, 36) : Color(72, 92, 28));
			fill();
		}
		if (!on)
			return;
		fontSize(kCharH * 0.9f);
		textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
		fillColor(kLcdInk);
		for (int i = 0; i < 20; i ++)
		{
			const unsigned char c = static_cast<unsigned char>(m_panel.text[i]);
			if (c <= ' ' || c >= 0x7F)
				continue;
			const char t[2] = { char(c), '\0' };
			text(x0 + (float(i) + 0.5f) * kCharW, y0 + kCharH / 2 + 1, t, nullptr);
		}
	}

	void drawKeys()
	{
		fontSize(9);
		for (uint32_t k = 0; k < kNumKeys; k ++)
		{
			const Rect r = keyRect(k);
			textAlign(ALIGN_CENTER | ALIGN_BASELINE);
			fillColor(Color(200, 200, 204));
			text(r.x + r.w / 2, r.y - 8, kKeyNames[k], nullptr);
			beginPath();
			roundedRect(r.x, r.y, r.w, r.h, 3);
			fillPaint(linearGradient(r.x, r.y, r.x, r.y + r.h,
				m_keyLit[k] ? Color(120, 120, 126) : Color(92, 92, 98),
				m_keyLit[k] ? Color(90, 90, 96) : Color(58, 58, 62)));
			fill();
			strokeColor(Color(20, 20, 22));
			strokeWidth(1);
			stroke();
		}
	}

	void lamp(float cx, float cy, bool lit, const Color &c)
	{
		const float rad = 4.0f;
		if (lit)
		{
			beginPath();
			circle(cx, cy, rad + 10);
			Color glow = c, none = c;
			glow.alpha = 0.45f;
			none.alpha = 0.0f;
			fillPaint(radialGradient(cx, cy, rad, rad + 10, glow, none));
			fill();
		}
		beginPath();
		circle(cx, cy, rad);
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

	// MIDI MESSAGE: under the display, at its right end, as the firmware lights it.
	void drawMidiLamp()
	{
		const float cx = kLcd.x + kLcd.w - 6, cy = kLcd.y + kLcd.h + 30;
		lamp(cx, cy, (m_panel.flags & 2) != 0, Color(255, 70, 40));
		fontSize(9);
		textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
		fillColor(Color(170, 170, 170));
		text(cx - 12, cy + 1, "MIDI MESSAGE", nullptr);
	}

	// The VOLUME knob: the firmware's pot, 0 .. kKnobSteps over 300 degrees from bottom
	// left.  It moves when the plugin has turned it.
	void drawKnob()
	{
		const float cx = kKnob.x + kKnob.w / 2, cy = kKnob.y + kKnob.h / 2, r = kKnob.w / 2;
		fontSize(11);
		textAlign(ALIGN_CENTER | ALIGN_BASELINE);
		fillColor(Color(200, 200, 204));
		text(cx, kKnob.y - 12, "VOLUME", nullptr);
		// The scale around it.
		for (int i = 0; i <= 10; i ++)
		{
			const float a = float(M_PI) * (2.0f / 3.0f + 5.0f / 3.0f * float(i) / 10.0f);
			beginPath();
			moveTo(cx + std::cos(a) * (r + 4), cy + std::sin(a) * (r + 4));
			lineTo(cx + std::cos(a) * (r + 9), cy + std::sin(a) * (r + 9));
			strokeColor(Color(130, 130, 136));
			strokeWidth(1.5f);
			stroke();
		}
		beginPath();
		circle(cx, cy, r);
		fillPaint(radialGradient(cx - r * 0.3f, cy - r * 0.3f, r * 0.1f, r * 1.2f, Color(84, 84, 90), Color(36, 36, 40)));
		fill();
		strokeColor(Color(16, 16, 18));
		strokeWidth(2);
		stroke();
		const float t = float(std::min<int>(m_panel.knob, kKnobSteps)) / float(kKnobSteps);
		const float a = float(M_PI) * (2.0f / 3.0f + 5.0f / 3.0f * t);
		beginPath();
		moveTo(cx + std::cos(a) * r * 0.35f, cy + std::sin(a) * r * 0.35f);
		lineTo(cx + std::cos(a) * r * 0.9f, cy + std::sin(a) * r * 0.9f);
		strokeColor(Color(240, 240, 240));
		strokeWidth(3);
		stroke();
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

		if (machineOffered())
			m_bar.field(kMachineBtn, "CONTROL ROM", 82, machineLabel(), m_bar.isOpen(m_machineList), false);
		drawPartCh();
		m_bar.drawFields();

		const float w = m_bar.leftEdge(kPartChBtn.x) - 24 - 16;
		fontSize(11);
		textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
		fillColor(m_panel.flags & 1 ? Color(140, 140, 140) : Color(240, 170, 60));
		const std::string status = m_panel.flags & 4 ? std::string("Starting\xe2\x80\xa6") : m_status;
		text(16, kBarY + kBarH / 2 + 1, m_bar.fit(status, w, 11).c_str(), nullptr);
	}

	// PART = CH: a box to tick.  On, channels 1-8 play parts 1-8 (the plugin moves them to
	// the unit's 2-9); off, the unit hears the channels as they come.
	void drawPartCh()
	{
		const Rect &r = kPartChBtn;
		beginPath();
		roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 3);
		fillColor(Color(30, 30, 33));
		fill();
		strokeColor(Color(84, 84, 90));
		strokeWidth(1);
		stroke();

		constexpr float b = 10.0f;
		const float bx = r.x + 8, by = r.y + (r.h - b) / 2;
		beginPath();
		roundedRect(bx, by, b, b, 2);
		if (m_partCh)
		{
			fillColor(Color(255, 140, 40));
			fill();
			beginPath();
			moveTo(bx + 2.2f, by + 5.2f);
			lineTo(bx + 4.3f, by + 7.4f);
			lineTo(bx + 7.9f, by + 2.6f);
			strokeColor(Color(40, 20, 0));
			strokeWidth(1.6f);
			stroke();
		}
		else
		{
			strokeColor(Color(150, 150, 156));
			strokeWidth(1);
			stroke();
		}
		fontSize(9);
		textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
		fillColor(m_partCh ? Color(220, 220, 224) : Color(150, 150, 156));
		text(bx + b + 8, r.y + r.h / 2 + 1, "PART = CH", nullptr);
	}

	// ---- the control ROM: the machines the images make ---------------------------------

	void setMachines(const char *value)
	{
		std::vector<std::string> part(1);
		for (const char *c = value; *c; c ++)
		{
			if (*c == kFieldSep)
				part.emplace_back();
			else
				part.back() += *c;
		}
		m_running = part[0];
		m_machineIds.clear();
		m_machineNames.clear();
		for (size_t i = 1; i + 1 < part.size(); i += 2)
		{
			m_machineIds.push_back(part[i]);
			m_machineNames.push_back(part[i + 1]);
		}
		m_machinesSeen = true;
	}

	bool machineOffered() const { return !m_machineIds.empty(); }

	std::string machineLabel() const
	{
		const std::string &id = m_running.empty() ? m_wantMachine : m_running;
		for (size_t i = 0; i < m_machineIds.size(); i ++)
			if (m_machineIds[i] == id)
				return m_machineNames[i];
		return "\xe2\x80\x94";
	}

	// ---- ROM setup -------------------------------------------------------------------------

	void browseRom()
	{
		if (m_romChecking)
			return;
		FileBrowserOptions opts;
		opts.title = "Pick an MT-32 ROM image or a zip -- the rest of its folder is searched too";
		opts.startDir = m_romDir.empty() ? nullptr : m_romDir.c_str();
		m_browsing = true;
		if (!openFileBrowser(opts))
			m_browsing = false;
	}

	void setRomSet(const char *value)
	{
		std::string part[kRomSlots + 1];
		unsigned k = 0;
		for (const char *c = value; *c; c ++)
		{
			if (*c == kFieldSep) { if (++ k >= kRomSlots + 1) break; continue; }
			part[k] += *c;
		}
		for (unsigned s = 0; s < kRomSlots; s ++)
			m_romWhat[s] = part[s];
		m_romTarget = part[kRomSlots];
		m_romSetSeen = k >= kRomSlots;
		m_romChecking = false;
	}

	// Only once the plugin has said what is there, and while no machine can run.
	bool romSetup() const
	{
		return m_romSetSeen && m_machinesSeen && m_machineIds.empty() && !(m_panel.flags & 1);
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
		textBox(132, 102, 450,
			"Ostrich 32 runs the MT-32's own firmware on 88emu and needs dumps of its ROM chips. "
			"Click CHOOSE FILE and pick any one of the images, or a zip with them: the rest of "
			"that folder is searched too, by contents, not names. They are copied to:", nullptr);
		fillColor(Color(140, 140, 146));
		text(132, 172, m_bar.fit(m_romTarget, 450, 11).c_str(), nullptr);
		textLineHeight(1.0f);

		m_bar.button(kRomChoose, m_romChecking ? "CHECKING\xe2\x80\xa6" : "CHOOSE FILE\xe2\x80\xa6", !m_romChecking);
		fontSize(10);
		textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
		fillColor(Color(110, 110, 116));
		text(kRomChoose.x + kRomChoose.w + 14, kRomChoose.y + kRomChoose.h / 2 + 1,
		     "MT-32 (old board): control ROM 1.04 \xe2\x80\x93 1.07, Blue Ridge or M-9  \xc2\xb7  roms/README.md lists them", nullptr);

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

	PanelBlob m_panel;
	std::string m_status;
	bool m_browsing = false;
	unsigned m_seq = 0;                         // makes the same request twice two requests
	std::string m_romWhat[kRomSlots], m_romTarget, m_romDir;
	bool m_romSetSeen = false, m_romChecking = false;
	std::chrono::steady_clock::time_point m_romAskedAt {};
	std::string m_running, m_wantMachine;       // machine ids
	std::vector<std::string> m_machineIds, m_machineNames;
	bool m_machinesSeen = false;
	bool m_partCh = true;                       // PART = CH, as the plugin has it
	PanelSize m_size { *this, kW, kH };
	StandaloneBar m_bar { *this, "Ostrich 32", kBarY, kW };

	// CONTROL ROM's list: the machines the images make, then "Add ROM images".
	struct MachineList : BarList
	{
		OstrichUI &ui;
		explicit MachineList(OstrichUI &u) : ui(u) {}
		int count() const override { return int(ui.m_machineIds.size()) + 1; }
		std::string label(int i) const override
		{
			return size_t(i) < ui.m_machineIds.size() ? ui.m_machineNames[size_t(i)] : "Add ROM imagesâ¦";
		}
		bool current(int i) const override
		{
			return size_t(i) < ui.m_machineIds.size() && ui.m_machineIds[size_t(i)] == ui.m_running;
		}
		bool ruleAfter(int i) const override { return i == int(ui.m_machineIds.size()) - 1; }
		void choose(int i) override
		{
			if (size_t(i) >= ui.m_machineIds.size())
				ui.browseRom();
			else if (ui.m_machineIds[size_t(i)] != ui.m_running)
			{
				ui.m_wantMachine = ui.m_machineIds[size_t(i)];
				ui.setState("machine", ui.m_wantMachine.c_str());
			}
		}
	};
	MachineList m_machineList { *this };
	bool m_keyLit[kNumKeys] = {};
	float m_wheel = 0.0f;
	int m_pressed = -1;
	bool m_dragKnob = false;
	float m_dragY = 0;

	DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstrichUI)
};

UI *createUI()
{
	return new OstrichUI();
}

END_NAMESPACE_DISTRHO
