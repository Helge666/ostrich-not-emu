# Roadmap

## Done

- [x] Repository with the submodules munt (our fork, `libmt32emu_2_8_6`, unmodified) and
      DPF (our fork, `logan-2`), the documents, `.gitignore` against Roland data, the
      version in `plugin/ostrich/Version.hpp` (12.11.2026).
- [x] Fork [Helge666/munt](https://github.com/Helge666/munt), its description naming Munt's
      authors and licences (12.11.2026).

## Next

- [ ] Core: libmt32emu built as a static library (CMake, C++ interface off, internal
      resampler), the ROM setup by SHA-1 with the panel's CHOOSE FILE as in Ostrich 55.
- [ ] Plugin: VST3, CLAP, standalone (Windows, Linux), MIDI in with sample-accurate timing,
      SysEx in (CLAP, standalone; VST3 needs the DPF fork, as for Ostrich 55).
- [ ] Panel after the hardware: the MT-32's front (LCD with its 20 characters, PART
      buttons, VOLUME, the MASTER VOLUME knob, MIDI MESSAGE lamp), drawn after the unit, not
      a copy of it; CM-32L as a second face.
- [ ] `tools/ostrich_clap_render` and a smoke test without ROMs.
- [ ] CI as for Ostrich 55 and 7765: Linux and Windows (mingw, cross-compiled), zips with
      the licence texts, a `v*` tag publishes a release.
- [ ] GitHub repository `Helge666/ostrich32` (with the owner).

## Later

- [ ] Settings libmt32emu offers: analogue output mode, reverb on/off and mode, DAC input
      mode, partial count, MIDI delay mode.
- [ ] Windows installer, as planned for Ostrich 55.
