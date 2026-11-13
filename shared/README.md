# shared

What more than one instrument uses, so that it lives once.

| File | Used by | What |
|---|---|---|
| `plugin/ZipReader.hpp` | 32, 55, 220, 7765 | ROM images out of zip files, as MAME keeps them |
| `plugin/FileIO.hpp` | 32, 55, 220 | whole files in and out, wide paths on Windows |
| `plugin/Resampler.hpp` | 55, 220 | fixed-ratio polyphase resampler, core rate to host rate (from emu110) |
| `plugin/MidiInput.hpp`, `.cpp` | 32, 55 | MIDI IN for the Windows standalone (WinMM) |

Instruments include these by relative path (`../../../../shared/plugin/...` from
`plugin/ostrich/`), so a tool compiled by hand finds them without extra `-I` flags. A
`.cpp` here is built by a one-line file in the instrument that includes it (DPF builds
only the files its Makefile names in `FILES_DSP`).

A change here changes every instrument that uses it: build them all
(`tools/build.sh`) and compare renders before and after.
