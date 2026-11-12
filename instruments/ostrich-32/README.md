# Ostrich 32 (O-32)

An emulation of the **Roland MT-32** (1987) and its relatives **CM-32L, CM-64 and
LAPC-I** as a VST3/CLAP plugin and a standalone program. A non-commercial fan project, the
sibling of [Ostrich 55](https://github.com/Helge666/ostrich55),
[Ostrich 220](https://github.com/Helge666/ostrich220) and
[Ostrich 7765](https://github.com/Helge666/ostrich7765).

The emulation is **[Munt](https://github.com/munt/munt)**'s library libmt32emu, by Dean
Beeler, Jerome Fisher, Sergey V. Mikayev and the Munt contributors (see [Origin and
thanks](#origin-and-thanks)). It does all the work that makes the sound. Ostrich 32 adds the
plugin around it: VST3, CLAP and a standalone program, and a panel that looks and handles
like the hardware.

You need your own ROM images. **This repository contains no Roland data and never will.**

**Status: in development; nothing to download yet.**

## Origin and thanks

Ostrich 32 stands on the shoulders of giants.

- **Munt** ([munt/munt](https://github.com/munt/munt)): libmt32emu, Copyright © 2003–2026
  Dean Beeler, Jerome Fisher, Sergey V. Mikayev, under the GNU LGPL 2.1 or later. Its
  `AUTHORS.txt` names everyone whose work, ROM dumps and analysis made the emulation
  possible: Dean "Canadacow" Beeler (original author), Jerome "KingGuppy" Fisher, "Mok",
  Tristan, "SergM", and as contributors Jonathan Gevaryahu, "The Guru", Laust "Talus"
  Brock-Nannestad, "balrog", "Sarayan", "IssaUt", "Cloudschatze", Martin Lukasek,
  "sdhizumi", and the people of VOGONS.
- Ostrich 32 uses libmt32emu unmodified, from our fork
  [Helge666/munt](https://github.com/Helge666/munt) pinned to an upstream release tag, and
  keeps every notice. Please report bugs you find in Ostrich 32 here, not to the Munt
  team.
- The plugin follows Ostrich 55 and Ostrich 220, whose structure goes back to Elliott H.
  Liggett's emu110 / Voltaire 110.

Submodules:

| Path | Source | What for |
|---|---|---|
| `munt/` | [Helge666/munt](https://github.com/Helge666/munt), a fork of [munt/munt](https://github.com/munt/munt), at tag `libmt32emu_2_8_6` | the emulation (libmt32emu, unmodified) |
| `plugin/dpf/` | [Helge666/DPF](https://github.com/Helge666/DPF), branch `logan` (a fork of [DISTRHO/DPF](https://github.com/DISTRHO/DPF), [DPF-LOGAN.md](DPF-LOGAN.md)) | plugin framework |

## Licences

- Ostrich 32 as a whole: GPL-3.0-or-later ([LICENSE](LICENSE)).
- libmt32emu (`munt/mt32emu/`) and our core around it (`plugin/core/`):
  LGPL-2.1-or-later (`munt/mt32emu/COPYING.LESSER.txt`).
- Third-party code and its licences: [THIRD-PARTY.md](THIRD-PARTY.md).

"Roland", "MT-32", "CM-32L", "CM-64" and "LAPC-I" are trademarks of Roland Corporation.
This project is not affiliated with Roland, and neither is Munt.
