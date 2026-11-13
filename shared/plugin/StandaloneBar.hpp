// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich != Emu (shared/plugin) -- the panel's half of the standalone's own controls, in the
// bar under the panel: MIDI IN (the Windows standalone, StandaloneMidi.hpp is the DSP's
// half) and BUFFER (DPF's own standalone, whose block size can change), each a field that
// opens a list upwards, over the panel.  An instrument can open lists of its own the same
// way (BarList).  Include after DistrhoUI.hpp and DistrhoStandaloneUtils.hpp.
//
//   stateChanged("midiports", v) -> setMidiPorts(v)
//   uiIdle()                     -> idle()
//   drawing, in design units     -> drawFields() with the bar, drawList() last
//   mouse, in design units       -> press(x, y) first, motion(x, y); a key: escape()

#ifndef OSTRICH_STANDALONE_BAR_HPP
#define OSTRICH_STANDALONE_BAR_HPP

#include "FileIO.hpp"
#include "Folders.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace ostrich {

struct BarRect
{
	float x, y, w, h;
	bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

/// The bar's colours; the defaults are Ostrich 32's and 55's dark bar.
struct BarStyle
{
	using Color = DGL_NAMESPACE::Color;
	Color field { 30, 30, 33 }, edge { 84, 84, 90 }, accent { 255, 140, 40 };
	Color name { 150, 150, 156 }, value { 220, 220, 224 }, warn { 240, 170, 60 }, arrow { 200, 200, 204 };
	Color litText { 40, 20, 0 };
	Color list { 24, 24, 27 }, hover { 60, 44, 24 }, rule { 60, 60, 66 };
};

/// A list a field opens: its items, which one is current, where a line separates them.
class BarList
{
public:
	virtual ~BarList() = default;
	virtual int count() const = 0;
	virtual std::string label(int i) const = 0;
	virtual bool current(int i) const = 0;
	virtual bool ruleAfter(int) const { return false; }
	virtual void choose(int i) = 0;
};

class StandaloneBar
{
public:
	using Color = DGL_NAMESPACE::Color;
	using NanoVG = DGL_NAMESPACE::NanoVG;

	/// `name`: the instrument's settings folder ("Ostrich 55").  `listBottom`: where the
	/// lists end (the bar's top); `panelW`: the design width, which they never cross.
	StandaloneBar(DISTRHO::UI &ui, const char *name, float listBottom, float panelW, const BarStyle &style = BarStyle())
		: m_ui(ui), m_style(style), m_listBottom(listBottom), m_panelW(panelW),
		  m_rows(int((listBottom - 8.0f) / kItemH)),
		  m_bufferFile(standaloneSettingFile(name, "standalone-buffer.txt")),
		  m_midiList(*this), m_bufferList(*this) {}

	/// Where the two fields are, in design units.
	void place(const BarRect &midi, const BarRect &buffer)
	{
		m_midiField = midi;
		m_bufferField = buffer;
	}

	// ---- the state ---------------------------------------------------------------------------

	/// "midiports": empty where there is no MIDI IN of our own; else the choice, what went
	/// wrong, then the ports (StandaloneMidi.hpp).
	void setMidiPorts(const char *value)
	{
		std::vector<std::string> part(1);
		for (const char *c = value; *c; c ++)
		{
			if (*c == kSep)
				part.emplace_back();
			else
				part.back() += *c;
		}
		m_midiOffered = part.size() >= 2;
		if (!m_midiOffered)
		{
			if (m_open == &m_midiList)
				m_open = nullptr;
			return;
		}
		m_midiChoice = part[0];
		m_midiProblem = part[1];
		m_midiPorts.assign(part.begin() + 2, part.end());
	}

	bool midiOffered() const { return m_midiOffered; }

	/// DPF's own standalone (no JACK) plays through RtAudio, whose block size can change.
	static bool bufferOffered()
	{
		return DISTRHO::isUsingNativeAudio() && DISTRHO::supportsBufferSizeChanges();
	}

