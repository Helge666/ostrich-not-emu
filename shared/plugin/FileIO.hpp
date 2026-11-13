// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich != Emu (shared/plugin; Ostrich 32, 55 and 220) -- whole files in and out, with
// wide paths on Windows (_wfopen), so that ROM folders with any name work there too.

#ifndef OSTRICH_FILE_IO_HPP
#define OSTRICH_FILE_IO_HPP

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

namespace ostrich {

/// The whole file, or nothing if it is missing, empty, unreadable or larger than `limit`.
inline std::vector<uint8_t> readFile(const std::filesystem::path &p, size_t limit = 0x200000)
{
	std::vector<uint8_t> data;
	std::error_code ec;
	const auto n = std::filesystem::file_size(p, ec);
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

inline bool writeFile(const std::filesystem::path &p, const std::vector<uint8_t> &data)
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

} // namespace ostrich

#endif
