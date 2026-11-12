# Ostrich 32 (O-32)

An emulation of the **Roland MT-32** (1987) and its relatives **CM-32L and CM-32LN** as a
VST3/CLAP plugin and a standalone program. A non-commercial fan project, the sibling of
[Ostrich 55](https://github.com/Helge666/ostrich55),
[Ostrich 220](https://github.com/Helge666/ostrich220) and
[Ostrich 7765](https://github.com/Helge666/ostrich7765).

The emulation is **88emu**, part of [gearmulator](https://github.com/dsp56300/gearmulator)
by The Usual Suspects (see [Origin and thanks](#origin-and-thanks)). It runs the unit's
own firmware on an emulated CPU, with emulations of the LA32 sound chip and the Boss
reverb running the original microcode, so the sound, the display, the buttons and the knob
all behave as the firmware makes them. Ostrich 32 adds the plugin around it: VST3, CLAP
and a standalone program, and a panel that looks and handles like the hardware.

You need your own ROM images. **This repository contains no Roland data and never will.**

**Status: in development; nothing to download yet.** The plugin builds and plays, with a
plain first panel (display, switches, knob); the panel after the unit comes next.

## Origin and thanks

Ostrich 32 stands on the shoulders of giants.

- **gearmulator / 88emu** ([dsp56300/gearmulator](https://github.com/dsp56300/gearmulator),
  [dsp56300.com](https://dsp56300.com/)) by The Usual Suspects, under the GNU GPL 3: the
  emulation of the board, the 8095 CPU running the firmware, the LA32 and the reverb.
- **Nuked-MT32** by nukeykt ([nukeykt/Nuked-MT32](https://github.com/nukeykt/Nuked-MT32),
  GPL-2.0-or-later): 88emu's LA32 emulation is derived from it, improved and checked
  against the hardware by The Usual Suspects.
- **Sergey V. Mikayev** and **Munt** ([munt/munt](https://github.com/munt/munt)): 88emu's
  emulation of the Boss reverb gate array is derived from his HG61H20R36F emulator in Munt
  (LGPL-2.1-or-later), and its MT-32 ROM catalogue and much of what is known about the
  MT-32 family come from Munt: Dean Beeler, Jerome Fisher, Sergey V. Mikayev and the Munt
  contributors. Ostrich 32's first core was built on Munt's libmt32emu.
- 88emu's own credits (`gearmulator/doc/88emu_readme.md`) name many more: MAME and
  mamedev, the DTM MIDI Central community, the die-shot photographers and others.
- Ostrich 32 uses gearmulator from our fork
  [Helge666/gearmulator](https://github.com/Helge666/gearmulator), pinned to a tag on its
  branch `ostrich`: the upstream release plus only what a mingw-w64 build needs, with
  dsp56300 from [Helge666/dsp56300](https://github.com/Helge666/dsp56300) in the same way.
  Please report bugs you find in Ostrich 32 here, not to The Usual Suspects.
- The plugin follows Ostrich 55 and Ostrich 220, whose structure goes back to Elliott H.
  Liggett's emu110 / Voltaire 110.

Submodules:

| Path | Source | What for |
|---|---|---|
| `gearmulator/` | [Helge666/gearmulator](https://github.com/Helge666/gearmulator), a fork of [dsp56300/gearmulator](https://github.com/dsp56300/gearmulator), at tag `ostrich-1` (upstream `2.2.27` plus mingw-w64 fixes) | the emulation (88emu) |
| `plugin/dpf/` | [Helge666/DPF](https://github.com/Helge666/DPF), branch `logan` (a fork of [DISTRHO/DPF](https://github.com/DISTRHO/DPF), [DPF-LOGAN.md](DPF-LOGAN.md)) | plugin framework |

## Licences

- Ostrich 32 as a whole: GPL-3.0-or-later ([LICENSE](LICENSE)).
- gearmulator and 88emu: GPL-3.0 (`gearmulator/LICENSE.md`); parts derived from other free
  software keep their licences (Nuked-MT32 GPL-2.0-or-later, the reverb after Munt
  LGPL-2.1-or-later), see `gearmulator/doc/88emu_readme.md`.
- Third-party code and its licences: [THIRD-PARTY.md](THIRD-PARTY.md).

"Roland", "MT-32", "CM-32L" and "CM-32LN" are trademarks of Roland Corporation. This
project is not affiliated with Roland, and neither are gearmulator, Nuked-MT32 or Munt.