	/// The block size saved by the last run, set again once the panel is up.
	void idle()
	{
		if (m_bufferRestored || !bufferOffered())
			return;
		m_bufferRestored = true;
		const std::vector<uint8_t> saved = m_bufferFile.empty() ? std::vector<uint8_t>() : readFile(m_bufferFile, 256);
		const unsigned n = unsigned(std::strtoul(std::string(saved.begin(), saved.end()).c_str(), nullptr, 10));
		for (unsigned b : kBufferChoices)
			if (b == n && n != DISTRHO::getBufferSize())
			{
				DISTRHO::requestBufferSizeChange(n);
				m_ui.repaint();
			}
	}

	/// The left edge of the leftmost field shown, else `otherwise`: where the status line
	/// must end.
	float leftEdge(float otherwise) const
	{
		return bufferOffered() ? m_bufferField.x : m_midiOffered ? m_midiField.x : otherwise;
	}

	// ---- drawing -----------------------------------------------------------------------------

	void drawFields()
	{
		if (m_midiOffered)
		{
			std::string t = midiLabel(m_midiChoice);
			if (!m_midiProblem.empty())
				t += " \xe2\x80\x94 " + m_midiProblem;
			field(m_midiField, "MIDI IN", 52, t, m_open == &m_midiList, !m_midiProblem.empty());
		}
		if (bufferOffered())
			field(m_bufferField, "BUFFER", 50, bufferLabel(DISTRHO::getBufferSize(), m_ui.getSampleRate()),
			      m_open == &m_bufferList, false);
	}

	/// The open list, if any; drawn last, over the panel.
	void drawList()
	{
		if (m_open == nullptr)
			return;
		NanoVG &g = m_ui;
		const int n = items();
		const BarRect top = item(0);
		g.beginPath();
		g.roundedRect(top.x - 1, top.y - 1, float(cols()) * m_colW + 2, float(rows()) * kItemH + 2, 3);
		g.fillColor(m_style.list);
		g.fill();
		g.strokeColor(m_style.accent);
		g.strokeWidth(1);
		g.stroke();
		for (int i = 0; i < n; i ++)
		{
			const BarRect r = item(i);
			if (i == m_hover)
			{
				g.beginPath();
				g.rect(r.x, r.y, r.w, r.h);
				g.fillColor(m_style.hover);
				g.fill();
			}
			const bool current = m_open->current(i);
			g.textAlign(NanoVG::ALIGN_LEFT | NanoVG::ALIGN_MIDDLE);
			g.fontSize(11);
			g.fillColor(current ? m_style.accent : m_style.value);
			if (current)
				g.text(r.x + 8, r.y + r.h / 2 + 1, "\xe2\x9c\x93", nullptr);
			g.text(r.x + 26, r.y + r.h / 2 + 1, fit(m_open->label(i), r.w - 34, 11).c_str(), nullptr);
			if (m_open->ruleAfter(i))
			{
				g.beginPath();
				g.rect(r.x + 6, r.y + r.h - 0.5f, r.w - 12, 1);
				g.fillColor(m_style.rule);
				g.fill();
			}
		}
	}

	/// A field in the bar: a small name, the value, ▾.
	void field(const BarRect &r, const char *name, float valueX, const std::string &value, bool open, bool warn)
	{
		NanoVG &g = m_ui;
		g.beginPath();
		g.roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 3);
		g.fillColor(m_style.field);
		g.fill();
		g.strokeColor(open ? m_style.accent : m_style.edge);
		g.strokeWidth(1);
		g.stroke();

		g.textAlign(NanoVG::ALIGN_LEFT | NanoVG::ALIGN_MIDDLE);
		g.fontSize(9);
		g.fillColor(m_style.name);
		g.text(r.x + 8, r.y + r.h / 2 + 1, name, nullptr);
		g.fillColor(warn ? m_style.warn : m_style.value);
		g.text(r.x + valueX, r.y + r.h / 2 + 1, fit(value, r.w - valueX - 22, 10).c_str(), nullptr);
		g.beginPath();
		g.moveTo(r.x + r.w - 15, r.y + r.h / 2 - 2);
		g.lineTo(r.x + r.w - 7, r.y + r.h / 2 - 2);
		g.lineTo(r.x + r.w - 11, r.y + r.h / 2 + 3);
		g.closePath();
		g.fillColor(m_style.arrow);
		g.fill();
	}

