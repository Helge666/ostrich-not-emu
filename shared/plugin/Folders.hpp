// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ostrich != Emu (shared/plugin; Ostrich 32, 55 and 220) -- where an instrument looks for
// the user's ROM images, where it copies the ones picked in the panel, and the Windows
// standalone's settings.  Every path here is a promise to the users of a released plugin:
// never change one (AGENTS.md); add new ones after the old.
//
// On Windows ProgramData comes first: a tool inside an MSIX package sees a virtualised
// %LOCALAPPDATA%, the DAW does not (as Ostrich 220 learnt).

#ifndef OSTRICH_FOLDERS_HPP
#define OSTRICH_FOLDERS_HPP

#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

namespace ostrich {

/// A path as UTF-8 in a std::string (C++20's u8string() is a different type).
inline std::string pstr(const std::filesystem::path &p)
{
	const auto u = p.u8string();
	return std::string(u.begin(), u.end());
}

/// A UTF-8 string as a path.
inline std::filesystem::path upath(const std::string &s)
{
	return std::filesystem::path(reinterpret_cast<const char8_t *>(s.c_str()));
}

namespace detail {
inline void addEnvDir(std::vector<std::filesystem::path> &dirs, const char *name, const std::string &tail)
{
	if (const char *v = std::getenv(name))
		if (*v) dirs.push_back((upath(v) / upath(tail)).make_preferred());
}
} // namespace detail

/// Where the user puts the images, most specific first: the folder in the environment
/// variable `env` (for tests), then "<name>/roms" in the user's and the system's data
/// folders, for each of `names` in turn (an instrument's old names after its own).
inline std::vector<std::filesystem::path> romDirs(const char *env, std::initializer_list<const char *> names)
{
	std::vector<std::filesystem::path> dirs;
	detail::addEnvDir(dirs, env, "");
	for (const char *name : names)
	{
		const std::string roms = std::string(name) + "/roms";
#ifdef _WIN32
		detail::addEnvDir(dirs, "PROGRAMDATA", roms);
		detail::addEnvDir(dirs, "LOCALAPPDATA", roms);
		detail::addEnvDir(dirs, "USERPROFILE", "Documents/" + roms);
#else
		detail::addEnvDir(dirs, "XDG_DATA_HOME", roms);
		detail::addEnvDir(dirs, "HOME", ".local/share/" + roms);
		dirs.push_back("/usr/share/" + roms);
#endif
	}
	return dirs;
}

/// Where images the user picks in the panel are copied to: the first of these that takes
/// them.  romDirs() searches them all.
inline std::vector<std::filesystem::path> installDirs(const char *name)
{
	std::vector<std::filesystem::path> dirs;
	const std::string roms = std::string(name) + "/roms";
#ifdef _WIN32
	detail::addEnvDir(dirs, "PROGRAMDATA", roms);
	detail::addEnvDir(dirs, "LOCALAPPDATA", roms);
	detail::addEnvDir(dirs, "USERPROFILE", "Documents/" + roms);
#else
	detail::addEnvDir(dirs, "XDG_DATA_HOME", roms);
	detail::addEnvDir(dirs, "HOME", ".local/share/" + roms);
#endif
	return dirs;
}

/// A setting of the Windows standalone, per user: %LOCALAPPDATA%\<name>\<file>.  Empty
/// elsewhere: the plugins have their host's session, and on Linux JACK keeps the rest.
inline std::filesystem::path standaloneSettingFile(const char *name, const char *file)
{
#ifdef _WIN32
	if (const wchar_t *v = _wgetenv(L"LOCALAPPDATA"))
		if (*v)
			return std::filesystem::path(v) / upath(name) / upath(file);
#else
	(void)name;
	(void)file;
#endif
	return std::filesystem::path();
}

} // namespace ostrich

#endif // OSTRICH_FOLDERS_HPP
