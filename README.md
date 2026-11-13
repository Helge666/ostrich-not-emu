# Ostrich != Emu

The home of the Ostrich instruments: emulations of hardware instruments that run the
unit's own firmware, in whole or in part, as VST3/CLAP plugins and standalone programs.
Non-commercial fan projects, not affiliated with any manufacturer.

| Instrument | What it emulates | Folder |
|---|---|---|
| Ostrich 32 (O-32) | Roland MT-32, CM-32L, CM-32LN | [instruments/ostrich-32](instruments/ostrich-32) |
| Ostrich 55 (O-55) | Roland SC-55 Sound Canvas | [instruments/ostrich-55](instruments/ostrich-55) |
| Ostrich 220 (O-220) | Roland U-220 RS-PCM Sound Module | [instruments/ostrich-220](instruments/ostrich-220) |
| Ostrich 7765 | Roland TR-505, TR-626, TR-707, TR-727 | [instruments/ostrich-7765](instruments/ostrich-7765) |

You need your own ROM images. **This repository contains no manufacturer data and never
will.**

Releases are tagged per instrument: `ostrich55-v0.1.0`, `ostrich220-v0.1.4`, ...

## Build and install

Everything builds on Linux; Windows binaries are cross-compiled there with mingw-w64.
Each instrument's BUILDING.md lists what it needs.

```sh
tools/build.sh [instrument ...]        # Linux + Windows builds, then installs the Linux plugins
tools/install-plugins.sh [instrument]  # Linux: ~/.vst3, ~/.clap, ~/.local/bin
```

From Windows, with the build host reachable over SSH:

```powershell
tools\build.ps1 [-Instruments 32,55]   # builds on the build host, installs on both sides
tools\install-plugins.ps1              # only fetch and install the Windows builds
```

On Windows the plugins go to the user's own folders, `%LOCALAPPDATA%\Programs\Common\VST3`
and `...\CLAP`, the standalone programs to `%LOCALAPPDATA%\Programs\Ostrich-not-Emu`; no
administrator rights are needed.