	/// A control in the bar: dark with a thin edge, as a host's own; lit when on.
	void button(const BarRect &r, const char *label, bool lit)
	{
		NanoVG &g = m_ui;
		g.beginPath();
		g.roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 3);
		g.fillColor(lit ? m_style.accent : m_style.field);
		g.fill();
		g.strokeColor(lit ? m_style.accent : m_style.edge);
		g.strokeWidth(1);
		g.stroke();
		g.fontSize(10);
		g.textAlign(NanoVG::ALIGN_CENTER | NanoVG::ALIGN_MIDDLE);
		g.fillColor(lit ? m_style.litText : m_style.arrow);
		g.text(r.x + r.w / 2, r.y + r.h / 2 + 1, label, nullptr);
	}

	/// Cuts text to a width, with an ellipsis.
	std::string fit(const std::string &t, float w, float size)
	{
		NanoVG &g = m_ui;
		g.fontSize(size);
		std::string s = t;
		DGL_NAMESPACE::Rectangle<float> b;
		while (!s.empty() && g.textBounds(0, 0, (s + (s.size() < t.size() ? "\xe2\x80\xa6" : "")).c_str(), nullptr, b) > w)
			s.pop_back();
		return s.size() < t.size() ? s + "\xe2\x80\xa6" : s;
	}

	// ---- the lists ---------------------------------------------------------------------------

	/// Opens `list` above `anchor`, in columns `colW` wide (more items than fit above the
	/// bar go into further columns, to the left).
	void open(BarList &list, const BarRect &anchor, float colW)
	{
		m_open = &list;
		m_anchor = anchor;
		m_colW = colW;
		m_hover = -1;
		m_ui.repaint();
	}

	bool isOpen() const { return m_open != nullptr; }
	bool isOpen(const BarList &list) const { return m_open == &list; }

	void close()
	{
		if (m_open == nullptr)
			return;
		m_open = nullptr;
		m_ui.repaint();
	}

	/// A press: with a list open, a click into it picks and anywhere else only closes it;
	/// else a click on one of the fields opens its list.  True when taken.
	bool press(float x, float y)
	{
		if (m_open != nullptr)
		{
			BarList *const list = m_open;
			const int i = itemAt(x, y);
			m_open = nullptr;
			if (i >= 0)
				list->choose(i);
			m_ui.repaint();
			return true;
		}
		if (m_midiOffered && m_midiField.contains(x, y))
		{
			open(m_midiList, m_midiField, kMidiColW);
			sendMidiIn(kRescan);            // the list again: a keyboard plugged in since
			return true;
		}
		if (bufferOffered() && m_bufferField.contains(x, y))
		{
			open(m_bufferList, m_bufferField, m_bufferField.w + 40.0f);
			return true;
		}
		return false;
	}

	/// The mouse moved: true while a list is open (it takes the motion).
	bool motion(float x, float y)
	{
		if (m_open == nullptr)
			return false;
		const int i = itemAt(x, y);
		if (i != m_hover)
		{
			m_hover = i;
			m_ui.repaint();
		}
		return true;
	}

	/// Escape: closes an open list; true when it did.
	bool escape()
	{
		if (m_open == nullptr)
			return false;
		close();
		return true;
	}

