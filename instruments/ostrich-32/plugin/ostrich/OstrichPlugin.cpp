// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 32 -- the Roland MT-32 as a plugin.  The DSP half: ROMs, the machine, MIDI in,
// the panel out to the UI.  Built like Ostrich 55's.
//
// The machine is 88emu (gearmulator: The Usual Suspects; LA32 after nukeykt's Nuked-MT32,
// reverb after Sergey V. Mikayev's emulator in Munt) behind mt32::Core (plugin/core).  It
// runs the unit's own firmware at the host's rate.  Everything here is about being a
// plugin; nothing here may change how the machine behaves.

#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"
#include "OstrichShared.hpp"
#include "Version.hpp"

#include "mt32_core.h"
#include "../../../../shared/plugin/StandaloneMidi.hpp"
#include "../../../../shared/plugin/FileIO.hpp"
#include "../../../../shared/plugin/Folders.hpp"
#include "../../../../shared/plugin/ZipReader.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

START_NAMESPACE_DISTRHO

namespace {

namespace fs = std::filesystem;
using namespace ostrich;

std::vector<fs::path> romDirs() { return ostrich::romDirs("OSTRICH32_ROMS", { "Ostrich 32" }); }
std::vector<fs::path> installDirs() { return ostrich::installDirs("Ostrich 32"); }

/// The machine a new instance runs when its images are there: the old MT-32's first
/// firmware, the one the owner's unit of 1987 had or was close to.
constexpr const char *kDefaultMachine = "mt32old/1.04";

/// The sizes an image of an LA board can have (a chip, a whole ROM); anything else is not
/// read at all.
bool plausibleSize(uintmax_t n)
{
	return n >= 0x2000 && n <= 0x200000 && (n & (n - 1)) == 0;
}


using ostrich::readFile;
using ostrich::writeFile;

/// "mt32old/1.04" -> "MT-32 1.04", for the panel.
std::string machineName(const std::string &id)
{
	const size_t cut = id.find('/');
	const std::string board = id.substr(0, cut), fw = cut == std::string::npos ? std::string() : id.substr(cut + 1);
	const char *name = board == "mt32old" || board == "mt32new" ? "MT-32"
	                 : board == "cm32l" ? "CM-32L" : board == "cm32ln" ? "CM-32LN" : board.c_str();
	return std::string(name) + " " + fw;
}

/// What one or more folders hold: every image 88emu knows for an LA board, whatever its
/// file name, each once.  An image out of a zip has the zip as @c from and its own name as
/// @c member.
struct RomScan
{
	struct Image
	{
		fs::path from;
		std::string member;
		std::vector<uint8_t> data;
		mt32::RomId id;
	};
	std::vector<Image> images;
	fs::path where;             ///< the folder the first image came from

	bool has(const std::vector<uint8_t> &d) const
	{
		return std::any_of(images.begin(), images.end(), [&](const Image &i) { return i.data == d; });
	}

	std::vector<std::vector<uint8_t>> data() const
	{
		std::vector<std::vector<uint8_t>> out;
		for (const Image &i : images)
			out.push_back(i.data);
		return out;
	}

	/// Whether an image for the old MT-32's slot @p slot ("control", "wave", "reverb") is there.
	bool oldBoardHas(const char *slot) const
	{
		return std::any_of(images.begin(), images.end(), [&](const Image &i) {
			return i.id.slot == slot && i.id.devices.find("MT-32 1.x") != std::string::npos; });
	}

	/// Adds what @p dir holds: loose files of an image's size, then what zips in it hold
	/// (a MAME set).
	void scan(const fs::path &dir)
	{
		std::error_code ec;
		if (!fs::is_directory(dir, ec))
			return;
		std::vector<fs::path> files, zips;
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
				files.push_back(e.path());
		}
		std::sort(files.begin(), files.end());
		std::sort(zips.begin(), zips.end());
		for (const fs::path &f : files)
			take(readFile(f), f, std::string(), dir);
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
		mt32::RomId id;
		if (d.empty() || has(d) || !mt32::identifyRom(d.data(), d.size(), id))
			return;
		images.push_back({ path, inZip, std::move(d), id });
		if (where.empty())
			where = dir;
	}
};

} // anonymous namespace


class OstrichPlugin : public Plugin
{
public:
	OstrichPlugin()
		: Plugin(kParamCount, 0, kStateCount)
	{
		for (auto &k : m_keySeq) k.store(0);
		m_hostRate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
		setupRate();
		rescanRoms();
		m_worker = std::thread([this] { work(); });
		m_midi.start();
	}

	~OstrichPlugin() override
	{
		{
			std::lock_guard<std::mutex> lock(m_jobLock);
			m_quit = true;
		}
		m_jobCv.notify_all();
		m_worker.join();
	}

protected:
	const char *getLabel() const override { return "Ostrich32"; }
	const char *getMaker() const override { return "Ostrich 32 project"; }
	const char *getLicense() const override { return "GPL-3.0-or-later"; }
	const char *getHomePage() const override { return "https://github.com/Helge666/ostrich-not-emu/tree/main/instruments/ostrich-32"; }
	uint32_t getVersion() const override
	{
		return d_version(OSTRICH_VERSION_MAJOR, OSTRICH_VERSION_MINOR, OSTRICH_VERSION_PATCH);
	}
	int64_t getUniqueId() const override { return d_cconst('O', '0', '3', '2'); }
	const char *getDescription() const override
	{
		return "An emulation of the Roland MT-32, running its original firmware on 88emu "
		       "(gearmulator). Requires the user's own ROM images.";
	}

	// ---- parameters ------------------------------------------------------------------

	void initParameter(uint32_t index, Parameter &p) override
	{
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
		const uint32_t k = index - kParamKeyFirst;
		return k < kNumKeys && m_keyDown[k].load() ? 1.0f : 0.0f;
	}

	void setParameterValue(uint32_t index, float value) override
	{
		const uint32_t k = index - kParamKeyFirst;
		if (k < kNumKeys)
			setKey(k, value > 0.5f);
	}

