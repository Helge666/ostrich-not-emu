// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 55 -- the Roland SC-55 as a plugin.  The DSP half: ROMs, the machine, MIDI in,
// the core's rate -> host rate, the panel out to the UI.
//
// The machine is Nuked-SC55 behind sc55::Core (plugin/core).  Everything here is about
// being a plugin; nothing here may change how the machine behaves.

#include "DistrhoPlugin.hpp"
#include "OstrichShared.hpp"

#include "sc55_core.h"
#include "Resampler.hpp"
#include "ZipReader.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

START_NAMESPACE_DISTRHO

namespace {

namespace fs = std::filesystem;
using namespace ostrich;

/// A path as UTF-8 in a std::string (C++20's u8string() is a different type).
std::string pstr(const fs::path &p)
{
	const auto u = p.u8string();
	return std::string(u.begin(), u.end());
}

fs::path upath(const std::string &s)
{
	return fs::path(reinterpret_cast<const char8_t *>(s.c_str()));
}

/// The family this build plays.  Nuked-SC55 knows the later models too (mk2, SC-55st, ...);
/// they need their own panels and the core's other rates, see ROADMAP.md.
constexpr const char *kFamily = "mk1";

/// The SC-55 (mk1) versions, the one tried first first: v1.21 is the most common one, and
/// the one most music was written on.
const char *const kPreferred[] = { "mk1-v1.21", "mk1-v2.00", "mk1-v1.20", "mk1-v1.10", "mk1-v1.00" };

/// The sizes an image of any model can have; anything else is not read at all.
bool plausibleSize(uintmax_t n)
{
	return n == 0x1000 || n == 0x8000 || n == 0x40000 || n == 0x80000 || n == 0x100000 || n == 0x200000;
}

std::vector<uint8_t> readFile(const fs::path &p, size_t limit = 0x200000)
{
	std::vector<uint8_t> data;
	std::error_code ec;
	const auto n = fs::file_size(p, ec);
	if (ec || n == 0 || n > limit)
		return data;
	FILE *f = nullptr;
#ifdef _WIN32
	f = _wfopen(p.c_str(), L"rb");
#else
	f = std::fopen(p.c_str(), "rb");
#endif
	if (!f)
		return data;
	data.resize(size_t(n));
	if (std::fread(data.data(), 1, data.size(), f) != data.size())
		data.clear();
	std::fclose(f);
	return data;
}

bool writeFile(const fs::path &p, const std::vector<uint8_t> &data)
{
	FILE *f = nullptr;
#ifdef _WIN32
	f = _wfopen(p.c_str(), L"wb");
#else
	f = std::fopen(p.c_str(), "wb");
#endif
	if (!f)
		return false;
	const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
	return std::fclose(f) == 0 && ok;
}

/// Where the user puts the images, most specific first.  On Windows ProgramData comes
/// first: a tool inside an MSIX package sees a virtualised %LOCALAPPDATA%, the DAW does
/// not (as Ostrich 220 learnt).
std::vector<fs::path> romDirs()
{
	std::vector<fs::path> dirs;
	auto env = [&](const char *name, const char *tail) {
		if (const char *v = std::getenv(name))
			if (*v) dirs.push_back((upath(v) / upath(tail)).make_preferred());
	};
	env("OSTRICH55_ROMS", "");
#ifdef _WIN32
	env("PROGRAMDATA", "Ostrich 55/roms");
	env("LOCALAPPDATA", "Ostrich 55/roms");
	env("USERPROFILE", "Documents/Ostrich 55/roms");
#else
	env("XDG_DATA_HOME", "Ostrich 55/roms");
	env("HOME", ".local/share/Ostrich 55/roms");
	dirs.push_back("/usr/share/Ostrich 55/roms");
#endif
	return dirs;
}

/// Where images the user picks in the panel are copied to: the first of these that takes
/// them.  romDirs() searches them all.
std::vector<fs::path> installDirs()
{
	std::vector<fs::path> dirs;
	auto env = [&](const char *name, const char *tail) {
		if (const char *v = std::getenv(name))
			if (*v) dirs.push_back((upath(v) / upath(tail)).make_preferred());
	};
#ifdef _WIN32
	env("PROGRAMDATA", "Ostrich 55/roms");
	env("LOCALAPPDATA", "Ostrich 55/roms");
	env("USERPROFILE", "Documents/Ostrich 55/roms");
#else
	env("XDG_DATA_HOME", "Ostrich 55/roms");
	env("HOME", ".local/share/Ostrich 55/roms");
#endif
	return dirs;
}

/// What one or more folders hold, by SHA-256 whatever the names: for every ROM set
/// Nuked-SC55 knows, the images found for it.  An image out of a zip has the zip as
/// @c from and its own name as @c member.
struct RomScan
{
	struct Image
	{
		fs::path from;
		std::string member;
		std::vector<uint8_t> data;
		bool found() const { return !data.empty(); }
	};
	std::map<std::string, std::array<Image, sc55::kRomLocations>> sets;
	fs::path where;             ///< the folder the first image came from

