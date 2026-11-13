# Building Ostrich 55

> Part of the monorepo Ostrich != Emu. Packages, cloning, the submodules (shared, in
> `third_party/` at the repository root), building all instruments, installing and
> checking renders: [the root BUILDING.md](../../BUILDING.md). This file adds what is
> particular to this instrument; its commands run in this folder unless they say otherwise.

Everything builds on Linux (developed on Ubuntu 24.04). Windows binaries are cross-compiled
there with mingw-w64, and so is the Windows installer.

## 1. Packages

See the root BUILDING.md, section 1. The Windows installer also needs `nsis`.
Nuked-SC55's backend is C++23 (`std::unreachable`, `std::byteswap`): g++ 13 or newer, as
on Ubuntu 24.04.

## 2. Clone

See the root BUILDING.md, section 2. This instrument needs `third_party/dpf` (with its pugl)
and `third_party/nuked-sc55`; Nuked-SC55's own submodules (SDL, RtMidi for its frontends)
are not used.

## 3. Build

```sh
cd plugin/ostrich
nice -n 19 make -j5          # Linux: VST3, CLAP, standalone (JACK)  -> plugin/bin/
nice -n 19 make -j5 win      # Windows: VST3, CLAP, standalone       -> plugin/ostrich/bin-win/
```

Each run, in order:

1. builds the emulation core, `plugin/tools/build_core.sh`: Nuked-SC55's backend
   (`third_party/nuked-sc55/src/backend`, unmodified, without its CMake build, which wants SDL2 for the
   frontends) plus `plugin/core/`, into `plugin/build-core/libsc55core.a` (Windows:
   `plugin/ostrich/build-win/core/`), and the tool `sc55_render`;
2. links the plugin. A core newer than a binary forces the relink.

The Windows binaries are statically linked and depend on nothing beyond Windows. The
standalone uses DPF's RtAudio backend on Windows and JACK on Linux.

## 4. The installer (not released yet)

Written, not yet built or used: it comes later, together with one for Ostrich 220
(ROADMAP.md).

```sh
installer/build.sh          # -> installer/Ostrich55-Setup-<version>.exe
```

It takes the products of `make win` and the texts (LICENSE, `installer/README.txt`,
THIRD-PARTY.md, with Windows line ends) and runs NSIS on `installer/ostrich55.nsi`. The
version comes from `d_version()` in `plugin/ostrich/OstrichPlugin.cpp`. The installer is
English and German, lets each format be left out, and makes
`C:\ProgramData\Ostrich 55\roms` writable for the users of the machine, so the panel can
copy ROM images there without admin rights. The uninstaller leaves that folder alone.

## 5. DPF

`third_party/dpf` is our fork of DPF, [Helge666/DPF](https://github.com/Helge666/DPF), branch
`logan`, pinned to a tag (`logan-2`). Nothing is patched at build time: `third_party/dpf` stays
clean. What the fork is and how it changes: [DPF-LOGAN.md](../../DPF-LOGAN.md).

## 6. Checking it

`sc55_render` runs the core without a host: it finds an SC-55 set in a folder by the
images' contents, boots it, plays a test chord or a MIDI file and writes a WAV at the
core's rate (64 kHz), then prints the display.

```sh
plugin/build-core/sc55_render /path/to/roms out.wav 6 [song.mid]
```

`SC55_KEYS="bit@sec,..."` presses panel keys (the bits are `sc55::Button` in
`plugin/core/sc55_core.h`).

`shared/tools/ostrich_clap_render.c` (the monorepo's, for all instruments) loads the CLAP
without a host or a display, plays a note, presses panel keys, saves the state and writes
the output to a WAV:

```sh
gcc -std=gnu11 -O2 -o /tmp/render ../../shared/tools/ostrich_clap_render.c \
    -I../../third_party/dpf/distrho/src -ldl -lm
OSTRICH55_ROMS=/path/to/roms /tmp/render plugin/bin/Ostrich55.clap out.wav
```

Its options are described at the top of the source. Without ROMs the plugin stays
silent and says so in its status; that is the test CI can run.

## 7. Before publishing

A clone can fetch only pushed commits. Check that both submodule pins are on their remotes:

```sh
git -C nuked-sc55 ls-remote origin | grep "$(git rev-parse :nuked-sc55)"
git -C third_party/dpf ls-remote origin | grep "$(git rev-parse :third_party/dpf)"
```

The Nuked-SC55 pin must be a commit that jcmoyer's fork has too, or one pushed to
Helge666/Nuked-SC55. And the history must hold no ROM, wave, SysEx or manual file:

```sh
git log --all --name-only --format= | sort -u | grep -iE '\.(bin|rom|syx|ic[0-9]+|wav|mid|zip)$'
```

## 8. Where things are

| Path | What |
|---|---|
| `plugin/ostrich/` | the plugin: DSP (`OstrichPlugin.cpp`), panel (`OstrichUI.cpp`), resampler, zip reader |
| `plugin/core/` | `sc55::Core`, the core around Nuked-SC55's `Emulator`, and ROM recognition |
| `plugin/tools/` | `build_core.sh`, `sc55_render` |
| `third_party/nuked-sc55/` | Nuked-SC55 (submodule); only `src/backend` is built |
| `installer/` | the NSIS script, its build script and the README that goes with the installer |
| `roms/` | the place for your own ROM images during development (ignored by git) |