private:
	static constexpr char kSep = '\x1f';
	static constexpr const char *kAll = "*";
	static constexpr const char *kRescan = "?";
	static constexpr float kItemH = 18.0f, kMidiColW = 260.0f;
	static constexpr unsigned kBufferChoices[] = { 64, 128, 256, 512, 1024 };

	static std::string midiLabel(const std::string &choice)
	{
		return choice.empty() ? "Off" : choice == kAll ? "All inputs" : choice;
	}

	static std::string bufferLabel(unsigned frames, double rate)
	{
		char t[48];
		std::snprintf(t, sizeof(t), "%u  (%.1f ms)", frames, rate > 0.0 ? 1000.0 * frames / rate : 0.0);
		return t;
	}

	void sendMidiIn(const std::string &choice)
	{
		const std::string msg = std::to_string(++ m_seq) + " " + choice;
		m_ui.setState("midiin", msg.c_str());
	}

	int items() const { return m_open == nullptr ? 0 : std::min(m_open->count(), m_rows * 5); }
	int rows() const { return std::min(items(), m_rows); }
	int cols() const { return (items() + m_rows - 1) / m_rows; }

	BarRect item(int i) const
	{
		const float left = std::min(m_anchor.x, m_panelW - 8.0f - float(cols()) * m_colW);
		const float top = m_listBottom - 4.0f - float(rows()) * kItemH;
		return { left + float(i / m_rows) * m_colW, top + float(i % m_rows) * kItemH, m_colW, kItemH };
	}

	int itemAt(float x, float y) const
	{
		for (int i = 0; i < items(); i ++)
			if (item(i).contains(x, y))
				return i;
		return -1;
	}

	// MIDI IN: Off, All inputs, a line, then the ports.
	struct MidiList : BarList
	{
		StandaloneBar &bar;
		explicit MidiList(StandaloneBar &b) : bar(b) {}
		std::string choice(int i) const
		{
			return i == 0 ? std::string() : i == 1 ? std::string(kAll) : bar.m_midiPorts[size_t(i - 2)];
		}
		int count() const override { return int(bar.m_midiPorts.size()) + 2; }
		std::string label(int i) const override { return midiLabel(choice(i)); }
		bool current(int i) const override { return choice(i) == bar.m_midiChoice; }
		bool ruleAfter(int i) const override { return i == 1; }
		void choose(int i) override
		{
			const std::string c = choice(i);
			bar.m_midiChoice = c;           // shown at once; the plugin's answer follows
			bar.m_midiProblem.clear();
			bar.sendMidiIn(c);
		}
	};

	// BUFFER: the block sizes.
	struct BufferList : BarList
	{
		StandaloneBar &bar;
		explicit BufferList(StandaloneBar &b) : bar(b) {}
		int count() const override { return int(sizeof(kBufferChoices) / sizeof(kBufferChoices[0])); }
		std::string label(int i) const override { return bufferLabel(kBufferChoices[i], bar.m_ui.getSampleRate()); }
		bool current(int i) const override { return kBufferChoices[i] == DISTRHO::getBufferSize(); }
		void choose(int i) override
		{
			const unsigned n = kBufferChoices[i];
			if (n != DISTRHO::getBufferSize())
				DISTRHO::requestBufferSizeChange(n);    // restarts the audio; falls back if refused
			if (bar.m_bufferFile.empty())
				return;
			std::error_code ec;
			std::filesystem::create_directories(bar.m_bufferFile.parent_path(), ec);
			const std::string v = std::to_string(DISTRHO::getBufferSize());
			writeFile(bar.m_bufferFile, std::vector<uint8_t>(v.begin(), v.end()));
		}
	};

	DISTRHO::UI &m_ui;
	const BarStyle m_style;
	const float m_listBottom, m_panelW;
	const int m_rows;
	const std::filesystem::path m_bufferFile;
	BarRect m_midiField {}, m_bufferField {};

	bool m_midiOffered = false;
	std::string m_midiChoice, m_midiProblem;
	std::vector<std::string> m_midiPorts;
	bool m_bufferRestored = false;
	unsigned m_seq = 0;

	MidiList m_midiList;
	BufferList m_bufferList;
	BarList *m_open = nullptr;
	BarRect m_anchor {};
	float m_colW = kMidiColW;
	int m_hover = -1;
};

} // namespace ostrich

#endif // OSTRICH_STANDALONE_BAR_HPP