	bool complete(const std::string &set) const
	{
		const auto it = sets.find(set);
		if (it == sets.end())
			return false;
		for (unsigned loc : sc55::romsetLocations(set))
			if (!it->second[loc].found())
				return false;
		return true;
	}

	/// The set to run: the first of kPreferred that is complete, or "".
	std::string best() const
	{
		for (const char *s : kPreferred)
			if (complete(s))
				return s;
		return std::string();
	}

	/// The set closest to complete, for the panel's list while images are missing.
	std::string closest() const
	{
		std::string pick = kPreferred[0];
		size_t most = 0;
		for (const char *s : kPreferred)
		{
			const auto it = sets.find(s);
			if (it == sets.end())
				continue;
			size_t n = 0;
			for (unsigned loc : sc55::romsetLocations(s))
				n += it->second[loc].found() ? 1 : 0;
			if (n > most)
			{
				most = n;
				pick = s;
			}
		}
		return pick;
	}

	bool any() const { return !sets.empty(); }

	std::string missing(const std::string &set) const
	{
		std::string m;
		const auto it = sets.find(set);
		for (unsigned slot = 0; slot < kRomSlots; slot ++)
		{
			const unsigned loc = kRomSlotLocation[slot];
			if (it == sets.end() || !it->second[loc].found())
				m += std::string(m.empty() ? "" : ", ") + kRomTitles[slot];
		}
		return m;
	}

	/// Adds what @p dir holds: loose files of an image's size, then what zips in it hold
	/// (a MAME set).
	void scan(const fs::path &dir)
	{
		std::error_code ec;
		if (!fs::is_directory(dir, ec))
			return;
		std::vector<fs::path> zips;
		for (const auto &e : fs::directory_iterator(dir, ec))
		{
			std::error_code ec2;
			const auto sz = e.is_regular_file(ec2) ? e.file_size(ec2) : 0;
			if (ec2)
				continue;
			std::string ext = pstr(e.path().extension());
			for (char &c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
			if (ext == ".zip")
				zips.push_back(e.path());
			else if (plausibleSize(sz))
				take(readFile(e.path()), e.path(), std::string(), dir);
		}
		std::sort(zips.begin(), zips.end());
		for (const fs::path &z : zips)
			for (const zip::Entry &e : zip::list(z))
			{
				if (!plausibleSize(e.size))
					continue;
				// Windows PowerShell 5's Compress-Archive writes folders with '\'.
				const size_t cut = e.name.find_last_of("/\\");
				const std::string name = cut == std::string::npos ? e.name : e.name.substr(cut + 1);
				std::vector<uint8_t> d;
				if (zip::extract(z, e, d))
					take(std::move(d), z, name, dir);
			}
	}

private:
	void take(std::vector<uint8_t> d, const fs::path &path, const std::string &inZip, const fs::path &dir)
	{
		if (d.empty())
			return;
		bool used = false;
		for (const sc55::RomsetPart &p : sc55::identifyRom(d.data(), d.size()))
		{
			Image &img = sets[p.romset][p.location];
			if (img.found())
				continue;
			img.from = path;
			img.member = inZip;
			img.data = d;
			used = true;
		}
		if (used && where.empty())
			where = dir;
	}
};

constexpr uint8_t kGsReset[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7 };
constexpr uint8_t kGmReset[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };

} // anonymous namespace


class OstrichPlugin : public Plugin
{
public:
	OstrichPlugin()
		: Plugin(kParamCount, 0, kStateCount)
	{
		for (auto &k : m_keySeq) k.store(0);
		for (auto &f : m_keyFreeFrames) f = kKeyGapFrames;     // the first press goes at once
		loadRoms();
		setupRate(getSampleRate());
	}

