// SPDX-License-Identifier: GPL-2.0-or-later
//
// Ostrich 55 -- the core around Nuked-SC55 (sc55_core.h).
//
// LCD layout of the SC-55 (mk1 and mk2), as Nuked-SC55's lcd.cpp draws it; offsets into
// LcdState::data:
//    0..2    PART             3..18   INSTRUMENT (16 characters)
//   40..42   LEVEL           43..45   PAN
//   49..51   REVERB          46..48   CHORUS
//   52..54   KEY SHIFT       55..57   MIDI CH
//   58       the L/R marks (bit 0 of the glyph's first row)
//   20..23, 60..63  the 16 x 16 dot bar display: custom glyphs, 8 rows of 5 dots each,
//                   the fourth of each row only one dot wide

#include "sc55_core.h"

#include "emu.h"
#include "rom.h"
#include "rom_io.h"
#include "sha256.h"
#include "standard_romsets.h"

#include <atomic>
#include <cstring>
#include <mutex>

namespace sc55 {

namespace {

/// LCD_Write keeps the display's state only when a backend observes it; this one draws
/// nothing, the plugin reads the state instead.
class NullLcd final : public LCD_Backend
{
public:
	bool Start(const lcd_t &) override { return true; }
	void Stop() override {}
	void Render() override {}
};

} // namespace

const char *romLocationName(unsigned location)
{
	return location < kRomLocations ? ToCString(RomLocation(location)) : "?";
}

std::vector<RomsetPart> identifyRom(const uint8_t *data, size_t size)
{
	std::vector<RomsetPart> parts;
	SHA256_Digest digest;
	if (!SHA256_HashBytes(std::span<uint8_t>(const_cast<uint8_t *>(data), size), digest))
		return parts;
	for (const RomsetDefinition &def : GetStandardRomsetDefinitions())
		for (const ::RomLocation loc : def.hashes.GetValidLocations())
			if (def.GetHash(loc) == digest)
				parts.push_back({ def.name, unsigned(loc) });
	return parts;
}

std::vector<std::string> romsetsOfFamily(const char *family)
{
	std::vector<std::string> names;
	Romset romset;
	if (!ParseRomsetName(family, romset))
		return names;
	for (const RomsetDefinition &def : GetStandardRomsetDefinitions())
		if (def.romset == romset)
			names.push_back(def.name);
	return names;
}

std::vector<unsigned> romsetLocations(const std::string &name)
{
	std::vector<unsigned> locations;
	for (const RomsetDefinition &def : GetStandardRomsetDefinitions())
		if (name == def.name)
			for (const ::RomLocation loc : def.hashes.GetValidLocations())
				locations.push_back(unsigned(loc));
	return locations;
}

struct Core::Impl
{
	Emulator emu;
	NullLcd lcd;
	RomsetInfo roms;            ///< must outlive the emulator (emu.h, LoadRoms)

	// render() hands these to the sample callback; Step() delivers one frame at a time.
	float *outL = nullptr, *outR = nullptr;
	uint32_t want = 0, got = 0;

	std::atomic<uint32_t> buttons { 0 };

	static void onSample(void *user, const AudioFrame<int32_t> &in)
	{
		Impl &self = *static_cast<Impl *>(user);
		if (self.got >= self.want)
			return;     // a second frame in the same Step(): cannot happen, but stay inside the block
		AudioFrame<float> f;
		Normalize(in, f);
		self.outL[self.got] = f.left;
		self.outR[self.got] = f.right;
		self.got ++;
	}
};

Core::Core() : m(std::make_unique<Impl>()) {}
Core::~Core() = default;

bool Core::start(const std::string &romset, const std::array<std::string, kRomLocations> &paths,
		std::string &error)
{
	m_running = false;
	m = std::make_unique<Impl>();

	const RomsetDefinition *def = nullptr;
	for (const RomsetDefinition &d : GetStandardRomsetDefinitions())
		if (romset == d.name)
			def = &d;
	if (def == nullptr)
	{
		error = "unknown ROM set " + romset;
		return false;
	}

	for (unsigned i = 0; i < kRomLocations; i ++)
		if (def->ContainsHash(::RomLocation(i)))
		{
			if (paths[i].empty())
			{
				error = std::string("no image for ") + romLocationName(i);
				return false;
			}
			m->roms.rom_paths[i] = std::filesystem::u8path(paths[i]);
		}

	RomLoadStatusSet status;
	if (!LoadRomset(m->roms, &status))
	{
		for (unsigned i = 0; i < kRomLocations; i ++)
			if (status[i] == RomLoadStatus::Failed)
			{
				error = std::string("cannot read ") + paths[i];
				return false;
			}
		error = "cannot read the ROM images";
		return false;
	}

	if (!m->emu.Init({ .lcd_backend = &m->lcd, .nvram_filename = {} }))
	{
		error = "out of memory";
		return false;
	}
	if (!m->emu.LoadRoms(def->romset, m->roms))
	{
		error = "the emulator refused the ROM images";
		return false;
	}
	m->emu.Reset();
	m->emu.GetPCM().enable_oversampling = true;
	m->emu.StartLCD();
	m->emu.SetSampleCallback(&Impl::onSample, m.get());

	m_romset = romset;
	m_running = true;
	return true;
}

double Core::sampleRate() const
{
	return m_running ? double(PCM_GetOutputFrequency(m->emu.GetPCM())) : 64000.0;
}

void Core::setOversampling(bool on)
{
	if (m_running)
		m->emu.GetPCM().enable_oversampling = on;
}

void Core::midi(const uint8_t *data, size_t size)
{
	if (m_running)
		m->emu.PostMIDI(std::span<const uint8_t>(data, size));
}

void Core::render(float *left, float *right, uint32_t frames)
{
	if (!m_running)
	{
		std::memset(left, 0, frames * sizeof(float));
		std::memset(right, 0, frames * sizeof(float));
		return;
	}
	m->emu.GetMCU().button_pressed = m->buttons.load(std::memory_order_relaxed);
	m->outL = left;
	m->outR = right;
	m->want = frames;
	m->got = 0;
	while (m->got < frames)
		m->emu.Step();
}

void Core::setButtons(uint32_t mask)
{
	m->buttons.store(mask, std::memory_order_relaxed);
}

bool Core::lcd(LcdState &out)
{
	if (!m_running)
		return false;
	lcd_t &lcd = m->emu.GetLCD();
	std::unique_lock lock(lcd.mutex, std::try_to_lock);
	if (!lock.owns_lock())
		return false;
	std::memcpy(out.data, lcd.LCD_Data, sizeof(out.data));
	std::memcpy(out.cgram, lcd.LCD_CG, sizeof(out.cgram));
	out.enabled = lcd.enable;
	return true;
}

} // namespace sc55
