# plugin/

The Ostrich 55 plugin and the core it runs. Building: [../BUILDING.md](../BUILDING.md).

```
plugin/
  core/      GPL-2.0-or-later.  sc55::Core: Nuked-SC55's Emulator behind a small,
             real-time safe interface; sc55_core.h is the interface.  ROM recognition by
             SHA-256 (Nuked-SC55's table), the LCD snapshot and its font.
  ostrich/   GPL-3.0-or-later.  The plugin (DPF): OstrichPlugin.cpp, OstrichUI.cpp, the
             resampler from the core's rate to the host's, the zip reader.
  tools/     build_core.sh, sc55_render, ostrich_clap_render.
  dpf/       DPF, a submodule: our fork Helge666/DPF, branch logan (DPF-LOGAN.md).
```

The core is GPL-2.0-or-later like Nuked-SC55, so it could go back there; it uses nothing of
the plugin.

## The core

- `Core::start()` takes a romset name and the images as dumped (the wave ROMs are
  descrambled here), boots the firmware and starts the LCD. `skip()` runs past the boot.
- `render()` is real-time safe (no allocation, no locks, no file access) and renders at
  `sampleRate()`: 64 kHz on the SC-55 with oversampling, 32 kHz without.
- MIDI in through the MCU's serial port (`midi()`), panel keys as a bit mask
  (`setButtons()`), a snapshot of the display (`lcd()`, try-lock, may fail).

## The plugin

- `run()` renders the core in pieces between the host's MIDI events, resamples to the
  host's rate (voltaire::Resampler) and applies VOLUME.
- Panel keys are parameters, so a host can automate them. A press is held at least 0.1 s
  of emulated time (one step on the unit), and as long as the mouse button stays down.
- The display goes to the UI as a state string ("panel", hex) at 30 Hz; status, ROM setup
  and resets as further states (`OstrichShared.hpp`).
- ROMs: searched in the folders of README.md, by contents; CHOOSE FILE copies a set (loose
  files or a zip) into the first writable one. Only the instance the host activated writes
  files (under VST3 the controller has its own instance).
