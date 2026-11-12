# ROM images

Ostrich 32 needs your own dumps of a Roland MT-32, CM-32L, CM-64 or LAPC-I: one control ROM
and the matching PCM ROM. **None are included, and none ever will be.** This folder holds
only this README; `.gitignore` keeps everything else in it out of the repository.

The images are recognised by their SHA-1, never by name. Ostrich 32 leaves that to
libmt32emu: the list of every known image, with sizes and checksums, is in
[`munt/mt32emu/src/ROMInfo.cpp`](https://github.com/munt/munt/blob/libmt32emu_2_8_6/mt32emu/src/ROMInfo.cpp).
In short:

| Model | Control ROM | PCM ROM |
|---|---|---|
| MT-32 (old) | v1.04, 1.05, 1.06, 1.07, BlueRidge: 64 KiB, or the two 32 KiB halves (IC26/IC27) | MT-32 PCM: 512 KiB, or the two 256 KiB halves (IC21/IC22) |
| MT-32 (new) | v2.03, 2.04, 2.06, 2.07: 128 KiB | MT-32 PCM |
| CM-32L, CM-64, LAPC-I | v1.00, 1.02: 64 KiB | CM-32L PCM: 1 MiB, or two halves |
| CM-32LN, CM-500, LAPC-N | v1.00: 64 KiB | CM-32L PCM |

Where Ostrich 32 looks for them is settled with the plugin (Windows:
`C:\ProgramData\Ostrich 32\roms\`; Linux: `~/.local/share/Ostrich 32/roms/`).
