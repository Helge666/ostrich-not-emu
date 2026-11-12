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
- [ ] ROM setup in the plugin: the panel's CHOOSE FILE as in Ostrich 55, zips included.
- [ ] Plugin: VST3, CLAP, standalone (Windows, Linux), MIDI in with sample-accurate timing,
      SysEx in (CLAP, standalone; VST3 needs the DPF fork, as for Ostrich 55). The core is
      not thread-safe: panel actions and the display go through the audio thread.
- [ ] Panel after the hardware ([docs/PANEL.md](docs/PANEL.md)): the MT-32's display and
      every button and the knob, all driving the firmware. Extra functions in the bar under
      the panel, as few as possible; a SETUP panel only if they do not fit.
- [ ] Real-time safety: `emu88_get_display_text` builds a `std::string`; read the display
      once per block or less, or find an allocation-free way.
- [ ] Startup: opening a machine takes about 0.7 s (firmware boot); keep it off the audio
      thread.
- [ ] `tools/ostrich_clap_render` and a smoke test without ROMs.
- [ ] CI as for Ostrich 55 and 7765: Linux and Windows (mingw, cross-compiled), zips with
      the licence texts, a `v*` tag publishes a release. 88emu's build needs about 660 MB of
      shallow submodules; cache them.
- [ ] GitHub repository `Helge666/ostrich32` (with the owner).

## Later

- [ ] The new MT-32 (control ROM v2.03, 2.04, 2.06, 2.07), the CM-32L / LAPC-I (v1.00,
      1.02) and the CM-32LN (v1.00): 88emu runs them, but they need the reverb ROM
      R15179917, which is not at hand. Dumps of all other images but v2.03 are
      (`~/coding/ostrich32-roms/mame-versioned`). Open: the CM-32L has no display or
      buttons; how its panel looks.
- [ ] The M-9 firmware boots with master volume 0: check whether that is how it is.
- [ ] Windows installer, as planned for Ostrich 55.
