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
- [x] The session keeps the unit's setup (14.11.2026; the owner: "it forgets its settings").
      The core keeps a copy of the unit's memory (88emu's `mt32::StateTracker`) from every
      message the machine is given; what the panel changes it learns by data requests
      (RQ1: system area and patch temps, 167 bytes), 250 ms after the panel was last
      touched, or when the host saves before that. The firmware sends the answers between
      the notes it plays: reading the timbre temps and rhythm setup too (2.4 KB, the first
      version, 14.11.2026) made the owner's playing stumble -- measured, 4 of 40 hi-hats lost
      and one 56 ms late; the 167 bytes lose none. The timbre temps need no reading: where
      the panel picked another sound, the copy loads that timbre as the unit does, and a
      restore selects the sound so that the unit loads it again. Saved as the state
      `setup` (`<machine> <SysEx as hex> ...`, from
      88emu's `createRestore`), brought back after the boot, paced as the firmware wants
      (faster pacing gives "Exc. Buffer overflow" on 1.04), and carried over when the
      control ROM changes within a board. The fork's tag `ostrich-2` adds
      `emu88_read_midi_out` for the answers. `mt32_setup_test` checks it without a host.
- [x] **Part = Ch** (the owner's decision, 13.11.2026; done 14.11.2026): PART = CH in the bar,
      on by default and kept in the session (state `partch`). While it is on, incoming MIDI
      on channels 1-8 goes to the MT-32 on 2-9: the eight synth parts are then on the
      channels of their numbers, RHYTHM on 10. Channel 9 coming in reaches PART 8 as
      channel 8 does; channels 10-16 stay as they are. Sessions made before it have it on
      too: their tracks on channels 2-9 then play one part further; untick it there.
- [x] The knob follows the volume the display shows (14.11.2026; the owner: "the volume
      knob is always fully clockwise"). The unit's knob is a pot the firmware reads (10 bit,
      value = (pot >> 3) * 100 / 127, measured for every value on 1.04 to 1.07); the plugin's
      knob now has a motor: when MASTER VOLUME or a part's VOLUME is shown and the knob was
      left alone for 0.3 s, it moves to where that value lies (the fork's tag `ostrich-3`
      adds `emu88_set_panel_knob`). A part's volume changes only once the pot passes it
      (the firmware's pickup), so the jump changes nothing; in SOUND the pot picks the
      sound at once, so there the knob stays where it is. A drag, wheel notch or arrow
      key is one of the 128 steps the firmware tells apart. RHYTHM's volume too (the owner,
      14.11.2026: the knob stood still there): its screen reads "Rhythm Part |vol> 80".
      Which volume the knob follows comes from the display, not from the copy of the
      unit's memory: which screen is up is the firmware's, not in its memory, and the copy
      learns a panel change only with the next reading, after the knob has moved.
- [x] The session keeps the panel's screen (the owner's idea, 14.11.2026: after loading,
      the unit stood on its main screen, not on PART 4 with VOLUME where it was left).
      Learnt from the display's text (state `screen`: "main", "rhythm", "part 4 volume",
      "part 4 sound", "part 4 group"); a machine that starts -- a session loaded, another
      control ROM -- is brought back there once it has booted and taken the setup: PART n,
      then VOLUME, SOUND or SOUND GROUP, one after the other (held together they would be
      another combination, as MASTER VOLUME + VOLUME is the reverb setting). Measured on
      1.04-1.07: the part keeps the mode from the part before, a second press changes
      nothing, no screen times out to the main one. Checked through the CLAP: after a
      session with PART 4 and SOUND is loaded, VOLUME leads to PART 4's volume (to PART 1's
      without the restore); the same on 1.07.
- [x] CI in the monorepo, as for the other instruments (`ci/`, `tools/ci/`): Linux and
      Windows (mingw, cross-compiled), zips with the licence texts, an `ostrich32-v*` tag
      publishes a release (13.11.2026, green on GitHub). 88emu's build needs about 660 MB
      of shallow submodules; cache them if it is slow.
- [x] GitHub: part of the monorepo `Helge666/ostrich-not-emu` instead of a repository of
      its own (13.11.2026).
