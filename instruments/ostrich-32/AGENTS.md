# Ostrich 32

Part of the monorepo Ostrich != Emu: read the root AGENTS.md first (rules for all
instruments, the build host, shared code); this file adds what is particular to this one.

## About the project

An emulation of the Roland MT-32 (1987) and its relatives CM-32L / CM-32LN as a DAW plugin
(VST3, CLAP, standalone; Windows and Linux). The emulation is 88emu from gearmulator (The
Usual Suspects; LA32 derived from nukeykt's Nuked-MT32, reverb from Sergey V. Mikayev's
emulator in Munt), which runs the unit's own firmware; this project adds the plugin around
it: a panel that looks and handles like the hardware. Until 12.11.2026 the core was Munt's
libmt32emu; the owner chose 88emu because Munt sounded similar to their own MT-32's
recordings, but not the same. The sibling of Ostrich 55 (SC-55), Ostrich 220 (U-220) and Ostrich 7765
(TR drums), and built the same way as Ostrich 55. README.md says what it does,
BUILDING.md how to build it, ROADMAP.md what is open.

## Rules

- **Never commit Roland data**: no control ROM, PCM ROM, SysEx dump, manual or service
  notes, not even briefly. Recordings of the emulation or of a real unit stay out of the
  repository too. `.gitignore` covers the known formats; check `git status` before every
  commit. The owner's dumps in `../Research/` never enter the repository.
- **Respect every licence; we stand on the shoulders of giants.** gearmulator is GPL-3.0,
  with parts under their own licences (`third_party/gearmulator/doc/88emu_readme.md`): keep its
  notices, change it only by commits in our fork, and credit The Usual Suspects, nukeykt
  (Nuked-MT32), Sergey V. Mikayev and Munt wherever the project names where its sound comes
  from. New files say their licence in the first line (GPL-3.0-or-later). Third-party code
  goes into THIRD-PARTY.md.
- Make commits; do not push to the public repository. Pushing is done together with the
  owner, step by step.
- Do not contact other developers or projects. Bugs found in 88emu are written down in
  ROADMAP.md first; anything for dsp56300/gearmulator only with the owner, after checking
  its policy on LLM-written contributions.
- Do not run executables downloaded from elsewhere (patchers, installers); the owner does
  that.
- Compile with at most `-j5` and `nice -n 19`; the build host stays responsive.
- On the build host change no audio or video settings and open no audio device (it streams
  its desktop); listening and DAW tests happen on the owner's Windows machine.
- Ask before drawing windows on the owner's desktop.
- Do not offer to run commands as root; say what needs installing.
- No Roland logo and nothing that looks like Roland's own product; "Roland", "MT-32",
  "CM-32L" and "CM-32LN" are named only to say what is emulated, with the trademark notice.
- The user interface and the repository are English; the conversation with the owner is
  German.

## Facts that are easy to get wrong

- The submodule `third_party/gearmulator/` is our fork Helge666/gearmulator, pinned to a tag
  (`ostrich-N`) on its branch `ostrich`: an upstream release plus only commits a mingw-w64
  build needs; its dsp56300 submodule points to Helge666/dsp56300 the same way
  (BUILDING.md). Only 88emu is built (`88emu_bundle`). Never clone it with all submodules:
  JUCE and the rest are large and unused.
- 88emu runs the firmware: the display, the buttons, the knob and the MIDI MESSAGE lamp are
  the firmware's, through `emu88_set_panel_buttons`, `emu88_turn_panel_encoder`,
  `emu88_get_display_text`, `emu88_get_panel_leds`. Ostrich 32 draws them, it does not
  imitate them (docs/PANEL.md).
- 88emu finds ROMs by searching folders (process-wide, by content, not name) and picks the
  firmware it likes best when several are present. The core therefore identifies the
  images itself with 88emu's registry (`88lib/rom/romRegistry.h`) and opens each machine
  from a private temp folder holding exactly its images, one machine at a time.
- A board needs its reverb ROM: 88emu runs the Boss reverb's microcode. The new MT-32 and
  the CM-32L(N) need R15179917, which is not at hand yet.
- An 88emu context is not thread-safe. The core takes MIDI with frame offsets and renders
  up to each one before handing the message over (88emu takes it with the next frame).
- The M-9 firmware boots with master volume 0 (seen 12.11.2026); not a bug of ours.
- DPF is our fork Helge666/DPF (branch `logan`, pinned to a tag); nothing is patched at
  build time and `third_party/dpf` stays clean. Changes to DPF are commits in the fork
  (DPF-LOGAN.md).
- The version is in `plugin/ostrich/Version.hpp`, nowhere else; CI refuses an `ostrich32-v*` tag
  that does not match it.
- On Windows, files written by tools inside a packaged (MSIX) app under `%LOCALAPPDATA%`
  are invisible to the DAW; the ROM folder is `C:\ProgramData\Ostrich 32\roms`.
- Test ROMs on the build host: `/data/coding/Ostrich-not-Emu/roms/ostrich-32` (`OSTRICH32_ROMS` for
  `shared/tools/ostrich_clap_render`).
