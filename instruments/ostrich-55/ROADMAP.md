# Roadmap

The approach is the one Nuked-SC55 takes: the SC-55's own firmware on emulated MCUs, the
PCM chip after its die. Ostrich 55 adds the plugin, the panel and the ROM setup; the
emulation itself stays Nuked-SC55's, unmodified.

## Where it stands (2.11.2026)

- **Fork:** `nuked-sc55` is [Helge666/Nuked-SC55](https://github.com/Helge666/Nuked-SC55),
  a fork of [jcmoyer/Nuked-SC55](https://github.com/jcmoyer/Nuked-SC55), pinned to
  jcmoyer's `66d1272`.
- **Core** `sc55::Core` (`plugin/core/`): the backend behind a real-time safe interface;
  ROM recognition by SHA-256; boots v1.21 on Linux and Windows, renders at 64 kHz.
- **Plugin** (VST3, CLAP, standalone; Windows and Linux): panel with the LCD, the keys,
  VOLUME and the MIDI MESSAGE lamp; GS and GM reset; ROM setup in the panel (loose files
  or zips); keyboard and mouse wheel for the keys; a MIDI IN port choice in the Windows
  standalone (WinMM, time-stamped; played from a keyboard on Windows), a BUFFER choice.
- **Installer** for Windows (NSIS, `installer/`): written, not yet built or used; it comes
  later, together with Ostrich 220's (see Later).
- **Tested:** the VST3 in a DAW on Windows (2.11.2026: works); the Windows standalone with
  a USB keyboard (MIDI IN, BUFFER 64 without dropouts); the ROM dialog with a real v1.21
  set; the CLAP headless on Linux (`ostrich_clap_render`) at 44.1 and 48 kHz, with and
  without ROMs.

## Before the first public release (0.1)

- [x] Test on Windows: the VST3, the standalone, the ROM dialog with a real set.
- [ ] The CLAP in a DAW.
- [ ] Old songs: play a few GS songs through it and compare with memory and recordings of
      the unit, where there are any.
- [x] GitHub: [Helge666/ostrich55](https://github.com/Helge666/ostrich55) (2.11.2026), with
      both submodule pins pushed.
- [ ] CI like Ostrich 220: Linux and Windows builds without ROMs, smoke test, zips,
      release on a `v*` tag.

## Open

### Next

| What | Notes |
|---|---|
| Load a MIDI/SysEx file from the panel | to send a song's setup to the unit without a DAW track, as the U-220 panel does |
| ROM version | with several mk1 sets installed, v1.21 is played first; a choice in the panel or the state |
| Standalone latency | WASAPI shared mode with RtAudio: callbacks come in bursts of one device period (~10 ms), so steady MIDI timing costs about half a period on average. Lower: WASAPI low-latency shared mode (IAudioClient3) or ASIO in the standalone, both changes to RtAudio in our DPF fork (DPF-LOGAN.md, after Helge confirms). |

### Later

| What | Notes |
|---|---|
| SysEx into the VST3 | not essential, as with Ostrich 220. DPF's VST3 wrapper leaves SysEx input out (a `TODO` in `DistrhoPluginVST3.cpp`); GS songs' setup messages from a DAW track then do not arrive. A change in our DPF fork (DPF-LOGAN.md: in `dpf-logan`, a commit on `logan`, a new tag, after Helge confirms), for both Ostriches. The CLAP and the standalone take SysEx already. |
| Installer | `installer/ostrich55.nsi` and `installer/build.sh` are written (NSIS 3: VST3, CLAP, standalone, English and German, the ROM folder in ProgramData writable for users). Built and released later, together with an installer for Ostrich 220; needs `nsis` on the build host. |
| SC-55mk2 and the other models | Nuked-SC55 runs SC-55mk2, SC-55st, SC-155(mk2), CM-300/SCC-1(A), JV-880 and others. Each has its own rate (mk2: 66207 Hz), its own LCD and keys; the resampler takes any rational ratio. One plugin with a model choice, or one plugin per model. |
| State | the SC-55 forgets its panel settings at power-off. Check which models keep NVRAM (Nuked-SC55 saves it for some) and whether a session should restore the panel. |
| Platforms | the Windows standalone and the Linux builds have seen little use; no macOS build |

## Rules for upstream

Nuked-SC55 is archived; we report nothing there and ask nothing of nukeykt.
Fixes to the backend, if any, go into our fork with a clear commit message; offering them
to jcmoyer only with Helge, and only after checking how that project feels about
LLM-written contributions.
