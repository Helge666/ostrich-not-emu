# Third-party code

Ostrich 32 as a whole is distributed under the GNU General Public License, version 3 or
later ([LICENSE](LICENSE)). It contains, or is built from, the following.

| Component | Where | Licence | Copyright |
|---|---|---|---|
| libmt32emu (Munt) | `munt/mt32emu/` (submodule): the emulation of the MT-32, CM-32L and LAPC-I | LGPL-2.1-or-later (`munt/mt32emu/COPYING.LESSER.txt`) | Dean Beeler, Jerome Fisher, Sergey V. Mikayev; the authors and contributors in `munt/mt32emu/AUTHORS.txt` |
| smallsha1 | inside libmt32emu, used to recognise the ROM images | BSD-3-Clause (`munt/mt32emu/AUTHORS.txt`) | Micael Hildenborg |
| DPF | `plugin/dpf/` (submodule) | ISC | Filipe Coelho |
| pugl | `plugin/dpf/dgl/src/pugl-upstream` | ISC | David Robillard |
| NanoVG | `plugin/dpf/dgl/src/nanovg` | zlib | Mikko Mononen |
| stb_truetype, stb_image | `plugin/dpf/dgl/src/nanovg` | public domain / MIT | Sean Barrett |
| DejaVu Sans | the panel's font, built into DGL | Bitstream Vera / DejaVu licence (`plugin/dpf/dgl/src/resources/LICENSE-DejaVuSans.ttf.txt`) | Bitstream, DejaVu authors |
| RtAudio | the Windows standalone's audio, through DPF | MIT-style (RtAudio licence) | Gary P. Scavone |
| RtMidi | the standalone's MIDI, through DPF | MIT-style (RtMidi licence) | Gary P. Scavone |

Code taken over from the sibling projects (Ostrich 55, Ostrich 220) is listed here as it
arrives, with its origin.

The release zips will carry these licence texts in their folder `licenses`, next to the
GPL 3.0 (`LICENSE`).

Ostrich 32 uses libmt32emu as it is, unmodified, built from the Munt source at a release
tag. Of the Munt project only the library is used; mt32emu_qt, the drivers and the other
programs are not part of Ostrich 32. The LGPL 2.1 allows libmt32emu to be combined into a
GPL 3.0 program; its own notices stay as they are, and the complete source of every
release, libmt32emu included, is public. Our own files say which licence they are under in
their first lines.

No Roland control ROM, PCM ROM, SysEx data or manual is part of this project. "Roland",
"MT-32", "CM-32L", "CM-64" and "LAPC-I" are trademarks of Roland Corporation; this project
is not affiliated with Roland.
