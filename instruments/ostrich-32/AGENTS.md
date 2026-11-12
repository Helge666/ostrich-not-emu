# Ostrich 32

## About the project

An emulation of the Roland MT-32 (1987) and its relatives CM-32L / CM-64 / LAPC-I as a DAW
plugin (VST3, CLAP, standalone; Windows and Linux). The emulation is Munt's libmt32emu
(Dean Beeler, Jerome Fisher, Sergey V. Mikayev and the Munt contributors), used
unmodified; this project adds the plugin around it: a panel that looks and handles like
the hardware. The sibling of Ostrich 55 (SC-55), Ostrich 220 (U-220) and Ostrich 7765
(TR drums), and built the same way as Ostrich 55. README.md says what it does,
BUILDING.md how to build it, ROADMAP.md what is open.

## Rules

- **Never commit Roland data**: no control ROM, PCM ROM, SysEx dump, manual or service
  notes, not even briefly. Recordings of the emulation or of a real unit stay out of the
  repository too. `.gitignore` covers the known formats; check `git status` before every
  commit. The owner's dumps in `../Research/` never enter the repository.
- **Respect every licence; we stand on the shoulders of giants.** libmt32emu is
  LGPL-2.1-or-later: keep its notices, use it unmodified, credit Munt's authors and
  contributors (`munt/mt32emu/AUTHORS.txt`) wherever the project names where its sound
  comes from. New files say their licence in the first line (plugin and tools
  GPL-3.0-or-later). Third-party code goes into THIRD-PARTY.md.
- Make commits; do not push to the public repository. Pushing is done together with the
  owner, step by step.
- Do not contact other developers or projects. Bugs found in libmt32emu are written down
  here first; anything for munt/munt only with the owner, after checking its policy on
  LLM-written contributions.
- Compile with at most `-j5` and `nice -n 19`; the build host stays responsive.
- On the build host change no audio or video settings and open no audio device (it streams
  its desktop); listening and DAW tests happen on the owner's Windows machine.
- Ask before drawing windows on the owner's desktop.
- Do not offer to run commands as root; say what needs installing.
- No Roland logo and nothing that looks like Roland's own product; "Roland", "MT-32",
  "CM-32L", "CM-64" and "LAPC-I" are named only to say what is emulated, with the trademark
  notice.
- The user interface and the repository are English; the conversation with the owner is
  German.

## Facts that are easy to get wrong

- The submodule `munt/` is our fork Helge666/munt, pinned to an upstream tag
  (`libmt32emu_2_8_6`), unmodified. Only the library `munt/mt32emu` is built; mt32emu_qt
  and the drivers (GPL) are not part of Ostrich 32.
- libmt32emu recognises the ROM images by SHA-1 (`munt/mt32emu/src/ROMInfo.cpp`), never by
  name; it also joins the two halves of the MT-32's split control and PCM ROMs.
- DPF is our fork Helge666/DPF (branch `logan`, pinned to a tag); nothing is patched at
  build time and `plugin/dpf` stays clean. Changes to DPF are commits in the fork
  (DPF-LOGAN.md).
- The version is in `plugin/ostrich/Version.hpp`, nowhere else; CI will refuse a `v*` tag
  that does not match it.
- On Windows, files written by tools inside a packaged (MSIX) app under `%LOCALAPPDATA%`
  are invisible to the DAW; the ROM folder is `C:\ProgramData\Ostrich 32\roms`.
- Build host: kellerkind2, `~/coding/ostrich32`. Test ROMs there live outside the
  repository, in `~/coding/ostrich32-roms`.
