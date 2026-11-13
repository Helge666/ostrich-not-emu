// SPDX-License-Identifier: GPL-3.0-or-later
//
// mt32_core -- see mt32_core.h.  88emu is used through its C interface for running a
// machine and through its ROM registry (a header of size and digest per known image) for
// telling the user's images apart; nothing of 88emu is changed.

#include "mt32_core.h"

#include "88lib/c_interface.h"
#include "88lib/rom/romRegistry.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <set>

namespace fs = std::filesystem;
using namespace emu88Lib;

namespace mt32 {

namespace {

// The LA boards Ostrich 32 runs, in the order they are offered.
struct Board { RomDevice rom; const char *id; const char *name; emu88_device_id device; };
const Board kBoards[] = {
	{ RomDevice::Mt32Old, "mt32old", "MT-32 1.x", EMU88_DEVICE_MT32_OLD },
	{ RomDevice::Mt32New, "mt32new", "MT-32 2.x", EMU88_DEVICE_MT32_NEW },
	{ RomDevice::Cm32l,   "cm32l",   "CM-32L",    EMU88_DEVICE_CM32L },
	{ RomDevice::Cm32ln,  "cm32ln",  "CM-32LN",   EMU88_DEVICE_CM32LN },
};

const char *slotName(RomSlot s)
{
	switch (s)
	{
	case RomSlot::Control: return "control";
	case RomSlot::Wave:    return "wave";
	case RomSlot::Reverb:  return "reverb";
	default:               return nullptr;
	}
}

// The registry rows an image matches (one image can fill several rows: the PCM mask ROM
// R15449121 is the MT-32's whole wave ROM and the first chip of the CM-32L's).
std::vector<const RomRegistryEntry *> matches(const uint8_t *data, size_t size)
{
	std::vector<const RomRegistryEntry *> out;
	bool any = false;
	for (const auto &e : g_romRegistry)
		if (e.size == size) { any = true; break; }
	if (!any)
		return out;
	const baseLib::MD5 md5(data, uint32_t(size));
	const baseLib::SHA1 sha1(data, size);
	const baseLib::SHA256 sha256(data, size);
	for (const auto &e : g_romRegistry)
	{
		if (e.size != size || !slotName(e.slot))
			continue;
		bool la = false;
		for (const Board &b : kBoards)
			la |= usedBy(e, b.rom);
		if (!la)
			continue;
		if ((e.hash.isValid() && e.hash == md5) || (e.sha1.isValid() && e.sha1 == sha1)
		    || (e.sha256.isValid() && e.sha256 == sha256))
			out.push_back(&e);
	}
	return out;
}

// "1.04, 14 July 87" -> "1.04"; "1.04 A, IC27" -> "1.04"; "Blue Ridge B" -> "Blue Ridge".
std::string firmwareName(const RomRegistryEntry &e)
{
	std::string v = e.version;
	v = v.substr(0, v.find(','));
	if (e.index != 0 && v.size() > 2 && v[v.size() - 2] == ' ' && (v.back() == 'A' || v.back() == 'B'))
		v.resize(v.size() - 2);
	return v;
}

// What the images offer one board: per slot, the images for the whole (index 0) and for
// each chip (index 1, 2); control ROMs further by firmware.
struct Offer
{
	std::map<std::string, std::map<int, size_t>> control;   // firmware -> index -> image
	std::map<int, size_t> wave;                              // index -> image
	std::map<int, size_t> reverb;
};

Offer offerFor(const Board &b, const std::vector<std::vector<uint8_t>> &images)
{
	Offer o;
	for (size_t i = 0; i < images.size(); i ++)
		for (const RomRegistryEntry *e : matches(images[i].data(), images[i].size()))
		{
			if (!usedBy(*e, b.rom))
				continue;
			if (e->slot == RomSlot::Control)
				o.control[firmwareName(*e)].emplace(e->index, i);
			else if (e->slot == RomSlot::Wave)
				o.wave.emplace(e->index, i);
			else if (e->slot == RomSlot::Reverb)
				o.reverb.emplace(e->index, i);
		}
	return o;
}

// The images that make a slot complete: the whole, else both chips; empty if neither.
std::vector<size_t> complete(const std::map<int, size_t> &slot)
{
	if (auto it = slot.find(0); it != slot.end())
		return { it->second };
	if (slot.count(1) && slot.count(2))
		return { slot.at(1), slot.at(2) };
	return {};
}

// 88emu's ROM search path is process-wide: one machine opens at a time.
std::mutex g_openMutex;

} // namespace

bool identifyRom(const uint8_t *data, size_t size, RomId &out)
{
	const auto found = matches(data, size);
	if (found.empty())
		return false;
	const RomRegistryEntry &e = *found.front();
	out = RomId();
	out.slot = slotName(e.slot);
	out.version = e.version;
	out.chip = e.index != 0;
	for (const Board &b : kBoards)
		for (const RomRegistryEntry *f : found)
			if (usedBy(*f, b.rom))
			{
				out.devices += (out.devices.empty() ? "" : ", ") + std::string(b.name);
				break;
			}
	return true;
}

std::vector<Machine> findMachines(const std::vector<std::vector<uint8_t>> &images)
{
	std::vector<Machine> out;
	for (const Board &b : kBoards)
	{
		const Offer o = offerFor(b, images);
		if (complete(o.wave).empty() || complete(o.reverb).empty())
			continue;
		for (const auto &[firmware, chips] : o.control)      // std::map: sorted by name
			if (!complete(chips).empty())
				out.push_back({ std::string(b.id) + "/" + firmware,
				                std::string(b.name) + ", control ROM " + firmware });
	}
	return out;
}

const char *libraryVersion()
{
	return emu88_get_library_version_string();
}

// ---------------------------------------------------------------------------------------

struct Core::Impl
{
	emu88_context ctx = nullptr;

