# Roadmap

## Done

- [x] Repository with the submodules munt (our fork, `libmt32emu_2_8_6`, unmodified) and
      DPF (our fork, `logan-2`), the documents, `.gitignore` against Roland data, the
      version in `plugin/ostrich/Version.hpp` (12.11.2026).
- [x] Fork [Helge666/munt](https://github.com/Helge666/munt), its description naming Munt's
      authors and licences (12.11.2026).
- [x] First core on libmt32emu (12.11.2026). It played all old MT-32 sets, but sounded
      similar to the owner's recordings of their MT-32, not the same.
- [x] Engine changed to **88emu** (gearmulator), which runs the original firmware, the LA32
      after Nuked-MT32 and the reverb microcode (12.11.2026). Forks
      [Helge666/gearmulator](https://github.com/Helge666/gearmulator) and
      [Helge666/dsp56300](https://github.com/Helge666/dsp56300), branch `ostrich`, tag
      `ostrich-1` (upstream 2.2.27 plus mingw-w64 fixes, BUILDING.md); `munt/` removed.
- [x] Core on 88emu (12.11.2026): `plugin/core/mt32_core` (images identified with 88emu's
      registry, the complete machines per firmware, each opened from its own temp folder;
      MIDI with frame offsets; rendering at the host's rate; panel switches, knob, LCD and
      MIDI MESSAGE lamp from the firmware), `mt32_render` with a panel test. Linux and
      Windows (mingw) build and render the same, bit for bit; all six old MT-32 firmwares
      start and play.
- [x] Plugin, first state (12.11.2026): `plugin/ostrich` after Ostrich 55 (VST3, CLAP,
      standalone; Linux and Windows). ROM search by content in the ROM folders and zips,
      CHOOSE FILE while none is complete (copies to `C:\ProgramData\Ostrich 32\roms`);
      the control ROM in the bar (the machines the images make; v1.04 by default; the
      session keeps it); the machine starts in `activate()` or on a worker thread, never on
      the audio thread; MIDI at its frame; MIDI IN and BUFFER for the Windows standalone as
      in Ostrich 55. A plain panel: the 20 characters (drawn with a font), MIDI MESSAGE,
      the ten switches, the knob (the firmware's pot; drag, wheel over knob or display,
      arrow keys). `shared/tools/ostrich_clap_render` plays through the CLAP.

## Next

First target: the **old MT-32** (control ROM v1.04, 1.05, 1.06, 1.07, Blue Ridge, M-9; PCM
and reverb ROM of the old board). The owner's own unit was bought in 1987, so an early
version; the panel and the defaults follow the old MT-32, the earliest set (v1.04) first.
These sets are at hand on the build host (`~/coding/ostrich32-roms`).

- [ ] Listening check by the owner, by rebuilding songs: the owner's MT-32 songs of 1987
      survive only as digitised cassette recordings (no MIDI files), made with the built-in
      sounds only. The owner plans to rebuild some of them faithfully in Ostrich 32 and
      compare by ear. So what matters first: the presets as the firmware has them,
      sample-accurate MIDI, the display and buttons for finding sounds, and the early
      firmwares selectable (v1.04 default, 1.05 to 1.07 to try). SysEx stays planned (DOS
      games load their sounds with it), but after the basics.
- [ ] The owner's first look on Windows: the VST3 in Cubase (installed by
      `tools\install-plugins.ps1` in `C:\Program Files\Common Files\VST3`, the CLAP in
      `...\Common Files\CLAP`; from the monorepo it works in Cubase, 13.11.2026), the switches
      and the knob against the unit's behaviour, switching the control ROM. Seen in the
      standalone (12.11.2026): the ROMs from a zip are taken; a keyboard plays once it
      sends on channel 2. The VST3 in Cubase (12.11.2026): playable latency; switches,
      knob, display and sound work. Still to see: switching the control ROM, and the
      session restoring it.
- [ ] Testing happens in the plugin in Cubase from now on: the Windows standalone's
      latency is unplayable even at 64 frames (as for Ostrich 55: WASAPI shared mode
      through RtAudio calls in bursts of a device period). Lower latency there needs
      WASAPI low-latency shared mode or ASIO in our DPF fork, with the owner.
- [ ] **Part = Ch** (the owner's decision, 13.11.2026): a checkbox in the bar, on by
      default. While it is on, incoming MIDI on channels 1-5 goes to the MT-32 on 2-6, so
      that the parts the display shows (1-5) match the MIDI channels; a keyboard on
      channel 1 plays PART 1. The MT-32 itself hears PART 1 on channel 2 and nothing on 1
      (RHYTHM on 10). Its state is kept in the session. Still to settle with the owner:
      what incoming channels 6-9 do while it is on (6 would reach the part that 5 now
      reaches).
- [ ] SysEx in: CLAP and standalone pass it; VST3 needs the DPF fork, as for Ostrich 55.
- [ ] Panel after the hardware ([docs/PANEL.md](docs/PANEL.md)): the LCD as the unit's 5 x 8
      dot matrix (the font of its controller), the body, the switches and the knob after
      the unit. Extra functions in the bar, as few as possible.
- [ ] The MIDI MESSAGE lamp is read 30 times a second; a short blink may be missed. Read
      the LEDs every block if it looks wrong.
- [ ] States meant only for the panel (status, romset, machines, panel) end up in the saved
      session too (seen in the CLAP state); harmless, but tidy that up (Ostrich 55 too).
- [ ] Real-time safety: `emu88_get_display_text` builds a `std::string`; read the display
      once per block or less, or find an allocation-free way.
- [ ] CI in the monorepo, as for the other instruments (`ci/`, `tools/ci/`): Linux and
      Windows (mingw, cross-compiled), zips with the licence texts, an `ostrich32-v*` tag
      publishes a release. Written 13.11.2026; green on GitHub (13.11.2026). 88emu's
      build needs about 660 MB of shallow submodules; cache them if it is slow.
- [x] GitHub: part of the monorepo `Helge666/ostrich-not-emu` instead of a repository of
      its own (13.11.2026).

## Later

- [ ] The new MT-32 (control ROM v2.03, 2.04, 2.06, 2.07), the CM-32L / LAPC-I (v1.00,
      1.02) and the CM-32LN (v1.00): 88emu runs them, but they need the reverb ROM
      R15179917, which is not at hand. Dumps of all other images but v2.03 are
      (`~/coding/ostrich32-roms/mame-versioned`). Open: the CM-32L has no display or
      buttons; how its panel looks.
- [ ] The M-9 firmware boots with master volume 0: check whether that is how it is.
- [ ] Windows installer, as planned for Ostrich 55.
