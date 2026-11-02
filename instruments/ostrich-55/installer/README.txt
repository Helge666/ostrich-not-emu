Ostrich 55
==========

An emulation of the Roland SC-55 Sound Canvas (1991) as a VST3 and CLAP plugin and a
standalone program. It runs the unit's original firmware on Nuked-SC55, the emulation by
nukeykt, in J.C. Moyer's fork. A non-commercial fan project, not affiliated with Roland.

You need your own ROM images of an SC-55 (mk1, firmware v1.00, 1.10, 1.20, 1.21 or 2.00).
This program contains none.

First start
-----------

Load Ostrich 55 in your DAW, or start it from the Start menu. Its panel asks for the ROM
images: click CHOOSE FILE and pick any one of them, or a zip with them. The others in the
same folder are found by their contents, whatever their names, and copied to

    C:\ProgramData\Ostrich 55\roms

The status line under the panel says which set was found and where, or what is missing.

Playing it
----------

- Send it MIDI as you would the unit. GS RESET and GM RESET in the bar under the panel
  send the reset messages.
- The keys of the panel work as the unit's. The mouse wheel over a field of the display
  (PART, INSTRUMENT, LEVEL, ...) presses its keys; the arrow keys of the computer
  keyboard press PART and INSTRUMENT.
- VOLUME is the output level; the host can automate it.

More: https://github.com/Helge666/ostrich55

Licence: GPL-3.0-or-later (LICENSE.txt). The emulation core, Nuked-SC55, is
GPL-2.0-or-later. Third-party code and its licences: THIRD-PARTY.txt.

"Roland", "SC-55" and "Sound Canvas" are trademarks of Roland Corporation.
