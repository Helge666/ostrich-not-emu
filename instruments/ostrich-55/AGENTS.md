# Ostrich 55

## About the project

An emulation of the Roland SC-55 Sound Canvas (1991) as a DAW plugin (VST3, CLAP,
standalone); a Windows installer is planned. The emulation is Nuked-SC55 (nukeykt), in J.C.
Moyer's fork, used unmodified; this project adds the plugin around it. The sister of
Ostrich 220 (U-220). README.md says what it does, BUILDING.md how to build it, ROADMAP.md
what is open.

## Rules

- **Never commit Roland data**: no firmware, wave ROM, SysEx dump, manual or service
  notes, not even briefly. Recordings of the emulation or of a real unit stay out of the
  repository too. `.gitignore` covers the known formats; check `git status` before every
  commit.
- **Respect every licence.** Nuked-SC55 is GPL-2.0-or-later: keep its notices, use it
  unmodified where possible, credit nukeykt, J.C. Moyer and its contributors. New files say
  their licence in the first line (core GPL-2.0-or-later, plugin and tools
  GPL-3.0-or-later). Third-party code goes into THIRD-PARTY.md.
- Make commits; do not push to the public repository. Pushing is done together with the
  owner, step by step.
- Do not contact other developers or projects. nukeykt has archived Nuked-SC55: no issues,
  no pull requests, no requests of any kind there. Anything for jcmoyer's
  fork only with the owner, after checking its policy on LLM-written contributions.
- Compile with at most `-j5` and `nice -n 19`; the build host stays responsive.
- On the build host change no audio or video settings and open no audio device (it streams
  its desktop); listening and DAW tests happen on the owner's Windows machine.
- Ask before drawing windows on the owner's desktop.
- Do not offer to run commands as root; say what needs installing.
- No Roland logo and nothing that looks like Roland's own product; "Roland", "SC-55" and
  "Sound Canvas" are named only to say what is emulated, with the trademark notice.

## Facts that are easy to get wrong

- The SC-55 (mk1) core runs at 64 kHz (Nuked-SC55's oversampling; 32 kHz without); the
  mk2 at 66207 Hz. The plugin resamples to the host's rate.
- The firmware needs about 2.5 s of emulated time to boot; the plugin runs that before it
  plays.
- Nuked-SC55's LCD needs a backend, or it never updates its buffer; the core passes an
  empty one.
- The ROM images are recognised by SHA-256, never by name; the wave ROMs are descrambled
  by the core, not on disk.
- DPF is our fork Helge666/DPF (branch `logan`, pinned to a tag); nothing is patched at
  build time and `plugin/dpf` stays clean. Changes to DPF are commits in the fork
  (DPF-LOGAN.md).
- On Windows, files written by tools inside a packaged (MSIX) app under `%LOCALAPPDATA%`
  are invisible to the DAW; the ROM folder is `C:\ProgramData\Ostrich 55\roms`.
- Build host: kellerkind2, `~/coding/ostrich55`. Test ROMs there live outside the
  repository, in `~/coding/ostrich55-roms`.
