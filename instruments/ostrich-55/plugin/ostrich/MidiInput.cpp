// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich 55 -- MIDI IN for the Windows standalone (MidiInput.hpp).  windows.h stays in
// this file.

#include "MidiInput.hpp"

#include <algorithm>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <mmsystem.h>
#endif

namespace ostrich {

#ifdef _WIN32

namespace {

std::string utf8(const wchar_t *w)
{
	const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	if (n <= 1)
		return std::string();
	std::string s(size_t(n - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
	return s;
}

/// The length of a short message by its status byte (WinMM always hands over the status).
size_t shortLength(uint8_t status)
{
	if (status < 0x80)
		return 0;
	if (status < 0xF0)
		return (status & 0xE0) == 0xC0 ? 2 : 3;     // program change, channel pressure: 2
	switch (status)
	{
	case 0xF1: case 0xF3: return 2;
	case 0xF2: return 3;
	default: return 1;                              // tune request, real time
	}
}

} // anonymous namespace

struct MidiInput::Port
{
	static constexpr int kBuffers = 4;
	static constexpr DWORD kBufferSize = 4096;      // a GS setup message is far shorter

	MidiInput *owner = nullptr;
	HMIDIIN handle = nullptr;
	MIDIHDR hdr[kBuffers] = {};
	char buf[kBuffers][kBufferSize] = {};
	std::atomic<bool> closing { false };

	static void CALLBACK proc(HMIDIIN h, UINT msg, DWORD_PTR instance, DWORD_PTR p1, DWORD_PTR)
	{
		Port *const port = reinterpret_cast<Port *>(instance);
		if (msg == MIM_DATA)
		{
			const uint8_t b[3] = { uint8_t(p1), uint8_t(p1 >> 8), uint8_t(p1 >> 16) };
			if (const size_t n = shortLength(b[0]))
				port->owner->push(b, n);
		}
		else if (msg == MIM_LONGDATA)
		{
			// SysEx, a buffer at a time.  Handed back for the next one, as RtMidi does, unless
			// the port is closing (midiInReset returns every buffer this way).
			MIDIHDR *const hdr = reinterpret_cast<MIDIHDR *>(p1);
			if (hdr->dwBytesRecorded > 0)
				port->owner->push(reinterpret_cast<const uint8_t *>(hdr->lpData), hdr->dwBytesRecorded);
			if (!port->closing.load())
				midiInAddBuffer(h, hdr, sizeof(MIDIHDR));
		}
	}

	bool open(UINT id)
	{
		if (midiInOpen(&handle, id, DWORD_PTR(&Port::proc), DWORD_PTR(this), CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
		{
			handle = nullptr;
			return false;
		}
		for (int i = 0; i < kBuffers; i ++)
		{
			hdr[i].lpData = buf[i];
			hdr[i].dwBufferLength = kBufferSize;
			midiInPrepareHeader(handle, &hdr[i], sizeof(MIDIHDR));
			midiInAddBuffer(handle, &hdr[i], sizeof(MIDIHDR));
		}
		midiInStart(handle);
		return true;
	}

	~Port()
	{
		if (handle == nullptr)
			return;
		closing.store(true);
		midiInStop(handle);
		midiInReset(handle);
		for (int i = 0; i < kBuffers; i ++)
			midiInUnprepareHeader(handle, &hdr[i], sizeof(MIDIHDR));
		midiInClose(handle);
	}
};

bool MidiInput::supported() { return true; }

std::vector<std::string> MidiInput::ports()
{
	std::vector<std::string> names;
	const UINT n = midiInGetNumDevs();
	for (UINT i = 0; i < n; i ++)
	{
		MIDIINCAPSW caps {};
		std::string name;
		if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
			name = utf8(caps.szPname);
		if (name.empty())
			name = "MIDI input " + std::to_string(i + 1);
		const std::string base = name;
		for (int k = 2; std::find(names.begin(), names.end(), name) != names.end(); k ++)
			name = base + " (" + std::to_string(k) + ")";
		names.push_back(name);
	}
	return names;
}

std::string MidiInput::open(const std::string &choice)
{
	close();
	if (choice == kOff)
		return std::string();
	const std::vector<std::string> names = ports();
	std::string failed;
	bool found = false;
	for (UINT i = 0; i < names.size(); i ++)
	{
		if (choice != kAll && names[i] != choice)
			continue;
		found = true;
		auto port = std::make_unique<Port>();
		port->owner = this;
		if (port->open(i))
			m_open.push_back(std::move(port));
		else
			failed += (failed.empty() ? "" : ", ") + names[i];
	}
	if (choice == kAll)
		return failed.empty() ? std::string() : "in use by another program: " + failed;
	if (!found)
		return "not connected";
	return failed.empty() ? std::string() : "in use by another program";
}

#else

struct MidiInput::Port {};

bool MidiInput::supported() { return false; }
std::vector<std::string> MidiInput::ports() { return {}; }
std::string MidiInput::open(const std::string &) { return std::string(); }

#endif

MidiInput::MidiInput() = default;

MidiInput::~MidiInput()
{
	close();
}

void MidiInput::close()
{
	m_open.clear();
}

void MidiInput::push(const uint8_t *data, size_t n)
{
	std::lock_guard<std::mutex> lock(m_push);
	const size_t w = m_write.load(std::memory_order_relaxed);
	const size_t r = m_read.load(std::memory_order_acquire);
	if (kRing - (w - r) < n)
		return;
	for (size_t i = 0; i < n; i ++)
		m_ring[(w + i) & (kRing - 1)] = data[i];
	m_write.store(w + n, std::memory_order_release);
}

size_t MidiInput::read(uint8_t *out, size_t n)
{
	const size_t r = m_read.load(std::memory_order_relaxed);
	const size_t w = m_write.load(std::memory_order_acquire);
	const size_t k = std::min(n, w - r);
	for (size_t i = 0; i < k; i ++)
		out[i] = m_ring[(r + i) & (kRing - 1)];
	m_read.store(r + k, std::memory_order_release);
	return k;
}

} // namespace ostrich
