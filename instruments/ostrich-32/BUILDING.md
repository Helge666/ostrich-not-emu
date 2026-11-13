# Building Ostrich 32

> Part of the monorepo Ostrich != Emu. Packages, cloning, the submodules (shared, in
> `third_party/` at the repository root), building all instruments, installing and
> checking renders: [the root BUILDING.md](../../BUILDING.md). This file adds what is
> particular to this instrument; its commands run in this folder unless they say otherwise.

The core, its test tool and the plugin (VST3, CLAP, standalone) build on Linux, natively
and cross-compiled for Windows. After Ostrich 55's BUILDING.md.

Needs: a C++20 compiler (GCC 13 tested), CMake 3.15 or newer, git, make; for the plugin
on Linux the X11, OpenGL and JACK development headers DPF needs (as for Ostrich 55); for
Windows binaries mingw-w64 (win32 thread model), cross-compiled on Linux as for the other
Ostriches.

## 1. Cloning

See the root BUILDING.md, section 2. Ostrich 32 needs `third_party/dpf` and
`third_party/gearmulator`, the latter shallow and **without** `--recursive`:

```bash
git submodule update --init --recursive third_party/dpf
git submodule update --init --depth 1 third_party/gearmulator
```

gearmulator carries submodules Ostrich 32 does not need (JUCE, plugin SDKs);
`plugin/tools/build_core.sh` fetches the few the build needs, as shallow clones
(about 660 MB in all).

## 2. Submodules

| Path | Pinned to | Notes |
|---|---|---|
| `third_party/gearmulator/` | [Helge666/gearmulator](https://github.com/Helge666/gearmulator), tag `ostrich-1` (`467d994`) | upstream tag `2.2.27` plus two commits on branch `ostrich`; only 88emu is built |
| `third_party/gearmulator/source/cpu/dsp56300` | [Helge666/dsp56300](https://github.com/Helge666/dsp56300), tag `ostrich-1` (`4bd5aeb`) | the commit `2.2.27` pins plus one commit |
| `third_party/dpf/` | [Helge666/DPF](https://github.com/Helge666/DPF), tag `logan-2` (`b9424f1`) | see DPF-LOGAN.md |

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

## 4. The plugin

`plugin/ostrich` is the plugin on DPF, built like Ostrich 55's. `make` there builds the
core first (section 3), then the VST3, the CLAP and the standalone (JACK) into
`plugin/bin/`; `make win` does the same with mingw-w64 into `plugin/ostrich/bin-win/`
(`Ostrich32.vst3`, `Ostrich32.clap`, `Ostrich32.exe`), with the core in
`plugin/ostrich/build-win/core` (`WIN_CORE=...` names another folder).

```bash
cd plugin/ostrich
nice -n 19 make -j5
nice -n 19 make -j5 win
```

The plugin looks for ROM images in `$OSTRICH32_ROMS`, then `C:\ProgramData\Ostrich 32\roms`
(Windows) or `~/.local/share/Ostrich 32/roms` (Linux) and the other folders its status
line lists; the panel's CHOOSE FILE copies them there. It starts the control ROM the
session names, else v1.04, else the first complete one.

`plugin/tools/ostrich_clap_render.c` loads the CLAP without a host or a display, plays a
note, presses panel switches, saves and prints the state and writes the output to a WAV:

```bash
gcc -std=gnu11 -O2 -o plugin/tools/ostrich_clap_render plugin/tools/ostrich_clap_render.c \
    -I../../third_party/dpf/distrho/src -ldl
OSTRICH32_ROMS=~/coding/ostrich32-roms plugin/tools/ostrich_clap_render plugin/bin/Ostrich32.clap /tmp/out.wav
```

Its arguments are described at the top of the source. Without ROMs the plugin stays silent
and says so in its status; that is the test CI can run.

## 5. Before publishing

- Every submodule pin exists on its remote (`git ls-remote`), the nested dsp56300 one
  included.
- The history holds no ROM, recording, SysEx or manual (`git log --all --stat`).
