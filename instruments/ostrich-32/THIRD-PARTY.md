# Third-party code

Ostrich 32 as a whole is distributed under the GNU General Public License, version 3 or
later ([LICENSE](LICENSE)). It contains, or is built from, the following.

| Component | Where | Licence | Copyright |
|---|---|---|---|
| gearmulator, 88emu | `third_party/gearmulator/` (submodule): `source/ronaldo/88emu` (the MT-32 and CM-32L boards), `source/framework`, the CPU cores in `source/cpu` | GPL-3.0 (`third_party/gearmulator/LICENSE.md`) | The Usual Suspects ([dsp56300.com](https://dsp56300.com/)) |
| Nuked-MT32 | inside 88emu: its LA32 emulation is derived from it | GPL-2.0-or-later | nukeykt |
| HG61H20R36F emulator (Munt) | inside 88emu: its Boss reverb emulation is derived from it | LGPL-2.1-or-later | Sergey V. Mikayev |
| dsp56300 | `third_party/gearmulator/source/cpu/dsp56300` (submodule of the submodule): its base library (threads, logging) | GPL-3.0 (`LICENSE.md` there) | The Usual Suspects |
| AsmJit | `third_party/gearmulator/source/cpu/dsp56300/source/asmjit`: the JIT of the reverb emulation | zlib | The AsmJit Authors |
| mc68k, Musashi | `third_party/gearmulator/source/cpu/mc68k`, linked into 88emu's bundle | GPL-3.0; Musashi MIT-style | The Usual Suspects; Karl Stenerud |
| libresample | `third_party/gearmulator/source/3rdparty/libresample`: resampling to the host's rate | BSD-3-Clause or LGPL-2.1 (`LICENSE-BSD.txt`, `LICENSE-LGPL.txt` there) | Dominic Mazzoni |
| MAME resamplers | `third_party/gearmulator/source/framework/synthLib/mameResamplers.cpp` | BSD-3-Clause | Olivier Galibert |
| Intel ITT API | `third_party/gearmulator/source/cpu/dsp56300/source/vtuneSdk`: profiling hooks, inert in our builds | GPL-2.0-only or BSD-3-Clause | Intel Corporation |
| DPF | `third_party/dpf/` (submodule) | ISC | Filipe Coelho |
| pugl | `third_party/dpf/dgl/src/pugl-upstream` | ISC | David Robillard |
| NanoVG | `third_party/dpf/dgl/src/nanovg` | zlib | Mikko Mononen |
| stb_truetype, stb_image | `third_party/dpf/dgl/src/nanovg` | public domain / MIT | Sean Barrett |
| DejaVu Sans | the panel's font, built into DGL | Bitstream Vera / DejaVu licence (`third_party/dpf/dgl/src/resources/LICENSE-DejaVuSans.ttf.txt`) | Bitstream, DejaVu authors |
| RtAudio | the Windows standalone's audio, through DPF | MIT-style (RtAudio licence) | Gary P. Scavone |
| RtMidi | the standalone's MIDI, through DPF | MIT-style (RtMidi licence) | Gary P. Scavone |

Code taken over from the sibling projects (Ostrich 55, Ostrich 220) is listed here as it
arrives, with its origin.

The release zips will carry these licence texts in their folder `licenses`, next to the
GPL 3.0 (`LICENSE`).

Ostrich 32 builds 88emu with gearmulator's own CMake (target `88emu_bundle`: one static
library of 88emu and everything it links) from our fork Helge666/gearmulator. Its branch
`ostrich` is an upstream release tag plus only the commits a mingw-w64 build needs, each a
plain commit in the fork (and in Helge666/dsp56300, which it points to); nothing is
patched at build time. Of gearmulator only 88emu is built; its other emulations, its
plugins, JUCE and the player are not part of Ostrich 32. gearmulator's CMake reads some
further submodules while configuring (RmlUi, FreeType, LunaSVG, cpp-terminal, PortMidi);
they are fetched for the build but not linked. For mingw-w64 the build adds two header
shims of our own (`plugin/tools/mingw`). Our own files say which licence they are under in
their first lines, GPL-3.0-or-later; combined, the binaries are under the GPL 3.0.

No Roland control ROM, PCM ROM, reverb ROM, SysEx data or manual is part of this project.
"Roland", "MT-32", "CM-32L" and "CM-32LN" are trademarks of Roland Corporation; this
project is not affiliated with Roland.
