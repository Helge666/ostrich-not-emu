# Building Ostrich 32

So far the core and its test tool build; the plugin follows (ROADMAP.md). This file grows
with it, after Ostrich 55's BUILDING.md.

Needs: a C++17 compiler, CMake 3.x, git; for Windows binaries mingw-w64 (win32 thread
model), cross-compiled on Linux as for the other Ostriches.

## 1. Cloning

```bash
git clone --recurse-submodules https://github.com/Helge666/ostrich32.git
```

On Windows with `core.autocrlf=true`, set `core.autocrlf false` in the repository and in
`plugin/dpf` before the submodules are checked out (DPF-LOGAN.md); the Make build needs LF.

`git submodule status --recursive` must show a leading space on every line; `+` or `m`
means a submodule is not at its pinned state.

## 2. Submodules

| Path | Pinned to | Notes |
|---|---|---|
| `munt/` | [Helge666/munt](https://github.com/Helge666/munt), tag `libmt32emu_2_8_6` (`91d2205`) | upstream Munt, unmodified; only `munt/mt32emu` (libmt32emu, CMake) is built |
| `plugin/dpf/` | [Helge666/DPF](https://github.com/Helge666/DPF), tag `logan-2` (`b9424f1`) | see DPF-LOGAN.md |

To move munt to a newer release: fetch the tags into our fork (`git fetch upstream --tags`,
push the tag to `Helge666/munt`, together with the owner), then
`git -C munt checkout libmt32emu_X_Y_Z` and commit the new pointer. Read
`munt/mt32emu/NEWS.txt` first. Nothing in `munt/` is ever changed in place.

## 3. The core

`plugin/tools/build_core.sh` builds libmt32emu with Munt's own CMake (static, C++
interface, built-in resampler, no tests) into `plugin/build-core/munt/`, compiles our core
(`plugin/core/mt32_core.cpp`) into `libmt32core.a` and builds `mt32_render`. On the build
host:

```bash
nice -n 19 plugin/tools/build_core.sh
```

For Windows, the same with mingw-w64:

```bash
MT32_CORE_BUILD=$PWD/plugin/ostrich/build-win/core CXX=x86_64-w64-mingw32-g++ \
  CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar nice -n 19 plugin/tools/build_core.sh
```

`mt32_render` runs the core without a host: it lists which images in a folder libmt32emu
recognises and which machines they complete, starts one, plays a test chord (or a MIDI
file) and writes a WAV, printing the display as it changes.

```bash
plugin/build-core/mt32_render ~/coding/ostrich32-roms /tmp/out.wav 4 "" mt32_1_04
```

Arguments: ROM folder, WAV, seconds, MIDI file (`""` for the test chord), machine (default:
the first complete one), sample rate (default 48000). Without a complete machine it says so
and exits with 1.

## 4. Before publishing

- Both submodule pins exist on their remotes (`git ls-remote`).
- The history holds no ROM, recording, SysEx or manual (`git log --all --stat`).
