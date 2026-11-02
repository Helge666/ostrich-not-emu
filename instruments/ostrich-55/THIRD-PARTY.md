# Third-party code

Ostrich 55 as a whole is distributed under the GNU General Public License, version 3 or
later ([LICENSE](LICENSE)). It contains, or is built from, the following.

| Component | Where | Licence | Copyright |
|---|---|---|---|
| Nuked-SC55 | `nuked-sc55/` (submodule): the emulation of the SC-55's MCUs, PCM chip, LCD and its font; the backend library | GPL-2.0-or-later (`nuked-sc55/LICENSE`) | nukeykt; J.C. Moyer; the contributors named in its README |
| SHA-224/256 (RFC 6234) | `nuked-sc55/src/backend/sha/`, used through Nuked-SC55 to recognise the ROM images | BSD-3-Clause (`sha.h`) | IETF Trust and the authors of RFC 6234 |
| Resampler | `plugin/ostrich/Resampler.hpp`, from emu110 / Voltaire 110 by way of Ostrich 220 | GPL-3.0-or-later | Elliott H. Liggett |
| Ostrich 220 | the plugin's structure, the ROM setup, `ZipReader.hpp`, `ostrich_clap_render.c` | GPL-3.0-or-later | Helge Thorsten Kautz and contributors |
| DPF | `plugin/dpf/` (submodule) | ISC | Filipe Coelho |
| pugl | `plugin/dpf/dgl/src/pugl-upstream` | ISC | David Robillard |
| NanoVG | `plugin/dpf/dgl/src/nanovg` | zlib | Mikko Mononen |
| stb_truetype, stb_image | `plugin/dpf/dgl/src/nanovg` | public domain / MIT | Sean Barrett |
| DejaVu Sans | the panel's font, built into DGL | Bitstream Vera / DejaVu licence (`plugin/dpf/dgl/src/resources/LICENSE-DejaVuSans.ttf.txt`) | Bitstream, DejaVu authors |
| RtAudio | the Windows standalone's audio, through DPF | MIT-style (RtAudio licence) | Gary P. Scavone |

Nuked-SC55 was first published under a non-commercial licence; nukeykt relicensed it under
the GPL 2.0 or later in 2026. Ostrich 55 uses the backend as it is, unmodified, from our
fork of J.C. Moyer's fork. Our own files say which licence they are under in their first
lines: the core around Nuked-SC55 (`plugin/core/`) is GPL-2.0-or-later like Nuked-SC55;
the plugin and the tools are GPL-3.0-or-later. Combined, the binaries are under the GPL 3.0.

No Roland firmware, wave ROM, SysEx data or manual is part of this project. "Roland",
"SC-55" and "Sound Canvas" are trademarks of Roland Corporation; this project is not
affiliated with Roland.