	// A click can press and release before run() comes round.  So a press also bumps a
	// counter, and run() presses for every new count, holding long enough in emulated time
	// for the firmware's panel scan to see it.
	void setKey(uint32_t k, bool down)
	{
		if (down && !m_keyDown[k].load())
			m_keySeq[k].fetch_add(1);
		m_keyDown[k].store(down);
	}

	// ---- state ---------------------------------------------------------------------------

	void initState(uint32_t index, State &state) override
	{
		state.hints = 0;
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
			// The panel's own switches come this way and not as parameter edits: under VST3 a
			// parameter edit from the UI reaches the DSP only through the host's automation
			// path (Ostrich 220: in Cubase the clicks never arrived).
			state.key = "key";
			state.label = "Panel switch";
			state.hints = kStateIsOnlyForDSP;
			break;
		case kStateKnob:
			state.key = "knob";
			state.label = "Panel knob";
			state.hints = kStateIsOnlyForDSP;
			break;
		case kStateUiSize:
			// Kept here only so that the host saves it with the session (OstrichUI.cpp).
			state.key = "uisize";
			state.label = "Panel size";
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
		case kStateMachine:
			// Saved with the session: the firmware the song was made on.
			state.key = "machine";
			state.label = "Machine";
			break;
		case kStateMachines:
			state.key = "machines";
			state.label = "Machines";
			state.hints = kStateIsOnlyForUI;
			break;
		case kStateMidiIn:
			state.key = "midiin";
			state.label = "MIDI IN port";
			state.hints = kStateIsOnlyForDSP;
			break;
		case kStateMidiPorts:
			state.key = "midiports";
			state.label = "MIDI IN ports";
			state.hints = kStateIsOnlyForUI;
			break;
		case kStateSetup:
			// Saved with the session: what the song set the unit to, from the panel or by MIDI.
			state.key = "setup";
			state.label = "MT-32 setup";
			state.hints = kStateIsOnlyForDSP;
			break;
		case kStatePartCh:
			// Saved with the session; the bar's PART = CH switch.
			state.key = "partch";
			state.label = "Part = channel";
			break;
		case kStateFx:
			state.key = "fx";
			state.label = "Effect setting";
			state.hints = kStateIsOnlyForDSP;
			break;
		case kStateFxState:
			state.key = "fxstate";
			state.label = "Effect";
			state.hints = kStateIsOnlyForUI;
			break;
		case kStateReverbSw:
			// Saved with the session: the parts' reverb switches this instance keeps.
			state.key = "reverbsw";
			state.label = "Reverb per part";
			state.hints = kStateIsOnlyForDSP;
			break;
		}
		state.defaultValue = index == kStatePartCh ? "1" : index == kStateReverbSw ? "11111111" : "";
	}

	String getState(const char *key) const override
	{
		if (std::strcmp(key, "setup") == 0)
			return String(const_cast<OstrichPlugin *>(this)->currentSetup().c_str());
		if (std::strcmp(key, "partch") == 0)
			return String(m_partCh.load() ? "1" : "0");
		if (std::strcmp(key, "reverbsw") == 0)
		{
			char sw[9] = {};
			for (unsigned p = 0; p < 8; p ++)
				sw[p] = m_partSw[p].load() ? '1' : '0';
			return String(sw);
		}
		std::lock_guard<std::mutex> lock(m_statusLock);
		if (std::strcmp(key, "fxstate") == 0)
			return String(m_fxState.c_str());
		if (std::strcmp(key, "status") == 0)
			return String(m_status.c_str());
		if (std::strcmp(key, "romset") == 0)
			return String(m_romSet.c_str());
		if (std::strcmp(key, "uisize") == 0)
			return String(m_uiSize.c_str());
		if (std::strcmp(key, "machine") == 0)
			return String(m_wantMachine.c_str());
		if (std::strcmp(key, "machines") == 0)
			return String(m_machines.c_str());
		if (std::strcmp(key, "midiports") == 0)
			return String(m_midi.portsText().c_str());
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
		if (std::strcmp(key, "knob") == 0)
		{
			unsigned n = 0;
			int steps = 0;
			if (std::sscanf(value, "%u %d", &n, &steps) == 2)
				m_knobSteps.fetch_add(std::clamp(steps, -kKnobSteps, kKnobSteps));
			return;
		}
		if (std::strcmp(key, "uisize") == 0)
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			m_uiSize = value;
			return;
		}
		if (std::strcmp(key, "machine") == 0)
		{
			{
				std::lock_guard<std::mutex> lock(m_statusLock);
				if (m_wantMachine == value)
					return;
				m_wantMachine = value;
			}
			// Under VST3, DPF runs a second, whole instance for the edit controller: only the
			// instance that plays starts machines.
			if (m_activated)
				queueJob(std::string(), true);
			return;
		}
		if (std::strcmp(key, "romfile") == 0)
		{
			unsigned n = 0;
			int used = 0;
			if (std::sscanf(value, "%u %n", &n, &used) >= 1 && used > 0 && value[used] != '\0' && m_activated)
				queueJob(value + used, true);
			return;
		}
		if (std::strcmp(key, "midiin") == 0)
		{
			m_midi.setState(value);
			return;
		}
		if (std::strcmp(key, "setup") == 0)
		{
			if (value[0] == '\0')
				return;                     // nothing to bring the unit to
			{
				std::lock_guard<std::mutex> lock(m_statusLock);
				m_setup = value;
				m_setupApply = true;
				m_setupGen ++;
			}
			if (m_activated)
				queueJob(std::string(), true);
			return;
		}
		if (std::strcmp(key, "partch") == 0)
		{
			m_partCh.store(std::strcmp(value, "0") != 0);
			return;
		}
		if (std::strcmp(key, "reverbsw") == 0)
		{
			if (std::strlen(value) == 8)
				for (unsigned p = 0; p < 8; p ++)
					m_partSw[p].store(value[p] != '0');
			return;
		}
		if (std::strcmp(key, "fx") == 0)
		{
			setFx(value);
			return;
		}
	}

	// ---- the EFFECT tab --------------------------------------------------------------------
	// What the tab asks for waits here until it has been left alone for kFxQuietSeconds (a
	// drag over the steps is one change), then the worker thread sends what differs from the
	// unit, paced (pumpOutbox).  On the old firmware a write to a part's patch temp reloads
	// its sound and may cut its notes; that is the price of a switch the panel lacks.

	void setFx(const char *value)
	{
		unsigned n = 0;
		char what[16] = {};
		int a = -1, b = -1;
		if (std::sscanf(value, "%u %15s %d %d", &n, what, &a, &b) < 3)
			return;
		std::lock_guard<std::mutex> lock(m_fxLock);
		FxWant &w = m_fxWant;
		if (std::strcmp(what, "mode") == 0 && a >= 0 && a < kReverbModes)
			w.mode = a;
		else if (std::strcmp(what, "time") == 0 && a >= 0 && a < kReverbTimes)
			w.time = a;
		else if (std::strcmp(what, "level") == 0 && a >= 0 && a < kReverbLevels)
			w.level = a;
		else if (std::strcmp(what, "preset") == 0 && a >= 0 && a < kReverbPresets)
		{
			w.mode = 0;
			w.time = kReverbPresetTime[a];
			w.level = kReverbPresetLevel[a];
		}
		else if (std::strcmp(what, "part") == 0 && a >= 0 && a < 9 && (b == 0 || b == 1))
		{
			w.part[a] = b;
			if (a < 8)
				m_partSw[a].store(b != 0);  // the instance's: set again after a program change
		}
		else
			return;
		m_fxTouched = std::chrono::steady_clock::now();
		m_fxUnsent = true;
	}

	// ---- audio ---------------------------------------------------------------------------

	void sampleRateChanged(double newRate) override
	{
		m_hostRate = newRate > 0.0 ? newRate : 48000.0;
		setupRate();
	}

	// The machine starts here, on the host's thread and not on the audio thread: the
	// firmware's boot takes the better part of a second.  Here, and not in the constructor,
	// so that a host's first notes meet a machine that listens, and so that VST3's edit
	// controller (never activated) starts none.
	void activate() override
	{
		m_activated = true;
		startMachine();
	}

	void deactivate() override { m_activated = false; }

	void run(const float **, float **outputs, uint32_t frames,
	         const MidiEvent *midiEvents, uint32_t midiEventCount) override
	{
		float *const outL = outputs[0];
		float *const outR = outputs[1];

		flushStateToUi();

		// The worker thread starts machines.  Never wait for it here.
		std::unique_lock<std::mutex> lock(m_machine, std::try_to_lock);
		if (!lock.owns_lock() || !m_core.running() || m_core.sampleRate() != m_hostRate)
		{
			std::memset(outL, 0, sizeof(float) * frames);
			std::memset(outR, 0, sizeof(float) * frames);
			m_midi.drop();
			m_knobSteps.store(0);
			publishPanel(frames, false);
			return;
		}

		tickKeys(frames);
		if (const int steps = m_knobSteps.exchange(0))
		{
			const int to = std::clamp(m_knob + steps * kPotPerStep, 0, int(mt32::kKnobMaximum));
			if (to != m_knob)
			{
				m_core.setPanelKnob(uint16_t(to));
				m_panelTouched.store(true);
				m_panelQuiet = 0;
				m_knobIdle = 0;
			}
			m_knob = to;
		}
		m_knobIdle = m_knobIdle < m_knobSettle ? m_knobIdle + frames : m_knobIdle;

		// What MIDI changes, the core's copy of the unit's memory follows as it goes; what the
		// panel changed, it learns by asking the unit once the panel has been left alone for
		// kPanelQuietSeconds (a turn of the knob is dozens of changes: one reading for all),
		// or at once when the host saves before that (readPanel()).
		m_panelQuiet = m_panelQuiet < m_panelSettle ? m_panelQuiet + frames : m_panelQuiet;
		if ((m_readRequest.load() || (m_panelTouched.load() && m_panelQuiet >= m_panelSettle)) && !m_core.busy()
		    && !m_outPending.load())
		{
			m_panelTouched.store(false);
			m_core.readSetup();
			m_readRequest.store(false);
		}
		pumpOutbox(frames);

		// MIDI in at its place in the block, the host's and the standalone's own merged.
		// The core renders up to each message before it hands it over.  PART = CH moves
		// channels 1-8 to the unit's 2-9, so that channel n plays part n; 9 and 10-16 stay
		// (9 then also plays part 8, 10 the rhythm part).
		const bool partCh = m_partCh.load();
		const uint32_t own = m_midi.collect(frames, m_hostRate);
		for (uint32_t i = 0, o = 0; i < midiEventCount || o < own; )
		{
			uint32_t frame, size;
			const uint8_t *data;
			if (o < own && (i >= midiEventCount || m_midi.event(o).frame < midiEvents[i].frame))
			{
				const StandaloneMidi::Event e = m_midi.event(o ++);
				frame = e.frame;
				size = e.size;
				data = e.data;
			}
			else
			{
				const MidiEvent &ev = midiEvents[i ++];
				frame = ev.frame;
				size = ev.size;
				data = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
			}
			// System Realtime (clock, start, active sensing) is not for the unit's parser.
			if (size == 0 || data == nullptr || data[0] >= 0xF8)
				continue;
			uint8_t moved[3];
			if (partCh && size <= 3 && data[0] >= 0x80 && data[0] < 0xF0 && (data[0] & 0x0F) < 8)
			{
				std::memcpy(moved, data, size);
				moved[0] = uint8_t(data[0] + 1);
				data = moved;
			}
			const uint32_t at = std::min(frame, frames - 1);
			m_core.midi(data, size, at);
			// A program change brings the patch's own reverb switch (on in every factory
			// patch): where this instance has a part's switched off, off again, at once, so
			// that the notes after it are dry.
			if (size >= 2 && (data[0] & 0xF0) == 0xC0)
				for (uint8_t p = 0; p < 8; p ++)
					if (!m_partSw[p].load() && m_partChan[p].load() == (data[0] & 0x0F))
					{
						const uint8_t off = 0;
						uint8_t msg[16];
						m_core.midi(msg, dt1(0x03, 0x00, uint8_t(p * 0x10 + 6), &off, 1, msg), at);
					}
		}
		m_core.render(outL, outR, frames);
		m_blocks.fetch_add(1);
		publishPanel(frames, true);
	}