	static constexpr uint32_t kChunk = 256;
	float buffer[kChunk * 2];

	struct Event { uint32_t offset; uint32_t msg; uint32_t sysexAt, sysexLen; };
	std::vector<Event> events;           // sorted by offset, capacity reserved
	std::vector<uint8_t> sysex;          // the queued SysEx bytes, capacity reserved
	size_t sysexUsed = 0;

	Impl()
	{
		events.reserve(2048);
		sysex.resize(65536);
	}

	~Impl() { close(); }

	void close()
	{
		if (ctx)
			emu88_free_context(ctx);
		ctx = nullptr;
		events.clear();
		sysexUsed = 0;
	}

	void renderChunk(float *l, float *r, uint32_t frames)
	{
		while (frames)
		{
			const uint32_t n = std::min(frames, kChunk);
			emu88_render_float(ctx, buffer, n);
			for (uint32_t i = 0; i < n; i ++)
			{
				l[i] = buffer[i * 2];
				r[i] = buffer[i * 2 + 1];
			}
			l += n; r += n; frames -= n;
		}
	}

	void play(const Event &e)
	{
		if (e.sysexLen)
			emu88_play_sysex(ctx, sysex.data() + e.sysexAt, e.sysexLen);
		else
			emu88_play_msg(ctx, e.msg);
	}
};

Core::Core() : m(new Impl) {}

Core::~Core() = default;

bool Core::start(const std::string &machine, const std::vector<std::vector<uint8_t>> &images,
                 double sampleRate, std::string &error)
{
	stop();
	const size_t slash = machine.find('/');
	const Board *board = nullptr;
	for (const Board &b : kBoards)
		if (machine.compare(0, slash, b.id) == 0)
			board = &b;
	if (!board || slash == std::string::npos)
	{
		error = "unknown machine \"" + machine + "\"";
		return false;
	}
	const Offer o = offerFor(*board, images);
	const auto control = o.control.find(machine.substr(slash + 1));
	const std::vector<size_t> wave = complete(o.wave), reverb = complete(o.reverb);
	if (control == o.control.end() || complete(control->second).empty() || wave.empty() || reverb.empty())
	{
		error = "the images do not make " + machine + " complete";
		return false;
	}

	// A private folder with exactly this machine's images: 88emu finds ROMs by searching
	// folders and would otherwise pick the firmware it likes best.
	std::error_code ec;
	std::random_device rd;
	const fs::path dir = fs::temp_directory_path(ec) / ("ostrich32-" + std::to_string(rd()) + std::to_string(rd()));
	fs::create_directories(dir, ec);
	if (ec)
	{
		error = "cannot create a folder in the temp folder: " + ec.message();
		return false;
	}
	int n = 0;
	auto put = [&](size_t i) {
		std::ofstream f(dir / ("rom" + std::to_string(n ++) + ".bin"), std::ios::binary);
		f.write(reinterpret_cast<const char *>(images[i].data()), std::streamsize(images[i].size()));
		return bool(f);
	};
	bool written = true;
	for (size_t i : complete(control->second)) written &= put(i);
	for (size_t i : wave) written &= put(i);
	for (size_t i : reverb) written &= put(i);

	int rc = EMU88_RC_FAILED;
	if (written)
	{
		std::lock_guard<std::mutex> lock(g_openMutex);
		emu88_set_rom_path(dir.string().c_str());
		m->ctx = emu88_create_context();
		emu88_select_device(m->ctx, board->device);
		emu88_set_stereo_output_samplerate(m->ctx, sampleRate);
		rc = emu88_open_synth(m->ctx);
	}
	fs::remove_all(dir, ec);
	if (!written)
	{
		error = "cannot write the images to " + dir.string();
		return false;
	}
	if (rc != EMU88_RC_OK)
	{
		m->close();
		error = "88emu could not open " + machine + " (code " + std::to_string(rc) + ")";
		return false;
	}
	m_running = true;
	m_machine = machine;
	m_rate = sampleRate;
	return true;
}

void Core::stop()
{
	m->close();
	m_running = false;
	m_machine.clear();
	m_rate = 0;
}

void Core::midi(const uint8_t *data, size_t size, uint32_t offset)
{
	if (!m_running || !size || m->events.size() == m->events.capacity())
		return;
	Impl::Event e { offset, 0, 0, 0 };
	if (data[0] == 0xF0)
	{
		if (m->sysexUsed + size > m->sysex.size())
			return;
		std::memcpy(m->sysex.data() + m->sysexUsed, data, size);
		e.sysexAt = uint32_t(m->sysexUsed);
		e.sysexLen = uint32_t(size);
		m->sysexUsed += size;
	}
	else
	{
		for (size_t i = 0; i < std::min<size_t>(size, 3); i ++)
			e.msg |= uint32_t(data[i]) << (8 * i);
	}
	// keep the queue sorted, events with the same offset in arrival order
	auto it = std::upper_bound(m->events.begin(), m->events.end(), offset,
	                           [](uint32_t o, const Impl::Event &x) { return o < x.offset; });
	m->events.insert(it, e);
}

void Core::render(float *left, float *right, uint32_t frames)
{
	if (!m_running)
	{
		std::fill_n(left, frames, 0.f);
		std::fill_n(right, frames, 0.f);
		return;
	}
	// 88emu takes a message with the next frame it renders: render up to each event's
	// offset, then hand it over.
	uint32_t pos = 0;
	for (const Impl::Event &e : m->events)
	{
		const uint32_t at = std::min(e.offset, frames);
		if (at > pos)
		{
			m->renderChunk(left + pos, right + pos, at - pos);
			pos = at;
		}
		m->play(e);
	}
	m->events.clear();
	m->sysexUsed = 0;
	if (frames > pos)
		m->renderChunk(left + pos, right + pos, frames - pos);
}

void Core::panelButtons(uint32_t buttons)
{
	if (m_running)
		emu88_set_panel_buttons(m->ctx, buttons);
}

void Core::panelKnob(int steps)
{
	if (m_running && steps)
		emu88_turn_panel_encoder(m->ctx, steps);
}

bool Core::display(char text[21], bool &midiLamp) const
{
	std::memset(text, ' ', 20);
	text[20] = 0;
	midiLamp = false;
	if (!m_running)
		return false;
	char buf[64];
	emu88_get_display_text(m->ctx, 0, buf, sizeof buf);
	for (int i = 0; i < 20 && buf[i] && buf[i] != '\n'; i ++)
		text[i] = buf[i];
	midiLamp = (emu88_get_panel_leds(m->ctx) & 1) != 0;
	return true;
}

void Core::silence()
{
	if (m_running)
		emu88_play_silence(m->ctx);
}

} // namespace mt32
