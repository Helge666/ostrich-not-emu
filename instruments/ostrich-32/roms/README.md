# ROM images

Ostrich 32 needs your own dumps of a Roland MT-32, CM-32L or CM-32LN: the control ROM, the
PCM (wave) ROM and the reverb ROM of the Boss reverb chip. **None are included, and none
ever will be.** This folder holds only this README; `.gitignore` keeps everything else in
it out of the repository.

The images are recognised by their content (size and MD5 or SHA-1), never by name.
Ostrich 32 leaves that to 88emu's ROM registry, which lists every known image:
`third_party/gearmulator/source/ronaldo/88emu/88lib/rom/romRegistry.h`. In short:

| Model | Control ROM | PCM ROM | Reverb ROM |
|---|---|---|---|
| MT-32 (old board) | v1.04, 1.05, 1.06, 1.07, Blue Ridge, M-9: 64 KiB, or the two 32 KiB EPROMs (IC27 A, IC26 B) | R15449121 (IC37): 512 KiB, or R15179844 and R15179845 (IC21, IC22): 256 KiB each | R15179857 (IC13): 32 KiB |
| MT-32 (new board) | v2.03, 2.04, 2.06, 2.07: 128 KiB | as the old board | R15179917: 32 KiB |
| CM-32L, LAPC-I | v1.00, 1.02: 64 KiB | R15449121 + R15179945: 1 MiB, or the two 512 KiB chips | R15179917 |
| CM-32LN, CM-500, LAPC-N | v1.00: 64 KiB | as the CM-32L | R15179917 |

The reverb ROM is part of the sound: 88emu runs the reverb chip's own microcode, so a
board without its reverb ROM does not start. The old board's R15179857 does not run on the
new board.

Several control ROMs may sit side by side; the plugin offers each one that makes a complete
machine. Where Ostrich 32 looks for them is settled with the plugin (Windows:
`C:\ProgramData\Ostrich 32\roms\`; Linux: `~/.local/share/Ostrich 32/roms/`).
