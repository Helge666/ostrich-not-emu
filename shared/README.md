# shared

What more than one instrument uses, so that it lives once.

| File | Used by | What |
|---|---|---|
| `plugin/ZipReader.hpp` | 32, 55, 220, 7765 | ROM images out of zip files, as MAME keeps them |
| `plugin/FileIO.hpp` | all (7765 through the standalone's files) | whole files in and out, wide paths on Windows |
| `plugin/Folders.hpp` | all (7765 likewise) | the ROM folders an instrument searches, its install folders, the standalone's settings files |
| `plugin/Resampler.hpp` | 55, 220 | fixed-ratio polyphase resampler, core rate to host rate (from emu110) |
| `plugin/PanelSize.hpp` | 32, 55, 220, 7765 | the panel's size: kept in the host's session, fitted to the host's frame, mouse scaling |
| `plugin/MidiInput.hpp`, `.cpp` | 32, 55, 220, 7765 | MIDI IN for the Windows standalone (WinMM) |
| `plugin/StandaloneMidi.hpp` | 32, 55, 220, 7765 | its DSP half: the port chosen and saved, the messages placed in the block |
| `plugin/StandaloneBar.hpp` | 32, 55, 220, 7765 | its panel half: the MIDI IN and BUFFER fields and their lists, in the instrument's colours |
| `tools/ostrich_clap_render.c` | 32, 55, 220, 7765 | loads a Linux CLAP without a host, plays notes and MIDI, writes a WAV; CI's smoke test |

Instruments include these by relative path (`../../../../shared/plugin/...` from
`plugin/ostrich/`), so a tool compiled by hand finds them without extra `-I` flags. A
`.cpp` here is built by a one-line file in the instrument that includes it (DPF builds
only the files its Makefile names in `FILES_DSP`).

The standalone's MIDI IN, in an instrument: the states "midiin" (UI -> DSP,
`kStateIsOnlyForDSP`) and "midiports" (DSP -> UI, `kStateIsOnlyForUI`) in its initState;
a `StandaloneMidi` member that run() merges with the host's MIDI; a `StandaloneBar` member
in the panel; `MidiInput.cpp` in `FILES_DSP` and `-lwinmm` for Windows. Its settings go to
`%LOCALAPPDATA%\<instrument>\standalone-midi-in.txt` and `standalone-buffer.txt`.

A change here changes every instrument that uses it: build them all
(`tools/build.sh`) and compare renders before and after.
