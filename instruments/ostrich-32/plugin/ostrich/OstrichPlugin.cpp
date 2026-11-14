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
		}
		state.defaultValue = index == kStatePartCh ? "1" : "";
	}

	String getState(const char *key) const override
	{
		if (std::strcmp(key, "setup") == 0)
			return String(currentSetup().c_str());
		if (std::strcmp(key, "partch") == 0)
			return String(m_partCh.load() ? "1" : "0");
		std::lock_guard<std::mutex> lock(m_statusLock);
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
			const int to = std::clamp(m_knob + steps, 0, kKnobSteps);
			if (to != m_knob)
			{
				m_core.panelKnob(to - m_knob);
				m_panelDirty = true;
				m_panelQuiet = 0;
			}
			m_knob = to;
		}

		// What MIDI changes, the core's copy of the unit's memory follows as it goes; what the
		// panel changes, it learns by asking the unit, once the panel has been left alone.
		if (m_panelDirty && (m_panelQuiet += frames) >= m_panelSettle && !m_core.busy())
		{
			m_core.readSetup();
			m_panelDirty = false;
		}

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
			m_core.midi(data, size, std::min(frame, frames - 1));
		}
		m_core.render(outL, outR, frames);
		publishPanel(frames, true);
	}

private:
	static constexpr double kPanelRefreshHz = 30.0;
	static constexpr double kKeyHoldSeconds = 0.08;     // long enough for the panel scan
	static constexpr double kKeyGapSeconds = 0.04;
	static constexpr uint32_t kKeyMaxQueued = 16;
	static constexpr double kPanelSettleSeconds = 0.5;  // then the setup is read (run())

	void setupRate()
	{
		m_publishPeriod = uint32_t(m_hostRate / kPanelRefreshHz);
		m_keyHoldFrames = uint32_t(m_hostRate * kKeyHoldSeconds);
		m_keyGap = uint32_t(m_hostRate * kKeyGapSeconds);
		m_panelSettle = uint32_t(m_hostRate * kPanelSettleSeconds);
		for (auto &f : m_keyFreeFrames) f = m_keyGap;   // the first press goes at once
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
			m_panelDirty = true;
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
				std::memcpy(blob.text, text, sizeof(blob.text));
			blob.flags = uint8_t(1 | (lamp ? 2 : 0));
			blob.knob = uint8_t(m_knob);
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
				m_knob = kKnobSteps;        // a new machine's knob stands fully clockwise
				m_keyMask = 0;
				m_panelDirty = false;
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

	/// "mt32old" of "mt32old/1.04".
	static std::string boardOf(const std::string &id) { return id.substr(0, id.find('/')); }

	/// What the session keeps of the unit, "<machine> <SysEx as hex> ...": the running
	/// machine's setup, else (none runs, or a restore is under way) the last one known.
	std::string currentSetup() const
	{
		std::lock_guard<std::mutex> romLock(m_romLock);     // no machine starts or stops meanwhile
		{
			std::lock_guard<std::mutex> lock(m_statusLock);
			if (m_setupApply)
				return m_setup;                 // the session's, not in a machine yet
		}
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
			m_jobCv.wait(lock, [this] { return m_quit || m_jobStart || !m_jobPicked.empty(); });
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
			lock.lock();
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
	int m_knob = kKnobSteps;                    // where the firmware's knob stands
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
	bool m_panelDirty = false;
	uint32_t m_panelQuiet = 0, m_panelSettle = 24000;

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
