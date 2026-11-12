# Building Ostrich 32

So far the core and its test tool build; the plugin follows (ROADMAP.md). This file grows
with it, after Ostrich 55's BUILDING.md.

Needs: a C++17 compiler (GCC 13 tested), CMake 3.15 or newer, git; for Windows binaries
mingw-w64 (win32 thread model), cross-compiled on Linux as for the other Ostriches.

## 1. Cloning

```bash
git clone https://github.com/Helge666/ostrich32.git
cd ostrich32
git submodule update --init plugin/dpf
git submodule update --init --depth 1 gearmulator
```

gearmulator is large and carries submodules Ostrich 32 does not need (JUCE, plugin SDKs).
Do not clone it with `--recurse-submodules`; `plugin/tools/build_core.sh` fetches the few
of its submodules the build needs, as shallow clones (about 660 MB in all).

On Windows with `core.autocrlf=true`, set `core.autocrlf false` in the repository and in
`plugin/dpf` before the submodules are checked out (DPF-LOGAN.md); the Make build needs LF.

`git submodule status` must show a leading space on both lines; `+` or `m` means a
submodule is not at its pinned state.

## 2. Submodules

| Path | Pinned to | Notes |
|---|---|---|
| `gearmulator/` | [Helge666/gearmulator](https://github.com/Helge666/gearmulator), tag `ostrich-1` (`467d994`) | upstream tag `2.2.27` plus two commits on branch `ostrich`; only 88emu is built |
| `gearmulator/source/cpu/dsp56300` | [Helge666/dsp56300](https://github.com/Helge666/dsp56300), tag `ostrich-1` (`4bd5aeb`) | the commit `2.2.27` pins plus one commit |
| `plugin/dpf/` | [Helge666/DPF](https://github.com/Helge666/DPF), tag `logan-2` (`b9424f1`) | see DPF-LOGAN.md |

The commits on the `ostrich` branches, each needed only by mingw-w64 (GCC):

| Repository | Commit | Why |
|---|---|---|
| dsp56300 | `4bd5aeb` structured exception handling only with MSVC | GCC has no `__try`/`__except`; the block only names a thread for a debugger |
| gearmulator | `18a6884` `deviceException.cpp` includes `<cstdint>` | it uses `int32_t` without including it |
| gearmulator | `467d994` dsp56300 from the Ostrich fork | `.gitmodules` and the pointer to the commit above |

Two more differences need no commit: the header names gearmulator spells as the Windows SDK
does (`<Windows.h>`, `<shlobj_core.h>`) come from shims in `plugin/tools/mingw`, and
`_WIN32_WINNT`/`NTDDI_VERSION` select Windows 10 for the memory-mapping calls dsp56300
uses. `build_core.sh` sets both for a mingw build.

To move to a newer gearmulator release (together with the owner): in our forks, start a new
`ostrich` state from the new upstream tag and the commit dsp56300 is pinned to there,
re-apply the commits above that upstream has not taken (drop the rest), tag `ostrich-N`,
push branch and tag, then point `gearmulator` at the new tag and commit. Read the release
notes first and compare renders of `mt32_render` before and after. Nothing in
`gearmulator/` is ever changed in place.

## 3. The core

`plugin/tools/build_core.sh` fetches the gearmulator submodules it needs, builds 88emu with
gearmulator's own CMake (target `88emu_bundle`: `lib88emu.a`, 88emu and all it links) into
`plugin/build-core/88emu/`, compiles our core (`plugin/core/mt32_core.cpp`) into
`libmt32core.a` and builds `mt32_render`. On the build host (it runs everything with
`nice -n 19` and `-j5`):

```bash
plugin/tools/build_core.sh
```

For Windows, the same with mingw-w64:

```bash
MT32_CORE_BUILD=$PWD/plugin/build-core-win CXX=x86_64-w64-mingw32-g++ CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar plugin/tools/build_core.sh
```

A first build of 88emu takes about a minute with five jobs.

`mt32_render` runs the core without a host: it lists which images in a folder 88emu
recognises and which machines they complete, starts one, plays a test sequence (or a MIDI
file) and writes a WAV, printing the display as it changes. The test sequence also presses
MASTER VOLUME and SOUND and turns the knob, so the panel is tested too.

```bash
plugin/build-core/mt32_render ~/coding/ostrich32-roms /tmp/out.wav 6.5 "" mt32old/1.04
```

Arguments: ROM folder, WAV, seconds, MIDI file (`""` for the test sequence), machine
(default: the first complete one), sample rate (default 48000). Without a complete machine
it says so and exits with 1. The Linux and the Windows build render the same WAV, bit for
bit.

## 4. Before publishing

- Every submodule pin exists on its remote (`git ls-remote`), the nested dsp56300 one
  included.
- The history holds no ROM, recording, SysEx or manual (`git log --all --stat`).
