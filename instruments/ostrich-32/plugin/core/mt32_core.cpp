// SPDX-License-Identifier: LGPL-2.1-or-later
//
// mt32_core -- see mt32_core.h.  Everything that makes the sound is libmt32emu's (Munt:
// Dean Beeler, Jerome Fisher, Sergey V. Mikayev and contributors); this file only chooses
// the ROM images, feeds MIDI in at the right time and resamples to the host's rate.

#include "mt32_core.h"

#include <mt32emu.h>

#include <algorithm>
#include <cstdarg>
#include <cstring>

namespace mt32 {

using namespace MT32Emu;

namespace {

// libmt32emu talks on stdout by default; a plugin must not.
struct QuietReportHandler : ReportHandler
{
	void printDebug(const char *, va_list) override {}
	void showLCDMessage(const char *) override {}
};

const MachineConfiguration *findConfiguration(const std::string &id)
{
	Bit32u n = 0;
	const MachineConfiguration * const *all = MachineConfiguration::getAllMachineConfigurations(&n);
	for (Bit32u i = 0; i < n; i ++)
		if (id == all[i]->getMachineID())
			return all[i];
	return nullptr;
}

/// The images of one machine, as libmt32emu's File objects over the caller's data.
struct Choice
{
	ArrayFile *full = nullptr;            // a whole ROM
	ArrayFile *half[2] = {};              // or its two halves / interleaved chips
	bool complete() const { return full || (half[0] && half[1]); }
};

/// Picks, for one machine, the control and the PCM ROM from `files`; whole images first.
void choose(const MachineConfiguration &cfg, const std::vector<std::unique_ptr<ArrayFile>> &files,
            Choice &control, Choice &pcm)
{
	const ROMInfo * const *compatible = cfg.getCompatibleROMInfos();
	std::vector<std::pair<ArrayFile *, const ROMInfo *>> known;
	for (const auto &f : files)
		if (const ROMInfo *ri = ROMInfo::getROMInfo(f.get(), compatible))
			known.emplace_back(f.get(), ri);

	for (Choice *c : { &control, &pcm })
	{
		const ROMInfo::Type type = c == &control ? ROMInfo::Control : ROMInfo::PCM;
		for (const auto &[f, ri] : known)
			if (ri->type == type && ri->pairType == ROMInfo::Full && !c->full)
				c->full = f;
		if (c->full)
			continue;
		// A half whose partner is present too.  pairROMInfo links the two halves.
		for (const auto &[f, ri] : known)
		{
			if (ri->type != type || ri->pairType == ROMInfo::Full || !ri->pairROMInfo)
				continue;
			for (const auto &[g, rj] : known)
				if (rj == ri->pairROMInfo)
				{
					c->half[0] = f;
					c->half[1] = g;
					break;
				}
			if (c->half[0])
				break;
		}
	}
}

std::vector<std::unique_ptr<ArrayFile>> wrap(const std::vector<std::vector<uint8_t>> &images)
{
	std::vector<std::unique_ptr<ArrayFile>> files;
	for (const auto &img : images)
		if (!img.empty())
			files.push_back(std::make_unique<ArrayFile>(img.data(), size_t(img.size())));
	return files;
}

const ROMImage *makeImage(const Choice &c, const ROMInfo * const *compatible)
{
	if (c.full)
		return ROMImage::makeROMImage(c.full, compatible);
	return ROMImage::makeROMImage(c.half[0], c.half[1]);
}

} // namespace

bool identifyRom(const uint8_t *data, size_t size, RomId &out)
{
	ArrayFile f(data, size);
	const ROMInfo *ri = ROMInfo::getROMInfo(&f);
	if (!ri)
		return false;
	out.name = ri->shortName;
	out.description = ri->description;
	out.control = ri->type == ROMInfo::Control;
	out.partial = ri->pairType != ROMInfo::Full;
	ROMInfo::freeROMInfo(ri);
	return true;
}

std::vector<Machine> findMachines(const std::vector<std::vector<uint8_t>> &images)
{
	std::vector<Machine> out;
	const auto files = wrap(images);
	Bit32u n = 0;
	const MachineConfiguration * const *all = MachineConfiguration::getAllMachineConfigurations(&n);
	for (Bit32u i = 0; i < n; i ++)
	{
		Choice control, pcm;
		choose(*all[i], files, control, pcm);
		if (!control.complete() || !pcm.complete())
			continue;
		const ROMInfo *ri = ROMInfo::getROMInfo(control.full ? control.full : control.half[0]);
		out.push_back({ all[i]->getMachineID(), ri ? ri->description : all[i]->getMachineID() });
		if (ri)
			ROMInfo::freeROMInfo(ri);
	}
	return out;
}

const char *libraryVersion()
{
	return Synth::getLibraryVersionString();
}

// ---------------------------------------------------------------------------------------

struct Core::Impl
{
	static constexpr uint32_t kChunk = 256;   // frames per call into the resampler