- [x] The **EFFECT tab** (the owner, 14.11.2026; it replaced the REVERB MODE button planned
      earlier the same day; done 14.11.2026). EFFECT in the bar (the owner, after the first
      look: one switch, not two tabs) shows the page in the unit's place, lit orange while
      it does; clicked again, the unit. The page sets what the unit can do but its panel
      cannot reach, or only with two switches held:
      - Reverb mode (Room, Hall, Plate, Tap Delay), time (1-8) and level (0-7): the system
        area, 10 00 01-03.
      - The unit's own presets 0-10: on the unit MASTER VOLUME held, then VOLUME, shows
        "** Reverb mode : n" and the knob picks n (bands of about 100 of the pot's 1024).
        Each sets Room and a time and level, measured on 1.04, 1.05, 1.06 and 1.07, all
        alike (time as 1-8): 0 = 1/0, 1 = 3/0, 2 = 4/1, 3 = 5/2, 4 = 5/3, 5 = 6/3 (the
        power-on setting), 6 = 6/4, 7 = 6/5, 8 = 6/6, 9 = 6/7, 10 = 7/7. The tab sends these
        values as SysEx; a preset is lit while the settings are one of them.
      - Reverb on/off per part 1-8: the patch temp's reverb switch (offset 6). The firmware
        has only on/off, no send level per part. It is on at power-on and in all 128 factory
        patches, and the old unit's panel cannot change it (the owner's question,
        14.11.2026: in their 1987 recordings some parts seem dry; with the built-in sounds
        every part went through the reverb, so that is more likely the pans -- power-on:
        part 7 hard left, part 8 hard right -- the levels and the sounds' own envelopes).
      - RHYTHM's reverb as one switch (the owner's decision): every key of the rhythm setup
        at once (03 01 10, 4 bytes a key, the switch the 4th; 64 keys on the old board, 85
        on the new), in messages of 32 keys with the keys' other bytes as the unit has them.
      How (the owner's decisions, 14.11.2026): the tab shows the unit's reverb as the copy
      of its memory has it (`Core::effect()`), read by the worker thread whenever it
      changed. What the tab asks for is sent once it has been left alone for 250 ms, only
      where it differs from the unit, one message at a time with the time the firmware
      wants (as a restore), never while a reading or restore is under way. On the old
      firmware a part's switch reloads its sound and may cut its notes (accepted). A
      program change brings the patch's own switch back: where this instance has a part's
      switched off (state `reverbsw`, kept in the session), the plugin sends it off again
      right after the program change, at the same frame, before the notes that follow. A
      new instance starts with the power-on settings. Checked through the CLAP
      (`ostrich_clap_render --midi`): Plate, preset 9, a part off, RHYTHM off, the
      re-send after a program change, a session restored into a new instance; the sound
      of a note unchanged, bit for bit.

## Ongoing

- The owner's listening check, by rebuilding songs (begun 14.11.2026; not a step but an
  evaluation that goes on alongside everything else). The owner's MT-32 songs of 1987
  survive only as digitised cassette recordings (no MIDI files), made with the built-in
  sounds only; the owner rebuilds some of them in Ostrich 32 and compares by ear. What it
  needs: the presets as the firmware has them, sample-accurate MIDI, the display and
  buttons for finding sounds, the early firmwares selectable (v1.04 default, 1.05 to 1.07
  to try).

## Next

First target: the **old MT-32** (control ROM v1.04, 1.05, 1.06, 1.07, Blue Ridge, M-9; PCM
and reverb ROM of the old board). The owner's own unit was bought in 1987, so an early
version; the panel and the defaults follow the old MT-32, the earliest set (v1.04) first.
These sets are at hand on the build host (`/data/coding/Ostrich-not-Emu/roms/ostrich-32`).
The owner's order (14.11.2026): the separate outputs next.

- [ ] **Separate outputs**: one stereo output per part 1-8 plus RHYTHM (the owner,
      14.11.2026). Always dry: the reverb is heard only on the main output (the owner's
      decision). Open how: the LA32 chip mixes all partials into one DAC stream and the
      reverb works on that sum, so 88emu has no per-part signal. It needs the LA32
      emulation to sum the partials per part as well, with the partial-to-part assignment
      taken from the firmware's RAM. Research first (88emu's LA32 and the firmware's
      partial table); a change in our gearmulator fork.
- [ ] The owner's look on Windows: the VST3 in Cubase (installed by
      `tools\install-plugins.ps1` in `C:\Program Files\Common Files\VST3`, the CLAP in
      `...\Common Files\CLAP`). Seen: playable latency; switches, knob, display and sound
      work (12.11.2026); switching the control ROM works (14.11.2026). Still to see: the
      session restoring the control ROM.
- [ ] Testing happens in the plugin in Cubase from now on: the Windows standalone's
      latency is unplayable even at 64 frames (as for Ostrich 55: WASAPI shared mode
      through RtAudio calls in bursts of a device period). Lower latency there needs
      WASAPI low-latency shared mode or ASIO in our DPF fork, with the owner.
- [ ] States meant only for the panel (status, romset, machines, panel) end up in the saved
      session too (seen in the CLAP state); harmless, but tidy that up (Ostrich 55 too).
- [ ] Real-time safety: `emu88_get_display_text` builds a `std::string`; read the display
      once per block or less, or find an allocation-free way. The setup's copy of the unit's
      memory takes a mutex on the audio thread for every message that writes memory (88emu's
      `StateTracker`), held while the host saves (`createRestore`), and the answers to the
      data requests are parsed there into vectors. No dropout heard from it so far.

## Later

- [ ] Reverb on/off per rhythm key (the owner, 14.11.2026: perhaps later; RHYTHM has one
      switch for all keys now). Layout ideas: a keyboard strip, or a list grouped by kind
      with a switch per group; a note on channel 10 selects its key.
- [ ] SysEx in for VST3 (nice to have, not essential; the owner, 14.11.2026, as for the
      other plugins): CLAP and standalone pass it already; VST3 needs the DPF fork, as for
      Ostrich 55.
- [ ] Panel after the hardware ([docs/PANEL.md](docs/PANEL.md)): the LCD as the unit's 5 x 8
      dot matrix (the font of its controller), the body, the switches and the knob after
      the unit. Less important than SysEx in, perhaps not needed at all (the owner,
      14.11.2026).
- [ ] The new MT-32 (control ROM v2.03, 2.04, 2.06, 2.07), the CM-32L / LAPC-I (v1.00,
      1.02) and the CM-32LN (v1.00): 88emu runs them, but they need the reverb ROM
      R15179917, which is not at hand. Dumps of all other images but v2.03 are
      (`/data/coding/Ostrich-not-Emu/roms/ostrich-32/mame-versioned`). Open: the CM-32L has no display or
      buttons; how its panel looks.
- [ ] The M-9 firmware boots with master volume 0: check whether that is how it is.
- [ ] Windows installer, as planned for Ostrich 55.