	~OstrichPlugin() override
	{
		if (m_romThread.joinable())
			m_romThread.join();
	}

protected:
	const char *getLabel() const override { return "Ostrich55"; }
	const char *getMaker() const override { return "Ostrich 55 project"; }
	const char *getLicense() const override { return "GPL-3.0-or-later"; }
	const char *getHomePage() const override { return "https://github.com/Helge666/ostrich55"; }
	uint32_t getVersion() const override { return d_version(0, 1, 0); }
	int64_t getUniqueId() const override { return d_cconst('O', '0', '5', '5'); }
	const char *getDescription() const override
	{
		return "An emulation of the Roland SC-55 Sound Canvas, running its original firmware "
		       "on Nuked-SC55. Requires the user's own ROM images.";
	}

	// ---- parameters ------------------------------------------------------------------

	void initParameter(uint32_t index, Parameter &p) override
	{
		if (index == kParamVolume)
		{
			p.hints = kParameterIsAutomatable;
			p.name = "Volume";
			p.symbol = "volume";
			p.unit = "dB";
			p.ranges.def = 0.0f;
			p.ranges.min = -40.0f;
			p.ranges.max = 12.0f;
			return;
		}
		const uint32_t k = index - kParamKeyFirst;
		p.hints = kParameterIsBoolean | kParameterIsAutomatable;
		p.name = kKeyNames[k];
		p.symbol = kKeySymbols[k];
		p.ranges.def = 0.0f;
		p.ranges.min = 0.0f;
		p.ranges.max = 1.0f;
	}

	float getParameterValue(uint32_t index) const override
	{
		if (index == kParamVolume)
			return m_volumeDb;
		return m_keyDown[index - kParamKeyFirst].load() ? 1.0f : 0.0f;
	}

	void setParameterValue(uint32_t index, float value) override
	{
		if (index == kParamVolume)
		{
			m_volumeDb = value;
			m_gainTarget = std::pow(10.0f, value / 20.0f);
			return;
		}
		const uint32_t k = index - kParamKeyFirst;
		if (k < kNumKeys)
			setKey(k, value > 0.5f);
	}

	// A click can press and release before run() comes round.  So a press also bumps a
	// counter, and run() presses for every new count, holding long enough in EMULATED time
	// for the firmware's key scan to see it.
	void setKey(uint32_t k, bool down)
	{
		if (down && !m_keyDown[k].load())
			m_keySeq[k].fetch_add(1);
		m_keyDown[k].store(down);
	}

	// ---- state ---------------------------------------------------------------------------