	std::vector<std::vector<uint8_t>> images;          // the data the files point at
	std::vector<std::unique_ptr<ArrayFile>> files;
	const ROMImage *control = nullptr;
	const ROMImage *pcm = nullptr;
	QuietReportHandler report;
	std::unique_ptr<Synth> synth;
	std::unique_ptr<SampleRateConverter> src;
	float buffer[2 * kChunk];

	~Impl() { close(); }

	void close()
	{
		src.reset();
		synth.reset();                      // before the images: the synth uses them
		if (control) ROMImage::freeROMImage(control);
		if (pcm) ROMImage::freeROMImage(pcm);
		control = pcm = nullptr;
		files.clear();
		images.clear();
	}
};

Core::Core() : m(std::make_unique<Impl>()) {}
Core::~Core() = default;

bool Core::start(const std::string &machine, std::vector<std::vector<uint8_t>> images,
                 double sampleRate, std::string &error)
{
	stop();
	const MachineConfiguration *cfg = findConfiguration(machine);
	if (!cfg)
	{
		error = "unknown machine " + machine;
		return false;
	}
	m->images = std::move(images);
	m->files = wrap(m->images);
	Choice control, pcm;
	choose(*cfg, m->files, control, pcm);
	if (!control.complete() || !pcm.complete())
	{
		error = std::string(!control.complete() ? "control ROM" : "PCM ROM") + " missing for " + machine;
		m->close();
		return false;
	}
	const ROMInfo * const *compatible = cfg->getCompatibleROMInfos();
	m->control = makeImage(control, compatible);
	m->pcm = makeImage(pcm, compatible);
	if (!m->control || !m->pcm)
	{
		error = "libmt32emu did not accept the ROM images for " + machine;
		m->close();
		return false;
	}

	m->synth = std::make_unique<Synth>(&m->report);
	const AnalogOutputMode analog = SampleRateConverter::getBestAnalogOutputMode(sampleRate);
	if (!m->synth->open(*m->control, *m->pcm, analog))
	{
		error = "libmt32emu could not open " + machine;
		m->close();
		return false;
	}
	// SysEx in one preallocated buffer: safe on the audio thread.
	m->synth->configureMIDIEventQueueSysexStorage(32768);
	m->src = std::make_unique<SampleRateConverter>(*m->synth, sampleRate,
	                                               SamplerateConversionQuality_GOOD);
	m_machine = machine;
	m_rate = sampleRate;
	m_running = true;
	return true;
}

void Core::stop()
{
	m->close();
	m_running = false;
	m_machine.clear();
}

void Core::midi(const uint8_t *data, size_t size, uint32_t offset)
{
	if (!m_running || size == 0)
		return;
	const Bit32u when = m->synth->getInternalRenderedSampleCount()
	                  + Bit32u(m->src->convertOutputToSynthTimestamp(offset));
	if (data[0] == 0xF0)
	{
		m->synth->playSysex(data, Bit32u(size), when);
		return;
	}
	if (data[0] < 0x80 || data[0] >= 0xF8)
		return;
	Bit32u msg = data[0];
	if (size > 1) msg |= Bit32u(data[1]) << 8;
	if (size > 2) msg |= Bit32u(data[2]) << 16;
	m->synth->playMsg(msg, when);
}

void Core::render(float *left, float *right, uint32_t frames)
{
	if (!m_running)
	{
		std::fill(left, left + frames, 0.f);
		std::fill(right, right + frames, 0.f);
		return;
	}
	while (frames > 0)
	{
		const uint32_t n = std::min(frames, Impl::kChunk);
		m->src->getOutputSamples(m->buffer, n);
		for (uint32_t i = 0; i < n; i ++)
		{
			*left ++ = m->buffer[2 * i];
			*right ++ = m->buffer[2 * i + 1];
		}
		frames -= n;
	}
}

bool Core::display(char text[21], bool &midiLamp) const
{
	if (!m_running)
		return false;
	midiLamp = m->synth->getDisplayState(text);
	return true;
}

void Core::mainDisplay()
{
	if (m_running)
		m->synth->setMainDisplayMode();
}

uint32_t Core::partStates() const
{
	return m_running ? m->synth->getPartStates() : 0;
}

} // namespace mt32
