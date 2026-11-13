// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich != Emu (shared/plugin; all instruments) -- the panel's size, kept with the host's
// session.  Include after DistrhoUI.hpp.
//
// The size lives in the plugin's state ("uisize", "<w> <h>"), so the host saves it with
// the session.  Unscaled -- window pixels over the desktop scale factor -- so that it opens
// the same on a screen with other scaling.  The panel draws in design units (designW x
// designH) scaled to whatever size the window has.
//
//   stateChanged("uisize", v)  -> restore(v)
//   uiIdle()                   -> idle()
//   mouse events               -> divide the position by mouseScale()

#ifndef OSTRICH_PANEL_SIZE_HPP
#define OSTRICH_PANEL_SIZE_HPP

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace ostrich {

class PanelSize
{
public:
	using Clock = std::chrono::steady_clock;

	PanelSize(DISTRHO::UI &ui, float designW, float designH)
		: m_ui(ui), m_w(designW), m_h(designH) {}

	/// A saved size.  One saved by an earlier panel of another shape (Ostrich 220's, before
	/// the card slots and the bar) keeps its width; the height follows.
	void restore(const char *value)
	{
		unsigned w = 0, h = 0;
		if (std::sscanf(value, "%u %u", &w, &h) != 2 || w < 100 || h < 20)
			return;
		if (std::fabs(double(w) / h - m_w / m_h) > 0.05)
			h = unsigned(std::lround(w * m_h / m_w));
		m_saved = value;
		DGL_NAMESPACE::Window &win = m_ui.getWindow();
		const double scale = win.getScaleFactor();
		const unsigned pw = unsigned(std::lround(w * scale)), ph = unsigned(std::lround(h * scale));
		if (std::abs(int(pw) - int(win.getWidth())) > 2 || std::abs(int(ph) - int(win.getHeight())) > 2)
			win.setSize(pw, ph);
	}

	/// From uiIdle(): the host frame in the first seconds, then the size once it settled.
	void idle()
	{
		if (!m_frameFitted && Clock::now() - m_created < std::chrono::seconds(2))
			fitHostFrame();
		saveWhenSettled();
	}

	/// Mouse events come in the window's own pixels.  The panel is drawn in design units
	/// scaled by getWidth() / designW on top of DGL's automatic scaling, so the full factor
	/// is the window width over the design width.
	float mouseScale() const
	{
		return float(m_ui.getWindow().getWidth()) / m_w;
	}

private:
	// Sends the size once it has stood still for a second -- not while the user drags, and
	// not in the first seconds, while the host still settles the window and the saved size
	// is on its way (restore).
	void saveWhenSettled()
	{
		DGL_NAMESPACE::Window &win = m_ui.getWindow();
		const auto now = Clock::now();
		const unsigned pw = win.getWidth(), ph = win.getHeight();
		if (pw != m_seenW || ph != m_seenH)
		{
			m_seenW = pw;
			m_seenH = ph;
			m_seenSince = now;
			return;
		}
		if (now - m_created < std::chrono::seconds(3) || now - m_seenSince < std::chrono::seconds(1))
			return;
		const double scale = win.getScaleFactor();
		char value[32];
		std::snprintf(value, sizeof(value), "%u %u", unsigned(std::lround(pw / scale)), unsigned(std::lround(ph / scale)));
		if (m_saved == value)
			return;
		unsigned sw = 0, sh = 0;
		if (std::sscanf(m_saved.c_str(), "%u %u", &sw, &sh) == 2
				&& std::abs(int(sw * scale) - int(pw)) <= 2 && std::abs(int(sh * scale) - int(ph)) <= 2)
			return;                     // the host's rounding, not a resize
		m_saved = value;
		m_ui.setState("uisize", value);
	}

	// Cubase at 125 % tells a new view "scale 1.0" before it opens and sizes it for that,
	// while its own window is made for 125 %: the view sat in a black margin right and below
	// until reopened (the second time Cubase gives the right size).  A view below its least
	// size at the desktop's scale is that; it asks the host for the least size, as a
	// reopened view gets.  Once, after DPF's first idle (until then it drops the request).
	void fitHostFrame()
	{
		if (Clock::now() - m_created < std::chrono::milliseconds(200))
			return;
		m_frameFitted = true;
		DGL_NAMESPACE::Window &win = m_ui.getWindow();
		bool keep = false;
		const DGL_NAMESPACE::Size<unsigned> least = win.getGeometryConstraints(keep);
		const double scale = win.getScaleFactor();
		const unsigned lw = unsigned(std::lround(least.getWidth() * scale)), lh = unsigned(std::lround(least.getHeight() * scale));
		if (win.getWidth() + 2 < lw || win.getHeight() + 2 < lh)
			win.setSize(lw, lh);
	}

	DISTRHO::UI &m_ui;
	const float m_w, m_h;
	const Clock::time_point m_created = Clock::now();
	std::string m_saved;                    // "<w> <h>" unscaled, as in the plugin state
	unsigned m_seenW = 0, m_seenH = 0;
	Clock::time_point m_seenSince {};
	bool m_frameFitted = false;
};

} // namespace ostrich

#endif // OSTRICH_PANEL_SIZE_HPP
