# Roadmap

## Done

- [x] Repository with the submodules munt (our fork, `libmt32emu_2_8_6`, unmodified) and
      DPF (our fork, `logan-2`), the documents, `.gitignore` against Roland data, the
      version in `plugin/ostrich/Version.hpp` (12.11.2026).
- [x] Fork [Helge666/munt](https://github.com/Helge666/munt), its description naming Munt's
      authors and licences (12.11.2026).

## Next

First target: the **old MT-32** (control ROM v1.04, 1.05, 1.06, 1.07, BlueRidge; MT-32 PCM
ROM). The owner's own unit was bought in 1987, so an early version; the panel and the
defaults follow the old MT-32, the earliest set (v1.04) first. These sets are at hand on the
build host (`~/coding/ostrich32-roms`, recognised by libmt32emu on 12.11.2026).

- [x] Core (12.11.2026): libmt32emu built with Munt's CMake as a static library (C++
      interface, built-in resampler), `plugin/core/mt32_core` (ROM images by SHA-1, the
      complete machines, MIDI with frame offsets, rendering at the host's rate, the LCD and
      the MIDI MESSAGE lamp), `mt32_render`. Linux and Windows (mingw) build; all five old
      MT-32 sets play, sound starts on the frame asked for.
- [ ] ROM setup in the plugin: the panel's CHOOSE FILE as in Ostrich 55, zips included.
- [ ] Listening check: after a note-off of the test chord (v1.04, part 1's power-on sound) there is
      a short burst of level; compare with mt32emu_qt and, if possible, a real unit.
- [ ] Plugin: VST3, CLAP, standalone (Windows, Linux), MIDI in with sample-accurate timing,
      SysEx in (CLAP, standalone; VST3 needs the DPF fork, as for Ostrich 55).
- [ ] Panel after the hardware ([docs/PANEL.md](docs/PANEL.md)): the MT-32's display and
      every button and the knob; the buttons' behaviour built on top of libmt32emu (it
      emulates the display, not the front panel), after the owner's manual. Extra functions
      in the bar under the panel, as few as possible; a SETUP panel only if they do not fit.
- [ ] `tools/ostrich_clap_render` and a smoke test without ROMs.
- [ ] CI as for Ostrich 55 and 7765: Linux and Windows (mingw, cross-compiled), zips with
      the licence texts, a `v*` tag publishes a release.
- [ ] GitHub repository `Helge666/ostrich32` (with the owner).

## Later

- [ ] The new MT-32 (control ROM v2.03, 2.04, 2.06, 2.07) and the CM-32L / CM-64 / LAPC-I
      (v1.00, 1.02) and CM-32LN (v1.00): libmt32emu plays them already; needs dumps for
      testing and, for the CM-32L, a panel of its own. No such dumps at hand yet.
- [ ] Settings libmt32emu offers: analogue output mode, reverb on/off and mode, DAC input
      mode, partial count, MIDI delay mode.
- [ ] Windows installer, as planned for Ostrich 55.
