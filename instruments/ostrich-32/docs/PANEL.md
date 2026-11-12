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

## What 88emu gives, and what we draw

88emu runs the unit's own control ROM, front-panel code included (since 12.11.2026; the
first core, Munt's libmt32emu, had no panel). So Ostrich 32 **imitates nothing**:

- A button on the panel is a switch held down for the firmware (`emu88_set_panel_buttons`,
  one bit per switch, several at once possible). The firmware reads the panel at its own
  pace; a click is held for at least a few milliseconds of rendering.
- The knob turns the firmware's VOLUME/VALUE pot by 32nds of its travel
  (`emu88_turn_panel_encoder`); a mouse drag or the wheel becomes steps. The firmware
  treats it as an absolute pot, as on the unit.
- The 20 characters and the MIDI MESSAGE lamp are what the firmware writes to the LCD
  controller and the lamp, character for character (`emu88_get_display_text`,
  `emu88_get_panel_leds` bit 0). Ostrich 32 draws them: the LCD's font as a 5 x 8 dot
  matrix, yellow-green backlight.
- Seen on v1.04 (12.11.2026): MASTER VOLUME, then the knob: `vol:100` to `vol: 87` and
  `vol: 74`; SOUND shows `1|Bass   >Slap Bass1` and the knob steps through the sounds; on
  MIDI the part's number blanks for a moment, as on the unit.

## The bar

From the left: the status line (which ROM set runs, or what is missing and where it was
looked for). From the right, as few as possible:

| Control | Why the unit cannot do it |
|---|---|
| ROM images ... (CHOOSE FILE while images are missing) | the plugin needs the user's dumps |
| the control ROM (v1.04 ... Blue Ridge, M-9, of those present) | the unit has one chip; the plugin can have several |
| MIDI IN, BUFFER (Windows standalone only) | as in Ostrich 55 |

Candidates only if needed, then possibly in SETUP: reset (all notes off / power-on), the
power-on intro the firmware shows (88emu skips it by default).
