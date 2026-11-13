# Ostrich != Emu

## About the project

The home of the Ostrich instruments: emulations of hardware instruments that run the
unit's own firmware, in whole or in part, as DAW plugins (VST3, CLAP) and standalone
programs. One folder per instrument under `instruments/`; each has its own AGENTS.md,
README.md, BUILDING.md and ROADMAP.md, which add what is particular to it. Read this file
first, then the instrument's.

| Folder | Instrument |
|---|---|
| `instruments/ostrich-32` | Ostrich 32: Roland MT-32, CM-32L(N), on gearmulator's 88emu |
| `instruments/ostrich-55` | Ostrich 55: Roland SC-55, on Nuked-SC55 |
| `instruments/ostrich-220` | Ostrich 220: Roland U-220, on MAME's devices and its own core |
| `instruments/ostrich-7765` | Ostrich 7765: Roland TR-505, TR-626, TR-707, TR-727 |
| `shared/` | code more than one instrument uses (shared/README.md) |
| `third_party/` | submodules: DPF (all), MAME (220), Nuked-SC55 (55), gearmulator (32) |
| `tools/` | build and install scripts for all instruments |

BUILDING.md says how to build and check; DPF-LOGAN.md what our DPF fork is.

## Rules for all instruments

- **Never commit manufacturer data**: no firmware, wave ROM, SysEx dump, card dump, manual
  or service notes, not even briefly. Recordings of the emulation or of a real unit stay
  out of the repository too. `.gitignore` (root and instrument) covers the known formats;
  check `git status` before every commit.
- **Respect every licence.** Keep third-party notices; each instrument's THIRD-PARTY.md
  lists what its binaries contain. New files say their licence in the first line.
- Make commits; do not push to the public repository. Pushing is done together with the
  owner, step by step. Pushing to the build host (`git push kk2 main`) to build there is
  fine.
- Do not contact other developers or projects; anything for upstream only with the owner.
- Compile with at most `-j5` and `nice -n 19`; the build host stays responsive.
- On the build host change no audio or video settings and open no audio device (it streams
  its desktop); listening and DAW tests happen on the owner's Windows machine.
- Ask before drawing windows on the owner's desktop.
- Do not offer to run commands as root; say what needs installing.
- No manufacturer logos and nothing that looks like the manufacturer's own product; names
  of units are used only to say what is emulated, with the trademark notice.
- **Never change** a released plugin's VST3 UID, CLAP id, binary name or the paths of its
  settings and ROM folders: hosts and saved projects find the plugin by them.
- A change in `shared/` or `third_party/` affects several instruments: build them all and
  compare renders before and after (BUILDING.md, section 4).

## Facts that are easy to get wrong

- Build host: kellerkind2, working copy `/data/coding/Ostrich-not-Emu/ostrich-not-emu`
  (remote `kk2`, which updates the checkout on push). It cannot reach GitHub by SSH;
  pushes to the forks go from the owner's Windows machine.
- Test ROMs on the build host live outside the repository (each instrument's AGENTS.md
  says where).
- The emulation forks (mame, Nuked-SC55, gearmulator, dsp56300) carry our commits on a
  branch `ostrich`; DPF and pugl on `logan`, shared with the LOGAN plugins.
- Releases are tagged per instrument: `ostrich55-v0.1.0`, `ostrich220-v0.1.4`, ...
- On Windows `tools\install-plugins.ps1` installs to `C:\Program Files\Common Files\VST3`
  and `...\CLAP` (the owner's choice: the usual place, one UAC prompt per install), the
  standalones to `C:\Program Files\Ostrich-not-Emu`. Files written by tools inside a
  packaged (MSIX) app under `%LOCALAPPDATA%` are invisible to the DAW, so the ROM folders
  are under `C:\ProgramData`.