	void initState(uint32_t index, State &state) override
	{
		switch (index)
		{
		case kStatePanel:
			state.key = "panel";
			state.label = "Panel";
			state.hints = kStateIsOnlyForUI;
			break;
		case kStateStatus:
			state.key = "status";
			state.label = "Status";
			state.hints = kStateIsOnlyForUI;
			break;
		case kStateKey:
			// The panel's own keys come this way and not as parameter edits: under VST3 a
			// parameter edit from the UI reaches the DSP only through the host's automation
			// path (Ostrich 220: in Cubase the clicks never arrived).  The key parameters
			// stay, for automation and generic host UIs.
			state.key = "key";
			state.label = "Panel key";
			state.hints = kStateIsOnlyForDSP;
			break;
		case kStateUiSize:
			// Kept here only so that the host saves it with the session (OstrichUI.cpp).
			state.key = "uisize";
			state.label = "Panel size";
			state.hints = 0;
			break;
		case kStateRomSet:
			state.key = "romset";
			state.label = "ROM images";
			state.hints = kStateIsOnlyForUI;
			break;
		case kStateRomFile:
			state.key = "romfile";
			state.label = "ROM image file";
			state.hints = kStateIsOnlyForDSP;
			break;
		case kStateReset:
			state.key = "reset";
			state.label = "GS/GM reset";
			state.hints = kStateIsOnlyForDSP;
			break;
		}
		state.defaultValue = "";
	}

	String getState(const char *key) const override
	{
		if (std::strcmp(key, "status") == 0)
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			return String(m_status.c_str());
		}
		if (std::strcmp(key, "romset") == 0)
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			return String(m_romSet.c_str());
		}
		if (std::strcmp(key, "uisize") == 0)
		{
			std::lock_guard<std::mutex> lock(m_uiSizeLock);
			return String(m_uiSize.c_str());
		}
		return String();
	}

	void setState(const char *key, const char *value) override
	{
		if (value == nullptr)
			return;
		if (std::strcmp(key, "key") == 0)
		{
			// A restored session hands every key back, empty: ignore anything malformed.
			unsigned k = 0, down = 0;
			if (std::sscanf(value, "%u %u", &k, &down) == 2 && k < kNumKeys)
				setKey(k, down != 0);
			return;
		}
		if (std::strcmp(key, "uisize") == 0)
		{
			std::lock_guard<std::mutex> lock(m_uiSizeLock);
			m_uiSize = value;
			return;
		}
		if (std::strcmp(key, "romfile") == 0)
		{
			if (value[0] != '\0')
				requestRomFile(value);
			return;
		}
		if (std::strcmp(key, "reset") == 0)
		{
			unsigned n = 0;
			char what[4] = {};
			if (std::sscanf(value, "%u %3s", &n, what) == 2)
				m_resetRequest.store(std::strcmp(what, "gm") == 0 ? 2 : std::strcmp(what, "gs") == 0 ? 1 : 0);
			return;
		}
	}

	// ---- audio ---------------------------------------------------------------------------

	void sampleRateChanged(double newRate) override { setupRate(newRate); }

	void activate() override
	{
		m_activated = true;
		for (auto &r : m_rs)
			r.reset();
		m_gain = m_gainTarget;
	}

	void deactivate() override { m_activated = false; }

	void run(const float **, float **outputs, uint32_t frames,
	         const MidiEvent *midiEvents, uint32_t midiEventCount) override
	{
		float *const outL = outputs[0];
		float *const outR = outputs[1];

		flushStateToUi();

		// loadRoms() starts the machine on another thread.  Never wait for it here.
		std::unique_lock<std::mutex> lock(m_machine, std::try_to_lock);
		if (!lock.owns_lock() || !m_core.running() || m_coreRate != m_core.sampleRate())
		{
			std::memset(outL, 0, sizeof(float) * frames);
			std::memset(outR, 0, sizeof(float) * frames);
			if (lock.owns_lock() && m_core.running())
				setupRate(m_hostRate);          // a machine of another rate came up
			publishPanel(frames, lock.owns_lock());
			return;
		}

		const uint32_t coreFrames = m_rs[0].inputsFor(frames);
		if (m_coreBuf[0].size() < coreFrames)
			for (auto &b : m_coreBuf)
				b.resize(coreFrames);           // only on a host block larger than any before
		tickKeys(coreFrames);

		if (const int reset = m_resetRequest.exchange(0))
		{
			if (reset == 1)
				m_core.midi(kGsReset, sizeof(kGsReset));
			else
				m_core.midi(kGmReset, sizeof(kGmReset));
		}

		// MIDI in at its place in the block: the core is rendered up to each event.  The
		// bytes then reach the firmware at the wire's 31250 baud, as from a sequencer.
		float *const cl = m_coreBuf[0].data(), *const cr = m_coreBuf[1].data();
		uint32_t done = 0;
		for (uint32_t i = 0; i < midiEventCount; i ++)
		{
			const MidiEvent &ev = midiEvents[i];
			const uint8_t *data = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
			if (ev.size == 0 || data == nullptr)
				continue;
			uint32_t at = uint32_t(double(ev.frame) * m_coreRate / m_hostRate);
			at = std::min(at, coreFrames);
			if (at > done)
			{
				m_core.render(cl + done, cr + done, at - done);
				done = at;
			}
			m_core.midi(data, ev.size);
			m_midiSeen = true;
		}
		if (done < coreFrames)
			m_core.render(cl + done, cr + done, coreFrames - done);

		m_rs[0].process(cl, coreFrames, outL, frames);
		m_rs[1].process(cr, coreFrames, outR, frames);

		for (uint32_t i = 0; i < frames; i ++)
		{
			m_gain += (m_gainTarget - m_gain) * 0.001f;
			outL[i] *= m_gain;
			outR[i] *= m_gain;
		}
		publishPanel(frames, true);
	}

