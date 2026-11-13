# Building Ostrich != Emu

What all instruments share. Each instrument's own BUILDING.md (`instruments/<name>/`) adds
what is particular to it: its core, its tests, its submodule's commits.

Everything builds on Linux (developed on Ubuntu 24.04). Windows binaries are cross-compiled
there with mingw-w64.

## 1. Packages

```sh
sudo apt-get install --no-install-recommends build-essential git pkg-config ccache cmake \
    python3 libjack-jackd2-dev libgl1-mesa-dev libx11-dev libxext-dev libxcursor-dev \
    libcairo2-dev libdbus-1-dev g++-mingw-w64-x86-64
```

Ostrich 55's Windows installer also needs `nsis`. Ostrich 220's measurement tools need
`python3-numpy python3-scipy`, and work on its MAME driver needs MAME's own build
dependencies (instruments/ostrich-220/BUILDING.md).

`g++-mingw-w64-x86-64` brings two thread models, chosen with `update-alternatives`. Builds
here and in CI use **win32** (the default on Ubuntu 24.04): the binaries then need no mingw
runtime DLL. Objects from the two models must not be mixed.

## 2. Clone

```sh
git clone https://github.com/Helge666/ostrich-not-emu.git
cd ostrich-not-emu
git submodule update --init --recursive third_party/dpf
```

The other submodules are large; fetch only those of the instruments you build:

| Instrument | Command (from the repository root) | Size |
|---|---|---|
| all | `git submodule update --init --recursive third_party/dpf` (DPF and its pugl) | 15 MB |
| Ostrich 32 | `git submodule update --init --depth 1 third_party/gearmulator`, **not** `--recursive`: its build script fetches the few nested submodules it needs | 220 MB, 660 MB after the first build |
| Ostrich 55 | `git submodule update --init third_party/nuked-sc55` (its own submodules, SDL and RtMidi, are not used) | 4 MB |
| Ostrich 220 | `git submodule update --init --depth 1 third_party/mame` | 930 MB |
| Ostrich 7765 | nothing more | |

`git submodule status` shows a leading space on each line it has fetched; `+` means a
submodule is not at its pinned commit, `-` that it is not fetched.

On Windows with `core.autocrlf=true`, set `git config core.autocrlf false` in the
repository and in `third_party/dpf` before checking files out; the Make build needs LF.
Building happens on Linux anyway.

## 3. Build and install

```sh
tools/build.sh [instrument ...]    # e.g. tools/build.sh 55 220; default: all
```

For each instrument `make` (Linux VST3, CLAP, standalone -> `instruments/<name>/plugin/bin`)
and `make win` (Windows -> `instruments/<name>/plugin/ostrich/bin-win`), at `-j5` and
`nice -n 19`; then the Linux builds go to `~/.vst3`, `~/.clap` and `~/.local/bin`
(`tools/install-plugins.sh`). `--linux-only` and `--no-install` leave parts out.

By hand, in an instrument's `plugin/ostrich`: `make -j5` and `make -j5 win`.

From Windows, `tools\build.ps1` runs `tools/build.sh` on the build host over SSH and then
`tools\install-plugins.ps1`, which fetches the Windows builds and puts them into the user's
`%LOCALAPPDATA%\Programs\Common\VST3` and `...\CLAP` (standalones:
`%LOCALAPPDATA%\Programs\Ostrich-not-Emu`). No administrator rights are needed. The build
host must have the commit checked out that the Windows checkout has (`git push kk2 main`).

## 4. Checking a change

`shared/tools/ostrich_clap_render.c` loads an instrument's Linux CLAP without a host or a
display, plays notes, presses panel keys, saves and loads state, and writes a WAV. Its
options are described at the top of the source.

```sh
gcc -std=gnu11 -O2 -o /tmp/render shared/tools/ostrich_clap_render.c \
    -Ithird_party/dpf/distrho/src -ldl -lm
OSTRICH55_ROMS=/path/to/roms /tmp/render instruments/ostrich-55/plugin/bin/Ostrich55.clap /tmp/out.wav
```

A change that should not change the sound (shared code, the build, DPF) is checked by
rendering before and after with a fresh `XDG_CONFIG_HOME` and comparing the WAVs: they
must be identical. Ostrich 7765's TR-707 with vintage timing is not repeatable: its 2.8 ms
grid runs free, with a phase each instance takes from its own address, as the unit's
depends on when it was switched on. For such a comparison switch its timing off
(`--param 57=0`) or play the TR-505 or TR-626.

## 5. CI and releases

`.github/workflows/build.yml` runs on GitHub; its steps are the scripts in `tools/ci/`,
which run on the build host just as well. What is particular to an instrument is in its
`ci/` folder: `ci.conf` (names, release tag, submodules, smoke test), `licenses.sh` (the
licence texts in the zips) and `release-text.sh` (INSTALL.txt and the release notes).

| Event | What runs |
|---|---|
| push to `main` | the instruments whose folders changed; all if anything outside `instruments/` changed; nothing for Markdown alone |
| tag `<tag>-preview-1.2.3` | that instrument, without a release (the dry run) |
| tag `<tag>-v1.2.3` | that instrument, then a GitHub release with its zips |
| manual run | the instruments named, or all |

`<tag>` is `ostrich32`, `ostrich55`, `ostrich220` or `ostrich7765`. The version in a tag
must be the one in the instrument's source (`tools/ci/version.sh`), or the run fails.

By hand, after `make` and `make win` in an instrument: `tools/ci/smoke.sh 55`,
`tools/ci/check-imports.sh 55`, `tools/ci/package.sh 55 0.1.0 /tmp/dist`.

## 6. Shared code and DPF

- `shared/` holds what more than one instrument uses (shared/README.md). A change there
  is a change to each instrument that uses it: build them all and compare renders.
- `third_party/dpf` is our fork Helge666/DPF, branch `logan`, pinned to a tag; it is shared
  with the LOGAN plugins (DPF-LOGAN.md). Nothing is patched at build time. A change is a
  commit on the fork's `logan` branch and a new tag; then move the pin
  (`git -C third_party/dpf fetch --tags && git -C third_party/dpf checkout logan-N`),
  build DGL from clean (`rm -rf third_party/dpf/build*`), build every instrument and
  compare renders.