private:
	static constexpr double kPanelRefreshHz = 30.0;
	static constexpr double kKeyHoldSeconds = 0.08;     // long enough for the panel scan
	static constexpr double kKeyGapSeconds = 0.04;
	static constexpr uint32_t kKeyMaxQueued = 16;
	static constexpr double kPanelQuietSeconds = 0.25;  // then what the panel changed is read
	static constexpr double kKnobSettleSeconds = 0.3;   // then the knob follows the value
	static constexpr int kPotPerStep = 8;               // a panel step: one the firmware reads
	static constexpr double kFxQuietSeconds = 0.25;     // then what the EFFECT tab asks is sent
	static constexpr double kFxPollSeconds = 0.05;      // the worker looks at the effect
	static constexpr double kFxGiveUpSeconds = 3.0;

	/// What the EFFECT tab asks for: -1 = nothing (as the unit has it).
	struct FxWant
	{
		int mode = -1, time = -1, level = -1;
		int part[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // 8 = RHYTHM
	};

	void setupRate()
	{
		m_publishPeriod = uint32_t(m_hostRate / kPanelRefreshHz);
		m_keyHoldFrames = uint32_t(m_hostRate * kKeyHoldSeconds);
		m_keyGap = uint32_t(m_hostRate * kKeyGapSeconds);
		m_panelSettle = uint32_t(m_hostRate * kPanelQuietSeconds);
		m_knobSettle = uint32_t(m_hostRate * kKnobSettleSeconds);
		for (auto &f : m_keyFreeFrames) f = m_keyGap;   // the first press goes at once
	}

	/// A DT1 (data set) to the unit, device ID 17, at a 7-bit address: its length in @p out
	/// (at least n + 10 bytes).
	static size_t dt1(uint8_t a0, uint8_t a1, uint8_t a2, const uint8_t *d, size_t n, uint8_t *out)
	{
		size_t i = 0;
		for (uint8_t b : { 0xF0, 0x41, 0x10, 0x16, 0x12 })
			out[i ++] = b;
		unsigned sum = a0 + a1 + a2;
		out[i ++] = a0;
		out[i ++] = a1;
		out[i ++] = a2;
		for (size_t k = 0; k < n; k ++)
		{
			out[i ++] = d[k];
			sum += d[k];
		}
		out[i ++] = uint8_t((128 - sum % 128) % 128);
		out[i ++] = 0xF7;
		return i;
	}

	/// The time the firmware wants after a message of @p bytes before the next (as the core
	/// paces a restore, mt32_core.cpp).
	static double messageSeconds(size_t bytes) { return double(bytes) * (11.0 / 32000.0 + 0.001) + 0.04; }

	/// run(): the next message the worker left in the outbox, once the one before had its time.
	void pumpOutbox(uint32_t frames)
	{
		m_outWait = m_outWait > frames ? m_outWait - frames : 0;
		if (!m_outPending.load() || m_outWait || m_core.busy())
			return;
		std::unique_lock<std::mutex> lock(m_outLock, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		if (m_outNext < m_outEnds.size())
		{
			const uint32_t from = m_outNext ? m_outEnds[m_outNext - 1] : 0, to = m_outEnds[m_outNext ++];
			m_core.midi(m_outBytes.data() + from, to - from, 0);
			m_outWait = uint32_t(messageSeconds(to - from) * m_hostRate);
		}
		if (m_outNext >= m_outEnds.size())
			m_outPending.store(false);
	}

	// Presses queue up (a fast hand), each played as a press of its own: held for
	// kKeyHoldSeconds of emulated time, then a gap.  A switch held down on the panel is held
	// on the unit, so the firmware does what the unit does with a held switch.
	void tickKeys(uint32_t frames)
	{
		uint32_t mask = 0;
		for (unsigned k = 0; k < kNumKeys; k ++)
		{
			const uint32_t seq = m_keySeq[k].load();
			if (!m_keyHeld[k] && m_keyFreeFrames[k] < m_keyGap)
				m_keyFreeFrames[k] += frames;
			if (!m_keyHeld[k] && seq != m_keySeen[k] && m_keyFreeFrames[k] >= m_keyGap)
			{
				if (seq - m_keySeen[k] > kKeyMaxQueued)
					m_keySeen[k] = seq - kKeyMaxQueued;
				m_keySeen[k] ++;
				m_keyHeld[k] = true;
				m_keyHeldFrames[k] = 0;
			}
			else if (m_keyHeld[k])
			{
				m_keyHeldFrames[k] += frames;
				if ((!m_keyDown[k].load() || m_keySeq[k].load() != m_keySeen[k]) && m_keyHeldFrames[k] >= m_keyHoldFrames)
				{
					m_keyHeld[k] = false;
					m_keyFreeFrames[k] = 0;
				}
			}
			if (m_keyHeld[k])
				mask |= kKeyBits[k];
		}
		if (mask != m_keyMask)
		{
			m_core.panelButtons(mask);
			m_keyMask = mask;
			m_panelTouched.store(true);
			m_panelQuiet = 0;
		}
	}

	void flushStateToUi()
	{
		m_midi.takePorts([this](const char *t) { updateStateValue("midiports", t); });
		if (!m_statusPending.load())
			return;
		// Set on other threads too: never wait for it here.
		std::unique_lock<std::mutex> sl(m_statusLock, std::try_to_lock);
		if (!sl.owns_lock())
			return;
		m_statusPending.store(false);
		updateStateValue("status", m_status.c_str());
		updateStateValue("romset", m_romSet.c_str());
		updateStateValue("machines", m_machines.c_str());
		updateStateValue("fxstate", m_fxState.c_str());
	}

	void publishPanel(uint32_t frames, bool haveMachine)
	{
		m_publishAccum += frames;
		if (m_publishAccum < m_publishPeriod)
			return;
		m_publishAccum = 0;

		PanelBlob blob = m_lastBlob;
		if (haveMachine)
		{
			// TODO real-time safety: 88emu builds the text in a std::string (ROADMAP.md).
			char text[21];
			bool lamp = false;
			if (m_core.display(text, lamp))
			{
				std::memcpy(blob.text, text, sizeof(blob.text));
				followValue(text);
			}
			blob.flags = uint8_t(1 | (lamp ? 2 : 0));
			blob.knob = uint8_t((m_knob * kKnobSteps + mt32::kKnobMaximum / 2) / mt32::kKnobMaximum);
		}
		else
		{
			blob = PanelBlob {};
			blob.knob = uint8_t(kKnobSteps);
			if (m_starting.load())
				blob.flags = 4;
		}
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

	// The knob as if it had a motor: where the display shows a volume -- the master volume on
	// the main screen ("1 2 3 4 5 R vol: 71"), a part's after PART n ("1|Fretless 2|vol> 80"),
	// RHYTHM's ("Rhythm Part |vol> 80") -- the knob goes to where that volume stands, once nobody has turned it for
	// kKnobSettleSeconds.  The unit's own knob stays where the hand left it, so after a
	// restore or another part it would point elsewhere, and a part's volume only follows it
	// once it passes the value.  The firmware reads the 10-bit pot as 7 bits and maps 0-127
	// to 0-100 (measured on 1.04 and 1.07, every position); the knob goes to the middle of
	// the value's span, so the firmware reads the value it has.  Not in SOUND mode: there
	// the knob picks a sound at once, and a jump would pick another.
	static int potValue(int pot) { return (pot >> 3) * 100 / 127; }
	static int potFor(int value)
	{
		const int low = (value * 127 + 99) / 100, high = std::min(127, ((value + 1) * 127 + 99) / 100 - 1);
		return std::min(int(mt32::kKnobMaximum), (low + high) * 4 + 4);
	}

	void followValue(const char *t)
	{
		if (m_knobIdle < m_knobSettle)
			return;
		const bool master = t[1] == ' ' && std::memcmp(t + 13, "vol:", 4) == 0;
		// A part's "1|Fretless 2|vol> 80" and RHYTHM's "Rhythm Part |vol> 80" alike.
		const bool part = t[12] == '|' && std::memcmp(t + 13, "vol>", 4) == 0;
		if (!master && !part)
			return;
		int value = 0;
		for (int i = 17; i < 20; i ++)
		{
			if (t[i] >= '0' && t[i] <= '9')
				value = value * 10 + (t[i] - '0');
			else if (t[i] != ' ')
				return;
		}
		if (value > 100 || potValue(m_knob) == value)
			return;
		m_knob = potFor(value);
		m_core.setPanelKnob(uint16_t(m_knob));
	}

	void setStatus(const std::string &s)
	{
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			m_status = s;
			m_statusPending.store(true);
		}
		d_stdout("Ostrich 32: %s", s.c_str());
	}

	// ---- ROMs and machines -----------------------------------------------------------

	/// Looks in the ROM folders: which machines the images make, and what the panel shows.
	void rescanRoms()
	{
		RomScan r;
		for (const fs::path &dir : romDirs())
			r.scan(dir);
		std::vector<mt32::Machine> machines = mt32::findMachines(r.data());

		std::string set;
		for (unsigned slot = 0; slot < kRomSlots; slot ++)
		{
			static const char *const slots[kRomSlots] = { "control", "wave", "reverb" };
			set += std::string(r.oldBoardHas(slots[slot]) ? "found" : "") + kFieldSep;
		}
		const auto dirs = installDirs();
		set += dirs.empty() ? std::string() : pstr(dirs.front());

		std::lock_guard<std::mutex> lock(m_romLock);
		m_images = r.data();
		m_available = std::move(machines);
		m_romWhere = r.where;
		{
			std::lock_guard<std::mutex> sl(m_statusLock);
			m_romSet = set;
			m_statusPending.store(true);
		}
		publishMachines();
		if (m_available.empty())
		{
			std::string dirsText;
			for (const fs::path &d : romDirs())
				dirsText += (dirsText.empty() ? "" : "  |  ") + pstr(d);
			std::string have;
			for (const RomScan::Image &i : r.images)
				have += (have.empty() ? "" : ", ") + i.id.slot + " " + i.id.version;
			setStatus(std::string(have.empty() ? "No MT-32 ROM images found" : "Incomplete: " + have)
			          + ".  Put the MT-32 ROM images (any file names) into: " + dirsText);
		}
	}

	/// "machines" for the panel; under m_romLock.
	void publishMachines()
	{
		std::string t = m_runningId;
		for (const mt32::Machine &m : m_available)
			t += kFieldSep + m.id + kFieldSep + machineName(m.id);
		std::lock_guard<std::mutex> lock(m_statusLock);
		m_machines = t;
		m_statusPending.store(true);
	}

	/// The machine to run: the session's (or the panel's) choice if its images are there,
	/// else v1.04, else the first there is.  Under m_romLock.
	std::string chooseMachine() const
	{
		std::string want;
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			want = m_wantMachine;
		}
		for (const char *id : { want.c_str(), kDefaultMachine })
			for (const mt32::Machine &m : m_available)
				if (m.id == id)
					return m.id;
		return m_available.empty() ? std::string() : m_available.front().id;
	}

	/// Starts the machine chooseMachine() names at the host's rate, unless it runs already,
	/// and brings it to the session's setup if one waits.  Not on the audio thread: run()
	/// plays silence meanwhile.
	void startMachine()
	{
		std::lock_guard<std::mutex> romLock(m_romLock);
		const std::string id = chooseMachine();
		if (id.empty())
			return;
		std::string setup;
		bool apply;
		uint32_t gen;
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			setup = m_setup;
			apply = m_setupApply;
			gen = m_setupGen;
		}
		if (!apply)
			readPanel();                    // what the machine to be stopped has from its panel
		std::string error;
		bool started = false, ok = true;
		{
			std::lock_guard<std::mutex> lock(m_machine);
			if (!m_core.running() || m_core.machine() != id || m_core.sampleRate() != m_hostRate)
			{
				// Another machine, or another rate: the new one takes the setup over, unless
				// the session brought one.
				if (!apply && m_core.running())
				{
					const std::string s = m_core.setup();
					if (!s.empty())
						setup = m_core.machine() + " " + s;
					apply = !setup.empty();
				}
				m_starting.store(true);
				m_core.stop();
				ok = m_core.start(id, m_images, m_hostRate, error);
				m_knob = mt32::kKnobMaximum;    // a new machine's knob stands fully clockwise
				m_knobIdle = 0;
				m_keyMask = 0;
				m_panelTouched.store(false);
				m_readRequest.store(false);
				{
					std::lock_guard<std::mutex> ol(m_outLock);   // meant for the machine stopped
					m_outBytes.clear();
					m_outEnds.clear();
					m_outNext = 0;
					m_outPending.store(false);
					m_outWait = 0;
				}
				m_fxKnown = false;
				started = true;
			}
			if (ok && apply)
			{
				// Only into the board it was made on: the boards' memories differ (the CM-32L
				// has more rhythm keys and its own sounds there).
				const size_t space = setup.find(' ');
				if (space != std::string::npos && boardOf(setup.substr(0, space)) == boardOf(id))
					m_core.restoreSetup(setup.substr(space + 1));
				apply = false;
			}
		}
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			if (m_setupGen == gen)          // else the session brought another meanwhile
			{
				m_setup = setup;
				m_setupApply = apply;
			}
		}
		if (!started)
			return;
		m_starting.store(false);
		m_runningId = ok ? id : std::string();
		if (ok)
		{
			// The session keeps what it was made on, also when that was only the default.
			std::lock_guard<std::mutex> lock(m_statusLock);
			if (m_wantMachine.empty())
				m_wantMachine = id;
		}
		publishMachines();
		if (ok)
			setStatus(machineName(id) + " running (88emu " + mt32::libraryVersion() + ").  ROMs from " + pstr(m_romWhere));
		else
			setStatus(machineName(id) + " did not start: " + error);
	}

	/// What the panel changed and run() has not read yet (the host saves, or the machine
	/// changes, within kPanelQuietSeconds of the last touch): read now.  run() starts the
	/// reading at its next block and it takes about 0.4 s; while the host does not run the
	/// plugin, the reading is rendered here.  Under m_romLock, not m_machine.
	void readPanel()
	{
		using clock = std::chrono::steady_clock;
		// Also waits for a reading (or a restore) under way.
		if (m_runningId.empty() || !(m_panelTouched.load() || m_core.busy()))
			return;
		if (m_panelTouched.load())
			m_readRequest.store(true);
		const clock::time_point start = clock::now();
		clock::time_point progress = start;
		uint32_t blocks = m_blocks.load();
		while (m_readRequest.load() || m_core.busy())
		{
			const clock::time_point now = clock::now();
			if (m_blocks.load() != blocks)
			{
				blocks = m_blocks.load();
				progress = now;
			}
			if (now - start > std::chrono::seconds(3))
				return;                         // what is known so far
			if (now - progress > std::chrono::milliseconds(200))
			{
				std::lock_guard<std::mutex> lock(m_machine);
				if (m_readRequest.load() && !m_core.busy())
				{
					m_panelTouched.store(false);
					m_core.readSetup();
					m_readRequest.store(false);
				}
				float l[256], r[256];
				for (int i = 0; i < 2000 && m_core.busy(); i ++)
					m_core.render(l, r, 256);
				m_readRequest.store(false);
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}

	/// "mt32old" of "mt32old/1.04".
	static std::string boardOf(const std::string &id) { return id.substr(0, id.find('/')); }

	/// What the session keeps of the unit, "<machine> <SysEx as hex> ...": the running
	/// machine's setup, else (none runs, or a restore is under way) the last one known.
	std::string currentSetup()
	{
		std::lock_guard<std::mutex> romLock(m_romLock);     // no machine starts or stops meanwhile
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			if (m_setupApply)
				return m_setup;                 // the session's, not in a machine yet
		}
		readPanel();
		const std::string s = m_runningId.empty() ? std::string() : m_core.setup();
		std::lock_guard<std::mutex> lock(m_statusLock);
		if (!s.empty())
			m_setup = m_runningId + " " + s;
		return m_setup;
	}

	// ---- the worker thread: picked ROM files, machine changes ----------------------------

	void queueJob(const std::string &picked, bool start)
	{
		{
			std::lock_guard<std::mutex> lock(m_jobLock);
			if (!picked.empty())
				m_jobPicked = picked;
			m_jobStart = m_jobStart || start;
		}
		m_jobCv.notify_all();
	}

	void work()
	{
		std::unique_lock<std::mutex> lock(m_jobLock);
		for (;;)
		{
			// Also every kFxPollSeconds: the EFFECT tab (pollEffect).
			m_jobCv.wait_for(lock, std::chrono::duration<double>(kFxPollSeconds),
			                 [this] { return m_quit || m_jobStart || !m_jobPicked.empty(); });
			if (m_quit)
				return;
			const std::string picked = std::move(m_jobPicked);
			m_jobPicked.clear();
			const bool start = m_jobStart;
			m_jobStart = false;
			lock.unlock();
			if (!picked.empty())
				installRoms(upath(picked));
			if (start && m_activated)
				startMachine();
			pollEffect();
			lock.lock();
		}
	}

	/// The worker's part of the EFFECT tab: what the copy of the unit's memory says the
	/// reverb is, to the tab; and what the tab asked for, once left alone, as the messages
	/// that make the unit so, into the outbox (pumpOutbox sends them).  What the tab asked
	/// for is shown until the unit has it, or for kFxGiveUpSeconds after it was sent.
	void pollEffect()
	{
		using clock = std::chrono::steady_clock;
		std::lock_guard<std::mutex> romLock(m_romLock);     // no machine starts or stops meanwhile
		if (m_runningId.empty() || !m_activated)
		{
			m_fxKnown = false;
			publishFx(FxWant());
			return;
		}
		const uint64_t rev = m_core.revision();
		if (!m_fxKnown || rev != m_fxRev)
		{
			m_fxKnown = m_core.effect(m_fx);
			m_fxRev = rev;
			for (unsigned p = 0; p < 8; p ++)
				m_partChan[p].store(m_fx.partChannel[p]);
		}
		if (!m_fxKnown)
		{
			publishFx(FxWant());
			return;
		}
		const clock::time_point now = clock::now();
		FxWant want;
		{
			std::lock_guard<std::mutex> lock(m_fxLock);
			if (m_fxUnsent && now - m_fxTouched >= std::chrono::duration<double>(kFxQuietSeconds)
			    && !m_outPending.load() && !m_core.busy())
			{
				queueFx(m_fxWant);
				m_fxUnsent = false;
				m_fxSentAt = now;
			}
			// What the unit has now, or will not get, is no longer asked for.
			FxWant &w = m_fxWant;
			const bool gaveUp = !m_fxUnsent && !m_outPending.load()
			                    && now - m_fxSentAt > std::chrono::duration<double>(kFxGiveUpSeconds);
			auto settle = [&](int &field, int unit) { if (field >= 0 && (field == unit || gaveUp)) field = -1; };
			settle(w.mode, m_fx.mode);
			settle(w.time, m_fx.time);
			settle(w.level, m_fx.level);
			for (unsigned p = 0; p < 8; p ++)
				settle(w.part[p], m_fx.partReverb[p]);
			settle(w.part[8], m_fx.rhythmReverb());
			want = w;
		}
		publishFx(want);
	}

	/// The messages that bring the unit to what @p w asks, as far as it differs: the reverb
	/// settings in one, a part's switch in one each, RHYTHM's in the rhythm setup's every key
	/// (its other bytes as the unit has them).  Under m_romLock and m_fxLock.
	void queueFx(const FxWant &w)
	{
		std::lock_guard<std::mutex> lock(m_outLock);
		m_outBytes.clear();
		m_outEnds.clear();
		m_outNext = 0;
		uint8_t msg[16 + 32 * 4];
		auto put = [&](size_t n) {
			m_outBytes.insert(m_outBytes.end(), msg, msg + n);
			m_outEnds.push_back(uint32_t(m_outBytes.size()));
		};
		const mt32::Effect &u = m_fx;
		const uint8_t rev[3] = { uint8_t(w.mode >= 0 ? w.mode : u.mode), uint8_t(w.time >= 0 ? w.time : u.time),
		                         uint8_t(w.level >= 0 ? w.level : u.level) };
		if (rev[0] != u.mode || rev[1] != u.time || rev[2] != u.level)
			put(dt1(0x10, 0x00, 0x01, rev, 3, msg));
		for (unsigned p = 0; p < 8; p ++)
			if (w.part[p] >= 0 && (w.part[p] != 0) != u.partReverb[p])
			{
				const uint8_t on = uint8_t(w.part[p]);
				put(dt1(0x03, 0x00, uint8_t(p * 0x10 + 6), &on, 1, msg));
			}
		if (w.part[8] >= 0)
		{
			uint8_t keys[mt32::Effect::kMaxRhythmKeys * 4];
			std::memcpy(keys, u.rhythm, sizeof keys);
			bool differs = false;
			for (unsigned k = 0; k < u.rhythmKeys; k ++)
			{
				differs = differs || keys[k * 4 + 3] != uint8_t(w.part[8]);
				keys[k * 4 + 3] = uint8_t(w.part[8]);
			}
			// 32 keys a message; the rhythm setup from 03 01 10, 4 bytes a key.
			for (unsigned k = 0; differs && k < u.rhythmKeys; k += 32)
			{
				const unsigned n = std::min(32u, u.rhythmKeys - k);
				const uint32_t lin = (0x03u << 14 | 0x01u << 7 | 0x10u) + k * 4;
				put(dt1(uint8_t(lin >> 14), uint8_t(lin >> 7 & 0x7F), uint8_t(lin & 0x7F), keys + k * 4, n * 4, msg));
			}
		}
		m_outPending.store(!m_outEnds.empty());
	}

	/// "fxstate" for the tab: the unit's reverb, with what the tab asked for over it.
	void publishFx(const FxWant &w)
	{
		std::string s;
		if (m_fxKnown)
		{
			const mt32::Effect &u = m_fx;
			char sw[10] = {};
			for (unsigned p = 0; p < 8; p ++)
				sw[p] = (w.part[p] >= 0 ? w.part[p] != 0 : u.partReverb[p]) ? '1' : '0';
			sw[8] = (w.part[8] >= 0 ? w.part[8] != 0 : u.rhythmReverb()) ? '1' : '0';
			char buf[48];
			std::snprintf(buf, sizeof buf, "%d %d %d %s", w.mode >= 0 ? w.mode : u.mode,
			              w.time >= 0 ? w.time : u.time, w.level >= 0 ? w.level : u.level, sw);
			s = buf;
		}
		std::lock_guard<std::mutex> lock(m_statusLock);
		if (s != m_fxState)
		{
			m_fxState = s;
			m_statusPending.store(true);
		}
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

		if (found.images.empty())
		{
			setStatus(pickedName + " is not an MT-32 ROM image, and its folder holds none either.");
			return;
		}
		bool pickedIsOne = false;
		std::vector<const RomScan::Image *> copy;
		for (const RomScan::Image &img : found.images)
		{
			pickedIsOne = pickedIsOne || (img.member.empty() && img.from.filename() == picked.filename())
			              || (!img.member.empty() && img.from == picked);
			if (!have.has(img.data))
				copy.push_back(&img);
		}

		// A loose file is copied, one out of a zip is written out under its own name: the
		// folder then holds plain files.  Never over a file of the same name.
		std::string target, error;
		for (const fs::path &to : installDirs())
		{
			if (copy.empty())
				break;
			std::error_code ec;
			fs::create_directories(to, ec);
			bool ok = !ec;
			for (const RomScan::Image *c : copy)
			{
				if (!ok)
					break;
				const fs::path name = c->member.empty() ? c->from.filename() : upath(c->member);
				fs::path dest = to / name;
				for (int i = 2; fs::exists(dest, ec); i ++)
					dest = to / upath(pstr(name.stem()) + " (" + std::to_string(i) + ")" + pstr(name.extension()));
				ok = writeFile(dest, c->data);
				if (!ok)
					error = pstr(dest) + ": cannot write";
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
			return;
		}

		rescanRoms();
		std::string msg = copy.empty() ? "Nothing new in " + pstr(dir) + "."
		                               : "Copied " + std::to_string(copy.size()) + " image(s) from " + pstr(dir) + " to " + target + ".";
		if (!pickedIsOne)
			msg += "  (" + pickedName + " itself is not one of them.)";
		bool any;
		{
			std::lock_guard<std::mutex> lock(m_romLock);
			any = !m_available.empty();
		}
		if (!any)
		{
			std::string status;
			{
				std::lock_guard<std::mutex> lock(m_statusLock);
				status = m_status;
			}
			setStatus(msg + "  " + status);
		}
		else
			setStatus(msg);
	}

	// ---- members ---------------------------------------------------------------------

	// The machine: run() and startMachine().  Lock order: m_romLock, m_machine, m_statusLock.
	mutable std::mutex m_machine;
	mt32::Core m_core;
	int m_knob = mt32::kKnobMaximum;            // where the unit's knob stands, 0 .. 1023
	uint32_t m_knobIdle = 0, m_knobSettle = 14400;  // frames since it was turned
	uint32_t m_keyMask = 0;
	std::atomic<bool> m_starting { false };
	std::atomic<bool> m_activated { false };
	double m_hostRate = 48000.0;

	// What the ROM folders hold.
	mutable std::mutex m_romLock;
	std::vector<std::vector<uint8_t>> m_images;
	std::vector<mt32::Machine> m_available;
	fs::path m_romWhere;
	std::string m_runningId;

	// The worker thread (queueJob, work).
	std::thread m_worker;
	std::mutex m_jobLock;
	std::condition_variable m_jobCv;
	std::string m_jobPicked;
	bool m_jobStart = false, m_quit = false;

	// What goes to the panel, and the session's strings.
	mutable std::mutex m_statusLock;
	std::string m_status, m_romSet, m_machines, m_wantMachine, m_uiSize;
	std::atomic<bool> m_statusPending { false };

	// The setup the session keeps: the last one known, and whether it came with the session
	// and is still to be brought to a machine.  Under m_statusLock.
	mutable std::string m_setup;
	bool m_setupApply = false;
	uint32_t m_setupGen = 0;
	// PART = CH; the panel's activity, after which run() reads the setup.
	std::atomic<bool> m_partCh { true };
	std::atomic<bool> m_panelTouched { false };     // since the last reading
	std::atomic<bool> m_readRequest { false };      // run() starts a reading
	std::atomic<uint32_t> m_blocks { 0 };           // blocks run() rendered
	uint32_t m_panelQuiet = 0, m_panelSettle = 12000;   // run(): frames since the panel was touched

	// The EFFECT tab.  Asked for, not yet sent: under m_fxLock.  The unit's reverb as the
	// worker last read it: under m_romLock.  The fxstate string: under m_statusLock.
	std::mutex m_fxLock;
	FxWant m_fxWant;
	bool m_fxUnsent = false;
	std::chrono::steady_clock::time_point m_fxTouched {}, m_fxSentAt {};
	mt32::Effect m_fx;
	bool m_fxKnown = false;
	uint64_t m_fxRev = 0;
	std::string m_fxState;
	// The parts' reverb switches this instance keeps (state "reverbsw"), and the channel each
	// part receives on (from the copy of the unit's memory; power-on: parts 1-8 on 2-9).
	std::atomic<bool> m_partSw[8] { true, true, true, true, true, true, true, true };
	std::atomic<uint8_t> m_partChan[8] { 1, 2, 3, 4, 5, 6, 7, 8 };
	// Messages the worker made for run() to send, one at a time (pumpOutbox): the bytes, and
	// where each message ends.  Under m_outLock; run() only tries it.
	std::mutex m_outLock;
	std::vector<uint8_t> m_outBytes;
	std::vector<uint32_t> m_outEnds;
	size_t m_outNext = 0;
	std::atomic<bool> m_outPending { false };
	uint32_t m_outWait = 0;                         // run(): frames until the next may go

	std::atomic<bool> m_keyDown[kNumKeys] {};
	std::atomic<uint32_t> m_keySeq[kNumKeys];
	std::atomic<int> m_knobSteps { 0 };
	uint32_t m_keySeen[kNumKeys] = {};
	bool m_keyHeld[kNumKeys] = {};
	uint32_t m_keyHeldFrames[kNumKeys] = {};
	uint32_t m_keyFreeFrames[kNumKeys] = {};    // since the key's last release
	uint32_t m_keyHoldFrames = 3840, m_keyGap = 1920;

	uint32_t m_publishPeriod = 1600, m_publishAccum = 0, m_publishesSame = 0;
	PanelBlob m_lastBlob {};
	char m_blobHex[sizeof(PanelBlob) * 2 + 1] = {};

	// The Windows standalone's own MIDI IN.
	StandaloneMidi m_midi { std::strstr(getPluginFormatName(), "Standalone") != nullptr, "Ostrich 32" };

	DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OstrichPlugin)
};

Plugin *createPlugin()
{
	return new OstrichPlugin();
}

END_NAMESPACE_DISTRHO