private:
	static constexpr double kPanelRefreshHz = 30.0;
	// tickKeys, in core frames at 64 kHz (computed from the core's rate when it differs)
	static constexpr double kKeyHoldSeconds = 0.1;      // one step per press (sc55_render, SC55_KEYS)
	static constexpr double kKeyGapSeconds = 0.04;
	static constexpr uint32_t kKeyGapFrames = 64000 / 25;
	static constexpr uint32_t kKeyMaxQueued = 16;
	static constexpr double kBootSeconds = 2.5;         // power-on to the main page (sc55_render)

	void setupRate(double rate)
	{
		m_hostRate = rate > 0.0 ? rate : 48000.0;
		m_coreRate = m_core.sampleRate();
		for (auto &r : m_rs)
			r.setup(m_coreRate, m_hostRate);
		m_publishPeriod = uint32_t(m_hostRate / kPanelRefreshHz);
		const uint32_t worst = m_rs[0].inputsFor(8192) + 16;
		for (auto &b : m_coreBuf)
			b.assign(worst, 0.0f);
		m_keyHoldFrames = uint32_t(m_coreRate * kKeyHoldSeconds);
		m_keyGap = uint32_t(m_coreRate * kKeyGapSeconds);
		setLatency(m_rs[0].latency());
	}

	// Presses queue up (the wheel over the display, a fast hand), each played as a press of
	// its own: held for kKeyHoldSeconds of emulated time, then a gap.  A key held down on
	// the panel is held on the unit, so its firmware repeats as the unit does.
	void tickKeys(uint32_t coreFrames)
	{
		uint32_t mask = 0;
		for (unsigned k = 0; k < kNumKeys; k ++)
		{
			const uint32_t seq = m_keySeq[k].load();
			if (!m_keyHeld[k] && m_keyFreeFrames[k] < m_keyGap)
				m_keyFreeFrames[k] += coreFrames;
			if (!m_keyHeld[k] && seq != m_keySeen[k] && m_keyFreeFrames[k] >= m_keyGap)
			{
				if (seq - m_keySeen[k] > kKeyMaxQueued)
					m_keySeen[k] = seq - kKeyMaxQueued;     // a spun wheel: not minutes of it
				m_keySeen[k] ++;
				m_keyHeld[k] = true;
				m_keyHeldFrames[k] = 0;
			}
			else if (m_keyHeld[k])
			{
				m_keyHeldFrames[k] += coreFrames;
				// Held while the key is down, or the press is not over yet.
				if ((!m_keyDown[k].load() || m_keySeq[k].load() != m_keySeen[k]) && m_keyHeldFrames[k] >= m_keyHoldFrames)
				{
					m_keyHeld[k] = false;
					m_keyFreeFrames[k] = 0;
				}
			}
			if (m_keyHeld[k])
				mask |= 1u << kKeyBits[k];
		}
		m_core.setButtons(mask);
	}

	void flushStateToUi()
	{
		if (!m_statusPending.load() && !m_romSetPending.load())
			return;
		// Set on other threads too: never wait for it here.
		std::unique_lock<std::mutex> sl(m_statusLock, std::try_to_lock);
		if (!sl.owns_lock())
			return;
		if (m_statusPending.exchange(false))
			updateStateValue("status", m_status.c_str());
		if (m_romSetPending.exchange(false))
			updateStateValue("romset", m_romSet.c_str());
	}

	void publishPanel(uint32_t frames, bool haveMachine)
	{
		m_publishAccum += frames;
		if (m_publishAccum < m_publishPeriod)
			return;
		m_publishAccum = 0;

		PanelBlob blob = m_lastBlob;
		if (haveMachine && m_core.running())
		{
			sc55::LcdState st;
			if (m_core.lcd(st))
			{
				std::memcpy(blob.data, st.data, sizeof(blob.data));
				std::memcpy(blob.cgram, st.cgram, sizeof(blob.cgram));
				blob.flags = 1 | (st.enabled ? 4 : 0);
			}
			if (m_midiSeen)
				blob.flags |= 2;
			else
				blob.flags &= ~2;
			m_midiSeen = false;
		}
		else if (!m_romsLoaded)
			blob = PanelBlob {};
		// Sent when it changed, and once a second regardless: the queue to the UI drops a
		// push when it is full, and a display that then stood still would show the dropped
		// state's predecessor for good.
		if (std::memcmp(&blob, &m_lastBlob, sizeof(blob)) != 0 || ++m_publishesSame >= kPanelRefreshHz)
		{
			encodeHex(reinterpret_cast<const uint8_t *>(&blob), sizeof(blob), m_blobHex);
			if (updateStateValue("panel", m_blobHex))
			{
				m_lastBlob = blob;
				m_publishesSame = 0;
			}
		}
	}

	void setStatus(const std::string &s)
	{
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			m_status = s;
			m_statusPending.store(true);
		}
		d_stdout("Ostrich 55: %s", s.c_str());
	}

	// ---- ROMs ------------------------------------------------------------------------

	/// Looks in the ROM folders and starts the machine when a whole set is there.  From the
	/// constructor, and again once the panel has copied images in (installRoms).
	bool loadRoms(std::string *missing = nullptr)
	{
		RomScan r;
		for (const fs::path &dir : romDirs())
			r.scan(dir);
		const std::string set = r.best();
		publishRomSet(r, set.empty() ? r.closest() : set);
		if (set.empty())
		{
			const std::string m = r.missing(r.closest());
			if (missing)
				*missing = m;
			std::string dirs;
			for (const fs::path &d : romDirs())
				dirs += (dirs.empty() ? "" : "  |  ") + pstr(d);
			setStatus("Missing: " + m + ".  Put the SC-55 ROM images (any file names) into: " + dirs);
			return false;
		}

		std::array<std::vector<uint8_t>, sc55::kRomLocations> images;
		for (unsigned loc : sc55::romsetLocations(set))
			images[loc] = r.sets[set][loc].data;
		std::lock_guard<std::mutex> lock(m_machine);
		std::string error;
		if (!m_core.start(set, std::move(images), error))
		{
			setStatus("The SC-55 did not start (" + set + "): " + error);
			return false;
		}
		// Power-on to the main page in emulated time, flat out: the host's first notes
		// then meet a machine that listens.
		m_core.skip(uint32_t(kBootSeconds * m_core.sampleRate()));
		m_romsLoaded = true;
		setStatus("SC-55 " + set.substr(4) + " running.  ROMs from " + pstr(r.where));
		return true;
	}

	/// What the panel's ROM list shows (OstrichShared.hpp, kStateRomSet).
	void publishRomSet(const RomScan &r, const std::string &set)
	{
		std::string t;
		const auto it = r.sets.find(set);
		for (unsigned slot = 0; slot < kRomSlots; slot ++)
		{
			const bool found = it != r.sets.end() && it->second[kRomSlotLocation[slot]].found();
			t += std::string(found ? "found" : "") + kFieldSep;
		}
		t += (set.size() > 4 ? set.substr(4) : std::string()) + kFieldSep;
		const auto dirs = installDirs();
		t += dirs.empty() ? std::string() : pstr(dirs.front());
		std::lock_guard<std::mutex> lock(m_statusLock);
		m_romSet = t;
		m_romSetPending.store(true);
	}

	// From the panel: "<n> <path>", a file the user picked.  Its folder is searched for all
	// the images, on a thread of its own: SHA-256 over a few MB, then the boot.
	void requestRomFile(const char *value)
	{
		unsigned n = 0;
		int used = 0;
		if (std::sscanf(value, "%u %n", &n, &used) < 1 || used == 0 || value[used] == '\0' || m_romsLoaded)
			return;
		// Under VST3, DPF runs a second, whole instance for the edit controller, and the
		// panel's state messages reach both: only the instance that plays writes files.
		if (!m_activated)
			return;
		if (m_romBusy.exchange(true))
			return;
		if (m_romThread.joinable())
			m_romThread.join();
		const fs::path picked = upath(value + used);
		m_romThread = std::thread([this, picked] {
			installRoms(picked);
			m_romBusy.store(false);
		});
	}

	/// Copies what the picked file's folder holds and the ROM folders lack into the first
	/// install folder that takes it, then looks again.
	void installRoms(const fs::path &picked)
	{
		RomScan have, found;
		for (const fs::path &dir : romDirs())
			have.scan(dir);
		const fs::path dir = picked.parent_path();
		found.scan(dir);
		const std::string pickedName = pstr(picked.filename());

		// Every SC-55 (mk1) image found that the ROM folders lack.  A loose file is copied, one
		// out of a zip is written out under its own name: the folder then holds plain files.
		struct Copy { fs::path from; std::string name; const std::vector<uint8_t> *data; };
		std::vector<Copy> copy;
		std::vector<const std::vector<uint8_t> *> seen;
		bool pickedIsOne = false;
		for (const std::string &set : sc55::romsetsOfFamily(kFamily))
		{
			const auto it = found.sets.find(set);
			if (it == found.sets.end())
				continue;
			for (unsigned loc : sc55::romsetLocations(set))
			{
				const RomScan::Image &img = it->second[loc];
				if (!img.found())
					continue;
				pickedIsOne = pickedIsOne || (img.member.empty() && img.from.filename() == picked.filename());
				const auto h = have.sets.find(set);
				if (h != have.sets.end() && h->second[loc].found())
					continue;
				// The same image serves several versions (the wave ROMs): copied once.
				const bool dup = std::any_of(copy.begin(), copy.end(), [&](const Copy &c) {
					return c.from == img.from && c.name == (img.member.empty() ? pstr(img.from.filename()) : img.member); });
				if (!dup)
					copy.push_back({ img.from, img.member.empty() ? pstr(img.from.filename()) : img.member,
					                 img.member.empty() ? nullptr : &img.data });
			}
		}
		if (!found.any())
		{
			setStatus(pickedName + " is not an SC-55 ROM image, and its folder holds none either.");
			publishRomSet(have, have.closest());
			return;
		}

		std::string target, error;
		for (const fs::path &to : installDirs())
		{
			if (copy.empty())
				break;
			std::error_code ec;
			fs::create_directories(to, ec);
			bool ok = !ec;
			for (const Copy &c : copy)
			{
				if (!ok)
					break;
				// Never over a file of the same name: the user's, whatever it is.
				const fs::path name = upath(c.name);
				fs::path dest = to / name;
				for (int i = 2; fs::exists(dest, ec); i ++)
					dest = to / upath(pstr(name.stem()) + " (" + std::to_string(i) + ")" + pstr(name.extension()));
				if (c.data != nullptr)
				{
					ok = writeFile(dest, *c.data);
					if (!ok)
						error = pstr(dest) + ": cannot write";
				}
				else
				{
					ok = fs::copy_file(c.from, dest, ec) && !ec;
					if (!ok)
						error = pstr(dest) + ": " + ec.message();
				}
			}
			if (ok)
			{
				target = pstr(to);
				break;
			}
		}
		if (!copy.empty() && target.empty())
		{
			setStatus("Could not copy the images: " + error);
			publishRomSet(have, have.closest());
			return;
		}

		std::string missing;
		const bool running = loadRoms(&missing);
		// What matters first: the status line is cut at the panel's width.
		std::string msg = copy.empty() ? "Nothing new in " + pstr(dir) + "."
		                               : "Copied " + std::to_string(copy.size()) + " image(s) from " + pstr(dir) + " to " + target + ".";
		if (!pickedIsOne)
			msg += "  (" + pickedName + " itself is not one of them.)";
		setStatus(running ? "The SC-55 is running.  " + msg : "Still missing: " + missing + ".  " + msg);
	}

	// ---- members ---------------------------------------------------------------------

	mutable std::mutex m_machine;
	sc55::Core m_core;
	std::atomic<bool> m_romsLoaded { false };
	std::thread m_romThread;                    // installRoms
	std::atomic<bool> m_romBusy { false };
	// Whether the host has activated this instance (requestRomFile).
	std::atomic<bool> m_activated { false };
	std::atomic<int> m_resetRequest { 0 };      // 1 GS, 2 GM

	std::string m_status, m_romSet;
	mutable std::mutex m_statusLock;
	std::atomic<bool> m_statusPending { false }, m_romSetPending { false };
	std::string m_uiSize;           // "<w> <h>", the panel's, saved with the session
	mutable std::mutex m_uiSizeLock;

	double m_hostRate = 48000.0, m_coreRate = 64000.0;
	voltaire::Resampler m_rs[2];
	std::vector<float> m_coreBuf[2];

	float m_volumeDb = 0.0f, m_gainTarget = 1.0f, m_gain = 1.0f;

	std::atomic<bool> m_keyDown[kNumKeys] {};
	std::atomic<uint32_t> m_keySeq[kNumKeys];
	uint32_t m_keySeen[kNumKeys] = {};
	bool m_keyHeld[kNumKeys] = {};
	uint32_t m_keyHeldFrames[kNumKeys] = {};
	uint32_t m_keyFreeFrames[kNumKeys] = {};    // since the key's last release
	uint32_t m_keyHoldFrames = 6400, m_keyGap = kKeyGapFrames;

	uint32_t m_publishPeriod = 1600, m_publishAccum = 0, m_publishesSame = 0;
	PanelBlob m_lastBlob {};
	bool m_midiSeen = false;        // MIDI in since the last refresh: the MIDI MESSAGE lamp
	char m_blobHex[sizeof(PanelBlob) * 2 + 1] = {};

	DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstrichPlugin)
};

Plugin *createPlugin()
{
	return new OstrichPlugin();
}

END_NAMESPACE_DISTRHO
