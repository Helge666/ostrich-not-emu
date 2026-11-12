# The panel

Decided with the owner on 12.11.2026.

## Principle

- **The front is the MT-32's**: the LCD with its 20 characters and the MIDI MESSAGE lamp,
  and every control the unit has, where the unit has it:
  - PART 1, 2, 3, 4, 5 and RHYTHM (two rows of three),
  - SOUND GROUP, SOUND, VOLUME, MASTER VOLUME (two rows of two),
  - the SELECT/VOLUME knob.
  Drawn after the unit, not a copy of it: no Roland logo, "Ostrich 32" where the unit has
  its name (AGENTS.md).
- **Everything the unit does not have goes into the bar under the panel**, as in
  Ostrich 220 and Ostrich 55: the status line on the left, buttons on the right.
- **As few extra functions as possible.** Only if they do not fit into the bar: a SETUP
  panel, opened from the bar.

## What libmt32emu gives, and what we build

libmt32emu emulates the synthesiser and the **display** as MIDI drives it (the main view
with the parts and `|vol:`, the startup message, the program change view
`1|Synth-1>Soundtrack`, SysEx display messages, the checksum error). It does **not** run
the control ROM's front-panel code: the **buttons and the knob are not emulated**. Ostrich
32 builds them on top:

- A button press selects a mode (part, sound group, sound, volume, master volume); the
  knob changes the value of that mode.
- The change goes to the synth as the unit would make it: a program change on the part
  (libmt32emu then shows its own program change view), the part's volume, the master
  volume (libmt32emu then shows `|vol:` in its main view).
- What the display shows while a button mode is active and libmt32emu has no view for it
  is drawn by Ostrich 32, after the unit's behaviour as the MT-32 owner's manual describes
  it (the manual is consulted locally, never committed).
- The display and the lamp otherwise come from libmt32emu, character for character; the
  LCD's font is drawn by Ostrich 32 (a 5 x 8 dot matrix, yellow-green backlight).

## The bar

From the left: the status line (which ROM set runs, or what is missing and where it was
looked for). From the right, as few as possible:

| Control | Why the unit cannot do it |
|---|---|
| ROM images ... (CHOOSE FILE while images are missing) | the plugin needs the user's dumps |
| the control ROM (v1.04 ... BlueRidge, of those present) | the unit has one chip; the plugin can have several |
| MIDI IN, BUFFER (Windows standalone only) | as in Ostrich 55 |

Candidates only if needed, then possibly in SETUP: reset (all notes off / power-on),
libmt32emu's options (analogue output, DAC input mode, reverb, MIDI delay).
