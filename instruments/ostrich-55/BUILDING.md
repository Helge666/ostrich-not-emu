# Building Ostrich 55

Everything builds on Linux (developed on Ubuntu 24.04). Windows binaries are cross-compiled
there with mingw-w64, and so is the Windows installer.

## 1. Packages

```sh
sudo apt-get install --no-install-recommends build-essential git pkg-config \
    libjack-jackd2-dev libgl1-mesa-dev libx11-dev libxext-dev libxcursor-dev \
    libcairo2-dev libdbus-1-dev g++-mingw-w64-x86-64 nsis
```

Nuked-SC55's backend is C++23 (`std::unreachable`, `std::byteswap`): g++ 13 or newer, as
on Ubuntu 24.04. `nsis` is only needed for the installer.

`g++-mingw-w64-x86-64` brings two thread models, chosen with `update-alternatives`. Builds
here use **win32** (the default on Ubuntu 24.04): the binaries then need no mingw runtime
DLL. Objects from the two models must not be mixed.

## 2. Clone

```sh
git clone --recurse-submodules https://github.com/Helge666/ostrich55.git
```

Needed: `nuked-sc55`, `plugin/dpf`, and DPF's own `dgl/src/pugl-upstream`. After a clone
without `--recurse-submodules`, or after a pull that moves a submodule:

```sh
git submodule update --init --recursive
```

`git submodule status --recursive` shows a leading space on every line when all is in place.
Nuked-SC55's own submodules (SDL, RtMidi for its frontends) are not used here.

## 3. Build

```sh
cd plugin/ostrich
nice -n 19 make -j5          # Linux: VST3, CLAP, standalone (JACK)  -> plugin/bin/
nice -n 19 make -j5 win      # Windows: VST3, CLAP, standalone       -> plugin/ostrich/bin-win/
```

Each run, in order:

1. builds the emulation core, `plugin/tools/build_core.sh`: Nuked-SC55's backend
   (`nuked-sc55/src/backend`, unmodified, without its CMake build, which wants SDL2 for the
   frontends) plus `plugin/core/`, into `plugin/build-core/libsc55core.a` (Windows:
   `plugin/ostrich/build-win/core/`), and the tool `sc55_render`;
2. links the plugin. A core newer than a binary forces the relink.

The Windows binaries are statically linked and depend on nothing beyond Windows. The
standalone uses DPF's RtAudio backend on Windows and JACK on Linux.

## 4. The installer

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

`plugin/dpf` is our fork of DPF, [Helge666/DPF](https://github.com/Helge666/DPF), branch
`logan`, pinned to a tag (`logan-1`). Nothing is patched at build time: `plugin/dpf` stays
clean. What the fork is and how it changes: [DPF-LOGAN.md](DPF-LOGAN.md).

## 6. Checking it

`sc55_render` runs the core without a host: it finds an SC-55 set in a folder by the
images' contents, boots it, plays a test chord or a MIDI file and writes a WAV at the
core's rate (64 kHz), then prints the display.

```sh
plugin/build-core/sc55_render /path/to/roms out.wav 6 [song.mid]
```

`SC55_KEYS="bit@sec,..."` presses panel keys (the bits are `sc55::Button` in
`plugin/core/sc55_core.h`).

`plugin/tools/ostrich_clap_render.c` loads the CLAP without a host or a display, plays a
note, presses panel keys, saves the state and writes the output to a WAV:

```sh
gcc -std=gnu11 -O2 -o ostrich_clap_render plugin/tools/ostrich_clap_render.c \
    -Iplugin/dpf/distrho/src -ldl
OSTRICH55_ROMS=/path/to/roms ./ostrich_clap_render plugin/bin/Ostrich55.clap out.wav
```

Its arguments are described at the top of the source. Without ROMs the plugin stays
silent and says so in its status; that is the test CI can run.

## 7. Before publishing

A clone can fetch only pushed commits. Check that both submodule pins are on their remotes:

```sh
git -C nuked-sc55 ls-remote origin | grep "$(git rev-parse :nuked-sc55)"
git -C plugin/dpf ls-remote origin | grep "$(git rev-parse :plugin/dpf)"
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
| `plugin/tools/` | `build_core.sh`, `sc55_render`, `ostrich_clap_render` |
| `nuked-sc55/` | Nuked-SC55 (submodule); only `src/backend` is built |
| `installer/` | the NSIS script, its build script and the README that goes with the installer |
| `roms/` | the place for your own ROM images during development (ignored by git) |
