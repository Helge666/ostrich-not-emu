# Building Ostrich 7765

> Part of the monorepo Ostrich != Emu. Packages, cloning, the submodules (shared, in
> `third_party/` at the repository root), building all instruments, installing and
> checking renders: [the root BUILDING.md](../../BUILDING.md). This file adds what is
> particular to this instrument; its commands run in this folder unless they say otherwise.

Everything builds on Linux (developed on Ubuntu 24.04). Windows binaries are cross-compiled
there with mingw-w64.

## 1. Packages

See the root BUILDING.md, section 1.

## 2. Clone

See the root BUILDING.md, section 2. This instrument needs `third_party/dpf` (with its pugl),
nothing else.

## 3. Build

```sh
cd plugin/ostrich
make -j5          # Linux: VST3, CLAP, standalone (JACK)  -> plugin/bin/
make -j5 win      # Windows: VST3, CLAP, standalone       -> plugin/ostrich/bin-win/
```

The Windows binaries are statically linked and depend on nothing beyond Windows. The
standalone uses DPF's RtAudio backend on Windows and JACK on Linux.

The smoke test (as in CI) loads the Linux CLAP without a host, plays a few notes, saves
the state and writes the outputs to a WAV:

```sh
gcc -std=gnu11 -O2 -o plugin/tools/ostrich_clap_render plugin/tools/ostrich_clap_render.c \
    -I../../third_party/dpf/distrho/src -ldl
plugin/tools/ostrich_clap_render plugin/bin/Ostrich7765.clap /tmp/out.wav
```

Without ROM images the outputs stay silent. With images in a folder (roms/README.md;
`OSTRICH_ROMS` points at any folder) each machine plays on its own channel: TR-505 10,
TR-626 11, TR-707 12, TR-727 13 (changed on the SETTINGS page). For the TR-707's bass
drum 2, snare 1 and closed hi-hat:

```sh
OSTRICH_ROMS=~/my-roms plugin/tools/ostrich_clap_render plugin/bin/Ostrich7765.clap /tmp/out.wav \
    36@0.5,38@1.0,42@1.5 12
```

Its comment at the top lists what it reads from the environment for measuring: parameters
to set, a state to load or save, all output channels as raw data.

`plugin/tools/ostrich_engine_render.cpp` plays a machine's engine without the plugin (every
sound at three velocities); `tools/check_tr707_render.py` checks such a render against the
ROMs. Both are described at their top.

## 4. DPF

`third_party/dpf` is our fork of DPF, [Helge666/DPF](https://github.com/Helge666/DPF), branch
`logan`, pinned to a tag (`logan-1`, ...); DPF's own pugl submodule points at
[Helge666/pugl](https://github.com/Helge666/pugl). The changes we need are commits there,
not patches here: `third_party/dpf` stays clean. What the fork adds is listed in Ostrich 220's
BUILDING.md; Ostrich 7765 relies on these:

| Change | What it does |
|---|---|
| VST3: extra outputs off | the 16 mono outputs are main buses, off until the host switches them on |
| CLAP: port type, version | a bus of a group of the plugin's own is mono or stereo by its width; the descriptor has the version |
| VST3: 16 MIDI channels | the MIDI input bus has 16 channels (Cubase folded all onto channel 1) |
| JACK, CLAP, VST3: DSP → UI state | `updateStateValue()` reaches the UI |
| VST3: size rounding, UI idle watchdog | the panel opens at the right size in Cubase and keeps redrawing |
| VST3: UTF-8 state | state strings (file paths) converted between UTF-8 and UTF-16 for real |

## 5. Versions

The version is in `plugin/ostrich/Version.hpp` and nowhere else; the panel and the plugin
formats take it from there. A release: raise it in a commit "Version x.y.z", tag that
commit `vx.y.z` and push the tag; CI checks that tag and header agree, builds, and
publishes the zips. A `preview-x.y.z` tag does the same without publishing.
